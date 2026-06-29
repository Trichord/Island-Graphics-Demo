// Functions for rendering trees, grass, flowers, etc.

// Constants
const float alphaCutoff = 0.425;

// Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
float calcMipLevel(vec2 textureCoords)
{
    vec2 dx = dFdx(textureCoords);
    vec2 dy = dFdy(textureCoords);
    float deltaMaxSqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(deltaMaxSqr));
}

// Credit: Ben Golus. Rescale alpha by partial derivative. 
float rescaleAlpha(float alpha)
{
    return (alpha - alphaCutoff) / max(fwidth(alpha), 0.0001) + 0.5;
}
