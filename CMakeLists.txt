cmake_minimum_required(VERSION 3.26)
project(island_demo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/$<CONFIG>)

# Dependencies

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)

# Source files

set(HEADERS
    headers/application/application.h
    headers/application/gl_context.h
    headers/application/gl_debug.h
    headers/application/glfw_callbacks.h
    headers/application/graphics_settings.h
    headers/application/input.h
    headers/application/settings_window.h
    headers/audio/audio_engine.h
    headers/audio/audio_file.h
    headers/audio/audio_player.h
    headers/audio/sound_effect.h
    headers/game/camera.h
    headers/game/game.h
    headers/models/aabb.h
    headers/models/instanced_model.h
    headers/models/model.h
    headers/models/particles.h
    headers/models/primitive.h
    headers/models/skybox.h
    headers/models/animation/animation.h
    headers/models/animation/animator.h
    headers/models/animation/node_graph.h
    headers/models/animation/skeleton.h
    headers/physics/character_capsule.h
    headers/physics/collision_shapes.h
    headers/physics/jolt_impl.h
    headers/physics/moved.h
    headers/physics/physics_utilities.h
    headers/physics/physics_world.h
    headers/physics/player_state.h
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
    headers/renderer/texture_view.h
    headers/scene/directional_light.h
    headers/scene/entry_animation.h
    headers/scene/handle.h
    headers/scene/scene.h
    headers/scene/scene_file.h
    headers/scene/island_scene/island_scene.h
    headers/scene/island_scene/seagull.h
    headers/utilities/array_texture.h
    headers/utilities/asset_loader.h
    headers/utilities/buffer_builder.h
    headers/utilities/json_loader.h
    headers/utilities/json_parsing.h
    headers/utilities/ktx_texture.h
    headers/utilities/overloaded.h
    headers/utilities/random.h
    headers/utilities/scene_processor.h
    headers/utilities/string_hash.h
    headers/utilities/texture_loader.h
    headers/utilities/texture_manager.h
    headers/utilities/texture_utilities.h
    headers/utilities/thread_pool.h
    headers/utilities/transform.h
    headers/utilities/vkformat_enum.h
    headers/utilities/world_vectors.h
)

set(SOURCES
    src/main.cpp
    src/application/application.cpp
    src/application/settings_window.cpp
    src/audio/audio_player.cpp
    src/audio/miniaudio_impl.cpp
    src/audio/sound_effect.cpp
    src/game/camera.cpp
    src/game/game.cpp
    src/models/aabb.cpp
    src/models/animator.cpp
    src/models/instanced_model.cpp
    src/models/model.cpp
    src/models/particles.cpp
    src/models/skybox.cpp
    src/physics/character_capsule.cpp
    src/physics/physics_utilities.cpp
    src/physics/physics_world.cpp
    src/renderer/cascade_shadow.cpp
    src/renderer/gl_state.cpp
    src/renderer/render_target.cpp
    src/renderer/renderer.cpp
    src/renderer/renderer_draw.cpp
    src/renderer/renderer_setup.cpp
    src/renderer/texture_view.cpp
    src/renderer/shader/compute_shader.cpp
    src/renderer/shader/graphics_shader.cpp
    src/renderer/shader/shader_base.cpp
    src/scene/entry_animation.cpp
    src/scene/directional_light.cpp
    src/scene/scene.cpp
    src/scene/island_scene/island_scene.cpp
    src/scene/island_scene/seagull.cpp
    src/utilities/array_texture.cpp
    src/utilities/asset_loader.cpp
    src/utilities/buffer_builder.cpp
    src/utilities/json_loader.cpp
    src/utilities/ktx_texture.cpp
    src/utilities/random.cpp
    src/utilities/scene_processor.cpp
    src/utilities/texture_loader.cpp
    src/utilities/texture_manager.cpp
    src/utilities/texture_utilities.cpp
    src/utilities/thread_pool.cpp
    src/utilities/transform.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/headers
    ${MINIAUDIO_INCLUDE_DIR}
)

# Link libraries

target_link_libraries(${PROJECT_NAME} PRIVATE
    fastgltf::fastgltf
    glfw
    glad::glad
    glm::glm
    imgui::imgui
    Jolt::Jolt
    KTX::ktx
    nlohmann_json::nlohmann_json
    opengl32
    gdi32
)

# Compiler options

target_compile_options(${PROJECT_NAME} PRIVATE
    /permissive-
    /GR-
    /we4541
    $<$<CONFIG:Release>:/GL>
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/LTCG>
)
# Speed up MSBuild
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    target_compile_options(${PROJECT_NAME} PRIVATE /MP)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
)

# Mirror res/ and shaders/ next to the exe on every build (only changed files are copied)
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMENT "Syncing res/ and shaders/ to output directory"
)
add_dependencies(${PROJECT_NAME} copy_assets)
