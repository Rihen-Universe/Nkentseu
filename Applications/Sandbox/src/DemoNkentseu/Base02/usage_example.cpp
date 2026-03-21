// =============================================================================
// usage_example.cpp
// Démonstrations : suffisance du NkSurfaceDesc + multi-renderer
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// CAS 1 : OpenGL sur Windows (WGL) — le cas le plus simple
// PrepareWindowConfig = no-op, aucune contrainte sur HWND avant création.
// ─────────────────────────────────────────────────────────────────────────────
void ExampleOpenGL_Win32() {
    // NkSurfaceDesc fournit : hwnd, hinstance, width, height, dpi
    // Le contexte OpenGL fait GetDC(hwnd) lui-même → suffisant.

    NkWindowConfig wcfg;
    wcfg.title = "OpenGL Win32"; wcfg.width = 1280; wcfg.height = 720;

    auto gcfg = NkGraphicsContextConfig::ForOpenGL(4, 6, /*debug=*/true);

    // PrepareWindowConfig = no-op sur Win32
    NkGraphicsContextFactory::PrepareWindowConfig(wcfg, gcfg);

    NkWindow window(wcfg);
    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);

    // Récupérer les handles WGL pour initialiser glad :
    auto* gl = ctx->GetNativeHandle<NkOpenGLNativeHandles>();
    // gl->getProcAddress = wglGetProcAddress (rempli par le contexte)
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(gl->getProcAddress));

    // Boucle principale
    NkFrameContext frame;
    while (window.IsOpen()) {
        NkEvents().PollEvents();
        if (!ctx->BeginFrame(frame)) {
            ctx->Recreate(window.GetSize().x, window.GetSize().y);
            continue;
        }
        glClearColor(0.1f, 0.1f, 0.2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // ... rendu ...
        ctx->EndFrame(frame);
        ctx->Present(frame);  // SwapBuffers interne
    }
    ctx->Shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// CAS 2 : OpenGL sur Linux XLib (GLX)
// PrepareWindowConfig est NÉCESSAIRE pour que XCreateWindow utilise
// la bonne XVisualInfo (sinon le contexte GL échoue à la création).
// ─────────────────────────────────────────────────────────────────────────────
void ExampleOpenGL_XLib() {
    auto gcfg = NkGraphicsContextConfig::ForOpenGL(4, 6, true);

    NkWindowConfig wcfg;
    wcfg.title = "OpenGL XLib"; wcfg.width = 1280; wcfg.height = 720;

    // ← ÉTAPE 1 : injecter GlxVisualId dans wcfg.surfaceHints
    //   NkWindow ne sait pas que c'est pour OpenGL.
    //   Il voit juste un hint opaque { GlxVisualId, <visualid> }.
    NkGraphicsContextFactory::PrepareWindowConfig(wcfg, gcfg);

    // ← ÉTAPE 2 : créer la fenêtre avec la bonne Visual
    NkWindow window(wcfg);

    // ← ÉTAPE 3 : créer le contexte
    //   NkSurfaceDesc fournit : display, window(XID), screen, appliedHints(GlxFBConfigPtr)
    //   Le contexte GLX utilise appliedHints.Get(GlxFBConfigPtr) pour
    //   glXCreateContextAttribsARB — pas besoin de refaire glXChooseFBConfig.
    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);

    auto* gl = ctx->GetNativeHandle<NkOpenGLNativeHandles>();
    // gl->display, gl->drawable, gl->glxContext disponibles
    // gl->getProcAddress = (void*(*)(const char*)) glXGetProcAddressARB
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(gl->getProcAddress));

    // Boucle identique au cas Win32
    NkFrameContext frame;
    while (window.IsOpen()) { /* ... */ }
    ctx->Shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// CAS 3 : Vulkan — aucune préparation nécessaire
// NkSurfaceDesc fournit exactement ce qu'il faut pour vkCreate*SurfaceKHR
// ─────────────────────────────────────────────────────────────────────────────
void ExampleVulkan() {
    NkWindowConfig wcfg;
    wcfg.title = "Vulkan"; wcfg.width = 1920; wcfg.height = 1080;

    // Aucun PrepareWindowConfig — Vulkan n'a pas de contrainte pré-création
    NkWindow window(wcfg);

    auto gcfg = NkGraphicsContextConfig::ForVulkan(/*validation=*/true);
    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);

    auto* vk = ctx->GetNativeHandle<NkVulkanNativeHandles>();
    // vk->instance     → VkInstance    (pour vkEnumeratePhysicalDevices etc.)
    // vk->device       → VkDevice      (pour vkCreateBuffer, vkCreateImage etc.)
    // vk->swapchain    → VkSwapchainKHR
    // vk->renderPass   → VkRenderPass principal
    // vk->commandPool  → pour allouer des VkCommandBuffer supplémentaires
    // vk->commandBuffer→ VkCommandBuffer de la frame courante (après BeginFrame)

    NkFrameContext frame;
    while (window.IsOpen()) {
        NkEvents().PollEvents();
        if (!ctx->BeginFrame(frame)) {
            ctx->Recreate(window.GetSize().x, window.GetSize().y);
            continue;
        }
        // frame.commandBuffer = VkCommandBuffer* prêt à enregistrer
        VkCommandBuffer cmd = *reinterpret_cast<VkCommandBuffer*>(frame.commandBuffer);
        // vkCmdDraw(cmd, ...);
        ctx->EndFrame(frame);
        ctx->Present(frame);
    }
    ctx->Shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// CAS 4 : MULTI-RENDERER sur un seul contexte OpenGL
// Réponse à la question : "peut-on avoir plusieurs renderers différents
// associés à un seul backend ?"  → OUI.
// ─────────────────────────────────────────────────────────────────────────────

// Exemple de renderer 2D
class Renderer2D {
public:
    explicit Renderer2D(NkGraphicsContext& ctx) {
        auto* gl = ctx.GetNativeHandle<NkOpenGLNativeHandles>();
        assert(gl && "Renderer2D requiert OpenGL");
        // Créer VAO/VBO/shader sprite ici — ressources propres à ce renderer
        InitSpritePipeline();
    }
    void DrawSprite(float x, float y, float w, float h, uint32 textureId) { /* ... */ }
    void DrawText  (const char* text, float x, float y)                   { /* ... */ }
    void Flush     (const NkFrameContext&)                                 { /* ... */ }
private:
    void InitSpritePipeline() { /* glGenVertexArrays, glCreateProgram… */ }
    uint32 mVAO = 0, mVBO = 0, mShader = 0;
};

// Exemple de renderer 3D
class Renderer3D {
public:
    explicit Renderer3D(NkGraphicsContext& ctx) {
        auto* gl = ctx.GetNativeHandle<NkOpenGLNativeHandles>();
        assert(gl && "Renderer3D requiert OpenGL");
        // Créer UBOs, FBOs, shadow maps — ressources propres à ce renderer
        InitPBRPipeline();
    }
    void RenderScene(/* scene graph */) { /* ... */ }
    void Flush(const NkFrameContext&)   { /* ... */ }
private:
    void InitPBRPipeline() { /* shaders PBR, FBO HDR… */ }
    uint32 mHDRFBO = 0, mShadowMap = 0;
};

// Renderer de debug (wireframe, AABB, gizmos)
class RendererDebug {
public:
    explicit RendererDebug(NkGraphicsContext& ctx) {
        auto* gl = ctx.GetNativeHandle<NkOpenGLNativeHandles>();
        assert(gl);
        InitLinePipeline();
    }
    void DrawAABB  (/* ... */) { /* ... */ }
    void DrawGrid  (/* ... */) { /* ... */ }
    void Flush     (const NkFrameContext&) { /* ... */ }
private:
    void InitLinePipeline() { /* GL_LINES shader */ }
    uint32 mVAO = 0, mShader = 0;
};

void ExampleMultiRenderer() {
    auto gcfg = NkGraphicsContextConfig::ForOpenGL(4, 6);
    NkWindowConfig wcfg; wcfg.title = "Multi-Renderer"; wcfg.width = 1280;

    NkGraphicsContextFactory::PrepareWindowConfig(wcfg, gcfg); // Linux: injecte GLX hint
    NkWindow window(wcfg);

    // ── Un seul contexte ────────────────────────────────────────────────────
    auto ctx = NkGraphicsContextFactory::Create(window, gcfg);

    // ── Plusieurs renderers indépendants, tous sur le même ctx ─────────────
    Renderer3D   r3d(*ctx);   // ressources 3D propres
    Renderer2D   r2d(*ctx);   // ressources 2D propres
    RendererDebug dbg(*ctx);  // ressources debug propres

    // Tous partagent le même GL context courant.
    // L'ordre de rendu est contrôlé par l'application, pas par NkWindow.

    NkFrameContext frame;
    while (window.IsOpen()) {
        NkEvents().PollEvents();

        if (!ctx->BeginFrame(frame)) {
            ctx->Recreate(window.GetSize().x, window.GetSize().y);
            continue;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        r3d.RenderScene(/* scene */);   // ← 1. scène 3D
        dbg.DrawGrid(/* ... */);        // ← 2. gizmos par-dessus
        r2d.DrawSprite(/* ... */);      // ← 3. UI/HUD par-dessus tout
        r2d.DrawText("FPS: 60", 10, 10);

        // Flush dans l'ordre
        r3d.Flush(frame);
        dbg.Flush(frame);
        r2d.Flush(frame);

        ctx->EndFrame(frame);
        ctx->Present(frame);
    }

    // Destruction dans l'ordre inverse de création
    // (les renderers sont détruits avant le contexte)
    ctx->Shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// CAS 5 : Software renderer — NkSurfaceDesc minimal
// Le contexte software alloue son propre framebuffer CPU.
// NkSurfaceDesc n'a besoin que de width/height + le handle pour blitter.
// ─────────────────────────────────────────────────────────────────────────────
void ExampleSoftware() {
    NkWindowConfig wcfg; wcfg.title = "Software"; wcfg.width = 800; wcfg.height = 600;
    NkWindow window(wcfg);

    auto ctx = NkGraphicsContextFactory::Create(window,
                   NkGraphicsContextConfig::ForSoftware());

    auto* sw = ctx->GetNativeHandle<NkSoftwareNativeHandles>();
    // sw->pixels : buffer RGBA8 (width * height * 4 bytes)
    // Dessiner directement : sw->pixels[y * sw->stride/4 + x] = 0xFF0000FF;
    // ctx->Present() blitte ce buffer vers la fenêtre (Win32: BitBlt,
    //   XLib: XPutImage, Wayland: wl_buffer, etc.)

    NkFrameContext frame;
    while (window.IsOpen()) {
        NkEvents().PollEvents();
        ctx->BeginFrame(frame);

        // Rendu software pur
        uint32_t* pixels = static_cast<uint32_t*>(sw->pixels);
        for (uint32 y = 0; y < sw->height; ++y)
            for (uint32 x = 0; x < sw->width; ++x)
                pixels[y * (sw->stride / 4) + x] = 0xFF202040; // fond foncé

        ctx->EndFrame(frame);
        ctx->Present(frame);  // blitte sw->pixels → fenêtre
    }
    ctx->Shutdown();
}