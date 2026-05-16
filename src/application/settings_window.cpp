#include "settings_window.h"

#include <iostream>
#include <vector>

#include "gl_context.h"
#include "graphics_settings.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "texture_loader.h"
#include "texture_utilities.h"
#include "textures.h"

constexpr int GUIWidth{1024};
constexpr int GUIHeight{650};
constexpr int numColumns{2};
constexpr float baseFontSize = 22.0f;

SettingsWindow::~SettingsWindow()
{
    if (textureID_)
        glDeleteTextures(1, &textureID_);
}

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << description << std::endl;
}

bool SettingsWindow::launchSettingsWindow(GraphicsSettings &settings)
{
    if (!createWindow())
        return false;

    loadConfigWindowImage();

    setupImGui();

    matchSettingsToGUI(settings);

    bool settingsConfirmed{renderImGui(settings)};

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    if (textureID_)
        glDeleteTextures(1, &textureID_);

    return settingsConfirmed;
}

bool SettingsWindow::createWindow()
{
    // Init window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    GLFWmonitor *primary = glfwGetPrimaryMonitor();

    // Hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    window = glfwCreateWindow(GUIWidth, GUIHeight, "Island Graphics Demo", nullptr, nullptr);

    if (!window)
        return false;

    // Make current context
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Refocus
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    // Center
    int xpos, ypos, width, height;
    glfwGetMonitorWorkarea(primary, &xpos, &ypos, &width, &height);

    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    glfwSetWindowPos(window, xpos + width / 2 - winW / 2, ypos + height / 2 - winH / 2);

    return true;
}

void SettingsWindow::setupImGui()
{
    // Create ImGui context
    const char *glsl_version = "#version 460";
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Enable Keyboard Controls
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Configure backend rendering
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Get window scale for font size
    float windowScale{ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())};

    // Custom font
    io.Fonts->AddFontFromFileTTF("./res/Inter_18pt-Black.ttf", baseFontSize * windowScale);

    // Set Style
    ImGui::StyleColorsDark();

    // Scale all elements
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(windowScale);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
}

bool SettingsWindow::renderImGui(GraphicsSettings &settings)
{
    bool settingsConfirmed{false};

    float windowScale{ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())};
    const ImVec2 dummySpacing{0.0f, 10.0f * windowScale};

    // Loop until settings are confirmed
    while (!glfwWindowShouldClose(window))
    {
        using namespace ImGui;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        NewFrame();

        ImGuiViewport *imGuiViewPort{GetMainViewport()};

        float dpi{imGuiViewPort->DpiScale};
        ImGui::SetNextWindowSize(ImVec2{GUIWidth * dpi, GUIHeight * dpi}, ImGuiCond_Always);

        ImVec2 imguiCenter{imGuiViewPort->GetCenter()};
        SetNextWindowPos(imguiCenter, ImGuiCond_Always, ImVec2{0.5f, 0.5f});

        Begin("Island Graphics Demo - Display Settings", NULL,
              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        PushItemWidth(GetContentRegionAvail().x / numColumns);

        // Start of settings and image row
        BeginGroup();

        // Start of left column (all settings)
        BeginGroup();

        SeparatorText("Display Settings");
        Dummy(dummySpacing);

        Text("Resolution");
        if (BeginCombo("##resolutionCombo", resolutionPreviewValue_, ImGuiComboFlags_HeightLargest))
        {
            for (int i = 0; i < resolutions_.size(); ++i)
            {
                const bool isResolutionSelected{resolutionSelectedIndex_ == i};
                if (Selectable(resolutions_[i], isResolutionSelected))
                {
                    resolutionSelectedIndex_ = i;
                    resolutionPreviewValue_ = resolutions_[resolutionSelectedIndex_];
                    updateResolution(settings, i);
                }
                if (isResolutionSelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }

        Dummy(dummySpacing);

        Text("Gamma");
        SliderFloat("##gammaSlider", &settings.gamma, 0.8f, 1.3f, "%.2f", ImGuiSliderFlags_ClampOnInput);
        if (IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay))
            SetTooltip("Ctrl + Click to edit", GetStyle().HoverDelayShort);

        Spacing();

        Dummy(dummySpacing);
        Dummy(dummySpacing);

        Checkbox("Fullscreen", &settings.fullscreen);

        SameLine(GetContentRegionAvail().x / 4.0f);

        Checkbox("VSync", &settings.vsync);

        Dummy(dummySpacing);
        Dummy(dummySpacing);
        Spacing();
        Spacing();

        SeparatorText("Advanced Options");
        Dummy(dummySpacing);

        Text("Anisotropic Filtering");
        if (BeginCombo("##anisoCombo", anisoPreviewValue_))
        {
            for (int i{0}; i < anisoLevel.size(); ++i)
            {
                const bool isAnisoSelected{anisoSelectedIndex_ == i};
                if (Selectable(anisoLevel[i], isAnisoSelected))
                {
                    anisoSelectedIndex_ = i;
                    anisoPreviewValue_ = anisoLevel[anisoSelectedIndex_];
                    updateAniso(settings, i);
                }
                if (isAnisoSelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }

        Dummy(dummySpacing);

        Text("MSAA");
        if (BeginCombo("##msaaCombo", msaaPreviewValue_))
        {
            for (int i{0}; i < msaaLevel_.size(); ++i)
            {
                const bool isMSAASelected{msaaSelectedIndex_ == i};
                if (Selectable(msaaLevel_[i], isMSAASelected))
                {
                    msaaSelectedIndex_ = i;
                    msaaPreviewValue_ = msaaLevel_[msaaSelectedIndex_];
                    updateMSAA(settings, i);
                }
                if (isMSAASelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }

        Dummy(dummySpacing);

        Text("Shadow Quality");
        if (BeginCombo("##shadowCombo", shadowPreviewValue_))
        {
            for (int i{0}; i < shadowLevel.size(); ++i)
            {
                const bool isShadowSelected{shadowSelectedIndex_ == i};
                if (Selectable(shadowLevel[i], isShadowSelected))
                {
                    shadowSelectedIndex_ = i;
                    shadowPreviewValue_ = shadowLevel[shadowSelectedIndex_];
                    updateShadowQuality(settings, i);
                }
                if (isShadowSelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }

        PopItemWidth();

        // end of left column
        EndGroup();

        SameLine();

        // start of right column (image)
        BeginGroup();
        Dummy(dummySpacing);
        Dummy(dummySpacing);

        // Image
        Image((ImTextureID)(intptr_t)textureID_, ImVec2(GetContentRegionAvail().x, GetContentRegionAvail().x));

        Dummy(dummySpacing);

        // Reset button
        float resetButtonWidth{CalcTextSize("Reset").x + GetStyle().FramePadding.x * 2.0f};
        SetCursorPosX(GetCursorPosX() + GetContentRegionAvail().x - resetButtonWidth);
        if (Button("Reset"))
        {
            settings = GraphicsSettings{};
            matchSettingsToGUI(settings);
        }

        // End of right column
        EndGroup();

        // End of settings and image row
        EndGroup();

        // Buttons
        const float buttonHeight{GetFrameHeight()};
        const ImVec2 windowPadding{GetStyle().WindowPadding};
        const ImVec2 windowSize{GetWindowSize()};
        const ImVec2 windowPosition{GetWindowPos()};

        // Cancel button
        float cancelX{windowPadding.x};
        float cancelY{windowPosition.y + windowSize.y - buttonHeight - windowPadding.y};

        SetCursorScreenPos(ImVec2{cancelX, cancelY});
        if (Button("Cancel"))
            glfwSetWindowShouldClose(window, true);

        SameLine();

        // Confirm button
        float confirmButtonWidth{CalcTextSize("Confirm").x};

        float confirmXPos{windowPosition.x + windowSize.x - (windowPadding.x * 2.0f) - confirmButtonWidth};

        float confirmYPos{windowPosition.y + windowSize.y - buttonHeight - windowPadding.y};

        SetCursorScreenPos(ImVec2{confirmXPos, confirmYPos});
        if (Button("Confirm"))
        {
            settingsConfirmed = true;
            glfwSetWindowShouldClose(window, true);
        }

        // End
        End();

        // Rendering
        Render();
        int displayWidth{0};
        int displayHeight{0};
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return settingsConfirmed;
}

void SettingsWindow::loadConfigWindowImage()
{
    TextureDetails details{
        .sampler = {.magFilter = GL_LINEAR,
                    .minFilter = GL_LINEAR,
                    .wrapS = GL_CLAMP_TO_EDGE,
                    .wrapT = GL_CLAMP_TO_EDGE},
        .anisotropicFiltering = false,
    };

    textureID_ = TextureUtilities::textureFromFile("./res/textures/wireframe_sphere.ktx2", details);
}

void SettingsWindow::updateResolution(GraphicsSettings &settings, int index)
{
    if (index < resolutionTable_.size())
    {
        settings.screenWidth = resolutionTable_[index].first;
        settings.screenHeight = resolutionTable_[index].second;
    }
}

void SettingsWindow::updateMSAA(GraphicsSettings &settings, int index)
{
    if (index < msaaTable_.size())
        settings.msaa = msaaTable_[index];
}
void SettingsWindow::updateAniso(GraphicsSettings &settings, int index)
{
    if (index < anisoTable_.size())
        settings.anisotropicFiltering = anisoTable_[index];
}

void SettingsWindow::updateShadowQuality(GraphicsSettings &settings, int index)
{
    if (index < shadowTable_.size())
        settings.shadowResolution = shadowTable_[index];
}

void SettingsWindow::matchSettingsToGUI(const GraphicsSettings &settings)
{
    for (int i{0}; i < static_cast<int>(resolutionTable_.size()); ++i)
    {
        const auto &[width, height]{resolutionTable_[i]};

        if (width == settings.screenWidth && height == settings.screenHeight)
            resolutionSelectedIndex_ = i;
    }

    for (int i{0}; i < static_cast<int>(msaaTable_.size()); ++i)
    {
        if (msaaTable_[i] == settings.msaa)
            msaaSelectedIndex_ = i;
    }

    for (int i{0}; i < static_cast<int>(anisoTable_.size()); ++i)
    {
        if (anisoTable_[i] == settings.anisotropicFiltering)
            anisoSelectedIndex_ = i;
    }

    for (int i{0}; i < static_cast<int>(shadowTable_.size()); ++i)
    {
        if (shadowTable_[i] == settings.shadowResolution)
            shadowSelectedIndex_ = i;
    }

    if (resolutionSelectedIndex_ < resolutions_.size())
        resolutionPreviewValue_ = resolutions_[resolutionSelectedIndex_];

    if (msaaSelectedIndex_ < msaaLevel_.size())
        msaaPreviewValue_ = msaaLevel_[msaaSelectedIndex_];

    if (anisoSelectedIndex_ < anisoLevel.size())
        anisoPreviewValue_ = anisoLevel[anisoSelectedIndex_];

    if (shadowSelectedIndex_ < shadowLevel.size())
        shadowPreviewValue_ = shadowLevel[shadowSelectedIndex_];
}