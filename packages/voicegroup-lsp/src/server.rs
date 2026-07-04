use anyhow::{Context, Result};
use lsp_server::{Connection, ErrorCode, Message, Notification as ServerNotification, Response};
use lsp_types::{
    notification::{
        DidChangeTextDocument, DidChangeWatchedFiles, DidCloseTextDocument, DidOpenTextDocument,
        Notification,
    },
    request::{GotoDefinition, Request as LspRequest},
    DidChangeTextDocumentParams, DidChangeWatchedFilesParams, DidCloseTextDocumentParams,
    DidOpenTextDocumentParams, GotoDefinitionParams, GotoDefinitionResponse, Location, Position,
    PublishDiagnosticsParams,
};
use serde::de::DeserializeOwned;
use std::{
    collections::HashMap,
    fs,
    path::{Path, PathBuf},
};
use url::Url;

use crate::{
    definition::goto_definition, diagnostics::diagnostics_for_text, documents::DocumentStore,
};
use voicegroup_core::project_index::ProjectIndex;

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
        "textDocumentSync": 1,
        "definitionProvider": true
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
                    self.handle_request(request)?;
                }
                Message::Notification(notification) => self.handle_notification(notification)?,
                Message::Response(_) => {}
            }
        }
        Ok(())
    }

    /// Handles editor requests that need a direct response instead of a notification.
    fn handle_request(&mut self, request: lsp_server::Request) -> Result<()> {
        let id = request.id.clone();
        match request.method.as_str() {
            GotoDefinition::METHOD => {
                let params: GotoDefinitionParams =
                    serde_json::from_value(request.params).context("decoding definition params")?;
                let response = self.definition_response(params)?;
                self.connection
                    .sender
                    .send(Message::Response(Response::new_ok(
                        id,
                        serde_json::to_value(response)
                            .context("serializing definition response")?,
                    )))
                    .context("sending definition response")
            }
            _ => self.respond_method_not_found(request),
        }
    }

    /// Resolves a goto-definition request against the latest open buffer text.
    fn definition_response(
        &mut self,
        params: GotoDefinitionParams,
    ) -> Result<Option<GotoDefinitionResponse>> {
        let params = params.text_document_position_params;
        let uri = parse_document_uri(&params.text_document.uri)?;
        let Some(text) = self.document_text(&uri) else {
            return Ok(None);
        };
        Ok(self
            .project_contexts
            .definition_for_document(&uri, &text, params.position)
            .map(GotoDefinitionResponse::Scalar))
    }

    /// Reads unsaved document text first, falling back to disk for unopened files.
    fn document_text(&self, uri: &Url) -> Option<String> {
        self.documents.get(uri).map(str::to_string).or_else(|| {
            uri.to_file_path()
                .ok()
                .and_then(|path| fs::read_to_string(path).ok())
        })
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
                let params: DidChangeWatchedFilesParams =
                    notification_params::<DidChangeWatchedFiles>(notification)?;
                self.did_change_watched_files(params)
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

    /// Invalidates cached project symbols and refreshes diagnostics for open buffers.
    fn did_change_watched_files(&mut self, _params: DidChangeWatchedFilesParams) -> Result<()> {
        self.project_contexts.clear();
        let documents = self
            .documents
            .iter()
            .map(|(uri, text)| (uri.clone(), text.to_string()))
            .collect::<Vec<_>>();

        for (uri, text) in documents {
            let lsp_uri = url_to_lsp_uri(&uri)?;
            let diagnostics = self.project_contexts.diagnostics_for_document(&uri, &text);
            self.publish_diagnostics(lsp_uri, diagnostics)?;
        }

        Ok(())
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

/// Converts stored URL keys back into LSP URI values for diagnostic refreshes.
fn url_to_lsp_uri(uri: &Url) -> Result<lsp_types::Uri> {
    uri.as_str()
        .parse()
        .with_context(|| format!("converting document URL {} to LSP URI", uri))
}

struct ProjectContexts {
    projects: HashMap<PathBuf, Option<ProjectIndex>>,
}

impl ProjectContexts {
    /// Starts with no cached project contexts; roots are loaded on first document use.
    fn new() -> Self {
        Self {
            projects: HashMap::new(),
        }
    }

    /// Loads diagnostics through a project context derived from the document path,
    /// avoiding false symbol errors when the editor workspace is a parent repo.
    fn diagnostics_for_document(&mut self, uri: &Url, text: &str) -> Vec<lsp_types::Diagnostic> {
        let Some(root) = project_root_for_document(uri) else {
            return diagnostics_for_text(text, None);
        };

        let analysis_context = self
            .project_index_for_root(root)
            .map(ProjectIndex::analysis_context);
        diagnostics_for_text(text, analysis_context.as_ref())
    }

    /// Finds the project-level definition location for the voicegroup symbol under the cursor.
    fn definition_for_document(
        &mut self,
        uri: &Url,
        text: &str,
        position: Position,
    ) -> Option<Location> {
        let root = project_root_for_document(uri)?;
        let index = self.project_index_for_root(root.clone())?;
        goto_definition(index, &root, text, position)
    }

    /// Loads each project index once so diagnostics and definitions share the same source model.
    fn project_index_for_root(&mut self, root: PathBuf) -> Option<&ProjectIndex> {
        if !self.projects.contains_key(&root) {
            self.projects
                .insert(root.clone(), ProjectIndex::load(&root).ok());
        }
        self.projects.get(&root).and_then(Option::as_ref)
    }

    /// Drops cached project symbols so watched asset-file changes take effect.
    fn clear(&mut self) {
        self.projects.clear();
    }
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
    use super::{server_capabilities, ProjectContexts};
    use std::{
        fs,
        time::{SystemTime, UNIX_EPOCH},
    };
    use url::Url;

    #[test]
    fn server_capabilities_advertise_definition_provider() {
        let capabilities = server_capabilities();

        assert_eq!(
            capabilities.get("definitionProvider"),
            Some(&serde_json::Value::Bool(true))
        );
    }

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

    #[test]
    fn definition_for_included_voicegroup_resolves_to_project_include() {
        let parent = temp_project_root("definition");
        let project = parent.join("decomp");
        fs::create_dir_all(project.join("sound/voicegroups")).expect("create voicegroup dir");
        fs::write(
            project.join("sound/voice_groups.inc"),
            ".include \"sound/voicegroups/village_bridge.inc\"\n",
        )
        .expect("write voicegroup include table");
        let text = "\
voice_group village_bridge
\tvoice_square_1 60, 0, 0, 2, 1, 2, 8, 3
";
        fs::write(project.join("sound/voicegroups/village_bridge.inc"), text)
            .expect("write voicegroup file");

        let document_uri =
            Url::from_file_path(project.join("sound/voicegroups/village_bridge.inc"))
                .expect("convert document path to uri");
        let mut contexts = ProjectContexts::new();
        let location = contexts
            .definition_for_document(&document_uri, text, lsp_types::Position::new(0, 14))
            .expect("definition location");

        let expected_uri = Url::from_file_path(project.join("sound/voice_groups.inc"))
            .expect("convert definition path to url")
            .as_str()
            .parse()
            .expect("convert definition url to lsp uri");
        let _ = fs::remove_dir_all(parent);
        assert_eq!(
            location,
            lsp_types::Location {
                uri: expected_uri,
                range: lsp_types::Range::new(
                    lsp_types::Position::new(0, 10),
                    lsp_types::Position::new(0, 46),
                ),
            }
        );
    }

    fn temp_project_root(name: &str) -> std::path::PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time after epoch")
            .as_nanos();
        std::env::temp_dir().join(format!("voicegroup-lsp-{name}-{nonce}"))
    }
}
