#pragma once

#include <array>
#include <utility>

#include "gl_context.h"

/**
 * @brief Creates a GLFW window, using Dear ImGUI as a user interface.  Allows
 * the user to adjust display settings.
 */
class ConfigWindow {
 public:
  struct GraphicsSettings {
    int screenWidth{2560};
    int screenHeight{1440};
    bool fullscreen{true};
    bool vsync{false};
    int msaa{8};
    int anisotropicFiltering{16};
    float gamma{1.0f};
  };

  ConfigWindow() = default;
  ConfigWindow(const ConfigWindow&) = delete;
  ConfigWindow& operator=(const ConfigWindow&) = delete;
  ConfigWindow(ConfigWindow&& other) = delete;
  ConfigWindow& operator=(ConfigWindow&& other) = delete;
  ~ConfigWindow();

  /**
   * @brief Creates the Display Settings window.
   *
   * @return Settings confirmed or cancelled.
   */
  bool launchSettingsWindow(GraphicsSettings& settings);

 private:
  GLFWwindow* window{nullptr};
  inline static const std::array<const char*, 6> resolutions{
      "3840 x 2160", "2560 x 1440", "1920 x 1080",
      "1600 x 900",  "1366 x 768",  "1280 x 720"};

  inline static const std::array<std::pair<int, int>, 6>
      resolutionTable{{{3840, 2160},
                       {2560, 1440},
                       {1920, 1080},
                       {1600, 900},
                       {1366, 768},
                       {1280, 720}}};

  inline static const std::array<const char*, 4> msaaLevel{"Off", "2x", "4x",
                                                           "8x"};
  inline static const std::array<int, 4> msaaTable{0, 2, 4, 8};
  inline static const std::array<const char*, 5> anisoLevel{"Off", "2x", "4x",
                                                            "8x", "16x"};
  inline static const std::array<int, 5> anisoTable{0, 2, 4, 8, 16};

  inline static int resolutionSelectedIndex{1};
  inline static int msaaSelectedIndex{2};
  inline static int anisoSelectedIndex{4};
  inline static const char* resolutionPreviewValue{
      resolutions[resolutionSelectedIndex]};
  inline static const char* msaaPreviewValue{msaaLevel[msaaSelectedIndex]};
  inline static const char* anisoPreviewValue{anisoLevel[anisoSelectedIndex]};

  GLuint textureID{0};

  /**
   * @brief Creates the GLFW window, centered and scaled by monitor zoom.
   */
  bool createWindow();

  /**
   * @brief Creates Dear ImGui context. Sets Dear ImGui style and loads a custom
   * font.
   */
  void setupImGui();

  /**
   * @brief Creates an OpenGL texture ID for the configuration window's image.
   */
  void loadConfigWindowImage();

  /**
   * @brief The Dear ImGui render loop. Contains all user interface logic.
   *
   * @returns Settings confirmed or cancelled.
   */
  bool renderImGui(GraphicsSettings& settings);

  /**
   * @brief Helper function for the resolution drop-down menu.
   *
   * @param index The resolution drop-down selected index.
   */
  void updateResolution(GraphicsSettings& settings, int index);

  /**
   * @brief Helper function for the MSAA drop-down menu.
   *
   * @param index The msaa drop-down selected index.
   */
  void updateMSAA(GraphicsSettings& settings, int index);

  /**
   * @brief Helper function for anisotropic filtering drop-down menu.
   *
   * @param index The anisotropic filtering drop-down selected index.
   */
  void updateAniso(GraphicsSettings& settings, int index);
};