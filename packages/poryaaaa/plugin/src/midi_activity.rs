use std::sync::atomic::{AtomicU64, Ordering};

pub(crate) const MIDI_CHANNEL_COUNT: usize = 16;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) struct MidiChannelActivitySnapshot {
    pub(crate) note_events: u64,
    pub(crate) other_events: u64,
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) struct MidiActivitySnapshot {
    pub(crate) channels: [MidiChannelActivitySnapshot; MIDI_CHANNEL_COUNT],
}

#[derive(Default)]
struct MidiChannelActivity {
    note_events: AtomicU64,
    other_events: AtomicU64,
}

#[derive(Default)]
pub(crate) struct MidiActivity {
    channels: [MidiChannelActivity; MIDI_CHANNEL_COUNT],
}

impl MidiActivity {
    pub(crate) fn record_note_event(&self, channel: u8) {
        if let Some(channel) = self.channels.get(channel as usize) {
            channel.note_events.fetch_add(1, Ordering::Relaxed);
        }
    }

    pub(crate) fn record_other_event(&self, channel: u8) {
        if let Some(channel) = self.channels.get(channel as usize) {
            channel.other_events.fetch_add(1, Ordering::Relaxed);
        }
    }

    pub(crate) fn snapshot(&self) -> MidiActivitySnapshot {
        MidiActivitySnapshot {
            channels: std::array::from_fn(|index| MidiChannelActivitySnapshot {
                note_events: self.channels[index].note_events.load(Ordering::Relaxed),
                other_events: self.channels[index].other_events.load(Ordering::Relaxed),
            }),
        }
    }
}
