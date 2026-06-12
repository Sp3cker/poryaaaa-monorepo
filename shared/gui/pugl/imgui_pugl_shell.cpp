#include "imgui_pugl_shell.h"

#include <algorithm>
#include <cstdio>
#include <string>

#ifdef __APPLE__
#include "imgui_impl_metal.h"
#include "pugl_mac_metal.h"
#else
#include "backends/imgui_impl_opengl3.h"
#include <pugl/gl.h>
#endif

#include <pugl/pugl.h>

#include "imgui.h"
#include "imgui_impl_pugl.h"

namespace poryaaaa::gui {

namespace {

constexpr std::uintptr_t kRenderTimerId = 1;

bool hasPlainModifiers(PuglMods mods) {
  return (mods & (PUGL_MOD_SHIFT | PUGL_MOD_CTRL | PUGL_MOD_ALT |
                  PUGL_MOD_SUPER)) == 0;
}

bool isPlainSpaceEvent(const PuglEvent *event) {
  if (!event)
    return false;
  if (event->type == PUGL_KEY_PRESS || event->type == PUGL_KEY_RELEASE)
    return event->key.key == PUGL_KEY_SPACE &&
           hasPlainModifiers(event->key.state);
  if (event->type == PUGL_TEXT)
    return event->text.character == ' ' && hasPlainModifiers(event->text.state);
  return false;
}

bool fileExists(const char *path) {
  if (!path)
    return false;
  FILE *file = std::fopen(path, "rb");
  if (!file)
    return false;
  std::fclose(file);
  return true;
}

} // namespace

struct ImGuiPuglShell {
  ImGuiPuglShellConfig config = {};
  ImGuiPuglShellCallbacks callbacks = {};
  PuglWorld *world = nullptr;
  PuglView *view = nullptr;
  ImGuiContext *imgui = nullptr;
  ImFont *boldFont = nullptr;
  bool realized = false;
  bool renderInited = false;
  bool embedded = false;
  bool wasClosed = false;
  bool destroying = false;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

namespace {

void renderFrame(ImGuiPuglShell *shell) {
  if (!shell || !shell->view)
    return;

#ifdef __APPLE__
  auto *metalContext =
      static_cast<PuglMetalContext *>(puglGetContext(shell->view));
  if (!metalContext || !metalContext->renderPassDescriptor ||
      !metalContext->commandBuffer || !metalContext->renderEncoder)
    return;
  ImGui_ImplMetal_NewFrame(metalContext->renderPassDescriptor);
#else
  ImGui_ImplOpenGL3_NewFrame();
#endif

  ImGui_ImplPugl_NewFrame();
  ImGui::NewFrame();

  if (shell->callbacks.drawFrame)
    shell->callbacks.drawFrame(shell->callbacks.userData, shell->width,
                               shell->height);

  ImGui::Render();
#ifdef __APPLE__
  ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(),
                                 metalContext->commandBuffer,
                                 metalContext->renderEncoder);
#else
  glViewport(0, 0, static_cast<int>(shell->width),
             static_cast<int>(shell->height));
  glClearColor(0.10f, 0.11f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

PuglStatus onPuglEvent(PuglView *view, const PuglEvent *event) {
  auto *shell = static_cast<ImGuiPuglShell *>(puglGetHandle(view));
  if (!shell || !event)
    return PUGL_SUCCESS;

  ImGui::SetCurrentContext(shell->imgui);

  switch (event->type) {
  case PUGL_REALIZE:
    if (!shell->renderInited) {
#ifdef __APPLE__
      auto *metalContext =
          static_cast<PuglMetalContext *>(puglGetContext(shell->view));
      if (!metalContext || !metalContext->device)
        break;
      ImGui_ImplMetal_Init(metalContext->device);
#else
      ImGui_ImplOpenGL3_Init(shell->config.glslVersion);
#endif
      shell->renderInited = true;
    }
    break;
  case PUGL_UNREALIZE:
    if (shell->renderInited) {
#ifdef __APPLE__
      ImGui_ImplMetal_Shutdown();
#else
      ImGui_ImplOpenGL3_Shutdown();
#endif
      shell->renderInited = false;
    }
    break;
  case PUGL_CONFIGURE:
    shell->width = event->configure.width;
    shell->height = event->configure.height;
    break;
  case PUGL_UPDATE:
    puglObscureView(view);
    break;
  case PUGL_EXPOSE:
    if (shell->renderInited)
      renderFrame(shell);
    break;
  case PUGL_TIMER:
    if (event->timer.id == kRenderTimerId) {
      if (shell->callbacks.timer)
        shell->callbacks.timer(shell->callbacks.userData);
      else
        imgui_pugl_shell_tick(shell);
    }
    break;
  case PUGL_CLOSE:
    shell->wasClosed = true;
    imgui_pugl_shell_stop_timer(shell);
    if (!shell->destroying && shell->callbacks.closed)
      shell->callbacks.closed(shell->callbacks.userData, false);
    break;
  case PUGL_BUTTON_PRESS:
    puglGrabFocus(view);
    ImGui_ImplPugl_ProcessEvent(event);
    break;
  case PUGL_KEY_PRESS:
  case PUGL_KEY_RELEASE:
  case PUGL_TEXT:
    if (shell->embedded && isPlainSpaceEvent(event) &&
        !ImGui::GetIO().WantTextInput)
      return PUGL_UNSUPPORTED;
    ImGui_ImplPugl_ProcessEvent(event);
    break;
  default:
    ImGui_ImplPugl_ProcessEvent(event);
    break;
  }

  return PUGL_SUCCESS;
}

} // namespace

ImGuiPuglShell *
imgui_pugl_shell_create(const ImGuiPuglShellConfig &config,
                        const ImGuiPuglShellCallbacks &callbacks) {
  auto *shell = new ImGuiPuglShell();
  shell->config = config;
  shell->callbacks = callbacks;
  shell->width = config.defaultWidth;
  shell->height = config.defaultHeight;

  shell->world = puglNewWorld(PUGL_MODULE, 0);
  if (!shell->world) {
    delete shell;
    return nullptr;
  }

  puglSetWorldString(shell->world, PUGL_CLASS_NAME, config.className);
  shell->view = puglNewView(shell->world);
  if (!shell->view) {
    puglFreeWorld(shell->world);
    delete shell;
    return nullptr;
  }

#ifdef __APPLE__
  puglSetBackend(shell->view, puglMetalBackend());
#else
  puglSetBackend(shell->view, puglGlBackend());
  puglSetViewHint(shell->view, PUGL_CONTEXT_API, PUGL_OPENGL_API);
  puglSetViewHint(shell->view, PUGL_CONTEXT_VERSION_MAJOR, 3);
  puglSetViewHint(shell->view, PUGL_CONTEXT_VERSION_MINOR, 3);
  puglSetViewHint(shell->view, PUGL_CONTEXT_PROFILE, PUGL_OPENGL_CORE_PROFILE);
  puglSetViewHint(shell->view, PUGL_DOUBLE_BUFFER, 1);
#endif

  puglSetViewHint(shell->view, PUGL_RESIZABLE, 1);
  puglSetViewHint(shell->view, PUGL_IGNORE_KEY_REPEAT, 1);
  puglSetSizeHint(shell->view, PUGL_DEFAULT_SIZE, config.defaultWidth,
                  config.defaultHeight);
  puglSetSizeHint(shell->view, PUGL_MIN_SIZE, config.minWidth,
                  config.minHeight);
  puglSetViewString(shell->view, PUGL_WINDOW_TITLE, config.title);
  puglSetHandle(shell->view, shell);
  puglSetEventFunc(shell->view, onPuglEvent);

  shell->imgui = ImGui::CreateContext();
  ImGui::SetCurrentContext(shell->imgui);

  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImFont *regularFont = nullptr;
  if (fileExists(config.regularFontPath))
    regularFont =
        io.Fonts->AddFontFromFileTTF(config.regularFontPath, config.fontSize);
  if (fileExists(config.boldFontPath))
    shell->boldFont =
        io.Fonts->AddFontFromFileTTF(config.boldFontPath, config.fontSize);
  if (regularFont)
    io.FontDefault = regularFont;

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(config.uiScale);
  style.FramePadding.x += config.uiScale;
  style.FramePadding.y += config.uiScale;
  io.FontGlobalScale = config.uiScale;

  if (callbacks.setupStyle)
    callbacks.setupStyle(callbacks.userData, regularFont, shell->boldFont);

  ImGui_ImplPugl_Init(shell->view);
  return shell;
}

void imgui_pugl_shell_prepare_destroy(ImGuiPuglShell *shell) {
  if (!shell)
    return;
  shell->destroying = true;
  imgui_pugl_shell_stop_timer(shell);
}

void imgui_pugl_shell_destroy(ImGuiPuglShell *shell) {
  if (!shell)
    return;

  imgui_pugl_shell_prepare_destroy(shell);
  ImGui::SetCurrentContext(shell->imgui);

  if (shell->view && shell->renderInited) {
    puglEnterContext(shell->view);
#ifdef __APPLE__
    ImGui_ImplMetal_Shutdown();
#else
    ImGui_ImplOpenGL3_Shutdown();
#endif
    shell->renderInited = false;
    puglLeaveContext(shell->view);
  }

  ImGui_ImplPugl_Shutdown();
  if (shell->view)
    puglFreeView(shell->view);
  if (shell->imgui)
    ImGui::DestroyContext(shell->imgui);
  if (shell->world)
    puglFreeWorld(shell->world);
  delete shell;
}

bool imgui_pugl_shell_set_parent(ImGuiPuglShell *shell,
                                 std::uintptr_t nativeParent) {
  if (!shell || !shell->view || shell->realized)
    return false;

  puglSetParent(shell->view, static_cast<PuglNativeView>(nativeParent));
  const PuglStatus status = puglRealize(shell->view);
  if (status != PUGL_SUCCESS)
    return false;
  shell->embedded = true;
  shell->realized = true;
  return true;
}

bool imgui_pugl_shell_show(ImGuiPuglShell *shell) {
  if (!shell || !shell->view)
    return false;

  if (!shell->realized) {
    const PuglStatus status = puglRealize(shell->view);
    if (status != PUGL_SUCCESS)
      return false;
    shell->realized = true;
  }

  puglShow(shell->view,
           shell->embedded ? PUGL_SHOW_PASSIVE : PUGL_SHOW_RAISE);
  return true;
}

bool imgui_pugl_shell_hide(ImGuiPuglShell *shell) {
  if (!shell || !shell->view)
    return false;
  imgui_pugl_shell_stop_timer(shell);
  puglHide(shell->view);
  return true;
}

bool imgui_pugl_shell_set_size(ImGuiPuglShell *shell, std::uint32_t width,
                               std::uint32_t height) {
  if (!shell || !shell->view)
    return false;

  double scale = puglGetScaleFactor(shell->view);
  if (scale < 1.0)
    scale = 1.0;
  puglSetSizeHint(shell->view, PUGL_CURRENT_SIZE,
                  static_cast<PuglSpan>(width * scale),
                  static_cast<PuglSpan>(height * scale));
  return true;
}

void imgui_pugl_shell_get_size(const ImGuiPuglShell *shell,
                               std::uint32_t *width,
                               std::uint32_t *height) {
  if (!width || !height)
    return;
  if (!shell) {
    *width = 0;
    *height = 0;
    return;
  }

  double scale = shell->view ? puglGetScaleFactor(shell->view) : 1.0;
  if (scale < 1.0)
    scale = 1.0;
  *width = static_cast<std::uint32_t>(shell->width / scale);
  *height = static_cast<std::uint32_t>(shell->height / scale);
}

bool imgui_pugl_shell_can_resize(const ImGuiPuglShell *shell) {
  return shell && shell->embedded;
}

bool imgui_pugl_shell_was_closed(const ImGuiPuglShell *shell) {
  return shell && shell->wasClosed;
}

void imgui_pugl_shell_set_title(ImGuiPuglShell *shell, const char *title) {
  if (!shell || !shell->view || !title)
    return;
  puglSetViewString(shell->view, PUGL_WINDOW_TITLE, title);
}

void imgui_pugl_shell_tick(ImGuiPuglShell *shell) {
  if (!shell || shell->destroying || !shell->world)
    return;
  if (shell->view && shell->realized)
    puglObscureView(shell->view);
  puglUpdate(shell->world, 0.0);
}

void imgui_pugl_shell_start_timer(ImGuiPuglShell *shell) {
  if (!shell || !shell->view || !shell->realized)
    return;
  (void)puglStartTimer(shell->view, kRenderTimerId, 1.0 / 60.0);
}

void imgui_pugl_shell_stop_timer(ImGuiPuglShell *shell) {
  if (!shell || !shell->view || !shell->realized)
    return;
  puglStopTimer(shell->view, kRenderTimerId);
}

ImFont *imgui_pugl_shell_bold_font(ImGuiPuglShell *shell) {
  return shell ? shell->boldFont : nullptr;
}

ImGuiContext *imgui_pugl_shell_context(ImGuiPuglShell *shell) {
  return shell ? shell->imgui : nullptr;
}

} // namespace poryaaaa::gui
