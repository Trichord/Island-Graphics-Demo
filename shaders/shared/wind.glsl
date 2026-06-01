// Functions for wind effects

const float windWaveLength = 5.5;
const float windTimeScale = 0.025;

float getWindFactor(vec3 worldPos, float time)
{
	float wind = (worldPos.x + worldPos.y + worldPos.z);
	wind /= windWaveLength;
	wind += time;

	return wind;
}

void largeScaleWind(inout vec3 worldPos, vec4 vertexColor, vec3 windDirection, float windFactor, float noise)
{
	float largeWindPower = sin(windFactor);

	// Amplify in one direction
	if (largeWindPower < 0.0)
		largeWindPower *= 0.4;
	else
		largeWindPower *= 0.6;

	largeWindPower *= noise;
	largeWindPower *= vertexColor.r;

	worldPos.x += windDirection.x * largeWindPower;
	worldPos.y += windDirection.y * largeWindPower;
	worldPos.z += windDirection.z * largeWindPower;
}

void mediumScaleWind(inout vec3 worldPos, vec4 vertexColor, vec3 noise, float windFactor)
{
	float x = (2 * sin(1 * windFactor)) + 1;
	float z = (1 * sin(2 * windFactor)) + 0.5;
	vec3 mediumWindPower = vec3(x, 0.0, z) * vec3(0.1);

	mediumWindPower *= noise;
	mediumWindPower *= vertexColor.g;

	worldPos += mediumWindPower;
}

void smallScaleWind(inout vec3 worldPos, vec4 vertexColor, vec3 normal, vec3 noise, float windFactor, float windStrength)
{
	float smallWindPower = (0.065 * sin(2.65 * windFactor));
	smallWindPower *= windStrength;

	vec3 smallJitter = vec3(smallWindPower);
	smallJitter *= normal;
	smallJitter *= vec3(1.0, 0.35, 1.0);
	smallJitter *= vec3(0.075);
	smallJitter *= noise;
	smallJitter *= vertexColor.b;

	worldPos += smallJitter;
}

