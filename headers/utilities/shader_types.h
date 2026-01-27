#pragma once

/* Order is important.When a scene is passed to the renderer, these enums will
 * be rendered in order (skybox is usually last) as an optimization.
 */

enum class ShaderType {
  FLOWERS,
  TREES,
  DIRECTIONAL_PBR,
  POINT_PBR,
  REFLECTIVE,
  LIGHT_SOURCE,
  SKYBOX
};

enum class CullStrategy { NONE, MODEL, INSTANCED, INSTANCED_DISTANCE, BY_MESH };

CullStrategy getCullStrategy(ShaderType shader);