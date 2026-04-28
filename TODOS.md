This file tracks known issues and to-do items for the plugin.

- GUI implemented via Dear ImGui + GLFW (floating window). Known gaps:
    - High-DPI / scale-aware window sizing not yet implemented.
    - File browser dialog for Project Root not yet implemented.
- Full midi -> .wav output regression tests
- Architecture document
- Identify any performance issues/improvements
- **Layered DSP rewrite of m4a_channel.c / m4a_engine.c** — see NEXT_SESSION.md.
  Splits clean voice synthesis from per-channel hardware filtering and a single
  output resampler.  Replaces the chain of one-off fixes accumulated 2026-04.
