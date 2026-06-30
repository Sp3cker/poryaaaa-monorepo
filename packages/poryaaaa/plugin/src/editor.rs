use crate::{
    midi_activity::{MidiActivitySnapshot, MIDI_CHANNEL_COUNT},
    params::VoicegroupLoadStatus,
    plugin::PoryaaaaBackgroundTask,
    shared_projects_json, PoryaaaaParams, PoryaaaaPlugin,
};
use egui::{Margin, Vec2};
use egui_file_dialog::FileDialog;
use nice_plug::prelude::*;
use nice_plug_egui::{create_egui_editor, resizable_window::ResizableWindow, EguiSettings};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant};

const MIN_WINDOW_WIDTH: u32 = 420;
const MIN_WINDOW_HEIGHT: u32 = 260;
const PROJECT_ROOT_BROWSE_WIDTH: f32 = 72.0;
const VOICEGROUP_LOAD_WIDTH: f32 = 56.0;
pub(crate) const MIDI_ACTIVITY_HOLD: Duration = Duration::from_millis(180);
pub(crate) const CALAMITY_REGULAR_FONT: &str = "Calamity Regular";
pub(crate) const CALAMITY_BOLD_FONT: &str = "Calamity Bold";
pub(crate) const CALAMITY_BOLD_FAMILY: &str = "Calamity Bold";

const MIDI_ACTIVITY_REPAINT: Duration = Duration::from_millis(33);

pub(crate) struct GuiState {
    pub(crate) draft_project_root: String,
    pub(crate) draft_voicegroup: String,
    project_root_dialog: FileDialog,
    pub(crate) voicegroup_status: Option<VoicegroupLoadStatus>,
    channel_activity: [MidiActivityLight; MIDI_CHANNEL_COUNT],
}

impl GuiState {
    pub(crate) fn from_params(params: &PoryaaaaParams) -> Self {
        Self {
            draft_project_root: params
                .project_root
                .read()
                .expect("project root read")
                .clone(),
            draft_voicegroup: params.voicegroup.read().expect("voicegroup read").clone(),
            project_root_dialog: project_root_dialog(None),
            voicegroup_status: params.voicegroup_status(),
            channel_activity: std::array::from_fn(|_| MidiActivityLight::default()),
        }
    }
}

pub(crate) fn apply_project_root_selection(gui_state: &mut GuiState, path: &Path) {
    gui_state.draft_project_root = path.to_string_lossy().into_owned();
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

#[derive(Default)]
pub(crate) struct MidiActivityLight {
    last_count: u64,
    active_until: Option<Instant>,
}

impl MidiActivityLight {
    pub(crate) fn is_active(&mut self, count: u64, now: Instant) -> bool {
        if count != self.last_count {
            self.last_count = count;
            self.active_until = Some(now + MIDI_ACTIVITY_HOLD);
        }

        self.active_until.is_some_and(|until| now <= until)
    }
}

fn show_midi_activity(ui: &mut egui::Ui, gui_state: &mut GuiState, snapshot: MidiActivitySnapshot) {
    let now = Instant::now();
    let mut any_active = false;

    ui.label("MIDI activity by channel");
    ui.horizontal_wrapped(|ui| {
        for (index, channel) in snapshot.channels.iter().enumerate() {
            let count = channel.note_events + channel.other_events;
            let active = gui_state.channel_activity[index].is_active(count, now);
            any_active |= active;
            show_activity_light(ui, &format!("● {:02}", index + 1), active);
        }
    });

    if any_active {
        ui.ctx().request_repaint_after(MIDI_ACTIVITY_REPAINT);
    }
}

fn show_activity_light(ui: &mut egui::Ui, label: &str, active: bool) {
    let color = if active {
        egui::Color32::from_rgb(80, 220, 120)
    } else {
        ui.visuals().weak_text_color()
    };
    ui.colored_label(color, label);
}

pub(crate) fn calamity_font_definitions() -> egui::FontDefinitions {
    let mut fonts = egui::FontDefinitions::default();
    fonts.font_data.insert(
        CALAMITY_REGULAR_FONT.to_owned(),
        Arc::new(egui::FontData::from_static(include_bytes!(
            "../../../../shared/assets/fonts/Calamity/Calamity-Regular.ttf"
        ))),
    );
    fonts.font_data.insert(
        CALAMITY_BOLD_FONT.to_owned(),
        Arc::new(egui::FontData::from_static(include_bytes!(
            "../../../../shared/assets/fonts/Calamity/Calamity-Bold.ttf"
        ))),
    );

    fonts
        .families
        .entry(egui::FontFamily::Proportional)
        .or_default()
        .insert(0, CALAMITY_REGULAR_FONT.to_owned());
    fonts.families.insert(
        egui::FontFamily::Name(CALAMITY_BOLD_FAMILY.into()),
        vec![CALAMITY_BOLD_FONT.to_owned()],
    );
    fonts
}

fn apply_calamity_fonts(ctx: &egui::Context) {
    ctx.set_fonts(calamity_font_definitions());
}

/// Builds the egui editor around Rust-owned params.
pub(crate) fn create_editor(
    params: Arc<PoryaaaaParams>,
    async_executor: AsyncExecutor<PoryaaaaPlugin>,
) -> Option<Box<dyn Editor>> {
    let egui_state = params.editor_state.clone();
    let settings = EguiSettings {
        resize_hint: ResizeHint::resizable(),
        min_size: (MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT),
        ..Default::default()
    };

    create_egui_editor(
        egui_state.clone(),
        GuiState::from_params(params.as_ref()),
        settings,
        |egui_ctx, _queue, _gui_state| {
            apply_calamity_fonts(egui_ctx);
        },
        move |ui, setter, _queue, gui_state| {
            ResizableWindow::new("poryaaaa-main").show(ui, egui_state.as_ref(), |ui| {
                show_editor_frame(ui, |ui| {
                    ui.heading("poryaaaa");
                    ui.separator();
                    show_midi_activity(ui, gui_state, params.midi_activity.snapshot());
                    ui.separator();

                    ui.label("Project root");
                    let selector =
                        show_project_root_selector(ui, &mut gui_state.draft_project_root);
                    if selector.browse_clicked {
                        let initial_directory = (!gui_state.draft_project_root.is_empty())
                            .then(|| PathBuf::from(gui_state.draft_project_root.as_str()));
                        gui_state.project_root_dialog = project_root_dialog(initial_directory);
                        gui_state.project_root_dialog.pick_directory();
                    }

                    gui_state.project_root_dialog.update(ui.ctx());
                    if let Some(path) = gui_state.project_root_dialog.take_picked() {
                        apply_project_root_selection(gui_state, &path);
                    }

                    ui.label("Voicegroup");
                    let mut load_requested = false;
                    ui.horizontal(|ui| {
                        let field_width = remaining_width_for_leading_field(
                            ui.available_width(),
                            VOICEGROUP_LOAD_WIDTH,
                            ui.spacing().item_spacing.x,
                        );
                        ui.add_sized(
                            [field_width, ui.spacing().interact_size.y],
                            egui::TextEdit::singleline(&mut gui_state.draft_voicegroup),
                        );
                        load_requested = ui
                            .add_sized(
                                [VOICEGROUP_LOAD_WIDTH, ui.spacing().interact_size.y],
                                egui::Button::new("Load"),
                            )
                            .clicked();
                    });
                    if load_requested {
                        if let Some(path) = shared_projects_json::default_projects_json_path() {
                            let status = VoicegroupLoadStatus {
                                text: format!("Loading {}", gui_state.draft_voicegroup),
                                is_error: false,
                            };
                            params.write_voicegroup_status(Some(status.clone()));
                            gui_state.voicegroup_status = Some(status);
                            async_executor.execute_gui(PoryaaaaBackgroundTask::LoadVoicegroup {
                                project_root: gui_state.draft_project_root.clone(),
                                bank: gui_state.draft_voicegroup.clone(),
                                projects_json_path: path,
                            });
                        } else {
                            let status = VoicegroupLoadStatus {
                                text: "Bad project root: HOME is not set".to_string(),
                                is_error: true,
                            };
                            params.write_voicegroup_status(Some(status.clone()));
                            gui_state.voicegroup_status = Some(status);
                        }
                    }

                    gui_state.voicegroup_status = params.voicegroup_status();
                    if params.take_host_restart_request() && !setter.request_restart() {
                        let status = VoicegroupLoadStatus {
                            text: "Loaded voicegroup, but host restart is unavailable".to_string(),
                            is_error: true,
                        };
                        params.write_voicegroup_status(Some(status.clone()));
                        gui_state.voicegroup_status = Some(status);
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
    )
}
