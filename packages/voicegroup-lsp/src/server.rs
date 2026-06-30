use anyhow::{Context, Result};
use lsp_server::{Connection, ErrorCode, Message, Notification as ServerNotification, Response};
use lsp_types::{
    notification::{
        DidChangeTextDocument, DidChangeWatchedFiles, DidCloseTextDocument, DidOpenTextDocument,
        Notification,
    },
    DidChangeTextDocumentParams, DidChangeWatchedFilesParams, DidCloseTextDocumentParams,
    DidOpenTextDocumentParams, PublishDiagnosticsParams,
};
use serde::de::DeserializeOwned;
use std::{
    collections::HashMap,
    path::{Path, PathBuf},
};
use url::Url;

use crate::{diagnostics::diagnostics_for_text, documents::DocumentStore};
use voicegroup_core::{analyzer::AnalysisContext, project_index::ProjectIndex};

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
    project_contexts: ProjectContexts,
}

impl Server {
    /// Creates a server with empty in-memory document state and project-context cache.
    fn new(connection: Connection) -> Self {
        Self {
            connection,
            documents: DocumentStore::new(),
            project_contexts: ProjectContexts::new(),
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
            DidChangeWatchedFiles::METHOD => {
                let _params: DidChangeWatchedFilesParams =
                    notification_params::<DidChangeWatchedFiles>(notification)?;
                self.project_contexts.clear();
                Ok(())
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
        let diagnostics = self.project_contexts.diagnostics_for_document(&uri, &text);
        self.documents.open(uri, text);
        self.publish_diagnostics(lsp_uri, diagnostics)
    }

    /// Applies the latest full-content change and republishes diagnostics.
    fn did_change(&mut self, params: DidChangeTextDocumentParams) -> Result<()> {
        let lsp_uri = params.text_document.uri;
        let uri = parse_document_uri(&lsp_uri)?;
        let Some(change) = params.content_changes.into_iter().last() else {
            return Ok(());
        };
        let text = change.text;
        let diagnostics = self.project_contexts.diagnostics_for_document(&uri, &text);
        self.documents.replace(uri, text);
        self.publish_diagnostics(lsp_uri, diagnostics)
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
    serde_json::from_value(notification.params)
        .with_context(|| format!("decoding {} params", N::METHOD))
}

/// Converts lsp-types' URI wrapper into url::Url for stable document-store keys.
fn parse_document_uri(uri: &lsp_types::Uri) -> Result<Url> {
    Url::parse(uri.as_str()).with_context(|| format!("parsing document URI {}", uri.as_str()))
}

struct ProjectContexts {
    contexts: HashMap<PathBuf, Option<AnalysisContext>>,
}

impl ProjectContexts {
    /// Starts with no cached project contexts; roots are loaded on first document use.
    fn new() -> Self {
        Self {
            contexts: HashMap::new(),
        }
    }

    /// Loads diagnostics through a project context derived from the document path,
    /// avoiding false symbol errors when the editor workspace is a parent repo.
    fn diagnostics_for_document(&mut self, uri: &Url, text: &str) -> Vec<lsp_types::Diagnostic> {
        let Some(root) = project_root_for_document(uri) else {
            return diagnostics_for_text(text, None);
        };

        let analysis_context = self
            .contexts
            .entry(root.clone())
            .or_insert_with(|| load_analysis_context(&root));
        diagnostics_for_text(text, analysis_context.as_ref())
    }

    /// Drops cached project symbols so watched asset-file changes take effect.
    fn clear(&mut self) {
        self.contexts.clear();
    }
}

/// Builds analyzer symbols from the nearest voicegroup project root for a file.
fn load_analysis_context(root: &Path) -> Option<AnalysisContext> {
    ProjectIndex::load(root)
        .ok()
        .map(|index| index.analysis_context())
}

/// Walks upward from a document to find the decomp project root that owns sound/.
fn project_root_for_document(uri: &Url) -> Option<PathBuf> {
    let path = uri.to_file_path().ok()?;
    let start = path.parent()?;
    start
        .ancestors()
        .find(|candidate| has_voicegroup_project_markers(candidate))
        .map(Path::to_path_buf)
}

/// Recognizes the two supported source layouts without indexing parent repos.
fn has_voicegroup_project_markers(root: &Path) -> bool {
    root.join("sound/voice_groups.inc").is_file() || root.join("sound/voicegroups").is_dir()
}

#[cfg(test)]
mod tests {
    use super::ProjectContexts;
    use std::{
        fs,
        time::{SystemTime, UNIX_EPOCH},
    };
    use url::Url;

    #[test]
    fn diagnostics_for_document_loads_symbols_from_document_project_root() {
        let parent = temp_project_root("parent-workspace");
        let project = parent.join("decomp");
        fs::create_dir_all(project.join("sound/voicegroups")).expect("create voicegroup dir");
        fs::write(
            project.join("sound/direct_sound_data.inc"),
            "DirectSoundWaveData_kick::\n\t.incbin \"sound/direct_sound_samples/kick.bin\"\n",
        )
        .expect("write direct sound symbols");

        let document_uri = Url::from_file_path(project.join("sound/voicegroups/voicegroup001.inc"))
            .expect("convert document path to uri");
        let mut contexts = ProjectContexts::new();
        let diagnostics = contexts.diagnostics_for_document(
            &document_uri,
            "voicegroup001:: @\n voice_directsound 60, 0, DirectSoundWaveData_kick, 255, 0, 255, 165\n",
        );

        let _ = fs::remove_dir_all(parent);
        assert_eq!(diagnostics, []);
    }

    #[test]
    fn project_context_cache_can_be_cleared_when_symbol_files_change() {
        let parent = temp_project_root("cache-clear");
        let project = parent.join("decomp");
        fs::create_dir_all(project.join("sound/voicegroups")).expect("create voicegroup dir");
        let symbols_path = project.join("sound/direct_sound_data.inc");
        fs::write(
            &symbols_path,
            "DirectSoundWaveData_kick::\n\t.incbin \"sound/direct_sound_samples/kick.bin\"\n",
        )
        .expect("write initial symbols");

        let document_uri = Url::from_file_path(project.join("sound/voicegroups/voicegroup001.inc"))
            .expect("convert document path to uri");
        let text =
            "voicegroup001:: @\n voice_directsound 60, 0, DirectSoundWaveData_kick, 255, 0, 255, 165\n";
        let mut contexts = ProjectContexts::new();
        assert_eq!(contexts.diagnostics_for_document(&document_uri, text), []);

        fs::write(
            &symbols_path,
            "DirectSoundWaveData_snare::\n\t.incbin \"sound/direct_sound_samples/snare.bin\"\n",
        )
        .expect("replace symbols");
        assert_eq!(contexts.diagnostics_for_document(&document_uri, text), []);

        contexts.clear();
        let diagnostics = contexts.diagnostics_for_document(&document_uri, text);

        let _ = fs::remove_dir_all(parent);
        assert!(diagnostics.iter().any(|diagnostic| {
            diagnostic.code
                == Some(lsp_types::NumberOrString::String(
                    "unknown-directsound-symbol".to_string(),
                ))
        }));
    }

    #[test]
    fn diagnostics_for_document_falls_back_to_syntax_only_without_project_root() {
        let parent = temp_project_root("no-project-root");
        fs::create_dir_all(parent.join("packages")).expect("create parent dir");

        let document_uri = Url::from_file_path(parent.join("packages/voicegroup001.inc"))
            .expect("convert document path to uri");
        let mut contexts = ProjectContexts::new();
        let diagnostics = contexts.diagnostics_for_document(
            &document_uri,
            "voicegroup001:: @\n voice_directsound 60, 0, MissingSample, 255, 0, 255, 165\n",
        );

        let _ = fs::remove_dir_all(parent);
        assert_eq!(diagnostics, []);
    }

    fn temp_project_root(name: &str) -> std::path::PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time after epoch")
            .as_nanos();
        std::env::temp_dir().join(format!("voicegroup-lsp-{name}-{nonce}"))
    }
}
