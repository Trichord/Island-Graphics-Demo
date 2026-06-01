// Functions for wind effects

const float windWaveLength = 5.75;

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

	largeWindPower *= vertexColor.r;

	worldPos.x += windDirection.x * largeWindPower;
	worldPos.z += windDirection.z * largeWindPower;
}

void mediumScaleWind(inout vec3 worldPos, vec4 vertexColor, vec3 noise, float windFactor)
{
	float x = (0.0525 * sin(1.7 * windFactor));
	float z = (0.0045 * sin(1.2 * windFactor));
	vec3 mediumWindPower = vec3(x, 0.0, z);

	mediumWindPower += noise * 0.075;
	mediumWindPower *= vertexColor.g;

	worldPos += mediumWindPower;
}

void smallScaleWind(inout vec3 worldPos, vec4 vertexColor, vec3 normal, vec3 noise, float windFactor, float windStrength)
{
	float smallWindPower = (0.125 * sin(2.0 * windFactor));
	smallWindPower *= windStrength;

	vec3 smallJitter = vec3(smallWindPower);
	smallJitter *= normal;
	smallJitter *= vec3(1.0, 0.35, 1.0);
	smallJitter *= noise;
	smallJitter *= vertexColor.b;

	worldPos += smallJitter;
}

