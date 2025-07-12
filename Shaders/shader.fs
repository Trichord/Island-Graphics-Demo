#version 330 core
out vec4 FragColor;

uniform vec3 uLightColor;
uniform vec3 uObjectColor;
uniform vec3 uLightPosition;
uniform vec3 uViewPosition;

in vec3 vNormalVector;
in vec3 vFragPosition;

void main(){
float ambientStrength = 0.1f;
float specularStrength = 0.5f;

//Normalize incoming surface normal vector (may have been distorted by translation by model matrix) and calculate direction from light to fragment
vec3 norm = normalize(vNormalVector);
vec3 lightDirection = normalize(uLightPosition - vFragPosition);

//Calculate view direction and specular light reflection direction
vec3 viewDirection = normalize(uViewPosition - vFragPosition);
vec3 reflectDirection = reflect(-lightDirection, norm);

//Dot product between light direction and normal
float diffuseLight = max(dot(norm, lightDirection), 0.0f);

//Specular component
float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), 32);
vec3 specularLight = specularStrength * spec * uLightColor;

//Add ambient + diffuse and multiply by color to get final color
vec3 ambientLight = ambientStrength * uLightColor;
vec3 finalColor = (ambientLight + diffuseLight + specularLight) * uObjectColor;

FragColor = vec4(finalColor, 1.0f);
};