#include "voicegroup_project_internal.h"

#include <stdlib.h>
#include <string.h>

static void generation_free(ProjectGeneration* generation)
{
    if (!generation)
        return;
    generation_clear_asset_cache(generation);
    voicegroup_core_project_index_free(generation->index);
    project_storage_dispose(generation->snapshot);
    free(generation);
}

static ProjectResultStorage* project_failure_copy(const VoicegroupProject* project)
{
    if (!project || !project->failure)
        return NULL;
    ProjectResultStorage* result = project_storage_create();
    if (result && !copy_project_result(result, &project->failure->view))
    {
        project_storage_dispose(result);
        return NULL;
    }
    return result;
}
static bool project_refresh_failed(VoicegroupProject* project,
                                   VoicegroupProjectResult* output,
                                   const char* code,
                                   const char* message)
{
    ProjectResultStorage* failure = project_storage_create();
    if (failure && !add_simple_diagnostic(&failure->arena,
                                          (VoicegroupDiagnostic**)&failure->view.diagnostics,
                                          &failure->view.diagnostic_count,
                                          code,
                                          message,
                                          0,
                                          project->root,
                                          NULL,
                                          false,
                                          0))
    {
        project_storage_dispose(failure);
        failure = NULL;
    }
    if (failure)
        failure->view.succeeded = false;
    project_storage_dispose(project->failure);
    project->failure = failure;
    project->state = PROJECT_REFRESH_FAILED;
    if (output)
    {
        *output = (VoicegroupProjectResult){0};
        ProjectResultStorage* copy = project_failure_copy(project);
        if (copy)
            *output = copy->view;
    }
    return false;
}

static bool project_rebuild(VoicegroupProject* project, VoicegroupProjectResult* output)
{
    VoicegroupCoreProjectIndex* index = NULL;
    VoicegroupCoreProjectSnapshotResult* snapshot = NULL;
    VoicegroupCoreStatus status = voicegroup_core_project_index_load(project->root, &index);
    if (status == VOICEGROUP_CORE_STATUS_OK && index)
        status = voicegroup_core_project_index_snapshot(index, &snapshot);

    if (status != VOICEGROUP_CORE_STATUS_OK || !snapshot)
    {
        voicegroup_core_project_snapshot_result_free(snapshot);
        voicegroup_core_project_index_free(index);
        return project_refresh_failed(
            project, output, "project.index_load_failed", "voicegroup project index could not be loaded");
    }

    /* The index is available; install the generation even with catalog diagnostics. */
    ProjectGeneration* generation = calloc(1, sizeof(*generation));
    ProjectResultStorage* storage = project_storage_create();
    bool ok = generation && storage && project_storage_copy_core_snapshot(storage, snapshot);
    if (ok)
    {
        generation->index = index;
        generation->snapshot = storage;
        generation_free(project->generation);
        project->generation = generation;
        project_storage_dispose(project->failure);
        project->failure = NULL;
        project->state = PROJECT_FRESH;
        if (output)
        {
            ProjectResultStorage* copy = project_storage_create();
            if (copy && copy_project_result(copy, &storage->view))
                *output = copy->view;
            else
                project_storage_dispose(copy);
        }
        voicegroup_core_project_snapshot_result_free(snapshot);
        return true;
    }

    project_storage_dispose(storage);
    free(generation);
    voicegroup_core_project_snapshot_result_free(snapshot);
    voicegroup_core_project_index_free(index);
    return project_refresh_failed(
        project, output, "project.out_of_memory", "voicegroup project generation could not be installed");
}

bool ensure_generation(VoicegroupProject* project)
{
    if (project->state == PROJECT_FRESH && project->generation)
        return true;
    if (project->state == PROJECT_REFRESH_FAILED)
        return false;
    return project_rebuild(project, NULL);
}

VoicegroupProject* voicegroup_project_open(const char* root, size_t root_len)
{
    if (!root && root_len > 0)
        return NULL;
    VoicegroupProject* project = calloc(1, sizeof(*project));
    if (!project)
        return NULL;
    project->root = duplicate_bytes(root ? root : "", root_len);
    if (!project->root)
    {
        free(project);
        return NULL;
    }
    project->state = PROJECT_STALE;
    return project;
}

void voicegroup_project_refresh(VoicegroupProject* project, VoicegroupProjectResult* out)
{
    if (out)
        *out = (VoicegroupProjectResult){0};
    if (!project)
        return;
    generation_clear_asset_cache(project->generation);
    project->state = PROJECT_STALE;
    project_rebuild(project, out);
}

void voicegroup_project_mark_stale(VoicegroupProject* project)
{
    if (!project)
        return;
    generation_clear_asset_cache(project->generation);
    project->state = PROJECT_STALE;
}

void voicegroup_project_free(VoicegroupProject* project)
{
    if (!project)
        return;
    generation_free(project->generation);
    project_storage_dispose(project->failure);
    free(project->root);
    free(project);
}

static LoadResultStorage*
load_failure_from_project(const VoicegroupProject* project, const char* code, const char* message)
{
    LoadResultStorage* storage = load_storage_create();
    if (!storage)
        return NULL;
    if (project && project->failure && project->failure->view.diagnostic_count > 0)
    {
        size_t count = project->failure->view.diagnostic_count;
        VoicegroupDiagnostic* diagnostics = arena_alloc(&storage->arena, count * sizeof(*diagnostics));
        if (!diagnostics)
        {
            load_storage_dispose(storage);
            return NULL;
        }
        for (size_t i = 0; i < count; i++)
            if (!copy_diagnostic(&storage->arena, &diagnostics[i], &project->failure->view.diagnostics[i]))
            {
                load_storage_dispose(storage);
                return NULL;
            }
        storage->view.diagnostics = diagnostics;
        storage->view.diagnostic_count = count;
    }
    else if (!add_simple_diagnostic(&storage->arena,
                                    (VoicegroupDiagnostic**)&storage->view.diagnostics,
                                    &storage->view.diagnostic_count,
                                    code,
                                    message,
                                    0,
                                    NULL,
                                    NULL,
                                    false,
                                    0))
    {
        load_storage_dispose(storage);
        return NULL;
    }
    storage->view.succeeded = false;
    return storage;
}

static bool copy_bank_diagnostics(LoadResultStorage* storage,
                                  const VoicegroupCoreBankResult* result,
                                  const VoicegroupMaterializationReport* report)
{
    size_t coreCount = voicegroup_core_bank_result_diagnostic_count(result);
    size_t reportCount = report ? report->count : 0;
    size_t total = coreCount + reportCount;
    if (total == 0)
        return true;
    VoicegroupDiagnostic* diagnostics = arena_alloc(&storage->arena, total * sizeof(*diagnostics));
    if (!diagnostics)
        return false;
    for (size_t i = 0; i < coreCount; i++)
    {
        VoicegroupCoreDiagnostic source;
        if (!voicegroup_core_bank_result_diagnostic(result, i, &source) ||
            !copy_core_diagnostic(&storage->arena, &diagnostics[i], &source))
            return false;
    }
    for (size_t i = 0; i < reportCount; i++)
    {
        VoicegroupDiagnostic* diagnostic = &diagnostics[coreCount + i];
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = arena_copy_string(&storage->arena, "materialization.failed");
        diagnostic->message = arena_copy_string(&storage->arena, report->failures[i].message);
        diagnostic->scope = 2;
        diagnostic->asset_path = arena_copy_string(&storage->arena, report->failures[i].assetPath);
        diagnostic->has_slot = true;
        diagnostic->slot = report->failures[i].slot;
        if (!diagnostic->code || !diagnostic->message || !diagnostic->asset_path)
            return false;
    }
    storage->view.diagnostics = diagnostics;
    storage->view.diagnostic_count = total;
    return true;
}

VoicegroupLoadResult voicegroup_project_load(VoicegroupProject* project, const VoicegroupLoadRequest* request)
{
    VoicegroupLoadResult empty = {0};
    if (!project || !request)
    {
        LoadResultStorage* failure =
            load_failure_from_project(NULL, "load.invalid_request", "voicegroup load requires a project and request");
        return failure ? failure->view : empty;
    }
    if (!request->bank_name || request->bank_name_len == 0 ||
        (request->mode == VG_LOAD_SOURCE && ((!request->relative_path && request->relative_path_len > 0) ||
                                             (!request->source_bytes && request->source_len > 0))))
    {
        LoadResultStorage* failure = load_failure_from_project(
            project, "load.invalid_request", "voicegroup load request has missing source fields");
        return failure ? failure->view : empty;
    }
    if (request->mode != VG_LOAD_SAVED && request->mode != VG_LOAD_SOURCE)
    {
        LoadResultStorage* failure =
            load_failure_from_project(project, "load.invalid_mode", "voicegroup load mode is not recognized");
        return failure ? failure->view : empty;
    }

    if (request->mode == VG_LOAD_SAVED)
    {
        if (!ensure_generation(project))
        {
            LoadResultStorage* failure =
                load_failure_from_project(project, "project.refresh_failed", "voicegroup project refresh failed");
            return failure ? failure->view : empty;
        }
    }
    else if (!project->generation && project->state != PROJECT_REFRESH_FAILED && !ensure_generation(project))
    {
        LoadResultStorage* failure =
            load_failure_from_project(project, "project.refresh_failed", "voicegroup project has no retained index");
        return failure ? failure->view : empty;
    }
    if (!project->generation)
    {
        LoadResultStorage* failure =
            load_failure_from_project(project, "project.no_generation", "voicegroup project has no retained index");
        return failure ? failure->view : empty;
    }

    char* bankName = duplicate_bytes(request->bank_name, request->bank_name_len);
    if (!bankName)
        return empty;
    VoicegroupCoreBankResult* coreResult = NULL;
    VoicegroupCoreStatus status;
    if (request->mode == VG_LOAD_SAVED)
        status = voicegroup_core_project_index_load_program_bank(project->generation->index, bankName, &coreResult);
    else
        status =
            voicegroup_core_project_index_load_program_bank_source(project->generation->index,
                                                                   request->relative_path ? request->relative_path : "",
                                                                   request->relative_path_len,
                                                                   request->source_bytes ? request->source_bytes : "",
                                                                   request->source_len,
                                                                   bankName,
                                                                   request->bank_name_len,
                                                                   request->overlay ? request->overlay->core : NULL,
                                                                   &coreResult);
    free(bankName);

    if (status != VOICEGROUP_CORE_STATUS_OK || !coreResult)
    {
        LoadResultStorage* failure =
            load_failure_from_project(project, "load.core_failed", "voicegroup-core could not load the bank");
        return failure ? failure->view : empty;
    }

    LoadResultStorage* storage = load_storage_create();
    VoicegroupMaterializationReport report = {0};
    bool hasBank = storage && voicegroup_core_bank_result_has_bank(coreResult) &&
                   voicegroup_core_bank_result_diagnostic_count(coreResult) == 0;
    if (hasBank)
        hasBank = voicegroup_materialize_core_bank(
            project->root, project->generation->index, coreResult, &storage->bank, &report);
    bool diagnosticsOk = storage && copy_bank_diagnostics(storage, coreResult, &report);
    if (storage && !hasBank && storage->view.diagnostic_count == 0)
        add_simple_diagnostic(&storage->arena,
                              (VoicegroupDiagnostic**)&storage->view.diagnostics,
                              &storage->view.diagnostic_count,
                              "load.bank_missing",
                              "voicegroup bank was not found or has no materialized programs",
                              0,
                              NULL,
                              NULL,
                              false,
                              0);
    if (storage)
    {
        storage->view.succeeded =
            hasBank && report.count == 0 && voicegroup_core_bank_result_diagnostic_count(coreResult) == 0;
        if (!diagnosticsOk && storage->view.diagnostic_count == 0)
            add_simple_diagnostic(&storage->arena,
                                  (VoicegroupDiagnostic**)&storage->view.diagnostics,
                                  &storage->view.diagnostic_count,
                                  "load.out_of_memory",
                                  "voicegroup load result could not retain diagnostics",
                                  2,
                                  NULL,
                                  NULL,
                                  false,
                                  0);
    }
    voicegroup_materialization_report_free(&report);
    voicegroup_core_bank_result_free(coreResult);
    if (!storage)
        return empty;
    return storage->view;
}

LoadedVoiceGroup* voicegroup_load_result_take(VoicegroupLoadResult* result)
{
    if (!result || !result->_private_storage || !result->succeeded)
        return NULL;
    LoadResultStorage* storage = result->_private_storage;
    LoadedVoiceGroup* bank = storage->bank;
    storage->bank = NULL;
    return bank;
}

VoicegroupSynthOverlay* voicegroup_synth_overlay_create(void)
{
    VoicegroupSynthOverlay* overlay = calloc(1, sizeof(*overlay));
    if (!overlay)
        return NULL;
    overlay->core = voicegroup_core_synth_overlay_create();
    if (!overlay->core)
    {
        free(overlay);
        return NULL;
    }
    return overlay;
}

void voicegroup_synth_overlay_add(VoicegroupSynthOverlay* overlay,
                                  const char* name,
                                  size_t name_len,
                                  uint8_t descriptor[6])
{
    if (!overlay || !overlay->core || (!name && name_len > 0) || !descriptor)
        return;
    voicegroup_core_synth_overlay_add(overlay->core, name ? name : "", name_len, descriptor);
}

void voicegroup_synth_overlay_free(VoicegroupSynthOverlay* overlay)
{
    if (!overlay)
        return;
    voicegroup_core_synth_overlay_free(overlay->core);
    free(overlay);
}
