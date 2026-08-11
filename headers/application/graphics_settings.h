#pragma once

#include <nlohmann/json.hpp>

namespace
{
constexpr int minRes{1};
constexpr int minMSAA{2};
constexpr int minAniso{0};
constexpr int minShadowRes{256};
constexpr float minGamma{0.01};
constexpr int minReflectionRes{64};

constexpr int defaultRenderWidth{1920};
constexpr int defaultRenderHeight{1080};
constexpr int defaultMSAA{4};
constexpr int defaultAnisotropicFiltering{4};
constexpr int defaultShadowResolution{1024};
constexpr bool defaultFullscreen{true};
constexpr bool defaultVsync{true};
constexpr float defaultGamma{1.0f};
constexpr int defaultReflectionResolution{1024};
} // namespace

struct GraphicsSettings
{
    int renderWidth{defaultRenderWidth};
    int renderHeight{defaultRenderHeight};
    int msaa{defaultMSAA};
    int anisotropicFiltering{defaultAnisotropicFiltering};
    int shadowResolution{defaultShadowResolution};
    bool fullscreen{defaultFullscreen};
    bool vsync{defaultVsync};
    float gamma{defaultGamma};
    int reflectionResolution{defaultReflectionResolution};
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

    s.renderWidth = std::max(j.value("renderWidth", defaultRenderWidth), minRes);
    s.renderHeight = std::max(j.value("renderHeight", defaultRenderHeight), minRes);
    s.msaa = std::max(j.value("msaa", defaultMSAA), minMSAA);
    s.anisotropicFiltering = std::max(j.value("aniso", defaultAnisotropicFiltering), minAniso);
    s.shadowResolution = j.value("shadowResolution", defaultShadowResolution);
    s.fullscreen = j.value("fullscreen", defaultFullscreen);
    s.vsync = j.value("vsync", defaultVsync);
    s.gamma = std::max(j.value("gamma", defaultGamma), minGamma);
    s.reflectionResolution = std::max(j.value("reflectionResolution", defaultReflectionResolution), minReflectionRes);

    // Verify minimum viable shadow map resolution
    if (s.shadowResolution < minShadowRes)
        s.shadowResolution = 0;
}