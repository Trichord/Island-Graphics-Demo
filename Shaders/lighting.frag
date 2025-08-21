#version 330 core

#define MAX_POINT_LIGHTS 4

struct MaterialProperties{
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
    float shininess;
};

struct DirectionalLight{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight{
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct SpotLight{
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
};

struct MaterialTexture{
    vec4 diffuse;
    vec4 specular;
};

vec3 calcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, MaterialTexture materialTexture);
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 reflectDir, vec3 lightDir, MaterialTexture materialTexture);  
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialTexture materialTexture);

in vec2 texCoords;
in vec3 normalVector;
in vec3 fragPosition;

uniform bool spotLightOn;
uniform bool directionalLightOn;
uniform bool pointLightOn;

uniform vec3 viewPosition;

uniform MaterialProperties material;
uniform DirectionalLight directionalLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight spotLight;

out vec4 fragColor;

void main() {
    vec3 result = vec3(0.0f);
    vec3 norm = normalize(normalVector); //Normal vector may have been distorted by scale/rotation
    vec3 viewDir = normalize(viewPosition - fragPosition);
    MaterialTexture materialTexture =  MaterialTexture(texture(material.texture_diffuse1, texCoords), texture(material.texture_specular1, texCoords)); //Sample textures once

    if (directionalLightOn)
        result += calcDirLight(directionalLight, norm, viewDir, materialTexture);

    if (pointLightOn)
        for (int i = 0; i < MAX_POINT_LIGHTS; i++){
            vec3 lightDir = normalize(pointLights[i].position - fragPosition);
            vec3 reflectDir = reflect(-lightDir, norm);
            result += calcPointLight(pointLights[i], norm, fragPosition, viewDir, reflectDir, lightDir, materialTexture);
        }

    if (spotLightOn)
        result += calcSpotLight(spotLight, norm, fragPosition, viewDir, materialTexture);

    if (!directionalLightOn && !pointLightOn && !spotLightOn)
        result += vec3(materialTexture.diffuse) * 0.05f;

    fragColor = vec4(result, 1.0f);
}

vec3 calcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, MaterialTexture materialTexture){
        vec3 lightDir = normalize(-light.direction);
        vec3 reflectDir = reflect(-lightDir, normal);

        float diffuseStrength = max(dot(normal, lightDir), 0.0f);
        float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0f), material.shininess);

        vec3 ambient = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuse = light.diffuse * diffuseStrength * vec3(materialTexture.diffuse);
        vec3 specular = light.specular * specularStrength * vec3(materialTexture.specular);

        return ambient + diffuse + specular;
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 reflectDir, vec3 lightDir, MaterialTexture materialTexture){
        float lightDistance = length(light.position - fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * lightDistance + light.quadratic * (lightDistance * lightDistance));
    
        float diffuseStrength = max(dot(normal, lightDir), 0.0f);    
        float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0f), material.shininess);
        
        vec3 ambient = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuse = light.diffuse * diffuseStrength * vec3(materialTexture.diffuse);
        vec3 specular = light.specular * specularStrength * vec3(materialTexture.specular);

        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;
    
        return ambient + diffuse + specular;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialTexture materialTexture){  
        vec3 lightPos = normalize(light.position - fragPos);
        vec3 reflectDir = reflect(-lightPos, normal);

        float lightDistance = length(light.position - fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * lightDistance + light.quadratic * (lightDistance * lightDistance));
        
        //Calculate intensity of light.  Beyond outerCutoff = 0, inside cutoff = 1, otherwise blend
        float theta = dot(viewDir, normalize(-light.direction));
        float epsilon = light.cutoff - light.outerCutoff;
        float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0f, 1.0f);
        
        float diff = max(dot(normal, viewDir), 0.0f);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0f), material.shininess);     

        vec3 ambient = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuse = light.diffuse * diff * vec3(materialTexture.diffuse);
        vec3 specular = light.specular * spec * vec3(materialTexture.specular);

        diffuse *= intensity;
        specular *= intensity;

        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;

        return ambient + diffuse + specular;  
}