use crate::PoryaaaaParams;
use egui::{Margin, Vec2};
use nice_plug::prelude::*;
use nice_plug_egui::{create_egui_editor, resizable_window::ResizableWindow};
use std::sync::Arc;

const MIN_WINDOW_WIDTH: u32 = 420;
const MIN_WINDOW_HEIGHT: u32 = 260;

#[derive(Default)]
struct GuiState {
    project_root: String,
    voicegroup: String,
}

/// Builds the egui editor around Rust-owned params and GUI state.
pub(crate) fn create_editor(params: Arc<PoryaaaaParams>) -> Option<Box<dyn Editor>> {
    let egui_state = params.editor_state.clone();

    create_egui_editor(
        params.editor_state.clone(),
        GuiState::default(),
        Default::default(),
        |_egui_ctx, _queue, _gui_state| {},
        move |ui, _setter, _queue, gui_state| {
            ResizableWindow::new("poryaaaa-main")
                .min_size(Vec2::new(MIN_WINDOW_WIDTH as f32, MIN_WINDOW_HEIGHT as f32))
                .show(ui, egui_state.as_ref(), |ui| {
                    egui::Frame::new()
                        .inner_margin(Margin::same(8))
                        .show(ui, |ui| {
                            ui.heading("poryaaaa");
                            ui.separator();

                            ui.label("Project root");
                            ui.text_edit_singleline(&mut gui_state.project_root);

                            ui.label("Voicegroup");
                            ui.text_edit_singleline(&mut gui_state.voicegroup);
                        });
                });
        },
    )
}
