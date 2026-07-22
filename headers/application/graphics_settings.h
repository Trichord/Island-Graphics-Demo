#pragma once

#include <nlohmann/json.hpp>

struct GraphicsSettings
{
    int renderWidth{1920};
    int renderHeight{1080};
    int msaa{4};
    int anisotropicFiltering{4};
    int shadowResolution{1024};
    bool fullscreen{true};
    bool vsync{true};
    float gamma{1.0f};
    int reflectionResolution{1024};
};

inline void to_json(nlohmann::json &j, const GraphicsSettings &s)
{
    using json = nlohmann::json;

    j = json{{"renderWidth", s.renderWidth},
             {"renderHeight", s.renderHeight},
             {"msaa", s.msaa},
             {"aniso", s.anisotropicFiltering},
             {"shadowResolution", s.shadowResolution},
             {"fullscreen", s.fullscreen},
             {"vsync", s.vsync},
             {"gamma", s.gamma},
             {"reflectionResolution", s.reflectionResolution}};
    }

inline void from_json(const nlohmann::json &j, GraphicsSettings &s)
{
    s.renderWidth = j.value("renderWidth", 1920);
    s.renderHeight = j.value("renderHeight", 1080);
    s.msaa = j.value("msaa", 4);
    s.anisotropicFiltering = j.value("aniso", 4);
    s.shadowResolution = j.value("shadowResolution", 1024);
    s.fullscreen = j.value("fullscreen", true);
    s.vsync = j.value("vsync", true);
    s.gamma = j.value("gamma", 1.0f);
    s.reflectionResolution = j.value("reflectionResolution", 1024);
}