use crate::PoryaaaaPlugin;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{LazyLock, Mutex};
use std::time::{SystemTime, UNIX_EPOCH};

pub(crate) static TEST_ENV_LOCK: LazyLock<Mutex<()>> = LazyLock::new(|| Mutex::new(()));
pub(crate) struct TestInitContext;

impl nice_plug::prelude::InitContext<PoryaaaaPlugin> for TestInitContext {
    fn plugin_api(&self) -> nice_plug::prelude::PluginApi {
        nice_plug::prelude::PluginApi::Clap
    }

    fn execute(&self, _task: <PoryaaaaPlugin as nice_plug::prelude::Plugin>::BackgroundTask) {}

    fn set_latency_samples(&self, _samples: u32) {}

    fn set_current_voice_capacity(&self, _capacity: u32) {}
}

pub(crate) fn temp_project(name: &str) -> PathBuf {
    let nonce = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("clock should be after unix epoch")
        .as_nanos();
    let root = std::env::temp_dir().join(format!("poryaaaa-rs-{name}-{nonce}"));
    fs::create_dir_all(&root).expect("create temp project");
    root
}

pub(crate) fn write_file(root: &Path, relative_path: &str, contents: &str) {
    let path = root.join(relative_path);
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).expect("create parent directories");
    }
    fs::write(path, contents).expect("write fixture file");
}
