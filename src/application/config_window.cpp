#include "config_window.h"

#include <iostream>
#include <vector>

#include "gl_context.h"
#include "image_loader.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "texture_utilities.h"

namespace
{
constexpr int GUIWidth{1024};
constexpr int GUIHeight{576};
constexpr int numColumns{2};
constexpr float baseFontSize = 22.0f;
} // namespace

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

    // Hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Create window with graphics context
    float windowScale{ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor())};
    window = glfwCreateWindow(static_cast<int>(GUIWidth * windowScale), static_cast<int>(GUIHeight * windowScale),
                              "Island Graphics Demo - Display Settings", nullptr, nullptr);

    if (!window)
        return false;

    // Make current context
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Refocus
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

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
}

bool ConfigWindow::renderImGui(GraphicsSettings &settings)
{
    bool settingsConfirmed{false};

    // Loop until settings are confirmed
    while (!glfwWindowShouldClose(window))
    {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport *imGuiViewPort{ImGui::GetMainViewport()};

        // Size
        float dpi{imGuiViewPort->DpiScale};
        ImGui::SetNextWindowSize(ImVec2{GUIWidth * dpi, GUIHeight * dpi}, ImGuiCond_Always);

        // Center
        ImVec2 imguiCenter{imGuiViewPort->GetCenter()};
        ImGui::SetNextWindowPos(imguiCenter, ImGuiCond_Always, ImVec2{0.5f, 0.5f});

        // Dear ImGUI window
        ImGui::Begin("Island Graphics Demo - Display Settings", NULL,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        // start of display settings, image, and advanced options group
        ImGui::BeginGroup();

        // start of display settings group
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / numColumns);
        ImGui::BeginGroup();
        ImGui::SeparatorText("Display Settings");
        ImGui::Spacing();

        // Resolution
        ImGui::Text("Resolution");
        if (ImGui::BeginCombo("##resolutionCombo", resolutionPreviewValue, ImGuiComboFlags_HeightLargest))
        {
            for (int i = 0; i < resolutions.size(); ++i)
            {
                const bool isResolutionSelected{resolutionSelectedIndex == i};
                if (ImGui::Selectable(resolutions[i], isResolutionSelected))
                {
                    resolutionSelectedIndex = i;
                    resolutionPreviewValue = resolutions[resolutionSelectedIndex];
                    updateResolution(settings, i);
                }
                if (isResolutionSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        ImGui::Checkbox("Fullscreen", &settings.fullscreen);
        ImGui::Spacing();

        ImGui::Checkbox("VSync", &settings.vsync);
        ImGui::Spacing();

        // Gamma
        ImGui::Text("Gamma");
        ImGui::SliderFloat("##gammaSlider", &settings.gamma, 0.8f, 1.3f, "%.2f", ImGuiSliderFlags_ClampOnInput);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_NoSharedDelay))
            ImGui::SetTooltip("Ctrl + Click to edit", ImGui::GetStyle().HoverDelayShort);

        // Advanced options
        ImGui::NewLine();
        ImGui::SeparatorText("Advanced Options");
        ImGui::Spacing();

        // MSAA
        ImGui::Text("MSAA");
        if (ImGui::BeginCombo("##msaaCombo", msaaPreviewValue, ImGuiComboFlags_HeightLargest))
        {
            for (int i = 0; i < msaaLevel.size(); ++i)
            {
                const bool isMSAASelected{msaaSelectedIndex == i};
                if (ImGui::Selectable(msaaLevel[i], isMSAASelected))
                {
                    msaaSelectedIndex = i;
                    msaaPreviewValue = msaaLevel[msaaSelectedIndex];
                    updateMSAA(settings, i);
                }
                if (isMSAASelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        // Anisotropic filtering
        ImGui::Text("Anisotropic Filtering");
        if (ImGui::BeginCombo("##anisoCombo", anisoPreviewValue, ImGuiComboFlags_HeightLargest))
        {
            for (int i = 0; i < anisoLevel.size(); ++i)
            {
                const bool isAnisoSelected{anisoSelectedIndex == i};
                if (ImGui::Selectable(anisoLevel[i], isAnisoSelected))
                {
                    anisoSelectedIndex = i;
                    anisoPreviewValue = anisoLevel[anisoSelectedIndex];
                    updateAniso(settings, i);
                }
                if (isAnisoSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // end of display settigns
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        // start of image group
        ImGui::SameLine();
        ImGui::BeginGroup();

        // Image
        ImGui::Image((ImTextureID)(intptr_t)textureID,
                     ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().x));

        // end of image and display settings
        ImGui::EndGroup();

        // end of display settings, image, and advanced options group
        ImGui::EndGroup();

        // Buttons
        const float buttonHeight{ImGui::GetFrameHeight()};
        const ImVec2 windowPadding{ImGui::GetStyle().WindowPadding};
        const ImVec2 windowSize{ImGui::GetWindowSize()};
        const ImVec2 windowPosition{ImGui::GetWindowPos()};

        // Cancel button
        float cancelX{windowPadding.x};
        float cancelY{windowPosition.y + windowSize.y - buttonHeight - windowPadding.y};

        ImGui::SetCursorScreenPos(ImVec2{cancelX, cancelY});
        if (ImGui::Button("Cancel"))
            glfwSetWindowShouldClose(window, true);

        ImGui::SameLine();

        // Confirm button
        float confirmButtonWidth{ImGui::CalcTextSize("Confirm").x};

        float confirmXPos{windowPosition.x + windowSize.x - (windowPadding.x * 2.0f) - confirmButtonWidth};

        float confirmYPos{windowPosition.y + windowSize.y - buttonHeight - windowPadding.y};

        ImGui::SetCursorScreenPos(ImVec2{confirmXPos, confirmYPos});
        if (ImGui::Button("Confirm"))
        {
            settingsConfirmed = true;
            glfwSetWindowShouldClose(window, true);
        }

        // End
        ImGui::End();

        // Rendering
        ImGui::Render();
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

    textureID = textureUtilities::generateTextureID(imageData[0], Sampler{}, TextureDetails{}, false);
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