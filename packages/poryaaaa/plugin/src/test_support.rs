use crate::PoryaaaaPlugin;
use std::ffi::OsString;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::{LazyLock, Mutex, MutexGuard};
use std::time::{SystemTime, UNIX_EPOCH};

pub(crate) static TEST_ENV_LOCK: LazyLock<Mutex<()>> = LazyLock::new(|| Mutex::new(()));

pub(crate) struct IsolatedHome {
    _env_lock: MutexGuard<'static, ()>,
    old_home: Option<OsString>,
    root: PathBuf,
    projects_json_path: PathBuf,
}

impl IsolatedHome {
    pub(crate) fn new(name: &str) -> Self {
        let env_lock = TEST_ENV_LOCK.lock().expect("test env lock");
        let root = temp_project(name);
        let old_home = std::env::var_os("HOME");
        std::env::set_var("HOME", &root);
        let projects_json_path = crate::shared_projects_json::default_projects_json_path()
            .expect("default projects path");
        assert!(
            projects_json_path.starts_with(&root),
            "test must isolate projects.json under its temp HOME, got {}",
            projects_json_path.display()
        );
        let _ = fs::remove_file(&projects_json_path);

        Self {
            _env_lock: env_lock,
            old_home,
            root,
            projects_json_path,
        }
    }

    pub(crate) fn projects_json_path(&self) -> &Path {
        &self.projects_json_path
    }
}

impl Drop for IsolatedHome {
    fn drop(&mut self) {
        if let Some(old_home) = &self.old_home {
            std::env::set_var("HOME", old_home);
        } else {
            std::env::remove_var("HOME");
        }
        let _ = fs::remove_dir_all(&self.root);
    }
}
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
