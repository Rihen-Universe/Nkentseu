#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiTypes.h
// @Brief   Types fondamentaux NKGui : Id, alias géométriques (NKMath), enums.
// @License Proprietary - Free to use and modify
//
// Aucun identifiant dérivé d'ImGui (cf. ARCHITECTURE.md §2). Les types
// géométriques réutilisent NKMath — idiome Nkentseu, pas un lien ImGui.
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKMath/NkVec.h"
#include "NKMath/NkColor.h"
#include "NKMath/NkRectangle.h"

namespace nkentseu {
    namespace nkgui {

        // ── Alias géométriques (NKMath) ───────────────────────────────────────
        using NkVec2  = math::NkVec2;
        using NkVec4  = math::NkVec4;
        using NkColor = math::NkColor;
        using NkRect  = math::NkFloatRect;

        // ── Identifiant de widget (hash FNV-1a 32 bits) ───────────────────────
        using NkGuiId = uint32;
        static constexpr NkGuiId NKGUI_ID_NONE = 0;

        NKENTSEU_NKGUI_API_INLINE NkGuiId NkGuiHashStr(const char* s, NkGuiId seed = 2166136261u) noexcept {
            NkGuiId h = seed;
            while (s && *s) { h ^= static_cast<uint8>(*s++); h *= 16777619u; }
            return h ? h : 1u;
        }
        NKENTSEU_NKGUI_API_INLINE NkGuiId NkGuiHashPtr(const void* p, NkGuiId seed = 2166136261u) noexcept {
            NkGuiId h = seed;
            usize v = reinterpret_cast<usize>(p);
            for (int i = 0; i < 8; ++i) { h ^= static_cast<uint8>(v >> (i * 8)); h *= 16777619u; }
            return h ? h : 1u;
        }

        // ── Machine à états d'interaction (⭐ le cœur du fix UX) ───────────────
        // À chaque frame, UN SEUL mode actif et explicite. Zones de préhension
        // disjointes et priorisées (resize > move > contenu), chacune avec son
        // curseur et son affordance. Voir ARCHITECTURE.md §4.
        enum class NkGuiInteract : uint8 {
            None = 0,
            HoverWidget,   ///< survol d'un widget
            EditWidget,    ///< édition active (slider/input…)
            MoveWindow,    ///< déplacement d'une fenêtre (barre de titre / onglet)
            ResizeWindow,  ///< redimensionnement (bord/coin — cf. edge)
            DragSplitter,  ///< glissement d'un séparateur de dock
            DragTab,       ///< glissement d'un onglet (réordre / détache)
            DockTarget     ///< visée d'une cible de dock (boussole)
        };

        // Bord/coin concerné lors d'un ResizeWindow (bitmask).
        enum class NkGuiEdge : uint8 {
            None   = 0,
            Left   = 1 << 0,
            Right  = 1 << 1,
            Top    = 1 << 2,
            Bottom = 1 << 3
        };
        NKENTSEU_NKGUI_API_INLINE NkGuiEdge operator|(NkGuiEdge a, NkGuiEdge b) noexcept {
            return static_cast<NkGuiEdge>(static_cast<uint8>(a) | static_cast<uint8>(b));
        }
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasEdge(NkGuiEdge set, NkGuiEdge e) noexcept {
            return (static_cast<uint8>(set) & static_cast<uint8>(e)) != 0;
        }

        // ── Touches d'édition suivies par NKGui (pour la répétition au maintien) ─
        enum class NkGuiKey : uint8 {
            Left = 0, Right, Up, Down, Home, End, Backspace, Delete, Enter, Escape, Count
        };

        // ── Flags de Table ────────────────────────────────────────────────────
        enum class NkGuiTableFlags : uint32 {
            None           = 0,
            Borders        = 1 << 0,   ///< traits entre colonnes/lignes + contour
            RowBg          = 1 << 1,   ///< fond alterné des lignes (zébrures)
            Resizable      = 1 << 2,   ///< séparateurs INTERNES (redistribue, largeur globale stable)
            Header         = 1 << 3,   ///< (info) une ligne d'en-tête est dessinée
            ResizableOuter = 1 << 4,   ///< poignée au BORD DROIT → change la largeur GLOBALE (opt-in)
            Sortable       = 1 << 5    ///< en-têtes cliquables (cycle asc/desc) + flèche de tri (opt-in)
        };
        NKENTSEU_NKGUI_API_INLINE NkGuiTableFlags operator|(NkGuiTableFlags a, NkGuiTableFlags b) noexcept {
            return static_cast<NkGuiTableFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasTableFlag(NkGuiTableFlags s, NkGuiTableFlags f) noexcept {
            return (static_cast<uint32>(s) & static_cast<uint32>(f)) != 0;
        }

        // ── Flags d'édition de couleur (ColorEdit/ColorPicker/ColorButton) ────
        enum class NkGuiColorFlags : uint32 {
            None      = 0,
            NoAlpha   = 1 << 0,   ///< ignore le canal alpha (3 composantes au lieu de 4)
            Wheel     = 1 << 1,   ///< roue de teinte + triangle SV
            NoInputs  = 1 << 2,   ///< masque les champs numériques HSV/RGB sous le picker
            Disc      = 1 << 3,   ///< disque plein (teinte=angle, saturation=rayon) + barre valeur
            Hexagon   = 1 << 4,   ///< hexagone dégradé continu (6 teintes + centre blanc) + barre valeur
            Honeycomb = 1 << 5    ///< palette en nid d'abeille (pastilles hexagonales discrètes)
        };
        NKENTSEU_NKGUI_API_INLINE NkGuiColorFlags operator|(NkGuiColorFlags a, NkGuiColorFlags b) noexcept {
            return static_cast<NkGuiColorFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasColorFlag(NkGuiColorFlags s, NkGuiColorFlags f) noexcept {
            return (static_cast<uint32>(s) & static_cast<uint32>(f)) != 0;
        }

        // ── Curseur souhaité par un widget (l'app le mappe vers NkWindow::SetCursor,
        //    donc render-agnostique et OPTIONNEL : l'app applique si elle veut). ───
        enum class NkGuiCursor : uint8 {
            Arrow = 0,   ///< flèche standard
            Text,        ///< I-beam (champ texte)
            Hand,        ///< main (cliquable)
            ResizeEW,    ///< redimensionnement horizontal ↔ (DragFloat/splitter)
            ResizeNS     ///< redimensionnement vertical ↕
        };

        // ── Direction de glissement d'un DragFloat/DragInt (au choix de l'app) ────
        enum class NkGuiDragDir : uint8 {
            Horizontal = 0,  ///< glisser ←→ (défaut)
            Vertical,        ///< glisser ↑↓ (haut = augmente)
            Both             ///< les deux axes combinés
        };

        // ── Modificateur clavier (combinaison configurable, ex. scroll horizontal) ─
        enum class NkGuiKeyMod : uint8 {
            None  = 0,   ///< aucune touche requise (toujours actif)
            Shift = 1,
            Ctrl  = 2,
            Alt   = 3
        };

        // ── État d'une case à cocher (tri-état) ───────────────────────────────
        enum class NkGuiCheck : uint8 {
            Off   = 0,   ///< décoché
            On    = 1,   ///< coché
            Mixed = 2    ///< indéterminé (ex. « tout sélectionner » partiel)
        };

        // ── Flags de liste sélectionnable ─────────────────────────────────────
        enum class NkGuiSelectFlags : uint32 {
            None        = 0,
            MultiSelect = 1 << 0   ///< Ctrl+clic = toggle, Shift+clic = plage
        };
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasSelectFlag(NkGuiSelectFlags s, NkGuiSelectFlags f) noexcept {
            return (static_cast<uint32>(s) & static_cast<uint32>(f)) != 0;
        }

        // ── Flags de champ de saisie (InputText) ──────────────────────────────
        enum class NkGuiInputFlags : uint32 {
            None         = 0,
            Password     = 1 << 0,   ///< affiche des points (masque la saisie)
            CharsDecimal = 1 << 1,   ///< n'accepte que 0-9 . + -
            CharsHex     = 1 << 2,   ///< n'accepte que 0-9 a-f A-F
            Uppercase    = 1 << 3,   ///< force les minuscules ASCII en majuscules
            NoBlank      = 1 << 4,   ///< interdit les espaces
            ReadOnly     = 1 << 5    ///< affiché mais non éditable (caret de lecture ok)
        };
        NKENTSEU_NKGUI_API_INLINE NkGuiInputFlags operator|(NkGuiInputFlags a, NkGuiInputFlags b) noexcept {
            return static_cast<NkGuiInputFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasInputFlag(NkGuiInputFlags s, NkGuiInputFlags f) noexcept {
            return (static_cast<uint32>(s) & static_cast<uint32>(f)) != 0;
        }

        // ── Flags de bouton ───────────────────────────────────────────────────
        enum class NkGuiButtonFlags : uint32 {
            None   = 0,
            Repeat = 1 << 0   ///< rafale : déclenche en continu tant que maintenu
        };
        NKENTSEU_NKGUI_API_INLINE NkGuiButtonFlags operator|(NkGuiButtonFlags a, NkGuiButtonFlags b) noexcept {
            return static_cast<NkGuiButtonFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasFlag(NkGuiButtonFlags set, NkGuiButtonFlags f) noexcept {
            return (static_cast<uint32>(set) & static_cast<uint32>(f)) != 0;
        }

        // ── Fenêtres flottantes (Begin/End) — multi-fenêtres dans la fenêtre OS ─
        enum class NkGuiWindowFlags : uint32 {
            None        = 0,
            NoResize    = 1 << 0,   ///< pas de redimensionnement
            NoMove      = 1 << 1,   ///< pas de déplacement (barre de titre figée)
            NoCollapse  = 1 << 2,   ///< pas de bouton repli
            NoTitleBar  = 1 << 3,   ///< pas de barre de titre
            NoClose     = 1 << 4    ///< pas de bouton fermer (même si `open` fourni)
        };
        NKENTSEU_NKGUI_API_INLINE NkGuiWindowFlags operator|(NkGuiWindowFlags a, NkGuiWindowFlags b) noexcept {
            return static_cast<NkGuiWindowFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }
        NKENTSEU_NKGUI_API_INLINE bool NkGuiHasWinFlag(NkGuiWindowFlags s, NkGuiWindowFlags f) noexcept {
            return (static_cast<uint32>(s) & static_cast<uint32>(f)) != 0;
        }
        // État PERSISTANT d'une fenêtre (position/taille/repli/z-order), gardé par id.
        struct NkGuiWindowMeta {
            NkGuiId id       = NKGUI_ID_NONE;
            NkRect  rect     = { 60.f, 60.f, 320.f, 220.f };
            bool    collapsed = false;
            bool    init      = false;   ///< rect déjà initialisé (1re apparition)
            int32   zOrder    = 0;
            // ── Docking ──
            int32   dockNode      = -1;   ///< feuille de dock où la fenêtre est ancrée (-1 = flottante)
            NkRect  dockRect      = { 0.f, 0.f, 0.f, 0.f };  ///< zone de contenu quand dockée (posée par DockSpace)
            bool    dockActiveTab = false;///< est l'onglet ACTIF de sa feuille ?
            NkRect  floatRect     = { 60.f, 60.f, 320.f, 220.f };  ///< taille mémorisée pour le désancrage
            char    title[48]     = {};   ///< titre copié (les onglets de dock en ont besoin avant Begin)
            int32   hostRoot      = -1;   ///< #3 : si >=0, cette fenêtre est un HÔTE de dock flottant (rend l'arbre)
            NkRect  hostRect      = { 80.f, 80.f, 360.f, 240.f };  ///< rect flottant de l'hôte
            NkGuiId dockHost      = NKGUI_ID_NONE;  ///< #3 : hôte flottant où cette fenêtre est ancrée (NONE = central/aucun)
            bool    hostRendered  = false;          ///< #3 : chrome+arbre de l'hôte déjà dessinés cette frame ?
            int32   frameDL       = -1;             ///< #3 : slot winDL de l'hôte cette frame (-1 = pas encore alloué)
            int32   dockDL        = -1;             ///< draw-list où DockRenderNode a peint le FOND de cet onglet (-1 = central/dl) — le contenu DOIT s'y dessiner
        };

        // Nœud de l'arbre de DOCK : SPLIT (2 enfants, axe + ratio + splitter) ou
        // FEUILLE (fenêtres ancrées en onglets). Rects recalculés chaque frame.
        struct NkGuiDockNode {
            uint8   kind     = 0;        ///< 0 = vide, 1 = split, 2 = feuille
            bool    vertical = false;    ///< split : true = gauche|droite, false = haut|bas
            float32 ratio    = 0.5f;     ///< fraction du 1er enfant
            int32   child0   = -1;
            int32   child1   = -1;
            int32   parent   = -1;
            NkGuiId windows[8] = {};     ///< feuille : fenêtres ancrées (onglets)
            int32   winCount   = 0;
            int32   activeTab  = 0;
            NkRect  rect       = { 0.f, 0.f, 0.f, 0.f };
        };

        // ── Hook de style : override de DESSIN par widget ─────────────────────
        // L'app enregistre `ctx.styleFn` ; pour chaque élément visuel, NKGui l'appelle
        // AVANT le rendu par défaut. Si le callback retourne true, il a dessiné lui-même
        // (le défaut est sauté) → re-skin TOTAL sans toucher la logique du widget.
        enum class NkGuiStyleKind : uint8 {
            Button = 0,   ///< cadre + libellé d'un bouton
            FrameBg,      ///< fond d'un champ/cadre (input, slider track…)
            CheckMark,    ///< case à cocher (boîte + coche)
            Header,       ///< bandeau (TreeNode, en-tête de panneau)
            Selectable,   ///< ligne sélectionnable (liste/arbre)
            DockTarget,   ///< pastille/ligne de cible d'ancrage (docking) — `value` = direction
            Count
        };
        struct NkGuiStyleItem {
            NkGuiStyleKind kind     = NkGuiStyleKind::Button;
            NkRect         rect     = { 0.f, 0.f, 0.f, 0.f };
            NkGuiId        id       = NKGUI_ID_NONE;
            const char*    label    = nullptr;
            bool           hovered  = false;
            bool           active   = false;   ///< pressé / en cours d'édition / cible visée
            bool           selected = false;
            bool           disabled = false;
            int32          value    = 0;        ///< donnée par type (DockTarget : 0=onglet,1=G,2=D,3=H,4=B,5=bord)
        };

        // ── Helpers géométriques de base ──────────────────────────────────────
        NKENTSEU_NKGUI_API_INLINE bool NkGuiRectContains(const NkRect& r, const NkVec2& p) noexcept {
            return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
        }

    } // namespace nkgui
} // namespace nkentseu
