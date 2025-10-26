#pragma once

// Order is important.  When a scene is passed to the renderer, these enums will
// be rendered in order (skybox is usually last)

enum class ShaderType {
  GRASS,
  DIRECTIONAL_PBR,
  POINT_PBR,
  REFLECTIVE,
  LIGHT_SOURCE,
  SKYBOX
};