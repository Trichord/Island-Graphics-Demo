#pragma once

/* Order is important.When a scene is passed to the renderer, these enums will
 * be rendered in order (skybox is usually last) as an optimization.
 */

enum class ShaderType {
  FLOWERS,
  DIRECTIONAL_PBR,
  POINT_PBR,
  REFLECTIVE,
  LIGHT_SOURCE,
  SKYBOX
};