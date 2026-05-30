// Functions for wind effects

#include "random.glsl"

vec3 calcWindTranslation(mat4 transform, float timeStamp, vec3 windDir, float windStr)
{
	const vec4 translation = transform[3];
	float time1 = sin(timeStamp + random(vec2(translation.x, translation.z)));

	// Remap waves from [-1, 1] to [-0.25, 0.75] 
	float wave = (time1 * 0.5 + 0.25);

	return windDir * wave * windStr;
}