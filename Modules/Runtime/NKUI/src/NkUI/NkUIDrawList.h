#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Draw command stream and primitive emission contract.
 * Main data: Vertices, indices, draw commands, clip stack definitions.
 * Change this file when: Renderer command model or primitive format changes.
 */
/**
 * @File    NkUIDrawList.h
 * @Brief   Liste de commandes de dessin — le cœur du rendu NkUI.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkUIDrawList est une liste de commandes de dessin qui peuvent être
 *  envoyées à n'importe quel backend (CPU NkImage, OpenGL, Vulkan, DX11).
 *  Inspiré de ImDrawList mais avec :
 *    - Support des paths SVG
 *    - Gradients linéaires et radiaux
 *    - Clip rects empilés
 *    - Textures/Images
 *    - Primitives texte
 *    - Coins arrondis par coin individuellement
 *
 *  Mémoire : buffer circulaire fixe dans une NkMemArena.
 *  Pas de malloc/free — tout est réinitialisé à chaque frame.
 *
 *  Architecture :
 *    DrawCmd  : une commande (type + données)
 *    DrawList : tableau de DrawCmd + vertex/index buffers
 *    La sérialisation pour le GPU est faite par le backend.
 */
#include "NkUI/NkUIExport.h"

namespace nkentseu {
    namespace nkui {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Vertex — structure minimale pour le GPU
        // ─────────────────────────────────────────────────────────────────────────────
        struct NkUIVertex {
            NkVec2  pos;
            NkVec2  uv;    // [0,1] — (0,0) = couleur solide (pas de texture)
            uint32  col;   // RGBA packed
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  Commandes de dessin
        // ─────────────────────────────────────────────────────────────────────────────
        enum class NkUIDrawCmdType : uint8 {
            NK_TRIANGLES,     // liste de triangles (vertex + index)
            NK_TEXTURED_TRIS, // triangles avec texture
            NK_CLIP_RECT,     // push/pop clip rect
            NK_SET_FONT,      // change de police
        };

        struct NKUI_API NkUIDrawCmd {
            NkUIDrawCmdType type     = NkUIDrawCmdType::NK_TRIANGLES;
            uint32          idxOffset= 0;
            uint32          idxCount = 0;
            uint32          texId    = 0;   // handle texture (atlas font ou image)
            NkRect          clipRect = {};
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  NkUIDrawList
        // ─────────────────────────────────────────────────────────────────────────────
        struct NKUI_API NkUIDrawList {
            // Buffers de géométrie
            NkUIVertex* vtx     = nullptr;
            uint32*     idx     = nullptr;
            uint32      vtxCount= 0;
            uint32      idxCount= 0;
            uint32      vtxCap  = 0;
            uint32      idxCap  = 0;

            // Commandes
            NkUIDrawCmd* cmds   = nullptr;
            uint32       cmdCount=0;
            uint32       cmdCap = 0;

            // Pile de clip rects (max 32)
            NkRect  clipStack[32]= {};
            int32   clipDepth    = 0;

            // Rects opaques déjà dessinés (pour l'occlusion — reset par Reset())
            NkRect  opaqueRects[64] = {};
            int32   opaqueCount     = 0;

            // Style courant
            NkColor fillColor    = NkColor::White();
            NkColor strokeColor  = NkColor::Black();
            float32 strokeWidth  = 1.f;
            float32 cornerRadius = 4.f;
            uint32  fontId       = 0;

            // ── Initialisation / Reset ────────────────────────────────────────────────
            bool Init(uint32 maxVtx=65536, uint32 maxIdx=196608, uint32 maxCmds=512) noexcept;
            void Destroy() noexcept;
            void Reset()   noexcept;   // chaque début de frame

            // ── Clip rect ─────────────────────────────────────────────────────────────
            void PushClipRect(NkRect r, bool intersect=true) noexcept;
            void PopClipRect() noexcept;
            NkRect GetClipRect() const noexcept {
                return clipDepth>0?clipStack[clipDepth-1]:NkRect{0,0,1e9f,1e9f};
            }

            // ── Primitives basse couche ───────────────────────────────────────────────
            void AddTriangle(NkVec2 a,NkVec2 b,NkVec2 c,NkColor col) noexcept;
            void AddTriangleFilled(NkVec2 a,NkVec2 b,NkVec2 c,NkColor col) noexcept;
            void AddLine(NkVec2 a,NkVec2 b,NkColor col,float32 thickness=1.f) noexcept;
            void AddPolyline(const NkVec2* pts,int32 n,NkColor col, float32 thickness=1.f,bool closed=false) noexcept;
            void AddConvexPolyFilled(const NkVec2* pts,int32 n,NkColor col) noexcept;

            // ── Rectangles ────────────────────────────────────────────────────────────
            // rx[0]=TL, rx[1]=TR, rx[2]=BR, rx[3]=BL — 0=carré
            void AddRect(NkRect r,NkColor col,float32 thickness=1.f, float32 rx=0,float32 ry=0) noexcept;
            void AddRectFilled(NkRect r,NkColor col, float32 rx=0,float32 ry=0) noexcept;
            void AddRectFilledMultiColor(NkRect r, NkColor colTL,NkColor colTR, NkColor colBR,NkColor colBL) noexcept;
            // Coins individuels : masque = TL|TR|BR|BL (bits 0-3)
            void AddRectFilledCorners(NkRect r,NkColor col, float32 radius,uint8 cornerMask=0xF) noexcept;

            // ── Cercles / Ellipses ────────────────────────────────────────────────────
            void AddCircle(NkVec2 c,float32 r,NkColor col, float32 thickness=1.f,int32 segs=0) noexcept;
            void AddCircleFilled(NkVec2 c,float32 r,NkColor col,int32 segs=0) noexcept;
            void AddEllipseFilled(NkVec2 c,float32 rx,float32 ry, NkColor col,int32 segs=0) noexcept;

            // ── Arcs ──────────────────────────────────────────────────────────────────
            void AddArc(NkVec2 c,float32 r,float32 a0,float32 a1, NkColor col,float32 thickness=1.f,int32 segs=0) noexcept;
            void AddArcFilled(NkVec2 c,float32 r,float32 a0,float32 a1, NkColor col,int32 segs=0) noexcept;

            // ── Bezier ────────────────────────────────────────────────────────────────
            void AddBezierCubic(NkVec2 p0,NkVec2 p1,NkVec2 p2,NkVec2 p3, NkColor col,float32 thickness=1.f,int32 segs=0) noexcept;
            void AddBezierQuadratic(NkVec2 p0,NkVec2 p1,NkVec2 p2, NkColor col,float32 thickness=1.f,int32 segs=0) noexcept;

            // ── Texte ─────────────────────────────────────────────────────────────────
            void AddText(NkVec2 pos,const char* text,NkColor col, float32 size=0.f,uint32 fontId=0) noexcept;
            void AddTextWrapped(NkRect bounds,const char* text,NkColor col, float32 size=0.f,uint32 fontId=0) noexcept;

            // ── Images / Textures ─────────────────────────────────────────────────────
            void AddImage(uint32 texId,NkRect dst, NkVec2 uvMin={0,0},NkVec2 uvMax={1,1}, NkColor tint=NkColor::White()) noexcept;
            void AddImageRounded(uint32 texId,NkRect dst, float32 radius,NkColor tint=NkColor::White()) noexcept;

            // ── Path builder (SVG-like) ───────────────────────────────────────────────
            void PathClear() noexcept;
            void PathMoveTo(NkVec2 p) noexcept;
            void PathLineTo(NkVec2 p) noexcept;
            void PathArcTo(NkVec2 c,float32 r,float32 a0,float32 a1,int32 segs=0) noexcept;
            void PathBezierCubicTo(NkVec2 p1,NkVec2 p2,NkVec2 p3,int32 segs=0) noexcept;
            void PathBezierQuadTo(NkVec2 p1,NkVec2 p2,int32 segs=0) noexcept;
            void PathRect(NkRect r,float32 rx=0) noexcept;
            void PathFill(NkColor col) noexcept;
            void PathStroke(NkColor col,float32 thickness=1.f,bool closed=true) noexcept;

            // ── Widget helpers composites (utilisés par NkUIWidgets) ─────────────────
            void AddShadow(NkRect r,float32 radius,NkColor col,NkVec2 offset={2,2}) noexcept;
            void AddCheckMark(NkVec2 pos,float32 sz,NkColor col) noexcept;
            void AddArrow(NkVec2 center,float32 sz,int32 dir,NkColor col) noexcept;
            void AddResizeGrip(NkVec2 pos,float32 sz,NkColor col) noexcept;
            void AddScrollbarThumb(NkRect r,NkColor col,bool vertical) noexcept;
            void AddSpinner(NkVec2 center,float32 r,float32 angle,NkColor col) noexcept;
            void AddColorWheel(NkRect r,float32 hue,float32 sat,float32 val) noexcept;

        private:
            // Path buffer
            NkVec2  pathPts[4096];
            int32   pathCount=0;

            // Helpers internes
            void ReserveVtx(uint32 n) noexcept;
            void ReserveIdx(uint32 n) noexcept;
            void PushCmd(NkUIDrawCmdType type) noexcept;
            void EnsureCmd() noexcept;
            uint32 VtxWrite(NkVec2 pos,NkVec2 uv,NkColor col) noexcept;
            void   IdxWrite(uint32 a,uint32 b,uint32 c) noexcept;

            // Culling helpers
            bool IsOccluded(NkRect r) const noexcept;
            void TrackOpaque(NkRect r, NkColor col) noexcept;

            static int32 CalcCircleSegs(float32 r) noexcept {
                const int32 s=static_cast<int32>(8.f*r/4.f)+8;
                return s<8?8:s>128?128:s;
            }
        };
    }
} // namespace nkentseu
