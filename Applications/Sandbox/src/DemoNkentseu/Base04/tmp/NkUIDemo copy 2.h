#pragma once
/**
 * @File    NkUIDemo.h
 * @Brief   Démo complète NkUI — tous les widgets, panels 3D, graphiques, etc.
 *
 * Usage :
 *   #include "NkUIDemo.h"
 *   nkentseu::NkUIDemo demo;
 *   demo.Init(ctx, wm, dock, fontMgr);
 *   // Par frame :
 *   demo.Render(ctx, wm, dock, ls, dt);
 */
#include "NKUI/NkUI.h"
#include "NKUI/NkUILayout2.h"
#include "NKLogger/NkLog.h"
#include "NkUIFontIntegration.h"

// RHI (optionnel — définir NKUIDEMO_WITH_RHI pour activer les panels 3D)
#ifdef NKUIDEMO_WITH_RHI
#  include "NKRHI/Core/NkIDevice.h"
#  include "NKRHI/Commands/NkICommandBuffer.h"
#endif

#include "NKFont/NKFont.h"

#include <cstring>
#include <cstdio>
#include <cmath>

namespace nkentseu {
    namespace nkui {

        // =============================================================================
        // Structures de données internes à la démo
        // =============================================================================

        // État du panel Widgets
        struct NkUIDemoWidgets {
            bool     checkA       = true;
            bool     checkB       = false;
            bool     toggleA      = true;
            int32    radioSel     = 0;
            float32  sliderF      = 0.42f;
            int32    sliderI      = 7;
            float32  sliderV2[2]  = {0.3f, 0.7f};
            float32  dragF        = 1.23f;
            int32    dragI        = 42;
            char     inputStr[256]= "Bonjour NkUI !";
            int32    inputInt     = 100;
            float32  inputFlt     = 3.14159f;
            char     multiline[1024] = "Ligne 1\nLigne 2\nLigne 3\n";
            int32    comboSel     = 2;
            int32    listSel      = 0;
            float32  progress     = 0.65f;
            bool     progressAnim = true;
            NkColor  pickedColor  = {80, 140, 255, 255};
            bool     treeOpen[4]  = {true, false, false, false};
        };

        // État du panel Graphiques
        struct NkUIDemoCharts {
            static constexpr int32 kSamples = 128;
            float32 sinData[kSamples]   = {};
            float32 cosData[kSamples]   = {};
            float32 noiseData[kSamples] = {};
            float32 barData[8]          = {0.7f, 0.4f, 0.9f, 0.55f, 0.8f, 0.35f, 0.6f, 0.95f};
            float32 phase               = 0.f;
            float32 scrollX             = 0.f;
            bool    animating           = true;

            void Update(float32 dt) {
                if (!animating) return;
                phase += dt * 1.5f;
                for (int32 i = 0; i < kSamples; ++i) {
                    const float32 t = (float32)i / kSamples;
                    sinData[i]   = 0.5f + 0.45f * ::sinf(t * NKUI_PI * 4.f + phase);
                    cosData[i]   = 0.5f + 0.45f * ::cosf(t * NKUI_PI * 3.f + phase * 0.7f);
                    noiseData[i] = 0.5f + 0.45f * ::sinf(t * NKUI_PI * 8.f + phase * 1.3f)
                                        * ::cosf(t * NKUI_PI * 5.f + phase * 0.5f);
                }
                // Bars
                for (int32 i = 0; i < 8; ++i)
                    barData[i] = 0.5f + 0.45f * ::sinf(phase * (1.f + i * 0.3f));
            }

            void Init() { Update(0.f); }
        };

        // État du panel 3D Viewport
        struct NkUIDemoViewport {
            float32 rotX      = 20.f;
            float32 rotY      = 0.f;
            float32 camDist   = 4.f;
            float32 fov       = 60.f;
            bool    wireframe = false;
            bool    showGrid  = true;
            bool    rotating  = true;
            int32   meshType  = 0; // 0=cube, 1=sphere, 2=torus

            // Rendu software (preview du mesh)
            struct PreviewMesh {
                struct Tri { float32 v[3][3]; float32 n[3]; };
                static constexpr int32 kMaxTris = 2048;
                Tri  tris[kMaxTris];
                int32 numTris = 0;
            } mesh;

            void BuildCube(PreviewMesh& m);
            void BuildSphere(PreviewMesh& m, int32 stacks=12, int32 slices=18);
            void BuildTorus(PreviewMesh& m, float32 R=0.6f, float32 r=0.25f,
                            int32 rings=24, int32 sides=16);
            void RebuildMesh();
        };

        // État du panel Textures
        struct NkUIDemoTextures {
            static constexpr int32 kAtlasW = 256;
            static constexpr int32 kAtlasH = 256;
            uint8    pixels[kAtlasW * kAtlasH * 4] = {};
            uint32   texId    = 0;      // handle GPU/CPU
            bool     uploaded = false;
            int32    pattern  = 0;      // 0=checker, 1=gradient, 2=noise, 3=mandelbrot
            float32  zoom     = 1.f;
            float32  panX     = 0.f;
            float32  panY     = 0.f;

            void GeneratePattern(int32 p, float32 t = 0.f);
            void GenerateChecker();
            void GenerateGradient(float32 t);
            void GenerateNoise(float32 t);
            void GenerateMandelbrot();
        };

        // État du panel Console
        struct NkUIDemoConsole {
            static constexpr int32 kMaxLines = 256;
            char     lines[kMaxLines][128]= {};
            int32    numLines  = 0;
            char     inputBuf[256] = {};
            float32  scrollY   = 0.f;
            bool     autoScroll= true;

            void AddLine(const char* fmt, ...);
            void Clear() { numLines = 0; scrollY = 0.f; }
            void ExecCommand(const char* cmd);
        };

        // État du panel Style
        struct NkUIDemoStyle {
            int32   themeIdx   = 1; // 0=Default, 1=Dark, 2=Minimal, 3=HighContrast
            float32 cornerR    = 5.f;
            float32 spacing    = 6.f;
            float32 padX       = 10.f;
            float32 padY       = 6.f;
            float32 itemH      = 28.f;
            float32 borderW    = 1.f;
            float32 globalAlpha= 1.f;
            bool    animations = true;
        };

        // État du panel Performances
        struct NkUIDemoPerfMonitor {
            static constexpr int32 kHistory = 120;
            float32 fps[kHistory]       = {};
            float32 frameMs[kHistory]   = {};
            float32 drawCalls[kHistory] = {};
            int32   head                = 0;
            uint32  totalVtx            = 0;
            uint32  totalIdx            = 0;
            uint32  totalCmds           = 0;
            float32 avgFps              = 0.f;

            void Record(float32 dt, const NkUIContext& ctx);
        };

        // =============================================================================
        // NkUIDemo — classe principale
        // =============================================================================
        class NkUIDemo {
            public:
                NkUIDemo() = default;
                // ── Cycle de vie ─────────────────────────────────────────────────────────
                bool Init(NkUIContext& ctx,
                        NkUIWindowManager& wm,
                        NkUIFontLoader& fontLoader);
                void Destroy();

                /// Appeler chaque frame. dt en secondes.
                void Render(NkUIContext& ctx,
                            NkUIWindowManager& wm,
                            NkUIDockManager& dock,
                            NkUILayoutStack& ls,
                            float32 dt);

                // Accès aux sous-états
                NkUIDemoWidgets&     Widgets()     { return mWidgets; }
                NkUIDemoCharts&      Charts()      { return mCharts; }
                NkUIDemoViewport&    Viewport()    { return mViewport; }
                NkUIDemoTextures&    Textures()    { return mTextures; }
                NkUIDemoConsole&     Console()     { return mConsole; }

            private:
                // ── Panels ───────────────────────────────────────────────────────────────
                void RenderMainMenuBar (NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font);
                void RenderDockspace   (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUIDockManager& dock, NkUILayoutStack& ls,
                                        NkUIFont& font);

                // Fenêtres individuelles
                void WinWidgets        (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinCharts         (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinViewport3D     (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinTextures       (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinColorPicker    (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinStyle          (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinConsole        (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinPerf           (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinAbout          (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinDemoWidgets2   (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);
                void WinScene3D        (NkUIContext& ctx, NkUIWindowManager& wm,
                                        NkUILayoutStack& ls, NkUIFont& font);

                // ── Helpers de rendu custom ───────────────────────────────────────────────
                void DrawLineChart     (NkUIDrawList& dl, NkRect r,
                                        const float32* data, int32 n,
                                        NkColor col, float32 thickness = 1.5f,
                                        bool filled = false);
                void DrawBarChart      (NkUIDrawList& dl, NkRect r,
                                        const float32* data, int32 n, NkColor col);
                void DrawViewport3D    (NkUIDrawList& dl, NkRect r,
                                        NkUIContext& ctx, float32 dt);
                void DrawTexturePreview(NkUIDrawList& dl, NkRect r,
                                        const NkUIDemoTextures& tex);
                void DrawGrid          (NkUIDrawList& dl, NkRect r, NkColor col, int32 div=10);
                void DrawNodeGraph     (NkUIDrawList& dl, NkRect r, NkUIContext& ctx);
                void DrawGizmo         (NkUIDrawList& dl, NkVec2 center, float32 size,
                                        float32 rx, float32 ry);
                void DrawScatterPlot   (NkUIDrawList& dl, NkRect r,
                                        NkUIContext& ctx, float32 t);
                void DrawRadarChart    (NkUIDrawList& dl, NkRect r, const float32* vals, int32 n);
                void DrawHeatmap       (NkUIDrawList& dl, NkRect r, NkUIContext& ctx);
                void DrawSpectrum      (NkUIDrawList& dl, NkRect r, float32 phase);
                void DrawMiniViewport  (NkUIDrawList& dl, NkRect r, NkUIContext& ctx,
                                        float32 rx, float32 ry);

                // ── Rendu 3D software ────────────────────────────────────────────────────
                struct Pixel { uint8 r,g,b,a; };
                // using Pixel = NkColor;
                void SoftRender3D      (Pixel* fb, int32 w, int32 h,
                                        float32 rx, float32 ry,
                                        int32 meshType, bool wireframe, bool grid);
                // Projection
                struct Vec3  { float32 x,y,z; };
                struct Vec4  { float32 x,y,z,w; };
                static Vec3  Cross     (Vec3 a, Vec3 b);
                static float32 Dot     (Vec3 a, Vec3 b);
                static Vec3  Normalize (Vec3 v);
                static Vec4  MulMV     (const float32 m[16], Vec3 v);

                // ── État interne ──────────────────────────────────────────────────────────
                NkUIFont*           mFont        = nullptr;
                NkUIDemoWidgets     mWidgets;
                NkUIDemoCharts      mCharts;
                NkUIDemoViewport    mViewport;
                NkUIDemoTextures    mTextures;
                NkUIDemoConsole     mConsole;
                NkUIDemoStyle       mStyle;
                NkUIDemoPerfMonitor mPerf;

                // Fenêtres ouvertes
                bool mShowWidgets   = true;
                bool mShowCharts    = true;
                bool mShowViewport  = true;
                bool mShowTextures  = true;
                bool mShowColor     = true;
                bool mShowStyle     = true;
                bool mShowConsole   = true;
                bool mShowPerf      = true;
                bool mShowAbout     = false;
                bool mShowWidgets2  = true;
                bool mShowScene3D   = true;
                bool mShowMenuBar   = true;

                // Framebuffer software pour le viewport 3D
                static constexpr int32 kFBW = 320;
                static constexpr int32 kFBH = 240;
                Pixel   mFB[kFBW * kFBH]     = {};
                float32 mZBuf[kFBW * kFBH]   = {};
                uint32  mFBTexId              = 0; // handle texture (simulé pour CPU renderer)

                // Viewport 3D state
                float32 mVPRotX = 20.f;
                float32 mVPRotY = 0.f;
                bool    mVPDragging = false;

                // Node graph state
                struct Node {
                    NkVec2 pos;
                    const char* name;
                    NkColor color;
                };
                static constexpr int32 kNumNodes = 6;
                Node mNodes[kNumNodes] = {
                    {{50,80},  "Texture",   {80,200,120,255}},
                    {{200,40}, "Normal Map",{80,140,255,255}},
                    {{200,130},"Roughness", {255,160,60,255}},
                    {{360,80}, "PBR Mix",   {200,80,200,255}},
                    {{510,60}, "Emissive",  {255,220,50,255}},
                    {{510,140},"Output",    {60,200,200,255}},
                };
                int32   mDragNodeIdx = -1;
                NkVec2  mDragOffset  = {};

                float32 mTime = 0.f;

                // Modal / popup
                bool    mShowSaveModal   = false;
                bool    mShowOpenModal   = false;
                char    mSavePathBuf[256] = "layout.json";

                NkUIAnimator mAnimator;

                // Pour les sliders de couleur dans style panel
                // NkColorPickerFull::State* dummy = nullptr;

                NkUIFontLoader mFontLoader;

            public:

                NkUIFontLoader& GetFontLoader() {
                    return mFontLoader;
                }
        };

        // =============================================================================
        // IMPLÉMENTATION
        // =============================================================================

        // ── NkUIDemoConsole ──────────────────────────────────────────────────────────

        inline void NkUIDemoConsole::AddLine(const char* fmt, ...) {
            if (numLines >= kMaxLines) {
                // Décale
                for (int32 i = 0; i < kMaxLines - 1; ++i)
                    ::memcpy(lines[i], lines[i+1], 128);
                numLines = kMaxLines - 1;
            }
            va_list ap;
            va_start(ap, fmt);
            ::vsnprintf(lines[numLines++], 128, fmt, ap);
            va_end(ap);
        }

        inline void NkUIDemoConsole::ExecCommand(const char* cmd) {
            AddLine("> %s", cmd);
            if (::strcmp(cmd, "clear") == 0)      { Clear(); AddLine("Console cleared."); }
            else if (::strcmp(cmd, "help") == 0)  { AddLine("Commands: clear, help, version, demo"); }
            else if (::strcmp(cmd, "version")==0) { AddLine("NkUI v1.0 — C++17 Immediate-mode UI"); }
            else if (::strcmp(cmd, "demo")==0)    { AddLine("Demo mode: all panels active!"); }
            else { AddLine("Unknown command: %s", cmd); }
        }

        // ── NkUIDemoTextures ─────────────────────────────────────────────────────────

        inline void NkUIDemoTextures::GenerateChecker() {
            for (int32 y = 0; y < kAtlasH; ++y)
                for (int32 x = 0; x < kAtlasW; ++x) {
                    const int32 tile = ((x/32) + (y/32)) & 1;
                    const uint8 c = tile ? 220 : 40;
                    uint8* p = pixels + (y*kAtlasW + x)*4;
                    p[0]=c; p[1]=c; p[2]=c; p[3]=255;
                }
        }

        inline void NkUIDemoTextures::GenerateGradient(float32 t) {
            for (int32 y = 0; y < kAtlasH; ++y)
                for (int32 x = 0; x < kAtlasW; ++x) {
                    float32 fx = (float32)x/kAtlasW;
                    float32 fy = (float32)y/kAtlasH;
                    float32 r = ::fabsf(::sinf(fx*NKUI_PI + t));
                    float32 g = ::fabsf(::sinf(fy*NKUI_PI*2.f + t*0.7f));
                    float32 b = ::fabsf(::cosf((fx+fy)*NKUI_PI + t*1.3f));
                    uint8* p = pixels + (y*kAtlasW + x)*4;
                    p[0]=(uint8)(r*255); p[1]=(uint8)(g*255);
                    p[2]=(uint8)(b*255); p[3]=255;
                }
        }

        inline void NkUIDemoTextures::GenerateNoise(float32 t) {
            // Perlin-like avec sines
            for (int32 y = 0; y < kAtlasH; ++y)
                for (int32 x = 0; x < kAtlasW; ++x) {
                    float32 fx = (float32)x/kAtlasW;
                    float32 fy = (float32)y/kAtlasH;
                    float32 v = 0.5f
                        + 0.25f*::sinf(fx*8.f*NKUI_PI + t)
                        * ::cosf(fy*6.f*NKUI_PI + t*0.8f)
                        + 0.15f*::sinf((fx+fy)*12.f*NKUI_PI + t*1.5f)
                        + 0.1f *::cosf(fx*20.f*NKUI_PI - fy*15.f*NKUI_PI);
                    v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
                    const uint8 c = (uint8)(v*255);
                    uint8* p = pixels + (y*kAtlasW + x)*4;
                    // Couleur turquoise noise
                    p[0]=(uint8)(v*80);  p[1]=(uint8)(v*200);
                    p[2]=(uint8)(v*220); p[3]=255;
                }
        }

        inline void NkUIDemoTextures::GenerateMandelbrot() {
            for (int32 py = 0; py < kAtlasH; ++py)
                for (int32 px = 0; px < kAtlasW; ++px) {
                    float32 cr = (float32)px/kAtlasW * 3.5f - 2.5f + panX;
                    float32 ci = (float32)py/kAtlasH * 2.0f - 1.0f + panY;
                    float32 zr=0.f, zi=0.f;
                    int32 n = 0;
                    while (zr*zr+zi*zi < 4.f && n < 64) {
                        float32 tmp = zr*zr - zi*zi + cr;
                        zi = 2.f*zr*zi + ci;
                        zr = tmp;
                        ++n;
                    }
                    const float32 t = (float32)n / 64.f;
                    uint8* p = pixels + (py*kAtlasW + px)*4;
                    p[0] = (uint8)(9.f  * (1-t)*t*t*t * 255);
                    p[1] = (uint8)(15.f * (1-t)*(1-t)*t*t * 255);
                    p[2] = (uint8)(8.5f * (1-t)*(1-t)*(1-t)*t * 255);
                    p[3] = 255;
                }
        }

        inline void NkUIDemoTextures::GeneratePattern(int32 p, float32 t) {
            pattern = p;
            switch(p) {
                case 0: GenerateChecker(); break;
                case 1: GenerateGradient(t); break;
                case 2: GenerateNoise(t); break;
                case 3: GenerateMandelbrot(); break;
                default: GenerateChecker(); break;
            }
            uploaded = false; // marque comme sale
        }

        // ── NkUIDemoPerfMonitor ──────────────────────────────────────────────────────

        inline void NkUIDemoPerfMonitor::Record(float32 dt, const NkUIContext& ctx) {
            fps[head]     = dt > 0.f ? 1.f/dt : 0.f;
            frameMs[head] = dt * 1000.f;
            head = (head + 1) % kHistory;

            // Compte vtx/idx/cmd des DrawLists
            totalVtx = totalIdx = totalCmds = 0;
            for (int32 i = 0; i < NkUIContext::LAYER_COUNT; ++i) {
                totalVtx  += ctx.layers[i].vtxCount;
                totalIdx  += ctx.layers[i].idxCount;
                totalCmds += ctx.layers[i].cmdCount;
            }
            // Moyenne fps
            float32 sum = 0.f;
            for (int32 i = 0; i < kHistory; ++i) sum += fps[i];
            avgFps = sum / kHistory;
        }

        // ── NkUIDemo::Init ───────────────────────────────────────────────────────────

inline bool NkUIDemo::Init(NkUIContext& ctx,
                            NkUIWindowManager& wm,
                            NkUIFontLoader& fontLoader)
{
    mCharts.Init();
    mTextures.GeneratePattern(0);
    mViewport.RebuildMesh();

    mConsole.AddLine("NkUI Demo v1.0 started.");
    mConsole.AddLine("Type 'help' for commands.");

    NkUIWindow::SetNextWindowPos({10, 30});
    NkUIWindow::SetNextWindowSize({380, 500});

    ctx.SetTheme(NkUITheme::Dark());

    // NE PAS réinitialiser mFontLoader - utiliser celui passé en paramètre
    // mFontLoader est déjà configuré avec le device GPU et le callback
    // On l'assigne simplement à notre membre
    // mFontLoader = fontLoader;  // Copie du loader configuré
    
    // Vérifier que le loader a un device GPU
    if (!mFontLoader.GetGPUDevice()) {
        logger.Warnf("[NkUIDemo] Font loader has no GPU device\n");
    } else {
        logger.Infof("[NkUIDemo] Font loader has GPU device\n");
    }
    
    // Charger les polices...
    const char* fontPaths[] = {
        "e:\\Projets\\MacShared\\Projets\\Jenga\\Jenga\\Exemples\\Nkentseu\\Build\\Bin\\Debug-Windows\\NkUIDemoNKEngine\\Resources\\Fonts\\opensans\\OpenSans-Regular.ttf",
        "e:\\Projets\\MacShared\\Projets\\Jenga\\Jenga\\Exemples\\Nkentseu\\Build\\Bin\\Debug-Windows\\NkUIDemoNKEngine\\Resources\\Fonts\\Roboto\\Roboto-Regular.ttf",
        "e:\\Projets\\MacShared\\Projets\\Jenga\\Jenga\\Exemples\\Nkentseu\\Build\\Bin\\Debug-Windows\\NkUIDemoNKEngine\\Resources\\Fonts\\Antonio-Regular.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
        nullptr
    };

    uint32 fontIdx = 0;
    for (int i = 0; fontPaths[i]; ++i) {
        fontIdx = mFontLoader.LoadFontFromFile(fontPaths[i], 16.0f, true);
        if (fontIdx != 0) {
            logger.Infof("[NkUIDemo] Loaded font: %s\n", fontPaths[i]);
            break;
        }
    }
    
    if (fontIdx == 0) {
        const char* systemFonts[] = {"Arial", "Consolas", "Courier New", "Verdana", nullptr};
        for (int i = 0; systemFonts[i]; ++i) {
            fontIdx = mFontLoader.LoadSystemFont(systemFonts[i], 16.0f, true);
            if (fontIdx != 0) {
                logger.Infof("[NkUIDemo] Loaded system font: %s\n", systemFonts[i]);
                break;
            }
        }
    }
    
    if (fontIdx != 0) {
        mFont = mFontLoader.GetFont(fontIdx);
        if (mFont) {
            logger.Infof("[NkUIDemo] Using font: %s (size=%.1f)\n", mFont->name, mFont->size);
        }
    } else {
        logger.Warnf("[NkUIDemo] No font loaded\n");
        // Fallback: créer une police par défaut simple
        // mFont = &defaultFont;
    }
    
    // Upload initial de l'atlas
    mFontLoader.UploadAtlas();

    (void)wm;
    return true;
}

        inline void NkUIDemo::Destroy() {
            mFontLoader.Destroy();
        }

        // ── NkUIDemo::Render — boucle principale ────────────────────────────────────

        inline void NkUIDemo::Render(NkUIContext& ctx,
                                    NkUIWindowManager& wm,
                                    NkUIDockManager& dock,
                                    NkUILayoutStack& ls,
                                    float32 dt)
        {
            mTime += dt;
            mCharts.Update(dt);
            mAnimator.Update(dt);
            mPerf.Record(dt, ctx);

            // NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];

            // // Utiliser la police chargée (personnalisée)
            // NkUIFont* font = mFontLoader.DefaultFont();  // mFont est maintenant la police chargée par le loader
            
            // if (font) {
            //     // Les métriques sont déjà configurées par le loader, pas besoin de les modifier
            //     // mais vous pouvez les ajuster si nécessaire:
            //     // font->metrics.spaceWidth = font->size * 0.45f;
            //     // font->metrics.lineHeight = font->size * 1.2f;

            //     dl.AddRectFilled({100, 100, 50, 50}, NkColor{255,0,0,255});

            //     // Texte normal
            //     font->RenderText(dl, {100, 100}, "Hello World!", NkColor{255,255,255,255});
                
            //     // Texte avec ellipsis
            //     font->RenderText(dl, {100, 300}, "Long text with ellipsis...", NkColor{255,255,255,255}, 200.0f, true);
                
            //     // Texte avec wrapping
            //     font->RenderTextWrapped(dl, {100, 350, 300, 100},
            //                             "This is a longer text that will wrap automatically to multiple lines",
            //                             NkColor{180,180,180,255});
                
            //     // Afficher le nom de la police chargée pour debug
            //     char fontInfo[128];
            //     snprintf(fontInfo, sizeof(fontInfo), "Font: %s (size=%.1f)", font->name, font->size);
            //     font->RenderText(dl, {100, 450}, fontInfo, NkColor{150,150,150,255});
            // } else {
            //     // Fallback: police builtin
            //     NkUIFont* fallback = mFontLoader.DefaultFont();
            //     if (fallback) {
            //         fallback->RenderText(dl, {100, 100}, "Hello World! (builtin font)", NkColor{255,255,255,255});
            //     }
            // }

            // // Upload de l'atlas si modifié (chargement dynamique de glyphes)
            // mFontLoader.UploadAtlas();

            // Texture animée
            if (mTextures.pattern == 1 || mTextures.pattern == 2)
                mTextures.GeneratePattern(mTextures.pattern, mTime);

            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];

            // Fond
            dl.AddRectFilled({0,0,(float32)ctx.viewW,(float32)ctx.viewH}, ctx.theme.colors.bgPrimary);

            // Menu bar
            if (mShowMenuBar)
                RenderMainMenuBar(ctx, dl, *mFont);

            // Dockspace (simplifié — on gère juste les fenêtres indépendantes)
            const float32 menuH = mShowMenuBar ? ctx.theme.metrics.titleBarHeight : 0.f;
            const NkRect dockRect = {0, menuH, (float32)ctx.viewW, (float32)ctx.viewH - menuH};
            dock.SetViewport(dockRect);
            dock.BeginFrame(ctx, wm, dl, *mFont);
            dock.Render(ctx, dl, *mFont, wm, ls);

            // === Fenêtres ============================================================
            if (mShowWidgets)  WinWidgets  (ctx, wm, ls, *mFont);
            if (mShowCharts)   WinCharts   (ctx, wm, ls, *mFont);
            if (mShowViewport) WinViewport3D(ctx, wm, ls, *mFont);
            if (mShowTextures) WinTextures (ctx, wm, ls, *mFont);
            if (mShowColor)    WinColorPicker(ctx, wm, ls, *mFont);
            if (mShowStyle)    WinStyle    (ctx, wm, ls, *mFont);
            if (mShowConsole)  WinConsole  (ctx, wm, ls, *mFont);
            if (mShowPerf)     WinPerf     (ctx, wm, ls, *mFont);
            if (mShowAbout)    WinAbout    (ctx, wm, ls, *mFont);
            if (mShowWidgets2) WinDemoWidgets2(ctx, wm, ls, *mFont);
            if (mShowScene3D)  WinScene3D  (ctx, wm, ls, *mFont);

            // Modals
            if (mShowSaveModal) {
                if (NkUIMenu::BeginModal(ctx, dl, *mFont, "Sauvegarder le layout",
                                        &mShowSaveModal, {400, 180})) {
                    NkUI::Text(ctx, ls, dl, *mFont, "Chemin du fichier :");
                    NkUI::InputText(ctx, ls, dl, *mFont, "##savepath",
                                    mSavePathBuf, sizeof(mSavePathBuf), 340.f);
                    NkUI::Spacing(ctx, ls, 10.f);
                    NkUI::BeginRow(ctx, ls);
                    if (NkUI::Button(ctx, ls, dl, *mFont, "Sauvegarder", {160,0})) {
                        mShowSaveModal = false;
                        mConsole.AddLine("Layout sauvegardé : %s", mSavePathBuf);
                    }
                    NkUI::SameLine(ctx, ls, 8.f);
                    if (NkUI::Button(ctx, ls, dl, *mFont, "Annuler", {100,0}))
                        mShowSaveModal = false;
                    NkUI::EndRow(ctx, ls);
                    NkUIMenu::EndModal(ctx, dl);
                }
            }
        }

        // ── Menu Bar ─────────────────────────────────────────────────────────────────

        inline void NkUIDemo::RenderMainMenuBar(NkUIContext& ctx,
                                                NkUIDrawList& dl,
                                                NkUIFont& font)
        {
            const NkRect barR = {0, 0, (float32)ctx.viewW,
                                ctx.theme.metrics.titleBarHeight};
            if (!NkUIMenu::BeginMenuBar(ctx, dl, font, barR)) return;

            if (NkUIMenu::BeginMenu(ctx, dl, font, "Fichier")) {
                if (NkUIMenu::MenuItem(ctx, dl, font, "Nouveau layout",    "Ctrl+N")) {}
                if (NkUIMenu::MenuItem(ctx, dl, font, "Ouvrir layout...",  "Ctrl+O")) {}
                if (NkUIMenu::MenuItem(ctx, dl, font, "Sauvegarder...",    "Ctrl+S"))
                    mShowSaveModal = true;
                NkUIMenu::Separator(ctx, dl);
                if (NkUIMenu::MenuItem(ctx, dl, font, "Quitter",           "Alt+F4")) {}
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, dl, font, "Affichage")) {
                NkUIMenu::MenuItem(ctx, dl, font, "Widgets",      nullptr, &mShowWidgets);
                NkUIMenu::MenuItem(ctx, dl, font, "Graphiques",   nullptr, &mShowCharts);
                NkUIMenu::MenuItem(ctx, dl, font, "Viewport 3D",  nullptr, &mShowViewport);
                NkUIMenu::MenuItem(ctx, dl, font, "Textures",     nullptr, &mShowTextures);
                NkUIMenu::MenuItem(ctx, dl, font, "Color Picker", nullptr, &mShowColor);
                NkUIMenu::MenuItem(ctx, dl, font, "Style",        nullptr, &mShowStyle);
                NkUIMenu::MenuItem(ctx, dl, font, "Console",      nullptr, &mShowConsole);
                NkUIMenu::MenuItem(ctx, dl, font, "Perf Monitor", nullptr, &mShowPerf);
                NkUIMenu::MenuItem(ctx, dl, font, "Widgets2",     nullptr, &mShowWidgets2);
                NkUIMenu::MenuItem(ctx, dl, font, "Scene 3D",     nullptr, &mShowScene3D);
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, dl, font, "Thème")) {
                const char* themes[]={"Default","Dark","Minimal","High Contrast"};
                for (int32 i = 0; i < 4; ++i) {
                    bool sel = (mStyle.themeIdx == i);
                    if (NkUIMenu::MenuItem(ctx, dl, font, themes[i], nullptr, &sel)) {
                        mStyle.themeIdx = i;
                        switch(i) {
                            case 0: ctx.SetTheme(NkUITheme::Default());      break;
                            case 1: ctx.SetTheme(NkUITheme::Dark());         break;
                            case 2: ctx.SetTheme(NkUITheme::Minimal());      break;
                            case 3: ctx.SetTheme(NkUITheme::HighContrast()); break;
                        }
                    }
                }
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, dl, font, "Aide")) {
                if (NkUIMenu::MenuItem(ctx, dl, font, "À propos de NkUI..."))
                    mShowAbout = true;
                NkUIMenu::EndMenu(ctx);
            }

            NkUIMenu::EndMenuBar(ctx);
        }

        // ── Fenêtre Widgets ───────────────────────────────────────────────────────────

        inline void NkUIDemo::WinWidgets(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({10, 40});
            NkUIWindow::SetNextWindowSize({380, 560});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Widgets", &mShowWidgets, NkUIWindowFlags::NK_NONE)) return;

            NkUI::Text(ctx, ls, dl, font, "── Texte & Labels ──────────────────");
            NkUI::Text(ctx, ls, dl, font, "Texte normal");
            NkUI::TextSmall(ctx, ls, dl, font, "Texte petit (secondaire)");
            NkUI::TextColored(ctx, ls, dl, font, {255,120,60,255}, "Texte coloré orange");
            NkUI::TextColored(ctx, ls, dl, font, {80,200,120,255}, "Texte coloré vert");
            NkUI::LabelValue(ctx, ls, dl, font, "Clé :", "Valeur");
            NkUI::Separator(ctx, ls, dl);

            NkUI::Text(ctx, ls, dl, font, "── Boutons ─────────────────────────");
            NkUI::BeginRow(ctx, ls);
            if (NkUI::Button(ctx, ls, dl, font, "Bouton")) mConsole.AddLine("Bouton cliqué.");
            NkUI::SameLine(ctx, ls, 6.f);
            if (NkUI::ButtonSmall(ctx, ls, dl, font, "Petit"))  mConsole.AddLine("Petit cliqué.");
            NkUI::SameLine(ctx, ls, 6.f);
            if (NkUI::Button(ctx, ls, dl, font, "Danger##d",  {80,0})) {
                ctx.PushStyleColor(NkStyleVar::NK_BUTTON_BG, {180,40,40,255});
            }
            NkUI::EndRow(ctx, ls);

            NkUI::BeginRow(ctx, ls);
            if (NkUI::Button(ctx, ls, dl, font, "Large##lg", {350, 34}))
                mConsole.AddLine("Large button clicked.");
            NkUI::EndRow(ctx, ls);
            NkUI::Separator(ctx, ls, dl);

            NkUI::Text(ctx, ls, dl, font, "── Cases / Radio / Toggle ──────────");
            NkUI::Checkbox(ctx, ls, dl, font, "Checkbox A", mWidgets.checkA);
            NkUI::Checkbox(ctx, ls, dl, font, "Checkbox B (désactivée)##cb2", mWidgets.checkB);
            NkUI::BeginRow(ctx, ls);
            NkUI::RadioButton(ctx, ls, dl, font, "Radio 0", mWidgets.radioSel, 0);
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::RadioButton(ctx, ls, dl, font, "Radio 1", mWidgets.radioSel, 1);
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::RadioButton(ctx, ls, dl, font, "Radio 2", mWidgets.radioSel, 2);
            NkUI::EndRow(ctx, ls);
            NkUI::Toggle(ctx, ls, dl, font, "Toggle switch", mWidgets.toggleA);
            NkUI::Separator(ctx, ls, dl);

            NkUI::Text(ctx, ls, dl, font, "── Sliders ─────────────────────────");
            NkUI::SliderFloat(ctx, ls, dl, font, "Float", mWidgets.sliderF, 0.f, 1.f);
            NkUI::SliderInt  (ctx, ls, dl, font, "Int",   mWidgets.sliderI, 0, 20);
            NkUI::SliderFloat2(ctx, ls, dl, font, "Vec2", mWidgets.sliderV2, 0.f, 1.f);

            NkUI::Text(ctx, ls, dl, font, "── Drag ────────────────────────────");
            NkUI::DragFloat(ctx, ls, dl, font, "Drag F", mWidgets.dragF, 0.01f, -10.f, 10.f);
            NkUI::DragInt  (ctx, ls, dl, font, "Drag I", mWidgets.dragI, 1.f, -100, 100);
            NkUI::Separator(ctx, ls, dl);

            NkUI::Text(ctx, ls, dl, font, "── Inputs ──────────────────────────");
            NkUI::InputText (ctx, ls, dl, font, "Texte##it", mWidgets.inputStr, 256, 240.f);
            NkUI::InputInt  (ctx, ls, dl, font, "Entier##ii", mWidgets.inputInt);
            NkUI::InputFloat(ctx, ls, dl, font, "Flottant##if", mWidgets.inputFlt);
            NkUI::InputMultiline(ctx, ls, dl, font, "##ml", mWidgets.multiline, 1024, {350, 60});
            NkUI::Separator(ctx, ls, dl);

            NkUI::Text(ctx, ls, dl, font, "── Combo / List ────────────────────");
            static const char* comboItems[] = {"Option A","Option B","Option C","Option D","Option E"};
            NkUI::Combo(ctx, ls, dl, font, "Combo", mWidgets.comboSel, comboItems, 5, 200.f);
            NkUI::ListBox(ctx, ls, dl, font, "Liste##lb", mWidgets.listSel, comboItems, 5, 4);
            NkUI::Separator(ctx, ls, dl);

            NkUI::Text(ctx, ls, dl, font, "── Progress Bar ────────────────────");
            if (mWidgets.progressAnim)
                mWidgets.progress = 0.5f + 0.5f * ::sinf(mTime * 0.8f);
            char pbuf[32]; ::snprintf(pbuf, sizeof(pbuf), "%.0f%%", mWidgets.progress*100.f);
            NkUI::ProgressBar(ctx, ls, dl, mWidgets.progress, {350,18}, pbuf);
            NkUI::Checkbox(ctx, ls, dl, font, "Animer##pa", mWidgets.progressAnim);

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "── Tree ────────────────────────────");
            if (NkUI::TreeNode(ctx, ls, dl, font, "Nœud parent", &mWidgets.treeOpen[0])) {
                NkUI::Text(ctx, ls, dl, font, "Enfant 1");
                NkUI::Text(ctx, ls, dl, font, "Enfant 2");
                if (NkUI::TreeNode(ctx, ls, dl, font, "Sous-nœud##sn", &mWidgets.treeOpen[1])) {
                    NkUI::Text(ctx, ls, dl, font, "Sous-enfant A");
                    NkUI::Text(ctx, ls, dl, font, "Sous-enfant B");
                    NkUI::TreePop(ctx, ls);
                }
                NkUI::TreePop(ctx, ls);
            }

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "── Couleur ─────────────────────────");
            NkUI::ColorButton(ctx, ls, dl, font, "Couleur##cbtn", mWidgets.pickedColor, 22.f);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Graphiques ────────────────────────────────────────────────────────

        inline void NkUIDemo::WinCharts(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({400, 40});
            NkUIWindow::SetNextWindowSize({500, 580});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Graphiques", &mShowCharts)) return;

            // Contrôles
            NkUI::BeginRow(ctx, ls);
            NkUI::Toggle(ctx, ls, dl, font, "Animation", mCharts.animating);
            NkUI::EndRow(ctx, ls);

            // === Line Chart Sin + Cos ================================================
            NkUI::Text(ctx, ls, dl, font, "── Courbes Sin / Cos ────────────────");
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 460.f, 100.f);
                dl.AddRectFilled(cr, ctx.theme.colors.bgTertiary, 4.f);
                dl.AddRect(cr, ctx.theme.colors.border, 1.f, 4.f);
                DrawGrid(dl, cr, ctx.theme.colors.separator, 8);
                DrawLineChart(dl, cr, mCharts.sinData, NkUIDemoCharts::kSamples,
                            {80,140,255,220}, 1.5f, true);
                DrawLineChart(dl, cr, mCharts.cosData, NkUIDemoCharts::kSamples,
                            {80,220,120,200}, 1.5f, false);
                // Légende
                dl.AddRectFilled({cr.x+8,cr.y+6,12,4}, {80,140,255,220});
                font.RenderText(dl, {cr.x+24, cr.y+2}, "Sin", ctx.theme.colors.textSecondary);
                dl.AddRectFilled({cr.x+60,cr.y+6,12,4}, {80,220,120,200});
                font.RenderText(dl, {cr.x+76, cr.y+2}, "Cos", ctx.theme.colors.textSecondary);
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUI::Spacing(ctx, ls, 8.f);

            // === Line Chart Noise ====================================================
            NkUI::Text(ctx, ls, dl, font, "── Signal composite ────────────────");
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 460.f, 80.f);
                dl.AddRectFilled(cr, ctx.theme.colors.bgTertiary, 4.f);
                dl.AddRect(cr, ctx.theme.colors.border, 1.f, 4.f);
                DrawGrid(dl, cr, ctx.theme.colors.separator, 6);
                DrawLineChart(dl, cr, mCharts.noiseData, NkUIDemoCharts::kSamples,
                            {255,140,60,220}, 1.5f, true);
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUI::Spacing(ctx, ls, 8.f);

            // === Bar Chart ===========================================================
            NkUI::Text(ctx, ls, dl, font, "── Bar Chart ───────────────────────");
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 460.f, 100.f);
                dl.AddRectFilled(cr, ctx.theme.colors.bgTertiary, 4.f);
                dl.AddRect(cr, ctx.theme.colors.border, 1.f, 4.f);
                DrawBarChart(dl, cr, mCharts.barData, 8, {80,140,255,200});
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUI::Spacing(ctx, ls, 8.f);

            // === Radar Chart =========================================================
            NkUI::Text(ctx, ls, dl, font, "── Radar Chart ─────────────────────");
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 220.f, 160.f);
                dl.AddRectFilled(cr, ctx.theme.colors.bgTertiary, 4.f);
                static float32 radarVals[6] = {0.8f, 0.6f, 0.9f, 0.4f, 0.7f, 0.5f};
                // Animate
                for (int32 i = 0; i < 6; ++i)
                    radarVals[i] = 0.5f + 0.4f * ::sinf(mTime*0.7f + i*1.1f);
                DrawRadarChart(dl, cr, radarVals, 6);
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUI::SameLine(ctx, ls, 8.f);

            // === Scatter Plot ========================================================
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 220.f, 160.f);
                dl.AddRectFilled(cr, ctx.theme.colors.bgTertiary, 4.f);
                dl.AddRect(cr, ctx.theme.colors.border, 1.f, 4.f);
                NkUI::Text(ctx, ls, dl, font, "Scatter");
                DrawScatterPlot(dl, cr, ctx, mTime);
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUI::Spacing(ctx, ls, 8.f);

            // === Heatmap =============================================================
            NkUI::Text(ctx, ls, dl, font, "── Heatmap ─────────────────────────");
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 460.f, 80.f);
                DrawHeatmap(dl, cr, ctx);
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUI::Spacing(ctx, ls, 8.f);

            // === Spectrum ============================================================
            NkUI::Text(ctx, ls, dl, font, "── Spectre audio (simulé) ───────────");
            {
                const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 460.f, 70.f);
                dl.AddRectFilled(cr, {10,12,18,255}, 4.f);
                DrawSpectrum(dl, cr, mTime);
                NkUILayout::AdvanceItem(ctx, ls, cr);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Viewport 3D ──────────────────────────────────────────────────────

        inline void NkUIDemo::WinViewport3D(NkUIContext& ctx,
                                            NkUIWindowManager& wm,
                                            NkUILayoutStack& ls,
                                            NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({910, 40});
            NkUIWindow::SetNextWindowSize({360, 360});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Viewport 3D", &mShowViewport)) return;

            // Toolbar
            NkUI::BeginRow(ctx, ls);
            static const char* meshNames[] = {"Cube","Sphere","Torus"};
            NkUI::Combo(ctx, ls, dl, font, "##mesh", mViewport.meshType, meshNames, 3, 100.f);
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::Toggle(ctx, ls, dl, font, "Wire##w", mViewport.wireframe);
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::Toggle(ctx, ls, dl, font, "Grille##g", mViewport.showGrid);
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::Toggle(ctx, ls, dl, font, "Rot##r", mViewport.rotating);
            NkUI::EndRow(ctx, ls);

            NkUI::SliderFloat(ctx, ls, dl, font, "FOV", mViewport.fov, 30.f, 120.f, "%.0f°");
            NkUI::SliderFloat(ctx, ls, dl, font, "Dist##d", mViewport.camDist, 1.5f, 10.f);

            // Zone de rendu
            const NkRect vpR = NkUILayout::NextItemRect(ctx, ls, 320.f, 220.f);

            // Rotation par drag
            const bool hovered = ctx.IsHovered(vpR);
            if (hovered && ctx.input.IsMouseClicked(0)) mVPDragging = true;
            if (mVPDragging) {
                mVPRotY += ctx.input.mouseDelta.x * 0.5f;
                mVPRotX += ctx.input.mouseDelta.y * 0.5f;
                if (!ctx.input.mouseDown[0]) mVPDragging = false;
            }
            if (mViewport.rotating) mVPRotY += 30.f * (1.f / 60.f);

            // Rendu soft
            for (int32 i = 0; i < kFBW*kFBH; ++i) { mFB[i]={18,20,26,255}; mZBuf[i]=1e9f; }
            SoftRender3D(mFB, kFBW, kFBH, mVPRotX, mVPRotY,
                        mViewport.meshType, mViewport.wireframe, mViewport.showGrid);

            // Affichage du framebuffer comme quads colorés (CPU renderer)
            const float32 scaleX = vpR.w / kFBW;
            const float32 scaleY = vpR.h / kFBH;
            // Pour le CPU renderer : on dessine les pixels comme des rectangles
            // (optimal : on pourrait passer une texture, mais voici la version universelle)
            dl.PushClipRect(vpR, true);
            for (int32 y = 0; y < kFBH; ++y) {
                for (int32 x = 0; x < kFBW; ++x) {
                    const Pixel& p = mFB[y*kFBW + x];
                    if (p.r == 18 && p.g == 20 && p.b == 26) continue; // skip bg
                    const NkRect pr = {vpR.x + x*scaleX, vpR.y + y*scaleY, scaleX+1, scaleY+1};
                    dl.AddRectFilled(pr, {p.r, p.g, p.b, p.a});
                }
            }

            // Grille de fond
            if (mViewport.showGrid)
                DrawGrid(dl, vpR, {40,44,55,200}, 10);

            // Gizmo axes
            DrawGizmo(dl, {vpR.x + vpR.w - 35, vpR.y + 35}, 25.f, mVPRotX, mVPRotY);

            // Overlay info
            char info[64];
            ::snprintf(info, sizeof(info), "Rx:%.0f Ry:%.0f | %s",
                    mVPRotX, mVPRotY, meshNames[mViewport.meshType]);
            font.RenderText(dl, {vpR.x+4, vpR.y+vpR.h-font.size-4},
                            info, ctx.theme.colors.textSecondary);
            dl.PopClipRect();

            NkUILayout::AdvanceItem(ctx, ls, vpR);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Textures ─────────────────────────────────────────────────────────

        inline void NkUIDemo::WinTextures(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({10, 620});
            NkUIWindow::SetNextWindowSize({380, 360});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Textures", &mShowTextures)) return;

            static const char* patNames[] = {"Checker","Gradient","Noise","Mandelbrot"};
            int32 oldPat = mTextures.pattern;
            NkUI::Combo(ctx, ls, dl, font, "Pattern##tp", mTextures.pattern, patNames, 4, 200.f);
            if (mTextures.pattern != oldPat)
                mTextures.GeneratePattern(mTextures.pattern, mTime);

            NkUI::SliderFloat(ctx, ls, dl, font, "Zoom##tz",  mTextures.zoom, 0.1f, 5.f);
            if (mTextures.pattern == 3) {
                NkUI::SliderFloat(ctx, ls, dl, font, "Pan X##tpx", mTextures.panX, -2.f, 1.f);
                NkUI::SliderFloat(ctx, ls, dl, font, "Pan Y##tpy", mTextures.panY, -1.f, 1.f);
                if (NkUI::Button(ctx, ls, dl, font, "Régénérer##tregen"))
                    mTextures.GeneratePattern(3, mTime);
            }

            NkUI::Separator(ctx, ls, dl);

            // Aperçu de la texture (rendu software pixel par pixel)
            const NkRect texR = NkUILayout::NextItemRect(ctx, ls, 340.f, 240.f);
            dl.PushClipRect(texR, true);
            dl.AddRectFilled(texR, {20,20,25,255}, 4.f);

            // Affiche l'atlas en le repassant pixel par pixel (zoom + pan)
            const float32 dispW = texR.w / mTextures.zoom;
            const float32 dispH = texR.h / mTextures.zoom;
            const float32 pxW   = texR.w / NkUIDemoTextures::kAtlasW;
            const float32 pxH   = texR.h / NkUIDemoTextures::kAtlasH;
            for (int32 y = 0; y < NkUIDemoTextures::kAtlasH; ++y) {
                for (int32 x = 0; x < NkUIDemoTextures::kAtlasW; ++x) {
                    const uint8* p = mTextures.pixels + (y*NkUIDemoTextures::kAtlasW + x)*4;
                    NkColor col = {p[0],p[1],p[2],p[3]};
                    // Simple zoom centré
                    const float32 sx = texR.x + x * pxW;
                    const float32 sy = texR.y + y * pxH;
                    if (pxW < 0.8f || pxH < 0.8f) {
                        // Trop petit : skip les pixels sombres
                        if (p[0] < 10 && p[1] < 10 && p[2] < 10) continue;
                    }
                    dl.AddRectFilled({sx, sy, pxW+0.5f, pxH+0.5f}, col);
                }
            }

            // Dimensions overlay
            char dimBuf[64];
            ::snprintf(dimBuf, sizeof(dimBuf), "%dx%d RGBA8",
                    NkUIDemoTextures::kAtlasW, NkUIDemoTextures::kAtlasH);
            font.RenderText(dl, {texR.x+4, texR.y+texR.h-font.size-2},
                            dimBuf, ctx.theme.colors.textSecondary);
            dl.PopClipRect();
            NkUILayout::AdvanceItem(ctx, ls, texR);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Color Picker ─────────────────────────────────────────────────────

        inline void NkUIDemo::WinColorPicker(NkUIContext& ctx,
                                            NkUIWindowManager& wm,
                                            NkUILayoutStack& ls,
                                            NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({400, 640});
            NkUIWindow::SetNextWindowSize({300, 460});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Color Picker", &mShowColor)) return;

            NkUI::Text(ctx, ls, dl, font, "Couleur active :");
            NkUI::BeginRow(ctx, ls);
            NkUI::ColorButton(ctx, ls, dl, font, "##cpbig", mWidgets.pickedColor, 32.f);
            NkUI::SameLine(ctx, ls, 6.f);
            char hex[16];
            ::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                    mWidgets.pickedColor.r, mWidgets.pickedColor.g,
                    mWidgets.pickedColor.b, mWidgets.pickedColor.a);
            NkUI::Text(ctx, ls, dl, font, hex);
            NkUI::EndRow(ctx, ls);

            NkUI::Separator(ctx, ls, dl);

            // Color picker complet (NkUILayout2)
            NkUIColorPickerFull::Draw(ctx, dl, font, ls, "##fullcp", mWidgets.pickedColor);

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "Préréglages :");
            NkUI::BeginRow(ctx, ls);
            static NkColor presets[] = {
                {255,80,80,255},{255,160,60,255},{255,230,50,255},
                {80,220,100,255},{60,180,255,255},{180,80,255,255},
                {255,255,255,255},{160,160,160,255},{30,30,35,255}
            };
            for (int32 i = 0; i < 9; ++i) {
                if (NkUI::ColorButton(ctx, ls, dl, font, "##pr", presets[i], 22.f))
                    mWidgets.pickedColor = presets[i];
                if (i < 8) NkUI::SameLine(ctx, ls, 4.f);
            }
            NkUI::EndRow(ctx, ls);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Style ─────────────────────────────────────────────────────────────

        inline void NkUIDemo::WinStyle(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({710, 40});
            NkUIWindow::SetNextWindowSize({290, 340});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Style éditeur", &mShowStyle)) return;

            static const char* themes[] = {"Default","Dark","Minimal","High Contrast"};
            int32 oldTheme = mStyle.themeIdx;
            NkUI::Combo(ctx, ls, dl, font, "Thème##st", mStyle.themeIdx, themes, 4, 180.f);
            if (mStyle.themeIdx != oldTheme) {
                switch(mStyle.themeIdx) {
                    case 0: ctx.SetTheme(NkUITheme::Default());      break;
                    case 1: ctx.SetTheme(NkUITheme::Dark());         break;
                    case 2: ctx.SetTheme(NkUITheme::Minimal());      break;
                    case 3: ctx.SetTheme(NkUITheme::HighContrast()); break;
                }
                mStyle.cornerR  = ctx.theme.metrics.cornerRadius;
                mStyle.spacing  = ctx.theme.metrics.itemSpacing;
                mStyle.padX     = ctx.theme.metrics.paddingX;
                mStyle.padY     = ctx.theme.metrics.paddingY;
                mStyle.itemH    = ctx.theme.metrics.itemHeight;
                mStyle.borderW  = ctx.theme.metrics.borderWidth;
                mStyle.animations = ctx.theme.anim.enabled;
            }

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "Métriques :");
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Coin##cr",    mStyle.cornerR, 0.f, 15.f, "%.1f"))
                ctx.theme.metrics.cornerRadius = mStyle.cornerR;
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Espac.##sp",  mStyle.spacing,  0.f, 20.f, "%.1f"))
                ctx.theme.metrics.itemSpacing = mStyle.spacing;
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Pad X##px",   mStyle.padX,     0.f, 20.f, "%.1f"))
                ctx.theme.metrics.paddingX = mStyle.padX;
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Pad Y##py",   mStyle.padY,     0.f, 20.f, "%.1f"))
                ctx.theme.metrics.paddingY = mStyle.padY;
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Hauteur##ih", mStyle.itemH,   16.f, 48.f, "%.1f"))
                ctx.theme.metrics.itemHeight = mStyle.itemH;
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Bordure##bw", mStyle.borderW,  0.f,  4.f, "%.1f"))
                ctx.theme.metrics.borderWidth = mStyle.borderW;
            if (NkUI::SliderFloat(ctx, ls, dl, font, "Alpha##ga",   mStyle.globalAlpha, 0.1f, 1.f, "%.2f"))
                ctx.globalAlpha = mStyle.globalAlpha;
            if (NkUI::Toggle(ctx, ls, dl, font, "Animations##an",   mStyle.animations))
                ctx.theme.anim.enabled = mStyle.animations;

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "Couleurs accent :");
            NkUI::ColorButton(ctx, ls, dl, font, "Accent##ac",    ctx.theme.colors.accent,    20.f);
            NkUI::SameLine(ctx, ls, 4.f);
            NkUI::ColorButton(ctx, ls, dl, font, "BG Win##bw",    ctx.theme.colors.bgWindow,  20.f);
            NkUI::SameLine(ctx, ls, 4.f);
            NkUI::ColorButton(ctx, ls, dl, font, "Text##txt",     ctx.theme.colors.textPrimary,20.f);
            NkUI::SameLine(ctx, ls, 4.f);
            NkUI::ColorButton(ctx, ls, dl, font, "Border##brd",   ctx.theme.colors.border,    20.f);

            NkUI::Separator(ctx, ls, dl);
            if (NkUI::Button(ctx, ls, dl, font, "Réinitialiser##sreset", {240,0})) {
                ctx.SetTheme(NkUITheme::Dark());
                mStyle = NkUIDemoStyle{};
                mStyle.themeIdx = 1;
                mStyle.cornerR  = ctx.theme.metrics.cornerRadius;
                mStyle.spacing  = ctx.theme.metrics.itemSpacing;
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Console ───────────────────────────────────────────────────────────

        inline void NkUIDemo::WinConsole(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({710, 400});
            NkUIWindow::SetNextWindowSize({490, 280});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Console", &mShowConsole)) return;

            // Toolbar
            NkUI::BeginRow(ctx, ls);
            if (NkUI::ButtonSmall(ctx, ls, dl, font, "Vider##cv"))
                mConsole.Clear();
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::Toggle(ctx, ls, dl, font, "Auto-scroll##cas", mConsole.autoScroll);
            NkUI::EndRow(ctx, ls);

            // Zone de texte scrollable
            const float32 inputH = ctx.GetItemHeight() + ctx.GetPaddingY()*2;
            const NkRect scrollR = NkUILayout::NextItemRect(ctx, ls, 460.f, 180.f);
            NkUI::BeginScrollRegion(ctx, ls, "##consolescroll", scrollR, nullptr, &mConsole.scrollY);

            for (int32 i = 0; i < mConsole.numLines; ++i) {
                const char* line = mConsole.lines[i];
                NkColor col = ctx.theme.colors.textPrimary;
                if (line[0] == '>') col = ctx.theme.colors.accent;
                else if (::strncmp(line, "Err", 3) == 0) col = {220,60,60,255};
                else if (::strncmp(line, "Warn", 4) == 0) col = {255,180,40,255};
                else col = ctx.theme.colors.textSecondary;
                NkUI::TextColored(ctx, ls, dl, font, col, line);
            }

            if (mConsole.autoScroll) {
                // Force scroll en bas
                NkUILayoutNode* n = ls.Top();
                if (n) mConsole.scrollY = n->contentSize.y;
            }
            NkUI::EndScrollRegion(ctx, ls);
            // NkUILayout::AdvanceItem(ctx, ls, scrollR);

            // Input
            bool submitted = false;
            NkUI::InputText(ctx, ls, dl, font, "##conin", mConsole.inputBuf, 256, 380.f);
            NkUI::SameLine(ctx, ls, 4.f);
            if (NkUI::ButtonSmall(ctx, ls, dl, font, "Exec##ce") ||
                (ctx.IsFocused(ctx.GetID("##conin")) &&
                ctx.input.IsKeyPressed(NkKey::NK_ENTER)))
            {
                submitted = true;
            }
            if (submitted && mConsole.inputBuf[0]) {
                mConsole.ExecCommand(mConsole.inputBuf);
                mConsole.inputBuf[0] = 0;
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Perf Monitor ──────────────────────────────────────────────────────

        inline void NkUIDemo::WinPerf(NkUIContext& ctx,
                                    NkUIWindowManager& wm,
                                    NkUILayoutStack& ls,
                                    NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({400, 640});
            NkUIWindow::SetNextWindowSize({300, 260});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Performances", &mShowPerf)) return;

            char buf[64];
            ::snprintf(buf, sizeof(buf), "FPS moy: %.1f", mPerf.avgFps);
            NkUI::Text(ctx, ls, dl, font, buf);

            float32 curFps = mPerf.fps[(mPerf.head + NkUIDemoPerfMonitor::kHistory - 1)
                                        % NkUIDemoPerfMonitor::kHistory];
            ::snprintf(buf, sizeof(buf), "FPS actuel: %.1f", curFps);
            NkUI::Text(ctx, ls, dl, font, buf);

            float32 curMs = mPerf.frameMs[(mPerf.head + NkUIDemoPerfMonitor::kHistory - 1)
                                        % NkUIDemoPerfMonitor::kHistory];
            ::snprintf(buf, sizeof(buf), "Frame: %.2f ms", curMs);
            NkUI::Text(ctx, ls, dl, font, buf);

            ::snprintf(buf, sizeof(buf), "Vtx: %u  Idx: %u  Cmds: %u",
                    mPerf.totalVtx, mPerf.totalIdx, mPerf.totalCmds);
            NkUI::Text(ctx, ls, dl, font, buf);

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "Historique FPS :");

            const NkRect cr = NkUILayout::NextItemRect(ctx, ls, 260.f, 80.f);
            dl.AddRectFilled(cr, ctx.theme.colors.bgTertiary, 4.f);

            // Réorganise l'historique pour l'affichage
            float32 ordered[NkUIDemoPerfMonitor::kHistory];
            for (int32 i = 0; i < NkUIDemoPerfMonitor::kHistory; ++i)
                ordered[i] = mPerf.fps[(mPerf.head + i) % NkUIDemoPerfMonitor::kHistory]
                            / 120.f;  // normalise sur 120 FPS max
            DrawLineChart(dl, cr, ordered, NkUIDemoPerfMonitor::kHistory,
                        {80,220,100,200}, 1.2f, true);
            // Ligne 60fps
            const float32 y60 = cr.y + cr.h * (1.f - 60.f/120.f);
            dl.AddLine({cr.x, y60}, {cr.x+cr.w, y60}, {200,200,60,100}, 1.f);
            NkUILayout::AdvanceItem(ctx, ls, cr);

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "DrawLists :");
            for (int32 i = 0; i < NkUIContext::LAYER_COUNT; ++i) {
                const char* layerNames[]={"BG","Windows","Popups","Overlay"};
                ::snprintf(buf, sizeof(buf), "  Layer[%s]: v=%u i=%u c=%u",
                        layerNames[i],
                        ctx.layers[i].vtxCount,
                        ctx.layers[i].idxCount,
                        ctx.layers[i].cmdCount);
                NkUI::TextSmall(ctx, ls, dl, font, buf);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre About ─────────────────────────────────────────────────────────────

        inline void NkUIDemo::WinAbout(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIDrawList& dl = *ctx.dl;
            NkUIWindow::SetNextWindowPos({400, 250});
            NkUIWindow::SetNextWindowSize({360, 280});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "À propos de NkUI", &mShowAbout, NkUIWindowFlags::NK_NO_RESIZE)) return;

            NkUI::Spacing(ctx, ls, 8.f);
            NkUI::TextColored(ctx, ls, dl, font, ctx.theme.colors.accent, "NkUI v1.0");
            NkUI::Text(ctx, ls, dl, font, "Système UI immediate-mode C++17");
            NkUI::Text(ctx, ls, dl, font, "Zéro dépendance externe.");
            NkUI::Separator(ctx, ls, dl);
            NkUI::LabelValue(ctx, ls, dl, font, "Auteur :", "TEUGUIA TADJUIDJE Rodolf");
            NkUI::LabelValue(ctx, ls, dl, font, "Licence :", "Apache-2.0");
            NkUI::LabelValue(ctx, ls, dl, font, "Standard :", "C++17");
            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "Fonctionnalités :");
            NkUI::TextSmall(ctx, ls, dl, font, "• DrawList vectoriel (triangles, texte, images)");
            NkUI::TextSmall(ctx, ls, dl, font, "• Layout flex (Row/Col/Grid/Scroll/Split/Tabs)");
            NkUI::TextSmall(ctx, ls, dl, font, "• Fenêtres flottantes + docking complet");
            NkUI::TextSmall(ctx, ls, dl, font, "• 27 widgets (Button, Slider, Input, Combo...)");
            NkUI::TextSmall(ctx, ls, dl, font, "• Thèmes JSON + animations tween");
            NkUI::TextSmall(ctx, ls, dl, font, "• Backend CPU offline + OpenGL optionnel");
            NkUI::Separator(ctx, ls, dl);
            if (NkUI::Button(ctx, ls, dl, font, "Fermer##abclose", {300,0}))
                mShowAbout = false;

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Widgets2 (tables, tooltips, node graph) ──────────────────────────

        inline void NkUIDemo::WinDemoWidgets2(NkUIContext& ctx,
                                            NkUIWindowManager& wm,
                                            NkUILayoutStack& ls,
                                            NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({1100, 40});
            NkUIWindow::SetNextWindowSize({380, 500});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Widgets avancés", &mShowWidgets2)) return;

            // === Table ===============================================================
            NkUI::Text(ctx, ls, dl, font, "── Table ───────────────────────────");
            if (NkUI::BeginTable(ctx, ls, dl, font, "##demo_table", 4, 350.f)) {
                NkUI::TableHeader(ctx, ls, dl, font, "Nom");
                NkUI::TableHeader(ctx, ls, dl, font, "Type");
                NkUI::TableHeader(ctx, ls, dl, font, "Valeur");
                NkUI::TableHeader(ctx, ls, dl, font, "Info");

                static const char* tableData[][4] = {
                    {"position","Vec3", "0,0,0",   "World pos"},
                    {"rotation","Euler","0°,0°,0°","Rotation"},
                    {"scale",   "Vec3", "1,1,1",   "Échelle"},
                    {"visible", "bool", "true",    "Rendu"},
                    {"castShadow","bool","true",   "Ombres"},
                    {"LOD",     "int",  "0",       "Niv. détail"},
                };
                for (int32 i = 0; i < 6; ++i) {
                    if (NkUI::TableNextRow(ctx, ls, dl, i%2==0)) {
                        for (int32 j = 0; j < 4; ++j) {
                            NkUI::Text(ctx, ls, dl, font, tableData[i][j]);
                            if (j < 3) NkUI::TableNextCell(ctx, ls);
                        }
                    }
                }
                NkUI::EndTable(ctx, ls, dl);
            }

            NkUI::Separator(ctx, ls, dl);

            // === Node Graph ==========================================================
            NkUI::Text(ctx, ls, dl, font, "── Node Graph ──────────────────────");
            {
                const NkRect ngR = NkUILayout::NextItemRect(ctx, ls, 350.f, 200.f);
                dl.AddRectFilled(ngR, {18, 20, 26, 255}, 4.f);
                dl.AddRect(ngR, ctx.theme.colors.border, 1.f, 4.f);
                dl.PushClipRect(ngR, true);
                DrawNodeGraph(dl, ngR, ctx);
                dl.PopClipRect();
                NkUILayout::AdvanceItem(ctx, ls, ngR);
            }

            NkUI::Separator(ctx, ls, dl);

            // === Tooltips ============================================================
            NkUI::Text(ctx, ls, dl, font, "── Tooltips (hover les boutons) ────");
            NkUI::BeginRow(ctx, ls);
            {
                const NkRect r = NkUILayout::NextItemRect(ctx, ls, 80.f, ctx.GetItemHeight());
                if (NkUI::Button(ctx, ls, dl, font, "Info##tt1")){}
                if (ctx.IsHovered(r))
                    NkUI::Tooltip(ctx, dl, font, "Ce bouton affiche des infos générales.");
            }
            NkUI::SameLine(ctx, ls, 6.f);
            {
                const NkRect r = NkUILayout::NextItemRect(ctx, ls, 80.f, ctx.GetItemHeight());
                if (NkUI::Button(ctx, ls, dl, font, "Alerte##tt2")){}
                if (ctx.IsHovered(r))
                    NkUI::Tooltip(ctx, dl, font, "ATTENTION: Action irréversible !");
            }
            NkUI::SameLine(ctx, ls, 6.f);
            {
                const NkRect r = NkUILayout::NextItemRect(ctx, ls, 80.f, ctx.GetItemHeight());
                if (NkUI::Button(ctx, ls, dl, font, "Config##tt3")){}
                if (ctx.IsHovered(r))
                    NkUI::Tooltip(ctx, dl, font, "Ouvre le panneau de configuration.");
            }
            NkUI::EndRow(ctx, ls);

            NkUI::Separator(ctx, ls, dl);

            // === Context menu ========================================================
            NkUI::Text(ctx, ls, dl, font, "── Clic droit (Context Menu) ───────");
            {
                const NkRect r = NkUILayout::NextItemRect(ctx, ls, 350.f, 60.f);
                NkColor c = ctx.IsHovered(r) ? ctx.theme.colors.bgTertiary : ctx.theme.colors.bgSecondary;
                dl.AddRectFilled(r, c, 4.f);
                dl.AddRect(r, ctx.theme.colors.border, 1.f, 4.f);
                font.RenderText(dl, {r.x+8, r.y+(r.h-font.size)*0.5f+font.metrics.ascender*0.5f},
                                "Clic droit ici →", ctx.theme.colors.textSecondary);
                NkUILayout::AdvanceItem(ctx, ls, r);

                if (NkUIMenu::BeginContextMenu(ctx, dl, font, "##ctxmenu_w2")) {
                    if (NkUIMenu::MenuItem(ctx, dl, font, "Copier",    "Ctrl+C")) {}
                    if (NkUIMenu::MenuItem(ctx, dl, font, "Coller",    "Ctrl+V")) {}
                    if (NkUIMenu::MenuItem(ctx, dl, font, "Supprimer", "Del"))    {}
                    NkUIMenu::Separator(ctx, dl);
                    if (NkUIMenu::MenuItem(ctx, dl, font, "Propriétés...")) {}
                    NkUIMenu::EndContextMenu(ctx);
                }
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // ── Fenêtre Scene 3D ─────────────────────────────────────────────────────────

        inline void NkUIDemo::WinScene3D(NkUIContext& ctx,
                                        NkUIWindowManager& wm,
                                        NkUILayoutStack& ls,
                                        NkUIFont& font)
        {
            NkUIWindow::SetNextWindowPos({1100, 560});
            NkUIWindow::SetNextWindowSize({380, 320});
            NkUIDrawList& dl = *ctx.dl;

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Scène 3D — Propriétés", &mShowScene3D)) return;

            NkUI::Text(ctx, ls, dl, font, "── Scène ───────────────────────────");
            static NkColor ambientCol = {40, 45, 60, 255};
            static float32 ambientIntensity = 0.2f;
            NkUI::ColorButton(ctx, ls, dl, font, "Couleur ambiante##sc_amb", ambientCol, 20.f);
            NkUI::SameLine(ctx, ls, 6.f);
            NkUI::SliderFloat(ctx, ls, dl, font, "Intensité##sc_ai", ambientIntensity, 0.f, 1.f);

            static NkColor fogCol = {30,35,50,255};
            static float32 fogNear = 5.f, fogFar = 30.f;
            static bool fogEnabled = true;
            NkUI::Toggle(ctx, ls, dl, font, "Brouillard##sc_fog", fogEnabled);
            if (fogEnabled) {
                NkUI::ColorButton(ctx, ls, dl, font, "Couleur brouillard##sc_fc", fogCol, 18.f);
                NkUI::SliderFloat(ctx, ls, dl, font, "Proche##sc_fn", fogNear, 0.f, 20.f, "%.1f");
                NkUI::SliderFloat(ctx, ls, dl, font, "Loin##sc_ff",   fogFar,  5.f, 100.f, "%.1f");
            }

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "── Lumière principale ───────────────");
            static NkColor lightCol = {255,240,200,255};
            static float32 lightYaw = -45.f, lightPitch = 60.f, lightIntensity = 1.f;
            static bool shadowEnabled = true;
            NkUI::ColorButton(ctx, ls, dl, font, "Couleur lumière##sc_lc", lightCol, 20.f);
            NkUI::SliderFloat(ctx, ls, dl, font, "Intensité##sc_li",  lightIntensity, 0.f, 5.f, "%.2f");
            NkUI::SliderFloat(ctx, ls, dl, font, "Yaw##sc_ly",        lightYaw,   -180.f, 180.f, "%.0f°");
            NkUI::SliderFloat(ctx, ls, dl, font, "Pitch##sc_lp",      lightPitch,  -90.f, 90.f,  "%.0f°");
            NkUI::Toggle(ctx, ls, dl, font, "Ombres portées##sc_sh",  shadowEnabled);

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "── Caméra ──────────────────────────");
            static float32 camFov = 60.f, camNear = 0.1f, camFar = 1000.f;
            static int32   camType = 0;
            static const char* camTypes[] = {"Perspective","Orthographique","Fisheye"};
            NkUI::Combo(ctx, ls, dl, font, "Type##sc_ct", camType, camTypes, 3, 180.f);
            NkUI::SliderFloat(ctx, ls, dl, font, "FOV##sc_fov",  camFov,  10.f, 170.f, "%.0f°");
            NkUI::SliderFloat(ctx, ls, dl, font, "Near##sc_cn",  camNear, 0.01f, 10.f, "%.3f");
            NkUI::SliderFloat(ctx, ls, dl, font, "Far##sc_cf",   camFar,  10.f, 10000.f, "%.0f");

            NkUI::Separator(ctx, ls, dl);
            NkUI::Text(ctx, ls, dl, font, "── Post-process ────────────────────");
            static bool bloomEnabled=true, tonemapEnabled=true, ssaoEnabled=false;
            static float32 bloomThresh=0.8f, bloomIntensity=1.f, exposure=1.f;
            NkUI::Toggle(ctx, ls, dl, font, "Bloom##sc_bl", bloomEnabled);
            if (bloomEnabled) {
                NkUI::SliderFloat(ctx, ls, dl, font, "Seuil##sc_bt",      bloomThresh,    0.f, 2.f, "%.2f");
                NkUI::SliderFloat(ctx, ls, dl, font, "Intensité B##sc_bi", bloomIntensity, 0.f, 5.f, "%.2f");
            }
            NkUI::Toggle(ctx, ls, dl, font, "Tone mapping##sc_tm", tonemapEnabled);
            if (tonemapEnabled)
                NkUI::SliderFloat(ctx, ls, dl, font, "Exposition##sc_ex", exposure, 0.1f, 4.f, "%.2f");
            NkUI::Toggle(ctx, ls, dl, font, "SSAO##sc_ssao", ssaoEnabled);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // =============================================================================
        // Helpers de rendu graphique
        // =============================================================================

        inline void NkUIDemo::DrawLineChart(NkUIDrawList& dl, NkRect r,
                                            const float32* data, int32 n,
                                            NkColor col, float32 thickness,
                                            bool filled)
        {
            if (n < 2) return;
            const float32 dx = r.w / (n - 1);

            if (filled) {
                // Polygon fill
                for (int32 i = 0; i < n - 1; ++i) {
                    const float32 x0 = r.x + i * dx;
                    const float32 x1 = r.x + (i+1) * dx;
                    const float32 y0 = r.y + r.h * (1.f - data[i]);
                    const float32 y1 = r.y + r.h * (1.f - data[i+1]);
                    const NkColor fc = {col.r, col.g, col.b, (uint8)(col.a * 0.25f)};
                    dl.AddTriangleFilled({x0, y0}, {x1, y1}, {x0, r.y+r.h}, fc);
                    dl.AddTriangleFilled({x1, y1}, {x1, r.y+r.h}, {x0, r.y+r.h}, fc);
                }
            }

            for (int32 i = 0; i < n - 1; ++i) {
                const float32 x0 = r.x + i * dx;
                const float32 x1 = r.x + (i+1) * dx;
                const float32 y0 = r.y + r.h * (1.f - data[i]);
                const float32 y1 = r.y + r.h * (1.f - data[i+1]);
                dl.AddLine({x0,y0},{x1,y1}, col, thickness);
            }
        }

        inline void NkUIDemo::DrawBarChart(NkUIDrawList& dl, NkRect r,
                                            const float32* data, int32 n,
                                            NkColor col)
        {
            if (n == 0) return;
            const float32 gap  = 2.f;
            const float32 barW = (r.w - gap * (n+1)) / n;
            for (int32 i = 0; i < n; ++i) {
                const float32 h = r.h * data[i];
                const NkRect bar = {r.x + gap + i*(barW+gap),
                                    r.y + r.h - h, barW, h};
                const NkColor shad = {(uint8)(col.r*0.7f),(uint8)(col.g*0.7f),(uint8)(col.b*0.7f),col.a};
                dl.AddRectFilled(bar, shad, 2.f);
                dl.AddRectFilled({bar.x, bar.y, bar.w, 4.f}, col, 2.f);
            }
        }

        inline void NkUIDemo::DrawGrid(NkUIDrawList& dl, NkRect r,
                                        NkColor col, int32 div)
        {
            for (int32 i = 1; i < div; ++i) {
                const float32 x = r.x + r.w * i / div;
                const float32 y = r.y + r.h * i / div;
                dl.AddLine({x, r.y}, {x, r.y+r.h}, col, 1.f);
                dl.AddLine({r.x, y}, {r.x+r.w, y},  col, 1.f);
            }
        }

        inline void NkUIDemo::DrawRadarChart(NkUIDrawList& dl, NkRect r,
                                            const float32* vals, int32 n)
        {
            const NkVec2 c = {r.x + r.w*0.5f, r.y + r.h*0.5f};
            const float32 radius = (r.w < r.h ? r.w : r.h) * 0.42f;

            // Grilles
            for (int32 g = 1; g <= 4; ++g) {
                const float32 gr = radius * g / 4.f;
                NkVec2 pts[16];
                for (int32 i = 0; i <= n; ++i) {
                    const float32 a = -NKUI_PI*0.5f + 2.f*NKUI_PI * (i%n) / n;
                    pts[i] = {c.x + ::cosf(a)*gr, c.y + ::sinf(a)*gr};
                }
                dl.AddPolyline(pts, n+1, {60,65,80,200}, 1.f, true);
            }
            // Axes
            for (int32 i = 0; i < n; ++i) {
                const float32 a = -NKUI_PI*0.5f + 2.f*NKUI_PI * i / n;
                dl.AddLine(c, {c.x+::cosf(a)*radius, c.y+::sinf(a)*radius},
                        {60,65,80,200}, 1.f);
            }
            // Données
            NkVec2 pts[8];
            for (int32 i = 0; i < n; ++i) {
                const float32 a = -NKUI_PI*0.5f + 2.f*NKUI_PI * i / n;
                const float32 rv = vals[i] * radius;
                pts[i] = {c.x + ::cosf(a)*rv, c.y + ::sinf(a)*rv};
            }
            dl.AddConvexPolyFilled(pts, n, {80,140,255,60});
            dl.AddPolyline(pts, n, {80,140,255,220}, 1.5f, true);
            // Points
            for (int32 i = 0; i < n; ++i)
                dl.AddCircleFilled(pts[i], 3.f, {80,140,255,255});
        }

        inline void NkUIDemo::DrawScatterPlot(NkUIDrawList& dl, NkRect r,
                                            NkUIContext&, float32 t)
        {
            static constexpr int32 kN = 40;
            // Données pseudo-aléatoires stables
            for (int32 i = 0; i < kN; ++i) {
                const float32 fx = 0.5f + 0.45f * ::sinf(i * 2.7f + t * 0.3f);
                const float32 fy = 0.5f + 0.45f * ::cosf(i * 1.9f + t * 0.2f);
                const float32 size = 2.f + 2.f * ::fabsf(::sinf(i * 0.8f));
                const NkColor col = {
                    (uint8)(100 + 100*::fabsf(::sinf(i*0.5f))),
                    (uint8)(140 + 80*::fabsf(::cosf(i*0.7f))),
                    (uint8)(200 + 55*::fabsf(::sinf(i*1.1f))),
                    200
                };
                dl.AddCircleFilled({r.x + fx*r.w, r.y + fy*r.h}, size, col);
            }
        }

        inline void NkUIDemo::DrawHeatmap(NkUIDrawList& dl, NkRect r, NkUIContext& ctx)
        {
            static constexpr int32 kW=24, kH=8;
            const float32 cw = r.w / kW, ch = r.h / kH;
            for (int32 y = 0; y < kH; ++y) {
                for (int32 x = 0; x < kW; ++x) {
                    const float32 v = 0.5f + 0.5f * ::sinf(x*0.4f + mTime*0.5f)
                                                * ::cosf(y*0.7f + mTime*0.3f);
                    // Palette : bleu → cyan → vert → jaune → rouge
                    NkColor col;
                    if (v < 0.25f) {
                        float32 t = v/0.25f;
                        col = {0,(uint8)(t*200),(uint8)((1-t)*255),255};
                    } else if (v < 0.5f) {
                        float32 t = (v-0.25f)/0.25f;
                        col = {0,(uint8)(200+t*55),(uint8)((1-t)*200),255};
                    } else if (v < 0.75f) {
                        float32 t = (v-0.5f)/0.25f;
                        col = {(uint8)(t*255),(uint8)(255-t*80),0,255};
                    } else {
                        float32 t = (v-0.75f)/0.25f;
                        col = {255,(uint8)((1-t)*175),0,255};
                    }
                    dl.AddRectFilled({r.x+x*cw, r.y+y*ch, cw-1, ch-1}, col, 1.f);
                }
            }
            (void)ctx;
        }

        inline void NkUIDemo::DrawSpectrum(NkUIDrawList& dl, NkRect r, float32 phase)
        {
            static constexpr int32 kBands = 48;
            for (int32 i = 0; i < kBands; ++i) {
                const float32 fi   = (float32)i / kBands;
                const float32 bW   = r.w / kBands - 1.f;
                const float32 v    = (0.3f + 0.7f * ::fabsf(::sinf(fi*12.f + phase*2.f)))
                                * (0.5f + 0.5f * ::cosf(fi*7.f + phase));
                const float32 bH   = v * r.h;
                // Gradient vert → jaune → rouge
                const uint8 rb = (uint8)(v * 255);
                const uint8 gb = (uint8)((1.f - v*0.8f) * 200);
                const NkRect bar = {r.x + i*(bW+1), r.y+r.h-bH, bW, bH};
                dl.AddRectFilled(bar, {rb, gb, 40, 200}, 1.f);
                // Highlight top
                dl.AddRectFilled({bar.x, bar.y, bar.w, 2.f}, {255,255,255,100});
            }
        }

        inline void NkUIDemo::DrawNodeGraph(NkUIDrawList& dl, NkRect r,
                                            NkUIContext& ctx)
        {
            const NkVec2 offset = {r.x, r.y};

            // Connexions
            struct Conn { int32 from; int32 to; };
            static const Conn conns[] = {{0,3},{1,3},{2,3},{3,4},{3,5}};
            for (const auto& c : conns) {
                const NkVec2 a = {offset.x + mNodes[c.from].pos.x + 60,
                                offset.y + mNodes[c.from].pos.y + 12};
                const NkVec2 b = {offset.x + mNodes[c.to].pos.x,
                                offset.y + mNodes[c.to].pos.y + 12};
                const NkVec2 cp1 = {a.x + 40, a.y};
                const NkVec2 cp2 = {b.x - 40, b.y};
                dl.AddBezierCubic(a, cp1, cp2, b, {100,120,160,200}, 1.5f);
            }

            // Nodes
            for (int32 i = 0; i < kNumNodes; ++i) {
                const NkRect nr = {offset.x + mNodes[i].pos.x,
                                offset.y + mNodes[i].pos.y,
                                72.f, 26.f};
                const bool hov = ctx.IsHovered(nr);

                // Drag node
                if (hov && ctx.input.IsMouseClicked(0)) {
                    mDragNodeIdx = i;
                    mDragOffset  = {ctx.input.mousePos.x - mNodes[i].pos.x,
                                    ctx.input.mousePos.y - mNodes[i].pos.y};
                }
                if (mDragNodeIdx == i) {
                    mNodes[i].pos = {ctx.input.mousePos.x - mDragOffset.x - offset.x,
                                    ctx.input.mousePos.y - mDragOffset.y - offset.y};
                    if (!ctx.input.mouseDown[0]) mDragNodeIdx = -1;
                }

                NkColor bg = mNodes[i].color;
                bg.a = hov ? 220 : 170;
                dl.AddRectFilled(nr, bg, 4.f);
                dl.AddRect(nr, hov?NkColor{220,220,255,200}:NkColor{60,65,80,200}, 1.f, 4.f);

                // Icône de port
                dl.AddCircleFilled({nr.x+5, nr.y+13}, 3.f, {200,200,200,200});
                dl.AddCircleFilled({nr.x+nr.w-5, nr.y+13}, 3.f, {200,200,200,200});

                // Label
                const float32 fw = 8.f * ::strlen(mNodes[i].name) * 0.6f;
                ctx.layers[NkUIContext::LAYER_OVERLAY].AddText(
                    {nr.x+4, nr.y+3}, mNodes[i].name, {230,230,230,220}, 8.f);
            }
        }

        inline void NkUIDemo::DrawGizmo(NkUIDrawList& dl, NkVec2 center,
                                        float32 size, float32 rx, float32 ry)
        {
            // Axes X Y Z projetés
            auto proj = [&](float32 ax, float32 ay, float32 az) -> NkVec2 {
                // Rotation Y
                const float32 cos_y = ::cosf(ry * NKUI_PI / 180.f);
                const float32 sin_y = ::sinf(ry * NKUI_PI / 180.f);
                float32 x1 = ax*cos_y + az*sin_y;
                float32 z1 = -ax*sin_y + az*cos_y;
                float32 y1 = ay;
                // Rotation X
                const float32 cos_x = ::cosf(rx * NKUI_PI / 180.f);
                const float32 sin_x = ::sinf(rx * NKUI_PI / 180.f);
                float32 y2 = y1*cos_x - z1*sin_x;
                float32 z2 = y1*sin_x + z1*cos_x;
                (void)z2;
                return {center.x + x1*size, center.y - y2*size};
            };

            const NkVec2 o  = proj(0,0,0);
            const NkVec2 px = proj(1,0,0);
            const NkVec2 py = proj(0,1,0);
            const NkVec2 pz = proj(0,0,1);

            dl.AddLine(o, px, {220,60,60,220}, 2.f);
            dl.AddLine(o, py, {60,220,60,220}, 2.f);
            dl.AddLine(o, pz, {60,100,220,220}, 2.f);

            dl.AddCircleFilled(px, 3.5f, {220,60,60,220});
            dl.AddCircleFilled(py, 3.5f, {60,220,60,220});
            dl.AddCircleFilled(pz, 3.5f, {60,100,220,220});
        }

        // =============================================================================
        // NkUIDemoViewport — construction des meshes
        // =============================================================================

        inline void NkUIDemoViewport::BuildCube(PreviewMesh& m) {
            m.numTris = 0;
            static const float P=0.5f, N=-0.5f;
            struct Face { float v[4][3]; float nx,ny,nz; };
            static const Face faces[6] = {
                {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}}, 0,0,1},
                {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}}, 0,0,-1},
                {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}}, 0,-1,0},
                {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}}, 0,1,0},
                {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}}, -1,0,0},
                {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}}, 1,0,0},
            };
            static const int32 idx[6] = {0,1,2,0,2,3};
            for (const auto& f : faces) {
                for (int32 t = 0; t < 2; ++t) {
                    if (m.numTris >= PreviewMesh::kMaxTris) return;
                    auto& tri = m.tris[m.numTris++];
                    tri.n[0]=f.nx; tri.n[1]=f.ny; tri.n[2]=f.nz;
                    for (int32 k = 0; k < 3; ++k) {
                        const int32 vi = idx[t*3+k];
                        tri.v[k][0]=f.v[vi][0];
                        tri.v[k][1]=f.v[vi][1];
                        tri.v[k][2]=f.v[vi][2];
                    }
                }
            }
        }

        inline void NkUIDemoViewport::BuildSphere(PreviewMesh& m, int32 stacks, int32 slices) {
            m.numTris = 0;
            for (int32 i = 0; i < stacks; ++i) {
                const float32 phi0 = NKUI_PI * i / stacks;
                const float32 phi1 = NKUI_PI * (i+1) / stacks;
                for (int32 j = 0; j < slices; ++j) {
                    const float32 th0 = 2.f*NKUI_PI * j / slices;
                    const float32 th1 = 2.f*NKUI_PI * (j+1) / slices;
                    auto v = [](float32 phi, float32 th, float32 out[3]) {
                        out[0]=::sinf(phi)*::cosf(th)*0.5f;
                        out[1]=::cosf(phi)*0.5f;
                        out[2]=::sinf(phi)*::sinf(th)*0.5f;
                    };
                    float32 a[3],b[3],c2[3],d[3];
                    v(phi0,th0,a); v(phi0,th1,b); v(phi1,th0,c2); v(phi1,th1,d);
                    auto addTri=[&](float32 p0[3],float32 p1[3],float32 p2[3]){
                        if(m.numTris>=PreviewMesh::kMaxTris) return;
                        auto& t=m.tris[m.numTris++];
                        // Normal = centroid normalisé
                        for(int32 k=0;k<3;++k) {
                            t.v[0][k]=p0[k]; t.v[1][k]=p1[k]; t.v[2][k]=p2[k];
                        }
                        float32 nx=(p0[0]+p1[0]+p2[0]),ny=(p0[1]+p1[1]+p2[1]),nz=(p0[2]+p1[2]+p2[2]);
                        float32 l=::sqrtf(nx*nx+ny*ny+nz*nz);
                        if(l>0.001f){nx/=l;ny/=l;nz/=l;}
                        t.n[0]=nx;t.n[1]=ny;t.n[2]=nz;
                    };
                    addTri(a,b,d); addTri(a,d,c2);
                }
            }
        }

        inline void NkUIDemoViewport::BuildTorus(PreviewMesh& m, float32 R, float32 r,
                                                int32 rings, int32 sides)
        {
            m.numTris = 0;
            for (int32 i = 0; i < rings; ++i) {
                const float32 u0 = 2.f*NKUI_PI * i / rings;
                const float32 u1 = 2.f*NKUI_PI * (i+1) / rings;
                for (int32 j = 0; j < sides; ++j) {
                    const float32 v0 = 2.f*NKUI_PI * j / sides;
                    const float32 v1 = 2.f*NKUI_PI * (j+1) / sides;
                    auto pos = [&](float32 u, float32 v, float32 out[3]) {
                        out[0] = (R + r*::cosf(v))*::cosf(u);
                        out[1] = r*::sinf(v);
                        out[2] = (R + r*::cosf(v))*::sinf(u);
                    };
                    auto nrm = [&](float32 u, float32 v, float32 n[3]) {
                        n[0] = ::cosf(v)*::cosf(u);
                        n[1] = ::sinf(v);
                        n[2] = ::cosf(v)*::sinf(u);
                    };
                    float32 p[4][3], n[4][3];
                    pos(u0,v0,p[0]); pos(u1,v0,p[1]);
                    pos(u1,v1,p[2]); pos(u0,v1,p[3]);
                    nrm(u0,v0,n[0]); nrm(u1,v0,n[1]);
                    nrm(u1,v1,n[2]); nrm(u0,v1,n[3]);
                    auto addTri=[&](int32 a,int32 b2,int32 c2){
                        if(m.numTris>=PreviewMesh::kMaxTris) return;
                        auto& t=m.tris[m.numTris++];
                        for(int32 k=0;k<3;++k){
                            const int32 vi[]={a,b2,c2};
                            t.v[k][0]=p[vi[k]][0]; t.v[k][1]=p[vi[k]][1]; t.v[k][2]=p[vi[k]][2];
                        }
                        t.n[0]=(n[a][0]+n[b2][0]+n[c2][0])/3.f;
                        t.n[1]=(n[a][1]+n[b2][1]+n[c2][1])/3.f;
                        t.n[2]=(n[a][2]+n[b2][2]+n[c2][2])/3.f;
                    };
                    addTri(0,1,2); addTri(0,2,3);
                }
            }
        }

        inline void NkUIDemoViewport::RebuildMesh() {
            switch(meshType) {
                case 0: BuildCube(mesh);   break;
                case 1: BuildSphere(mesh); break;
                case 2: BuildTorus(mesh);  break;
            }
        }

        // =============================================================================
        // NkUIDemo — rendu 3D software
        // =============================================================================

        inline NkUIDemo::Vec3 NkUIDemo::Cross(Vec3 a, Vec3 b) {
            return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
        }
        inline float32 NkUIDemo::Dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
        inline NkUIDemo::Vec3 NkUIDemo::Normalize(Vec3 v) {
            float32 l = ::sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);
            if (l < 0.0001f) return {0,1,0};
            return {v.x/l, v.y/l, v.z/l};
        }
        inline NkUIDemo::Vec4 NkUIDemo::MulMV(const float32 m[16], Vec3 v) {
            return {m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],
                    m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],
                    m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14],
                    m[3]*v.x+m[7]*v.y+m[11]*v.z+m[15]};
        }

        inline void NkUIDemo::SoftRender3D(Pixel* fb, int32 w, int32 h,
                                            float32 rx, float32 ry,
                                            int32 meshType, bool wireframe, bool showGrid)
        {
            // Matrices
            const float32 cyaw  = ::cosf(ry*NKUI_PI/180.f), syaw  = ::sinf(ry*NKUI_PI/180.f);
            const float32 cpitch= ::cosf(rx*NKUI_PI/180.f), spitch= ::sinf(rx*NKUI_PI/180.f);

            // Mat view (rotation uniquement, camera en 0,0,3)
            float32 camDist = 3.f;
            float32 matView[16] = {
                cyaw,        0,     -syaw,       0,
                syaw*spitch, cpitch, cyaw*spitch, 0,
                syaw*cpitch,-spitch, cyaw*cpitch, 0,
                0,           0,      -camDist,   1
            };

            // Perspective (simple)
            const float32 fov  = NKUI_PI/3.f; // 60°
            const float32 asp  = (float32)w/h;
            const float32 nearPlane = 0.1f;
            const float32 farPlane = 100.f;
            const float32 f    = 1.f / ::tanf(fov*0.5f);
            float32 matProj[16] = {
                f/asp, 0,  0,                            0,
                0,     f,  0,                            0,
                0,     0, (farPlane+nearPlane)/(nearPlane-farPlane),         -1,
                0,     0, (2*farPlane*nearPlane)/(nearPlane-farPlane),        0
            };

            // MVP combiné
            auto mul44 = [](const float32 a[16], const float32 b[16], float32 out[16]) {
                for (int32 r=0;r<4;++r)
                    for (int32 c=0;c<4;++c) {
                        float32 s=0;
                        for (int32 k=0;k<4;++k) s+=a[r+k*4]*b[k+c*4];
                        out[r+c*4]=s;
                    }
            };
            float32 mvp[16];
            mul44(matView, matProj, mvp);

            // Lumière directionnelle (normalisée)
            Vec3 light = Normalize({-0.6f, 0.8f, -0.4f});

            // Dessine la grille
            if (showGrid) {
                auto projectPt=[&](Vec3 p)->Vec4 {
                    Vec4 c = MulMV(mvp, p);
                    if (c.w > 0.001f) { c.x/=c.w; c.y/=c.w; c.z/=c.w; }
                    return c;
                };
                auto toScreen=[&](Vec4 c)->NkVec2 {
                    return {(c.x*0.5f+0.5f)*(float32)w,
                            (1.f-(c.y*0.5f+0.5f))*(float32)h};
                };
                for (int32 gi = -4; gi <= 4; ++gi) {
                    Vec4 a = projectPt({-4.f*0.25f, -0.5f, gi*0.25f});
                    Vec4 b = projectPt({ 4.f*0.25f, -0.5f, gi*0.25f});
                    Vec4 c2= projectPt({gi*0.25f, -0.5f, -4.f*0.25f});
                    Vec4 d = projectPt({gi*0.25f, -0.5f,  4.f*0.25f});
                    if (a.w>0&&b.w>0&&c2.w>0&&d.w>0) {
                        NkVec2 pa=toScreen(a), pb=toScreen(b),
                            pc=toScreen(c2), pd=toScreen(d);
                        // Trace ligne horizontale et verticale
                        for (int32 s=0;s<50;++s) {
                            float32 t=(float32)s/50.f;
                            int32 px=(int32)(pa.x*(1-t)+pb.x*t);
                            int32 py=(int32)(pa.y*(1-t)+pb.y*t);
                            if(px>=0&&px<w&&py>=0&&py<h)
                                fb[py*w+px]={40,44,55,255};
                            px=(int32)(pc.x*(1-t)+pd.x*t);
                            py=(int32)(pc.y*(1-t)+pd.y*t);
                            if(px>=0&&px<w&&py>=0&&py<h)
                                fb[py*w+px]={40,44,55,255};
                        }
                    }
                }
            }

            // Construit le mesh si besoin
            NkUIDemoViewport tmp;
            tmp.meshType = meshType;
            tmp.RebuildMesh();
            const NkUIDemoViewport::PreviewMesh& mesh = tmp.mesh;

            // Rasterise les triangles (très simplifié)
            for (int32 ti = 0; ti < mesh.numTris; ++ti) {
                const auto& tri = mesh.tris[ti];
                // Back-face culling en view space
                Vec3 n = {tri.n[0], tri.n[1], tri.n[2]};

                // Projette les 3 sommets
                Vec4 cv[3];
                NkVec2 sv[3];
                bool clip = false;
                for (int32 k = 0; k < 3; ++k) {
                    Vec3 vp = {tri.v[k][0], tri.v[k][1], tri.v[k][2]};
                    cv[k] = MulMV(mvp, vp);
                    if (cv[k].w < 0.01f) { clip=true; break; }
                    cv[k].x /= cv[k].w; cv[k].y /= cv[k].w; cv[k].z /= cv[k].w;
                    sv[k] = {(cv[k].x*0.5f+0.5f)*(float32)w,
                            (1.f-(cv[k].y*0.5f+0.5f))*(float32)h};
                }
                if (clip) continue;

                // Backface
                float32 ex1 = sv[1].x-sv[0].x, ey1 = sv[1].y-sv[0].y;
                float32 ex2 = sv[2].x-sv[0].x, ey2 = sv[2].y-sv[0].y;
                float32 area = ex1*ey2 - ex2*ey1;
                if (area >= 0.f) continue; // backface

                // Diffuse
                Vec3 rotN = {
                    n.x*cyaw + n.z*syaw,
                    n.x*syaw*spitch + n.y*cpitch - n.z*cyaw*spitch,
                    -n.x*syaw*cpitch + n.y*spitch + n.z*cyaw*cpitch
                };
                float32 diff = Dot(Normalize(rotN), light);
                diff = diff < 0.f ? 0.f : diff;
                const float32 intensity = 0.2f + 0.8f * diff;

                // Couleur selon mesh type
                float32 r0=0.5f, g0=0.6f, b0=0.9f;
                if (meshType==0) { r0=0.9f; g0=0.45f; b0=0.2f; }
                else if (meshType==1) { r0=0.2f; g0=0.55f; b0=0.9f; }
                else { r0=0.7f; g0=0.2f; b0=0.7f; }

                if (wireframe) {
                    // Dessine les arêtes
                    NkColor wc = {(uint8)(r0*200+55), (uint8)(g0*200+55), (uint8)(b0*200+55), 200};
                    for (int32 e = 0; e < 3; ++e) {
                        NkVec2 a = sv[e], b2 = sv[(e+1)%3];
                        int32 steps = (int32)(::sqrtf((a.x-b2.x)*(a.x-b2.x)+(a.y-b2.y)*(a.y-b2.y)))+1;
                        for (int32 s = 0; s <= steps; ++s) {
                            float32 t=(float32)s/steps;
                            int32 px=(int32)(a.x*(1-t)+b2.x*t);
                            int32 py=(int32)(a.y*(1-t)+b2.y*t);
                            if(px>=0&&px<w&&py>=0&&py<h) {
                                fb[py*w+px].r = wc.r;
                                fb[py*w+px].g= wc.g;
                                fb[py*w+px].b = wc.b;
                                fb[py*w+px].a = wc.a;
                            }
                        }
                    }
                } else {
                    // Filled triangle — bounding box scan
                    int32 minx = (int32)::fminf(sv[0].x,::fminf(sv[1].x,sv[2].x));
                    int32 maxx = (int32)::fmaxf(sv[0].x,::fmaxf(sv[1].x,sv[2].x));
                    int32 miny = (int32)::fminf(sv[0].y,::fminf(sv[1].y,sv[2].y));
                    int32 maxy = (int32)::fmaxf(sv[0].y,::fmaxf(sv[1].y,sv[2].y));
                    if(minx<0)minx=0; if(maxx>=w)maxx=w-1;
                    if(miny<0)miny=0; if(maxy>=h)maxy=h-1;

                    for (int32 py = miny; py <= maxy; ++py) {
                        for (int32 px = minx; px <= maxx; ++px) {
                            // Barycentric test
                            float32 qx=(float32)px+0.5f, qy=(float32)py+0.5f;
                            float32 d00=ex1*ey2-ex2*ey1;
                            float32 w1=((sv[1].x-qx)*(sv[2].y-qy)-(sv[2].x-qx)*(sv[1].y-qy))/d00;
                            float32 w2=((sv[2].x-qx)*(sv[0].y-qy)-(sv[0].x-qx)*(sv[2].y-qy))/d00;
                            float32 w0=1.f-w1-w2;
                            // Edge function
                            float32 e0=(sv[1].x-sv[0].x)*(qy-sv[0].y)-(sv[1].y-sv[0].y)*(qx-sv[0].x);
                            float32 e1=(sv[2].x-sv[1].x)*(qy-sv[1].y)-(sv[2].y-sv[1].y)*(qx-sv[1].x);
                            float32 e2=(sv[0].x-sv[2].x)*(qy-sv[2].y)-(sv[0].y-sv[2].y)*(qx-sv[2].x);
                            (void)w0;(void)w1;(void)w2;
                            if (e0<=0.f || e1<=0.f || e2<=0.f) continue;
                            // Z test (approximatif)
                            float32 z = (cv[0].z+cv[1].z+cv[2].z)/3.f;
                            if (z > mZBuf[py*w+px]) continue;
                            mZBuf[py*w+px] = z;
                            fb[py*w+px] = {
                                (uint8)(r0*intensity*255),
                                (uint8)(g0*intensity*255),
                                (uint8)(b0*intensity*255),
                                255
                            };
                        }
                    }
                }
            }
        }

        // =============================================================================
        // Alias pour que NkUIColorPickerFull::State soit accessible
        // =============================================================================
        // (NkUIColorPickerFull est dans NkUILayout2.h)
    }
} // namespace nkentseu