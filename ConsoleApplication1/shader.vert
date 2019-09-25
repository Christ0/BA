#version 450
#extension GL_ARB_separate_shader_objects : enable
//#extension GL_KHR_vulkan_glsl : enable

layout(std140, set = 0, binding = 0) uniform SceneObjectUbo{
    mat4 model;
	vec3 lightPosition;
} transform;

layout(std140, set = 2, binding = 0) buffer CameraUbo{
    mat4 view;
    mat4 proj;
    mat4 projview;
    vec3 cameraPosition;
} camera;

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec2 in_UVCoord;
layout(location = 3) in vec3 in_Normal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUVCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec3 fragViewVec;
layout(location = 5) out vec3 fragLightVec;

out gl_PerVertex{
	vec4 gl_Position;
};

void main(){ //Für jeden Vertex ausführen
	mat4 invTransModel = transpose(inverse(transform.model));
	mat4 mvp = camera.projview * transform.model;

	gl_Position = mvp * vec4(in_pos, 1.0);
	fragColor = in_color;
	fragUVCoord = in_UVCoord;

	fragNormal = normalize((invTransModel * vec4(in_Normal, 0.0)).xyz);
	fragWorldPos = vec3(transform.model * vec4(in_pos, 1.0));
	//fragViewVec = -(camera.view * fragWorldPos).xyz;
	fragLightVec = mat3(camera.view) * (transform.lightPosition - vec3(fragWorldPos));
	//fragNormal = mat3(camera.view) * mat3(transform.model) * in_Normal; //model zu mat3 casten, damit der Translate Teil wegfällt und die normals nicht verschoben werden
}