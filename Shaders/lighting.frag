#version 330 core

#define NR_POINT_LIGHTS 4

struct Material{
    sampler2D diffuse;
    sampler2D specular;
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

uniform vec3 uViewPosition;
uniform Material material;
uniform DirectionalLight directionalLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;

uniform bool spotLightOn;
uniform bool directionalLightOn;
uniform bool pointLightOn;

out vec4 FragColor;

void main(){

vec3 result = vec3(0.0f);
vec3 norm = normalize(vNormalVector); //Normalize incoming surface normal vector (may have been distorted by scale/rotation from normal matrix)
vec3 viewDirection = normalize(uViewPosition - vFragPosition); //Fragment to camera position
MaterialTexture materialTexture =  MaterialTexture(texture(material.diffuse, vTexCoords), texture(material.specular, vTexCoords));

if (directionalLightOn)
    result += CalcDirLight(directionalLight, norm, viewDirection, materialTexture);

if (pointLightOn){

    for (int i = 0; i < NR_POINT_LIGHTS; i++){
        vec3 lightDirection = normalize(pointLights[i].position - vFragPosition);
        vec3 reflectDirection = reflect(-lightDirection, vNormalVector);
        result += CalcPointLight(pointLights[i], norm, vFragPosition, viewDirection, reflectDirection, lightDirection, materialTexture);
    }
}

if (spotLightOn)
    result += CalcSpotLight(spotLight, norm, vFragPosition, viewDirection, materialTexture);

if (!directionalLightOn && !pointLightOn && !spotLightOn)
    result += vec3(materialTexture.diffuse) * 0.075;

FragColor = vec4(result, 1.0f);
};

vec3 CalcDirLight(DirectionalLight light, vec3 normal, vec3 viewDir, MaterialTexture materialTexture){
        vec3 lightDirection = normalize(-light.direction);
        vec3 reflectDirection = reflect(-lightDirection, normal);

        //Diffuse light strength
        float diffuseStrength = max(dot(normal, lightDirection), 0.0f);
        
        //Specular light strength
        float specularStrength = pow(max(dot(viewDir, reflectDirection), 0.0f), material.shininess);

        vec3 ambientLight = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuseLight = light.diffuse * diffuseStrength * vec3(materialTexture.diffuse);
        vec3 specularLight = light.specular * specularStrength * vec3(materialTexture.specular);

        return ambientLight + diffuseLight + specularLight;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 reflectDir, vec3 lightDir, MaterialTexture materialTexture){
        float lightDistance = length(light.position - fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * lightDistance + light.quadratic * (lightDistance * lightDistance));
    
        float diffuseStrength = max(dot(normal, lightDir), 0.0f);
        
        float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0f), material.shininess);
        
        vec3 ambientLight = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuseLight = light.diffuse * diffuseStrength * vec3(materialTexture.diffuse);
        vec3 specularLight = light.specular * specularStrength * vec3(materialTexture.specular);

        ambientLight *= attenuation;
        diffuseLight *= attenuation;
        specularLight *= attenuation;
    
        return ambientLight + diffuseLight + specularLight;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, MaterialTexture materialTexture){  
        vec3 lightDirection = normalize(light.position - fragPos);
        vec3 reflectDirection = reflect(-lightDirection, normal);
        
        //Calculate intensity of light.  Outside outer circle = 0, inside inner circle = 1
        float theta = dot(lightDirection, normalize(-light.direction));
        float epsilon = light.cutoff - light.outerCutoff;
        float intensity = clamp((theta - light.outerCutoff) / epsilon, 0.0f, 1.0f);
        
        float diff = max(dot(normal, lightDirection), 0.0f);
        float spec = pow(max(dot(viewDir, reflectDirection), 0.0f), material.shininess);     

        vec3 ambientLight = light.ambient * vec3(materialTexture.diffuse);
        vec3 diffuseLight = light.diffuse * diff * vec3(materialTexture.diffuse);
        vec3 specularLight = light.specular * spec * vec3(materialTexture.specular);

        diffuseLight *= intensity;
        specularLight *= intensity;

        return ambientLight + diffuseLight + specularLight;  
}