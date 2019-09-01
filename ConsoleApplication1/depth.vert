#version 450
#extension GL_ARB_separate_shader_objects : enable
// Vertex shader for depth prepass

layout(std140, set = 0, binding = 0) uniform SceneObjectUbo{
    mat4 model;
} transform;

layout(std140, set = 1, binding = 0) readonly buffer CameraUbo{ // FIXME: change back to uniform
    mat4 view;
    mat4 proj;
    mat4 projview;
    vec3 cameraPos;
} camera;

layout(location = 0) in vec3 in_position;

out gl_PerVertex{
    vec4 gl_Position;
};

void main(){
	mat4 mvp = camera.projview * transform.model;
	gl_Position = mvp * vec4(in_position, 1.0);
}
