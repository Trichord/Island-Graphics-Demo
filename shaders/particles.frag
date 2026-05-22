#version 460 core

// In
flat in vec4 color;

// Out
out vec4 fragColor;

void main()
{
	fragColor = color;
}