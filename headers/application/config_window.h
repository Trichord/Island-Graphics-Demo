#pragma once

#include <array>
#include <utility>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

/**
 * @class ConfigWindow
 * @brief Creates a GLFW window, using Dear ImGUI as a user interface.  Allows
 * the user to adjust display settings.
 */
class ConfigWindow {
 public:
  static inline struct GraphicsSettings {
    unsigned int screenWidth{2560};
    unsigned int screenHeight{1440};
    bool fullscreen{true};
    bool vsync{false};
    unsigned int msaa{8};
    float anisotropicFiltering{16.0f};
    float gamma{1.0f};
  } globalGraphicsSettings;

  ConfigWindow() = default;
  ConfigWindow(const ConfigWindow&) = delete;
  ConfigWindow& operator=(const ConfigWindow&) = delete;
  ConfigWindow(ConfigWindow&& other) = delete;
  ConfigWindow& operator=(ConfigWindow&& other) = delete;
  ~ConfigWindow();

  /**
  * Creates the Display Settings window.
  * 
  * @return Settings confirmed or cancelled.
  */
  bool launchSettingsWindow();

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

  inline static const std::array<const char*, 3> msaaLevel{"2x", "4x", "8x"};
  inline static const std::array<unsigned int, 3> msaaTable{2, 4, 8};
  inline static const std::array<const char*, 5> anisoLevel{"Off", "2x", "4x",
                                                            "8x", "16x"};
  inline static const std::array<float, 5> anisoTable{0.0f, 2.0f, 4.0f, 8.0f,
                                                      16.0f};

  inline static int resolutionSelectedIndex{1};
  inline static int msaaSelectedIndex{2};
  inline static int anisoSelectedIndex{4};
  inline static const char* resolutionPreviewValue{
      resolutions[resolutionSelectedIndex]};
  inline static const char* msaaPreviewValue{msaaLevel[msaaSelectedIndex]};
  inline static const char* anisoPreviewValue{anisoLevel[anisoSelectedIndex]};

  GLuint textureID{0};

  /**
  * Creates the GLFW window, centered and scaled by monitor zoom.
  */
  bool createWindow();

  /**
  * Creates Dear ImGui context. Sets Dear ImGui style and loads a custom font.
  */
  void setupImGui();

  /**
  * Creates an OpenGL texture ID for the configuration window's image.
  */
  void loadConfigWindowImage();

  /**
  * The Dear ImGui render loop. Contains all user interface logic. 
  * 
  * @returns Settings confirmed or cancelled.
  */
  bool renderImGui();

  /**
  * Helper function for the resolution drop-down menu.  Updates globalGraphicsSettings.
  * 
  * @param index The drop-down selected index.
  */
  void updateGlobalResolution(unsigned int index);

  /**
  * Helper function for the MSAA drop-down menu.  Updates globalGraphicsSettings.
  */
  void updateGlobalMSAA(unsigned int index);
  void updateGlobalAniso(unsigned int index);
};