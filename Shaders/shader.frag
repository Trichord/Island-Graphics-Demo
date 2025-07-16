#version 330 core

struct Material{
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light{
    vec4 position; //If w component is 0, treat as directional.  Otherwise it's a point.
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
    float cutoff;

    uint type;
};

in vec2 vTexCoords;
in vec3 vNormalVector;
in vec3 vFragPosition;

uniform vec3 uViewPosition;
uniform Material material;
uniform Light light;

out vec4 FragColor;

void main(){

const uint point = 0u;
const uint directional = 1u;
const uint spot = 2u;

vec3 lightDirection, viewDirection, reflectDirection, diffuseLight, ambientLight, specularLight;
float diff, spec;

//Normalize incoming surface normal vector (may have been distorted by scale/rotation from normal matrix) and calculate direction from light to fragment
vec3 norm = normalize(vNormalVector);

switch (light.type){

case point:

    lightDirection = normalize(vec3(light.position) - vFragPosition);
    float lightDistance = length(vec3(light.position) - vFragPosition);
    float attenuation = 1.0f / (light.constant + light.linear * lightDistance + light.quadratic * lightDistance * lightDistance);
    
    //Calculate view direction and specular light reflection direction
    viewDirection = normalize(uViewPosition - vFragPosition);
    reflectDirection = reflect(-lightDirection, norm);

    //Ambient light
    ambientLight = light.ambient * vec3(texture(material.diffuse, vTexCoords));

    //Diffuse light
    diff = max(dot(norm, lightDirection), 0.0f);
    diffuseLight = light.diffuse * diff * vec3(texture(material.diffuse, vTexCoords));

    //Specular light
    spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), material.shininess);
    specularLight = light.specular * spec * vec3(texture(material.specular, vTexCoords));

    ambientLight *= attenuation;
    diffuseLight *= attenuation;
    specularLight *= attenuation;
    
    FragColor = vec4(ambientLight + diffuseLight + specularLight, 1.0f);
    break;

case directional:

    lightDirection = vec3(normalize(-light.position));
    //Calculate view direction and specular light reflection direction
    viewDirection = normalize(uViewPosition - vFragPosition);
    reflectDirection = reflect(-lightDirection, norm);

    //Ambient light
    ambientLight = light.ambient * vec3(texture(material.diffuse, vTexCoords));

    //Diffuse light
    diff = max(dot(norm, lightDirection), 0.0f);
    diffuseLight = light.diffuse * diff * vec3(texture(material.diffuse, vTexCoords));

    //Specular light
    spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), material.shininess);
    specularLight = light.specular * spec * vec3(texture(material.specular, vTexCoords));

    FragColor = vec4(ambientLight + diffuseLight + specularLight, 1.0f);
    break;

case spot:

    break;

}
};