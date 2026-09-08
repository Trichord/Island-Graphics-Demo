// Uniform Buffer Objects. Guaranteed 12 minimum per stage

#include "shared/binding_points.glsl"

layout (std140, binding = UBO_BINDING_CAMERA) uniform cameraData 
{
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 inverseViewProjectMatrix;
	vec4 cameraPosition;
	float fov;
};

layout(binding = UBO_BINDING_FRUSTUM_PLANES, std140) uniform frustumPlanes
{
    vec4 planes[6];
};

layout (std140, binding = UBO_BINDING_CASCADE_SHADOW) uniform cascadeShadow
{
	vec4 near;
    vec4 far;
	vec4 texelSizes;
    mat4 lightMatrices[3];
};

layout (std140, binding = UBO_BINDING_TIME) uniform packedTime
{
	layout (offset = 0) float deltaTime;
	layout (offset = 4) float time;
};

layout (std140, binding = UBO_BINDING_SCENE) uniform scene
{
	vec4 lightDir;
	vec4 lightColor;
	vec4 ambientLight;

	vec4 windDirection;
    float windStrength;
};

layout (std140, binding = UBO_BINDING_WINDOW) uniform window
{
    int windowWidth;
    int windowHeight;
    float gamma;
};