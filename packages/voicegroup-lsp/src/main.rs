mod definition;
mod diagnostics;
mod documents;
mod server;

/// Starts the stdio LSP server process used by editor clients.
fn main() -> anyhow::Result<()> {
    server::run_stdio()
}
