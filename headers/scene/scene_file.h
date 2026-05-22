#pragma once

#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "transform.h"
#include "json/json.hpp"

NLOHMANN_JSON_SERIALIZE_ENUM(ShaderType, {{ShaderType::directionalPBR, "directionalPBR"},
                                          {ShaderType::reflective, "reflective"},
                                          {ShaderType::trees, "trees"},
                                          {ShaderType::grass, "grass"},
                                          {ShaderType::water, "water"},
                                          {ShaderType::ocean, "ocean"},
                                          {ShaderType::skybox, "skybox"}})

struct ModelEntry
{
    std::string name;
    Transform transform;
    std::filesystem::path modelPath;
    ShaderType shader{ShaderType::directionalPBR};
};

struct InstancedModelEntry
{
    std::string name;
    float minScale{0.0f};
    float maxScale{0.0f};
    std::filesystem::path modelPath;
    std::filesystem::path positionsPath;
    ShaderType shader{ShaderType::directionalPBR};
};

struct CubeMappedModelEntry
{
    std::string name;
    Transform transform;
    std::filesystem::path modelPath;
    std::filesystem::path cubemapPath;
    ShaderType shader{ShaderType::directionalPBR};
};

struct ParticlesEntry
{
    glm::vec3 position{0.0f};
    glm::vec3 extent{0.0f};
    glm::vec4 color{0.0f};
    glm::vec4 colorVariation{0.0f};
    int count{0};
};

struct SkyboxEntry
{
    std::filesystem::path skyboxPath;
};

struct SceneWind
{
    float strength{0.0f};
    glm::vec3 direction{0.0f};
};

struct SceneEnvironment
{
    glm::vec3 lightColor{0.0f};
    glm::vec3 ambientLight{0.0f};
    glm::vec3 sunDirection{0.0f};
};

struct SceneCamera
{
    glm::vec3 position{0.0f};
    float pitch{0.0f};
    float yaw{0.0f};
    float nearPlane{0.1f};
    float farPlane{1000.0f};
    float FOV{45.0f};
};

using SceneEntry = std::variant<ModelEntry, InstancedModelEntry, CubeMappedModelEntry, SkyboxEntry, ParticlesEntry>;

struct SceneDescription
{
    SceneEnvironment environment;
    SceneWind wind;
    SceneCamera camera;
    std::vector<SceneEntry> entries;
};

inline glm::vec3 parseVec3(const nlohmann::json &j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

inline glm::vec4 parseVec4(const nlohmann::json &j)
{
    return {j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
}

inline void from_json(const nlohmann::json &j, Transform &t)
{
    t = Transform{
        j.contains("position") ? parseVec3(j.at("position")) : glm::vec3{0.0f},
        j.contains("scale") ? parseVec3(j.at("scale")) : glm::vec3{1.0f},
        j.value("rotationDegrees", 0.0f),
        j.contains("rotationAxis") ? parseVec3(j.at("rotationAxis")) : glm::vec3{0.0, 1.0, 0.0},
    };
}

inline void from_json(const nlohmann::json &j, ModelEntry &model)
{
    model.name = j.value("name", "");
    model.transform = j.value("transform", Transform{});
    model.modelPath = j.value("modelPath", std::filesystem::path{});
    model.shader = j.value("shader", ShaderType::directionalPBR);
}

inline void from_json(const nlohmann::json &j, InstancedModelEntry &model)
{
    model.name = j.value("name", "");
    model.minScale = j.value("minScale", 1.0f);
    model.maxScale = j.value("maxScale", 1.0f);
    model.modelPath = j.value("modelPath", std::filesystem::path{});
    model.positionsPath = j.value("positionsPath", std::filesystem::path{});
    model.shader = j.value("shader", ShaderType::directionalPBR);
}

inline void from_json(const nlohmann::json &j, CubeMappedModelEntry &model)
{
    model.name = j.value("name", "");
    model.transform = j.value("transform", Transform{});
    model.modelPath = j.value("modelPath", std::filesystem::path{});
    model.cubemapPath = j.value("cubemapPath", std::filesystem::path{});
    model.shader = j.value("shader", ShaderType::directionalPBR);
}

inline void from_json(const nlohmann::json &j, ParticlesEntry &particle)
{
    particle.position = j.contains("position") ? parseVec3(j.at("position")) : glm::vec3{0.0f};
    particle.extent = j.contains("extent") ? parseVec3(j.at("extent")) : glm::vec3{0.0f};
    particle.color = j.contains("color") ? parseVec4(j.at("color")) : glm::vec4{1.0f};
    particle.colorVariation = j.contains("colorVariation") ? parseVec4(j.at("colorVariation")) : glm::vec4{0.0f};
    particle.count = j.value("count", 0);
}

inline void from_json(const nlohmann::json &j, SkyboxEntry &skybox)
{
    skybox.skyboxPath = j.value("skyboxPath", std::filesystem::path{});
}

inline void from_json(const nlohmann::json &j, SceneWind &wind)
{
    wind.strength = j.value("strength", 0.0f);
    wind.direction = j.contains("direction") ? parseVec3(j.at("direction")) : glm::vec3{0.0f};
}

inline void from_json(const nlohmann::json &j, SceneEnvironment &env)
{
    env.lightColor = j.contains("lightColor") ? parseVec3(j.at("lightColor")) : glm::vec3{1.0f};
    env.ambientLight = j.contains("ambientLight") ? parseVec3(j.at("ambientLight")) : glm::vec3{0.2f};
    env.sunDirection = j.contains("sunDirection") ? parseVec3(j.at("sunDirection")) : glm::vec3{0.0, 1.0, 0.0};
}

inline void from_json(const nlohmann::json &j, SceneCamera &c)
{
    c.position = j.contains("position") ? parseVec3(j.at("position")) : glm::vec3{0.0f};
    c.pitch = j.value("pitch", 0.0f);
    c.yaw = j.value("yaw", 0.0f);
    c.nearPlane = j.value("nearPlane", 0.1f);
    c.farPlane = j.value("farPlane", 1000.0f);
    c.FOV = j.value("FOV", 45.0f);
}

inline SceneEntry parseSceneEntry(const nlohmann::json &j)
{
    const std::string type = j.at("type").get<std::string>();

    if (type == "model")
        return j.get<ModelEntry>();
    if (type == "cubemapped")
        return j.get<CubeMappedModelEntry>();
    if (type == "instanced")
        return j.get<InstancedModelEntry>();
    if (type == "skybox")
        return j.get<SkyboxEntry>();
    if (type == "particles")
        return j.get<ParticlesEntry>();

    std::cerr << "ERROR: could not load model type " << type << std::endl;
    throw nlohmann::json::type_error::create(302, "Unknown model type: " + type, &j);
}

inline void from_json(const nlohmann::json &j, SceneDescription &s)
{
    s.environment = j.value("environment", SceneEnvironment{});
    s.wind = j.value("wind", SceneWind{});
    s.camera = j.value("camera", SceneCamera{});

    if (j.contains("entries"))
    {
        for (const auto &entry : j.at("entries"))
        {
            s.entries.emplace_back(parseSceneEntry(entry));
        }
    }
}