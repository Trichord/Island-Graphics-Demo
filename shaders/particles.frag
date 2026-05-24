#version 460 core

// In
flat in vec4 color;

// Out
out vec4 fragColor;

void main()
{
	vec2 point = gl_PointCoord * 2.0 - 1.0;

	if (dot(point, point) > 1)
		discard;
	
	float alpha = smoothstep(0.8, 1.0, color.a);
	fragColor = vec4(vec3(color), alpha);
}