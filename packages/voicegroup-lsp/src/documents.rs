use std::collections::HashMap;

use url::Url;

/// Keeps the latest full text for every open document so diagnostics analyze unsaved buffers.
#[derive(Debug, Default)]
pub struct DocumentStore {
    documents: HashMap<Url, String>,
}

impl DocumentStore {
    /// Creates an empty document store for a fresh LSP session.
    pub fn new() -> Self {
        Self::default()
    }

    /// Records the full text sent by didOpen.
    pub fn open(&mut self, uri: Url, text: String) {
        self.documents.insert(uri, text);
    }

    /// Replaces a document with the full text sent by didChange.
    pub fn replace(&mut self, uri: Url, text: String) {
        self.documents.insert(uri, text);
    }

    /// Removes a closed document and reports whether it had been tracked.
    pub fn close(&mut self, uri: &Url) -> bool {
        self.documents.remove(uri).is_some()
    }

    /// Iterates open documents as immutable snapshots for bulk diagnostic refresh.
    pub fn iter(&self) -> impl Iterator<Item = (&Url, &str)> {
        self.documents
            .iter()
            .map(|(uri, text)| (uri, text.as_str()))
    }

    /// Returns the current unsaved text for request handlers and tests.
    pub fn get(&self, uri: &Url) -> Option<&str> {
        self.documents.get(uri).map(String::as_str)
    }
}

#[cfg(test)]
mod tests {
    use super::DocumentStore;
    use url::Url;

    #[test]
    fn replacing_document_keeps_latest_full_text() {
        let uri = Url::parse("file:///tmp/voice.inc").unwrap();
        let mut documents = DocumentStore::new();

        documents.open(uri.clone(), "voice_group old".to_string());
        documents.replace(uri.clone(), "voice_group new".to_string());

        assert_eq!(documents.get(&uri), Some("voice_group new"));
    }

    #[test]
    fn closing_document_removes_stored_text() {
        let uri = Url::parse("file:///tmp/voice.inc").unwrap();
        let mut documents = DocumentStore::new();

        documents.open(uri.clone(), "voice_group gone".to_string());
        assert!(documents.close(&uri));

        assert_eq!(documents.get(&uri), None);
        assert!(!documents.close(&uri));
    }

    #[test]
    fn iterating_documents_exposes_open_document_snapshots() {
        let first = Url::parse("file:///tmp/first.inc").unwrap();
        let second = Url::parse("file:///tmp/second.inc").unwrap();
        let mut documents = DocumentStore::new();

        documents.open(first.clone(), "first text".to_string());
        documents.open(second.clone(), "second text".to_string());

        let mut snapshots = documents
            .iter()
            .map(|(uri, text)| (uri.clone(), text.to_string()))
            .collect::<Vec<_>>();
        snapshots.sort_by(|left, right| left.0.as_str().cmp(right.0.as_str()));

        assert_eq!(
            snapshots,
            vec![
                (first, "first text".to_string()),
                (second, "second text".to_string())
            ]
        );
    }
}
