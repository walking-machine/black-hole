#version 430

layout (location=0) in vec3 position;
out vec3 tc;

uniform mat4 mv_matrix;
uniform mat4 proj_matrix;

layout (binding = 0) uniform samplerCube samp;

void main(void)
{
    gl_Position = proj_matrix * mv_matrix * vec4(position, 1.0);
    //vec4 world_pos = mv_matrix * vec4(position, 1.0);
    tc = normalize(position);
}