#version 430
layout (binding = 0) uniform samplerCube samp;
in vec2 tc;
out vec4 color;

uniform mat4 mv_matrix;
uniform mat4 proj_matrix;
uniform float aspect;
uniform float sun_multi;
uniform float camera_r;

float hard_fun(float w, float C)
{
    return 1.0f - w * w * (1.0f - C * w);
}

float d_hard_fun(float w, float C)
{
    return -2 * w + 3*C*w*w;
}

const float PI = 3.1415926535897932384626433832795;

void main(void)
{
    float half_fov = 0.9f;
    float px = tc.x * aspect * tan(half_fov);
    float py = tc.y * tan(half_fov);
    vec4 v = normalize(vec4(px, py, -1, 0));

    float rv = sqrt(v.x * v.x + v.y * v.y);
    float phi = asin(rv);
    float b = camera_r * rv;
    float sun_mass = 1.47f;
    float C = 2.0f * sun_multi * sun_mass / b;

    if (C > 0.38f) {
        color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    float w1 = 1.0f;

    for (int i = 0; i < 6; i++) {
        w1 -= hard_fun(w1, C) / d_hard_fun(w1, C);
    }

    w1 -= 0.02f;
    float new_phi = 0.0f;
    float num_divs = 100.0f;
    float w_step = w1 * 0.01f;

    for (float i = 0.0f; i < num_divs; i += 1.0f) {
        float w = w1 * i * 0.01f;
        // float value = hard_fun(w + w_step * 0.5f, C);
        // value = 2.0f / sqrt(value);
        // new_phi += value * w_step;
        // float f_a = 1.0f / sqrt(hard_fun(w, C));
        // float f_b = 1.0f / sqrt(hard_fun(w + w_step, C));
        // new_phi += w_step * (f_a + f_b);
        float f_a = 2.0f / sqrt(hard_fun(w, C));
        float f_b = 2.0f / sqrt(hard_fun(w + w_step, C));
        float f_m = 2.0f / sqrt(hard_fun(w + w_step * 0.5f, C));
        new_phi += w_step * (f_a + 4.0f * f_m + f_b) / 6.0f;
    }

    new_phi = new_phi - PI;//; + phi;
    new_phi = phi - new_phi;
//    color = (14.0f * new_phi / PI) * vec4(1.0f, 0.0f, 1.0f, 1.0f);
//    color = (hard_fun(w, C) * 1000000.0f) * vec4(1.0f, 0.0f, 1.0f, 1.0f);
    v.x = v.x * sin(new_phi) / sin(phi);
    v.y = v.y * sin(new_phi) / sin(phi);
    v.z = -sqrt(1.0f - sin(new_phi) * sin(new_phi));
    v = inverse(mv_matrix) * v;

    color = texture(samp, v.xyz);
}