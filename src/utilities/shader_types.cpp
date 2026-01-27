#include "shader_types.h"

CullStrategy getCullStrategy(ShaderType shader) {
  using enum ShaderType;
  using enum CullStrategy;

  switch (shader) {
    case DIRECTIONAL_PBR:
    case POINT_PBR:
    case REFLECTIVE:
    case LIGHT_SOURCE:
      return MODEL;

    case FLOWERS:
      return INSTANCED_DISTANCE;

    case TREES:
      return INSTANCED;

    case SKYBOX:
      return NONE;

    default:
      return MODEL;
  }
}