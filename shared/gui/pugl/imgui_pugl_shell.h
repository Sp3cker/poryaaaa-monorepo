#ifndef PORYAAAA_SHARED_GUI_IMGUI_PUGL_SHELL_H
#define PORYAAAA_SHARED_GUI_IMGUI_PUGL_SHELL_H

#include <cstdint>

struct ImFont;
struct ImGuiContext;

namespace poryaaaa::gui {

struct ImGuiPuglShell;

struct ImGuiPuglShellCallbacks {
  void *userData = nullptr;
  void (*drawFrame)(void *userData, std::uint32_t width,
                    std::uint32_t height) = nullptr;
  void (*closed)(void *userData, bool wasDestroyed) = nullptr;
  void (*timer)(void *userData) = nullptr;
  void (*setupStyle)(void *userData, ImFont *regularFont,
                     ImFont *boldFont) = nullptr;
};

struct ImGuiPuglShellConfig {
  const char *title = "poryaaaa";
  const char *className = "poryaaaa";
  std::uint32_t defaultWidth = 900;
  std::uint32_t defaultHeight = 700;
  std::uint32_t minWidth = 320;
  std::uint32_t minHeight = 240;
  float uiScale = 1.0f;
  float fontSize = 13.0f;
  const char *regularFontPath = nullptr;
  const char *boldFontPath = nullptr;
  const char *glslVersion = "#version 150";
};

ImGuiPuglShell *
imgui_pugl_shell_create(const ImGuiPuglShellConfig &config,
                        const ImGuiPuglShellCallbacks &callbacks);
void imgui_pugl_shell_prepare_destroy(ImGuiPuglShell *shell);
void imgui_pugl_shell_destroy(ImGuiPuglShell *shell);

bool imgui_pugl_shell_set_parent(ImGuiPuglShell *shell,
                                 std::uintptr_t nativeParent);
bool imgui_pugl_shell_show(ImGuiPuglShell *shell);
bool imgui_pugl_shell_hide(ImGuiPuglShell *shell);
bool imgui_pugl_shell_set_size(ImGuiPuglShell *shell, std::uint32_t width,
                               std::uint32_t height);
void imgui_pugl_shell_get_size(const ImGuiPuglShell *shell,
                               std::uint32_t *width,
                               std::uint32_t *height);
bool imgui_pugl_shell_can_resize(const ImGuiPuglShell *shell);
bool imgui_pugl_shell_was_closed(const ImGuiPuglShell *shell);
void imgui_pugl_shell_set_title(ImGuiPuglShell *shell, const char *title);

void imgui_pugl_shell_tick(ImGuiPuglShell *shell);
void imgui_pugl_shell_start_timer(ImGuiPuglShell *shell);
void imgui_pugl_shell_stop_timer(ImGuiPuglShell *shell);

ImFont *imgui_pugl_shell_bold_font(ImGuiPuglShell *shell);
ImGuiContext *imgui_pugl_shell_context(ImGuiPuglShell *shell);

} // namespace poryaaaa::gui

#endif
