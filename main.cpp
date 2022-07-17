// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <GL/glew.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

static void printShaderLog(GLuint shader) {
    int len = 0;
    int chWrittn = 0;
    char *log;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
	    log = (char *)malloc(len);
	    glGetShaderInfoLog(shader, len, &chWrittn, log);
	    std::cout << "Shader Info Log: " << log << std::endl;
	    free(log);
    }
}

static void printProgramLog(int prog) {
    int len = 0;
    int chWrittn = 0;
    char *log;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
		log = (char *)malloc(len);
		glGetProgramInfoLog(prog, len, &chWrittn, log);
		std::cout << "Program Info Log: " << log << std::endl;
		free(log);
    }
}

static bool checkOpenGLError() {
    bool foundError = false;
    int glErr = glGetError();
    while (glErr != GL_NO_ERROR) {
		std::cout << "glError: " << glErr << std::endl;
		foundError = true;
		glErr = glGetError();
    }
    return foundError;
}

static GLenum sdl_surf_data_fmt(SDL_Surface *surf)
{
    switch (surf->format->BytesPerPixel)
    {
    case 3:
        return GL_RGB;
    case 4:
        return GL_RGBA;
    }

    std::cout << "Was not able to guess format for " <<
                 surf->format->BytesPerPixel << " bytes per pixel\n";
    return 0;
}

void fill_tex_with_image(std::string path, GLuint texture,
                         GLenum img_target)
{
    std::cout << "[TEMP] Setting slot " << img_target << " to " << path << "\n";
    SDL_Surface* surf = IMG_Load(path.c_str());
    if (!surf) {
        std::cout << "Error: " << SDL_GetError() << "\n";
        return;
    }

    GLenum data_fmt = sdl_surf_data_fmt(surf);
    if (!data_fmt) {
        SDL_FreeSurface(surf);
        return;
    }

    glTexImage2D(img_target, 0, GL_RGBA, surf->w, surf->h, 0, data_fmt,
                 GL_UNSIGNED_BYTE, surf->pixels);
    checkOpenGLError();

    SDL_FreeSurface(surf);
}

GLuint load_tex(std::string path)
{
    GLuint texture;
    
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    fill_tex_with_image(path, texture, GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    return texture;
}

//  order: X+,X-,Y+,Y-,Z+,Z-
GLuint load_cubemap(std::vector<std::string> file_paths)
{
    GLuint cubemap;

    if (file_paths.size() != 6) {
        std::cout << "6 textures must be provided for a cubemap, got " <<
                     file_paths.size() << "\n";
        return 0;
    }

    glGenTextures(1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

    for (unsigned int i = 0; i < 6; i++) {
        fill_tex_with_image(file_paths[i], cubemap, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
        checkOpenGLError();
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return cubemap;
}

GLuint read_shader(std::string shader_name, unsigned int flags)
{
    std::cout << shader_name << "\n";
    const std::string shader_dir = "shaders/";
    std::ifstream in(shader_dir + shader_name);
    std::cout << "building string\n";
    std::string code_str((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    
    //std::cout << code_str << "\n";
    std::cout << "create_shader\n";
    GLuint shader = glCreateShader(flags);
    const char *c_str = (GLchar *)code_str.c_str();
    std::cout << "shader_source\n";
    glShaderSource(shader, 1, &c_str, NULL);
    std::cout << "shader_compile\n";
    glCompileShader(shader);
    printShaderLog(shader);
    return shader;
}

GLuint create_program(std::vector<GLuint> shaders)
{
    GLuint program = glCreateProgram();
    for (auto shader : shaders) {
        glAttachShader(program, shader);
    }
    glLinkProgram(program);
    printProgramLog(program);
    return program;
}

const static int vao_num = 1;
const static int vbo_num = 3;
static GLuint prog = 0;
static GLuint prog_cm = 0;
static GLuint prog_rc = 0;
static GLuint vao[vao_num];
static GLuint vbo[vbo_num];
static GLuint cube_tex = 0;
static GLuint space_cubemap = 0;
static float aspect = 1.0f;
static float camera_pos[3] = {0.0f, 0.0f, 0.0f};
static float model_pos[3] = {0.0f, 0.0f, -6.0f};
static float rotation[3] = {0.0f, 0.0f, 0.0f};
static glm::quat camera_quat(1.0f, 0.0f, 0.0f, 0.0f);
float camera_r = 500.0f;
float sun_multi = 10.0f;

void res_init()
{
    std::cout << "Reading shaders\n";
    GLuint vs = read_shader("vs.glsl", GL_VERTEX_SHADER);
    GLuint fs = read_shader("fs.glsl", GL_FRAGMENT_SHADER);
    std::cout << "Creating program\n";
    prog = create_program({vs, fs});

    std::cout << "Reading shaders\n";
    vs = read_shader("vs_cm.glsl", GL_VERTEX_SHADER);
    fs = read_shader("fs_cm.glsl", GL_FRAGMENT_SHADER);
    std::cout << "Creating program\n";
    prog_cm = create_program({vs, fs});

    std::cout << "Reading shaders\n";
    vs = read_shader("vs_iden.glsl", GL_VERTEX_SHADER);
    fs = read_shader("fs_bh.glsl", GL_FRAGMENT_SHADER);
    std::cout << "Creating program\n";
    prog_rc = create_program({vs, fs});

    std::cout << "Generating arrays\n";
    float vertex_pos[108] = {
		-1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f,
		1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f,
		1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f
    };

    float tex_pos[72] = {
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    };

    float all_screen_vert[18] = {
        -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f
    };

    glGenVertexArrays(vao_num, vao);
    if (checkOpenGLError())
        std::cout << "glGenVertexArrays\n";
    glBindVertexArray(vao[0]);
    if (checkOpenGLError())
        std::cout << "glBindVertexArray\n";

    glGenBuffers(vbo_num, vbo);
    if (checkOpenGLError())
        std::cout << "glGenBuffers\n";
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    if (checkOpenGLError())
        std::cout << "glBindBuffer\n";
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos), vertex_pos, GL_STATIC_DRAW);
    if (checkOpenGLError())
        std::cout << "glBufferData\n";

    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    if (checkOpenGLError())
        std::cout << "glBindBuffer\n";
    glBufferData(GL_ARRAY_BUFFER, sizeof(tex_pos), tex_pos, GL_STATIC_DRAW);
    if (checkOpenGLError())
        std::cout << "glBufferData\n";

    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    if (checkOpenGLError())
        std::cout << "glBindBuffer\n";
    glBufferData(GL_ARRAY_BUFFER, sizeof(all_screen_vert), all_screen_vert, GL_STATIC_DRAW);
    if (checkOpenGLError())
        std::cout << "glBufferData\n";

    //space_tex = load_tex("ulukai/corona_ft.png");
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    cube_tex = load_tex("dark_dn.png");
    space_cubemap = load_cubemap({
                                  "ulukai/corona_rt.png",
                                  "ulukai/corona_lf.png",
                                  "ulukai/corona_up.png",
                                  "ulukai/corona_dn.png",
                                  "ulukai/corona_bk.png",
                                  "ulukai/corona_ft.png",
                                 });
    checkOpenGLError();
}

static glm::quat quat_from_axis_angle(glm::vec3 axis, float angle)
{
    angle *= 0.5f;
    float half_sin = sinf(angle);
    float half_cos = cosf(angle);

    float w = half_cos;
    axis *= half_sin;

    return glm::quat(w, axis.x, axis.y, axis.z);
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

    glm::mat4 prj_mtx = glm::perspective(1.0f, aspect, 0.1f, 1000.0f);
    glm::mat4 view_rot_mtx = glm::mat4_cast(camera_quat);

    glm::mat4 view_mtx = glm::translate(glm::vec3(0.0f, 0.0f, -camera_r)) * view_rot_mtx;

    glm::mat4 model_mtx = glm::scale(glm::vec3(0.1f, 0.1f, 0.1f));
    glm::mat4 mv_mtx = view_mtx * model_mtx;
    
    glUniformMatrix4fv(mv_loc, 1, GL_FALSE, glm::value_ptr(view_rot_mtx));
    glUniformMatrix4fv(prj_loc, 1, GL_FALSE, glm::value_ptr(prj_mtx));
    glUniform1f(aspect_loc, aspect);
    glUniform1f(r_loc, camera_r);
    glUniform1f(multi_loc, sun_multi);

    glBindVertexArray(vao[0]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

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

    glUseProgram(prog);

    mv_loc = glGetUniformLocation(prog, "mv_matrix");
    prj_loc = glGetUniformLocation(prog, "proj_matrix");

    glUniformMatrix4fv(mv_loc, 1, GL_FALSE, glm::value_ptr(mv_mtx));
    glUniformMatrix4fv(prj_loc, 1, GL_FALSE, glm::value_ptr(prj_mtx));

    glBindVertexArray(vao[0]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, cube_tex);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // glUseProgram(prog_cm);
    // mv_loc = glGetUniformLocation(prog_cm, "mv_matrix");
    // prj_loc = glGetUniformLocation(prog_cm, "proj_matrix");
    // glUniformMatrix4fv(mv_loc, 1, GL_FALSE, glm::value_ptr(view_rot_mtx));
    // glUniformMatrix4fv(prj_loc, 1, GL_FALSE, glm::value_ptr(prj_mtx));

    // glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    // glEnableVertexAttribArray(0);

    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_CUBE_MAP, space_cubemap);

    // checkOpenGLError();

    // glEnable(GL_CULL_FACE);
    // glCullFace(GL_FRONT);
    // glFrontFace(GL_CW);
    // glDrawArrays(GL_TRIANGLES, 0, 36);

    checkOpenGLError();
}

static bool handle_mouse(SDL_Event *event)
{
    switch (event->type) {
    case SDL_MOUSEMOTION:
        if (event->motion.state & SDL_BUTTON_RMASK) {
            glm::vec3 axis(-event->motion.yrel, event->motion.xrel, 0.0f);
            float angle = glm::length(axis) * 0.01f;
            std::cout << "[TEMP] rotating on angle: " << angle << "\n";
            camera_quat = quat_from_axis_angle(axis, angle) * camera_quat;
            camera_quat = glm::normalize(camera_quat);
            return true;
        } else if (event->motion.state & SDL_BUTTON_LMASK) {
            camera_r += 0.5f * event->motion.yrel;
            return true;
        }
        break;
    }

    return false;
}

// Main code
int main(int, char**)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // Create window with graphics context
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    if (glewInit() != GLEW_OK) {
        std::cout << "glew: " << (unsigned long) glewInit << "\n";
        return 1;
    }
    res_init();
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (!io.WantCaptureMouse)
                handle_mouse(&event);
        }
        

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        ImGui::DragFloat3("cube", model_pos, 0.4f, -10.0f, 10.f);
        ImGui::DragFloat3("camera", rotation, 0.05f, -glm::pi<float>(), glm::pi<float>());
        ImGui::End();

        // Rendering
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        aspect = (float)w / (float)h;
        glViewport(0, 0, w, h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        display();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
