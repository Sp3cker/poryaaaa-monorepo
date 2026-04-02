# Voicegroup Swapper

## Goal

Allow the plugin GUI to override the sample source used by a voicegroup slot from inside the plugin.

This feature will:

- keep the existing voice index selector on the `Voices` tab
- add a filterable sound asset selector on the selected voice's editor page
- only allow same-nature swaps
- apply swaps by reloading the voicegroup
- persist overrides in plugin state

## Agreed Constraints

- The user selects a voice slot the same way they do now: by index.
- The new selector lives on the selected voice's settings page.
- Project-wide browsing is required.
- Applying an override may require a reload.
- Same-type-only swapping is acceptable:
  - `DirectSound` and `DirectSound No Resample` may only swap with project-wide DirectSound assets
  - `Programmable Wave` may only swap with project-wide programmable wave assets
  - square, noise, keysplit, drum kit, and cry voices do not get a sample-swap selector
- Sample file names are assumed unique and stable across the project.
- The user-facing selector key is the sample file name.
- A third-party ImGui searchable-combo helper is allowed, but is not required.

## Main Design Decision

 build an in-memory project asset index from the same project discovery and asset-symbol parsing that already exists in `plugin/voicegroup_loader.c`.

Reasons:

- the existing loader already discovers asset files and maps symbols to asset paths
- the missing piece is a reusable catalog, not a database
- an in-memory index is sufficient for a project-wide browser and reload-time override application

## Implementation Shape

### New Module

Add a new module:

- `plugin/project_asset_index.h`
- `plugin/project_asset_index.c`

Use C, not C++.

Reason:

- this module needs to be consumed by `plugin/m4a_plugin.c`
- the loader is already C
- the data needed here is simple fixed-string metadata; C++ wrappers are unnecessary

The GUI in `plugin/m4a_gui.cpp` can consume this through plain C pointers passed from the plugin layer.

## Data Model

### Asset Kind

```c
typedef enum {
	PROJECT_ASSET_NONE = 0,
	PROJECT_ASSET_DIRECTSOUND,
	PROJECT_ASSET_PROG_WAVE,
} ProjectAssetKind;
```

### Project Asset Entry

This is one selectable project-wide asset.

```c
typedef struct {
	ProjectAssetKind kind;
	char file_name[256];      /* User-visible key, e.g. brass_1.wav or lead_00.bin */
	char rel_path[VG_MAX_PATH_LEN];
	char symbol[256];
} ProjectAssetEntry;
```

Notes:

- `file_name` is what the user sees and filters by
- `file_name` is also the stable override key because file names are assumed globally unique and stable
- `rel_path` is kept so the loader can load the actual file on reload
- `symbol` is retained because the loader already reasons in terms of symbols and it may be useful for debugging

### Per-Voice Binding

This stores what asset a voice originally used and what override is currently selected.

```c
typedef struct {
	ProjectAssetKind kind;
	char file_name[256];
} VoiceAssetBinding;
```

### Project Asset Index

```c
typedef struct {
	char project_root[512];
	VoicegroupLoaderConfig config;

	ProjectAssetEntry *directsound_assets;
	int directsound_count;
	int directsound_capacity;

	ProjectAssetEntry *prog_wave_assets;
	int prog_wave_count;
	int prog_wave_capacity;

	VoiceAssetBinding original_bindings[VOICEGROUP_SIZE];
	VoiceAssetBinding override_bindings[VOICEGROUP_SIZE];
} ProjectAssetIndex;
```

## Public API

### `project_asset_index.h`

Proposed API:

```c
typedef struct ProjectAssetIndex ProjectAssetIndex;

ProjectAssetIndex *project_asset_index_create(void);
void project_asset_index_destroy(ProjectAssetIndex *index);

bool project_asset_index_rebuild(ProjectAssetIndex *index,
                                 const char *project_root,
                                 const VoicegroupLoaderConfig *config);

void project_asset_index_clear_voice_bindings(ProjectAssetIndex *index);

bool project_asset_index_capture_original_bindings(ProjectAssetIndex *index,
                                                   const LoadedVoiceGroup *vg);

const ProjectAssetEntry *project_asset_index_get_directsound_assets(const ProjectAssetIndex *index,
                                                                    int *count);
const ProjectAssetEntry *project_asset_index_get_prog_wave_assets(const ProjectAssetIndex *index,
                                                                  int *count);

const VoiceAssetBinding *project_asset_index_get_original_bindings(const ProjectAssetIndex *index);
const VoiceAssetBinding *project_asset_index_get_override_bindings(const ProjectAssetIndex *index);

bool project_asset_index_set_override(ProjectAssetIndex *index,
                                      int voice_index,
                                      ProjectAssetKind kind,
                                      const char *file_name);

void project_asset_index_clear_override(ProjectAssetIndex *index, int voice_index);

bool project_asset_index_apply_overrides(ProjectAssetIndex *index,
                                         const char *project_root,
                                         const VoicegroupLoaderConfig *config,
                                         LoadedVoiceGroup *vg);
```

## Loader Reuse Strategy

The current loader already has the required building blocks, but many are `static` inside `plugin/voicegroup_loader.c`.

The cleanest plan is:

1. keep project asset indexing in `project_asset_index.c`
2. expose a small number of new loader helpers from `voicegroup_loader.h`
3. implement those helpers inside `voicegroup_loader.c` by reusing its existing discovery, symbol-map parsing, and asset-loading logic

### New Loader Helpers

Add these public helpers:

```c
typedef struct {
	ProjectAssetEntry *directsound_assets;
	int directsound_count;
	ProjectAssetEntry *prog_wave_assets;
	int prog_wave_count;
} VoicegroupProjectAssets;

bool voicegroup_loader_collect_project_assets(const char *project_root,
                                              const VoicegroupLoaderConfig *config,
                                              VoicegroupProjectAssets *out);

void voicegroup_loader_free_project_assets(VoicegroupProjectAssets *assets);

bool voicegroup_loader_get_voice_asset_binding(const LoadedVoiceGroup *vg,
                                               int voice_index,
                                               VoiceAssetBinding *out_binding);

bool voicegroup_loader_apply_voice_asset_override(LoadedVoiceGroup *vg,
                                                  int voice_index,
                                                  const char *project_root,
                                                  const VoicegroupLoaderConfig *config,
                                                  ProjectAssetKind kind,
                                                  const char *file_name);
```

### Why this split

- `voicegroup_loader.c` already knows how to discover, parse, resolve, and load assets
- `project_asset_index.c` should own persistence-facing state and GUI-facing catalog state
- the plugin should not duplicate loader logic

## How the Catalog is Built

When rebuilding the project asset index:

1. Run the existing project discovery logic.
2. Parse all DirectSound symbol maps.
3. Parse all programmable-wave symbol maps.
4. For each discovered asset:
   - derive `file_name` from the relative path basename
   - store `kind`
   - store `rel_path`
   - store `symbol`
5. Save the resulting arrays into `ProjectAssetIndex`.

No on-disk cache is needed for the first implementation.

## How Original Voice Bindings are Captured

After a voicegroup is loaded successfully:

1. The plugin has a fresh `LoadedVoiceGroup`.
2. Capture each voice slot's original asset binding into `original_bindings`.
3. Preserve `override_bindings` separately.

Expected behavior by voice type:

- DirectSound / DirectSound No Resample / DirectSound Alt:
  - capture the currently bound DirectSound file name
- Programmable Wave / Programmable Wave Alt:
  - capture the currently bound programmable-wave file name
- other voice types:
  - `valid = false`

This capture step must happen after base voicegroup load and before applying overrides.

## GUI Plan

### Existing Navigation Stays

Keep the current voice selector:

- slider
- integer input
- current voice index

No table view is added.

### New Controls on the Voice Page

For DirectSound voices:

- add a filterable selector labeled `Sample`
- the list source is `ProjectAssetIndex.directsound_assets`

For programmable-wave voices:

- add a filterable selector labeled `Wave`
- the list source is `ProjectAssetIndex.prog_wave_assets`

For unsupported voice types:

- no asset selector is shown
- existing parameter editing behavior remains as-is

### Selection UX

The selector shows and filters by `file_name` only.

Because file names are assumed unique and stable, no secondary folder/path label is required.

### Apply UX

Changing the selected asset should **not** mutate the live loaded voice immediately.

Instead:

1. the GUI records a pending override selection for the current voice
2. the voice page shows that a reload is required
3. the user triggers reload explicitly with a button on the `Voices` tab

Recommended new buttons on the `Voices` tab:

- `Reload To Apply Overrides`
- `Restore Original`

Behavior:

- `Reload To Apply Overrides` requests plugin restart using the existing restart flow
- `Restore Original` clears the current voice override and then also requires reload

This avoids surprise reloads while the user is browsing/filtering the selector.

### ImGui Implementation

Use built-in Dear ImGui first:

- `BeginCombo`
- `InputText` for filter text
- `Selectable`
- `ImGuiListClipper`

Only add a third-party searchable-combo helper if native ImGui becomes too awkward.

## Plugin Flow

### Plugin Data Changes

Extend `M4APluginData` in `plugin/m4a_plugin.h`:

- add `ProjectAssetIndex *assetIndex`
- remove or reduce any voice-override state that becomes duplicated by `assetIndex`

Keep existing:

- `originalVoices`
- `voiceOverrides`

but redefine responsibilities clearly:

- `originalVoices` continues to store original `ToneData` snapshots for non-sample parameter restoration
- `voiceOverrides[i]` remains the boolean "this slot differs from original" flag for GUI highlighting
- `assetIndex->override_bindings[i]` stores which project-wide sample override is selected

### Activation / Reload

Current flow:

1. load voicegroup
2. copy `loadedVg->voices` to `originalVoices`
3. wire GUI pointers

New flow:

1. rebuild `assetIndex` if project root or loader config changed, or if it does not exist yet
2. load base voicegroup
3. copy `loadedVg->voices` to `originalVoices`
4. capture original voice asset bindings into `assetIndex->original_bindings`
5. apply `assetIndex->override_bindings` to `loadedVg`
6. recompute `voiceOverrides[]`
7. wire GUI with:
   - live voices
   - original voices
   - boolean override flags
   - project asset index

### Applying Sample Overrides

Overrides are applied only after base voicegroup load.

For each voice slot with a valid override:

- verify the voice slot is a swappable type
- verify the override kind matches the slot kind
- resolve the selected `file_name` to a project asset entry
- load the replacement asset using loader helpers
- replace only the asset pointer:
  - DirectSound voices: replace `ToneData.wav`
  - Programmable wave voices: replace `ToneData.wavePointer`
- do not alter:
  - `type`
  - `key`
  - `panSweep`
  - ADSR
  - keysplit metadata

This preserves the voice definition while swapping the underlying sample source.

### State Save / Load

Current plugin state only stores:

- project root
- voicegroup name
- global parameters
- CLAP parameter state

It must be extended to store sample overrides.

Persist for each overridden voice:

- voice index
- asset kind
- selected file name

Suggested binary format after existing saved fields:

```c
uint32_t override_count;
for each override:
	uint8_t voice_index;
	uint8_t kind;
	uint32_t file_name_len;
	char[file_name_len] file_name;
```

### State Load Behavior

On state load:

1. restore normal plugin state
2. restore `assetIndex->override_bindings`
3. if activated, rebuild/reload the voicegroup and apply overrides even if the voicegroup name did not change

Reason:

- sample override state is part of the sound
- loading a saved DAW state must restore the correct asset-swapped voicegroup

## GUI API Changes

Extend the GUI C API so the `Voices` tab can browse project assets and submit override requests.

### Proposed Additions in `plugin/m4a_gui.h`

```c
typedef struct {
	int voice_index;
	bool valid;
	ProjectAssetKind kind;
	char file_name[256];
} M4AGuiVoiceAssetSelection;

void m4a_gui_set_project_assets(M4AGuiState *gui,
                                const ProjectAssetEntry *directsound_assets,
                                int directsound_count,
                                const ProjectAssetEntry *prog_wave_assets,
                                int prog_wave_count,
                                const VoiceAssetBinding *original_bindings,
                                const VoiceAssetBinding *override_bindings);

bool m4a_gui_poll_voice_asset_selection(M4AGuiState *gui,
                                        M4AGuiVoiceAssetSelection *selection);

bool m4a_gui_poll_voice_reload_request(M4AGuiState *gui);
```

### GUI Internal State Additions

Inside `M4AGuiState`:

- pointers to project asset arrays
- counts for both arrays
- current filter text for the asset selector
- pending asset-selection change
- pending reload request from the `Voices` tab

## Supported Voice Types

### Swappable

- `VOICE_DIRECTSOUND`
- `VOICE_DIRECTSOUND_ALT`
- `VOICE_DIRECTSOUND_NO_RESAMPLE`
- `VOICE_PROGRAMMABLE_WAVE`
- `VOICE_PROGRAMMABLE_WAVE_ALT`

### Not Swappable

- `VOICE_SQUARE_1`
- `VOICE_SQUARE_2`
- `VOICE_NOISE`
- `VOICE_KEYSPLIT`
- `VOICE_KEYSPLIT_ALL`
- `VOICE_CRY`
- `VOICE_CRY_REVERSE`

## Override Semantics

### DirectSound Family

Allowed source list:

- all project-wide DirectSound assets

Behavior:

- swap only `wav`
- preserve the target voice's type variant:
  - plain stays plain
  - alt stays alt
  - no-resample stays no-resample

This means the replacement sample inherits the playback behavior of the target voice slot.

### Programmable Wave Family

Allowed source list:

- all project-wide programmable-wave assets

Behavior:

- swap only `wavePointer`
- preserve all other target voice settings

## Validation Plan

After implementation:

1. Build the plugin target.
2. Build and run `poryaaaa_unit_tests`.
3. Manually validate in the plugin GUI:
   - load a project and voicegroup
   - select a DirectSound voice
   - filter and choose a project-wide DirectSound sample
   - reload and confirm the new sample is heard
   - restore original and reload and confirm restoration
   - repeat for a programmable-wave voice
4. Save plugin state in a host, reload the session, and confirm overrides restore correctly.

## Implementation Order

1. Add loader helper APIs for collecting project-wide assets and applying a file-name override to a loaded voice.
2. Add `project_asset_index.c/.h`.
3. Integrate `ProjectAssetIndex` into `M4APluginData`.
4. Extend plugin activation/reload flow to rebuild the index, capture original bindings, and apply overrides.
5. Extend plugin state save/load for persisted overrides.
6. Extend GUI API to receive project asset arrays and emit override/reload requests.
7. Add the filterable sample selector to the `Voices` tab.
8. Add explicit reload/apply controls on the `Voices` tab.
9. Verify both DirectSound and programmable-wave flows.

## Non-Goals for First Implementation

- cross-type swapping
- live hot-swapping without reload
- SQLite or any on-disk asset cache
- table-based overview UI
- editing keysplit/drumset sub-voice assets from this selector

