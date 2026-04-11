/*
    NkUIDemo.h - Un laboratoire de test pour les fonctionnalités de NkUI
    Auteur: TEUGUIA TADJUIDJE Rodolf Séderis
    License: Apache-2.0

    Ce code est un exemple de démonstration pour NkUI, illustrant diverses fonctionnalités
    telles que les widgets, la gestion de la mise en page, les textures, les entrées utilisateur,
    et plus encore. Il est conçu pour être intégré dans une application utilisant NkUI et sert
    de point de départ pour explorer les capacités de la bibliothèque.

    Note: Ce code est destiné à des fins de démonstration et peut ne pas être optimisé pour
    la production. Il est recommandé de l'adapter et de l'améliorer selon les besoins spécifiques
    de votre projet.
*/

#pragma once

#include "NKUI/NKUI.h"
#include "NKUI/NkUILayout2.h"
#include "NKUI/NkUITools.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkFunction.h"

#include "NKUI/Tools/Viewport/NkUIViewport3D.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace nkentseu {
    namespace nkui {

        class NkUIDemo {
            public:
                NkUIDemo() = default;

                bool Init(NkUIContext& ctx,
                          NkUIWindowManager& wm);

                void Destroy();

                void Render(NkUIContext& ctx,
                            NkUIWindowManager& wm,
                            NkUIDockManager& dock,
                            NkUILayoutStack& ls,
                            float32 dt);

                // Définit la fonction d'upload GPU pour les atlas et les textures de démo
                void SetGPUUploadFunc(void* func, void* device = nullptr) {
                    mGPUUploadFunc = func;
                    mGPUDevice = device;
                }

            private:
                static constexpr int32 kTexW = 256;
                static constexpr int32 kTexH = 256;
                static constexpr uint32 kTexId = 100001u;
                static constexpr int32 kLogLines = 64;

                // Gestionnaire de polices intégré
                // NkUIFontManager mFontManager;
                NkUIFont*       mFont = nullptr;
                NkUIFont        mFallbackFont{};

                // Callback GPU (pour l'atlas et la texture de démo)
                void* mGPUUploadFunc = nullptr;
                void* mGPUDevice     = nullptr;

                bool mInitialized = false;
                bool mResetLayout = true;
                float32 mTime = 0.f;

                bool mShowControl = false;
                bool mShowWidgets = false;
                bool mShowLayout = false;
                bool mShowData = false;
                bool mShowTextures = false;
                bool mShowViewport = false;
                bool mShowInput = false;
                bool mShowUnicode = false;
                bool mShowZOrder = false;
                bool mShowAbout = false;
                bool mShowMenuBar = true;
                bool mShowDocking = false;
                bool mPrevShowDocking = false;
                bool mDockBootstrap = false;
                bool mShowDockInspector = false;
                bool mShowDockConsole = false;
                bool mShowDockAssets = false;

                // ── File Browser ─────────────────────────────────────────
                bool                  mShowFileBrowser = false;
                NkUIFileBrowserConfig mFBConfig;
                NkUIFileBrowserState  mFBState;
                NkUIFSProvider        mFBProvider;
                bool                  mFBInited = false;

                // ── Content Browser ─────────────────────────────────────────
                bool                  mShowContentBrowser = false;
                NkUIContentBrowserConfig mCBConfig;
                NkUIContentBrowserState  mCBState;
                NkUIFSProvider        mCBProvider;
                bool                  mCBInited = false;

                // ── 3D Scene ─────────────────────────────────────────────
                bool             mShowScene3D    = false;
                float32          mS3DYaw         = 35.f;
                float32          mS3DPitch       = 25.f;
                bool             mS3DDragging    = false;
                NkVec2           mS3DDragStart   = {};
                int32            mS3DSelectedObj = -1;
                NkUIGizmoConfig  mS3DGizmoCfg;

                NkUIGridConfig   mS3DGridCfg;    // config grille 3D
                bool             mS3DGridSolid;  // toggle plan sol plein/vide

                bool            mShowSceneViewport3D    = false;
                NkVP3DConfig    mVPConfig;
                NkVP3DState     mVPState;
                bool            mVPInited       = false;

                struct S3DObject {
                    char               name[32]  = {};
                    NkUIGizmoTransform transform = {};
                    NkUIGizmoState     gizmoState= {};
                    int32              shape     = 0; // 0=cube 1=sphere
                    NkColor            color     = {200,200,200,255};
                };
                static constexpr int32 kMaxS3DObjects = 4;
                S3DObject mS3DObjects[kMaxS3DObjects];
                int32     mNumS3DObjects = 0;

                bool mOverlapOpen[3] = {true, true, true};
                int32 mOverlapClicks[3] = {0, 0, 0};

                bool mCheckA = true;
                bool mCheckB = false;
                bool mToggle = true;
                int32 mRadio = 0;
                float32 mSlider = 0.5f;
                int32 mSliderInt = 8;
                float32 mDrag = 1.0f;
                int32 mDragInt = 42;
                int32 mCombo = 1;
                int32 mList = 0;
                bool mListMultiSelect = false;
                bool mListRequireCtrl = false;
                bool mListToggleSelection = true;
                bool mListMask[10] = {};
                float32 mProgress = 0.3f;
                bool mProgressAnim = true;

                char mInputText[256] = "NkUI test";
                char mInputMulti[1024] = "Line 1\nLine 2\nLine 3";
                int32 mInputInt = 123;
                float32 mInputFloat = 3.14f;
                NkUITextInputOptions mTextInputOpts = {};

                float32 mInnerScrollX = 0.f;
                float32 mInnerScrollY = 0.f;

                uint8 mTexturePixels[kTexW * kTexH * 4] = {};
                bool mTextureUploaded = false;
                bool mTextureDirty = true;
                bool mTextureAnimate = false;
                int32 mTexturePattern = 0;
                float32 mTextureTime = 0.f;

                float32 mViewportYaw = 0.f;
                float32 mViewportPitch = 20.f;
                bool mViewportAutoRotate = true;

                char mUnicodeInput[256] = "Hello UTF-8";

                char mLog[kLogLines][128] = {};
                int32 mLogCount = 0;

                void PushLog(const char* fmt, ...);
                void EnsureFont();
                void SetupFallbackFont();

                void GenerateTexture();
                void GenerateChecker();
                void GenerateGradient();
                void GenerateWave();
                void UploadTextureIfNeeded();

                void WinControl(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinWidgets(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinLayout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinData(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinTextures(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinViewport(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinInput(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinUnicode(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinZOrder(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinAbout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinOverlap(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls,
                                int32 idx, const char* name, NkVec2 pos, NkVec2 size);
                void RenderMainMenuBar(NkUIContext& ctx, NkUIDrawList& dl);
                void WinDockInspector(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinDockConsole(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinDockAssets(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinDocking(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls);
                void WinFileBrowser(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls);
                void WinScene3D(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls, float32 dt);
                void WinSceneVp3D(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls, float32 dt);

                void DrawViewportPreview(NkUIDrawList& dl, NkRect area, NkUIContext& ctx, float32 dt);
                void DrawGrid(NkUIDrawList& dl, NkRect r, NkColor col, int32 div);
        };

        // ============================================================================
        // IMPLÉMENTATIONS INLINE (tout le code reste ici)
        // ============================================================================

        inline void NkUIDemo::PushLog(const char* fmt, ...) {
            if (mLogCount >= kLogLines) {
                for (int32 i = 1; i < kLogLines; ++i) {
                    memory::NkCopy(mLog[i - 1], mLog[i], sizeof(mLog[i - 1]));
                }
                mLogCount = kLogLines - 1;
            }
            va_list ap;
            va_start(ap, fmt);
            ::vsnprintf(mLog[mLogCount], sizeof(mLog[mLogCount]), fmt, ap);
            va_end(ap);
            ++mLogCount;
        }

        inline void NkUIDemo::SetupFallbackFont() {
            memory::NkSet(&mFallbackFont, 0, sizeof(mFallbackFont));
            memory::NkCopy(mFallbackFont.name, "builtin", sizeof("builtin"));
            mFallbackFont.size = 16.f;
            mFallbackFont.isBuiltin = true;
            mFallbackFont.atlas = nullptr;
            mFallbackFont.metrics.ascender = 12.f;
            mFallbackFont.metrics.descender = 4.f;
            mFallbackFont.metrics.lineHeight = 18.f;
            mFallbackFont.metrics.spaceWidth = 8.f;
            mFallbackFont.metrics.tabWidth = 32.f;
        }

        inline void NkUIDemo::EnsureFont() {
            // La police vient maintenant de ctx.fontManager (chargée dans nkmain).
            // On cherche la première police non-builtin disponible via SetActiveFont.
            // mFont est un pointeur vers une police du fontManager du contexte — on
            // ne charge rien ici, on récupère juste ce qui a déjà été chargé.

            if (mFont && mFont != &mFallbackFont) return;

            // La police a été chargée dans ctx.fontManager par nkmain.
            // NkUIDemo n'a pas accès direct au ctx ici, donc on garde mFontManager
            // uniquement pour les essais locaux si ctx.fontManager était vide.
            //
            // MAIS : dans Render(), ctx.SetActiveFont est appelé avec mFont.
            // Si mFont est null, on tente de charger via mFontManager local (bitmap builtin).
            // La vraie police TTF est accessible via ctx.fontManager.Default() — voir Render().
            SetupFallbackFont();
            mFont = &mFallbackFont;
        }

        // inline void NkUIDemo::EnsureFont() {
        //     // Si déjà chargée, ne pas recharger
        //     if (mFont && mFont != &mFallbackFont) return;

        //     // Initialiser le manager s'il ne l'est pas
        //     if (mFontManager.numFonts == 0) {
        //         NkUIFontConfig cfg;
        //         cfg.yAxisUp = false;          // dépend de l'API (OpenGL : false)
        //         cfg.enableAtlas = true;
        //         cfg.enableBitmapFallback = true;
        //         mFontManager.Init(cfg);
        //         // Si on a un callback GPU, l'enregistrer pour les atlas
        //         if (mGPUUploadFunc) {
        //             mFontManager.UploadDirtyAtlases(mGPUUploadFunc);
        //         }
        //     }

        //     // Essayer de charger une police TTF depuis un fichier
        //     const char* candidates[] = {
        //         "Resources/Fonts/Roboto-Medium.ttf",
        //         "Resources/Fonts/ProggyClean.ttf",
        //         "C:/Windows/Fonts/segoeui.ttf",
        //         "C:/Windows/Fonts/arial.ttf",
        //         nullptr
        //     };

        //     int32 fontIdx = -1;
        //     for (int32 i = 0; candidates[i]; ++i) {
        //         fontIdx = mFontManager.LoadFromFile(candidates[i], 16.f, nullptr, nullptr);
        //         if (fontIdx >= 0) break;
        //     }

        //     if (fontIdx >= 0) {
        //         mFont = mFontManager.Get(static_cast<uint32>(fontIdx));
        //         // Upload de l'atlas (si le callback est déjà défini)
        //         if (mGPUUploadFunc) {
        //             mFontManager.UploadDirtyAtlases(mGPUUploadFunc);
        //         }
        //         PushLog("Police TTF chargée: %s", mFont->name);
        //     } else {
        //         SetupFallbackFont();
        //         mFont = &mFallbackFont;
        //         PushLog("Aucune police TTF trouvée, utilisation du bitmap fallback");
        //     }
        // }

        inline void NkUIDemo::GenerateChecker() {
            for (int32 y = 0; y < kTexH; ++y) {
                for (int32 x = 0; x < kTexW; ++x) {
                    const int32 i = (y * kTexW + x) * 4;
                    const bool dark = ((x / 16 + y / 16) & 1) != 0;
                    const uint8 v = dark ? 45 : 210;
                    mTexturePixels[i + 0] = v;
                    mTexturePixels[i + 1] = v;
                    mTexturePixels[i + 2] = v;
                    mTexturePixels[i + 3] = 255;
                }
            }
        }

        inline void NkUIDemo::GenerateGradient() {
            for (int32 y = 0; y < kTexH; ++y) {
                for (int32 x = 0; x < kTexW; ++x) {
                    const int32 i = (y * kTexW + x) * 4;
                    mTexturePixels[i + 0] = static_cast<uint8>((x * 255) / (kTexW - 1));
                    mTexturePixels[i + 1] = static_cast<uint8>((y * 255) / (kTexH - 1));
                    mTexturePixels[i + 2] = 180;
                    mTexturePixels[i + 3] = 255;
                }
            }
        }

        inline void NkUIDemo::GenerateWave() {
            for (int32 y = 0; y < kTexH; ++y) {
                for (int32 x = 0; x < kTexW; ++x) {
                    const float32 fx = static_cast<float32>(x) / static_cast<float32>(kTexW);
                    const float32 fy = static_cast<float32>(y) / static_cast<float32>(kTexH);
                    const float32 s0 = 0.5f + 0.5f * ::sinf(fx * 20.f + mTextureTime * 2.0f);
                    const float32 s1 = 0.5f + 0.5f * ::cosf(fy * 18.f - mTextureTime * 1.7f);
                    const uint8 r = static_cast<uint8>(50.f + 205.f * s0);
                    const uint8 g = static_cast<uint8>(30.f + 225.f * s1);
                    const uint8 b = static_cast<uint8>(80.f + 120.f * (s0 * s1));
                    const int32 i = (y * kTexW + x) * 4;
                    mTexturePixels[i + 0] = r;
                    mTexturePixels[i + 1] = g;
                    mTexturePixels[i + 2] = b;
                    mTexturePixels[i + 3] = 255;
                }
            }
        }

        inline void NkUIDemo::GenerateTexture() {
            if (mTexturePattern == 0) {
                GenerateChecker();
            } else if (mTexturePattern == 1) {
                GenerateGradient();
            } else {
                GenerateWave();
            }
            mTextureDirty = true;
        }

        inline void NkUIDemo::UploadTextureIfNeeded() {
            if (!mTextureDirty) return;
            if (!mGPUUploadFunc || !mGPUDevice) {
                // Pas encore de callback, on ne peut pas uploader
                return;
            }
            using UploadFn = bool(*)(void* device, uint32 texId, const uint8* data, int32 w, int32 h);
            mTextureUploaded = (*reinterpret_cast<UploadFn*>(&mGPUUploadFunc))(mGPUDevice, kTexId,
                                                                                mTexturePixels,
                                                                                kTexW, kTexH);
            mTextureDirty = false;
        }

        inline bool NkUIDemo::Init(NkUIContext& ctx,
                                   NkUIWindowManager& wm) {
            (void)wm;
            ctx.SetTheme(NkUITheme::Dark());

            // On n'appelle pas EnsureFont ici car on n'a pas encore le callback GPU
            // Le callback sera défini par l'application via SetGPUUploadFunc()
            // On se contente de préparer la texture de démo
            GenerateTexture();

            mTextInputOpts.repeatBackspace = true;
            mTextInputOpts.repeatDelete = true;
            mTextInputOpts.allowEnterNewLine = true;
            mTextInputOpts.multilineLineSpacing = 1.2f;
            mTextInputOpts.multilineCharSpacing = 0.35f;
            mTextInputOpts.multilineWrap = true;

            // ── File Browser ────────────────────────────────────────────
            mFBProvider = NkUIFSProvider::NativeProvider();
            mFBConfig.showFileSize = true;
            mFBConfig.showModifiedDate = false;
            mFBConfig.allowRename = false;
            mFBConfig.allowDelete = false;
            mFBConfig.allowDnD   = true;
            #if defined(_WIN32)
                ::strncpy(mFBConfig.rootPath, "Computer", sizeof(mFBConfig.rootPath)-1);
                NkUIFileBrowser::Navigate(mFBState, "Computer");
            #else
                ::strncpy(mFBConfig.rootPath, "/", sizeof(mFBConfig.rootPath)-1);
                NkUIFileBrowser::Navigate(mFBState, "/");
            #endif
            mFBInited = true;

            // ── 3D Scene ────────────────────────────────────────────────
            mNumS3DObjects = 2;
            ::strncpy(mS3DObjects[0].name, "Cube",   31);
            mS3DObjects[0].shape = 0;
            mS3DObjects[0].color = {100, 180, 255, 255};
            mS3DObjects[0].transform.position = {-2.f, 0.f, 0.f};
            mS3DObjects[0].transform.scale    = {1.f, 1.f, 1.f};

            ::strncpy(mS3DObjects[1].name, "Sphere", 31);
            mS3DObjects[1].shape = 1;
            mS3DObjects[1].color = {255, 160, 60, 255};
            mS3DObjects[1].transform.position = {2.f, 0.f, 0.f};
            mS3DObjects[1].transform.scale    = {1.f, 1.f, 1.f};

            mS3DGizmoCfg.mode       = NkUIGizmoMode::NK_TRANSLATE;
            mS3DGizmoCfg.axisMask   = NK_GIZMO_AXIS_ALL;
            mS3DGizmoCfg.axisLength = 60.f;
            mS3DGizmoCfg.showCenter = true;
            mS3DGizmoCfg.showAxisLabels = true;
            mS3DGizmoCfg.showGrid   = false;  // grille au sol déjà dessinée manuellement
            mS3DGizmoCfg.modeMask = (1 << 0) | (1 << 1) | (1 << 2); // Translate + Rotate + Scale
            mS3DGizmoCfg.showPlanes = true;
            mS3DGizmoCfg.showCenterMove = true;
            mS3DGizmoCfg.showCenterScale = true;
            mS3DGizmoCfg.snap.enabled = false; // ou true selon besoin

            // ── Viewport 3D ─────────────────────────────────────────────
            if (!mVPInited) {
                NkUIViewport3D::SetupDemoScene(mVPState);
        
                // Grille
                mVPConfig.grid.enabled    = true;
                mVPConfig.grid.showPlaneXZ= true;
                mVPConfig.grid.infinite   = true;
                mVPConfig.grid.lineCount  = 48;
                mVPConfig.grid.cellSizePx = 20.f;
                mVPConfig.grid.fadeStart  = 0.40f;
                mVPConfig.grid.solid      = false;
                mVPConfig.grid.solidColor = {25,28,35,110};
                mVPConfig.grid.lineColor  = {55,60,75,120};
                mVPConfig.grid.axisXColor = {190,55,55,200};
                mVPConfig.grid.axisYColor = {55,185,95,200};
                mVPConfig.grid.axisZColor = {55,110,190,200};
        
                mVPConfig.showStats          = true;
                mVPConfig.showOrientationCube= true;
                mVPConfig.showMiniAxes       = true;
                mVPConfig.showSelectionInfo  = true;
                mVPConfig.showGrid           = true;
                mVPConfig.outlinerW          = 220.f;
                mVPConfig.toolbarH           = 34.f;
                mVPConfig.statusBarH         = 22.f;
                mVPConfig.orbitSensitivity   = 0.5f;
                mVPConfig.panSensitivity     = 0.014f;
                mVPConfig.zoomSensitivity    = 1.2f;
                mVPConfig.snapTranslate      = 0.25f;
                mVPConfig.snapRotate         = 15.f;
                mVPConfig.snapScale          = 0.1f;
        
                mVPInited = true;
                PushLog("3D Viewport initialized");
            }

            PushLog("NkUI Test Lab initialized (attente du callback GPU)");
            mInitialized = true;
            mResetLayout = true;
            return true;
        }

        inline void NkUIDemo::Destroy() {
            mInitialized = false;
            mFont = nullptr;
            // mFontManager.Destroy();
        }

        inline void NkUIDemo::DrawGrid(NkUIDrawList& dl, NkRect r, NkColor col, int32 div) {
            if (div < 2) div = 2;
            for (int32 i = 0; i <= div; ++i) {
                const float32 t = static_cast<float32>(i) / static_cast<float32>(div);
                const float32 x = r.x + t * r.w;
                const float32 y = r.y + t * r.h;
                dl.AddLine({x, r.y}, {x, r.y + r.h}, col, 1.f);
                dl.AddLine({r.x, y}, {r.x + r.w, y}, col, 1.f);
            }
        }

        inline void NkUIDemo::DrawViewportPreview(NkUIDrawList& dl, NkRect area, NkUIContext& ctx, float32 dt) {
            if (mViewportAutoRotate) {
                mViewportYaw += dt * 35.f;
            }
            if (ctx.IsHovered(area) && ctx.input.IsMouseDown(0)) {
                mViewportYaw += ctx.input.mouseDelta.x * 0.3f;
                mViewportPitch += ctx.input.mouseDelta.y * 0.3f;
            }

            dl.AddRectFilled(area, {16, 20, 30, 255}, 4.f);
            DrawGrid(dl, area, {45, 55, 80, 180}, 10);

            const NkVec2 c = {area.x + area.w * 0.5f, area.y + area.h * 0.5f};
            const float32 rr = (area.h < area.w ? area.h : area.w) * 0.25f;
            const float32 ay = mViewportYaw * NKUI_PI / 180.f;
            const float32 ap = mViewportPitch * NKUI_PI / 180.f;

            NkVec2 pts[4] = {};
            for (int32 i = 0; i < 4; ++i) {
                const float32 a = ay + i * NKUI_PI * 0.5f;
                const float32 x = ::cosf(a) * rr;
                const float32 z = ::sinf(a) * rr;
                const float32 y = ::sinf(ap + i * 0.9f) * rr * 0.4f;
                pts[i] = {c.x + x + z * 0.4f, c.y + y - z * 0.2f};
            }

            dl.AddLine(pts[0], pts[1], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[1], pts[2], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[2], pts[3], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[3], pts[0], {240, 120, 80, 255}, 2.f);
            dl.AddLine(pts[0], pts[2], {120, 200, 255, 200}, 1.5f);
            dl.AddLine(pts[1], pts[3], {120, 200, 255, 200}, 1.5f);

            char info[96];
            ::snprintf(info, sizeof(info), "Yaw %.1f Pitch %.1f", mViewportYaw, mViewportPitch);
            if (mFont) {
                mFont->RenderText(dl, {area.x + 8.f, area.y + 16.f}, info, {220, 220, 230, 255});
            }
        }

        // ============================================================================
        // Fenêtres de démonstration (inchangées, sauf l'utilisation de mFont)
        // ============================================================================

        inline void NkUIDemo::WinControl(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({12.f, 36.f});
                NkUIWindow::SetNextWindowSize({360.f, 300.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "UI Test Lab", &mShowControl)) {
                NkUI::Text(ctx, ls, dl, *mFont, "This window toggles all test panels.");
                NkUI::Separator(ctx, ls, dl);

                NkUI::Checkbox(ctx, ls, dl, *mFont, "Widgets", mShowWidgets);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Layout + Scroll", mShowLayout);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Data (Tree/Table)", mShowData);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Textures + Images", mShowTextures);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Viewport Preview", mShowViewport);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Input Diagnostics", mShowInput);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Unicode + Font", mShowUnicode);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Z-Order Test", mShowZOrder);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "About", mShowAbout);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "File Browser", mShowFileBrowser);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "3D Scene", mShowScene3D);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "3D Viewport Scene", mShowSceneViewport3D);

                if (NkUI::Button(ctx, ls, dl, *mFont, "Reset Window Layout")) {
                    mResetLayout = true;
                    PushLog("Window layout reset requested");
                }

                char ids[160];
                ::snprintf(ids, sizeof(ids), "hot=%u active=%u focus=%u hoveredWnd=%u",
                           ctx.hotId, ctx.activeId, ctx.focusId,
                           ctx.wm ? ctx.wm->hoveredId : 0u);
                NkUI::Text(ctx, ls, dl, *mFont, ids);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinWidgets(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            static const char* comboItems[] = {"One", "Two", "Three", "Four"};
            static const char* listItems[] = {
                "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India", "Juliet"
            };

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({390.f, 36.f});
                NkUIWindow::SetNextWindowSize({420.f, 520.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Widgets", &mShowWidgets)) {
                NkUI::Button(ctx, ls, dl, *mFont, "Simple Button");
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Check A", mCheckA);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Check B", mCheckB);
                NkUI::Toggle(ctx, ls, dl, *mFont, "Toggle", mToggle);
                NkUI::RadioButton(ctx, ls, dl, *mFont, "Radio 0", mRadio, 0);
                NkUI::RadioButton(ctx, ls, dl, *mFont, "Radio 1", mRadio, 1);

                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Slider", mSlider, 0.f, 1.f);
                NkUI::SliderInt(ctx, ls, dl, *mFont, "Slider Int", mSliderInt, 0, 20);
                NkUI::DragFloat(ctx, ls, dl, *mFont, "Drag Float", mDrag, 0.02f, -10.f, 10.f);
                NkUI::DragInt(ctx, ls, dl, *mFont, "Drag Int", mDragInt, 1.f, -100, 100);

                NkUI::Checkbox(ctx, ls, dl, *mFont, "Repeat Backspace", mTextInputOpts.repeatBackspace);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Repeat Delete", mTextInputOpts.repeatDelete);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Multiline Enter Newline", mTextInputOpts.allowEnterNewLine);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Multiline Wrap", mTextInputOpts.multilineWrap);
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Multiline Spacing",
                                  mTextInputOpts.multilineLineSpacing, 1.0f, 1.8f, "%.2f");
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Multiline Char Spacing",
                                  mTextInputOpts.multilineCharSpacing, 0.0f, 3.0f, "%.2f");

                NkUI::InputText(ctx, ls, dl, *mFont, "Input", mInputText,
                                static_cast<int32>(sizeof(mInputText)), 0.f, &mTextInputOpts);
                NkUI::InputInt(ctx, ls, dl, *mFont, "Input Int", mInputInt, &mTextInputOpts);
                NkUI::InputFloat(ctx, ls, dl, *mFont, "Input Float", mInputFloat, "%.3f", &mTextInputOpts);
                NkUI::InputMultiline(ctx, ls, dl, *mFont, "Multiline", mInputMulti,
                                     static_cast<int32>(sizeof(mInputMulti)), {0.f, 110.f}, &mTextInputOpts);

                NkUI::Combo(ctx, ls, dl, *mFont, "Combo", mCombo, comboItems, 4);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "List Multi-select", mListMultiSelect);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "List Toggle Selection", mListToggleSelection);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "List Require Ctrl", mListRequireCtrl);
                if (NkUI::Button(ctx, ls, dl, *mFont, "Clear List Selection")) {
                    for (int32 i = 0; i < 10; ++i) mListMask[i] = false;
                }
                NkUIListBoxOptions listOpts{};
                listOpts.multiSelect = mListMultiSelect;
                listOpts.toggleSelection = mListToggleSelection;
                listOpts.requireCtrlForMulti = mListRequireCtrl;
                listOpts.selectedMask = mListMask;
                listOpts.selectedMaskCount = 10;
                NkUI::ListBox(ctx, ls, dl, *mFont, "List", mList, listItems, 10, 6, &listOpts);

                if (mProgressAnim) {
                    mProgress += ctx.dt * 0.15f;
                    if (mProgress > 1.f) mProgress = 0.f;
                }
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Animate Progress", mProgressAnim);
                NkUI::ProgressBar(ctx, ls, dl, mProgress, {0.f, 0.f}, "Progress");
                NkUI::ProgressBarVertical(ctx, ls, dl, mProgress, {30.f, 110.f}, "V");
                char circularLabel[24];
                ::snprintf(circularLabel, sizeof(circularLabel), "%d%%", static_cast<int32>(mProgress * 100.f));
                NkUI::ProgressBarCircular(ctx, ls, dl, *mFont, mProgress, 72.f, circularLabel);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinLayout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({826.f, 36.f});
                NkUIWindow::SetNextWindowSize({430.f, 520.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Layout + Scroll", &mShowLayout)) {
                NkUI::Text(ctx, ls, dl, *mFont, "Window scroll should appear when content overflows.");

                for (int32 i = 0; i < 22; ++i) {
                    char line[64];
                    ::snprintf(line, sizeof(line), "Overflow line %d", i);
                    NkUI::Text(ctx, ls, dl, *mFont, line);
                }

                NkUI::Separator(ctx, ls, dl);
                NkUI::Text(ctx, ls, dl, *mFont, "Inner scroll region:");

                NkRect region = NkUILayout::NextItemRect(ctx, ls, 390.f, 190.f);
                if (NkUI::BeginScrollRegion(ctx, ls, "inner_scroll_demo", region, &mInnerScrollX, &mInnerScrollY)) {
                    for (int32 i = 0; i < 60; ++i) {
                        char row[96];
                        ::snprintf(row, sizeof(row), "Row %02d  wheel here to scroll", i);
                        NkUI::Text(ctx, ls, dl, *mFont, row);
                    }
                    NkUI::EndScrollRegion(ctx, ls);
                }
                NkUILayout::AdvanceItem(ctx, ls, region);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinData(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({12.f, 350.f});
                NkUIWindow::SetNextWindowSize({400.f, 340.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Data (Tree + Table)", &mShowData)) {
                if (NkUI::TreeNode(ctx, ls, dl, *mFont, "Root##tree_root")) {
                    NkUI::Text(ctx, ls, dl, *mFont, "Child A");
                    NkUI::Text(ctx, ls, dl, *mFont, "Child B");
                    if (NkUI::TreeNode(ctx, ls, dl, *mFont, "Subtree##tree_sub")) {
                        NkUI::Text(ctx, ls, dl, *mFont, "Leaf 1");
                        NkUI::Text(ctx, ls, dl, *mFont, "Leaf 2");
                        NkUI::TreePop(ctx, ls);
                    }
                    NkUI::TreePop(ctx, ls);
                }

                NkUI::Separator(ctx, ls, dl);
                if (NkUI::BeginTable(ctx, ls, dl, *mFont, "demo_table", 3, 0.f)) {
                    NkUI::TableHeader(ctx, ls, dl, *mFont, "ID");
                    NkUI::TableHeader(ctx, ls, dl, *mFont, "Name");
                    NkUI::TableHeader(ctx, ls, dl, *mFont, "Value");

                    for (int32 i = 0; i < 8; ++i) {
                        if (NkUI::TableNextRow(ctx, ls, dl, true)) {
                            char idBuf[16];
                            char valueBuf[32];
                            ::snprintf(idBuf, sizeof(idBuf), "%d", i);
                            ::snprintf(valueBuf, sizeof(valueBuf), "%.2f", 1.0f + i * 0.37f);

                            NkUI::Text(ctx, ls, dl, *mFont, idBuf);
                            NkUI::TableNextCell(ctx, ls);
                            NkUI::Text(ctx, ls, dl, *mFont, (i & 1) ? "Item B" : "Item A");
                            NkUI::TableNextCell(ctx, ls);
                            NkUI::Text(ctx, ls, dl, *mFont, valueBuf);
                        }
                    }
                    NkUI::EndTable(ctx, ls, dl);
                }

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinTextures(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            static const char* patterns[] = {"Checker", "Gradient", "Wave"};

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({430.f, 570.f});
                NkUIWindow::SetNextWindowSize({420.f, 300.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Textures + Images", &mShowTextures)) {
                NkUI::Combo(ctx, ls, dl, *mFont, "Pattern", mTexturePattern, patterns, 3);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Animate", mTextureAnimate);

                if (NkUI::Button(ctx, ls, dl, *mFont, "Regenerate Texture")) {
                    GenerateTexture();
                    UploadTextureIfNeeded();
                    PushLog("Texture regenerated");
                }

                if (mTextureAnimate && mTexturePattern == 2) {
                    mTextureTime += ctx.dt;
                    GenerateTexture();
                    UploadTextureIfNeeded();
                }

                const char* status = mTextureUploaded ? "uploaded" : "not uploaded";
                NkUI::LabelValue(ctx, ls, dl, *mFont, "GPU texture", status);

                NkUI::ButtonImage(ctx, ls, dl, kTexId, {120.f, 90.f});

                NkRect imageRect = NkUILayout::NextItemRect(ctx, ls, 360.f, 150.f);
                dl.AddRectFilled(imageRect, {18, 18, 24, 255}, 4.f);
                dl.AddRect(imageRect, {85, 85, 100, 255}, 1.f, 4.f);
                dl.AddImage(kTexId, {imageRect.x + 4.f, imageRect.y + 4.f, imageRect.w - 8.f, imageRect.h - 8.f});
                NkUILayout::AdvanceItem(ctx, ls, imageRect);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinViewport(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({870.f, 570.f});
                NkUIWindow::SetNextWindowSize({390.f, 300.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Viewport Preview", &mShowViewport)) {
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Auto rotate", mViewportAutoRotate);

                NkRect area = NkUILayout::NextItemRect(ctx, ls, 340.f, 190.f);
                DrawViewportPreview(dl, area, ctx, ctx.dt);
                NkUILayout::AdvanceItem(ctx, ls, area);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinInput(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({1274.f, 36.f});
                NkUIWindow::SetNextWindowSize({320.f, 400.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Input Diagnostics", &mShowInput)) {
                char line[160];
                ::snprintf(line, sizeof(line), "Mouse pos: %.1f %.1f", ctx.input.mousePos.x, ctx.input.mousePos.y);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Mouse delta: %.2f %.2f", ctx.input.mouseDelta.x, ctx.input.mouseDelta.y);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Wheel V/H: %.2f %.2f", ctx.input.mouseWheel, ctx.input.mouseWheelH);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Buttons down: L=%d R=%d M=%d",
                           ctx.input.mouseDown[0] ? 1 : 0,
                           ctx.input.mouseDown[1] ? 1 : 0,
                           ctx.input.mouseDown[2] ? 1 : 0);
                NkUI::Text(ctx, ls, dl, *mFont, line);
                ::snprintf(line, sizeof(line), "Clicked: L=%d R=%d M=%d",
                           ctx.input.mouseClicked[0] ? 1 : 0,
                           ctx.input.mouseClicked[1] ? 1 : 0,
                           ctx.input.mouseClicked[2] ? 1 : 0);
                NkUI::Text(ctx, ls, dl, *mFont, line);

                ::snprintf(line, sizeof(line), "Input chars this frame: %d", ctx.input.numInputChars);
                NkUI::Text(ctx, ls, dl, *mFont, line);

                NkUI::Separator(ctx, ls, dl);
                NkUI::Text(ctx, ls, dl, *mFont, "Recent demo log:");
                for (int32 i = (mLogCount > 8 ? mLogCount - 8 : 0); i < mLogCount; ++i) {
                    NkUI::Text(ctx, ls, dl, *mFont, mLog[i]);
                }

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinUnicode(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({1274.f, 450.f});
                NkUIWindow::SetNextWindowSize({320.f, 250.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Unicode + Font", &mShowUnicode)) {
                NkUI::Text(ctx, ls, dl, *mFont, "UTF-8 samples:");
                NkUI::Text(ctx, ls, dl, *mFont, "Fr: Bonjour, ca va?");
                NkUI::Text(ctx, ls, dl, *mFont, "Es: Hola senor, accion");
                NkUI::Text(ctx, ls, dl, *mFont, "Emoji: 😀 🚀 🎮 ✅");

                NkUI::InputText(ctx, ls, dl, *mFont, "Edit UTF-8", mUnicodeInput, static_cast<int32>(sizeof(mUnicodeInput)));
                NkUI::Text(ctx, ls, dl, *mFont, mUnicodeInput);

                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinOverlap(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls,
                                          int32 idx, const char* name, NkVec2 pos, NkVec2 size) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos(pos);
                NkUIWindow::SetNextWindowSize(size);
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, name, &mOverlapOpen[idx])) {
                char caption[96];
                ::snprintf(caption, sizeof(caption), "Clicks in this window: %d", mOverlapClicks[idx]);
                NkUI::Text(ctx, ls, dl, *mFont, caption);
                if (NkUI::Button(ctx, ls, dl, *mFont, "Click Me")) {
                    ++mOverlapClicks[idx];
                }
                NkUI::Text(ctx, ls, dl, *mFont, "Drag title bar and resize borders.");
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinZOrder(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({12.f, 706.f});
                NkUIWindow::SetNextWindowSize({400.f, 160.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Z-Order Test", &mShowZOrder)) {
                NkUI::Text(ctx, ls, dl, *mFont, "Three overlap windows are opened.");
                NkUI::Text(ctx, ls, dl, *mFont, "Front window should capture click/drag.");
                NkUI::Text(ctx, ls, dl, *mFont, "Click counters must change only on top window.");
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinAbout(NkUIContext& ctx, NkUIWindowManager& wm, NkUILayoutStack& ls) {
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({1274.f, 710.f});
                NkUIWindow::SetNextWindowSize({320.f, 160.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "About", &mShowAbout, NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUI::Text(ctx, ls, dl, *mFont, "NkUI Demo Test Lab");
                NkUI::Text(ctx, ls, dl, *mFont, "Goal: validate widgets, scroll, input,");
                NkUI::Text(ctx, ls, dl, *mFont, "textures, unicode and window behavior.");
                NkUI::Text(ctx, ls, dl, *mFont, "If one panel fails, report panel name.");
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::RenderMainMenuBar(NkUIContext& ctx, NkUIDrawList& dl) {
            if (!mFont) return;
            (void)dl;
            // Menubar sur l'overlay: évite toute interaction avec des clips de fenêtres.
            NkUIDrawList& menuDL = ctx.layers[NkUIContext::LAYER_OVERLAY];
            const NkRect barR = {0.f, 0.f, static_cast<float32>(ctx.viewW), ctx.theme.metrics.titleBarHeight};
            if (!NkUIMenu::BeginMenuBar(ctx, menuDL, *mFont, barR)) return;

            if (NkUIMenu::BeginMenu(ctx, menuDL, *mFont, "File")) {
                if (NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Reset Layout")) {
                    mResetLayout = true;
                    mDockBootstrap = true;
                    PushLog("Layout reset from menu");
                }
                if (NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Regenerate Texture")) {
                    GenerateTexture();
                    UploadTextureIfNeeded();
                    PushLog("Texture regenerated from menu");
                }
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, menuDL, *mFont, "View")) {
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Docking Demo", nullptr, &mShowDocking);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "UI Test Lab", nullptr, &mShowControl);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Widgets", nullptr, &mShowWidgets);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Layout + Scroll", nullptr, &mShowLayout);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Data", nullptr, &mShowData);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Textures", nullptr, &mShowTextures);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Viewport", nullptr, &mShowViewport);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Input", nullptr, &mShowInput);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Unicode", nullptr, &mShowUnicode);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Z-Order", nullptr, &mShowZOrder);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "About", nullptr, &mShowAbout);
                NkUIMenu::Separator(ctx, menuDL);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "File Browser", nullptr, &mShowFileBrowser);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "3D Scene",     nullptr, &mShowScene3D);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "3D Viewport Scene",     nullptr, &mShowSceneViewport3D);
                NkUIMenu::EndMenu(ctx);
            }

            if (NkUIMenu::BeginMenu(ctx, menuDL, *mFont, "Dock")) {
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Inspector", nullptr, &mShowDockInspector);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Console", nullptr, &mShowDockConsole);
                NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Assets", nullptr, &mShowDockAssets);
                if (NkUIMenu::MenuItem(ctx, menuDL, *mFont, "Redock All")) {
                    mDockBootstrap = true;
                }
                NkUIMenu::EndMenu(ctx);
            }

            NkUIMenu::EndMenuBar(ctx);
        }

        inline void NkUIDemo::WinDockInspector(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mShowDockInspector || !mFont) return;
            NkUIWindowState* ws = wm.Find("Dock Inspector");
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (ws && ws->isDocked) {
                if (!ws->isOpen) {
                    mShowDockInspector = false;
                    return;
                }
                if (dock.BeginDocked(ctx, wm, dl, "Dock Inspector")) {
                    char ids[160];
                    ::snprintf(ids, sizeof(ids), "hover=%u active=%u focus=%u", ctx.hotId, ctx.activeId, ctx.focusId);
                    NkUI::Text(ctx, ls, dl, *mFont, "Dock Inspector");
                    NkUI::Text(ctx, ls, dl, *mFont, ids);
                    NkUI::Checkbox(ctx, ls, dl, *mFont, "Auto Rotate Viewport", mViewportAutoRotate);
                    NkUI::SliderFloat(ctx, ls, dl, *mFont, "Yaw", mViewportYaw, -180.f, 180.f);
                    NkUI::SliderFloat(ctx, ls, dl, *mFont, "Pitch", mViewportPitch, -89.f, 89.f);
                    dock.EndDocked(ctx, dl);
                }
                return;
            }

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({20.f, 110.f});
                NkUIWindow::SetNextWindowSize({320.f, 260.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Dock Inspector", &mShowDockInspector)) {
                char ids[160];
                ::snprintf(ids, sizeof(ids), "hover=%u active=%u focus=%u", ctx.hotId, ctx.activeId, ctx.focusId);
                NkUI::Text(ctx, ls, dl, *mFont, "Dock Inspector");
                NkUI::Text(ctx, ls, dl, *mFont, ids);
                NkUI::Checkbox(ctx, ls, dl, *mFont, "Auto Rotate Viewport", mViewportAutoRotate);
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Yaw", mViewportYaw, -180.f, 180.f);
                NkUI::SliderFloat(ctx, ls, dl, *mFont, "Pitch", mViewportPitch, -89.f, 89.f);
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinDockConsole(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mShowDockConsole || !mFont) return;
            NkUIWindowState* ws = wm.Find("Dock Console");
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (ws && ws->isDocked) {
                if (!ws->isOpen) {
                    mShowDockConsole = false;
                    return;
                }
                if (dock.BeginDocked(ctx, wm, dl, "Dock Console")) {
                    NkUI::Text(ctx, ls, dl, *mFont, "Dock Console");
                    for (int32 i = (mLogCount > 10 ? mLogCount - 10 : 0); i < mLogCount; ++i) {
                        NkUI::Text(ctx, ls, dl, *mFont, mLog[i]);
                    }
                    dock.EndDocked(ctx, dl);
                }
                return;
            }

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({20.f, 380.f});
                NkUIWindow::SetNextWindowSize({500.f, 220.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Dock Console", &mShowDockConsole)) {
                NkUI::Text(ctx, ls, dl, *mFont, "Dock Console");
                for (int32 i = (mLogCount > 10 ? mLogCount - 10 : 0); i < mLogCount; ++i) {
                    NkUI::Text(ctx, ls, dl, *mFont, mLog[i]);
                }
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinDockAssets(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mShowDockAssets || !mFont) return;
            NkUIWindowState* ws = wm.Find("Dock Assets");
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
            if (ws && ws->isDocked) {
                if (!ws->isOpen) {
                    mShowDockAssets = false;
                    return;
                }
                if (dock.BeginDocked(ctx, wm, dl, "Dock Assets")) {
                    static const char* assets[] = {
                        "textures/checker.png",
                        "textures/gradient.png",
                        "models/cube.mesh",
                        "models/sphere.mesh",
                        "scenes/demo_scene.json",
                        "fonts/arial.ttf"
                    };
                    NkUI::Text(ctx, ls, dl, *mFont, "Dock Assets");
                    for (int32 i = 0; i < 6; ++i) {
                        char label[96];
                        ::snprintf(label, sizeof(label), "Open##asset_%d", i);
                        NkUI::LabelValue(ctx, ls, dl, *mFont, assets[i], "");
                        if (NkUI::Button(ctx, ls, dl, *mFont, label, {80.f, 0.f})) {
                            PushLog("Asset action: %s", assets[i]);
                        }
                    }
                    dock.EndDocked(ctx, dl);
                }
                return;
            }

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({530.f, 380.f});
                NkUIWindow::SetNextWindowSize({360.f, 220.f});
            }
            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Dock Assets", &mShowDockAssets)) {
                static const char* assets[] = {
                    "textures/checker.png",
                    "textures/gradient.png",
                    "models/cube.mesh",
                    "models/sphere.mesh",
                    "scenes/demo_scene.json",
                    "fonts/arial.ttf"
                };
                NkUI::Text(ctx, ls, dl, *mFont, "Dock Assets");
                for (int32 i = 0; i < 6; ++i) {
                    char label[96];
                    ::snprintf(label, sizeof(label), "Open##asset_%d", i);
                    NkUI::LabelValue(ctx, ls, dl, *mFont, assets[i], "");
                    if (NkUI::Button(ctx, ls, dl, *mFont, label, {80.f, 0.f})) {
                        PushLog("Asset action: %s", assets[i]);
                    }
                }
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        inline void NkUIDemo::WinDocking(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDockManager& dock, NkUILayoutStack& ls) {
            if (!mShowDocking || !mFont) return;
            (void)ctx;
            (void)ls;

            if (mDockBootstrap && dock.rootIdx >= 0) {
                bool didDock = false;
                if (mShowDockInspector) {
                    NkUIWindowState* wi = wm.Find("Dock Inspector");
                    if (wi && !wi->isDocked) {
                        didDock |= dock.DockWindow(wm, wi->id, dock.rootIdx, NkUIDockDrop::NK_CENTER);
                    }
                }
                if (mShowDockConsole) {
                    NkUIWindowState* wc = wm.Find("Dock Console");
                    if (wc && !wc->isDocked) {
                        didDock |= dock.DockWindow(wm, wc->id, dock.rootIdx, NkUIDockDrop::NK_CENTER);
                    }
                }
                if (mShowDockAssets) {
                    NkUIWindowState* wa = wm.Find("Dock Assets");
                    if (wa && !wa->isDocked) {
                        didDock |= dock.DockWindow(wm, wa->id, dock.rootIdx, NkUIDockDrop::NK_CENTER);
                    }
                }
                mDockBootstrap = false;
                if (didDock) {
                    PushLog("Dock layout initialized");
                }
            }
        }

        inline void NkUIDemo::Render(NkUIContext& ctx,
                                     NkUIWindowManager& wm,
                                     NkUIDockManager& dock,
                                     NkUILayoutStack& ls,
                                     float32 dt) {
            // if (!mInitialized) return;

            // // Si le callback GPU a été fourni, on peut initialiser la police
            // if (mGPUUploadFunc && !mFont) {
            //     EnsureFont();
            // }

            // // Si toujours pas de police, on utilise le fallback
            // if (!mFont) {
            //     SetupFallbackFont();
            //     mFont = &mFallbackFont;
            // }

            // ctx.SetActiveFont(mFont);
            // mTime += dt;

            // // Upload de la texture de démo si nécessaire
            // UploadTextureIfNeeded();
            if (!mInitialized) return;

            // Priorité 1 : police TTF chargée dans ctx.fontManager par nkmain
            if (!mFont || mFont == &mFallbackFont) {
                NkUIFont* ctxFont = ctx.fontManager.Default();
                // Sauter la police builtin (index 0 si aucune TTF), prendre la première TTF
                for (int32 i = 0; i < ctx.fontManager.numFonts; ++i) {
                    NkUIFont* f = ctx.fontManager.Get(static_cast<uint32>(i));
                    if (f && !f->isBuiltin) { mFont = f; break; }
                }
                // Priorité 2 : n'importe quelle police du manager (même builtin)
                if (!mFont || mFont == &mFallbackFont) {
                    mFont = ctxFont;
                }
                // Priorité 3 : bitmap fallback absolu
                if (!mFont) {
                    SetupFallbackFont();
                    mFont = &mFallbackFont;
                }
            }

            ctx.SetActiveFont(mFont);
            mTime += dt;

            // Upload texture de démo si nécessaire
            UploadTextureIfNeeded();

            NkUIDrawList& bg = ctx.layers[NkUIContext::LAYER_BG];
            bg.AddRectFilled({0.f, 0.f, static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH)}, ctx.theme.colors.bgPrimary);
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];

            const float32 menuH = mShowMenuBar ? ctx.theme.metrics.titleBarHeight : 0.f;
            if (mShowMenuBar) {
                RenderMainMenuBar(ctx, dl);
            }

            if (mResetLayout) {
                mDockBootstrap = true;
            }

            if (mShowDocking) {
                const NkRect dockRect = {0.f, menuH, static_cast<float32>(ctx.viewW), static_cast<float32>(ctx.viewH) - menuH};
                dock.SetViewport(dockRect, &wm);
                dock.BeginFrame(ctx, wm, dl, *mFont);
                dock.Render(ctx, dl, *mFont, wm, ls);
            }

            // Root widgets (outside any window) to validate non-window UI path.
            // Keep them on the windows layer so popup layers (menus/combos) stay visible on top.
            {
                NkUIDrawList& root = ctx.layers[NkUIContext::LAYER_WINDOWS];
                NkUIDrawList* savedDl = ctx.dl;
                NkVec2 savedCursor = ctx.cursor;
                NkVec2 savedCursorStart = ctx.cursorStart;
                const NkUIID savedWindowId = ctx.currentWindowId;

                ctx.dl = &root;
                ctx.currentWindowId = NKUI_ID_NONE;
                ctx.SetCursor({8.f, menuH + 6.f});
                ctx.cursorStart = ctx.cursor;

                static bool rootToggle = true;
                static float32 rootOpacity = 0.75f;
                static int32 rootClicks = 0;
                static int32 rootValuePlacement = 0;
                static const char* rootValuePlacementItems[] = {
                    "Above thumb",
                    "Below thumb",
                    "Left of slider",
                    "Right of slider",
                    "In label",
                    "Hidden"
                };

                NkUISliderValueOptions rootValueOpts{};
                switch (rootValuePlacement) {
                    default:
                    case 0: rootValueOpts.placement = NkUISliderValuePlacement::NK_ABOVE_THUMB; break;
                    case 1: rootValueOpts.placement = NkUISliderValuePlacement::NK_BELOW_THUMB; break;
                    case 2: rootValueOpts.placement = NkUISliderValuePlacement::NK_LEFT_OF_SLIDER; break;
                    case 3: rootValueOpts.placement = NkUISliderValuePlacement::NK_RIGHT_OF_SLIDER; break;
                    case 4: rootValueOpts.placement = NkUISliderValuePlacement::NK_IN_LABEL; break;
                    case 5: rootValueOpts.placement = NkUISliderValuePlacement::NK_HIDDEN; break;
                }
                rootValueOpts.followThumb = true;
                rootValueOpts.gap = 4.f;

                NkUI::BeginRow(ctx, ls, 24.f);
                if (NkUI::Button(ctx, ls, root, *mFont, "Root Button##root_btn")) {
                    ++rootClicks;
                }
                NkUI::Checkbox(ctx, ls, root, *mFont, "Root Toggle##root_toggle", rootToggle);
                NkUI::SliderFloat(
                    ctx, ls, root, *mFont,
                    "Root Opacity##root_opacity", rootOpacity, 0.f, 1.f,
                    "%.2f", 260.f, &rootValueOpts);
                NkUI::EndRow(ctx, ls);

                NkUI::Combo(
                    ctx, ls, root, *mFont,
                    "Root Value Pos##root_value_pos",
                    rootValuePlacement,
                    rootValuePlacementItems,
                    6,
                    220.f);

                char rootInfo[96];
                ::snprintf(rootInfo, sizeof(rootInfo), "Root clicks: %d", rootClicks);
                NkUI::Text(ctx, ls, root, *mFont, rootInfo);

                ctx.dl = savedDl;
                ctx.cursor = savedCursor;
                ctx.cursorStart = savedCursorStart;
                ctx.currentWindowId = savedWindowId;
            }

            enum class TaskId : uint8 {
                Control,
                Widgets,
                Layout,
                Data,
                Textures,
                Viewport,
                Input,
                Unicode,
                DockInspector,
                DockConsole,
                DockAssets,
                ZOrder,
                OverlapA,
                OverlapB,
                OverlapC,
                About,
                FileBrowser,
                Scene3D,
                SceneVp3D
            };
            struct Task {
                TaskId id;
                int32 order;
            };

            Task tasks[28] = {};
            int32 taskCount = 0;
            int32 fallbackOrder = 0;

            auto taskOrder = [&](const char* windowName) -> int32 {
                if (NkUIWindowState* ws = wm.Find(windowName)) {
                    return ws->zOrder;
                }
                return fallbackOrder;
            };
            auto pushTask = [&](TaskId id, const char* windowName, bool enabled) {
                if (!enabled) {
                    ++fallbackOrder;
                    return;
                }
                tasks[taskCount++] = {id, taskOrder(windowName)};
                ++fallbackOrder;
            };

            pushTask(TaskId::Control,  "UI Test Lab",        mShowControl);
            pushTask(TaskId::Widgets,  "Widgets",            mShowWidgets);
            pushTask(TaskId::Layout,   "Layout + Scroll",    mShowLayout);
            pushTask(TaskId::Data,     "Data (Tree + Table)",mShowData);
            pushTask(TaskId::Textures, "Textures + Images",  mShowTextures);
            pushTask(TaskId::Viewport, "Viewport Preview",   mShowViewport);
            pushTask(TaskId::Input,    "Input Diagnostics",  mShowInput);
            pushTask(TaskId::Unicode,  "Unicode + Font",     mShowUnicode);
            pushTask(TaskId::DockInspector, "Dock Inspector", mShowDocking && mShowDockInspector);
            pushTask(TaskId::DockConsole,   "Dock Console",   mShowDocking && mShowDockConsole);
            pushTask(TaskId::DockAssets,    "Dock Assets",    mShowDocking && mShowDockAssets);
            pushTask(TaskId::ZOrder,   "Z-Order Test",       mShowZOrder);
            pushTask(TaskId::OverlapA, "Overlap A",          mShowZOrder);
            pushTask(TaskId::OverlapB, "Overlap B",          mShowZOrder);
            pushTask(TaskId::OverlapC, "Overlap C",          mShowZOrder);
            pushTask(TaskId::About,       "About",        mShowAbout);
            pushTask(TaskId::FileBrowser, "File Browser", mShowFileBrowser);
            pushTask(TaskId::Scene3D,     "3D Scene",     mShowScene3D);
            pushTask(TaskId::SceneVp3D,     "3D Scene VP",     mShowSceneViewport3D);

            std::sort(tasks, tasks + taskCount, [](const Task& a, const Task& b) {
                return a.order < b.order;
            });

            for (int32 i = 0; i < taskCount; ++i) {
                switch (tasks[i].id) {
                    case TaskId::Control: {
                        WinControl(ctx, wm, ls);
                    } break;
                    case TaskId::Widgets: {
                        WinWidgets(ctx, wm, ls);
                    } break;
                    case TaskId::Layout: {
                        WinLayout(ctx, wm, ls);
                    } break;
                    case TaskId::Data: {
                        WinData(ctx, wm, ls);
                    } break;
                    case TaskId::Textures: {
                        WinTextures(ctx, wm, ls);
                    } break;
                    case TaskId::Viewport: {
                        WinViewport(ctx, wm, ls);
                    } break;
                    case TaskId::Input: {
                        WinInput(ctx, wm, ls);
                    } break;
                    case TaskId::Unicode: {
                        WinUnicode(ctx, wm, ls);
                    } break;
                    case TaskId::DockInspector: {
                        WinDockInspector(ctx, wm, dock, ls);
                    } break;
                    case TaskId::DockConsole: {
                        WinDockConsole(ctx, wm, dock, ls);
                    } break;
                    case TaskId::DockAssets: {
                        WinDockAssets(ctx, wm, dock, ls);
                    } break;
                    case TaskId::ZOrder: {
                        WinZOrder(ctx, wm, ls);
                    } break;
                    case TaskId::OverlapA: {
                        WinOverlap(ctx, wm, ls, 0, "Overlap A", {520.f, 360.f}, {300.f, 180.f});
                    } break;
                    case TaskId::OverlapB: {
                        WinOverlap(ctx, wm, ls, 1, "Overlap B", {560.f, 390.f}, {300.f, 180.f});
                    } break;
                    case TaskId::OverlapC: {
                        WinOverlap(ctx, wm, ls, 2, "Overlap C", {600.f, 420.f}, {300.f, 180.f});
                    } break;
                    case TaskId::About: {
                        WinAbout(ctx, wm, ls);
                    } break;
                    case TaskId::FileBrowser: {
                        WinFileBrowser(ctx, wm, ls);
                    } break;
                    case TaskId::Scene3D: {
                        WinScene3D(ctx, wm, ls, dt);
                    } break;
                    case TaskId::SceneVp3D: {
                        WinSceneVp3D(ctx, wm, ls, dt);
                    } break;
                }
            }

            WinDocking(ctx, wm, dock, ls);

            mResetLayout = false;
        }

        // =====================================================================
        //  WinFileBrowser
        // =====================================================================
        inline void NkUIDemo::WinFileBrowser(NkUIContext& ctx,
                                              NkUIWindowManager& wm,
                                              NkUILayoutStack& ls) {
            (void)ls;
            if (!mShowFileBrowser || !mFont) return;
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];

            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({920.f, 40.f});
                NkUIWindow::SetNextWindowSize({480.f, 600.f});
            }

            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "File Browser", &mShowFileBrowser)) {
                NkUIWindowState* ws = wm.Find("File Browser");
                if (ws) {
                    const float32 tbH  = ctx.theme.metrics.titleBarHeight;
                    const float32 pad  = 4.f;
                    const NkRect contentR = {
                        ws->pos.x + pad,
                        ws->pos.y + tbH + pad,
                        ws->size.x - pad * 2.f,
                        ws->size.y - tbH - pad * 2.f
                    };
                    NkUIFileBrowserResult r = NkUIFileBrowser::Draw(ctx, dl, *mFont, 0xFB001u, contentR, mFBConfig, mFBState, mFBProvider);

                    if (r.event == NkUIFileBrowserEvent::NK_FB_FILE_SELECTED)
                        PushLog("Open: %s", r.path);
                    else if (r.event == NkUIFileBrowserEvent::NK_FB_DIR_CHANGED)
                        PushLog("Dir: %s", r.path);
                    else if (r.event == NkUIFileBrowserEvent::NK_FB_RENAME_COMMITTED)
                        PushLog("Renamed: %s -> %s", r.path, r.target);
                }
                NkUIWindow::End(ctx, wm, dl, ls);
            }

            mShowContentBrowser = true;

            if (NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "Content Browser", &mShowContentBrowser)) {
                NkUIWindowState* ws = wm.Find("Content Browser");
                if (ws) {
                    const float32 tbH  = ctx.theme.metrics.titleBarHeight;
                    const float32 pad  = 4.f;
                    const NkRect contentR = {
                        ws->pos.x + pad,
                        ws->pos.y + tbH + pad,
                        ws->size.x - pad * 2.f,
                        ws->size.y - tbH - pad * 2.f
                    };
                    NkUIContentBrowserResult r = NkUIContentBrowser::Draw(ctx, dl, *mFont, 0xFB001u, contentR, mCBConfig, mCBState, mFBProvider);

                    // if (r.event == NkUIFileBrowserEvent::NK_FB_FILE_SELECTED)
                    //     PushLog("Open: %s", r.path);
                    // else if (r.event == NkUIFileBrowserEvent::NK_FB_DIR_CHANGED)
                    //     PushLog("Dir: %s", r.path);
                    // else if (r.event == NkUIFileBrowserEvent::NK_FB_RENAME_COMMITTED)
                    //     PushLog("Renamed: %s -> %s", r.path, r.target);
                }
                NkUIWindow::End(ctx, wm, dl, ls);
            }
        }

        // =====================================================================
        //  WinScene3D
        // =====================================================================
        inline void NkUIDemo::WinScene3D(NkUIContext& ctx,
                                          NkUIWindowManager& wm,
                                          NkUILayoutStack& ls,
                                          float32 dt) {
            (void)ls;
            if (!mShowScene3D || !mFont) return;
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
 
            // Initialisation lazy
            if (!mVPInited) {
                NkUIViewport3D::SetupDemoScene(mVPState);
 
                mVPConfig.grid.enabled     = true;
                mVPConfig.grid.showPlaneXZ = true;
                mVPConfig.grid.infinite    = true;
                mVPConfig.grid.lineCount   = 48;
                mVPConfig.grid.cellSizePx  = 20.f;
                mVPConfig.grid.fadeStart   = 0.40f;
                mVPConfig.grid.solid       = false;
                mVPConfig.grid.solidColor  = {25,28,35,110};
                mVPConfig.grid.lineColor   = {55,60,75,120};
                mVPConfig.grid.axisXColor  = {190,55,55,200};
                mVPConfig.grid.axisYColor  = {55,185,95,200};
                mVPConfig.grid.axisZColor  = {55,110,190,200};
                mVPConfig.showStats           = true;
                mVPConfig.showOrientationCube = true;
                mVPConfig.showMiniAxes        = true;
                mVPConfig.showSelectionInfo   = true;
                mVPConfig.showGrid            = true;
                mVPConfig.outlinerW           = 220.f;
                mVPConfig.toolbarH            = 34.f;
                mVPConfig.statusBarH          = 22.f;
                mVPConfig.bgColor             = {25, 25, 28, 255};
                mVPConfig.bgGradTop           = {20, 22, 30, 255};
                mVPConfig.bgGradBot           = {14, 16, 22, 255};
                mVPConfig.orbitSensitivity    = 0.5f;
                mVPConfig.panSensitivity      = 0.014f;
                mVPConfig.zoomSensitivity     = 1.2f;
                mVPConfig.snapTranslate       = 0.25f;
                mVPConfig.snapRotate          = 15.f;
                mVPConfig.snapScale           = 0.1f;
 
                mVPInited = true;
                PushLog("3D Viewport initialized");
            }
 
            // Fenêtre NkUI
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({20.f, 30.f});
                NkUIWindow::SetNextWindowSize({1100.f, 680.f});
            }
 
            if (!NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "3D Scene", &mShowScene3D))
                return;
 
            NkUIWindowState* ws = wm.Find("3D Scene");
            if (!ws) { NkUIWindow::End(ctx, wm, dl, ls); return; }
 
            const float32 tbH = ctx.theme.metrics.titleBarHeight;
            const float32 pad = 2.f;
            const NkRect contentR = {
                ws->pos.x + pad,
                ws->pos.y + tbH + pad,
                ws->size.x - pad * 2.f,
                ws->size.y - tbH - pad * 2.f
            };
 
            // Draw du viewport complet
            NkVP3DResult result = NkUIViewport3D::Draw(
                ctx, dl, *mFont,
                0x0201u,
                contentR,
                mVPConfig,
                mVPState,
                dt);
 
            // Log des événements
            if (result.event == NkVP3DEvent::NK_VP3D_OBJECT_SELECTED && result.objIdx >= 0)
                PushLog("Selected: %s", mVPState.objects[result.objIdx].name);
            else if (result.event == NkVP3DEvent::NK_VP3D_OBJECT_TRANSFORMED && result.objIdx >= 0) {
                const NkVP3DObject& o = mVPState.objects[result.objIdx];
                PushLog("Moved: %s  (%.2f, %.2f, %.2f)",
                        o.name,
                        o.transform.position.x,
                        o.transform.position.y,
                        o.transform.position.z);
            }
            else if (result.event == NkVP3DEvent::NK_VP3D_OBJECT_DELETED)
                PushLog("Deleted object %d", result.objIdx);
 
            NkUIWindow::End(ctx, wm, dl, ls);
        }

        // =====================================================================
        //  WinSceneVp3D — viewport 3D software + gizmo + inspector
        // =====================================================================
        inline void NkUIDemo::WinSceneVp3D(NkUIContext& ctx,
                                          NkUIWindowManager& wm,
                                          NkUILayoutStack& ls,
                                          float32 dt) {
            (void)ls; (void)dt;
            if (!mShowSceneViewport3D || !mFont) return;
            NkUIDrawList& dl = ctx.layers[NkUIContext::LAYER_WINDOWS];
 
            if (mResetLayout) {
                NkUIWindow::SetNextWindowPos({40.f, 40.f});
                NkUIWindow::SetNextWindowSize({900.f, 620.f});
            }
 
            if (!NkUIWindow::Begin(ctx, wm, dl, *mFont, ls, "3D Scene", &mShowSceneViewport3D)) return;
 
            NkUIWindowState* ws = wm.Find("3D Scene");
            if (!ws) { NkUIWindow::End(ctx, wm, dl, ls); return; }
 
            const float32 tbH   = ctx.theme.metrics.titleBarHeight;
            const float32 toolH = 34.f;
            const float32 inspW = 220.f;
            const float32 pad   = 4.f;
 
            const NkRect winContent = {
                ws->pos.x + pad, ws->pos.y + tbH + pad,
                ws->size.x - pad*2.f, ws->size.y - tbH - pad*2.f
            };
            const NkRect toolbarR = { winContent.x, winContent.y, winContent.w, toolH };
            const NkRect inspR = {
                winContent.x + winContent.w - inspW, winContent.y + toolH + pad,
                inspW, winContent.h - toolH - pad
            };
            const NkRect viewR = {
                winContent.x, winContent.y + toolH + pad,
                winContent.w - inspW - pad, winContent.h - toolH - pad
            };
 
            // ── Toolbar ────────────────────────────────────────────────────────────
            dl.AddRectFilled(toolbarR, ctx.theme.colors.bgHeader, 4.f);
            {
                const float32 btnW=80.f, btnH=toolH-6.f;
                float32 bx=toolbarR.x+6.f;
                const float32 by=toolbarR.y+3.f;
 
                struct ModeBtn { const char* label; NkUIGizmoMode mode; };
                const ModeBtn btns[] = {
                    {"Translate", NkUIGizmoMode::NK_TRANSLATE},
                    {"Rotate",    NkUIGizmoMode::NK_ROTATE},
                    {"Scale",     NkUIGizmoMode::NK_SCALE},
                };
                for (const auto& b : btns) {
                    const bool active=(mS3DGizmoCfg.mode==b.mode);
                    const NkRect br={bx,by,btnW,btnH};
                    const bool hov=NkRectContains(br,ctx.input.mousePos);
                    dl.AddRectFilled(br, active?ctx.theme.colors.accent
                                     :(hov?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg), 3.f);
                    dl.AddRect(br, active?ctx.theme.colors.accentHover:ctx.theme.colors.border,1.f,3.f);
                    mFont->RenderText(dl,{bx+6.f,by+(btnH-mFont->size)*0.5f},
                                     b.label,ctx.theme.colors.textPrimary,btnW-8.f,false);
                    if (hov&&ctx.input.IsMouseClicked(0)) mS3DGizmoCfg.mode=b.mode;
                    bx+=btnW+4.f;
                }
 
                // Snap
                {
                    const NkRect br={bx,by,68.f,btnH};
                    const bool hov=NkRectContains(br,ctx.input.mousePos);
                    const bool on=mS3DGizmoCfg.snap.enabled;
                    dl.AddRectFilled(br,on?ctx.theme.colors.accent:(hov?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg),3.f);
                    mFont->RenderText(dl,{bx+6.f,by+(btnH-mFont->size)*0.5f},
                                     on?"Snap ON":"Snap",ctx.theme.colors.textPrimary,60.f,false);
                    if (hov&&ctx.input.IsMouseClicked(0)) mS3DGizmoCfg.snap.enabled=!on;
                    bx+=72.f;
                }
 
                // Toggle sol plein/wireframe
                {
                    const NkRect br={bx,by,68.f,btnH};
                    const bool hov=NkRectContains(br,ctx.input.mousePos);
                    const bool solid=mS3DGridCfg.solid;
                    dl.AddRectFilled(br,solid?ctx.theme.colors.accent:(hov?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg),3.f);
                    mFont->RenderText(dl,{bx+6.f,by+(btnH-mFont->size)*0.5f},
                                     solid?"Sol plein":"Sol vide",ctx.theme.colors.textPrimary,60.f,false);
                    if (hov&&ctx.input.IsMouseClicked(0)) mS3DGridCfg.solid=!solid;
                    bx+=72.f;
                }
 
                // Toggle grille
                {
                    const NkRect br={bx,by,60.f,btnH};
                    const bool hov=NkRectContains(br,ctx.input.mousePos);
                    const bool ge=mS3DGridCfg.enabled;
                    dl.AddRectFilled(br,ge?ctx.theme.colors.accent:(hov?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg),3.f);
                    mFont->RenderText(dl,{bx+6.f,by+(btnH-mFont->size)*0.5f},
                                     ge?"Grille":"[Grid]",ctx.theme.colors.textPrimary,52.f,false);
                    if (hov&&ctx.input.IsMouseClicked(0)) mS3DGridCfg.enabled=!ge;
                    bx+=64.f;
                }
                (void)bx;
            }
 
            // ── Viewport 3D ────────────────────────────────────────────────────────
            dl.PushClipRect(viewR);
            dl.AddRectFilled(viewR, {14,18,28,255}, 2.f);
 
            const NkVec2 mp   = ctx.input.mousePos;
            const bool inView = NkRectContains(viewR, mp);
            const bool gizmoWasActive =
                (mS3DSelectedObj >= 0 && mS3DSelectedObj < mNumS3DObjects) &&
                (mS3DObjects[mS3DSelectedObj].gizmoState.active ||
                 mS3DObjects[mS3DSelectedObj].gizmoState.hovered);
 
            const bool leftClick = inView && ctx.input.IsMouseClicked(0) && !gizmoWasActive;
 
            // Orbite caméra
            if (inView && ctx.input.IsMouseDown(0) && !mS3DDragging && !gizmoWasActive) {
                if (!ctx.input.IsMouseClicked(0)) mS3DDragging=true;
                else mS3DDragStart=mp;
            }
            if (mS3DDragging) {
                const float32 ddx=mp.x-mS3DDragStart.x, ddy=mp.y-mS3DDragStart.y;
                if (ddx*ddx+ddy*ddy>9.f&&ctx.input.IsMouseDown(0)) {
                    mS3DYaw+=ctx.input.mouseDelta.x*0.5f;
                    mS3DPitch+=ctx.input.mouseDelta.y*0.3f;
                    if (mS3DPitch> 89.f) mS3DPitch= 89.f;
                    if (mS3DPitch<-89.f) mS3DPitch=-89.f;
                }
                if (!ctx.input.IsMouseDown(0)) mS3DDragging=false;
            }
 
            const float32 yawR=mS3DYaw*NKUI_PI/180.f, pitchR=mS3DPitch*NKUI_PI/180.f;
            const NkVec2  vctr={viewR.x+viewR.w*0.5f, viewR.y+viewR.h*0.5f};
            const float32 scale=70.f, camZ=10.f;
 
            auto project=[&](float wx,float wy,float wz)->NkVec2{
                const float cy=::cosf(yawR),sy=::sinf(yawR);
                const float x1=wx*cy-wz*sy, z1=wx*sy+wz*cy;
                const float cp=::cosf(pitchR),sp=::sinf(pitchR);
                const float y2=wy*cp-z1*sp, z2=wy*sp+z1*cp;
                const float dz=z2+camZ, d=(dz>0.1f)?(camZ/dz):0.f;
                return {vctr.x+x1*d*scale, vctr.y-y2*d*scale};
            };
 
            // ── Grille infinie ──────────────────────────────────────────────────────
            // On calcule les directions d'axe projetées pour la grille
            {
                const NkVec2 o2  = project(0.f,0.f,0.f);
                const NkVec2 px2 = project(1.f,0.f,0.f);
                const NkVec2 py2 = project(0.f,1.f,0.f);
                const NkVec2 pz2 = project(0.f,0.f,1.f);
 
                auto norm2=[](NkVec2 v)->NkVec2{
                    const float32 l=::sqrtf(v.x*v.x+v.y*v.y);
                    return l>1e-6f?NkVec2{v.x/l,v.y/l}:NkVec2{1.f,0.f};};
 
                NkUIGizmo3DDesc gridDesc;
                gridDesc.viewport     = viewR;
                gridDesc.originPx     = o2;  // grille centrée sur l'origine monde
                gridDesc.axisXDirPx   = norm2({px2.x-o2.x, px2.y-o2.y});
                gridDesc.axisYDirPx   = norm2({py2.x-o2.x, py2.y-o2.y});
                gridDesc.axisZDirPx   = norm2({pz2.x-o2.x, pz2.y-o2.y});
                gridDesc.unitsPerPixel= 1.f/scale;
 
                NkUIGizmo::DrawGrid3D(mS3DGridCfg, dl, gridDesc, mS3DGizmoCfg);
            }
 
            // ── Objets ──────────────────────────────────────────────────────────────
            for (int32 oi=0;oi<mNumS3DObjects;++oi) {
                S3DObject& obj=mS3DObjects[oi];
                const bool isSel=(mS3DSelectedObj==oi);
                const NkColor col=isSel?NkColor{255,240,80,255}:obj.color;
                const float32 px=obj.transform.position.x;
                const float32 py=obj.transform.position.y;
                const float32 pz=obj.transform.position.z;
                const float32 sx=::fmaxf(0.01f,obj.transform.scale.x);
                const float32 sy=::fmaxf(0.01f,obj.transform.scale.y);
                const float32 sz=::fmaxf(0.01f,obj.transform.scale.z);
                const NkVec2 center2D=project(px,py,pz);
 
                if (obj.shape==0) {
                    const float32 hx=sx*0.5f,hy=sy*0.5f,hz=sz*0.5f;
                    NkVec2 v[8];
                    v[0]=project(px-hx,py-hy,pz-hz); v[1]=project(px+hx,py-hy,pz-hz);
                    v[2]=project(px+hx,py+hy,pz-hz); v[3]=project(px-hx,py+hy,pz-hz);
                    v[4]=project(px-hx,py-hy,pz+hz); v[5]=project(px+hx,py-hy,pz+hz);
                    v[6]=project(px+hx,py+hy,pz+hz); v[7]=project(px-hx,py+hy,pz+hz);
                    const int32 edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
                    const float32 lw=isSel?2.f:1.2f;
                    for (const auto& e:edges) dl.AddLine(v[e[0]],v[e[1]],col,lw);
                } else {
                    const int32 seg=24; const float32 r=sx; const float32 lw=isSel?2.f:1.2f;
                    for (int32 plane=0;plane<3;++plane){
                        NkVec2 prev{};
                        for (int32 k=0;k<=seg;++k){
                            const float32 a=k*NKUI_PI*2.f/(float32)seg;
                            const float32 ca=::cosf(a)*r,sa=::sinf(a)*r;
                            NkVec2 pt;
                            if (plane==0) pt=project(px+ca,py+sa,pz);
                            else if(plane==1) pt=project(px+ca,py,pz+sa);
                            else pt=project(px,py+ca,pz+sa);
                            if (k>0) dl.AddLine(prev,pt,col,lw);
                            prev=pt;
                        }
                    }
                }
 
                if (isSel) dl.AddCircle(center2D,16.f,{255,240,80,120},16,3.f);
 
                if (leftClick){
                    const float32 ddx=mp.x-center2D.x,ddy=mp.y-center2D.y;
                    if (ddx*ddx+ddy*ddy<=576.f) // 24px²
                        mS3DSelectedObj=(mS3DSelectedObj==oi)?-1:oi;
                }
 
                mFont->RenderText(dl,{center2D.x+8.f,center2D.y-mFont->size-2.f},obj.name,col,120.f,false);
            }
 
            // ── Gizmo ───────────────────────────────────────────────────────────────
            if (mS3DSelectedObj >= 0 && mS3DSelectedObj < mNumS3DObjects) {
                S3DObject& obj=mS3DObjects[mS3DSelectedObj];
                const float32 px=obj.transform.position.x;
                const float32 py=obj.transform.position.y;
                const float32 pz=obj.transform.position.z;
 
                const NkVec2 orig=project(px,py,pz);
                const NkVec2 axX =project(px+1.f,py,pz);
                const NkVec2 axY =project(px,py+1.f,pz);
                const NkVec2 axZ =project(px,py,pz+1.f);
 
                auto norm2=[](NkVec2 v)->NkVec2{
                    const float32 l=::sqrtf(v.x*v.x+v.y*v.y);
                    return l>0.001f?NkVec2{v.x/l,v.y/l}:NkVec2{1.f,0.f};};
 
                NkUIGizmo3DDesc gdesc;
                gdesc.viewport      = viewR;
                gdesc.originPx      = orig;
                gdesc.axisXDirPx    = norm2({axX.x-orig.x, axX.y-orig.y});
                gdesc.axisYDirPx    = norm2({axY.x-orig.x, axY.y-orig.y});
                gdesc.axisZDirPx    = norm2({axZ.x-orig.x, axZ.y-orig.y});
                gdesc.unitsPerPixel = 1.f/scale;
                // Note : axisLength vient de mS3DGizmoCfg.axisLength (PAS de gdesc)
 
                NkUIGizmoResult gr = NkUIGizmo::Manipulate3D(
                    ctx, dl, gdesc, mS3DGizmoCfg,
                    obj.gizmoState, obj.transform);
 
                if (gr.changed) {
                    obj.transform.position.x += gr.deltaPosition.x;
                    obj.transform.position.y += gr.deltaPosition.y;
                    obj.transform.position.z += gr.deltaPosition.z;
                    // La rotation est déjà appliquée dans Manipulate3D (transform.rotationDeg)
                    obj.transform.scale.x = ::fmaxf(0.01f, obj.transform.scale.x + gr.deltaScale.x);
                    obj.transform.scale.y = ::fmaxf(0.01f, obj.transform.scale.y + gr.deltaScale.y);
                    obj.transform.scale.z = ::fmaxf(0.01f, obj.transform.scale.z + gr.deltaScale.z);
                }
            }
 
            // Info viewport
            {
                char info[64];
                ::snprintf(info,sizeof(info),"Yaw %.0f  Pitch %.0f",mS3DYaw,mS3DPitch);
                mFont->RenderText(dl,{viewR.x+8.f,viewR.y+8.f},info,{150,160,180,200});
                mFont->RenderText(dl,{viewR.x+8.f,viewR.y+viewR.h-mFont->size-6.f},
                                 "LMB: orbit  |  Click: select  |  Gizmo: drag axis  |  Sol: bouton toolbar",
                                 {120,130,150,180},viewR.w-16.f,false);
            }
 
            dl.PopClipRect();
 
            // ── Inspecteur ─────────────────────────────────────────────────────────
            dl.AddRectFilled(inspR,ctx.theme.colors.bgSecondary,3.f);
            dl.AddRect(inspR,ctx.theme.colors.border,1.f,3.f);
            {
                float32 iy=inspR.y+8.f;
                const float32 ix=inspR.x+8.f, iw=inspR.w-16.f;
                const float32 rowH=24.f;
 
                mFont->RenderText(dl,{ix,iy},"Objects",ctx.theme.colors.textPrimary);
                iy+=mFont->size+8.f;
                dl.AddLine({ix,iy},{ix+iw,iy},ctx.theme.colors.separator.WithAlpha(100),1.f);
                iy+=6.f;
 
                for (int32 oi=0;oi<mNumS3DObjects;++oi){
                    const bool isSel=(mS3DSelectedObj==oi);
                    const NkRect row={ix,iy,iw,rowH};
                    if (isSel) dl.AddRectFilled(row,ctx.theme.colors.accent.WithAlpha(80),3.f);
                    else if (NkRectContains(row,ctx.input.mousePos))
                        dl.AddRectFilled(row,ctx.theme.colors.buttonHover,3.f);
                    const char* ico=mS3DObjects[oi].shape==0?"[C]":"[S]";
                    const float32 ty=(rowH-mFont->size)*0.5f;
                    mFont->RenderText(dl,{ix+4.f,iy+ty},ico,mS3DObjects[oi].color,32.f,false);
                    mFont->RenderText(dl,{ix+36.f,iy+ty},mS3DObjects[oi].name,
                                     isSel?ctx.theme.colors.textPrimary:ctx.theme.colors.textSecondary,
                                     iw-38.f,false);
                    if (NkRectContains(row,ctx.input.mousePos)&&ctx.input.IsMouseClicked(0))
                        mS3DSelectedObj=(mS3DSelectedObj==oi)?-1:oi;
                    iy+=rowH;
                }
 
                if (mS3DSelectedObj>=0&&mS3DSelectedObj<mNumS3DObjects) {
                    S3DObject& obj=mS3DObjects[mS3DSelectedObj];
                    iy+=8.f;
                    dl.AddLine({ix,iy},{ix+iw,iy},ctx.theme.colors.separator.WithAlpha(100),1.f);
                    iy+=8.f;
                    mFont->RenderText(dl,{ix,iy},"Transform",ctx.theme.colors.textPrimary);
                    iy+=mFont->size+6.f;
 
                    auto lv=[&](const char* lbl, float32 val){
                        mFont->RenderText(dl,{ix,iy},lbl,ctx.theme.colors.textSecondary,36.f,false);
                        char buf[32]; ::snprintf(buf,sizeof(buf),"%.3f",val);
                        mFont->RenderText(dl,{ix+40.f,iy},buf,ctx.theme.colors.inputText,iw-42.f,false);
                        iy+=mFont->size+3.f;
                    };
 
                    mFont->RenderText(dl,{ix,iy},"Pos",ctx.theme.colors.accent,iw,false);
                    iy+=mFont->size+2.f;
                    lv("X:",obj.transform.position.x);
                    lv("Y:",obj.transform.position.y);
                    lv("Z:",obj.transform.position.z);
 
                    mFont->RenderText(dl,{ix,iy},"Rot",ctx.theme.colors.accent,iw,false);
                    iy+=mFont->size+2.f;
                    lv("X:",obj.transform.rotationDeg.x);
                    lv("Y:",obj.transform.rotationDeg.y);
                    lv("Z:",obj.transform.rotationDeg.z);
 
                    mFont->RenderText(dl,{ix,iy},"Scale",ctx.theme.colors.accent,iw,false);
                    iy+=mFont->size+2.f;
                    lv("X:",obj.transform.scale.x);
                    lv("Y:",obj.transform.scale.y);
                    lv("Z:",obj.transform.scale.z);
 
                    iy+=6.f;
                    const float32 bh=mFont->size+10.f;
                    const NkRect resetR={ix,iy,iw,bh};
                    const bool rH=NkRectContains(resetR,ctx.input.mousePos);
                    dl.AddRectFilled(resetR,rH?ctx.theme.colors.buttonHover:ctx.theme.colors.buttonBg,3.f);
                    dl.AddRect(resetR,ctx.theme.colors.border,1.f,3.f);
                    mFont->RenderText(dl,{ix+8.f,iy+(bh-mFont->size)*0.5f},
                                     "Reset Transform",ctx.theme.colors.textPrimary,iw-10.f,false);
                    if (rH&&ctx.input.IsMouseClicked(0)){
                        obj.transform.position    = {};
                        obj.transform.rotationDeg = {};
                        obj.transform.scale       = {1.f,1.f,1.f};
                        obj.transform.position.x  = (mS3DSelectedObj==1)?2.f:-2.f;
                    }
                } else {
                    iy+=14.f;
                    mFont->RenderText(dl,{ix,iy},"Cliquez un objet\npour inspecter.",
                                     ctx.theme.colors.textSecondary,iw,false);
                }
            }
 
            NkUIWindow::End(ctx, wm, dl, ls);
        }

    }
}