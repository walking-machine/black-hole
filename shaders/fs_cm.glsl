#version 430
in vec3 tc;
layout (binding = 0) uniform samplerCube samp;
out vec4 color;

uniform mat4 mv_matrix;
uniform mat4 proj_matrix;

void main(void)
{
    color = texture(samp, tc);
    //color = vec4(tc, 1.0f);
}