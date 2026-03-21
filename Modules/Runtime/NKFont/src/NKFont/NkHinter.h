#pragma once
/**
 * @File    NkHinter.h
 * @Brief   VM TrueType hinting — interpréteur d'instructions de grille.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  Le hinting TrueType est un programme bytecode exécuté à chaque rendu
 *  pour ajuster les coordonnées des points sur la grille pixel afin
 *  d'améliorer la lisibilité aux petites tailles.
 *
 *  Architecture de la VM :
 *    - Stack       : int32[], taille max configurable (maxp.maxStackElements)
 *    - Storage     : int32[], zones persistantes entre glyphes
 *    - CVT         : int32[], Control Value Table (déltas de design)
 *    - Graphics State (GS) : paramètres de la session en cours
 *    - Zones       : Zone 0 (twilight) et Zone 1 (points du glyphe)
 *
 *  Exécution en 3 passes :
 *    1. fpgm (Font Program) : défini les fonctions réutilisables
 *    2. prep (CVT Program)  : initialise le CVT à chaque taille
 *    3. Instructions du glyphe : ajuste les points
 *
 *  Opcodes implémentés (sous-ensemble production) :
 *    SVTCA, SPVTCA, SFVTCA, SPVTL, SFVTL, SPVFS, SFVFS
 *    SRP0, SRP1, SRP2, SZP0, SZP1, SZP2, SZPS, SLOOP
 *    RTG, RTHG, RTDG, RDTG, RUTG, ROFF
 *    MIAP, MSIRP, MDAP, ALIGNRP, ALIGNPTS, IP, IUP
 *    SHPIX, SHP, SHC, SHZ, SHPIX
 *    DELTAP1-3, DELTAC1-3
 *    ADD, SUB, MUL, DIV, ABS, NEG, MAX, MIN, ROUND, NROUND
 *    AND, OR, NOT, GT, GTEQ, LT, LTEQ, EQ, NEQ
 *    IF, ELSE, EIF, JMPR, JROF, JROT
 *    FDEF, ENDF, CALL, LOOPCALL
 *    RCVT, WCVT, WCVTF, MPPEM, MPS, GETINFO
 *    PUSHB, PUSHW, NPUSHB, NPUSHW
 *    RS, WS, DUP, POP, CLEAR, SWAP, DEPTH, CINDEX, MINDEX, ROLL
 *    FLIPPT, FLIPRGON, FLIPRGOFF, SCANCTRL, SCANTYPE, INSTCTRL
 *    GPV, GFV, SCVTCI, SSWCI, SSW, SMD, SCFS, GC, SCFS
 *    MD, MDRP, MIRP
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkOutline.h"
#include "NKFont/NkTTFParser.h"
#include "NKFont/NkMemArena.h"
#include "NKFont/NkFixed26_6.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =========================================================================
    //  Graphics State (GS)
    // =========================================================================

    struct NKENTSEU_FONT_API NkGraphicsState {
        // Vecteurs de projection et de liberté (F2Dot14 → F16Dot16)
        F16Dot16 pvX, pvY;   ///< Projection vector
        F16Dot16 fvX, fvY;   ///< Freedom vector
        F16Dot16 dvX, dvY;   ///< Dual projection vector

        // Reference points
        uint32   rp[3];      ///< rp0, rp1, rp2

        // Zone pointers
        uint32   zp[3];      ///< zp0, zp1, zp2 (0=twilight, 1=glyph)

        // Contrôle
        int32    loop;       ///< Compteur de répétition (SLOOP)
        F26Dot6  minDist;    ///< Distance minimale
        F26Dot6  controlValueCutIn;  ///< SCVTCI
        F26Dot6  singleWidthCutIn;   ///< SSWCI
        F26Dot6  singleWidthValue;   ///< SSW
        F26Dot6  deltaBase;  ///< DELTABASE
        F26Dot6  deltaShift; ///< DELTASHIFT

        // Round state
        enum class RoundMode : uint8 {
            ToGrid, ToHalfGrid, ToDoubleGrid,
            DownToGrid, UpToGrid, Off, Super, Super45
        } roundMode;

        bool autoFlip;
        bool instructControl; // bit 1 de INSTCTRL

        void Reset() noexcept;

        // Projette un vecteur sur l'axe de projection
        F26Dot6 Project(F26Dot6 x, F26Dot6 y) const noexcept {
            return F26Dot6::FromRaw(
                static_cast<int32>(
                    (static_cast<int64>(pvX.raw) * x.raw
                   + static_cast<int64>(pvY.raw) * y.raw) >> F16Dot16::SHIFT
                )
            );
        }

        // Applique le round state
        F26Dot6 Round(F26Dot6 d) const noexcept;
    };

    // =========================================================================
    //  NkHintZone — zone de points (twilight ou glyph)
    // =========================================================================

    struct NKENTSEU_FONT_API NkHintZone {
        NkPoint26_6* original;    ///< Coordonnées originales (non modifiées)
        NkPoint26_6* current;     ///< Coordonnées courantes (modifiées par hinting)
        bool*        touched;     ///< Point touché par une instruction
        uint32       numPoints;
        bool         isTwilight;

        void Init(NkMemArena& arena, uint32 n, bool twilight) noexcept {
            numPoints  = n;
            isTwilight = twilight;
            original   = arena.Alloc<NkPoint26_6>(n);
            current    = arena.Alloc<NkPoint26_6>(n);
            touched    = arena.Alloc<bool>(n);
        }
    };

    // =========================================================================
    //  NkHintVM — machine virtuelle TrueType
    // =========================================================================

    struct NKENTSEU_FONT_API NkHintVM {

        // ── Stack ─────────────────────────────────────────────────────────────
        int32*  stack;
        int32   stackSize;
        int32   stackTop;   ///< Indice du prochain slot libre (0 = vide)

        // ── Storage ───────────────────────────────────────────────────────────
        int32*  storage;
        int32   storageSize;

        // ── CVT ───────────────────────────────────────────────────────────────
        F26Dot6* cvt;
        int32    cvtSize;

        // ── Zones ─────────────────────────────────────────────────────────────
        NkHintZone zone0;  ///< Twilight zone
        NkHintZone zone1;  ///< Glyph zone

        // ── Fonctions définies ────────────────────────────────────────────────
        struct FuncDef {
            const uint8* data;   ///< Pointeur dans le bytecode
            uint32       length;
        };
        FuncDef* funcs;
        uint32   funcCount;
        uint32   funcCapacity;

        // ── État graphique ────────────────────────────────────────────────────
        NkGraphicsState gs;
        NkGraphicsState gsDefault; ///< Sauvegarde pour reset entre glyphes

        // ── Métriques courantes ────────────────────────────────────────────────
        uint16  ppem;
        F26Dot6 scale;       ///< ppem * 64 / unitsPerEm

        bool    isValid;

        // ── Opérations stack ──────────────────────────────────────────────────
        bool   Push(int32 v) noexcept { if (stackTop >= stackSize) return false; stack[stackTop++] = v; return true; }
        int32  Pop()         noexcept { return stackTop > 0 ? stack[--stackTop] : 0; }
        int32  Peek()  const noexcept { return stackTop > 0 ? stack[stackTop-1] : 0; }
        bool   IsEmpty()const noexcept { return stackTop == 0; }

        // ── Zone access ───────────────────────────────────────────────────────
        NkHintZone& Zone(uint32 z) noexcept { return z == 0 ? zone0 : zone1; }
    };

    // =========================================================================
    //  NkHinter
    // =========================================================================

    class NKENTSEU_FONT_API NkHinter {
    public:

        /**
         * @Brief Initialise la VM depuis une police parsée.
         *        Exécute fpgm (Font Program).
         * @param font   Police TrueType.
         * @param ppem   Taille en pixels par em.
         * @param arena  Arena permanent (VM survit à la session).
         * @param vm     VM initialisée.
         */
        static bool Init(
            const NkTTFFont& font,
            uint16           ppem,
            NkMemArena&      arena,
            NkHintVM&        vm
        ) noexcept;

        /**
         * @Brief Exécute le CVT Program (prep) pour la taille courante.
         *        À appeler après Init() et à chaque changement de ppem.
         */
        static bool RunPrep(
            const NkTTFFont& font,
            NkHintVM&        vm,
            NkMemArena&      arena
        ) noexcept;

        /**
         * @Brief Applique les instructions d'un glyphe sur ses points.
         * @param vm       VM initialisée.
         * @param points   Points du glyphe (modifiés en place).
         * @param tags     Tags des points.
         * @param contourEnds  Fins de contours.
         * @param numPoints    Nombre de points.
         * @param numContours  Nombre de contours.
         * @param instrData    Bytecode des instructions.
         * @param instrLen     Longueur du bytecode.
         * @param arena        Arena scratch.
         */
        static bool HintGlyph(
            NkHintVM&          vm,
            NkPoint26_6*       points,
            const uint8*       tags,
            const uint16*      contourEnds,
            uint16             numPoints,
            uint16             numContours,
            const uint8*       instrData,
            uint16             instrLen,
            NkMemArena&        arena
        ) noexcept;

    private:
        // ── Exécuteur de bytecode ─────────────────────────────────────────────
        struct ExecContext {
            NkHintVM*    vm;
            const uint8* code;
            uint32       codeLen;
            uint32       ip;
            int32        callDepth;
            NkMemArena*  arena;
        };

        static bool Execute(ExecContext& ctx) noexcept;

        // ── Opcodes groupés ───────────────────────────────────────────────────
        static bool Op_Push     (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Stack    (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Arith    (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Compare  (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Jump     (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Storage  (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_CVT      (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_GS       (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Move     (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Align    (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Function (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Delta    (ExecContext& ctx, uint8 opcode) noexcept;
        static bool Op_Misc     (ExecContext& ctx, uint8 opcode) noexcept;

        // ── Utilitaires ───────────────────────────────────────────────────────
        static F26Dot6 GetProjection(const NkHintVM& vm,
                                      const NkHintZone& zone, uint32 ptIdx) noexcept;
        static void    MovePoint    (NkHintVM& vm, NkHintZone& zone,
                                      uint32 ptIdx, F26Dot6 delta) noexcept;
        static F26Dot6 CVTCutIn     (const NkHintVM& vm, F26Dot6 dist,
                                      F26Dot6 cvtDist) noexcept;
        static F26Dot6 ComputeDist  (const NkHintVM& vm,
                                      const NkHintZone& z1, uint32 p1,
                                      const NkHintZone& z2, uint32 p2) noexcept;
    };

} // namespace nkentseu
