#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_vulkan_glsl : enable

const int TILE_SIZE = 16;

struct PointLight{
	vec3 pos;
	float radius;
	vec3 intensity;
};

#define MAX_POINT_LIGHT_PER_TILE 1023
struct LightVisiblity{
	uint count;
	uint lightindices[MAX_POINT_LIGHT_PER_TILE];
};

layout(push_constant) uniform PushConstantObject{
	ivec2 viewportSize;
	ivec2 tileNums;
	int debugviewIndex;
} pushConstants;

layout(std140, set = 0, binding = 0) uniform UniformBufferObject{
    mat4 model;
	vec3 lightPosition;
} transform;

layout(std140, set = 2, binding = 0) buffer CameraUbo{
    mat4 view;
    mat4 proj;
    mat4 projview;
    vec3 cameraPosition;
} ubo;

layout(std430, set = 1, binding = 0) readonly buffer TileLightVisiblities{
    LightVisiblity lightVisiblities[];
};

layout(std140, set = 1, binding = 1) readonly buffer PointLights{
	int lightNum;
	PointLight pointlights[2000];
};

layout(set = 3, binding = 0) uniform sampler2D depthSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUVCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragViewVec;
layout(location = 5) in vec3 fragLightVec;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D tex;

layout(early_fragment_tests) in;

//layout(push_constant) uniform PushConstants{
//	bool usePhong;
//} pushConts;

//vec4 phong();
//vec4 cartoon();

void main(){
	ivec2 tileID = ivec2(gl_FragCoord.xy / TILE_SIZE);
	uint tileIndex = tileID.y * pushConstants.tileNums.x + tileID.x;

	vec3 illuminance = vec3 (0.0);
	uint tileLightNum = lightVisiblities[tileIndex].count;

	//debugview
	if(pushConstants.debugviewIndex > 1){
		if(pushConstants.debugviewIndex == 2){
			float intensity = float(lightVisiblities[tileIndex].count) / 64;
			 outColor = vec4(vec3(intensity), 1.0) ; //light culling debug
		}
		else if(pushConstants.debugviewIndex == 3){
			float preDepth = texture(depthSampler, (gl_FragCoord.xy/pushConstants.viewportSize) ).x;
            outColor = vec4(vec3( preDepth ),1.0);
		}
		else if(pushConstants.debugviewIndex == 4){
			 outColor = vec4(abs(fragNormal), 1.0);
		}
		return;
	}

	for(int i = 0; i < tileLightNum; i++){
		PointLight light = pointlights[lightVisiblities[tileIndex].lightindices[i]];
		vec3 lightDirection = normalize(light.pos - fragWorldPos);
		float lambertian = max(dot(lightDirection, fragNormal), 0.0);

		if(lambertian > 0.0){
			float lightDistance = distance(light.pos, fragWorldPos);
            if (lightDistance > light.radius){
                continue;
            }
			vec3 viewDir = normalize(ubo.cameraPosition - fragWorldPos);
            vec3 halfDir = normalize(lightDirection + viewDir);
            float specAngle = max(dot(halfDir, fragNormal), 0.0);
            float specular = pow(specAngle, 32.0);

            float att = clamp(1.0 - lightDistance * lightDistance / (light.radius * light.radius), 0.0, 1.0);
            illuminance += light.intensity * att * (lambertian + specular);
		}
	}

	if(pushConstants.debugviewIndex == 1){
		float intensity = float(lightVisiblities[tileIndex].count) / (64/2.0);
		outColor = vec4(vec3(intensity, intensity * 0.5, intensity * 0.5) + illuminance * 0.25, 1.0);
		return;
	}

	outColor = vec4(illuminance, 1.0);
	
	//outColor = phong();

	//outColor = vec4(fragColor, 1.0);
	//outColor = texture(tex, fragUVCoord);


	
	
	//float val = dot(N, V);
	//outColor = vec4(val, val, val, 1.0);

	

	//outColor = vec4(N, 1.0);
}

vec4 phong(){
	vec3 N = normalize(fragNormal);
	vec3 L = normalize(fragLightVec);
	vec3 V = normalize(fragViewVec);
	vec3 R = reflect(-L, N);

	vec3 ambient = fragColor * 0.1;
	vec3 diffuse = max(dot(N, L), 0.0) * fragColor;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(1.35f) ;

	//return vec4(N + specular, 1.0);

	return vec4(ambient + diffuse + specular, 1.0);
}

vec4 cartoon(){
	vec3 N = normalize(fragNormal);
	vec3 L = normalize(fragLightVec);
	vec3 V = normalize(fragViewVec);
	vec3 R = reflect(-L, N);

	if(pow(max(dot(R, V), 0.0), 5.0) > 0.5){
		outColor = vec4(1.0);
	}
	else if(dot(V, N) < 0.5){
		outColor = vec4(fragColor / 10, 1.0);
	}
	else if(max(dot(N, L), 0.0) >= 0.1){
		outColor = vec4(fragColor, 1.0);
	}
	else{
		outColor = vec4(fragColor / 5, 1.0);
	}

	return outColor;
}