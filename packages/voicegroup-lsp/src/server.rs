use anyhow::{Context, Result};
use lsp_server::{Connection, ErrorCode, Message, Notification as ServerNotification, Response};
use lsp_types::{
    notification::{DidChangeTextDocument, DidCloseTextDocument, DidOpenTextDocument, Notification},
    DidChangeTextDocumentParams, DidCloseTextDocumentParams, DidOpenTextDocumentParams,
    PublishDiagnosticsParams,
};
use serde::de::DeserializeOwned;
use url::Url;

use crate::{diagnostics::diagnostics_for_text, documents::DocumentStore};

/// Runs the language server over stdio so editor clients can launch the binary directly.
pub fn run_stdio() -> Result<()> {
    let (connection, io_threads) = Connection::stdio();
    run(connection)?;
    io_threads.join().context("joining LSP stdio threads")?;
    Ok(())
}

/// Performs initialize, then handles document notifications until shutdown.
fn run(connection: Connection) -> Result<()> {
    connection
        .initialize(server_capabilities())
        .context("initializing LSP connection")?;

    let mut server = Server::new(connection);
    server.run()
}

/// Advertises only full document synchronization for the first Rust LSP milestone.
fn server_capabilities() -> serde_json::Value {
    serde_json::json!({
        "textDocumentSync": 1
    })
}

struct Server {
    connection: Connection,
    documents: DocumentStore,
}

impl Server {
    /// Creates a server with empty in-memory document state.
    fn new(connection: Connection) -> Self {
        Self {
            connection,
            documents: DocumentStore::new(),
        }
    }

    /// Dispatches LSP messages and lets lsp-server own the shutdown handshake.
    fn run(&mut self) -> Result<()> {
        while let Ok(message) = self.connection.receiver.recv() {
            match message {
                Message::Request(request) => {
                    if self
                        .connection
                        .handle_shutdown(&request)
                        .context("handling LSP shutdown")?
                    {
                        return Ok(());
                    }
                    self.respond_method_not_found(request)?;
                }
                Message::Notification(notification) => self.handle_notification(notification)?,
                Message::Response(_) => {}
            }
        }
        Ok(())
    }

    /// Handles document lifecycle notifications that can change diagnostics.
    fn handle_notification(&mut self, notification: ServerNotification) -> Result<()> {
        match notification.method.as_str() {
            DidOpenTextDocument::METHOD => {
                let params: DidOpenTextDocumentParams =
                    notification_params::<DidOpenTextDocument>(notification)?;
                self.did_open(params)
            }
            DidChangeTextDocument::METHOD => {
                let params: DidChangeTextDocumentParams =
                    notification_params::<DidChangeTextDocument>(notification)?;
                self.did_change(params)
            }
            DidCloseTextDocument::METHOD => {
                let params: DidCloseTextDocumentParams =
                    notification_params::<DidCloseTextDocument>(notification)?;
                self.did_close(params)
            }
            _ => Ok(()),
        }
    }

    /// Stores an opened document and immediately publishes diagnostics for its text.
    fn did_open(&mut self, params: DidOpenTextDocumentParams) -> Result<()> {
        let lsp_uri = params.text_document.uri;
        let uri = parse_document_uri(&lsp_uri)?;
        let text = params.text_document.text;
        self.documents.open(uri, text.clone());
        self.publish_diagnostics(lsp_uri, diagnostics_for_text(&text))
    }

    /// Applies the latest full-content change and republishes diagnostics.
    fn did_change(&mut self, params: DidChangeTextDocumentParams) -> Result<()> {
        let lsp_uri = params.text_document.uri;
        let uri = parse_document_uri(&lsp_uri)?;
        let Some(change) = params.content_changes.into_iter().last() else {
            return Ok(());
        };
        let text = change.text;
        self.documents.replace(uri, text.clone());
        self.publish_diagnostics(lsp_uri, diagnostics_for_text(&text))
    }

    /// Drops closed document text and publishes an empty list to clear stale diagnostics.
    fn did_close(&mut self, params: DidCloseTextDocumentParams) -> Result<()> {
        let lsp_uri = params.text_document.uri;
        let uri = parse_document_uri(&lsp_uri)?;
        self.documents.close(&uri);
        self.publish_diagnostics(lsp_uri, Vec::new())
    }

    /// Sends diagnostics using the standard LSP publishDiagnostics notification.
    fn publish_diagnostics(
        &self,
        uri: lsp_types::Uri,
        diagnostics: Vec<lsp_types::Diagnostic>,
    ) -> Result<()> {
        let params = PublishDiagnosticsParams {
            uri,
            diagnostics,
            version: None,
        };
        self.connection
            .sender
            .send(Message::Notification(ServerNotification {
                method: lsp_types::notification::PublishDiagnostics::METHOD.to_string(),
                params: serde_json::to_value(params).context("serializing diagnostics")?,
            }))
            .context("publishing diagnostics")
    }

    /// Returns a JSON-RPC method-not-found response for unsupported requests.
    fn respond_method_not_found(&self, request: lsp_server::Request) -> Result<()> {
        self.connection
            .sender
            .send(Message::Response(Response::new_err(
                request.id,
                ErrorCode::MethodNotFound as i32,
                format!("unsupported request: {}", request.method),
            )))
            .context("sending unsupported request response")
    }
}

/// Decodes typed LSP notification parameters with method-specific error context.
fn notification_params<N>(notification: ServerNotification) -> Result<N::Params>
where
    N: Notification,
    N::Params: DeserializeOwned,
{
    serde_json::from_value(notification.params).with_context(|| format!("decoding {} params", N::METHOD))
}

/// Converts lsp-types' URI wrapper into url::Url for stable document-store keys.
fn parse_document_uri(uri: &lsp_types::Uri) -> Result<Url> {
    Url::parse(uri.as_str()).with_context(|| format!("parsing document URI {}", uri.as_str()))
}
