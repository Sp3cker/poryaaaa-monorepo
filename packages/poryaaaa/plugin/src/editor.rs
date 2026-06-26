use crate::{voicegroup, PoryaaaaParams};
use egui::{Margin, Vec2};
use egui_file_dialog::FileDialog;
use nice_plug::prelude::*;
use nice_plug_egui::{create_egui_editor, resizable_window::ResizableWindow, EguiState};
use std::any::Any;
use std::path::{Path, PathBuf};
use std::sync::Arc;

const MIN_WINDOW_WIDTH: u32 = 420;
const MIN_WINDOW_HEIGHT: u32 = 260;
const PROJECT_ROOT_BROWSE_WIDTH: f32 = 72.0;
const VOICEGROUP_LOAD_WIDTH: f32 = 56.0;

struct GuiState {
    project_root_dialog: FileDialog,
    voicegroup_status: Option<voicegroup::VoicegroupLoadStatus>,
}

impl Default for GuiState {
    fn default() -> Self {
        Self {
            project_root_dialog: project_root_dialog(None),
            voicegroup_status: None,
        }
    }
}

pub(crate) fn apply_project_root_selection(params: &PoryaaaaParams, path: &Path) {
    *params.project_root.write().expect("project root write") = path.to_string_lossy().into_owned();
}

/// Loads the currently entered voicegroup and returns GUI-ready status text.
pub(crate) fn load_voicegroup_from_params(
    params: &PoryaaaaParams,
) -> voicegroup::VoicegroupLoadStatus {
    voicegroup::load_voicegroup_from_params(params)
}

pub(crate) struct ProjectRootSelectorResponse {
    #[cfg_attr(not(test), allow(dead_code))]
    pub text_rect: egui::Rect,
    pub browse_clicked: bool,
}

/// Computes the width left for a leading text field before a fixed trailing button.
pub(crate) fn remaining_width_for_leading_field(
    available_width: f32,
    trailing_width: f32,
    item_spacing: f32,
) -> f32 {
    (available_width - trailing_width - item_spacing).max(0.0)
}

/// Builds the project-root row so the path field consumes all width left by Browse.
pub(crate) fn show_project_root_selector(
    ui: &mut egui::Ui,
    project_root: &mut String,
) -> ProjectRootSelectorResponse {
    let mut text_rect = egui::Rect::NOTHING;
    let mut browse_clicked = false;

    ui.horizontal(|ui| {
        let field_width = remaining_width_for_leading_field(
            ui.available_width(),
            PROJECT_ROOT_BROWSE_WIDTH,
            ui.spacing().item_spacing.x,
        );
        let response = ui.add_sized(
            [field_width, ui.spacing().interact_size.y],
            egui::TextEdit::singleline(project_root),
        );
        text_rect = response.rect;
        browse_clicked = ui
            .add_sized(
                [PROJECT_ROOT_BROWSE_WIDTH, ui.spacing().interact_size.y],
                egui::Button::new("Browse"),
            )
            .clicked();
    });

    ProjectRootSelectorResponse {
        text_rect,
        browse_clicked,
    }
}

fn project_root_dialog(initial_directory: Option<PathBuf>) -> FileDialog {
    let dialog = FileDialog::new()
        .title("Choose Project Root")
        .default_size(Vec2::new(620.0, 420.0))
        .min_size(Vec2::new(240.0, 160.0))
        .show_search(false)
        .show_new_folder_button(false)
        .show_all_files_filter(false)
        .max_selections(1);

    if let Some(initial_directory) = initial_directory {
        dialog.initial_directory(initial_directory)
    } else {
        dialog
    }
}

/// Shows the editor frame and makes its background fill the resizable window.
pub(crate) fn show_editor_frame<R>(
    ui: &mut egui::Ui,
    add_contents: impl FnOnce(&mut egui::Ui) -> R,
) -> egui::InnerResponse<R> {
    egui::Frame::new()
        .fill(ui.visuals().window_fill())
        .inner_margin(Margin::same(8))
        .show(ui, |ui| {
            ui.set_min_size(ui.available_size());
            add_contents(ui)
        })
}

struct ResizableEguiEditor {
    inner: Box<dyn Editor>,
    egui_state: Arc<EguiState>,
}

impl Editor for ResizableEguiEditor {
    fn spawn(
        &self,
        parent: ParentWindowHandle,
        context: Arc<dyn GuiContext>,
    ) -> Box<dyn Any + Send> {
        self.inner.spawn(parent, context)
    }

    fn size(&self) -> (u32, u32) {
        self.inner.size()
    }

    fn set_scale_factor(&self, factor: f32) -> bool {
        self.inner.set_scale_factor(factor)
    }

    fn param_value_changed(&self, id: &str, normalized_value: f32) {
        self.inner.param_value_changed(id, normalized_value);
    }

    fn param_modulation_changed(&self, id: &str, modulation_offset: f32) {
        self.inner.param_modulation_changed(id, modulation_offset);
    }

    fn param_values_changed(&self) {
        self.inner.param_values_changed();
    }

    fn on_virtual_key_from_host(
        &self,
        key_code: VirtualKeyCode,
        is_down: bool,
        modifiers: Modifiers,
    ) -> bool {
        self.inner
            .on_virtual_key_from_host(key_code, is_down, modifiers)
    }

    fn set_size(&self, width: u32, height: u32) -> bool {
        self.egui_state
            .set_requested_size((width.max(MIN_WINDOW_WIDTH), height.max(MIN_WINDOW_HEIGHT)));
        true
    }

    fn resize_hint(&self) -> ResizeHint {
        ResizeHint::resizable()
    }
}

/// Builds the egui editor around Rust-owned params.
pub(crate) fn create_editor(params: Arc<PoryaaaaParams>) -> Option<Box<dyn Editor>> {
    let egui_state = params.editor_state.clone();
    let resize_state = egui_state.clone();

    let editor = create_egui_editor(
        egui_state.clone(),
        GuiState::default(),
        Default::default(),
        |_egui_ctx, _queue, _gui_state| {},
        move |ui, _setter, _queue, gui_state| {
            ResizableWindow::new("poryaaaa-main")
                .min_size(Vec2::new(MIN_WINDOW_WIDTH as f32, MIN_WINDOW_HEIGHT as f32))
                .show(ui, egui_state.as_ref(), |ui| {
                    show_editor_frame(ui, |ui| {
                        ui.heading("poryaaaa");
                        ui.separator();

                        ui.label("Project root");
                        let mut project_root =
                            params.project_root.write().expect("project root write");
                        let selector = show_project_root_selector(ui, &mut project_root);
                        if selector.browse_clicked {
                            let initial_directory = (!project_root.is_empty())
                                .then(|| PathBuf::from(project_root.as_str()));
                            gui_state.project_root_dialog = project_root_dialog(initial_directory);
                            gui_state.project_root_dialog.pick_directory();
                        }
                        drop(project_root);

                        gui_state.project_root_dialog.update(ui.ctx());
                        if let Some(path) = gui_state.project_root_dialog.take_picked() {
                            apply_project_root_selection(params.as_ref(), &path);
                        }

                        ui.label("Voicegroup");
                        let mut load_requested = false;
                        let mut voicegroup = params.voicegroup.write().expect("voicegroup write");
                        ui.horizontal(|ui| {
                            let field_width = remaining_width_for_leading_field(
                                ui.available_width(),
                                VOICEGROUP_LOAD_WIDTH,
                                ui.spacing().item_spacing.x,
                            );
                            ui.add_sized(
                                [field_width, ui.spacing().interact_size.y],
                                egui::TextEdit::singleline(&mut *voicegroup),
                            );
                            load_requested = ui
                                .add_sized(
                                    [VOICEGROUP_LOAD_WIDTH, ui.spacing().interact_size.y],
                                    egui::Button::new("Load"),
                                )
                                .clicked();
                        });
                        drop(voicegroup);
                        if load_requested {
                            gui_state.voicegroup_status =
                                Some(load_voicegroup_from_params(params.as_ref()));
                        }

                        if let Some(status) = &gui_state.voicegroup_status {
                            if status.is_error {
                                ui.colored_label(egui::Color32::RED, &status.text);
                            } else {
                                ui.label(&status.text);
                            }
                        }
                    });
                });
        },
    )?;

    Some(Box::new(ResizableEguiEditor {
        inner: editor,
        egui_state: resize_state,
    }))
}
