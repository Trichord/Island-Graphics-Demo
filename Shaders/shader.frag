#version 330 core

struct Material{
    sampler2D diffuse;
    vec3 specular;
    float shininess;
};

struct Light{
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

in vec2 vTexCoords;
in vec3 vNormalVector;
in vec3 vFragPosition;

uniform vec3 uViewPosition;
uniform Material material;
uniform Light light;

out vec4 FragColor;

void main(){

//Normalize incoming surface normal vector (may have been distorted by translation by model matrix) and calculate direction from light to fragment
vec3 norm = normalize(vNormalVector);
vec3 lightDirection = normalize(light.position - vFragPosition);

//Calculate view direction and specular light reflection direction
vec3 viewDirection = normalize(uViewPosition - vFragPosition);
vec3 reflectDirection = reflect(-lightDirection, norm);

//Ambient light
vec3 ambientLight = light.ambient * vec3(texture(material.diffuse, vTexCoords));

//Diffuse light
float diff = max(dot(norm, lightDirection), 0.0f);
vec3 diffuseLight = light.diffuse * diff * vec3(texture(material.diffuse, vTexCoords));

//Specular light
float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), material.shininess);
vec3 specularLight = light.specular * (spec * material.specular);

vec3 finalColor = ambientLight + diffuseLight + specularLight;

FragColor = vec4(finalColor, 1.0f);

};