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

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS headers/*.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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
