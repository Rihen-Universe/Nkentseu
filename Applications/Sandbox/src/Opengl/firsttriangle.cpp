// =============================================================================
// main8.cpp
// Validation manette: layout type PlayStation 3 en rectangles.
// Chaque bouton allume son rectangle. Les sticks affichent aussi leur position.
// =============================================================================

#include "NKWindow/NkWindow.h"
#include "NKWindow/Core/NkMain.h"

#include "NKLogger/NkLog.h"

#include <glad/wgl.h>
#include <glad/gl.h>

#if defined(Bool)
    #undef Bool
#endif

#include "NKContext/Factory/NkContextFactory.h"
#include "NKContext/Core/NkContextDesc.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Core/NkOpenGLDesc.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
    
using namespace nkentseu;

static bool LoadGL(NkIGraphicsContext* ctx) {
    auto loader = NkNativeContext::GetOpenGLProcAddressLoader(ctx);
    if (!loader) { 
        logger.Info("[RHIDemo] OpenGL loader introuvable\n"); 
        return false; 
    }

    int ver = gladLoadGL((GLADloadfunc)loader);
    
    if (!ver) { 
        logger.Error("[RHIDemo] gladLoadGL échoué\n"); 
        return false; 
    }
    logger.Infof("[RHIDemo] OpenGL %s  GLSL %s\n",
    glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    return true;
}


int nkmain(const nkentseu::NkEntryState& /*state*/) {
    
    NkWindowConfig cfg;
    cfg.title = "First Triangle";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.centered = true;
    cfg.resizable = true;
    cfg.dropEnabled = false;

    NkWindow window;
    
    if (!window.Create(cfg)) {
        logger.Error("La fenetre n'a pas ete cree");
        return -2;
    }

    NkContextDesc desc;
    desc.api    = NkGraphicsApi::NK_GFX_API_OPENGL;
    desc.opengl.majorVersion = 4;
    desc.opengl.minorVersion = 6;
    desc.opengl.profile = NkGLProfile::Core;
    desc.opengl.contextFlags = NkGLContextFlags::ForwardCompat | NkGLContextFlags::Debug;
    desc.opengl.runtime.installDebugCallback = true;
    desc.opengl.msaaSamples        = 4;
    desc.opengl.srgbFramebuffer    = true;
    desc.opengl.swapInterval       = NkGLSwapInterval::AdaptiveVSync;
    desc.opengl.runtime.autoLoadEntryPoints  = false;
    desc.opengl.runtime.validateVersion      = true;

    auto ctx = NkContextFactory::Create(window, desc);
    if (!ctx)  { 
        logger.Error("[RHIDemo] Contexte GL échoué"); 
        window.Close(); 
        return -3; 
    }
    if (!LoadGL(ctx)) { 
        ctx->Shutdown(); 
        window.Close(); 
        return -4; 
    }

    // ------------------------------- data opengl ---------------------------------------
    float vertices[] = {
        -0.5f, 0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f
    };

    const char *vertexShaderSource = "#version 460 core\n"
"layout (location = 0) in vec3 aPos;\n"
"void main()\n"
"{\n"
" gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
"}\0";

    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        logger.Error("ERROR::SHADER::VERTEX::COMPILATION_FAILED\n{0}", infoLog);
    }


    const char *fragmentShaderSource = "#version 460 core\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
"}\0";

    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        logger.Error("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n{0}", infoLog);
    }

    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        logger.Error("ERROR::SHADER::PORGRAMME::COMPILATION_FAILED\n{0}", infoLog);
    }

    glUseProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // vao
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    //    -- vbo
    unsigned int VBO;
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 1. then set the vertex attributes pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // ------------------------------------ end data opengl
    
    bool running = true;
    auto& events = NkEvents();

    while (running) {
        while (NkEvent* ev = events.PollEvent()) {
            if (ev->Is<NkWindowCloseEvent>()) {
                running = false;
                break;
            }
        }

        if (!running) {
            break;
        }

        if (ctx->BeginFrame()) {
            glViewport(0, 0, window.GetSize().width, window.GetSize().height);
            glClearColor(128.0f / 255.0f, 200.0f / 255.0f, 255.0f/255.0f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(shaderProgram);
            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            ctx->EndFrame();
            ctx->Present();
        }
    }

    ctx->Shutdown();
    window.Close();

    return 0;
}
