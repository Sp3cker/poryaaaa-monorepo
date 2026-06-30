use crate::{
    midi_activity::{MidiActivitySnapshot, MIDI_CHANNEL_COUNT},
    params::VoicegroupLoadStatus,
    plugin::PoryaaaaBackgroundTask,
    shared_projects_json, PoryaaaaParams, PoryaaaaPlugin,
};
use iced_audio::Gesture;
use nice_plug::prelude::*;
use nice_plug_iced::iced::{
    self,
    widget::{button, column, row, text, text_input, Column, Row},
    Center, Color, Element, Length, PollSubNotifier, Task, Theme,
};
use nice_plug_iced::{
    application, create_iced_editor, EditorSettings, EditorState, NiceGuiContext,
};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant};

pub(crate) const MIDI_ACTIVITY_HOLD: Duration = Duration::from_millis(180);
pub(crate) const STATUS_ERROR_COLOR: Color = Color {
    r: 0.95,
    g: 0.25,
    b: 0.25,
    a: 1.0,
};
pub(crate) const STATUS_SUCCESS_COLOR: Color = Color {
    r: 0.35,
    g: 0.85,
    b: 0.45,
    a: 1.0,
};
pub(crate) const STATUS_PENDING_COLOR: Color = Color {
    r: 0.82,
    g: 0.82,
    b: 0.82,
    a: 1.0,
};

#[derive(Debug, Clone)]
enum Message {
    Poll,
    ProjectRootChanged(String),
    VoicegroupChanged(String),
    BrowseProjectRoot,
    ProjectRootSelected(Option<PathBuf>),
    LoadVoicegroup,
    VolumeGestured(Gesture),
    ReverbGestured(Gesture),
}

pub(crate) struct GuiState {
    pub(crate) draft_project_root: String,
    pub(crate) draft_voicegroup: String,
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
            voicegroup_status: params.voicegroup_status(),
            channel_activity: std::array::from_fn(|_| MidiActivityLight::default()),
        }
    }
}

pub(crate) fn apply_project_root_selection(gui_state: &mut GuiState, path: &Path) {
    gui_state.draft_project_root = path.to_string_lossy().into_owned();
}

pub(crate) fn voicegroup_status_presentation(
    status: &Option<VoicegroupLoadStatus>,
) -> Option<(String, Color)> {
    status.as_ref().map(|status| {
        let color = if status.is_error {
            STATUS_ERROR_COLOR
        } else if status.text.starts_with("Loaded ") {
            STATUS_SUCCESS_COLOR
        } else {
            STATUS_PENDING_COLOR
        };
        (status.text.clone(), color)
    })
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct VoicegroupLoadRequest {
    pub(crate) project_root: String,
    pub(crate) bank: String,
    pub(crate) projects_json_path: PathBuf,
}

pub(crate) fn prepare_voicegroup_load_request(
    params: &PoryaaaaParams,
    gui_state: &mut GuiState,
) -> Option<VoicegroupLoadRequest> {
    match shared_projects_json::default_projects_json_path() {
        Some(projects_json_path) => {
            let status = VoicegroupLoadStatus {
                text: format!("Loading {}", gui_state.draft_voicegroup),
                is_error: false,
            };
            params.write_voicegroup_status(Some(status.clone()));
            gui_state.voicegroup_status = Some(status);
            Some(VoicegroupLoadRequest {
                project_root: gui_state.draft_project_root.clone(),
                bank: gui_state.draft_voicegroup.clone(),
                projects_json_path,
            })
        }
        None => {
            let status = VoicegroupLoadStatus {
                text: "Bad project root: HOME is not set".to_string(),
                is_error: true,
            };
            params.write_voicegroup_status(Some(status.clone()));
            gui_state.voicegroup_status = Some(status);
            None
        }
    }
}

#[derive(Default)]
pub(crate) struct MidiActivityLight {
    last_count: u64,
    active_until: Option<Instant>,
    active: bool,
}

impl MidiActivityLight {
    pub(crate) fn refresh(&mut self, count: u64, now: Instant) {
        if count != self.last_count {
            self.last_count = count;
            self.active_until = Some(now + MIDI_ACTIVITY_HOLD);
        }

        self.active = self.active_until.is_some_and(|until| now <= until);
    }
}

struct PoryaaaaEditorState {
    params: Arc<PoryaaaaParams>,
    async_executor: AsyncExecutor<PoryaaaaPlugin>,
}

struct PoryaaaaGui {
    editor_state: EditorState<PoryaaaaEditorState>,
    nice_ctx: NiceGuiContext,
    gui_state: GuiState,
}

impl PoryaaaaGui {
    fn new(editor_state: EditorState<PoryaaaaEditorState>, nice_ctx: NiceGuiContext) -> Self {
        let gui_state = GuiState::from_params(editor_state.params.as_ref());
        let mut this = Self {
            editor_state,
            nice_ctx,
            gui_state,
        };
        this.refresh_midi_activity();
        this
    }

    fn theme(&self) -> Option<Theme> {
        Some(Theme::Dark)
    }

    fn update(&mut self, message: Message) -> Task<Message> {
        let params = self.editor_state.params.clone();

        match message {
            Message::Poll => {
                self.gui_state.voicegroup_status = params.voicegroup_status();
                self.refresh_midi_activity();
                self.handle_pending_host_restart();
            }
            Message::ProjectRootChanged(project_root) => {
                self.gui_state.draft_project_root = project_root;
            }
            Message::VoicegroupChanged(voicegroup) => {
                self.gui_state.draft_voicegroup = voicegroup;
            }
            Message::BrowseProjectRoot => {
                return browse_project_root_task(self.gui_state.draft_project_root.clone());
            }
            Message::ProjectRootSelected(path) => {
                if let Some(path) = path {
                    apply_project_root_selection(&mut self.gui_state, &path);
                }
            }
            Message::LoadVoicegroup => {
                if let Some(request) =
                    prepare_voicegroup_load_request(params.as_ref(), &mut self.gui_state)
                {
                    self.editor_state.async_executor.execute_gui(
                        PoryaaaaBackgroundTask::LoadVoicegroup {
                            project_root: request.project_root,
                            bank: request.bank,
                            projects_json_path: request.projects_json_path,
                        },
                    );
                }
            }
            Message::VolumeGestured(gesture) => {
                let setter = self.nice_ctx.param_setter();
                iced_audio::param::set_nice_param(&params.volume, gesture, &setter);
            }
            Message::ReverbGestured(gesture) => {
                let setter = self.nice_ctx.param_setter();
                iced_audio::param::set_nice_param(&params.reverb, gesture, &setter);
            }
        }

        Task::none()
    }

    fn view(&self) -> Element<'_, Message> {
        column![
            text("poryaaaa").size(22),
            self.view_config_section(),
            self.view_knob_section(),
            self.view_midi_activity_section(),
        ]
        .padding(10)
        .spacing(8)
        .width(Length::Fill)
        .height(Length::Fill)
        .into()
    }

    fn view_config_section(&self) -> Column<'_, Message> {
        column![
            text("Project Root"),
            row![
                text_input("Project Root", &self.gui_state.draft_project_root)
                    .on_input(Message::ProjectRootChanged)
                    .width(Length::Fill),
                button("Browse").on_press(Message::BrowseProjectRoot),
            ]
            .spacing(8)
            .width(Length::Fill),
            text("Voicegroup"),
            row![
                text_input("Voicegroup", &self.gui_state.draft_voicegroup)
                    .on_input(Message::VoicegroupChanged)
                    .width(Length::Fill),
                button("Load").on_press(Message::LoadVoicegroup),
            ]
            .spacing(8)
            .width(Length::Fill),
            self.view_status_section(),
        ]
        .spacing(6)
        .width(Length::Fill)
    }

    fn view_knob_section(&self) -> Row<'_, Message> {
        let params = &self.editor_state.params;

        row![
            audio_knob_column(&params.volume, Message::VolumeGestured),
            audio_knob_column(&params.reverb, Message::ReverbGestured),
        ]
        .spacing(24)
        .align_y(Center)
        .width(Length::Fill)
    }

    fn view_status_section(&self) -> Element<'_, Message> {
        match voicegroup_status_presentation(&self.gui_state.voicegroup_status) {
            Some((text_value, color)) => text(text_value).size(13).color(color).into(),
            None => text("").size(13).into(),
        }
    }

    fn view_midi_activity_section(&self) -> Column<'_, Message> {
        let mut first_row = Row::new().spacing(8);
        let mut second_row = Row::new().spacing(8);
        for (index, light) in self.gui_state.channel_activity.iter().enumerate() {
            let mark = if light.active { "●" } else { "○" };
            let channel = text(format!("{:>2} {mark}", index + 1)).size(13);
            if index < 8 {
                first_row = first_row.push(channel);
            } else {
                second_row = second_row.push(channel);
            }
        }

        column![text("MIDI activity"), first_row, second_row]
            .spacing(3)
            .width(Length::Fill)
    }

    fn refresh_midi_activity(&mut self) {
        let snapshot = self.editor_state.params.midi_activity.snapshot();
        refresh_midi_activity_lights(
            &mut self.gui_state.channel_activity,
            snapshot,
            Instant::now(),
        );
    }

    fn handle_pending_host_restart(&mut self) {
        if self.editor_state.params.take_host_restart_request()
            && !self.nice_ctx.context.request_restart()
        {
            let status = VoicegroupLoadStatus {
                text: "Loaded voicegroup, but host restart is unavailable".to_string(),
                is_error: true,
            };
            self.editor_state
                .params
                .write_voicegroup_status(Some(status.clone()));
            self.gui_state.voicegroup_status = Some(status);
        }
    }
}

fn browse_project_root_task(initial_directory: String) -> Task<Message> {
    Task::perform(
        async move {
            let initial_directory = if initial_directory.is_empty() {
                None
            } else {
                Some(PathBuf::from(initial_directory))
            };
            let mut dialog = rfd::AsyncFileDialog::new().set_title("Choose Project Root");
            if let Some(initial_directory) = initial_directory {
                dialog = dialog.set_directory(initial_directory);
            }
            dialog
                .pick_folder()
                .await
                .map(|handle| handle.path().to_path_buf())
        },
        Message::ProjectRootSelected,
    )
}

fn audio_knob_column<'a>(
    param: &'a IntParam,
    on_gesture: fn(Gesture) -> Message,
) -> Column<'a, Message> {
    use iced_audio::param::nice_to_iced;

    column![
        text(param.name()),
        iced_audio::Knob::new(nice_to_iced(param)).on_gesture(on_gesture),
        text(param.to_string()),
    ]
    .spacing(4)
    .align_x(Center)
}

fn refresh_midi_activity_lights(
    channel_activity: &mut [MidiActivityLight; MIDI_CHANNEL_COUNT],
    snapshot: MidiActivitySnapshot,
    now: Instant,
) {
    for (index, channel) in snapshot.channels.iter().enumerate() {
        let count = channel.note_events + channel.other_events;
        channel_activity[index].refresh(count, now);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn midi_activity_light_holds_recent_counter_changes_briefly() {
        let mut light = MidiActivityLight::default();
        let now = Instant::now();

        light.refresh(1, now);
        assert!(light.active);
        light.refresh(1, now + MIDI_ACTIVITY_HOLD + Duration::from_millis(1));
        assert!(!light.active);
        light.refresh(2, now + Duration::from_secs(1));
        assert!(light.active);
    }
}

pub(crate) fn create_editor(
    params: Arc<PoryaaaaParams>,
    async_executor: AsyncExecutor<PoryaaaaPlugin>,
    notifier: PollSubNotifier,
) -> Option<Box<dyn Editor>> {
    create_iced_editor(
        params.window_state.clone(),
        PoryaaaaEditorState {
            params,
            async_executor,
        },
        notifier,
        EditorSettings {
            always_redraw: true,
            ..Default::default()
        },
        |editor_state, nice_ctx| {
            application(
                editor_state,
                nice_ctx,
                PoryaaaaGui::new,
                PoryaaaaGui::update,
                PoryaaaaGui::view,
            )
            .theme(PoryaaaaGui::theme)
            .subscription(|_| iced::poll_events().map(|_| Message::Poll))
            .run()
        },
    )
}
