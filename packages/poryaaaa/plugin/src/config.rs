#[cfg(test)]
use std::cell::RefCell;
use crate::params::MixerMode;
use nice_plug::log;
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
    pub pcm_mixer: MixerMode,
}

impl Default for PluginConfig {
    fn default() -> Self {
        Self {
            project_root: String::new(),
            voicegroup: String::new(),
            reverb: 0,
            volume: 127,
            pcm_mixer: MixerMode::Ipatix,
        }
    }
}

impl PluginConfig {
    /// Loads first-pass poryaaaa.cfg settings while ignoring deferred legacy keys.
    pub fn load_from_dir(dir: &Path) -> io::Result<Self> {
        let text = fs::read_to_string(dir.join("poryaaaa.cfg"))?;
        let mut config = Self::default();
        let mut pcm_mixer = None;

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
                "pcm_mixer" => pcm_mixer = Some(value.to_string()),
                "log" => {}
                _ => {}
            }
        }

        if let Some(value) = pcm_mixer {
            config.pcm_mixer = parse_pcm_mixer(&value)?;
        }
        Ok(config)
    }
}

pub(crate) fn set_default_config_dir_from_clap_path(plugin_path: *const c_char) {
    if let Some(dir) = config_dir_from_clap_path(plugin_path) {
        set_default_config_dir(Some(dir));
    }
}

/// Loads the default config while retaining a fatal present-file error for initialize().
pub(crate) fn load_default_config_with_error() -> (PluginConfig, Option<String>) {
    let Some(dir) = default_config_dir() else {
        return (PluginConfig::default(), None);
    };

    match PluginConfig::load_from_dir(&dir) {
        Ok(config) => (config, None),
        Err(error) if error.kind() == io::ErrorKind::NotFound => (PluginConfig::default(), None),
        Err(error) => {
            let diagnostic = error.to_string();
            log::error!("{diagnostic}");
            (PluginConfig::default(), Some(diagnostic))
        }
    }
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
pub(crate) fn invalid_pcm_mixer_diagnostic(value: &str) -> String {
    format!("invalid pcm_mixer value '{value}'; expected ipatix or sappy")
}

fn parse_pcm_mixer(value: &str) -> io::Result<MixerMode> {
    MixerMode::from_stable_id(value).ok_or_else(|| {
        io::Error::new(
            io::ErrorKind::InvalidData,
            invalid_pcm_mixer_diagnostic(value),
        )
    })
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
#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_support::{temp_project, write_file};
    use std::fs;

    #[test]
    fn mixer_config_accepts_exact_values_and_last_duplicate_wins() {
        let root = temp_project("config-mixer-valid");
        write_file(
            &root,
            "poryaaaa.cfg",
            "pcm_mixer=ipatix\npcm_mixer= sappy \n",
        );

        let config = PluginConfig::load_from_dir(&root).expect("valid mixer config");

        assert_eq!(config.pcm_mixer, MixerMode::Sappy);
        fs::remove_dir_all(root).expect("remove config fixture");
    }

    #[test]
    fn mixer_config_allows_a_later_valid_value_to_replace_an_invalid_duplicate() {
        let root = temp_project("config-mixer-invalid-then-valid");
        write_file(
            &root,
            "poryaaaa.cfg",
            "pcm_mixer=invalid\npcm_mixer=ipatix\n",
        );

        let config = PluginConfig::load_from_dir(&root).expect("last mixer assignment wins");

        assert_eq!(config.pcm_mixer, MixerMode::Ipatix);
        fs::remove_dir_all(root).expect("remove config fixture");
    }

    #[test]
    fn mixer_config_rejects_case_mismatch_with_invalid_data() {
        let root = temp_project("config-mixer-invalid");
        write_file(&root, "poryaaaa.cfg", "pcm_mixer=Sappy\n");

        let error = PluginConfig::load_from_dir(&root).expect_err("case mismatch must fail");

        assert_eq!(error.kind(), io::ErrorKind::InvalidData);
        assert_eq!(
            error.to_string(),
            "invalid pcm_mixer value 'Sappy'; expected ipatix or sappy"
        );
        fs::remove_dir_all(root).expect("remove config fixture");
    }

    #[test]
    fn missing_config_file_remains_nonfatal_for_default_loader() {
        let root = temp_project("config-mixer-missing");

        let (config, error) = {
            set_default_config_dir_for_test(Some(root.clone()));
            let loaded = load_default_config_with_error();
            set_default_config_dir_for_test(None);
            loaded
        };

        assert_eq!(config.pcm_mixer, MixerMode::Ipatix);
        assert_eq!(error, None);
        fs::remove_dir_all(root).expect("remove config fixture");
    }
}
