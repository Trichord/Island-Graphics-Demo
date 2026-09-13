#pragma once

#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "models/primitive.h"
#include "physics/collision_shapes.h"
#include "renderer/shader/graphics_shader.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

NLOHMANN_JSON_SERIALIZE_ENUM(ShaderType, {{ShaderType::baseColor, "baseColor"},
                                          {ShaderType::pbr, "pbr"},
                                          {ShaderType::reflectiveCubemap, "reflectiveCubemap"},
                                          {ShaderType::trees, "trees"},
                                          {ShaderType::vegetation, "vegetation"},
                                          {ShaderType::water, "water"},
                                          {ShaderType::ocean, "ocean"},
                                          {ShaderType::particles, "particles"},
                                          {ShaderType::skybox, "skybox"}})

struct InstancedEntry
{
    float minScale{1.0f};
    float maxScale{1.0f};
    std::filesystem::path positionsPath;
};

struct ModelEntry
{
    std::string name;
    std::filesystem::path modelPath;
    ShaderType shader{ShaderType::baseColor};
    std::variant<Transform, std::vector<Transform>> position;

    std::optional<InstancedEntry> instancedEntry{std::nullopt};
    std::optional<collision::Geometry> collisionGeometry{std::nullopt};
    std::optional<std::filesystem::path> cubemapPath{std::nullopt};
    std::optional<Primitive::VertexExplosion> vertexExplosion{std::nullopt};
};

struct ParticlesEntry
{
    glm::vec3 translation{0.0f};
    glm::vec3 extent{0.0f};
    glm::vec4 color{0.0f};
    glm::vec4 colorVariation{0.0f};
    float lifetime{0.0f};
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
    float fov{45.0f};
};

using FileEntry = std::variant<ModelEntry, SkyboxEntry, ParticlesEntry>;

struct SceneFile
{
    SceneEnvironment environment;
    SceneWind wind;
    SceneCamera camera;
    std::vector<FileEntry> entries;
};

// Helpers for basic types
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
    glm::vec3 translation{j.contains("translation") ? parseVec3(j.at("translation")) : glm::vec3{0.0f}};
    glm::vec3 scale{j.contains("scale") ? glm::vec3{j.at("scale").get<float>()} : glm::vec3{1.0f}};

    t = {translation, scale};

    if (j.contains("rotation"))
    {
        auto &rot = j.at("rotation");

        if (rot.is_number())
            t.setAbsoluteRotation(glm::angleAxis(glm::radians(rot.get<float>()), WorldVectors::worldUp));
        else if (rot.size() == 4 && rot.is_array())
        {
            const glm::vec4 vec{parseVec4(rot)};
            const glm::quat quat{vec.w, vec.x, vec.y, vec.z};

            if (glm::length(quat) < 0.001f)
                throw nlohmann::json::type_error::create(302, "rotation quaternion is degenerate", &rot);

            t.setAbsoluteRotation(glm::normalize(quat));
        }
        else
            throw nlohmann::json::type_error::create(302, "rotation must be a single yaw or quat", &rot);
    }
}

namespace collision
{
inline void from_json(const nlohmann::json &j, MaterialProperties &m)
{
    constexpr float lowest{0.0001f};

    MaterialProperties defaultMaterial{};
    const float density{j.value("density", defaultMaterial.density)};

    m.friction = std::clamp(j.value("friction", defaultMaterial.friction), lowest, 1.0f);
    m.density = std::max(density, lowest);
    m.buoyancy = std::clamp(j.value("buoyancy", defaultMaterial.buoyancy), 0.0f, 3.0f);
}

inline void from_json(const nlohmann::json &j, Geometry &geometry)
{
    constexpr float lowest{0.0001f};

    const auto &type = j.at("type").get<std::string>();

    if (type == "mesh")
        geometry = collision::Mesh{
            .path = j.value("path", std::filesystem::path{}),
            .material = j.value("material", MaterialProperties{}),
        };
    else if (type == "convexHull")
        geometry = collision::ConvexHull{
            .path = j.value("path", std::filesystem::path{}),
            .material = j.value("material", MaterialProperties{}),
            .compound = j.value("compound", false),
            .dynamic = j.value("dynamic", false),
        };
    else if (type == "capsule")
        geometry = collision::Capsule{
            .radius = j.value("radius", 0.0f),
            .height = j.value("height", 0.0f),
            .offset = j.contains("offset") ? parseVec3(j.at("offset")) : glm::vec3{0.0f},
            .material = j.value("material", MaterialProperties{}),
            .dynamic = j.value("dynamic", false),
        };
    else if (type == "sphere")
    {
        const float radius{j.value("radius", lowest)};

        geometry = collision::Sphere{
            .radius = std::max(radius, lowest),
            .offset = j.contains("offset") ? parseVec3(j.at("offset")) : glm::vec3{0.0f},
            .bounciness = std::clamp(j.value("bounciness", 0.0f), 0.0f, 1.0f),
            .material = j.value("material", MaterialProperties{}),
            .dynamic = j.value("dynamic", false),
        };
    }
    else
        throw nlohmann::json::type_error::create(302, "Unknown collision type: " + type, &j);
}
} // namespace collision

inline void from_json(const nlohmann::json &j, Primitive::VertexExplosion &v)
{
    v.minDistance = j.value("minDistance", 0.0f);
    v.maxDistance = j.value("maxDistance", 0.0f);
}

inline void from_json(const nlohmann::json &j, InstancedEntry &i)
{
    i.minScale = j.value("minScale", 1.0f);
    i.maxScale = j.value("maxScale", 1.0f);
    i.positionsPath = j.value("positionsPath", std::filesystem::path{});
}

inline void from_json(const nlohmann::json &j, ModelEntry &model)
{
    model.name = j.value("name", std::string{});
    model.modelPath = j.value("modelPath", std::filesystem::path{});
    model.shader = j.value("shader", ShaderType::pbr);

    if (j.contains("instancing"))
    {
        model.instancedEntry = j.at("instancing");
        model.position = std::vector<Transform>{};
    }
    else
        model.position = j.value("transform", Transform{});

    if (j.contains("collisionGeometry"))
        model.collisionGeometry = j.at("collisionGeometry").get<collision::Geometry>();

    if (j.contains("cubemapPath"))
        model.cubemapPath = j.at("cubemapPath");

    if (j.contains("vertexExplosion"))
        model.vertexExplosion = j.at("vertexExplosion");
}

inline void from_json(const nlohmann::json &j, ParticlesEntry &particle)
{
    particle.translation = j.contains("translation") ? parseVec3(j.at("translation")) : glm::vec3{0.0f};
    particle.extent = j.contains("extent") ? parseVec3(j.at("extent")) : glm::vec3{0.0f};
    particle.color = j.contains("color") ? parseVec4(j.at("color")) : glm::vec4{1.0f};
    particle.colorVariation = j.contains("colorVariation") ? parseVec4(j.at("colorVariation")) : glm::vec4{0.0f};
    particle.lifetime = j.value("lifetime", 0.0f);
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
    c.yaw = j.value("yaw", -90.0f);
    c.nearPlane = j.value("nearPlane", 0.1f);
    c.farPlane = j.value("farPlane", 1000.0f);
    c.fov = j.value("fov", 45.0f);
}

inline FileEntry parseFileEntries(const nlohmann::json &j)
{
    const std::string type = j.at("type").get<std::string>();

    if (type == "model")
        return j.get<ModelEntry>();
    if (type == "skybox")
        return j.get<SkyboxEntry>();
    if (type == "particles")
        return j.get<ParticlesEntry>();

    std::cerr << "ERROR: could not load model type " << type << std::endl;
    throw nlohmann::json::type_error::create(302, "Unknown model type: " + type, &j);
}

inline void from_json(const nlohmann::json &j, SceneFile &s)
{
    s.environment = j.value("environment", SceneEnvironment{});
    s.wind = j.value("wind", SceneWind{});
    s.camera = j.value("camera", SceneCamera{});

    if (j.contains("entries"))
    {
        for (const auto &entry : j.at("entries"))
        {
            s.entries.emplace_back(parseFileEntries(entry));
        }
    }
}