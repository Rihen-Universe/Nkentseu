#pragma once
/**
 * @File    NkUILayout2.h
 * @Brief   SaveLayout / LoadLayout JSON + ColorPicker complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NkUI/NkUIWindow.h"
#include "NkUI/NkUIDock.h"
#include "NkUI/NkUIFont.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  SaveLayout / LoadLayout — persistance de la configuration UI
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUILayoutSerializer {
            /**
             * @Brief Sauvegarde la configuration des fenêtres et du dock en JSON.
             * Format :
             * {
             *   "windows": [
             *     {"name":"Propriétés","x":100,"y":100,"w":400,"h":300,
             *      "open":true,"collapsed":false,"scrollY":0.0}
             *   ],
             *   "dock": {
             *     "nodes": [
             *       {"id":0,"type":"splitH","ratio":0.5,"children":[1,2]},
             *       {"id":1,"type":"leaf","windows":["Propriétés","Console"],"active":0},
             *       {"id":2,"type":"leaf","windows":["Viewport"],"active":0}
             *     ],
             *     "root":0
             *   }
             * }
             */
            static bool Save(const NkUIWindowManager& wm,
                            const NkUIDockManager&   dm,
                            const char* path) noexcept;

            static bool Load(NkUIWindowManager& wm,
                            NkUIDockManager&   dm,
                            const char* path) noexcept;

            // En mémoire (buffer à libérer avec free())
            static bool SaveToMemory(const NkUIWindowManager& wm,
                                    const NkUIDockManager&   dm,
                                    char*& outJson,
                                    usize& outLen) noexcept;

            static bool LoadFromMemory(NkUIWindowManager& wm,
                                        NkUIDockManager&   dm,
                                        const char* json, usize len) noexcept;
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  ColorPicker complet — HSV + sliders RGB/A + hex input
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIColorPickerFull {
            /**
             * @Brief Affiche un color picker complet dans un popup ou une fenêtre.
             * @return true si la couleur a changé.
             *
             * Composants :
             *   - Carré SV (saturation/valeur) avec gradient
             *   - Barre de teinte (hue strip)
             *   - Barre d'opacité (alpha strip) avec damier
             *   - Sliders RGBA (0-255)
             *   - Input hexadécimal (#RRGGBBAA)
             *   - Prévisualisation ancienne/nouvelle couleur
             */
            static bool Draw(NkUIContext& ctx,
                            NkUIDrawList& dl,
                            NkUIFont& font,
                            NkUILayoutStack& ls,
                            const char* id,
                            NkColor& color) noexcept;

            // Conversions HSV ↔ RGB
            static void RGBtoHSV(uint8 r,uint8 g,uint8 b,
                                float32& h,float32& s,float32& v) noexcept;
            static void HSVtoRGB(float32 h,float32 s,float32 v,
                                uint8& r,uint8& g,uint8& b) noexcept;

        private:
            // État persistant par ID
            struct State {
                NkUIID  id=0;
                float32 h=0,s=1,v=1,a=1;  // HSV + alpha [0,1]
                char    hexBuf[12]="#FFFFFFFF";
                bool    hexDirty=false;
            };
            static State sStates[16];
            static int32 sNumStates;
            static State* FindOrCreate(NkUIID id) noexcept;

            static void DrawSVSquare (NkUIContext& ctx,NkUIDrawList& dl,
                                    NkRect r,float32 h,float32& s,float32& v) noexcept;
            static void DrawHueBar   (NkUIContext& ctx,NkUIDrawList& dl,
                                    NkRect r,float32& h) noexcept;
            static void DrawAlphaBar (NkUIContext& ctx,NkUIDrawList& dl,
                                    NkRect r,float32 h,float32 s,float32 v,
                                    float32& a) noexcept;
            static void DrawPreview  (NkUIDrawList& dl,NkRect r,
                                    NkColor oldCol,NkColor newCol) noexcept;
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  Intégration NKFont dans NkUIFont
        //  (activé avec #define NKUI_WITH_NKFONT avant d'inclure NkUI.h)
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIFontNKFont {
            /**
             * @Brief Charge une police TTF/OTF/WOFF/WOFF2 via NKFont
             *        et l'intègre dans un NkUIFont pour le rendu NkUI.
             *
             * Nécessite NKUI_WITH_NKFONT.
             * La police est rastérisée dans un atlas 512×512 Gray8.
             *
             * @param fontData  Données brutes du fichier police
             * @param dataSize  Taille en octets
             * @param pixelSize Taille en pixels (ex: 14.f)
             * @param atlas     Atlas de destination
             * @param outFont   Police de sortie
             * @return true si succès
             */
            static bool Load(const uint8* fontData, usize dataSize,
                            float32 pixelSize,
                            NkUIFontAtlas& atlas,
                            NkUIFont& outFont) noexcept;

            /// Charge depuis un fichier .ttf/.otf/.woff/.woff2
            static bool LoadFile(const char* path,
                                float32 pixelSize,
                                NkUIFontAtlas& atlas,
                                NkUIFont& outFont) noexcept;

            /// Ajoute une plage de glyphes à l'atlas (ex: ASCII 32-127, Latin 128-255)
            static bool AddGlyphRange(NkUIFont& font,
                                    NkUIFontAtlas& atlas,
                                    uint32 first, uint32 last) noexcept;
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIOpenGLRenderer — backend GPU OpenGL (temps réel)
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIOpenGLRenderer {
            /**
             * @Brief Backend OpenGL pour NkUIRenderer.
             *
             * Nécessite un contexte OpenGL 3.3+ Core Profile actif.
             * Gère : VAO/VBO/EBO, shader GLSL, atlas texture, clip rect scissor.
             *
             * Usage :
             *   NkUIOpenGLRenderer gl;
             *   gl.Init(1280, 720);
             *   // ... boucle ...
             *   gl.BeginFrame(1280, 720);
             *   gl.Submit(ctx);
             *   gl.EndFrame();
             *   gl.Destroy();
             */

            // ── Shaders GLSL (inlined) ────────────────────────────────────────────────
            static const char* kVertSrc;
            static const char* kFragSrc;

            // ── État OpenGL ───────────────────────────────────────────────────────────
            uint32  mVAO=0, mVBO=0, mEBO=0;
            uint32  mShader=0;
            uint32  mFontTex=0;
            int32   mW=0, mH=0;
            bool    mInitialized=false;

            bool    Init(int32 w, int32 h) noexcept;
            void    Destroy() noexcept;
            void    BeginFrame(int32 w, int32 h) noexcept;
            void    Submit(const NkUIContext& ctx) noexcept;
            void    EndFrame() noexcept;

            uint32  UploadTexture(const uint8* pixels,
                                int32 w, int32 h, int32 channels) noexcept;
            void    FreeTexture(uint32 id) noexcept;

            void    SubmitDrawList(const NkUIDrawList& dl, int32 fbW, int32 fbH) noexcept;

            // Compile un shader, retourne l'ID GL ou 0 si erreur
            static uint32 CompileShader(uint32 type, const char* src) noexcept;
            static uint32 LinkProgram(uint32 vert, uint32 frag) noexcept;
        };
    }
} // namespace nkentseu
