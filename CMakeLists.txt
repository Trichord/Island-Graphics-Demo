cmake_minimum_required(VERSION 3.25)
project(island_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Dependencies

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)

# Source files

set(SOURCES
    src/main.cpp
    src/application/application.cpp
    src/application/glfw_callbacks.cpp
    src/application/settings_window.cpp
    src/models/aabb.cpp
    src/models/instanced_model.cpp
    src/models/model.cpp
    src/models/particles.cpp
    src/models/skybox.cpp
    src/physics/physics_utilities.cpp
    src/physics/player.cpp
    src/physics/player_physics.cpp
    src/renderer/cascade_shadow.cpp
    src/renderer/gl_state.cpp
    src/renderer/render_target.cpp
    src/renderer/renderer.cpp
    src/renderer/renderer_draw.cpp
    src/renderer/renderer_setup.cpp
    src/renderer/shader/compute_shader.cpp
    src/renderer/shader/graphics_shader.cpp
    src/renderer/shader/shader_base.cpp
    src/scene/camera.cpp
    src/scene/directional_light.cpp
    src/scene/island_scene/island_scene.cpp
    src/scene/light.cpp
    src/scene/point_light.cpp
    src/scene/scene.cpp
    src/utilities/array_texture.cpp
    src/utilities/asset_loader.cpp
    src/utilities/buffer_builder.cpp
    src/utilities/json_loader.cpp
    src/utilities/ktx_texture.cpp
    src/utilities/scene_processor.cpp
    src/utilities/texture_loader.cpp
    src/utilities/texture_manager.cpp
    src/utilities/texture_utilities.cpp
    src/utilities/thread_pool.cpp
    src/utilities/transform.cpp
)

set(HEADERS
    headers/application/application.h
    headers/application/gl_context.h
    headers/application/glfw_callbacks.h
    headers/application/graphics_settings.h
    headers/application/settings_window.h
    headers/models/aabb.h
    headers/models/instanced_model.h
    headers/models/model.h
    headers/models/particles.h
    headers/models/primitive.h
    headers/models/skybox.h
    headers/physics/jolt_impl.h
    headers/physics/physics_utilities.h
    headers/physics/player.h
    headers/physics/player_physics.h
    headers/renderer/buffers.h
    headers/renderer/cascade_shadow.h
    headers/renderer/gl_state.h
    headers/renderer/render_target.h
    headers/renderer/renderer.h
    headers/renderer/renderer_types.h
    headers/renderer/shader/compute_shader.h
    headers/renderer/shader/graphics_shader.h
    headers/renderer/shader/shader_base.h
    headers/renderer/textures.h
    headers/scene/camera.h
    headers/scene/directional_light.h
    headers/scene/handle.h
    headers/scene/island_scene/island_scene.h
    headers/scene/light.h
    headers/scene/point_light.h
    headers/scene/scene.h
    headers/scene/scene_file.h
    headers/utilities/array_texture.h
    headers/utilities/asset_loader.h
    headers/utilities/buffer_builder.h
    headers/utilities/json_loader.h
    headers/utilities/ktx_texture.h
    headers/utilities/scene_processor.h
    
    headers/utilities/texture_loader.h
    headers/utilities/texture_manager.h
    headers/utilities/texture_utilities.h
    headers/utilities/thread_pool.h
    headers/utilities/transform.h
    headers/utilities/vkformat_enum.h
    headers/utilities/world_vectors.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} "headers/models/skeleton.h"  "headers/models/animation.h" "headers/models/animator.h" "src/models/animator.cpp")

# Include directories

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/headers
)

# Link libraries

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glad::glad
    glm::glm
    imgui::imgui
    fastgltf::fastgltf
    nlohmann_json::nlohmann_json
    Jolt::Jolt
    KTX::ktx
    opengl32
    gdi32
)

# Compiler options

target_compile_options(${PROJECT_NAME} PRIVATE
    /permissive-
    $<$<CONFIG:Release>:/GL /Ot /Gy /GR->
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/LTCG>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    JPH_USE_AVX2
    JPH_USE_AVX
    JPH_USE_SSE4_1
    JPH_USE_SSE4_2
    JPH_USE_F16C
    JPH_USE_FMADD
    JPH_USE_LZCNT
    JPH_USE_TZCNT
)

# Copy resources

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)

# Clean install

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(DIRECTORY res/ DESTINATION res)
install(DIRECTORY shaders/ DESTINATION shaders)
