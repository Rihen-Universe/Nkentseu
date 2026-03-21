#pragma once
/**
 * @File    NkSVGDOM.h
 * @Brief   SVG DOM complet — parsing, héritage CSS, defs/use/symbol/g, gradients.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkSVGDOM construit un arbre de NkSVGElement depuis un NkXMLDocument.
 *  Chaque NkSVGElement contient :
 *    - Le style calculé (héritage CSS cascade complet)
 *    - La transformation CTM empilée
 *    - Les références résolues (<use> → élément cible dans <defs>)
 *    - Le gradient ou pattern de remplissage/contour
 *
 *  Fonctionnalités :
 *    <g>         — groupe avec CTM, héritage de style
 *    <defs>      — registre d'éléments réutilisables (ne sont pas rendus)
 *    <use>       — instanciation d'un élément de <defs> avec transformation
 *    <symbol>    — boîte réutilisable avec viewBox propre
 *    <linearGradient> / <radialGradient> — gradients avec stops, href, gradientUnits
 *    <clipPath>  — masque de découpe
 *    <pattern>   — motif répété (décrit mais non rendu par le rasterizer de base)
 *    CSS inline style= et attributs de présentation (fill, stroke, ...)
 *    Héritage complet des propriétés SVG héritables
 *    Valeurs CSS : currentColor, inherit, url(#id), none, transparent
 *    Unités : px, pt, mm, cm, in, em, ex, % (relatives au viewport)
 *    viewBox + preserveAspectRatio (xMidYMid meet/slice)
 */

#include "NKImage/Core/NkXMLParser.h"
#include "NKImage/Core/NkImageExport.h"
#include <cstring>
#include <cmath>

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────────
    //  Couleur SVG
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkSVGPaint {
        enum class Type : uint8 {
            None         = 0, // none
            Color        = 1, // couleur RGBA
            URL          = 2, // url(#id) → gradient/pattern
            CurrentColor = 3, // currentColor
            Inherit      = 4, // inherit
        };
        Type    type    = Type::Color;
        uint8   r=0,g=0,b=0,a=255;
        char    url[64] = {}; // ID du gradient/pattern référencé

        bool    IsNone()    const noexcept { return type==Type::None; }
        bool    IsColor()   const noexcept { return type==Type::Color; }
        bool    IsURL()     const noexcept { return type==Type::URL; }
        bool    IsInherit() const noexcept { return type==Type::Inherit; }

        static NkSVGPaint None() noexcept { NkSVGPaint p; p.type=Type::None; return p; }
        static NkSVGPaint Black() noexcept { return {Type::Color,0,0,0,255,{}}; }
        static NkSVGPaint White() noexcept { return {Type::Color,255,255,255,255,{}}; }
        static NkSVGPaint Transparent() noexcept { NkSVGPaint p; p.type=Type::Color; p.a=0; return p; }
        static NkSVGPaint Inherit() noexcept { NkSVGPaint p; p.type=Type::Inherit; return p; }

        static NkSVGPaint Parse(const char* str,
                                const NkSVGPaint& currentColor = NkSVGPaint::Black()) noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  Matrice de transformation 2D (3×3 homogène)
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkSVGMatrix {
        float a=1,b=0,c=0,d=1,e=0,f=0; // [a c e; b d f; 0 0 1]

        static NkSVGMatrix Identity() noexcept { return {}; }
        static NkSVGMatrix Translate(float tx,float ty) noexcept { return {1,0,0,1,tx,ty}; }
        static NkSVGMatrix Scale(float sx,float sy=0) noexcept {
            return {sx,0,0,(sy==0?sx:sy),0,0};
        }
        static NkSVGMatrix Rotate(float deg,float cx=0,float cy=0) noexcept;
        static NkSVGMatrix SkewX(float deg) noexcept;
        static NkSVGMatrix SkewY(float deg) noexcept;
        static NkSVGMatrix Parse(const char* str) noexcept;

        NkSVGMatrix operator*(const NkSVGMatrix& o) const noexcept {
            return {a*o.a+c*o.b, b*o.a+d*o.b,
                    a*o.c+c*o.d, b*o.c+d*o.d,
                    a*o.e+c*o.f+e, b*o.e+d*o.f+f};
        }
        NkSVGMatrix Inverse() const noexcept;

        void Apply(float& x, float& y) const noexcept {
            const float nx=a*x+c*y+e, ny=b*x+d*y+f; x=nx; y=ny;
        }
        bool IsIdentity() const noexcept {
            return a==1&&b==0&&c==0&&d==1&&e==0&&f==0;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  Style calculé SVG (après héritage en cascade)
    // ─────────────────────────────────────────────────────────────────────────────

    enum class NkSVGFillRule   : uint8 { NonZero=0, EvenOdd=1 };
    enum class NkSVGLineCap    : uint8 { Butt=0, Round=1, Square=2 };
    enum class NkSVGLineJoin   : uint8 { Miter=0, Round=1, Bevel=2 };
    enum class NkSVGDisplay    : uint8 { Inline=0, None=1 };
    enum class NkSVGVisibility : uint8 { Visible=0, Hidden=1, Collapse=2 };
    enum class NkSVGOverflow   : uint8 { Visible=0, Hidden=1, Scroll=2, Auto=3 };

    struct NKENTSEU_IMAGE_API NkSVGStyle {
            NkSVGPaint  fill         = NkSVGPaint::Black();
            NkSVGPaint  stroke       = NkSVGPaint::None();
            float       strokeWidth  = 1.f;
            float       strokeMiterLimit = 4.f;
            NkSVGFillRule fillRule   = NkSVGFillRule::NonZero;
            NkSVGLineCap  lineCap    = NkSVGLineCap::Butt;
            NkSVGLineJoin lineJoin   = NkSVGLineJoin::Miter;
            float       opacity      = 1.f;
            float       fillOpacity  = 1.f;
            float       strokeOpacity= 1.f;
            float       fontSize     = 16.f;
            NkSVGDisplay    display  = NkSVGDisplay::Inline;
            NkSVGVisibility visibility= NkSVGVisibility::Visible;
            // Dasharray simplifié (jusqu'à 8 valeurs)
            float       dashArray[8] = {};
            int32       dashCount    = 0;
            float       dashOffset   = 0.f;
            // Font
            char        fontFamily[64] = {};
            float       fontWeight   = 400.f;
            // Clip path ID
            char        clipPathId[64] = {};

            bool IsVisible() const noexcept {
                return display==NkSVGDisplay::Inline&&visibility==NkSVGVisibility::Visible;
            }

            /// Calcule le style hérité depuis le parent + les attributs de présentation du nœud
            static NkSVGStyle Compute(const NkSVGStyle& parent,
                                    NkXMLNode* node,
                                    float viewportW, float viewportH) noexcept;

        private:
            static void ParseInlineStyle(const char* css, NkSVGStyle& out,
                                        float vpW, float vpH) noexcept;
            static void ApplyProperty(const char* prop, const char* value,
                                    NkSVGStyle& out, float vpW, float vpH) noexcept;
            static float ParseLength(const char* str, float vpW, float vpH,
                                    float fontSize) noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGGradientStop
    // ─────────────────────────────────────────────────────────────────────────────

    struct NkSVGGradientStop {
        float     offset;  // [0,1]
        NkSVGPaint color;
        float     opacity;
        NkSVGGradientStop* next = nullptr;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGGradient — gradient linéaire ou radial
    // ─────────────────────────────────────────────────────────────────────────────

    enum class NkSVGGradientType    : uint8 { Linear, Radial };
    enum class NkSVGGradientUnits   : uint8 { UserSpace, ObjectBoundingBox };
    enum class NkSVGSpreadMethod    : uint8 { Pad, Reflect, Repeat };

    struct NKENTSEU_IMAGE_API NkSVGGradient {
        NkSVGGradientType   type       = NkSVGGradientType::Linear;
        NkSVGGradientUnits  units      = NkSVGGradientUnits::ObjectBoundingBox;
        NkSVGSpreadMethod   spread     = NkSVGSpreadMethod::Pad;
        NkSVGMatrix         transform  = NkSVGMatrix::Identity();
        // Linear
        float x1=0,y1=0,x2=1,y2=0;
        // Radial
        float cx=0.5f,cy=0.5f,r=0.5f,fx=0.5f,fy=0.5f;
        // Stops
        NkSVGGradientStop* firstStop = nullptr;
        int32 numStops = 0;
        // Héritage href
        char href[64] = {};
        char id  [64] = {};

        /// Échantillonne la couleur au paramètre t ∈ [0,1]
        void Sample(float t, uint8& R,uint8& G,uint8& B,uint8& A) const noexcept;
        /// Calcule t pour un point (x,y) dans l'espace de l'objet
        float LinearT(float x,float y) const noexcept;
        float RadialT(float x,float y) const noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGClipPath
    // ─────────────────────────────────────────────────────────────────────────────

    struct NkSVGClipPath {
        char id[64] = {};
        NkXMLNode* node = nullptr; // nœud XML source (pour rendu différé)
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGViewBox
    // ─────────────────────────────────────────────────────────────────────────────

    struct NkSVGViewBox {
        float x=0,y=0,w=0,h=0;
        bool  valid=false;
        // preserveAspectRatio
        enum class Align : uint8 {
            None, XMinYMin, XMidYMin, XMaxYMin,
            XMinYMid, XMidYMid, XMaxYMid,
            XMinYMax, XMidYMax, XMaxYMax
        };
        Align align = Align::XMidYMid;
        bool  meet  = true; // true=meet, false=slice

        static NkSVGViewBox Parse(const char* str) noexcept;
        static Align ParseAlign(const char* str) noexcept;

        /// Calcule la matrice viewport → userSpace
        NkSVGMatrix ToMatrix(float vpW, float vpH) const noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGDefs — registre des éléments réutilisables
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkSVGDefs {
            // Tables de hachage simples (listes chaînées)
            static constexpr int32 HASH_SIZE = 64;

            struct GradEntry { char id[64]; NkSVGGradient* grad; GradEntry* next; };
            struct ClipEntry { char id[64]; NkSVGClipPath* clip; ClipEntry* next; };
            struct ElemEntry { char id[64]; NkXMLNode*     node; ElemEntry* next; };

            GradEntry* grads[HASH_SIZE] = {};
            ClipEntry* clips[HASH_SIZE] = {};
            ElemEntry* elems[HASH_SIZE] = {};

            NkXMLArena* arena = nullptr;

            void Init(NkXMLArena* a) noexcept { arena=a; }

            void RegisterGradient(const char* id, NkSVGGradient* g) noexcept;
            void RegisterClipPath(const char* id, NkSVGClipPath* c) noexcept;
            void RegisterElement (const char* id, NkXMLNode* node)   noexcept;

            NkSVGGradient* FindGradient(const char* id) const noexcept;
            NkSVGClipPath* FindClipPath(const char* id) const noexcept;
            NkXMLNode*     FindElement (const char* id) const noexcept;

            // Résout les références href (gradients qui héritent d'autres gradients)
            void ResolveHrefs() noexcept;

        private:
            static uint32 Hash(const char* id) noexcept {
                uint32 h=5381;
                while(*id) h=((h<<5)+h)^static_cast<uint8>(*id++);
                return h % HASH_SIZE;
            }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGElement — élément SVG avec style calculé et CTM
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkSVGElement {
        NkXMLNode*    xmlNode   = nullptr;
        NkSVGStyle    style;            // style calculé (après cascade)
        NkSVGMatrix   localTransform;   // transformation de cet élément
        NkSVGMatrix   ctm;              // current transform matrix (cumulé depuis la racine)
        NkSVGViewBox  viewBox;          // si cet élément a un viewBox
        bool          inDefs    = false;// true si cet élément est dans <defs>
        NkSVGElement* parent    = nullptr;
        NkSVGElement* firstChild= nullptr;
        NkSVGElement* lastChild = nullptr;
        NkSVGElement* nextSibling=nullptr;
        NkSVGElement* prevSibling=nullptr;

        // Attributs géométriques pré-parsés (valeur finale en unités utilisateur)
        float x=0,y=0,width=0,height=0;   // rect, svg, image, use
        float rx=0,ry=0;                   // rect
        float cx=0,cy=0,r=0;              // circle
        float rx2=0,ry2=0;                 // ellipse
        float x1=0,y1=0,x2=0,y2=0;       // line
        char  d[4096]={};                  // path data (tronqué si > 4095)
        char  points[2048]={};             // polyline/polygon points

        NKIMG_NODISCARD const char* Tag() const noexcept {
            return xmlNode?xmlNode->localName:"";
        }
        NKIMG_NODISCARD bool Is(const char* tag) const noexcept {
            return xmlNode&&xmlNode->localName&&::strcmp(xmlNode->localName,tag)==0;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGDOM — arbre DOM SVG complet
    // ─────────────────────────────────────────────────────────────────────────────

    struct NKENTSEU_IMAGE_API NkSVGDOM {
        NkXMLArena    arena;
        NkXMLDocument xmlDoc;
        NkSVGDefs     defs;
        NkSVGElement* root     = nullptr; // racine <svg>
        float         viewportW= 0;
        float         viewportH= 0;
        NkSVGViewBox  viewBox;
        float         outputW  = 0;
        float         outputH  = 0;
        bool          isValid  = false;

        void Init(usize arenaBytes=8*1024*1024) noexcept;
        void Destroy() noexcept;

        // Trouve un gradient/clipPath par URL "url(#id)" ou "#id" ou "id"
        NkSVGGradient* FindGradient(const char* ref) const noexcept;
        NkSVGClipPath* FindClipPath(const char* ref) const noexcept;
        NkXMLNode*     FindDefElement(const char* id) const noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGDOMBuilder — construit le DOM SVG depuis un NkXMLDocument
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_IMAGE_API NkSVGDOMBuilder {
        public:
            /**
             * @Brief Construit un NkSVGDOM depuis un XML déjà parsé.
             * @param xmlDoc  Document XML source.
             * @param dom     DOM SVG de sortie (doit être initialisé).
             * @param outW    Largeur de rendu souhaitée (0 = depuis SVG).
             * @param outH    Hauteur de rendu souhaitée (0 = depuis SVG).
             */
            static bool Build(NkXMLDocument& xmlDoc, NkSVGDOM& dom,
                            float outW=0, float outH=0) noexcept;

            /// Parse + Build en une seule étape depuis des données XML.
            static bool BuildFromXML(const uint8* xml, usize size, NkSVGDOM& dom,
                                    float outW=0, float outH=0) noexcept;
            static bool BuildFromFile(const char* path, NkSVGDOM& dom,
                                    float outW=0, float outH=0) noexcept;

        private:
            struct BuildCtx {
                NkSVGDefs*   defs;
                NkXMLArena*  arena;
                NkSVGStyle   currentStyle;
                NkSVGMatrix  currentCTM;
                float        vpW, vpH;
                bool         inDefs;
            };

            static NkSVGElement* BuildElement(NkXMLNode* node, NkSVGElement* parent,
                                            BuildCtx& ctx) noexcept;
            static void BuildDefs   (NkXMLNode* defsNode, BuildCtx& ctx) noexcept;
            static void BuildGradient(NkXMLNode* node, BuildCtx& ctx) noexcept;
            static void BuildClipPath(NkXMLNode* node, BuildCtx& ctx) noexcept;
            static void ParseGeometry(NkXMLNode* node, NkSVGElement* elem,
                                    float vpW, float vpH) noexcept;

            static NkSVGElement* AllocElem(NkXMLArena& arena) noexcept {
                return arena.Alloc<NkSVGElement>();
            }
            static void AppendChild(NkSVGElement* parent, NkSVGElement* child) noexcept;
            static void ParseSVGRoot(NkXMLNode* svg, NkSVGDOM& dom) noexcept;
    };

} // namespace nkentseu