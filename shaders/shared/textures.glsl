// Samplers
layout(binding = 0) uniform sampler2DArray textures[12];

layout(binding = 12) uniform samplerCubeArray cubemaps[3];

layout(binding = 15) uniform sampler2DArrayShadow shadowMap;

layout(binding = 16) uniform sampler2D frameBuffer;

layout(binding = 17) uniform sampler2D depthMap;