use nice_plug::prelude::NoteEvent;

const MAX_RENDER_FRAMES: usize = 2048;

// Keeps the process loop independent from the C FFI wrapper shape.
pub(crate) trait ProcessRuntime {
    fn set_tempo_bpm(&mut self, bpm: f64);
    fn note_on(&mut self, track: i32, key: u8, velocity: u8);
    fn note_off(&mut self, track: i32, key: u8);
    fn program_change(&mut self, track: i32, program: u8);
    fn cc(&mut self, track: i32, cc: u8, value: u8);
    fn pitch_bend(&mut self, track: i32, bend: i16);
    fn process(&mut self, left: &mut [f32], right: &mut [f32]);
}

// Renders one NicePlug audio block with sample-accurate event ordering.
pub(crate) fn process_stereo<R, NextEvent>(
    runtime: &mut R,
    left: &mut [f32],
    right: &mut [f32],
    tempo_bpm: Option<f64>,
    mut next_event: NextEvent,
) where
    R: ProcessRuntime,
    NextEvent: FnMut() -> Option<NoteEvent<()>>,
{
    let frames = left.len().min(right.len());
    if frames == 0 {
        return;
    }

    if let Some(bpm) = tempo_bpm.filter(|bpm| bpm.is_finite() && *bpm > 0.0) {
        runtime.set_tempo_bpm(bpm);
    }

    let mut frame_pos = 0;
    let mut event = next_event();
    while frame_pos < frames {
        while event
            .as_ref()
            .is_some_and(|event| event.timing() as usize <= frame_pos)
        {
            if let Some(current) = event.take() {
                apply_event(runtime, current);
            }
            event = next_event();
        }

        let next_event_frame = event
            .as_ref()
            .map(|event| (event.timing() as usize).min(frames))
            .unwrap_or(frames);
        if next_event_frame > frame_pos {
            render_span(
                runtime,
                &mut left[frame_pos..next_event_frame],
                &mut right[frame_pos..next_event_frame],
            );
            frame_pos = next_event_frame;
        }
    }
}

pub(crate) fn clear_stereo(left: &mut [f32], right: &mut [f32]) {
    let frames = left.len().min(right.len());
    left[..frames].fill(0.0);
    right[..frames].fill(0.0);
}

fn render_span<R: ProcessRuntime>(runtime: &mut R, left: &mut [f32], right: &mut [f32]) {
    let mut offset = 0;
    while offset < left.len() {
        let end = (offset + MAX_RENDER_FRAMES).min(left.len());
        runtime.process(&mut left[offset..end], &mut right[offset..end]);
        offset = end;
    }
}

fn apply_event<R: ProcessRuntime>(runtime: &mut R, event: NoteEvent<()>) {
    match event {
        NoteEvent::NoteOn {
            channel,
            note,
            velocity,
            ..
        } => runtime.note_on(channel.into(), note, unit_to_midi(velocity)),
        NoteEvent::NoteOff { channel, note, .. } => runtime.note_off(channel.into(), note),
        NoteEvent::Choke { channel, note, .. } => runtime.note_off(channel.into(), note),
        NoteEvent::MidiProgramChange {
            channel, program, ..
        } => runtime.program_change(channel.into(), program),
        NoteEvent::MidiCC {
            channel, cc, value, ..
        } => runtime.cc(channel.into(), cc, unit_to_midi(value)),
        NoteEvent::MidiPitchBend { channel, value, .. } => {
            runtime.pitch_bend(channel.into(), unit_to_pitch_bend(value));
        }
        _ => {}
    }
}

fn unit_to_midi(value: f32) -> u8 {
    (value.clamp(0.0, 1.0) * 127.0).round() as u8
}

fn unit_to_pitch_bend(value: f32) -> i16 {
    ((value.clamp(0.0, 1.0) * 16383.0).round() as i32 - 8192).clamp(-8192, 8191) as i16
}

#[cfg(test)]
mod tests {
    use nice_plug::prelude::NoteEvent;

    #[derive(Debug, PartialEq, Eq)]
    enum Action {
        Tempo(u32),
        NoteOn(i32, u8, u8),
        Program(i32, u8),
        Cc(i32, u8, u8),
        Pitch(i32, i16),
        Render(usize),
    }

    #[derive(Default)]
    struct RecordingRuntime {
        actions: Vec<Action>,
    }

    impl super::ProcessRuntime for RecordingRuntime {
        fn set_tempo_bpm(&mut self, bpm: f64) {
            self.actions.push(Action::Tempo(bpm.round() as u32));
        }

        fn note_on(&mut self, track: i32, key: u8, velocity: u8) {
            self.actions.push(Action::NoteOn(track, key, velocity));
        }

        fn note_off(&mut self, _track: i32, _key: u8) {}
        fn program_change(&mut self, track: i32, program: u8) {
            self.actions.push(Action::Program(track, program));
        }

        fn cc(&mut self, track: i32, cc: u8, value: u8) {
            self.actions.push(Action::Cc(track, cc, value));
        }

        fn pitch_bend(&mut self, track: i32, bend: i16) {
            self.actions.push(Action::Pitch(track, bend));
        }

        fn process(&mut self, left: &mut [f32], right: &mut [f32]) {
            self.actions.push(Action::Render(left.len()));
            for (index, sample) in left.iter_mut().enumerate() {
                *sample = index as f32 + 1.0;
            }
            for (index, sample) in right.iter_mut().enumerate() {
                *sample = -(index as f32 + 1.0);
            }
        }
    }

    #[test]
    fn process_interleaves_timed_events_with_render_chunks() {
        let mut runtime = RecordingRuntime::default();
        let mut left = [0.0; 8];
        let mut right = [0.0; 8];
        let mut events = vec![
            NoteEvent::NoteOn {
                timing: 0,
                voice_id: None,
                channel: 2,
                note: 64,
                velocity: 1.0,
            },
            NoteEvent::MidiProgramChange {
                timing: 3,
                channel: 2,
                program: 9,
            },
        ]
        .into_iter();

        super::process_stereo(&mut runtime, &mut left, &mut right, Some(123.0), || {
            events.next()
        });

        assert_eq!(
            runtime.actions,
            [
                Action::Tempo(123),
                Action::NoteOn(2, 64, 127),
                Action::Render(3),
                Action::Program(2, 9),
                Action::Render(5),
            ]
        );
        assert_eq!(left[0], 1.0);
        assert_eq!(right[0], -1.0);
    }

    #[test]
    fn midi_program_change_is_forwarded_to_runtime() {
        let mut runtime = RecordingRuntime::default();
        let mut left = [0.0; 1];
        let mut right = [0.0; 1];
        let mut events = vec![NoteEvent::MidiProgramChange {
            timing: 0,
            channel: 4,
            program: 17,
        }]
        .into_iter();

        super::process_stereo(&mut runtime, &mut left, &mut right, None, || events.next());

        assert_eq!(runtime.actions[0], Action::Program(4, 17));
    }

    #[test]
    fn process_chunks_render_spans_above_engine_limit() {
        let mut runtime = RecordingRuntime::default();
        let mut left = vec![0.0; 4097];
        let mut right = vec![0.0; 4097];

        super::process_stereo(&mut runtime, &mut left, &mut right, None, || None);

        assert_eq!(
            runtime.actions,
            [
                Action::Render(2048),
                Action::Render(2048),
                Action::Render(1)
            ]
        );
    }

    #[test]
    fn process_translates_controller_and_pitch_values() {
        let mut runtime = RecordingRuntime::default();
        let mut left = [0.0; 1];
        let mut right = [0.0; 1];
        let mut events = vec![
            NoteEvent::MidiCC {
                timing: 0,
                channel: 1,
                cc: 7,
                value: 0.5,
            },
            NoteEvent::MidiPitchBend {
                timing: 0,
                channel: 1,
                value: 1.0,
            },
        ]
        .into_iter();

        super::process_stereo(&mut runtime, &mut left, &mut right, None, || events.next());

        assert_eq!(
            runtime.actions,
            [
                Action::Cc(1, 7, 64),
                Action::Pitch(1, 8191),
                Action::Render(1)
            ]
        );
    }
}
