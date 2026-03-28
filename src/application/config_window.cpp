#include "config_window.h"

#include <iostream>
#include <vector>

#include "gl_context.h"
#include "image_loader.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "texture_utilities.h"

constexpr int GUIWidth{1024};
constexpr int GUIHeight{625};
constexpr int numColumns{2};
constexpr float baseFontSize = 22.0f;

ConfigWindow::~ConfigWindow()
{
    if (textureID)
        glDeleteTextures(1, &textureID);
}

static void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << description << std::endl;
}

bool ConfigWindow::launchSettingsWindow(GraphicsSettings &settings)
{
    if (!createWindow())
        return false;

    loadConfigWindowImage();

    setupImGui();

    bool settingsConfirmed{renderImGui(settings)};

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    if (textureID)
        glDeleteTextures(1, &textureID);

    return settingsConfirmed;
}

bool ConfigWindow::createWindow()
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

    window = glfwCreateWindow(GUIWidth, GUIHeight, "Island Graphics Demo - Display Settings", nullptr, nullptr);

    if (!window)
        return false;

    // Make current context
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

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

void ConfigWindow::setupImGui()
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

bool ConfigWindow::renderImGui(GraphicsSettings &settings)
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
        Text("Resolution");
        if (BeginCombo("##resolutionCombo", resolutionPreviewValue, ImGuiComboFlags_HeightLargest))
        {
            for (int i = 0; i < resolutions.size(); ++i)
            {
                const bool isResolutionSelected{resolutionSelectedIndex == i};
                if (Selectable(resolutions[i], isResolutionSelected))
                {
                    resolutionSelectedIndex = i;
                    resolutionPreviewValue = resolutions[resolutionSelectedIndex];
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

        Dummy(dummySpacing);

        Checkbox("Fullscreen", &settings.fullscreen);

        SameLine(GetContentRegionAvail().x / 4.0f);

        Checkbox("VSync", &settings.vsync);

        Dummy(dummySpacing);
        Dummy(dummySpacing);
        Dummy(dummySpacing);

        SeparatorText("Advanced Options");

        Dummy(dummySpacing);

        Text("Anisotropic Filtering");
        if (BeginCombo("##anisoCombo", anisoPreviewValue))
        {
            for (int i{0}; i < anisoLevel.size(); ++i)
            {
                const bool isAnisoSelected{anisoSelectedIndex == i};
                if (Selectable(anisoLevel[i], isAnisoSelected))
                {
                    anisoSelectedIndex = i;
                    anisoPreviewValue = anisoLevel[anisoSelectedIndex];
                    updateAniso(settings, i);
                }
                if (isAnisoSelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }

        Dummy(dummySpacing);

        Text("MSAA");
        if (BeginCombo("##msaaCombo", msaaPreviewValue))
        {
            for (int i{0}; i < msaaLevel.size(); ++i)
            {
                const bool isMSAASelected{msaaSelectedIndex == i};
                if (Selectable(msaaLevel[i], isMSAASelected))
                {
                    msaaSelectedIndex = i;
                    msaaPreviewValue = msaaLevel[msaaSelectedIndex];
                    updateMSAA(settings, i);
                }
                if (isMSAASelected)
                    SetItemDefaultFocus();
            }
            EndCombo();
        }

        Dummy(dummySpacing);

        Text("Shadow Quality");
        if (BeginCombo("##shadowCombo", shadowPreviewValue))
        {
            for (int i{0}; i < shadowQuality.size(); ++i)
            {
                const bool isShadowSelected{shadowSelectedIndex == i};
                if (Selectable(shadowQuality[i], isShadowSelected))
                {
                    shadowSelectedIndex = i;
                    shadowPreviewValue = shadowQuality[shadowSelectedIndex];
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

        Image((ImTextureID)(intptr_t)textureID, ImVec2(GetContentRegionAvail().x, GetContentRegionAvail().x));

        // end of right column
        EndGroup();

        // end of settings and image row
        EndGroup();

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

void ConfigWindow::loadConfigWindowImage()
{
    ImageLoader imageLoader{"./res/textures/wireframe_sphere.png"};
    std::vector<LoadedImage> imageData{imageLoader.getImageData()};

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return;
    }

    if (!imageData.size())
        return;

    textureID = textureUtilities::generateTextureID(imageData[0], Sampler{}, TextureDetails{});
}

void ConfigWindow::updateResolution(GraphicsSettings &settings, int index)
{
    if (index < resolutionTable.size())
    {
        settings.screenWidth = resolutionTable[index].first;
        settings.screenHeight = resolutionTable[index].second;
    }
}

void ConfigWindow::updateMSAA(GraphicsSettings &settings, int index)
{
    if (index < msaaTable.size())
        settings.msaa = msaaTable[index];
}
void ConfigWindow::updateAniso(GraphicsSettings &settings, int index)
{
    if (index < anisoTable.size())
        settings.anisotropicFiltering = anisoTable[index];
}

void ConfigWindow::updateShadowQuality(GraphicsSettings &settings, int index)
{
    if (index < shadowTable.size())
        settings.shadowResolution = shadowTable[index];
}