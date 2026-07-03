#version 460 core

// In
flat in vec4 color;

// Out
out vec4 fragColor;

void main()
{
	vec2 point = gl_PointCoord * 2.0 - 1.0;
	float dist2 = dot(point, point);

	float alpha = color.a * (1.0 - smoothstep(0.9, 1.0, dist2));

	if (dist2 > 1)
		alpha = 0.0;
	
	fragColor = vec4(vec3(color), alpha);
}