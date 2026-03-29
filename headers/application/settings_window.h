#pragma once

#include <array>
#include <utility>

#include "gl_context.h"
#include "graphics_settings.h"

/**
 * @brief Creates a GLFW window, using Dear ImGUI as a user interface.  Allows
 * the user to adjust display settings.
 */
class SettingsWindow
{
  public:
    SettingsWindow() = default;
    SettingsWindow(const SettingsWindow &) = delete;
    SettingsWindow &operator=(const SettingsWindow &) = delete;
    SettingsWindow(SettingsWindow &&other) = delete;
    SettingsWindow &operator=(SettingsWindow &&other) = delete;
    ~SettingsWindow();

    /**
     * @brief Creates the Display Settings window.
     *
     * @return Settings confirmed or cancelled.
     */
    bool launchSettingsWindow(GraphicsSettings &settings);

  private:
    GLFWwindow *window{nullptr};
    inline static const std::array<const char *, 6> resolutions{"3840 x 2160", "2560 x 1440", "1920 x 1080",
                                                                "1600 x 900",  "1366 x 768",  "1280 x 720"};

    inline static const std::array<std::pair<int, int>, 6> resolutionTable{
        {{3840, 2160}, {2560, 1440}, {1920, 1080}, {1600, 900}, {1366, 768}, {1280, 720}}};

    inline static const std::array<const char *, 3> msaaLevel{"2x", "4x", "8x"};
    static constexpr std::array<int, 3> msaaTable{2, 4, 8};

    inline static const std::array<const char *, 5> anisoLevel{"Off", "2x", "4x", "8x", "16x"};
    static constexpr std::array<int, 5> anisoTable{0, 2, 4, 8, 16};

    inline static const std::array<const char *, 3> shadowLevel{"Off", "Medium", "High"};
    static constexpr std::array<int, 3> shadowTable{1, 1024, 2048};

    int resolutionSelectedIndex{1};
    int msaaSelectedIndex{2};
    int anisoSelectedIndex{4};
    int shadowSelectedIndex{1};

    const char *msaaPreviewValue{msaaLevel[msaaSelectedIndex]};
    const char *anisoPreviewValue{anisoLevel[anisoSelectedIndex]};
    const char *shadowPreviewValue{shadowLevel[shadowSelectedIndex]};
    const char *resolutionPreviewValue{resolutions[resolutionSelectedIndex]};

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
    bool renderImGui(GraphicsSettings &settings);

    /**
     * @brief Helper function for the resolution drop-down menu.
     *
     * @param index The resolution drop-down selected index.
     */
    void updateResolution(GraphicsSettings &settings, int index);

    /**
     * @brief Helper function for the MSAA drop-down menu.
     *
     * @param index The msaa drop-down selected index.
     */
    void updateMSAA(GraphicsSettings &settings, int index);

    /**
     * @brief Helper function for anisotropic filtering drop-down menu.
     *
     * @param index The anisotropic filtering drop-down selected index.
     */
    void updateAniso(GraphicsSettings &settings, int index);

    /**
     * @brief Helper function for shadow-quality drop-down menu.
     * @param The requested index from the drop-down menu.
     */
    void updateShadowQuality(GraphicsSettings &settings, int index);

    void matchSettingsToGUI(const GraphicsSettings &settings);
};