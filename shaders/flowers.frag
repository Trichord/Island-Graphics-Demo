#version 460 core

const vec3 lightColor = vec3(1.0, 0.95, 0.9);
const vec3 defaultNormal = vec3(0.0, 1.0, 0.0);
const float alphaCutoff = 0.5;
const float mipScale = 0.25;
const float ambientStrength = 0.8;

out vec4 fragColor;

uniform sampler2D colorTexture;
uniform vec3 lightDir;
uniform float gamma;

in vec2 texCoords;

//Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
float CalcMipLevel(vec2 texture_coord){
    vec2 dx = dFdx(texture_coord);
    vec2 dy = dFdy(texture_coord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(delta_max_sqr));
}

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) {
const float a = 2.51;
const float b = 0.03;
const float c = 2.43;
const float d = 0.59;
const float e = 0.14;

return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 grassColor = texture(colorTexture, texCoords);

    //Scale alpha by mip level
    vec2 texSize = vec2(textureSize(colorTexture, 0)); // Level 0 size
    grassColor.a *= 1 + max(0, CalcMipLevel(texCoords * texSize)) * mipScale;

    // Rescale alpha by partial derivative
    grassColor.a = (grassColor.a - alphaCutoff) / max(fwidth(grassColor.a), 0.0001) + 0.5;

    //Flip normal if 2D quad is facing away
    vec3 normal = gl_FrontFacing ? defaultNormal : -defaultNormal;
    
    //Ambient
    vec3 ambient = ambientStrength * lightColor * grassColor.rgb;

    //Diffuse
    vec3 lightDirection = normalize(-lightDir);
    float diffuseStrength = dot(normal, lightDirection) * 0.5 + 0.5; //remap to [0,1] range so perpendicular blades get some light
    vec3 diffuse = diffuseStrength * lightColor * grassColor.rgb;

    vec3 color = ambient + diffuse;

    //Tone mapping
    color = ACESFilm(color);

    //Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, grassColor.a);
}