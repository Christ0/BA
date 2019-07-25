#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUVCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragViewVec;
layout(location = 4) in vec3 fragLightVec;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D tex;

layout(push_constant) uniform PushConstants{
	bool usePhong;
} pushConts;

vec4 phong();
vec4 cartoon();

void main(){
	//outColor = vec4(fragColor, 1.0);
	//outColor = texture(tex, fragUVCoord);


	if(pushConts.usePhong){
		outColor = phong();
	}
	else{
		outColor = cartoon();
	}
	
	

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

	return vec4(N + specular, 1.0);

	//return vec4(ambient + diffuse + specular, 1.0);
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