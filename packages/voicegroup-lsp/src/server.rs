use anyhow::{Context, Result};
use lsp_server::{Connection, ErrorCode, Message, Notification as ServerNotification, Response};
use lsp_types::{
    notification::{
        DidChangeTextDocument, DidChangeWatchedFiles, DidCloseTextDocument, DidOpenTextDocument,
        Notification,
    },
    request::{Completion, GotoDefinition, Request as LspRequest, SemanticTokensFullRequest},
    CompletionItem, CompletionItemKind, CompletionParams, CompletionResponse,
    DidChangeTextDocumentParams, DidChangeWatchedFilesParams, DidCloseTextDocumentParams,
    DidOpenTextDocumentParams, GotoDefinitionParams, GotoDefinitionResponse, Location, Position,
    PublishDiagnosticsParams, Range, SemanticTokensParams, SemanticTokensResult, TextEdit,
};
use serde::de::DeserializeOwned;
use std::{
    collections::{BTreeSet, HashMap},
    fs,
    path::{Path, PathBuf},
};
use url::Url;

use crate::{
    definition::{goto_definition, semantic_tokens_for_text, SEMANTIC_TOKEN_TYPES},
    diagnostics::diagnostics_for_text,
    documents::DocumentStore,
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

/// Advertises document synchronization and editor language features.
fn server_capabilities() -> serde_json::Value {
    serde_json::json!({
        "textDocumentSync": 1,
        "definitionProvider": true,
        "completionProvider": {
            "resolveProvider": false,
            "triggerCharacters": ["\"", "/"]
        },
        "semanticTokensProvider": {
            "legend": {
                "tokenTypes": SEMANTIC_TOKEN_TYPES,
                "tokenModifiers": []
            },
            "full": true
        }
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
            Completion::METHOD => self.respond_completion(request),
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
            SemanticTokensFullRequest::METHOD => {
                let params: SemanticTokensParams = serde_json::from_value(request.params)
                    .context("decoding semantic tokens params")?;
                let response = self.semantic_tokens_response(params)?;
                self.connection
                    .sender
                    .send(Message::Response(Response::new_ok(
                        id,
                        serde_json::to_value(response)
                            .context("serializing semantic tokens response")?,
                    )))
                    .context("sending semantic tokens response")
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

    /// Resolves semantic tokens from the latest open buffer text.
    fn semantic_tokens_response(
        &self,
        params: SemanticTokensParams,
    ) -> Result<Option<SemanticTokensResult>> {
        let uri = parse_document_uri(&params.text_document.uri)?;
        let Some(text) = self.document_text(&uri) else {
            return Ok(None);
        };
        Ok(Some(SemanticTokensResult::Tokens(
            semantic_tokens_for_text(&text),
        )))
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

    /// Responds to completion requests with include paths where the document context allows it.
    fn respond_completion(&mut self, request: lsp_server::Request) -> Result<()> {
        let lsp_server::Request { id, params, .. } = request;
        let params: CompletionParams =
            serde_json::from_value(params).context("decoding textDocument/completion params")?;
        let result = self.completion(params)?;
        self.connection
            .sender
            .send(Message::Response(Response::new_ok(id, result)))
            .context("sending completion response")
    }

    /// Computes completion items from the current unsaved document snapshot.
    fn completion(&mut self, params: CompletionParams) -> Result<Option<CompletionResponse>> {
        let uri = parse_document_uri(&params.text_document_position.text_document.uri)?;
        let position = params.text_document_position.position;
        // Use the unsaved buffer so completions reflect includes the user just typed.
        let text = self.documents.get(&uri).unwrap_or("").to_string();
        let items = self
            .project_contexts
            .voicegroup_file_completions(&uri, &text, position);
        Ok(Some(CompletionResponse::Array(items)))
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
    indexes: HashMap<PathBuf, Option<ProjectIndex>>,
}

impl ProjectContexts {
    /// Starts with no cached project indexes; roots are loaded on first document use.
    fn new() -> Self {
        Self {
            indexes: HashMap::new(),
        }
    }

    /// Loads diagnostics through a project index derived from the document path,
    /// avoiding false symbol errors when the editor workspace is a parent repo.
    fn diagnostics_for_document(&mut self, uri: &Url, text: &str) -> Vec<lsp_types::Diagnostic> {
        let Some(index) = self.index_for_document(uri) else {
            return diagnostics_for_text(text, None);
        };
        let analysis_context = index.analysis_context();
        diagnostics_for_text(text, Some(&analysis_context))
    }

    /// Finds the project-level definition location for the voicegroup symbol under the cursor.
    fn definition_for_document(
        &mut self,
        uri: &Url,
        text: &str,
        position: Position,
    ) -> Option<Location> {
        let root = project_root_for_document(uri)?;
        let index = self.index_for_document(uri)?;
        goto_definition(index, &root, text, position)
    }

    /// Produces include completions from physical voicegroup files for voice_groups.inc.
    fn voicegroup_file_completions(
        &mut self,
        uri: &Url,
        text: &str,
        position: Position,
    ) -> Vec<CompletionItem> {
        if !is_voice_groups_document(uri) {
            return Vec::new();
        }
        let Some(index) = self.index_for_document(uri) else {
            return Vec::new();
        };
        // Completion should suggest physical files that are not already included in
        // this buffer; declared-symbol validity remains a core analysis concern.
        let included_paths = included_voicegroup_paths(text);
        let replacement_range = include_path_replacement_range(text, position);
        index
            .voicegroup_files()
            .filter(|path| !included_paths.contains(*path))
            .map(|path| voicegroup_file_completion(path, replacement_range))
            .collect()
    }

    /// Drops cached project indexes so watched file changes take effect.
    fn clear(&mut self) {
        self.indexes.clear();
    }

    /// Returns the cached project index for a document's project root.
    fn index_for_document(&mut self, uri: &Url) -> Option<&ProjectIndex> {
        let root = project_root_for_document(uri)?;
        self.indexes
            .entry(root.clone())
            .or_insert_with(|| ProjectIndex::load(&root).ok())
            .as_ref()
    }
}

/// Builds a completion item that inserts or replaces the include path text.
fn voicegroup_file_completion(path: &str, replacement_range: Option<Range>) -> CompletionItem {
    let text_edit = replacement_range.map(|range| {
        lsp_types::CompletionTextEdit::Edit(TextEdit {
            range,
            new_text: path.to_string(),
        })
    });
    CompletionItem {
        label: path.to_string(),
        kind: Some(CompletionItemKind::FILE),
        detail: Some("voicegroup file".to_string()),
        filter_text: path.rsplit('/').next().map(str::to_string),
        // When completion is not inside quotes, insert a full include directive.
        insert_text: if text_edit.is_none() {
            Some(format!(".include \"{path}\""))
        } else {
            None
        },
        text_edit,
        ..Default::default()
    }
}

/// Checks whether a request targets the project voicegroup include table.
fn is_voice_groups_document(uri: &Url) -> bool {
    uri.to_file_path()
        .ok()
        .and_then(|path| path.to_str().map(str::to_string))
        .is_some_and(|path| path.ends_with("/sound/voice_groups.inc"))
}

/// Finds voicegroup include paths already present in the unsaved document text.
fn included_voicegroup_paths(text: &str) -> BTreeSet<String> {
    text.lines()
        .filter_map(include_path_from_line)
        .filter(|path| {
            path.starts_with("sound/voicegroups/") && path.ends_with(".inc") && !path.contains('\\')
        })
        .collect()
}

/// Extracts a quoted `.include` path from a single source line.
fn include_path_from_line(line: &str) -> Option<String> {
    let trimmed = line.trim_start();
    if !trimmed.starts_with(".include") {
        return None;
    }
    let start = trimmed.find('"')? + 1;
    let end = trimmed[start..].find('"')? + start;
    Some(trimmed[start..end].to_string())
}

/// Replaces the current quoted include path prefix when completion runs inside one.
fn include_path_replacement_range(text: &str, position: Position) -> Option<Range> {
    let line = text.lines().nth(position.line as usize)?;
    let character = usize::min(position.character as usize, line.len());
    // This is intentionally a prefix replacement, not a full-line replacement,
    // so `.include "sound/voicegroups/` completes to the selected path.
    let line_prefix = &line[..character];
    let quote = line_prefix.rfind('"')?;
    Some(Range {
        start: Position {
            line: position.line,
            character: quote as u32 + 1,
        },
        end: position,
    })
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
    use super::{run, server_capabilities, ProjectContexts};
    use crate::definition::SEMANTIC_TOKEN_TYPES;
    use lsp_server::{Connection, Message, Notification as ServerNotification, RequestId};
    use lsp_types::{
        notification::{DidOpenTextDocument, Exit, Initialized, Notification},
        request::{Completion, Initialize, Request, Shutdown},
        CompletionContext, CompletionItem, CompletionParams, CompletionResponse,
        CompletionTextEdit, CompletionTriggerKind, DidOpenTextDocumentParams, InitializeParams,
        InitializedParams, Position, TextDocumentIdentifier, TextDocumentItem,
        TextDocumentPositionParams,
    };
    use std::{
        fs,
        path::Path,
        thread,
        time::{Duration, SystemTime, UNIX_EPOCH},
    };
    use url::Url;

    #[test]
    fn completion_suggests_physical_voicegroup_files_not_already_included() {
        let root = temp_project_root("include-completions");
        write_file(
            &root,
            "sound/voice_groups.inc",
            ".include \"sound/voicegroups/declared.inc\"\n",
        );
        write_file(
            &root,
            "sound/voicegroups/declared.inc",
            "voice_group declared\n\tvoice_square_2 60, 0, 2, 1, 2, 8, 3\n",
        );
        write_file(
            &root,
            "sound/voicegroups/unused.inc",
            "voice_group unused\n\tvoice_square_2 60, 0, 2, 1, 2, 8, 3\n",
        );

        let document_text = "\
.include \"sound/voicegroups/declared.inc\"
.include \"sound/voicegroups/";
        let document_uri = Url::from_file_path(root.join("sound/voice_groups.inc"))
            .expect("convert document path to uri");
        let lsp_uri = lsp_uri(&document_uri);
        let (server_connection, client_connection) = Connection::memory();
        let server_thread = thread::spawn(move || run(server_connection));
        initialize(&client_connection);
        open_document(&client_connection, lsp_uri.clone(), document_text);

        let response = request_completion(
            &client_connection,
            lsp_uri,
            Position {
                line: 1,
                character: document_text.lines().last().expect("completion line").len() as u32,
            },
        );
        shutdown(client_connection);
        let server_result = server_thread.join().expect("server thread should join");
        let _ = fs::remove_dir_all(root);
        server_result.expect("server should shut down cleanly");

        assert!(
            response.error.is_none(),
            "completion request failed: {:?}",
            response.error
        );
        let items = completion_items(response);
        assert!(
            items
                .iter()
                .any(|item| completion_text(item).contains("sound/voicegroups/unused.inc")),
            "expected an include completion for sound/voicegroups/unused.inc, got {items:#?}"
        );
        assert!(
            items
                .iter()
                .all(|item| !completion_text(item).contains("sound/voicegroups/declared.inc")),
            "already-declared voicegroup include should not be suggested: {items:#?}"
        );
    }
    #[test]
    fn server_capabilities_advertise_navigation_features() {
        let capabilities = server_capabilities();

        assert_eq!(
            capabilities.get("definitionProvider"),
            Some(&serde_json::Value::Bool(true))
        );

        let semantic_tokens = capabilities
            .get("semanticTokensProvider")
            .expect("semantic tokens provider");
        assert_eq!(
            semantic_tokens
                .get("legend")
                .and_then(|legend| legend.get("tokenTypes"))
                .and_then(serde_json::Value::as_array)
                .expect("semantic token types"),
            &SEMANTIC_TOKEN_TYPES
                .iter()
                .map(|token_type| serde_json::Value::String((*token_type).to_string()))
                .collect::<Vec<_>>()
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

    fn initialize(connection: &Connection) {
        let request_id = RequestId::from(1);
        connection
            .sender
            .send(Message::Request(lsp_server::Request {
                id: request_id.clone(),
                method: Initialize::METHOD.to_string(),
                params: serde_json::to_value(InitializeParams::default())
                    .expect("serialize initialize params"),
            }))
            .expect("send initialize request");
        let response = response_for_request(connection, request_id);
        assert!(
            response.error.is_none(),
            "initialize failed: {:?}",
            response.error
        );
        connection
            .sender
            .send(Message::Notification(ServerNotification {
                method: Initialized::METHOD.to_string(),
                params: serde_json::to_value(InitializedParams {})
                    .expect("serialize initialized params"),
            }))
            .expect("send initialized notification");
    }

    fn open_document(connection: &Connection, uri: lsp_types::Uri, text: &str) {
        connection
            .sender
            .send(Message::Notification(ServerNotification {
                method: DidOpenTextDocument::METHOD.to_string(),
                params: serde_json::to_value(DidOpenTextDocumentParams {
                    text_document: TextDocumentItem {
                        uri,
                        language_id: "voicegroup".to_string(),
                        version: 1,
                        text: text.to_string(),
                    },
                })
                .expect("serialize didOpen params"),
            }))
            .expect("send didOpen notification");
    }

    fn request_completion(
        connection: &Connection,
        uri: lsp_types::Uri,
        position: Position,
    ) -> lsp_server::Response {
        let request_id = RequestId::from(2);
        connection
            .sender
            .send(Message::Request(lsp_server::Request {
                id: request_id.clone(),
                method: Completion::METHOD.to_string(),
                params: serde_json::to_value(CompletionParams {
                    text_document_position: TextDocumentPositionParams {
                        text_document: TextDocumentIdentifier { uri },
                        position,
                    },
                    work_done_progress_params: Default::default(),
                    partial_result_params: Default::default(),
                    context: Some(CompletionContext {
                        trigger_kind: CompletionTriggerKind::INVOKED,
                        trigger_character: None,
                    }),
                })
                .expect("serialize completion params"),
            }))
            .expect("send completion request");
        response_for_request(connection, request_id)
    }

    fn shutdown(connection: Connection) {
        let request_id = RequestId::from(99);
        connection
            .sender
            .send(Message::Request(lsp_server::Request {
                id: request_id.clone(),
                method: Shutdown::METHOD.to_string(),
                params: serde_json::Value::Null,
            }))
            .expect("send shutdown request");
        let response = response_for_request(&connection, request_id);
        assert!(
            response.error.is_none(),
            "shutdown failed: {:?}",
            response.error
        );
        connection
            .sender
            .send(Message::Notification(ServerNotification {
                method: Exit::METHOD.to_string(),
                params: serde_json::Value::Null,
            }))
            .expect("send exit notification");
    }

    fn response_for_request(
        connection: &Connection,
        request_id: RequestId,
    ) -> lsp_server::Response {
        loop {
            match connection.receiver.recv_timeout(Duration::from_secs(1)) {
                Ok(Message::Response(response)) if response.id == request_id => return response,
                Ok(Message::Notification(_)) => {}
                Ok(message) => {
                    panic!("unexpected message while waiting for {request_id}: {message:?}")
                }
                Err(error) => panic!("timed out waiting for {request_id}: {error}"),
            }
        }
    }

    fn completion_items(response: lsp_server::Response) -> Vec<CompletionItem> {
        let result = response
            .result
            .expect("completion response should have a result");
        let response: Option<CompletionResponse> =
            serde_json::from_value(result).expect("decode completion response");
        match response.expect("completion response should not be null") {
            CompletionResponse::Array(items) => items,
            CompletionResponse::List(list) => list.items,
        }
    }

    fn completion_text(item: &CompletionItem) -> String {
        let mut text = item.label.clone();
        if let Some(detail) = &item.detail {
            text.push('\n');
            text.push_str(detail);
        }
        if let Some(filter_text) = &item.filter_text {
            text.push('\n');
            text.push_str(filter_text);
        }
        if let Some(insert_text) = &item.insert_text {
            text.push('\n');
            text.push_str(insert_text);
        }
        if let Some(text_edit) = &item.text_edit {
            text.push('\n');
            match text_edit {
                CompletionTextEdit::Edit(edit) => text.push_str(&edit.new_text),
                CompletionTextEdit::InsertAndReplace(edit) => text.push_str(&edit.new_text),
            }
        }
        text
    }

    fn lsp_uri(uri: &Url) -> lsp_types::Uri {
        uri.as_str()
            .parse()
            .expect("convert document URL to LSP URI")
    }

    fn write_file(root: &Path, relative_path: &str, contents: &str) {
        let path = root.join(relative_path);
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).expect("create parent directories");
        }
        fs::write(path, contents).expect("write fixture file");
    }

    fn temp_project_root(name: &str) -> std::path::PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system time after epoch")
            .as_nanos();
        std::env::temp_dir().join(format!("voicegroup-lsp-{name}-{nonce}"))
    }
}
