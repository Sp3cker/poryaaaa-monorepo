#[cfg(test)]
use std::cell::RefCell;
use std::ffi::{c_char, CStr};
use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::sync::{LazyLock, RwLock};

static DEFAULT_CONFIG_DIR: LazyLock<RwLock<Option<PathBuf>>> = LazyLock::new(|| RwLock::new(None));

#[cfg(test)]
thread_local! {
    static TEST_CONFIG_DIR: RefCell<Option<PathBuf>> = const { RefCell::new(None) };
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PluginConfig {
    pub project_root: String,
    pub voicegroup: String,
    pub reverb: u8,
    pub volume: u8,
}

impl Default for PluginConfig {
    fn default() -> Self {
        Self {
            project_root: String::new(),
            voicegroup: String::new(),
            reverb: 0,
            volume: 127,
        }
    }
}

impl PluginConfig {
    /// Loads first-pass poryaaaa.cfg settings while ignoring deferred legacy keys.
    pub fn load_from_dir(dir: &Path) -> io::Result<Self> {
        let text = fs::read_to_string(dir.join("poryaaaa.cfg"))?;
        let mut config = Self::default();

        for line in text.lines() {
            let trimmed = line.trim();
            if trimmed.is_empty() || trimmed.starts_with('#') {
                continue;
            }

            let Some((key, value)) = trimmed.split_once('=') else {
                continue;
            };
            let value = value.trim();

            match key.trim() {
                "project_root" => config.project_root = value.to_string(),
                "voicegroup" => config.voicegroup = value.to_string(),
                "reverb" => config.reverb = parse_m4a_u7(value, config.reverb),
                "volume" => config.volume = parse_m4a_u7(value, config.volume),
                "log" => {}
                _ => {}
            }
        }

        Ok(config)
    }
}

pub(crate) fn set_default_config_dir_from_clap_path(plugin_path: *const c_char) {
    if let Some(dir) = config_dir_from_clap_path(plugin_path) {
        set_default_config_dir(Some(dir));
    }
}

pub(crate) fn load_default_config() -> PluginConfig {
    default_config_dir()
        .and_then(|dir| PluginConfig::load_from_dir(&dir).ok())
        .unwrap_or_default()
}

fn set_default_config_dir(dir: Option<PathBuf>) {
    *DEFAULT_CONFIG_DIR
        .write()
        .expect("default config dir write") = dir;
}

fn default_config_dir() -> Option<PathBuf> {
    #[cfg(test)]
    if let Some(dir) = TEST_CONFIG_DIR.with(|dir| dir.borrow().clone()) {
        return Some(dir);
    }

    DEFAULT_CONFIG_DIR
        .read()
        .expect("default config dir read")
        .clone()
}

fn config_dir_from_clap_path(plugin_path: *const c_char) -> Option<PathBuf> {
    if plugin_path.is_null() {
        return None;
    }

    let plugin_path = unsafe { CStr::from_ptr(plugin_path) }
        .to_str()
        .ok()
        .filter(|path| !path.is_empty())?;
    config_dir_from_clap_path_text(plugin_path)
}

fn config_dir_from_clap_path_text(plugin_path: &str) -> Option<PathBuf> {
    let separator = plugin_path.rfind(['/', '\\'])?;
    let plugin_dir = &plugin_path[..separator];

    #[cfg(target_os = "macos")]
    {
        const MACOS_SUFFIX: &str = "/Contents/MacOS";
        if let Some(bundle_root) = plugin_dir.strip_suffix(MACOS_SUFFIX) {
            let bundle_separator = bundle_root.rfind('/')?;
            return Some(PathBuf::from(&bundle_root[..bundle_separator]));
        }
    }

    Some(PathBuf::from(plugin_dir))
}

#[cfg(test)]
pub(crate) fn set_default_config_dir_for_test(dir: Option<PathBuf>) {
    TEST_CONFIG_DIR.with(|current| *current.borrow_mut() = dir);
}

#[cfg(test)]
pub(crate) fn config_dir_from_clap_path_for_test(plugin_path: &str) -> Option<PathBuf> {
    config_dir_from_clap_path_text(plugin_path)
}

/// Parses legacy config numeric values into m4a's 0..127 byte range.
fn parse_m4a_u7(value: &str, fallback: u8) -> u8 {
    value
        .parse::<u16>()
        .map(|value| value.min(127) as u8)
        .unwrap_or(fallback)
}
