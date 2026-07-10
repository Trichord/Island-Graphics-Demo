#include "shared/binding_points.glsl"

// Images
layout(binding = IMAGE_BINDING_HI_Z_READ, r32f) uniform readonly image2D hiZRead;

layout(binding = IMAGE_BINDING_HI_Z_WRITE, r32f) uniform writeonly image2D hiZWrite;