// -----------------------------------------------------------------------------
// FICHIER: NkUIWindow.h
// DESCRIPTION: Déclaration des fenêtres flottantes NkUI — titre, redimensionnement,
//              déplacement, défilement, ordre Z.
// -----------------------------------------------------------------------------

#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Window manager data model and window API declarations.
 * Main data: Window state, z-order, move/resize flags and contracts.
 * Change this file when: Window interaction model or API shape changes.
 */

/**
 * @File    NkUIWindow.h
 * @Brief   Fenêtres flottantes NkUI — titre, resize, move, scroll, z-order.
 */

#include "NKUI/NkUILayout.h"
#include "NKUI/NkUIFont.h"

namespace nkentseu
{
    namespace nkui
    {
        // ============================================================
        // Énumération des drapeaux de fenêtre
        // ============================================================

        enum class NkUIWindowFlags : uint32
        {
            NK_NONE                       = 0,
            NK_NO_TITLE_BAR               = 1 << 0,
            NK_NO_RESIZE                  = 1 << 1,
            NK_NO_MOVE                    = 1 << 2,
            NK_NO_SCROLLBAR               = 1 << 3,
            NK_NO_SCROLL_WITH_MOUSE       = 1 << 4,
            NK_NO_COLLAPSE                = 1 << 5,
            NK_NO_CLOSE                   = 1 << 6,
            NK_NO_BACKGROUND              = 1 << 7,
            NK_NO_BRING_TO_FRONT_ON_FOCUS = 1 << 8,
            NK_ALWAYS_ON_TOP              = 1 << 9,
            NK_MODAL                      = 1 << 10,
            NK_AUTO_SIZE                  = 1 << 11,
            NK_CHILD_WINDOW               = 1 << 12,
            NK_NO_TABS                    = 1 << 13,  // Interdit l'ajout de cette fenêtre en onglet
            NK_ALLOW_DOCK_CHILD           = 1 << 14,  // Cette fenêtre peut contenir un sous‑dock
            NK_NO_DOCK                    = 1 << 15,  // Cette fenêtre ne peut pas être ancrée
        };

        // Opérateur OU binaire pour NkUIWindowFlags
        NKUI_INLINE NkUIWindowFlags operator|(NkUIWindowFlags a, NkUIWindowFlags b) noexcept
        {
            return static_cast<NkUIWindowFlags>(static_cast<uint32>(a) | static_cast<uint32>(b));
        }

        // Vérifie si un drapeau est présent dans un ensemble
        NKUI_INLINE bool HasFlag(NkUIWindowFlags flags, NkUIWindowFlags f) noexcept
        {
            return (static_cast<uint32>(flags) & static_cast<uint32>(f)) != 0;
        }

        // ============================================================
        // État d'une fenêtre
        // ============================================================

        struct NKUI_API NkUIWindowState
        {
            char             name[64]      = {};       // Nom de la fenêtre
            NkUIID           id            = 0;        // Identifiant unique
            NkVec2           pos           = {100,100}; // Position (coin supérieur gauche)
            NkVec2           size          = {400,300}; // Taille (largeur, hauteur)
            NkVec2           minSize       = {100,60};  // Taille minimale
            NkVec2           maxSize       = {9999,9999}; // Taille maximale
            NkVec2           contentSize   = {};       // Taille de la zone de contenu
            float32          scrollX       = 0;        // Défilement horizontal
            float32          scrollY       = 0;        // Défilement vertical
            int32            zOrder        = 0;        // Ordre Z (plus grand = plus haut)
            NkUIWindowFlags  flags         = NkUIWindowFlags::NK_NONE;
            bool             isOpen        = true;     // Fenêtre ouverte ?
            bool             isCollapsed   = false;    // Réduite ?
            bool             isDocked      = false;    // Ancrée ?
            NkUIID           dockNodeId    = 0;        // Identifiant du nœud d'ancrage
            int32            childDockRoot = -1;       // Index du nœud racine du dock enfant (-1 = aucun)
        };

        // ============================================================
        // Gestionnaire des fenêtres
        // ============================================================

        struct NKUI_API NkUIWindowManager
        {
            static constexpr int32 MAX_WINDOWS = 64;   // Nombre maximum de fenêtres

            NkUIWindowState  windows[MAX_WINDOWS] = {}; // Tableau des états
            int32            numWindows  = 0;           // Nombre de fenêtres actives
            NkUIID           zOrder[MAX_WINDOWS]  = {}; // Ordre Z (tableau d'ID)
            int32            numZOrder   = 0;           // Nombre d'entrées dans zOrder
            NkUIID           activeId    = NKUI_ID_NONE; // ID de la fenêtre active
            NkUIID           hoveredId   = NKUI_ID_NONE; // ID de la fenêtre survolée
            NkUIID           movingId    = NKUI_ID_NONE; // ID de la fenêtre en déplacement
            NkUIID           resizingId  = NKUI_ID_NONE; // ID de la fenêtre en redimensionnement
            NkVec2           moveOffset  = {};           // Décalage souris par rapport au coin pendant le déplacement
            NkVec2           resizeStartMouse = {};      // Position souris au début du redimensionnement
            NkVec2           resizeStartPos   = {};      // Position fenêtre au début du redimensionnement
            NkVec2           resizeStartSize  = {};      // Taille fenêtre au début du redimensionnement
            uint8            resizeEdge  = 0;             // Bord redimensionné (0=aucun, 1=...)

            void Init() noexcept;                        // Initialise le gestionnaire
            void Destroy() noexcept;                     // Libère les ressources

            // Trouve ou crée une fenêtre
            NkUIWindowState* FindOrCreate(const char* name,
                                          NkVec2 pos,
                                          NkVec2 size,
                                          NkUIWindowFlags flags = NkUIWindowFlags::NK_NONE) noexcept;

            NkUIWindowState* Find(NkUIID id) noexcept;          // Trouve par ID
            NkUIWindowState* Find(const char* name) noexcept;   // Trouve par nom

            void BringToFront(NkUIID id) noexcept;          // Amène la fenêtre au premier plan
            void SortZOrder() noexcept;                     // Trie l'ordre Z (plus grand = devant)

            // Teste quelle fenêtre est sous le curseur
            NkUIWindowState* HitTest(NkVec2 pos, float32 titleBarHeight) noexcept;

            void BeginFrame(NkUIContext& ctx) noexcept;     // Début de frame (réinitialise hovered, etc.)
            void EndFrame(NkUIContext& ctx) noexcept;       // Fin de frame
        };

        // ============================================================
        // API d'une fenêtre (Begin/End)
        // ============================================================

        struct NKUI_API NkUIWindow
        {
            // ── API principale ────────────────────────────────────────────────────────
            // Démarre une fenêtre (retourne true si le contenu doit être dessiné)
            static bool Begin(NkUIContext& ctx,
                              NkUIWindowManager& wm,
                              NkUIDrawList& dl,
                              NkUIFont& font,
                              NkUILayoutStack& ls,
                              const char* name,
                              bool* pOpen = nullptr,
                              NkUIWindowFlags flags = NkUIWindowFlags::NK_NONE) noexcept;

            // Termine la fenêtre courante
            static void End(NkUIContext& ctx,
                            NkUIWindowManager& wm,
                            NkUIDrawList& dl,
                            NkUILayoutStack& ls) noexcept;

            // ── Helpers ───────────────────────────────────────────────────────────────
            static void SetNextWindowPos(NkVec2 pos) noexcept;     // Position pour la prochaine fenêtre
            static void SetNextWindowSize(NkVec2 size) noexcept;   // Taille pour la prochaine fenêtre

            static NkVec2 GetWindowPos(NkUIWindowManager& wm, const char* name) noexcept;
            static NkVec2 GetWindowSize(NkUIWindowManager& wm, const char* name) noexcept;
            static bool IsWindowFocused(NkUIWindowManager& wm, const char* name) noexcept;
            static bool IsWindowHovered(NkUIContext& ctx, NkUIWindowManager& wm, const char* name) noexcept;
            static void SetWindowPos(NkUIWindowManager& wm, const char* name, NkVec2 pos) noexcept;
            static void SetWindowSize(NkUIWindowManager& wm, const char* name, NkVec2 size) noexcept;
            static void SetWindowCollapsed(NkUIWindowManager& wm, const char* name, bool collapsed) noexcept;
            static void CloseWindow(NkUIWindowManager& wm, const char* name) noexcept;

            // ── Rendu ─────────────────────────────────────────────────────────────────
            // Affiche toutes les fenêtres (utilisé si on ne gère pas Begin/End)
            static void RenderAll(NkUIContext& ctx, NkUIWindowManager& wm, NkUIDrawList& dl, NkUIFont& font) noexcept;

        private:
            // État courant (pile) pour gérer les fenêtres imbriquées
            struct Current
            {
                NkUIWindowState*  ws;           // Fenêtre courante
                NkUIWindowManager* wm;          // Gestionnaire associé
                NkVec2             startCursor; // Curseur au début de la fenêtre
                NkVec2             reserved;    // Réservé pour alignement
                NkUIDrawList*      savedDL;     // Sauvegarde du draw list courant
                int32              savedLSDepth;// Profondeur de layout stack sauvegardée
                NkUILayoutStack*   ls;          // Pointeur vers la pile de layout
                NkUIID             savedWindowId; // Ancien ID de fenêtre
            };

            static Current sStack[8];   // Pile d'états
            static int32   sDepth;      // Profondeur de pile
            static NkVec2  sNextPos;    // Position forcée pour la prochaine fenêtre
            static NkVec2  sNextSize;   // Taille forcée pour la prochaine fenêtre
            static bool    sHasNextPos; // true si sNextPos est valide
            static bool    sHasNextSize;// true si sNextSize est valide

            static void DrawShadow(NkUIDrawList& dl, const NkUIWindowState& ws, bool focused) noexcept;
            static void DrawTitleBar(NkUIContext& ctx, NkUIDrawList& dl, NkUIFont& font, NkUIWindowState& ws) noexcept;
            static void HandleResize(NkUIContext& ctx, NkUIDrawList& dl, NkUIWindowState& ws) noexcept;
            static void DrawResizeBorders(NkUIDrawList& dl, const NkUIContext& ctx, const NkUIWindowState& ws) noexcept;
        };

        // Remarque : le pointeur vers le gestionnaire de fenêtres est passé explicitement
        // aux fonctions Begin/End. Il n'est pas stocké dans NkUIContext pour rester
        // non-intrusif.
    }
} // namespace nkentseu