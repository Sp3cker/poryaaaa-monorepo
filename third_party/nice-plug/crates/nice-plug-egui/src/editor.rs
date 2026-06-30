//! An [`Editor`] implementation for egui.

use crate::EguiSettings;
use crate::EguiState;
use crossbeam::atomic::AtomicCell;
use egui::Context;
use egui::{Vec2, ViewportCommand};
use egui_baseview::baseview::{PhySize, Size, WindowHandle, WindowOpenOptions, WindowScalePolicy};
use egui_baseview::{EguiWindow, Queue};
use nice_plug_core::context::gui::GuiContext;
use nice_plug_core::context::gui::ParamSetter;
use nice_plug_core::editor::{Editor, ParentWindowHandle, ResizeHint};
use parking_lot::Mutex;
use raw_window_handle::{HasRawWindowHandle, RawWindowHandle};
use std::sync::atomic::Ordering;
use std::sync::Arc;

/// An [`Editor`] implementation that calls an egui draw loop.
pub(crate) struct EguiEditor<T> {
    pub(crate) egui_state: Arc<EguiState>,
    pub(crate) user_state: Arc<Mutex<T>>,

    pub(crate) settings: Arc<EguiSettings>,

    /// The user's build function. Applied once at the start of the application.
    pub(crate) build: Arc<dyn Fn(&Context, &mut Queue, &mut T) + 'static + Send + Sync>,
    /// The user's update function.
    pub(crate) update:
        Arc<dyn Fn(&mut egui::Ui, &ParamSetter, &mut Queue, &mut T) + 'static + Send + Sync>,

    /// The scaling factor reported by the host, if any. On macOS this will never be set and we
    /// should use the system scaling factor instead.
    pub(crate) scaling_factor: AtomicCell<Option<f32>>,
}

/// This version of `baseview` uses a different version of `raw_window_handle than nice-plug, so we
/// need to adapt it ourselves.
struct ParentWindowHandleAdapter(ParentWindowHandle);

unsafe impl HasRawWindowHandle for ParentWindowHandleAdapter {
    fn raw_window_handle(&self) -> RawWindowHandle {
        match self.0 {
            ParentWindowHandle::X11Window(window) => {
                let mut handle = raw_window_handle::XcbWindowHandle::empty();
                handle.window = window;
                RawWindowHandle::Xcb(handle)
            }
            ParentWindowHandle::AppKitNsView(ns_view) => {
                let mut handle = raw_window_handle::AppKitWindowHandle::empty();
                handle.ns_view = ns_view;
                RawWindowHandle::AppKit(handle)
            }
            ParentWindowHandle::Win32Hwnd(hwnd) => {
                let mut handle = raw_window_handle::Win32WindowHandle::empty();
                handle.hwnd = hwnd;
                RawWindowHandle::Win32(handle)
            }
        }
    }
}

impl<T> Editor for EguiEditor<T>
where
    T: 'static + Send,
{
    fn spawn(
        &self,
        parent: ParentWindowHandle,
        context: Arc<dyn GuiContext>,
    ) -> Box<dyn std::any::Any + Send> {
        let build = self.build.clone();
        let update = self.update.clone();
        let state = self.user_state.clone();
        let egui_state = self.egui_state.clone();
        let settings = self.settings.clone();

        #[cfg(all(feature = "opengl", not(feature = "wgpu")))]
        let gl_config = {
            let is_x11 = matches!(&parent, ParentWindowHandle::X11Window(_));

            let mut gl_config = self.settings.gl_config.clone();

            if is_x11 && self.settings.enable_vsync_on_x11 {
                gl_config.vsync = true;
            }

            gl_config
        };

        let (unscaled_width, unscaled_height) = self
            .adjust_size(self.egui_state.size().0, self.egui_state.size().1)
            .unwrap_or_else(|| self.egui_state.size());
        let scaling_factor = self.scaling_factor.load();

        #[cfg(all(feature = "opengl", not(feature = "wgpu")))]
        let window_settings = WindowOpenOptions {
            title: String::from("egui window"),
            // Baseview should be doing the DPI scaling for us
            size: Size::new(unscaled_width as f64, unscaled_height as f64),
            // NOTE: For some reason passing 1.0 here causes the UI to be scaled on macOS but
            //       not the mouse events.
            scale: scaling_factor
                .map(|factor| WindowScalePolicy::ScaleFactor(factor as f64))
                .unwrap_or(WindowScalePolicy::SystemScaleFactor),
            gl_config: Some(gl_config),
        };

        #[cfg(feature = "wgpu")]
        let window_settings = WindowOpenOptions {
            title: String::from("egui window"),
            // Baseview should be doing the DPI scaling for us
            size: Size::new(unscaled_width as f64, unscaled_height as f64),
            // NOTE: For some reason passing 1.0 here causes the UI to be scaled on macOS but
            //       not the mouse events.
            scale: scaling_factor
                .map(|factor| WindowScalePolicy::ScaleFactor(factor as f64))
                .unwrap_or(WindowScalePolicy::SystemScaleFactor),
            ..Default::default()
        };

        let window = EguiWindow::open_parented(
            &ParentWindowHandleAdapter(parent),
            window_settings,
            self.settings.graphics_config.clone(),
            state,
            move |egui_ctx, queue, state| build(egui_ctx, queue, &mut state.lock()),
            move |egui_ctx, queue, state| {
                let setter = ParamSetter::new(context.as_ref());

                if let Some(new_size) = egui_state.applied_size.swap(None) {
                    resize_window(egui_ctx, queue, new_size);
                } else if let Some(new_size) = egui_state.requested_size.swap(None) {
                    request_host_resize(&settings, egui_state.as_ref(), new_size, || {
                        context.request_resize()
                    });
                }

                // For now, just always redraw. Most plugin GUIs have meters, and those almost always
                // need a redraw. Later we can try to be a bit more sophisticated about this. Without
                // this we would also have a blank GUI when it gets first opened because most DAWs open
                // their GUI while the window is still unmapped.
                egui_ctx.request_repaint();

                (update)(egui_ctx, &setter, queue, &mut state.lock());
            },
        );

        self.egui_state.open.store(true, Ordering::Release);
        Box::new(EguiEditorHandle {
            egui_state: self.egui_state.clone(),
            window,
        })
    }

    /// Size of the editor window
    fn size(&self) -> (u32, u32) {
        let new_size = self.egui_state.requested_size.load();
        // This method will be used to ask the host for new size.
        // If the editor is currently being resized and new size hasn't been consumed and set yet, return new requested size.
        if let Some(new_size) = new_size {
            self.adjust_size(new_size.0, new_size.1)
                .unwrap_or_else(|| self.egui_state.size())
        } else {
            self.egui_state.size()
        }
    }

    fn set_size(&self, width: u32, height: u32) -> bool {
        if let Some(size) = self.adjust_size(width, height) {
            self.egui_state.size.store(size);
            self.egui_state.requested_size.store(None);
            self.egui_state.applied_size.store(Some(size));
            true
        } else {
            false
        }
    }

    fn adjust_size(&self, width: u32, height: u32) -> Option<(u32, u32)> {
        adjust_size(&self.settings, self.egui_state.size(), (width, height))
    }

    fn resize_hint(&self) -> ResizeHint {
        self.settings.resize_hint
    }

    fn set_scale_factor(&self, factor: f32) -> bool {
        // If the editor is currently open then the host must not change the current HiDPI scale as
        // we don't have a way to handle that. Ableton Live does this.
        if self.egui_state.is_open() {
            return false;
        }

        self.scaling_factor.store(Some(factor));
        true
    }

    fn param_value_changed(&self, _id: &str, _normalized_value: f32) {
        // As mentioned above, for now we'll always force a redraw to allow meter widgets to work
        // correctly. In the future we can use an `Arc<AtomicBool>` and only force a redraw when
        // that boolean is set.
    }

    fn param_modulation_changed(&self, _id: &str, _modulation_offset: f32) {}

    fn param_values_changed(&self) {
        // Same
    }
}

fn adjust_size(
    settings: &EguiSettings,
    current_size: (u32, u32),
    requested_size: (u32, u32),
) -> Option<(u32, u32)> {
    let min_size = (settings.min_size.0.max(1), settings.min_size.1.max(1));
    settings
        .resize_hint
        .adjust_size(requested_size.0, requested_size.1, current_size)
        .map(|size| (size.0.max(min_size.0), size.1.max(min_size.1)))
}

fn request_host_resize(
    settings: &EguiSettings,
    egui_state: &EguiState,
    requested_size: (u32, u32),
    request_resize: impl FnOnce() -> bool,
) -> bool {
    let previous_size = egui_state.size();
    let Some(new_size) = adjust_size(settings, previous_size, requested_size) else {
        return false;
    };

    egui_state.size.store(new_size);
    if request_resize() {
        true
    } else {
        egui_state.size.store(previous_size);
        false
    }
}

fn resize_window(egui_ctx: &Context, queue: &mut Queue, new_size: (u32, u32)) {
    let scale = egui_ctx.pixels_per_point();
    queue.resize(PhySize::new(
        (new_size.0 as f32 * scale).round() as u32,
        (new_size.1 as f32 * scale).round() as u32,
    ));

    egui_ctx.send_viewport_cmd(ViewportCommand::InnerSize(Vec2::new(
        new_size.0 as f32,
        new_size.1 as f32,
    )));
}

#[cfg(test)]
mod tests {
    use super::*;
    use nice_plug_core::editor::ResizeHint;

    #[test]
    fn requested_resize_is_reported_while_asking_host() {
        let egui_state = EguiState::from_size(420, 260);
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint::resizable();

        let accepted = request_host_resize(&settings, egui_state.as_ref(), (800, 600), || {
            assert_eq!(egui_state.size(), (800, 600));
            true
        });

        assert!(accepted);
        assert_eq!(egui_state.size(), (800, 600));
        assert_eq!(egui_state.applied_size.load(), None);
    }

    #[test]
    fn rejected_requested_resize_restores_previous_size() {
        let egui_state = EguiState::from_size(420, 260);
        let mut settings = EguiSettings::default();
        settings.resize_hint = ResizeHint::resizable();

        let accepted = request_host_resize(&settings, egui_state.as_ref(), (800, 600), || false);

        assert!(!accepted);
        assert_eq!(egui_state.size(), (420, 260));
        assert_eq!(egui_state.applied_size.load(), None);
    }
}

/// The window handle used for [`EguiEditor`].
struct EguiEditorHandle {
    egui_state: Arc<EguiState>,
    window: WindowHandle,
}

/// The window handle enum stored within 'WindowHandle' contains raw pointers. Is there a way around
/// having this requirement?
unsafe impl Send for EguiEditorHandle {}

impl Drop for EguiEditorHandle {
    fn drop(&mut self) {
        self.egui_state.open.store(false, Ordering::Release);
        // XXX: This should automatically happen when the handle gets dropped, but apparently not
        self.window.close();
    }
}
