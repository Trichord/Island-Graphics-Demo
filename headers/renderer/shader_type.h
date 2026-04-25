#pragma once

/* Order is important.When a scene is passed to the renderer, these enums will
 * be rendered in order (skybox is usually last) as an optimization.
 */

enum class ShaderType
{
    directionalPBR,
    reflective,
    trees,
    grass,
    water,
    ocean,
    skybox,
    shadowMap,
    shadowMapAlphaTested,
    count
};