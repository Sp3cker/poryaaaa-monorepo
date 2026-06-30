use crate::{
    midi_activity::{MidiActivitySnapshot, MIDI_CHANNEL_COUNT},
    params::VoicegroupLoadStatus,
    plugin::PoryaaaaBackgroundTask,
    shared_projects_json, PoryaaaaParams, PoryaaaaPlugin,
};
use iced_audio::Gesture;
use nice_plug::prelude::*;
use nice_plug_iced::iced::{
    self, Center, Element, Length, PollSubNotifier, Task, Theme,
    widget::{Column, Row, button, column, row, text, text_input},
};
use nice_plug_iced::{
    EditorSettings, EditorState, NiceGuiContext, WindowState, application, create_iced_editor,
};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant};

const MIN_WINDOW_WIDTH: u32 = 420;
const MIN_WINDOW_HEIGHT: u32 = 260;
pub(crate) const MIDI_ACTIVITY_HOLD: Duration = Duration::from_millis(180);

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

pub(crate) fn apply_optional_project_root_selection(
    gui_state: &mut GuiState,
    path: Option<PathBuf>,
) {
    if let Some(path) = path {
        apply_project_root_selection(gui_state, &path);
    }
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
    pub(crate) fn is_active(&mut self, count: u64, now: Instant) -> bool {
        if count != self.last_count {
            self.last_count = count;
            self.active_until = Some(now + MIDI_ACTIVITY_HOLD);
        }

        self.active = self.active_until.is_some_and(|until| now <= until);
        self.active
    }

    fn active(&self) -> bool {
        self.active
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
                apply_optional_project_root_selection(&mut self.gui_state, path);
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
            text("poryaaaa").size(24),
            self.view_config_section(),
            self.view_knob_section(),
            self.view_status_section(),
            self.view_midi_activity_section(),
        ]
        .padding(12)
        .spacing(10)
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
    }

    fn view_status_section(&self) -> Element<'_, Message> {
        match &self.gui_state.voicegroup_status {
            Some(status) if status.is_error => text(format!("Error: {}", status.text)).into(),
            Some(status) => text(status.text.clone()).into(),
            None => text("Ready").into(),
        }
    }

    fn view_midi_activity_section(&self) -> Column<'_, Message> {
        let mut lights = Row::new().spacing(6);
        for (index, light) in self.gui_state.channel_activity.iter().enumerate() {
            let mark = if light.active() { "●" } else { "○" };
            lights = lights.push(text(format!("{mark} {}", index + 1)).size(14));
        }

        column![text("MIDI activity"), lights].spacing(4)
    }

    fn refresh_midi_activity(&mut self) -> bool {
        let snapshot = self.editor_state.params.midi_activity.snapshot();
        refresh_midi_activity_lights(&mut self.gui_state.channel_activity, snapshot, Instant::now())
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
) -> bool {
    let mut any_active = false;
    for (index, channel) in snapshot.channels.iter().enumerate() {
        let count = channel.note_events + channel.other_events;
        any_active |= channel_activity[index].is_active(count, now);
    }
    any_active
}

pub(crate) fn create_editor(
    params: Arc<PoryaaaaParams>,
    async_executor: AsyncExecutor<PoryaaaaPlugin>,
    notifier: PollSubNotifier,
) -> Option<Box<dyn Editor>> {
    let window_state = params.window_state.clone();
    let editor = create_iced_editor(
        window_state.clone(),
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
    )?;

    Some(Box::new(ResizableIcedEditor {
        inner: editor,
        window_state,
    }))
}

struct ResizableIcedEditor {
    inner: Box<dyn Editor>,
    window_state: Arc<WindowState>,
}

impl Editor for ResizableIcedEditor {
    fn spawn(
        &self,
        parent: ParentWindowHandle,
        context: Arc<dyn GuiContext>,
    ) -> Box<dyn std::any::Any + Send> {
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

    fn set_size(&self, width: u32, height: u32) -> bool {
        let (width, height) = self.adjust_size(width, height).expect("resizable editor");
        self.window_state.set_requested_logical_size((width, height));
        true
    }

    fn adjust_size(&self, width: u32, height: u32) -> Option<(u32, u32)> {
        Some((width.max(MIN_WINDOW_WIDTH), height.max(MIN_WINDOW_HEIGHT)))
    }

    fn resize_hint(&self) -> ResizeHint {
        ResizeHint::resizable()
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
}
