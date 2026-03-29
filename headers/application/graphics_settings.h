#pragma once

#include <string>
#include <unordered_set>

#include "json/json.hpp"

struct GraphicsSettings
{
    int screenWidth{2560};
    int screenHeight{1440};
    int msaa{8};
    int anisotropicFiltering{16};
    int shadowResolution{1024};
    bool fullscreen{true};
    bool vsync{false};
    float gamma{1.0f};
};

inline void to_json(nlohmann::json &j, const GraphicsSettings &s)
{
    using json = nlohmann::json;

    j = json{{"screenWidth", s.screenWidth},
             {"screenHeight", s.screenHeight},
             {"msaa", s.msaa},
             {"aniso", s.anisotropicFiltering},
             {"shadowResolution", s.shadowResolution},
             {"fullscreen", s.fullscreen},
             {"vsync", s.vsync},
             {"gamma", s.gamma}};
}

inline void from_json(const nlohmann::json &j, GraphicsSettings &s)
{
    s.screenWidth = j.value("screenWidth", 2560);
    s.screenHeight = j.value("screenHeight", 1440);
    s.msaa = j.value("msaa", 8);
    s.anisotropicFiltering = j.value("aniso", 16);
    s.shadowResolution = j.value("shadowResolution", 1024);
    s.fullscreen = j.value("fullscreen", true);
    s.vsync = j.value("vsync", false);
    s.gamma = j.value("gamma", 1.0f);
}