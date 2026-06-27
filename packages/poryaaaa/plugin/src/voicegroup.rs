use std::env;
use std::path::PathBuf;

///TODO: better name. "Status" irkfull
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VoicegroupLoadStatus {
    pub text: String,
    pub is_error: bool,
}

/// Returns the shared projects.json path used by poryaaaa and ccomidi.
/// This is *not read* by poryaaaa. CComidi reads it to get voice name & line numbers
pub(crate) fn default_projects_json_path() -> Option<PathBuf> {
    let dir = projects_json_dir_for_env(
        env::var("APPDATA").ok().as_deref(),
        env::var("USERPROFILE").ok().as_deref(),
        env::var("HOME").ok().as_deref(),
        env::var("XDG_CONFIG_HOME").ok().as_deref(),
    )?;
    Some(dir.join("projects.json"))
}

fn projects_json_dir_for_env(
    _appdata: Option<&str>,
    _userprofile: Option<&str>,
    home: Option<&str>,
    _xdg_config_home: Option<&str>,
) -> Option<PathBuf> {
    #[cfg(windows)]
    {
        if let Some(appdata) = non_empty_value(_appdata) {
            return Some(PathBuf::from(appdata).join("poryaaaa"));
        }
        let home = non_empty_value(_userprofile).or_else(|| non_empty_value(home))?;
        Some(
            PathBuf::from(home)
                .join("AppData")
                .join("Roaming")
                .join("poryaaaa"),
        )
    }

    #[cfg(all(unix, not(target_os = "macos")))]
    {
        let home = non_empty_value(home)?;
        if let Some(xdg) = non_empty_value(_xdg_config_home) {
            return Some(PathBuf::from(xdg).join("poryaaaa"));
        }
        Some(PathBuf::from(home).join(".config").join("poryaaaa"))
    }

    #[cfg(target_os = "macos")]
    {
        let home = non_empty_value(home)?;
        Some(PathBuf::from(home).join("Library/Application Support/poryaaaa"))
    }
}

fn non_empty_value(value: Option<&str>) -> Option<&str> {
    value.filter(|value| !value.is_empty())
}

#[cfg(test)]
pub(crate) mod tests {
    #[cfg(target_os = "macos")]
    #[test]
    fn default_path_policy_matches_shared_macos_helper() {
        assert_eq!(
            super::projects_json_dir_for_env(None, None, Some("/Users/tester"), None)
                .expect("macos home")
                .to_string_lossy(),
            "/Users/tester/Library/Application Support/poryaaaa"
        );
        assert!(super::projects_json_dir_for_env(None, None, None, Some("/xdg")).is_none());
    }

    #[cfg(all(unix, not(target_os = "macos")))]
    #[test]
    fn default_path_policy_matches_shared_linux_helper() {
        assert_eq!(
            super::projects_json_dir_for_env(None, None, Some("/home/tester"), Some("/xdg"))
                .expect("linux xdg")
                .to_string_lossy(),
            "/xdg/poryaaaa"
        );
        assert_eq!(
            super::projects_json_dir_for_env(None, None, Some("/home/tester"), None)
                .expect("linux home")
                .to_string_lossy(),
            "/home/tester/.config/poryaaaa"
        );
        assert!(super::projects_json_dir_for_env(None, None, None, Some("/xdg")).is_none());
    }

    #[cfg(windows)]
    #[test]
    fn default_path_policy_matches_shared_windows_helper() {
        assert_eq!(
            super::projects_json_dir_for_env(
                Some("C:\\Users\\tester\\AppData\\Roaming"),
                None,
                None,
                None
            )
            .expect("windows appdata")
            .to_string_lossy(),
            "C:\\Users\\tester\\AppData\\Roaming\\poryaaaa"
        );
        assert_eq!(
            super::projects_json_dir_for_env(None, Some("C:\\Users\\tester"), None, None)
                .expect("windows userprofile")
                .to_string_lossy(),
            "C:\\Users\\tester\\AppData\\Roaming\\poryaaaa"
        );
        assert!(super::projects_json_dir_for_env(None, None, None, None).is_none());
    }
}
