#pragma once

#include <array>
#include <utility>

#include "application/gl_context.h"
#include "application/graphics_settings.h"

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
    GLFWwindow *settingsWindow_{nullptr};
    inline static const std::array<const char *, 6> resolutions_{"3840 x 2160", "2560 x 1440", "1920 x 1080",
                                                                 "1600 x 900",  "1366 x 768",  "1280 x 720"};

    inline static const std::array<std::pair<int, int>, 6> resolutionTable_{
        {{3840, 2160}, {2560, 1440}, {1920, 1080}, {1600, 900}, {1366, 768}, {1280, 720}}};

    inline static const std::array<const char *, 3> msaaLevel_{"2x", "4x", "8x"};
    static constexpr std::array<int, 3> msaaTable_{2, 4, 8};

    inline static const std::array<const char *, 5> anisoLevel_{"Off", "2x", "4x", "8x", "16x"};
    static constexpr std::array<int, 5> anisoTable_{0, 2, 4, 8, 16};

    inline static const std::array<const char *, 4> shadowQuality_{"Off", "Low", "Medium", "High"};
    static constexpr std::array<int, 4> shadowTable_{0, 1024, 2048, 4096};

    inline static const std::array<const char *, 3> reflectionQuality_{"Low", "Medium", "High"};
    static constexpr std::array<int, 3> reflectionTable_{512, 1024, 2048};

    int msaaSelectedIndex_{1};
    int anisoSelectedIndex_{2};
    int shadowSelectedIndex_{2};
    int resolutionSelectedIndex_{2};
    int reflectionSelectedIndex_{1};

    const char *msaaPreviewValue_{msaaLevel_[msaaSelectedIndex_]};
    const char *anisoPreviewValue_{anisoLevel_[anisoSelectedIndex_]};
    const char *shadowPreviewValue_{shadowQuality_[shadowSelectedIndex_]};
    const char *resolutionPreviewValue_{resolutions_[resolutionSelectedIndex_]};
    const char *reflectionPreviewValue_{reflectionQuality_[reflectionSelectedIndex_]};

    GLuint textureID_{0};
    bool createWindow();
    void setupImGui();
    void loadConfigWindowImage();

    /**
     * @brief Renders the settings window until the user confirms settings, hits cancel, or closes the window.
     *
     * @return Confirm button clicked.
     */
    bool renderSettingsWindow(GraphicsSettings &settings);

    void updateResolution(GraphicsSettings &settings, int index);
    void updateMSAA(GraphicsSettings &settings, int index);
    void updateAniso(GraphicsSettings &settings, int index);
    void updateShadowQuality(GraphicsSettings &settings, int index);
    void updateReflectionQuality(GraphicsSettings &settings, int index);

    /**
     * @brief Loops through the loaded JSON file and attempts to match JSON settings to dropdown menu choices.
     * Unmatched values are displayed as default, but the underlying graphics setting is not overridden. This allows
     * users to edit the JSON for display resolutions not found in the dropdowns, higher shadow map resolution,
     * etc.
     */
    void matchSettingsToGUI(const GraphicsSettings &settings);
};