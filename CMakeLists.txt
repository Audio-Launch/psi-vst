cmake_minimum_required(VERSION 3.22)

project(PsiSpectral VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE 8 (fetched automatically; no manual install needed) ─────────────────
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# ── The plugin ───────────────────────────────────────────────────────────────
# NOTE: NEEDS_WEB_BROWSER TRUE pulls in JUCE's WebBrowserComponent. On Windows
# we use the WebView2 (Chromium) backend so the modern web UI renders — the
# legacy IE backend cannot run it. See BUILD.md for the WebView2 requirement.
juce_add_plugin(PsiSpectral
  COMPANY_NAME "AudioLab"
  BUNDLE_ID tools.audiolab.psi
  PLUGIN_MANUFACTURER_CODE Aulb        # 4 chars — TODO Nathan: confirm
  PLUGIN_CODE PsiS                     # 4 chars — TODO Nathan: confirm
  FORMATS VST3 Standalone
  PRODUCT_NAME "Psi Spectral"
  NEEDS_WEB_BROWSER TRUE)

target_sources(PsiSpectral PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(PsiSpectral PUBLIC
  PSI_VERSION="${PROJECT_VERSION}"                # synced with project(... VERSION); used by the update check
  JUCE_WEB_BROWSER=1
  JUCE_USE_WIN_WEBVIEW2=1                        # Chromium backend on Windows
  JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1    # use WebView2LoaderStatic.lib — no loose DLL needed
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_USE_CURL=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_include_directories(PsiSpectral PRIVATE dsp)

target_link_libraries(PsiSpectral
  PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_cryptography              # SHA256 for the PKCE challenge (DeviceConnect.h)
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ── WebView2 SDK — Chromium backend so the web UI renders (Windows) ───────────
# SDK fetched from the Microsoft.Web.WebView2 NuGet into third_party/ (git-ignored).
if (WIN32)
  set(WEBVIEW2 "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/build/native")
  target_include_directories(PsiSpectral PRIVATE "${WEBVIEW2}/include")
  target_link_libraries(PsiSpectral PRIVATE "${WEBVIEW2}/x64/WebView2LoaderStatic.lib")
  target_link_libraries(PsiSpectral PRIVATE bcrypt)  # BCryptGenRandom — PKCE verifier entropy
  target_link_libraries(PsiSpectral PRIVATE crypt32) # CryptProtectData — DPAPI key-at-rest
endif()

# ── Self-contained UI: bake the built web bundle into the plugin ──────────────
# Release (default): the WebView is served from webui.zip embedded as BinaryData, so
# the plugin needs NO running dev server. Regenerate the zip with scripts/bundle-ui.ps1
# whenever the web UI changes, then rebuild. Dev loop: configure with -DPSI_DEV_SERVER=ON
# to instead load http://localhost:4321/plugin (Vite hot-reload).
option(PSI_DEV_SERVER "Load the UI from the Vite dev server instead of the embedded bundle" OFF)

if (PSI_DEV_SERVER)
  target_compile_definitions(PsiSpectral PRIVATE PSI_DEV_SERVER=1)
else()
  set(PSI_WEBUI_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/webui.zip")
  if (NOT EXISTS "${PSI_WEBUI_ZIP}")
    message(FATAL_ERROR "webui.zip not found. Run scripts/bundle-ui.ps1 first, or configure with -DPSI_DEV_SERVER=ON for the dev loop.")
  endif()
  juce_add_binary_data(PsiWebUI SOURCES "${PSI_WEBUI_ZIP}")
  target_link_libraries(PsiSpectral PRIVATE PsiWebUI)
  target_compile_definitions(PsiSpectral PRIVATE PSI_DEV_SERVER=0)
endif()
