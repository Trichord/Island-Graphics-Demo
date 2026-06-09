// Samplers
layout(binding = 0) uniform sampler2DArray textures[11];

layout(binding = 11) uniform samplerCubeArray cubemaps[3];

layout(binding = 14) uniform sampler2DArrayShadow shadowMap;

layout(binding = 15) uniform sampler2D frameBuffer;

layout(binding = 16) uniform sampler2D depthMap;