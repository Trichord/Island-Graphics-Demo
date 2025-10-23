#include "../headers/config_window.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../headers/texture_utilities.h"
#include "../headers/image_loader.h"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <vector>
#include <iostream>

#define GUI_WIDTH 1024
#define GUI_HEIGHT 576
#define NUM_COLUMNS 2

ConfigWindow::~ConfigWindow() {
  if (textureID != 0) glDeleteTextures(1, &textureID);
}

static void glfw_error_callback(int error, const char* description) {
  std::cerr << "GLFW Error " << error << description << std::endl;
}

bool ConfigWindow::launchSettingsWindow() {
  if (!createWindow()) return false;

  loadConfigWindowImage();

  setupImGui();

  bool settingsConfirmed = renderImGui();

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  if (textureID != 0) glDeleteTextures(1, &textureID);

  return settingsConfirmed;
}

bool ConfigWindow::createWindow() {
  // Init window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return false;

  // Hints
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

  // Create window with graphics context
  float main_scale =
      ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  window = glfwCreateWindow(
      (int)(GUI_WIDTH * main_scale), (int)(GUI_HEIGHT * main_scale),
      "OpenGL 4.6 Tech Demo - Graphics Settings", nullptr, nullptr);

  if (window == nullptr) return false;

  // Center window
  ImVec2 windowCenter = {GUI_WIDTH / 2, GUI_HEIGHT / 2};
  int workX, workY, workWidth, workHeight;
  glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &workX, &workY, &workWidth,
                         &workHeight);
  glfwSetWindowPos(window, (int)(workWidth / 2 - windowCenter.x),
                   (int)(workHeight / 2 - windowCenter.y));

  // Make current context
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  return true;
}

void ConfigWindow::setupImGui() {
  // Create ImGui context
  const char* glsl_version = "#version 460";
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Enable Keyboard Controls
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Configure backend rendering
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Setup Dear ImGui style and custom font
  ImGui::StyleColorsDark();
  io.Fonts->AddFontFromFileTTF("./res/Inter_18pt-Black.ttf");
}

bool ConfigWindow::renderImGui() {
  ImVec2 guiSize = {GUI_WIDTH, GUI_HEIGHT};

  // Loop until settings are confirmed
  while (true) {
    glfwPollEvents();

    // sleep if minimized
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Centered and sized to fit the glfw window
    ImVec2 windowCenter = {GUI_WIDTH / 2, GUI_HEIGHT / 2};
    ImGui::SetNextWindowSize(guiSize);
    ImGui::SetNextWindowPos(windowCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    // Dear ImGUI window
    ImGui::Begin("OpenGL 4.6 Tech Demo - Graphics Settings", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // start of display settings, image, and advanced options group
    ImGui::BeginGroup();

    // start of display settings group
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / NUM_COLUMNS);
    ImGui::BeginGroup();
    ImGui::SeparatorText("Display Settings");
    ImGui::Spacing();

    // Resolution
    ImGui::Text("Resolution");
    if (ImGui::BeginCombo("##resolutionCombo", resolutionPreviewValue,
                          ImGuiComboFlags_HeightLargest)) {
      for (int i = 0; i < resolutions.size(); ++i) {
        const bool is_res_selected = (resolutionSelectedIndex == i);
        if (ImGui::Selectable(resolutions[i], is_res_selected)) {
          resolutionSelectedIndex = i;
          resolutionPreviewValue = resolutions[resolutionSelectedIndex];
          updateGlobalResolution(i);
        }
        if (is_res_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::Spacing();

    ImGui::Checkbox("Fullscreen", &globalGraphicsSettings.fullscreen);
    ImGui::Spacing();

    ImGui::Checkbox("VSync", &globalGraphicsSettings.vsync);
    ImGui::Spacing();

    // Gamma
    ImGui::Text("Gamma");
    ImGui::SliderFloat("##gammaSlider", &globalGraphicsSettings.gamma, 0.5f,
                       3.0f, "%.3f", ImGuiSliderFlags_ClampOnInput);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort |
                             ImGuiHoveredFlags_NoSharedDelay))
      ImGui::SetTooltip("Ctrl + Click to edit",
                        ImGui::GetStyle().HoverDelayShort);

    // Advanced options
    ImGui::NewLine();
    ImGui::SeparatorText("Advanced Options");
    ImGui::Spacing();

    // MSAA
    ImGui::Text("MSAA");
    if (ImGui::BeginCombo("##msaaCombo", msaaPreviewValue,
                          ImGuiComboFlags_HeightLargest)) {
      for (int i = 0; i < msaaLevel.size(); ++i) {
        const bool is_msaa_selected = (msaaSelectedIndex == i);
        if (ImGui::Selectable(msaaLevel[i], is_msaa_selected)) {
          msaaSelectedIndex = i;
          msaaPreviewValue = msaaLevel[msaaSelectedIndex];
          updateGlobalMSAA(i);
        }
        if (is_msaa_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::Spacing();

    // Anisotropic filtering
    ImGui::Text("Anisotropic Filtering");
    if (ImGui::BeginCombo("##anisoCombo", anisoPreviewValue,
                          ImGuiComboFlags_HeightLargest)) {
      for (int i = 0; i < anisoLevel.size(); ++i) {
        const bool is_aniso_selected = (anisoSelectedIndex == i);
        if (ImGui::Selectable(anisoLevel[i], is_aniso_selected)) {
          anisoSelectedIndex = i;
          anisoPreviewValue = anisoLevel[anisoSelectedIndex];
          updateGlobalAniso(i);
        }
        if (is_aniso_selected) ImGui::SetItemDefaultFocus();
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
                 ImVec2(ImGui::GetContentRegionAvail().x,
                        ImGui::GetContentRegionAvail().x));

    // end of image and display settings
    ImGui::EndGroup();

    // end of display settings, image, and advanced options group
    ImGui::EndGroup();

    ImVec2 windowPadding = ImGui::GetStyle().WindowPadding;
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 windowPosition = ImGui::GetWindowPos();
    float buttonHeight = ImGui::GetFrameHeight();

    ImGui::SetCursorScreenPos(ImVec2(
        windowPadding.x,
        windowPosition.y + windowSize.y - buttonHeight - windowPadding.y));
    if (ImGui::Button("Cancel")) return false;

    ImGui::SameLine();

    float buttonWidth = ImGui::CalcTextSize("Confirm").x;

    ImGui::SetCursorScreenPos(ImVec2(
        windowPosition.x + windowSize.x - (windowPadding.x * 2.0f) -
            buttonWidth,
        windowPosition.y + windowSize.y - buttonHeight - windowPadding.y));
    if (ImGui::Button("Confirm")) break;
    ;

    // End
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }
  return true;
}

void ConfigWindow::loadConfigWindowImage() {
  ImageLoader imageLoader("./textures/wireframe_sphere.png");
  std::vector<ImageData> imageData = imageLoader.getImageData();

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return;
  }

  if (imageData.size() == 0) return;

  textureID = texture_utilities::generateTextureID(imageData[0], false, false);
}

void ConfigWindow::updateGlobalResolution(unsigned int index) {
  if (index < resolutionTable.size()) {
    globalGraphicsSettings.screenWidth = resolutionTable[index].first;
    globalGraphicsSettings.screenHeight = resolutionTable[index].second;
  }
}

void ConfigWindow::updateGlobalMSAA(unsigned int index) {
  if (index < msaaTable.size()) globalGraphicsSettings.msaa = msaaTable[index];
}
void ConfigWindow::updateGlobalAniso(unsigned int index) {
  if (index < anisoTable.size())
    globalGraphicsSettings.anisotropicFiltering = anisoTable[index];
}