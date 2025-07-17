#version 330 core

#define POINT_LIGHT 0u
#define DIRECTIONAL_LIGHT 1u
#define SPOT_LIGHT 2u

struct Material{
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light{
    vec3 position;
    vec3 direction;
   
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
   
   float cutoff;
   float outerCutoff;

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

vec3 lightDirection, viewDirection, reflectDirection, diffuseLight, ambientLight, specularLight;
float diff, spec;

//Normalize incoming surface normal vector (may have been distorted by scale/rotation from normal matrix) and calculate direction from light to fragment
vec3 norm = normalize(vNormalVector);

    switch (light.type){

    case POINT_LIGHT:

        lightDirection = normalize(light.position - vFragPosition);
    
        float lightDistance = length(light.position - vFragPosition);
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

    case DIRECTIONAL_LIGHT:

        lightDirection = normalize(-light.position);
    
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

    case SPOT_LIGHT:

        lightDirection = normalize(light.position - vFragPosition);
    
        //Intensity = (θ − γ) / ϵ.  Epsilon = inner cutoff - outer cutoff
        float theta = dot(lightDirection, normalize(-light.direction));
        float epislon = light.cutoff - light.outerCutoff;
        float intensity = clamp((theta - light.outerCutoff) / epislon, 0.0f, 1.0f);

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

        //Add spotlight intensity
        diffuseLight *= intensity;
        specularLight *= intensity;

        FragColor = vec4(ambientLight + diffuseLight + specularLight, 1.0f);        

        break;
    }
};