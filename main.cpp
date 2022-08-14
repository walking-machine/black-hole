#include "sdl-opengl-utils/opengl_sdl_utils.hpp"
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"

static GLuint prog_rc = 0;
static GLuint vao;
static GLuint vbo;
static GLuint space_cubemap = 0;
static float aspect = 1.0f;
static glm::quat camera_quat(1.0f, 0.0f, 0.0f, 0.0f);
static float camera_r = 500.0f;
static float sun_multi = 10.0f;
static unsigned int newtone_iters = 6;
static float w_decrement = 0.02f;
static unsigned int num_divs = 100;
static unsigned int integration_method = 0;

int res_init()
{
    std::cout << "Reading shaders\n";
    GLuint vs = read_shader("shaders/vs_iden.glsl", GL_VERTEX_SHADER);
    GLuint fs = read_shader("shaders/fs_bh.glsl", GL_FRAGMENT_SHADER);
    std::cout << "Creating program\n";
    prog_rc = create_program({vs, fs});

    std::cout << "Generating arrays\n";

    float all_screen_vert[18] = {
        -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f
    };

    glGenVertexArrays(1, &vao);
    if (checkOpenGLError())
        return -1;

    glBindVertexArray(vao);
    if (checkOpenGLError())
        return -1;

    glGenBuffers(1, &vbo);
    if (checkOpenGLError())
        return -1;

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (checkOpenGLError())
        return -1;

    glBufferData(GL_ARRAY_BUFFER, sizeof(all_screen_vert), all_screen_vert,
                 GL_STATIC_DRAW);
    if (checkOpenGLError())
        return -1;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    space_cubemap = load_cubemap({
                                  "ulukai/corona_rt.png",
                                  "ulukai/corona_lf.png",
                                  "ulukai/corona_up.png",
                                  "ulukai/corona_dn.png",
                                  "ulukai/corona_bk.png",
                                  "ulukai/corona_ft.png",
                                 });
    if (!space_cubemap)
        return -1;
    return 0;
}

void display()
{
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog_rc);

    GLuint mv_loc = glGetUniformLocation(prog_rc, "mv_matrix");
    GLuint prj_loc = glGetUniformLocation(prog_rc, "proj_matrix");
    GLuint aspect_loc = glGetUniformLocation(prog_rc, "aspect");
    GLuint r_loc = glGetUniformLocation(prog_rc, "camera_r");
    GLuint multi_loc = glGetUniformLocation(prog_rc, "sun_multi");
    GLuint w_decrement_loc = glGetUniformLocation(prog_rc, "w_decrement");
    GLuint newtone_iters_loc = glGetUniformLocation(prog_rc, "newtone_iters");
    GLuint num_divs_loc = glGetUniformLocation(prog_rc, "num_divs");
    GLuint method_loc = glGetUniformLocation(prog_rc, "integration_method");

    glm::mat4 prj_mtx = glm::perspective(1.0f, aspect, 0.1f, 1000.0f);
    glm::mat4 view_rot_mtx = glm::mat4_cast(camera_quat);

    glUniformMatrix4fv(mv_loc, 1, GL_FALSE, glm::value_ptr(view_rot_mtx));
    glUniformMatrix4fv(prj_loc, 1, GL_FALSE, glm::value_ptr(prj_mtx));
    glUniform1f(aspect_loc, aspect);
    glUniform1f(r_loc, camera_r);
    glUniform1f(multi_loc, sun_multi);
    glUniform1f(w_decrement_loc, w_decrement);
    glUniform1ui(newtone_iters_loc, newtone_iters);
    glUniform1ui(num_divs_loc, num_divs);
    glUniform1ui(method_loc, integration_method);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, space_cubemap);
    checkOpenGLError();

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    checkOpenGLError();
}

static void reset_viewport_to_window(SDL_Window *window)
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    aspect = (float)w / (float)h;
    glViewport(0, 0, w, h);
}

static bool handle_mouse_rotation(SDL_MouseMotionEvent *motion,
                                  glm::quat *camera_rot, float multi)
{
    glm::vec3 axis(-motion->yrel, motion->xrel, 0.0f);
    float angle = glm::length(axis) * multi;
    *camera_rot = quat_from_axis_angle(axis, angle) * (*camera_rot);
    *camera_rot = glm::normalize(*camera_rot);
    return true;
}

/* Mouse wheel would make more sense, just not on the laptop */
static bool handle_mouse_zoom(SDL_MouseMotionEvent *motion,
                              float *camera_dist, float multi)
{
    *camera_dist += 0.5f * motion->yrel;
    return true;
}

static bool handle_mouse(SDL_Event *event)
{
    switch (event->type) {
    case SDL_MOUSEMOTION:
        if (event->motion.state & SDL_BUTTON_RMASK) {
            return handle_mouse_rotation(&event->motion, &camera_quat, 0.01f);
        } else if (event->motion.state & SDL_BUTTON_LMASK) {
            return handle_mouse_zoom(&event->motion, &camera_r, 0.5f);
        }
        break;
    }

    return false;
}

int main(int, char**)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::cout << "SDL could not start, error: " << SDL_GetError() << "\n";
        return -1;
    }

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL |
                                                     SDL_WINDOW_RESIZABLE |
                                                     SDL_WINDOW_ALLOW_HIGHDPI);

    SDL_Window* window = SDL_CreateWindow("Black hole", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 1280, 720,
                                          window_flags);

    auto gl_context = create_context(window);
    if (!gl_context) {
        std::cout << "SDL could not create a context, error: " << SDL_GetError() << "\n";
        return -1;
    }

    GLenum glew_ret = glewInit();
    if (glew_ret != GLEW_OK) {
        std::cout << "glew could not start, error: " << (unsigned long) glew_ret << "\n";
        return -1;
    }

    if (SDL_GL_MakeCurrent(window, gl_context)) {
        std::cout << "SDL could make context current, error: " << SDL_GetError() << "\n";
        return -1;
    }

    if (SDL_GL_SetSwapInterval(1))  /* Enable vsync */ {
        std::cout << "SDL could set swap interval, error: " << SDL_GetError() << "\n";
        return -1;
    }

    res_init();
    reset_viewport_to_window(window);

    /* Setup Dear ImGui */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsLight();
    const char* glsl_version = "#version 130";
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (!io.WantCaptureMouse)
                handle_mouse(&event);
        }

        /* Display Imgui window */
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Shader settings");
        ImGui::Text("Numerical parameters");
        ImGui::DragFloat("Root decrement", &w_decrement, 0.002f, 0.0f, 0.06f);
        ImGui::DragInt("Newtone iterations", (int *)&newtone_iters, 1, 1, 20);
        ImGui::DragInt("Integration steps", (int *)&num_divs, 5, 5, 200);
        ImGui::Text("Physical parameters");
        ImGui::DragFloat("Observer distance", &camera_r, 100.0f, 100.0f, 2000.0f);
        ImGui::DragFloat("Multiple of Sun", &sun_multi, 5.0f, 5.0f, 500.0f);
        ImGui::RadioButton("Midpoint", (int *)&integration_method, 0);
        ImGui::RadioButton("Trapezoid", (int *)&integration_method, 1);
        ImGui::RadioButton("Simpson", (int *)&integration_method, 2);
        ImGui::End();
        ImGui::Render();

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        display();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    /* Cleanup */
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}