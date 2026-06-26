//! [egui](https://github.com/emilk/egui) editor support for nice-plug.
//!
//! TODO: Proper usage example, for now check out the gain_gui example

// See the comment in the main `nice-plug` crate
#![allow(clippy::type_complexity)]

use crossbeam::atomic::AtomicCell;
use egui::{Context, Ui};
use nice_plug_core::context::gui::ParamSetter;
use nice_plug_core::editor::{Editor, ResizeHint};
use nice_plug_core::params::persist::PersistentField;
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

#[cfg(not(any(feature = "opengl", feature = "wgpu")))]
compile_error!("There's currently no software rendering support for egui");

/// Re-export for convenience.
pub use egui_baseview::*;

#[cfg(all(feature = "opengl", not(feature = "wgpu")))]
pub use baseview::gl::{GlConfig, Profile};

mod editor;
pub mod resizable_window;
pub mod widgets;

#[derive(Default, Debug, Clone)]
pub struct EguiSettings {
    pub graphics_config: GraphicsConfig,
    pub resize_hint: ResizeHint,
    pub min_size: (u32, u32),

    #[cfg(all(feature = "opengl", not(feature = "wgpu")))]
    /// By default this is set to `false`.
    pub enable_vsync_on_x11: bool,

    #[cfg(all(feature = "opengl", not(feature = "wgpu")))]
    /// The configuration of the OpenGL context.
    ///
    /// By default this is set to:
    /// ```ignore
    /// GlConfig {
    ///     version: (3, 2),
    ///     profile: Profile::Core,
    ///     red_bits: 8,
    ///     blue_bits: 8,
    ///     green_bits: 8,
    ///     alpha_bits: 8,
    ///     depth_bits: 24,
    ///     stencil_bits: 8,
    ///     samples: None,
    ///     srgb: true,
    ///     double_buffer: true,
    ///     vsync: false,
    /// }
    /// ```
    pub gl_config: GlConfig,
}

/// Create an [`Editor`] instance using an [`egui`] GUI. Using the user state parameter is
/// optional, but it can be useful for keeping track of some temporary GUI-only settings. See the
/// `nice-plug_gain_egui` example for more information on how to use this. The [`EguiState`] passed
/// to this function contains the GUI's intitial size, and this is kept in sync whenever the GUI gets
/// resized. You can also use this to know if the GUI is open, so you can avoid performing
/// potentially expensive calculations while the GUI is not open. If you want this size to be
/// persisted when restoring a plugin instance, then you can store it in a `#[persist = "key"]`
/// field on your parameters struct.
///
/// See [`EguiState::from_size()`].
pub fn create_egui_editor<T, B, U>(
    egui_state: Arc<EguiState>,
    user_state: T,
    settings: EguiSettings,
    build: B,
    update: U,
) -> Option<Box<dyn Editor>>
where
    T: 'static + Send,
    B: Fn(&Context, &mut Queue, &mut T) + 'static + Send + Sync,
    U: Fn(&mut Ui, &ParamSetter, &mut Queue, &mut T) + 'static + Send + Sync,
{
    let min_size = (settings.min_size.0.max(1), settings.min_size.1.max(1));
    if let Some(size) = settings
        .resize_hint
        .adjust_size(egui_state.size().0, egui_state.size().1, egui_state.size())
        .map(|size| (size.0.max(min_size.0), size.1.max(min_size.1)))
    {
        egui_state.size.store(size);
    }

    Some(Box::new(editor::EguiEditor {
        egui_state,
        user_state: Arc::new(Mutex::new(user_state)),
        settings: Arc::new(settings),
        build: Arc::new(build),
        update: Arc::new(update),

        // TODO: We can't get the size of the window when baseview does its own scaling, so if the
        //       host does not set a scale factor on Windows or Linux we should just use a factor of
        //       1. That may make the GUI tiny but it also prevents it from getting cut off.
        #[cfg(target_os = "macos")]
        scaling_factor: AtomicCell::new(None),
        #[cfg(not(target_os = "macos"))]
        scaling_factor: AtomicCell::new(Some(1.0)),
    }))
}

/// State for an `nice-plug-egui` editor.
#[derive(Debug, Serialize, Deserialize)]
pub struct EguiState {
    /// The window's size in logical pixels before applying `scale_factor`.
    #[serde(with = "nice_plug_core::params::persist::serialize_atomic_cell")]
    size: AtomicCell<(u32, u32)>,

    /// The new size of the window, if it was requested to resize by the GUI.
    #[serde(skip)]
    requested_size: AtomicCell<Option<(u32, u32)>>,

    /// The host-accepted size that still needs to be applied to the egui window.
    #[serde(skip)]
    applied_size: AtomicCell<Option<(u32, u32)>>,

    /// Whether the editor's window is currently open.
    #[serde(skip)]
    open: AtomicBool,
}

impl<'a> PersistentField<'a, EguiState> for Arc<EguiState> {
    fn set(&self, new_value: EguiState) {
        self.size.store(new_value.size.load());
    }

    fn map<F, R>(&self, f: F) -> R
    where
        F: Fn(&EguiState) -> R,
    {
        f(self)
    }
}

impl EguiState {
    /// Initialize the GUI's state. This value can be passed to [`create_egui_editor()`]. The window
    /// size is in logical pixels, so before it is multiplied by the DPI scaling factor.
    pub fn from_size(width: u32, height: u32) -> Arc<EguiState> {
        Arc::new(EguiState {
            size: AtomicCell::new((width, height)),
            requested_size: Default::default(),
            applied_size: Default::default(),
            open: AtomicBool::new(false),
        })
    }

    /// Returns a `(width, height)` pair for the current size of the GUI in logical pixels.
    pub fn size(&self) -> (u32, u32) {
        self.size.load()
    }

    /// Whether the GUI is currently visible.
    // Called `is_open()` instead of `open()` to avoid the ambiguity.
    pub fn is_open(&self) -> bool {
        self.open.load(Ordering::Acquire)
    }

    /// Set the new size (in logical pixels) that will be used to resize the window if the host allows.
    pub fn set_requested_size(&self, new_size: (u32, u32)) {
        self.requested_size
            .store(Some((new_size.0.max(1), new_size.1.max(1))));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use nice_plug_core::editor::ResizeHint;

    #[test]
    fn editor_resize_policy_clamps_host_and_requested_sizes() {
        let egui_state = EguiState::from_size(420, 260);
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint::resizable();
        settings.min_size = (420, 260);

        let editor = create_egui_editor(
            egui_state,
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert!(editor.resize_hint().can_resize);
        assert_eq!(editor.adjust_size(1, 1), Some((420, 260)));
        assert!(editor.set_size(1, 1));
        assert_eq!(editor.size(), (420, 260));
    }

    #[test]
    fn host_resize_does_not_queue_plugin_resize_request() {
        let egui_state = EguiState::from_size(420, 260);
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint::resizable();

        let editor = create_egui_editor(
            egui_state.clone(),
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert!(editor.set_size(800, 600));
        assert_eq!(egui_state.requested_size.load(), None);
        assert_eq!(egui_state.applied_size.load(), Some((800, 600)));
        assert_eq!(editor.size(), (800, 600));
    }

    #[test]
    fn host_resize_clears_stale_plugin_resize_request() {
        let egui_state = EguiState::from_size(640, 480);
        egui_state.set_requested_size((900, 700));
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint::resizable();
        let editor = create_egui_editor(
            egui_state.clone(),
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert!(editor.set_size(800, 600));
        assert_eq!(egui_state.requested_size.load(), None);
        assert_eq!(egui_state.applied_size.load(), Some((800, 600)));
        assert_eq!(editor.size(), (800, 600));
    }

    #[test]
    fn editor_resize_policy_honors_axis_and_aspect_hints() {
        let egui_state = EguiState::from_size(400, 200);
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint {
            can_resize: true,
            can_resize_horizontally: false,
            can_resize_vertically: true,
            preserve_aspect_ratio: false,
            aspect_ratio_width: 1,
            aspect_ratio_height: 1,
        };

        let editor = create_egui_editor(
            egui_state.clone(),
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert_eq!(editor.adjust_size(900, 300), Some((400, 300)));

        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint {
            can_resize: true,
            can_resize_horizontally: true,
            can_resize_vertically: true,
            preserve_aspect_ratio: true,
            aspect_ratio_width: 2,
            aspect_ratio_height: 1,
        };
        let editor = create_egui_editor(
            egui_state,
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert_eq!(editor.adjust_size(900, 300), Some((900, 450)));

        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint {
            can_resize: true,
            can_resize_horizontally: false,
            can_resize_vertically: true,
            preserve_aspect_ratio: true,
            aspect_ratio_width: 2,
            aspect_ratio_height: 1,
        };
        let editor = create_egui_editor(
            EguiState::from_size(400, 200),
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert_eq!(editor.adjust_size(900, 300), Some((400, 200)));

        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint {
            can_resize: true,
            can_resize_horizontally: true,
            can_resize_vertically: false,
            preserve_aspect_ratio: true,
            aspect_ratio_width: 2,
            aspect_ratio_height: 1,
        };
        let editor = create_egui_editor(
            EguiState::from_size(400, 200),
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert_eq!(editor.adjust_size(900, 300), Some((400, 200)));
    }

    #[test]
    fn editor_creation_clamps_persisted_size_to_resize_policy() {
        let egui_state = EguiState::from_size(100, 100);
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint::resizable();
        settings.min_size = (420, 260);

        let editor = create_egui_editor(
            egui_state.clone(),
            (),
            settings,
            |_egui_ctx, _queue, _state| {},
            |_ui, _setter, _queue, _state| {},
        )
        .expect("editor");

        assert_eq!(egui_state.size(), (420, 260));
        assert_eq!(editor.size(), (420, 260));
    }
}
