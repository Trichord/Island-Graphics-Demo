// Uniform Buffer Objects. Guaranteed 12 minimum per stage

layout (std140, binding = 0) uniform cameraData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

layout(binding = 1, std140) uniform frustumPlanes
{
    vec4 planes[6];
};

layout (std140, binding = 2) uniform cascadeShadow
{
	vec4 near;
    vec4 far;
    mat4 lightMatrices[3];
};

layout (std140, binding = 3) uniform packedTime
{
	layout (offset = 0) float deltaTime;
	layout (offset = 4) float time;
};