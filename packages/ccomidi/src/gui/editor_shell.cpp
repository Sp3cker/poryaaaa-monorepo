#include "gui/editor_shell.h"

#include <string>

#include <clap/ext/gui.h>

#include "imgui_pugl_shell.h"

namespace ccomidi {

struct EditorShell {
  const clap_host_t *host = nullptr;
  EditorShellCallbacks callbacks = {};
  poryaaaa::gui::ImGuiPuglShell *shell = nullptr;
};

namespace {

void draw_frame(void *userData, std::uint32_t width, std::uint32_t height) {
  auto *shell = static_cast<EditorShell *>(userData);
  if (shell && shell->callbacks.drawFrame)
    shell->callbacks.drawFrame(shell->callbacks.userData, width, height);
}

void notify_host_closed(void *userData, bool wasDestroyed) {
  auto *shell = static_cast<EditorShell *>(userData);
  if (!shell || !shell->host)
    return;

  const auto *hostGui = static_cast<const clap_host_gui_t *>(
      shell->host->get_extension(shell->host, CLAP_EXT_GUI));
  if (hostGui && hostGui->closed)
    hostGui->closed(shell->host, wasDestroyed);
}

} // namespace

EditorShell *editor_shell_create(const clap_host_t *host,
                                 const EditorShellConfig &config,
                                 const EditorShellCallbacks &callbacks) {
  auto *editorShell = new EditorShell();
  editorShell->host = host;
  editorShell->callbacks = callbacks;

  const std::string fontDir = CCOMIDI_FONT_DIR;

  poryaaaa::gui::ImGuiPuglShellConfig shellConfig;
  shellConfig.title = config.title;
  shellConfig.className = config.className;
  shellConfig.defaultWidth = config.defaultWidth;
  shellConfig.defaultHeight = config.defaultHeight;
  shellConfig.minWidth = config.minWidth;
  shellConfig.minHeight = config.minHeight;
  shellConfig.uiScale = config.uiScale;
  const std::string regularFont = fontDir + "/Calamity-Regular.ttf";
  const std::string boldFont = fontDir + "/Calamity-Bold.ttf";
  shellConfig.regularFontPath = regularFont.c_str();
  shellConfig.boldFontPath = boldFont.c_str();

  poryaaaa::gui::ImGuiPuglShellCallbacks shellCallbacks;
  shellCallbacks.userData = editorShell;
  shellCallbacks.drawFrame = draw_frame;
  shellCallbacks.closed = notify_host_closed;

  editorShell->shell =
      poryaaaa::gui::imgui_pugl_shell_create(shellConfig, shellCallbacks);
  if (!editorShell->shell) {
    delete editorShell;
    return nullptr;
  }

  return editorShell;
}

void editor_shell_prepare_destroy(EditorShell *shell) {
  if (!shell)
    return;
  poryaaaa::gui::imgui_pugl_shell_prepare_destroy(shell->shell);
}

void editor_shell_destroy(EditorShell *shell) {
  if (!shell)
    return;
  poryaaaa::gui::imgui_pugl_shell_destroy(shell->shell);
  delete shell;
}

bool editor_shell_set_parent(EditorShell *shell, std::uintptr_t nativeParent) {
  return shell && poryaaaa::gui::imgui_pugl_shell_set_parent(shell->shell,
                                                             nativeParent);
}

bool editor_shell_show(EditorShell *shell) {
  return shell && poryaaaa::gui::imgui_pugl_shell_show(shell->shell);
}

bool editor_shell_hide(EditorShell *shell) {
  return shell && poryaaaa::gui::imgui_pugl_shell_hide(shell->shell);
}

bool editor_shell_set_size(EditorShell *shell, std::uint32_t width,
                           std::uint32_t height) {
  return shell &&
         poryaaaa::gui::imgui_pugl_shell_set_size(shell->shell, width, height);
}

void editor_shell_set_title(EditorShell *shell, const char *title) {
  if (!shell)
    return;
  poryaaaa::gui::imgui_pugl_shell_set_title(shell->shell, title);
}

ImFont *editor_shell_bold_font(EditorShell *shell) {
  return shell ? poryaaaa::gui::imgui_pugl_shell_bold_font(shell->shell)
               : nullptr;
}

void editor_shell_get_size(const EditorShell *shell, std::uint32_t *width,
                           std::uint32_t *height) {
  if (!shell) {
    if (width)
      *width = 0;
    if (height)
      *height = 0;
    return;
  }
  poryaaaa::gui::imgui_pugl_shell_get_size(shell->shell, width, height);
}

bool editor_shell_can_resize(const EditorShell *shell) {
  return shell && poryaaaa::gui::imgui_pugl_shell_can_resize(shell->shell);
}

bool editor_shell_was_closed(const EditorShell *shell) {
  return shell && poryaaaa::gui::imgui_pugl_shell_was_closed(shell->shell);
}

void editor_shell_start_timer(EditorShell *shell) {
  if (!shell)
    return;
  poryaaaa::gui::imgui_pugl_shell_start_timer(shell->shell);
}

void editor_shell_stop_timer(EditorShell *shell) {
  if (!shell)
    return;
  poryaaaa::gui::imgui_pugl_shell_stop_timer(shell->shell);
}

} // namespace ccomidi
