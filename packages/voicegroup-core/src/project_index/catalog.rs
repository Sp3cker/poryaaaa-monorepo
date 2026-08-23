use std::collections::{BTreeMap, BTreeSet};

use crate::ast::Diagnostic;
use crate::catalog::{CatalogEntry, CatalogEntryKind, KeysplitCatalogPair, ProjectCatalog};
use crate::program_bank::{ProgramBank, ProgramData};

use super::{append_unique_diagnostics, ProjectIndex};

pub(super) fn build_catalog(
    index: &ProjectIndex,
    diagnostics: &mut Vec<Diagnostic>,
) -> ProjectCatalog {
    let mut content_paths = BTreeSet::new();
    for relative_path in &index.voicegroup_files {
        content_paths.insert(relative_path.clone());
    }
    for voice_group in index.voice_groups.values() {
        content_paths.insert(voice_group.contents.relative_path().to_string());
    }

    let mut dependency_paths = BTreeSet::new();
    for asset in index
        .direct_sound_assets
        .values()
        .chain(index.programmable_wave_assets.values())
    {
        if !asset.relative_path.is_empty() {
            dependency_paths.insert(asset.relative_path.clone());
        }
    }

    let mut bank_records = Vec::new();
    let mut typical_by_symbol: BTreeMap<String, BTreeMap<[u8; 4], usize>> = BTreeMap::new();
    let mut typical_by_family: BTreeMap<String, BTreeMap<[u8; 4], usize>> = BTreeMap::new();
    let mut keysplit_pairs: BTreeMap<String, KeysplitCatalogPair> = BTreeMap::new();
    let mut drumkits = BTreeSet::new();

    for bank_name in index.voice_groups.keys() {
        let super::ProgramBankLoadResult {
            bank,
            diagnostics: bank_diagnostics,
        } = index.load_program_bank(bank_name);
        append_unique_diagnostics(diagnostics, bank_diagnostics);
        let Some(bank) = bank else {
            continue;
        };
        let mut bank_dependencies = BTreeSet::new();
        for record in bank.programs.iter().flatten() {
            let (family, adsr, symbol) = match &record.data {
                ProgramData::DirectSound(program) => (
                    Some("directsound"),
                    Some([
                        program.attack,
                        program.decay,
                        program.sustain,
                        program.release,
                    ]),
                    Some(program.sample_symbol.as_str()),
                ),
                ProgramData::ProgrammableWave(program) => (
                    Some("programmable_wave"),
                    Some([
                        program.attack,
                        program.decay,
                        program.sustain,
                        program.release,
                    ]),
                    Some(program.wave_symbol.as_str()),
                ),
                ProgramData::Square1(program) => (
                    Some("square_1"),
                    Some([
                        program.attack,
                        program.decay,
                        program.sustain,
                        program.release,
                    ]),
                    None,
                ),
                ProgramData::Square2(program) => (
                    Some("square_2"),
                    Some([
                        program.attack,
                        program.decay,
                        program.sustain,
                        program.release,
                    ]),
                    None,
                ),
                ProgramData::Noise(program) => (
                    Some("noise"),
                    Some([
                        program.attack,
                        program.decay,
                        program.sustain,
                        program.release,
                    ]),
                    None,
                ),
                ProgramData::Keysplit(program) => {
                    keysplit_pairs
                        .entry(program.table_symbol.clone())
                        .or_insert_with(|| KeysplitCatalogPair {
                            subgroup: program.sub_voicegroup.clone(),
                            table: program.table_symbol.clone(),
                        });
                    (None, None, None)
                }
                ProgramData::KeysplitAll(program) => {
                    drumkits.insert(program.sub_voicegroup.clone());
                    (None, None, None)
                }
                ProgramData::Cry(_) => (None, None, None),
            };
            match &record.data {
                ProgramData::DirectSound(program) if !program.sample_relative_path.is_empty() => {
                    bank_dependencies.insert(program.sample_relative_path.clone());
                }
                ProgramData::ProgrammableWave(program)
                    if !program.wave_relative_path.is_empty() =>
                {
                    bank_dependencies.insert(program.wave_relative_path.clone());
                }
                _ => {}
            }

            if let (Some(family), Some(adsr)) = (family, adsr) {
                // Keep the same useful-envelope filtering as the editor:
                // release-zero records click and DirectSound attack-zero
                // records never produce an audible sample.
                if adsr[3] != 0 && (family != "directsound" || adsr[0] != 0) {
                    *typical_by_family
                        .entry(family.to_string())
                        .or_default()
                        .entry(adsr)
                        .or_default() += 1;
                    if let Some(symbol) = symbol {
                        *typical_by_symbol
                            .entry(symbol.to_string())
                            .or_default()
                            .entry(adsr)
                            .or_default() += 1;
                    }
                }
            }
        }
        let mut visited = BTreeSet::new();
        append_bank_dependencies(index, &bank, &mut bank_dependencies, &mut visited);
        bank_records.push((bank, bank_dependencies));
    }

    let typical_adsr_by_symbol = mode_map(typical_by_symbol);
    let typical_adsr_by_family = mode_map(typical_by_family);
    let mut entries = Vec::new();

    for (bank, bank_dependencies) in bank_records {
        let mut keysplit = None;
        let mut drumkit = None;
        for record in bank.programs.iter().flatten() {
            match &record.data {
                ProgramData::Keysplit(program) if keysplit.is_none() => {
                    keysplit = Some(KeysplitCatalogPair {
                        subgroup: program.sub_voicegroup.clone(),
                        table: program.table_symbol.clone(),
                    });
                }
                ProgramData::KeysplitAll(program) if drumkit.is_none() => {
                    drumkit = Some(program.sub_voicegroup.clone());
                }
                _ => {}
            }
        }
        entries.push(CatalogEntry {
            kind: CatalogEntryKind::VoiceGroup,
            symbol: canonical_voicegroup_symbol(&bank.name),
            display_name: bank.name.clone(),
            source_path: Some(bank.source_relative_path.clone()),
            asset_path: None,
            dependency_paths: bank_dependencies.into_iter().collect(),
            keysplit,
            drumkit,
            typical_adsr: None,
            synth_desc: None,
        });
    }

    for asset in index.direct_sound_assets.values() {
        let kind = asset
            .synth_desc
            .map_or(CatalogEntryKind::DirectSound, |_| CatalogEntryKind::Synth);
        entries.push(CatalogEntry {
            kind,
            symbol: asset.symbol.clone(),
            display_name: asset.display_name.clone(),
            source_path: index
                .direct_sound_definitions
                .get(&asset.symbol)
                .map(|location| location.relative_path.clone()),
            asset_path: (!asset.relative_path.is_empty()).then(|| asset.relative_path.clone()),
            dependency_paths: (!asset.relative_path.is_empty())
                .then(|| vec![asset.relative_path.clone()])
                .unwrap_or_default(),
            keysplit: None,
            drumkit: None,
            typical_adsr: typical_adsr_by_symbol.get(&asset.symbol).copied(),
            synth_desc: asset.synth_desc,
        });
    }

    for asset in index.programmable_wave_assets.values() {
        entries.push(CatalogEntry {
            kind: CatalogEntryKind::ProgrammableWave,
            symbol: asset.symbol.clone(),
            display_name: asset.display_name.clone(),
            source_path: index
                .programmable_wave_definitions
                .get(&asset.symbol)
                .map(|location| location.relative_path.clone()),
            asset_path: Some(asset.relative_path.clone()),
            dependency_paths: vec![asset.relative_path.clone()],
            keysplit: None,
            drumkit: None,
            typical_adsr: typical_adsr_by_symbol.get(&asset.symbol).copied(),
            synth_desc: None,
        });
    }

    for symbol in index.keysplit_tables.keys() {
        entries.push(CatalogEntry {
            kind: CatalogEntryKind::Keysplit,
            symbol: symbol.clone(),
            display_name: symbol.clone(),
            source_path: index
                .keysplit_definitions
                .get(symbol)
                .map(|location| location.relative_path.clone()),
            asset_path: None,
            dependency_paths: Vec::new(),
            keysplit: keysplit_pairs.get(symbol).cloned(),
            drumkit: None,
            typical_adsr: None,
            synth_desc: None,
        });
    }

    for drumkit in drumkits {
        let bank_name = drumkit.strip_prefix("voicegroup_").unwrap_or(&drumkit);
        let source_path = index
            .voice_groups
            .get(bank_name)
            .map(|voice_group| voice_group.contents.relative_path().to_string());
        entries.push(CatalogEntry {
            kind: CatalogEntryKind::Drumkit,
            symbol: canonical_voicegroup_symbol(bank_name),
            display_name: bank_name.to_string(),
            source_path,
            asset_path: None,
            dependency_paths: Vec::new(),
            keysplit: None,
            drumkit: Some(drumkit),
            typical_adsr: None,
            synth_desc: None,
        });
    }

    let indexed_source_paths = [
        "sound/voice_groups.inc",
        "sound/voicegroups.inc",
        "sound/direct_sound_data.inc",
        "sound/direct_sound_synth_data.inc",
        "sound/programmable_wave_data.inc",
        "sound/keysplit_tables.inc",
        "sound/keysplit_tables.s",
    ]
    .into_iter()
    .filter(|path| index.root.join(path).is_file())
    .map(str::to_string);
    let watch_paths = content_paths
        .iter()
        .chain(indexed_source_paths.collect::<BTreeSet<_>>().iter())
        .chain(dependency_paths.iter())
        .cloned()
        .collect::<BTreeSet<_>>();

    ProjectCatalog {
        entries,
        content_paths: content_paths.into_iter().collect(),
        dependency_paths: dependency_paths.into_iter().collect(),
        watch_paths: watch_paths.into_iter().collect(),
        typical_adsr_by_symbol,
        typical_adsr_by_family,
    }
}

fn mode_map(counts: BTreeMap<String, BTreeMap<[u8; 4], usize>>) -> BTreeMap<String, [u8; 4]> {
    counts
        .into_iter()
        .filter_map(|(key, values)| {
            values
                .into_iter()
                .max_by(|(left_value, left_count), (right_value, right_count)| {
                    left_count
                        .cmp(right_count)
                        .then_with(|| right_value.cmp(left_value))
                })
                .map(|(value, _)| (key, value))
        })
        .collect()
}

fn canonical_voicegroup_symbol(name: &str) -> String {
    if name.starts_with("voicegroup_") {
        name.to_string()
    } else {
        format!("voicegroup_{name}")
    }
}

fn append_bank_dependencies(
    index: &ProjectIndex,
    bank: &ProgramBank,
    dependencies: &mut BTreeSet<String>,
    visited: &mut BTreeSet<String>,
) {
    if !visited.insert(bank.name.clone()) {
        return;
    }
    for record in bank.programs.iter().flatten() {
        let child_name = match &record.data {
            ProgramData::DirectSound(program) => {
                if !program.sample_relative_path.is_empty() {
                    dependencies.insert(program.sample_relative_path.clone());
                }
                None
            }
            ProgramData::ProgrammableWave(program) => {
                if !program.wave_relative_path.is_empty() {
                    dependencies.insert(program.wave_relative_path.clone());
                }
                None
            }
            ProgramData::Keysplit(program) => Some(program.sub_voicegroup.as_str()),
            ProgramData::KeysplitAll(program) => Some(program.sub_voicegroup.as_str()),
            _ => None,
        };
        let Some(child_name) = child_name else {
            continue;
        };
        let candidate = child_name.strip_prefix("voicegroup_").unwrap_or(child_name);
        let Some(child_key) = index
            .voice_groups
            .get(candidate)
            .map(|_| candidate)
            .or_else(|| index.voice_groups.get(child_name).map(|_| child_name))
        else {
            continue;
        };
        let Some(child_bank) = index.load_program_bank(child_key).bank else {
            continue;
        };
        append_bank_dependencies(index, &child_bank, dependencies, visited);
    }
}
