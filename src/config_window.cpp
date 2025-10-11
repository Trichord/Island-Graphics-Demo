#include "../headers/config_window.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_glfw.h"
#include "../imgui/imgui_impl_opengl3.h"
#include "../headers/texture_utilities.h"
#include "../headers/image_loader.h"
#include <filesystem>
#include <vector>
#include <GLFW/glfw3.h>
#include "glad/glad.h"
#include <iostream>

#define GUI_WIDTH 1024
#define GUI_HEIGHT 768
#define ELEMENT_SIZE 0.95f

ConfigWindow::~ConfigWindow() { if(textureID != 0) glDeleteTextures(1, &textureID); }

static void glfw_error_callback(int error, const char* description) {
  std::cerr << "GLFW Error " << error << description << std::endl;
}

bool ConfigWindow::createSettingsWindow() {
  // Init window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return false;

  // Hints
  const char* glsl_version = "#version 460";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

  // Create window with graphics context
  float main_scale =
      ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
  GLFWwindow* window = glfwCreateWindow(
      (int)(GUI_WIDTH * main_scale), (int)(GUI_HEIGHT * main_scale),
      "OpenGL 4.6 Tech Demo - Graphics Settings", nullptr, nullptr);

  if (window == nullptr) return false;

  int workX, workY, workWidth, workHeight;
  glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &workX, &workY, &workWidth,
                         &workHeight);

  glfwSetWindowPos(window, workWidth / 2 - GUI_WIDTH / 2,
                   workHeight / 2 - GUI_HEIGHT / 2);

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  //Our image for the config window
  loadConfigWindowImage();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  ImVec2 guiSize = {GUI_WIDTH, GUI_HEIGHT};
  ImVec2 windowCenter = {GUI_WIDTH / 2, GUI_HEIGHT / 2};

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  io.Fonts->AddFontFromFileTTF("./imgui/Inter_18pt-Black.ttf");

  // Loop until settings are confirmed
  while (!glfwWindowShouldClose(window)) {
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
    ImGui::SetNextWindowSize(guiSize);
    ImGui::SetNextWindowPos(windowCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    //Dear ImGUI window
    ImGui::Begin("OpenGL 4.6 Tech Demo - Graphics Settings", NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);  

    //start of display settings, image, and advanced options group
    ImGui::BeginGroup(); 
    
    //start of display settings group
    ImGui::PushItemWidth(GUI_WIDTH / 2 * ELEMENT_SIZE);
    ImGui::BeginGroup(); 
    
    //Resolution
    ImGui::Text("Resolution");
    if (ImGui::BeginCombo("##resolutionCombo", resolution_preview_value,
                          ImGuiComboFlags_HeightLargest)) {
      for (int i = 0; i < IM_ARRAYSIZE(resolutions); ++i) {
        const bool is_res_selected = (resolution_selected_idx == i);
        if (ImGui::Selectable(resolutions[i], is_res_selected)) {
          resolution_selected_idx = i;
          resolution_preview_value = resolutions[resolution_selected_idx];
        }
        if (is_res_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::Checkbox("Fullscreen", &graphicsSettings.fullscreen);
    ImGui::Checkbox("VSync", &graphicsSettings.vsync);
    
    //Gamma
    ImGui::Text("Gamma");
    ImGui::SliderFloat("##gammaSlider", &graphicsSettings.gamma, 0.5f, 3.0f, "%.3f",
                       ImGuiSliderFlags_ClampOnInput);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("(Ctrl+Click to edit)");
    
    //end of display settings
    ImGui::EndGroup(); 

    ImGui::SameLine();

    //start of image and advanced settings
    ImGui::BeginGroup();

    //Image
    ImGui::Image((ImTextureID)(intptr_t)textureID,
                 ImVec2(GUI_WIDTH / 2,
                        GUI_WIDTH / 2));

    //start of advanced options group
    ImGui::BeginGroup();
    ImGui::SeparatorText("Advanced Options");

    //MSAA
    ImGui::Text("MSAA");
    if (ImGui::BeginCombo("##msaaCombo", msaa_preview_value,
                          ImGuiComboFlags_HeightLargest)) {
      for (int i = 0; i < IM_ARRAYSIZE(msaaLevel); ++i) {
        const bool is_msaa_selected = (msaa_selected_idx == i);
        if (ImGui::Selectable(msaaLevel[i], is_msaa_selected)) {
          msaa_selected_idx = i;
          msaa_preview_value = msaaLevel[msaa_selected_idx];
        }
        if (is_msaa_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    //Anisotropic filtering
    ImGui::Text("Anisotropic Filtering");
    if (ImGui::BeginCombo("##anisoCombo", aniso_preview_value,
                          ImGuiComboFlags_HeightLargest)) {
      for (int i = 0; i < IM_ARRAYSIZE(anisoLevel); ++i) {
        const bool is_aniso_selected = (aniso_selected_idx == i);
        if (ImGui::Selectable(anisoLevel[i], is_aniso_selected)) {
          aniso_selected_idx = i;
          aniso_preview_value = anisoLevel[aniso_selected_idx];
        }
        if (is_aniso_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    //end of advanced settigns
    ImGui::EndGroup();

    //end of image and advanced settings
    ImGui::EndGroup();

    //end of display settings, image, and advanced options group
    ImGui::EndGroup();
    ImGui::PopItemWidth();

    ImGui::SetCursorPos(
        ImVec2(GUI_WIDTH * (1.0 - ELEMENT_SIZE), GUI_HEIGHT * ELEMENT_SIZE));

    if (ImGui::Button("Cancel")) {
      ImGui_ImplOpenGL3_Shutdown();
      ImGui_ImplGlfw_Shutdown();
      ImGui::DestroyContext();

      glfwDestroyWindow(window);
      glfwTerminate();
      return false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Confirm")) glfwSetWindowShouldClose(window, true);

    //End
    ImGui::End();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                 clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  if (textureID != 0) glDeleteTextures(1, &textureID);

  return true;
}

const GraphicsSettings& ConfigWindow::getGraphicsSettings() const {
  return graphicsSettings;
}

void ConfigWindow::loadConfigWindowImage() {
  std::vector<std::filesystem::path> sphereImage = {
      "./res/wireframe_sphere.png"};
      ImageLoader imageLoader(sphereImage);
  std::vector<ImageData> imageData = imageLoader.getImageData();

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
      return;
  }

  textureID = texture_utilities::generateTextureID(imageData[0], false);
}