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
    unsigned int screenWidth{2560};
    unsigned int screenHeight{1440};
    bool fullscreen{true};
    bool vsync{false};
    unsigned int msaa{8};
    unsigned int anisotropicFiltering{16u};
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

  inline static const std::array<std::pair<unsigned int, unsigned int>, 6>
      resolutionTable{{{3840u, 2160u},
                       {2560u, 1440u},
                       {1920u, 1080u},
                       {1600u, 900u},
                       {1366u, 768u},
                       {1280u, 720}}};

  inline static const std::array<const char*, 4> msaaLevel{"Off", "2x", "4x",
                                                           "8x"};
  inline static const std::array<unsigned int, 4> msaaTable{0, 2, 4, 8};
  inline static const std::array<const char*, 5> anisoLevel{"Off", "2x", "4x",
                                                            "8x", "16x"};
  inline static const std::array<unsigned int, 5> anisoTable{0u, 2u, 4u, 8u,
                                                      16u};

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
  void updateResolution(GraphicsSettings& settings, unsigned int index);

  /**
   * @brief Helper function for the MSAA drop-down menu.
   *
   * @param index The msaa drop-down selected index.
   */
  void updateMSAA(GraphicsSettings& settings, unsigned int index);

  /**
   * @brief Helper function for anisotropic filtering drop-down menu.
   *
   * @param index The anisotropic filtering drop-down selected index.
   */
  void updateAniso(GraphicsSettings& settings, unsigned int index);
};