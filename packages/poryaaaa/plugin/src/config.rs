use std::fs;
use std::io;
use std::path::Path;

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

/// Parses legacy config numeric values into m4a's 0..127 byte range.
fn parse_m4a_u7(value: &str, fallback: u8) -> u8 {
    value
        .parse::<u16>()
        .map(|value| value.min(127) as u8)
        .unwrap_or(fallback)
}
