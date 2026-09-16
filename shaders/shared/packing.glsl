// Bit packing functions to support compressed vertex data.

vec4 unpackSnorm3x10_1x2(uint p)
{
	int x = bitfieldExtract(int(p), 0, 10);
	int y = bitfieldExtract(int(p), 10, 10);
	int z = bitfieldExtract(int(p), 20, 10);
	int w = bitfieldExtract(int(p), 30, 2);

	vec4 unpacked = vec4(x, y, z, w) / vec4(511.0, 511.0, 511.0, 1.0);

	return clamp(unpacked, -1.0, 1.0);
}

uint packSnorm3x10_1x2(vec4 vec)
{
	vec = clamp(vec, -1.0, 1.0);
	ivec4 ivec = ivec4(round(vec * vec4(511.0, 511.0, 511.0, 1.0)));

	uint pked = 0u;

	pked = bitfieldInsert(pked, uint(ivec.x), 0, 10);
	pked = bitfieldInsert(pked, uint(ivec.y), 10, 10);
	pked = bitfieldInsert(pked, uint(ivec.z), 20, 10);
	pked = bitfieldInsert(pked, uint(ivec.w), 30, 2);

	return pked;
}