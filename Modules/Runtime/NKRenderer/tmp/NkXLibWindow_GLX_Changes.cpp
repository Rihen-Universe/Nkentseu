// =============================================================================
// NkXLibWindow_GLX_Changes.cpp
// Ce fichier documente et implémente les seuls changements nécessaires
// dans NkWindow pour que le système graphique fonctionne correctement.
//
// SEUL CAS QUI REQUIERT UNE ADAPTATION : OpenGL/GLX sur Linux (XLib/XCB).
// Toutes les autres APIs (Vulkan, DX, Metal, EGL, Software) ne nécessitent
// AUCUN changement dans NkWindow.
//
// POURQUOI GLX EST SPÉCIAL :
//   X11 crée les fenêtres avec un "Visual" — une description du format pixel
//   du framebuffer. Pour OpenGL/GLX, ce Visual DOIT correspondre exactement
//   au GLXFBConfig choisi. Si la fenêtre est créée avec le mauvais Visual,
//   glXMakeCurrent() échoue ou produit des artefacts.
//
//   → XCreateWindow attend un XVisualInfo* issu de glXGetVisualFromFBConfig().
//   → Ce VisualInfo est injecté dans NkWindowConfig::surfaceHints
//     par NkContextFactory::PrepareWindowConfig() AVANT Create().
//
// TOUTES LES AUTRES APIs :
//   Vulkan  : crée sa VkSurfaceKHR depuis HWND/window/wl_surface APRÈS Create()
//   DX11/12 : crée son swapchain depuis HWND APRÈS Create()
//   Metal   : attache CAMetalLayer à la NSView APRÈS Create()
//   EGL     : crée son EGLSurface depuis wl_surface/ANativeWindow APRÈS Create()
//   Software: alloue son framebuffer indépendamment APRÈS Create()
// =============================================================================

// =============================================================================
// NkXLibWindow.cpp — diff des changements requis
// Seul XCreateWindow doit être modifié.
// =============================================================================

// AVANT (fenêtre XLib standard sans considération GL) :
namespace Before {
    void CreateXLibWindow_Before(Display* display, NkWindowConfig& config) {
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);

        // Visual et colormap par défaut
        Visual*      visual   = DefaultVisual(display, screen);
        int          depth    = DefaultDepth(display, screen);
        Colormap     colormap = XCreateColormap(display, root, visual, AllocNone);

        XSetWindowAttributes attrs = {};
        attrs.colormap   = colormap;
        attrs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;

        Window window = XCreateWindow(
            display, root,
            config.x, config.y, config.width, config.height,
            0,              // border width
            depth,          // ← DefaultDepth — INCOMPATIBLE avec GLX si mauvais visual
            InputOutput,
            visual,         // ← DefaultVisual — INCOMPATIBLE avec GLX
            CWColormap | CWEventMask,
            &attrs
        );
        // Problem : si le contexte OpenGL/GLX demande un Visual RGBA avec depth 24
        // mais que DefaultVisual est RGB 16-bit, glXMakeCurrent() va échouer.
    }
}

// APRÈS (avec injection du hint GLX dans NkWindowConfig::surfaceHints) :
namespace After {
    // Inclure NkSurfaceHint.h et X11/Xlib.h dans NkXLibWindow.cpp
    // Ces headers sont déjà inclus dans NkXLibWindow.cpp normalement.
    //
    // La SEULE modification est dans la partie qui choisit Visual + Colormap.

    void CreateXLibWindow_After(Display* display, NkWindowConfig& config) {
        int screen = DefaultScreen(display);
        Window root = RootWindow(display, screen);

        // ── NOUVEAU : Lire le hint GLX injecté par PrepareWindowConfig() ──────
        Visual*  visual   = nullptr;
        int      depth    = 0;
        Colormap colormap = 0;

        uintptr visualIdHint = config.surfaceHints.Get(
            NkSurfaceHintKey::NK_GLX_VISUAL_ID, 0);

        if (visualIdHint != 0) {
            // PrepareWindowConfig() a injecté un VisualID GLX — l'utiliser.
            // Retrouver le XVisualInfo correspondant via XGetVisualInfo.
            XVisualInfo templ = {};
            templ.visualid = (VisualID)visualIdHint;
            int count = 0;
            XVisualInfo* vi = XGetVisualInfo(display, VisualIDMask, &templ, &count);

            if (vi && count > 0) {
                visual   = vi->visual;
                depth    = vi->depth;
                colormap = XCreateColormap(display, root, visual, AllocNone);
                XFree(vi);
                // Stocker le colormap dans NkWindowData pour le libérer au Close()
            }
        }

        // Fallback : Visual par défaut (pour toutes les APIs non-GLX)
        if (!visual) {
            visual   = DefaultVisual(display, screen);
            depth    = DefaultDepth(display, screen);
            colormap = XCreateColormap(display, root, visual, AllocNone);
        }
        // ── FIN DU CHANGEMENT ─────────────────────────────────────────────────

        XSetWindowAttributes attrs = {};
        attrs.colormap   = colormap;
        attrs.background_pixel = 0;
        attrs.border_pixel     = 0;
        attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask
                         | ButtonPressMask | ButtonReleaseMask
                         | PointerMotionMask | StructureNotifyMask
                         | FocusChangeMask;

        Window window = XCreateWindow(
            display, root,
            config.x, config.y, config.width, config.height,
            0,
            depth,    // ← profondeur issue du VisualInfo GLX (ou default)
            InputOutput,
            visual,   // ← visual issu du VisualInfo GLX (ou default)
            CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
            &attrs
        );

        // Stocker le colormap dans NkWindowData pour libération au Close()
        // mData.colormap = colormap;  (selon ta structure NkWindowData)
        // mData.window   = window;
        // mData.display  = display;
        // mData.screen   = screen;
    }
}

// =============================================================================
// NkXCBWindow.cpp — diff des changements requis
// Même logique que XLib, mais l'API est différente.
// =============================================================================

namespace XCBChanges {
    // XCB n'a pas de notion de Visual directe dans xcb_create_window_value_list_t.
    // On passe le visual_id directement dans xcb_create_window().

    void CreateXCBWindow_After(xcb_connection_t* connection,
                               NkWindowConfig& config) {
        xcb_screen_t* screen = xcb_setup_roots_iterator(
            xcb_get_setup(connection)).data;

        uint32_t    visualId   = screen->root_visual; // défaut
        uint8_t     depth      = screen->root_depth;
        xcb_window_t root      = screen->root;

        // ── NOUVEAU : Lire le hint GLX ───────────────────────────────────────
        // Pour XCB + GLX, on utilise le bridge Xlib-xcb.
        // PrepareWindowConfig() a mis le VisualID dans NK_GLX_VISUAL_ID.
        uintptr visualIdHint = config.surfaceHints.Get(
            NkSurfaceHintKey::NK_GLX_VISUAL_ID, 0);

        if (visualIdHint != 0) {
            // Chercher la depth correspondant à ce visual dans la liste des depths
            xcb_depth_iterator_t depthIter =
                xcb_screen_allowed_depths_iterator(screen);

            bool found = false;
            while (depthIter.rem && !found) {
                xcb_visualtype_iterator_t vtIter =
                    xcb_depth_visuals_iterator(depthIter.data);
                while (vtIter.rem) {
                    if (vtIter.data->visual_id == (xcb_visualid_t)visualIdHint) {
                        visualId = vtIter.data->visual_id;
                        depth    = depthIter.data->depth;
                        found    = true;
                        break;
                    }
                    xcb_visualtype_next(&vtIter);
                }
                xcb_depth_next(&depthIter);
            }
        }
        // ── FIN DU CHANGEMENT ────────────────────────────────────────────────

        // Créer un colormap XCB avec le bon visual
        xcb_colormap_t colormap = xcb_generate_id(connection);
        xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE,
                            colormap, root, visualId);

        xcb_window_t window = xcb_generate_id(connection);

        uint32_t eventMask = XCB_EVENT_MASK_EXPOSURE
                           | XCB_EVENT_MASK_KEY_PRESS
                           | XCB_EVENT_MASK_KEY_RELEASE
                           | XCB_EVENT_MASK_BUTTON_PRESS
                           | XCB_EVENT_MASK_BUTTON_RELEASE
                           | XCB_EVENT_MASK_POINTER_MOTION
                           | XCB_EVENT_MASK_STRUCTURE_NOTIFY
                           | XCB_EVENT_MASK_FOCUS_CHANGE;

        uint32_t valueMask = XCB_CW_BACK_PIXEL
                           | XCB_CW_BORDER_PIXEL
                           | XCB_CW_EVENT_MASK
                           | XCB_CW_COLORMAP;

        uint32_t values[] = {
            screen->black_pixel,  // back pixel
            screen->black_pixel,  // border pixel
            eventMask,
            colormap
        };

        xcb_create_window(
            connection,
            depth,         // ← depth issue du visual GLX (ou root_depth)
            window,
            root,
            (int16_t)config.x, (int16_t)config.y,
            (uint16_t)config.width, (uint16_t)config.height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            visualId,      // ← visual_id issu du hint GLX (ou root_visual)
            valueMask,
            values
        );

        xcb_flush(connection);
        // mData.window     = window;
        // mData.connection = connection;
        // mData.colormap   = colormap;
        // mData.screen     = screen;
    }
}

// =============================================================================
// NkWaylandWindow.cpp, NkWin32Window.cpp, NkCocoaWindow.cpp,
// NkAndroidWindow.cpp, NkEmscriptenWindow.cpp
// → AUCUN CHANGEMENT REQUIS.
//
// Ces backends créent leur fenêtre/surface normalement.
// Les APIs graphiques correspondantes (EGL, WGL, Metal, Vulkan, DX)
// récupèrent les handles depuis NkSurfaceDesc APRÈS la création.
// =============================================================================

// =============================================================================
// RÉSUMÉ — Ce que NkXLibWindow::Create() doit faire en pratique :
// =============================================================================
/*
bool NkXLibWindow::Create(const NkWindowConfig& config) {
    mData.display = XOpenDisplay(nullptr);
    if (!mData.display) return false;

    int screen = DefaultScreen(mData.display);
    Window root = RootWindow(mData.display, screen);

    // 1. Choisir le Visual (GLX si hint présent, sinon défaut)
    Visual* visual = nullptr;
    int depth = 0;
    Colormap colormap = 0;

    uintptr visualIdHint = config.surfaceHints.Get(
        NkSurfaceHintKey::NK_GLX_VISUAL_ID, 0);

    if (visualIdHint != 0) {
        XVisualInfo templ = {};
        templ.visualid = (VisualID)visualIdHint;
        int count = 0;
        XVisualInfo* vi = XGetVisualInfo(mData.display, VisualIDMask, &templ, &count);
        if (vi && count > 0) {
            visual   = vi->visual;
            depth    = vi->depth;
            colormap = XCreateColormap(mData.display, root, visual, AllocNone);
            XFree(vi);
        }
    }
    if (!visual) {
        visual   = DefaultVisual(mData.display, screen);
        depth    = DefaultDepth(mData.display, screen);
        colormap = XCreateColormap(mData.display, root, visual, AllocNone);
    }

    // 2. Créer la fenêtre avec ce Visual
    XSetWindowAttributes attrs = {};
    attrs.colormap     = colormap;
    attrs.border_pixel = 0;
    attrs.event_mask   = ExposureMask | StructureNotifyMask | ...;

    mData.window = XCreateWindow(
        mData.display, root,
        config.x, config.y, config.width, config.height,
        0, depth, InputOutput, visual,
        CWColormap | CWBorderPixel | CWEventMask,
        &attrs
    );
    mData.colormap = colormap;

    // 3. Remplir NkSurfaceDesc (pour les contexts graphiques)
    // → Fait dans GetSurfaceDesc() — aucun changement
    // mData.screen = DefaultScreen(mData.display);

    XMapWindow(mData.display, mData.window);
    XFlush(mData.display);
    return mData.window != 0;
}
*/
