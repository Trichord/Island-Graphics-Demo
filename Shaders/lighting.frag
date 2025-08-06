#version 330 core

#define NR_POINT_LIGHTS 4

struct Material{
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

vec3 CalcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, MaterialTexture materialTexture);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 reflectDir, vec3 lightDir, MaterialTexture materialTexture);  
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialTexture materialTexture);

in vec2 vTexCoords;
in vec3 vNormalVector;
in vec3 vFragPosition;

uniform bool uSpotLightOn;
uniform bool uDirectionalLightOn;
uniform bool uPointLightOn;

uniform vec3 uViewPosition;

uniform Material uMaterial;
uniform DirectionalLight uDirectionalLight;
uniform PointLight uPointLights[NR_POINT_LIGHTS];
uniform SpotLight uSpotLight;

out vec4 FragColor;

void main() {
    vec3 result = vec3(0.0f);
    vec3 norm = normalize(vNormalVector); //Normal vector may have been distorted by scale/rotation
    vec3 viewDir = normalize(uViewPosition - vFragPosition);
    MaterialTexture materialTexture =  MaterialTexture(texture(uMaterial.texture_diffuse1, vTexCoords), texture(uMaterial.texture_specular1, vTexCoords)); //Sample textures once

    if (uDirectionalLightOn)
        result += CalcDirLight(uDirectionalLight, norm, viewDir, materialTexture);

    if (uPointLightOn)
        for (int i = 0; i < NR_POINT_LIGHTS; i++){
            vec3 lightDir = normalize(uPointLights[i].position - vFragPosition);
            vec3 reflectDir = reflect(-lightDir, norm);
            result += CalcPointLight(uPointLights[i], norm, vFragPosition, viewDir, reflectDir, lightDir, materialTexture);
        }

    if (uSpotLightOn)
        result += CalcSpotLight(uSpotLight, norm, vFragPosition, viewDir, materialTexture);

    if (!uDirectionalLightOn && !uPointLightOn && !uSpotLightOn)
        result += vec3(materialTexture.diffuse) * 0.05f;

    FragColor = vec4(result, 1.0f);
}

vec3 CalcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, MaterialTexture materialTexture){
        vec3 lightDir = normalize(-light.direction);
        vec3 reflectDir = reflect(-lightDir, normal);

        float diffuseStrength = max(dot(normal, lightDir), 0.0f);
        float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0f), uMaterial.shininess);

        vec3 ambient = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuse = light.diffuse * diffuseStrength * vec3(materialTexture.diffuse);
        vec3 specular = light.specular * specularStrength * vec3(materialTexture.specular);

        return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 reflectDir, vec3 lightDir, MaterialTexture materialTexture){
        float lightDistance = length(light.position - fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * lightDistance + light.quadratic * (lightDistance * lightDistance));
    
        float diffuseStrength = max(dot(normal, lightDir), 0.0f);    
        float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0f), uMaterial.shininess);
        
        vec3 ambient = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuse = light.diffuse * diffuseStrength * vec3(materialTexture.diffuse);
        vec3 specular = light.specular * specularStrength * vec3(materialTexture.specular);

        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;
    
        return ambient + diffuse + specular;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialTexture materialTexture){  
        vec3 lightPos = normalize(light.position - fragPos);
        vec3 reflectDir = reflect(-lightPos, normal);

        float lightDistance = length(light.position - fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * lightDistance + light.quadratic * (lightDistance * lightDistance));
        
        //Calculate intensity of light.  Outside outer circle = 0, inside inner circle = 1
        float theta = dot(viewDir, normalize(-light.direction));
        float epsilon = light.cutoff - light.outerCutoff;
        float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0f, 1.0f);
        
        float diff = max(dot(normal, viewDir), 0.0f);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0f), uMaterial.shininess);     

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