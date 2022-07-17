#version 430

layout (location=0) in vec3 position;
out vec2 tc;

uniform mat4 mv_matrix;
uniform mat4 proj_matrix;
uniform float aspect;
uniform float sun_multi;
uniform float camera_r;

layout (binding = 0) uniform samplerCube samp;

void main(void)
{
    tc = position.xy;
    gl_Position = vec4(position, 1.0f);
}