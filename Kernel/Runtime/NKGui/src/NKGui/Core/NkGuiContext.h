#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiContext.h
// @Brief   Contexte NKGui — état par instance (IDs, thème, input, draw list,
//          machine à états d'interaction). Phase 2.
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "NKGui/Core/NkGuiInput.h"
#include "NKGui/Core/NkGuiDrawList.h"

namespace nkentseu {
    namespace nkgui {

        struct NkGuiFont;   // police par défaut (Core/NkGuiFont.h)

        // Version du framework (séparée de la version moteur).
        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiVersion {
            static constexpr int32 Major = 0;
            static constexpr int32 Minor = 1;
            static constexpr int32 Patch = 0;
            static const char* String() noexcept;   // "0.1.0"
        };

        // Thème minimal (Phase 2). Étendu en Phase 8.
        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiTheme {
            NkColor bgPrimary    = {  26,  29,  36, 255 };
            NkColor panel        = {  38,  42,  52, 255 };
            NkColor header       = {  46,  51,  63, 255 };
            NkColor button       = {  58,  64,  80, 255 };
            NkColor buttonHover  = {  78,  92, 120, 255 };
            NkColor buttonActive = {  96, 150, 230, 255 };
            NkColor border       = {  86,  94, 112, 255 };
            NkColor text         = { 230, 232, 240, 255 };
            NkColor textDisabled = { 120, 124, 134, 255 };  ///< texte grisé (désactivé)
            NkColor selection    = {  64, 110, 200, 235 };   ///< fond d'élément sélectionné
            NkColor accent       = {  96, 165, 250, 255 };
            NkColor track        = {  30,  34,  43, 255 };  ///< fond de slider
            // Onglets de dock — distincts du fond de la barre (personnalisables).
            NkColor tabBar       = {  22,  25,  31, 255 };  ///< fond de la BARRE d'onglets
            NkColor tab          = {  40,  45,  56, 255 };  ///< onglet inactif (≠ barre)
            NkColor tabHover     = {  58,  64,  80, 255 };  ///< onglet survolé
            NkColor tabActive    = {  52,  58,  72, 255 };  ///< onglet actif
            float32 rounding     = 5.f;
            float32 framePadX    = 10.f;   ///< padding horizontal interne d'un widget
            float32 framePadY    = 6.f;    ///< padding vertical interne d'un widget
        };

        // État de mise en page (curseur immédiat). Chaque widget « auto » prend son
        // rect via NextItemRect, qui avance le curseur (nouvelle ligne par défaut ;
        // SameLine() replace à droite de l'item précédent).
        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiLayout {
            NkRect  region       = { 0.f, 0.f, 0.f, 0.f };
            NkVec2  cursor       = { 0.f, 0.f };
            float32 lineStartX   = 0.f;
            float32 curLineH     = 0.f;
            NkRect  prevItem     = { 0.f, 0.f, 0.f, 0.f };
            float32 maxX         = 0.f;   ///< extrémité droite du contenu (scroll horizontal)
            float32 maxY         = 0.f;   ///< extrémité basse du contenu (taille des conteneurs)
            float32 padding      = 10.f;
            float32 itemSpacingX = 8.f;
            float32 itemSpacingY = 6.f;
            // ── Flux de mise en page (conteneurs) ──
            int32   flow         = 0;     ///< 0=vertical (défaut) · 1=horizontal (HBox) · 2=grille · 3=pile (Stack)
            int32   gridCols     = 0;     ///< Grid : nombre de colonnes
            int32   gridIdx      = 0;     ///< Grid : colonne courante
            float32 gridColW     = 0.f;   ///< Grid : largeur d'une colonne
            float32 stackW       = 0.f;   ///< Stack : largeur de la boîte
            float32 stackH       = 0.f;   ///< Stack : hauteur de la boîte
            int32   stackAnchor  = 0;     ///< Stack : 0=HG 1=HC 2=HD 3=CG 4=C 5=CD 6=BG 7=BC 8=BD
            // ── Flex (rangée/colonne à poids) : cellules pré-calculées, distribution single-pass ──
            float32 flexSlots[12] = {};   ///< tailles px des cellules (axe principal), calculées au Begin
            int32   flexCount    = 0;     ///< nombre de cellules
            int32   flexIdx      = 0;     ///< cellule courante
            float32 flexCross    = 0.f;   ///< taille axe transverse (hauteur de rangée / largeur de colonne)
        };

        // État de défilement persistant d'une zone (par id) : offset {x,y} + présence
        // des barres la frame précédente (pour réserver les gouttières sans osciller).
        struct NkGuiScrollState {
            float32 x = 0.f, y = 0.f;
            float32 maxX = 0.f, maxY = 0.f;   // bornes de la frame précédente (anti-overscroll)
            bool    barV = false, barH = false;
        };

        // Frame de zone défilable empilée (BeginChild/BeginPanel). Permet l'imbrication
        // (panneau scrollable contenant une ListBox, etc.).
        struct NkGuiChildFrame {
            NkGuiId     id          = NKGUI_ID_NONE;
            NkRect      area        = { 0.f, 0.f, 0.f, 0.f };   // zone de contenu (panel: sous l'en-tête)
            float32     contentTop  = 0.f;
            float32     contentLeft = 0.f;
            float32     scrollX     = 0.f;
            float32     scrollY     = 0.f;
            bool        horizontal  = false;
            NkGuiLayout savedLayout;
        };

        // Largeurs de colonnes redimensionnées par l'utilisateur (px ; 0 = non touché
        // → largeur de TableSetupColumn). Persistant par table.
        static constexpr int32 NkGuiTableMaxCols = 16;
        struct NkGuiTableW {
            float32 w[NkGuiTableMaxCols] = {};  ///< largeurs de colonnes (resize interne)
            float32 total   = 0.f;              ///< largeur globale override (0 = auto-remplir) — ResizableOuter
            int32   sortCol = -1;               ///< colonne triée (-1 = aucune) — Sortable
            bool    sortAsc = true;             ///< sens du tri
        };

        // Contexte principal. Explicite (multi-instance) ; un « contexte courant »
        // permet l'API immédiate terse (nkgui::Button(...) sans passer ctx).
        struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiContext {
            int32         viewW    = 0;
            int32         viewH    = 0;
            float32       scale    = 1.f;   ///< facteur d'échelle UI (DPI/HiDPI) — voir SetUiScale/Scaled
            NkGuiTheme    theme;
            NkGuiInput    input;
            NkGuiDrawList dl;               ///< couche principale (rendue en 1er)
            NkGuiDrawList dlOverlay;        ///< couche popups/overlay (rendue PAR-DESSUS)
            NkGuiLayout   layout;
            NkGuiFont*    font = nullptr;   ///< police par défaut (posée par l'app)

            // PILE de popups ouverts (chaîne menu → sous-menu → sous-sous-menu…).
            // `curPopupLevel` = niveau en cours de DESSIN (-1 = couche principale) ;
            // la liste active est l'overlay dès qu'on dessine un popup.
            static constexpr int32 PopupMax = 8;
            NkGuiId       popupStack[PopupMax]  = {};   ///< id du popup à chaque niveau
            NkRect        popupRects[PopupMax]  = {};   ///< zone de chaque niveau
            NkGuiLayout   popupSaved[PopupMax];          ///< layout sauvegardé par niveau
            int32         popupDepth     = 0;            ///< nb de popups ouverts
            int32         curPopupLevel  = -1;           ///< niveau dessiné (-1 = principale)
            NkVec2        popupPos        = { 0.f, 0.f }; ///< ancrage (menu contextuel)
            NkRect        popupAnchor     = { 0.f, 0.f, 0.f, 0.f }; ///< zone déclencheur (ne ferme pas)

            // ── Fenêtres flottantes (Begin/End) ───────────────────────────────
            // Chaque fenêtre dessine dans SA draw-list (pool `winDL`), fusionnées dans
            // `dl` triées par z-order à EndFrame → recouvrement correct + passage devant.
            static constexpr int32 WinMax = 32;
            NkGuiDrawList winDL[WinMax];           ///< pool de draw-lists (1 par fenêtre/frame)
            int32         winMeta[WinMax]  = {};   ///< index meta de la fenêtre frame k
            int32         winZ[WinMax]     = {};   ///< z-order de la fenêtre frame k (tri)
            int32         winCount         = 0;    ///< fenêtres soumises cette frame
            int32         curWindow        = -1;   ///< fenêtre courante (= index pool), -1 = aucune
            NkGuiId       curWindowId      = NKGUI_ID_NONE;  ///< id de la fenêtre courante
            NkGuiLayout   winSavedLayout;          ///< layout sauvegardé hors-fenêtre
            NkVector<NkGuiWindowMeta> windowMeta;  ///< état persistant par fenêtre
            NkGuiId       hoveredWindowId  = NKGUI_ID_NONE;  ///< fenêtre au-dessus sous le curseur (frame préc.)
            int32         windowZTop       = 0;    ///< compteur de z-order (passage devant)
            NkVec2        winDragOff       = { 0.f, 0.f };   ///< offset souris→origine pendant le déplacement
            bool          curWindowDocked  = false;          ///< la fenêtre courante est-elle ancrée ?
            // SetNextWindowPos/Size : appliqués à la CRÉATION de la prochaine fenêtre (FirstUseEver).
            bool          hasNextPos       = false;
            bool          hasNextSize      = false;
            NkVec2        nextPos          = { 0.f, 0.f };
            NkVec2        nextSize         = { 0.f, 0.f };

            // ── Docking (arbre de nœuds) ───────────────────────────────────────
            NkVector<NkGuiDockNode> dockNodes;
            int32         dockRoot      = -1;                ///< racine de l'arbre (-1 = pas de dockspace)
            NkGuiId       dockSpaceId   = NKGUI_ID_NONE;
            NkRect        dockSpaceRect = { 0.f, 0.f, 0.f, 0.f };
            NkGuiId       movingWindowId= NKGUI_ID_NONE;     ///< fenêtre flottante dont le titre est draggé (→ dock)
            bool          windowDockingEnabled = false;      ///< OPT-IN : droper une fenêtre sur une AUTRE flottante → hôte d'onglets (#3)
            int32         dockTargetNode= -1;                ///< feuille visée pendant le drag
            int32         dockTargetZone= -1;                ///< 0 centre, 1 G, 2 D, 3 H, 4 B
            bool          dockTabAddButton = false;          ///< l'app active le bouton « + » sur les barres d'onglets
            bool          dockHideSingleTab = false;         ///< masque la barre d'onglets d'un nœud à 1 seul panneau (façon VSCode/IDE)
            int32         dockTabAddNode   = -1;             ///< feuille où « + » a été cliqué (lu par l'app après DockSpace)
            NkGuiId       dockOverflowPopup= NKGUI_ID_NONE;  ///< popup « onglets cachés » ouvert (overflow)
            int32         dockOverflowNode = -1;             ///< feuille dont l'overflow est ouvert
            NkVec2        dockOverflowPos  = { 0.f, 0.f };

            NkGuiDrawList& DL() noexcept {
                if (curPopupLevel >= 0 || overlayDepth > 0) return dlOverlay;   // popup / couche overlay forcée
                return curWindow >= 0 ? winDL[curWindow] : dl;
            }

            // Menus : barre horizontale (curseur x) + auto-dimensionnement des popups
            // de menu (largeur/hauteur mesurées une frame, appliquées la suivante).
            NkRect        menuBarRect   = { 0.f, 0.f, 0.f, 0.f };
            float32       menuBarX      = 0.f;
            NkVector<NkGuiId> menuKeys;                  // id → taille mesurée
            NkVector<NkVec2>  menuSizes;
            // Mesure du menu en cours, PAR NIVEAU (sous-menus imbriqués).
            NkGuiId       menuMeasureId[PopupMax] = {};
            float32       menuMeasureW [PopupMax] = {};
            float32       menuMeasureH [PopupMax] = {};

            // IDs d'interaction
            NkGuiId       hotId     = NKGUI_ID_NONE;  ///< widget survolé (greedy : dernier soumis = au-dessus)
            NkGuiId       hotIdPrev = NKGUI_ID_NONE;  ///< hotId de la frame précédente (résout le z-ordre)
            NkGuiId       activeId  = NKGUI_ID_NONE;  ///< widget en interaction (persistant)
            NkGuiInteract interact  = NkGuiInteract::None;
            bool          lastItemHovered = false;    ///< le DERNIER widget interactif est-il survolé ? (tooltips)

            // Pile d'ID (scoping)
            NkGuiId       idStack[32] = {};
            int32         idDepth     = 0;

            // Pile « désactivé » (BeginDisabled/EndDisabled) — non-interactif + grisé.
            bool          disabledStack[16] = {};
            int32         disabledDepth     = 0;

            // Temps + état de SAISIE (champ focalisé, caret, défilement).
            float32       time        = 0.f;            ///< temps accumulé (blink du caret)
            NkGuiId       inputId     = NKGUI_ID_NONE;  ///< champ texte focalisé (0 = aucun)
            int32         inputCaret  = 0;              ///< position caret (octets) du champ focalisé
            float32       inputScroll = 0.f;            ///< défilement horizontal du champ focalisé
            bool          inputClickConsumed = false;   ///< un champ a-t-il pris le clic cette frame ?

            // Renommage inline (F2 / double-clic sur un item) : id de l'item édité
            // + sauvegarde du libellé pour annuler (Échap).
            NkGuiId       renameId   = NKGUI_ID_NONE;
            char          renameBackup[128] = {};

            // Champ numérique (DragFloat/DragInt) en édition TEXTE (double-clic) :
            // id du champ + tampon + dernière abscisse souris pour le cliquer-glisser.
            NkGuiId       dragEditId = NKGUI_ID_NONE;
            char          dragBuf[48] = {};
            float32       dragLastX  = 0.f;
            float32       dragLastY  = 0.f;

            // Curseur souhaité cette frame (posé par les widgets, ex. DragFloat → ↔).
            // L'app le mappe vers NkWindow::SetCursor (OPTIONNEL). Reset chaque frame.
            NkGuiCursor   wantCursor = NkGuiCursor::Arrow;

            // Stockage PERSISTANT (entre frames) : arbres ouverts + onglet
            // sélectionné par barre d'onglets.
            NkVector<NkGuiId> openNodes;
            NkVector<NkGuiId> tabBarKeys;
            NkVector<int32>   tabBarSel;

            // Liste sélectionnable (multi-select + focus clavier). Stockage par liste.
            NkVector<NkGuiId> selKeys;
            NkVector<int32>   selFocusStore;
            NkVector<int32>   selAnchorStore;
            NkGuiId  curSelList    = NKGUI_ID_NONE;   // liste en cours (entre Begin/End)
            bool*    selMask       = nullptr;          // masque de sélection (appartient à l'app)
            int32    selCount      = 0;
            bool     selMulti      = false;
            int32    selIdx        = 0;                // index de l'item courant
            int32    selFocus      = -1;               // item focalisé (clavier)
            int32    selAnchor     = -1;               // ancre (Shift+plage)
            NkGuiId  activeSelList = NKGUI_ID_NONE;    // liste ayant le focus clavier

            // Zones défilables (BeginChild/ListBox/BeginPanel) : état {x,y,barV,barH}
            // persistant par id + PILE de frames (imbrication supportée).
            NkVector<NkGuiId>          scrollKeys;
            NkVector<NkGuiScrollState> scrollVals;
            static constexpr int32 ChildMax = 8;
            NkGuiChildFrame childStack[ChildMax];
            int32           childDepth = 0;

            // Pile de CONTENEURS de layout (VBox/HBox/Grid/Stack/Group) : sauvegarde du
            // layout parent + origine, pour restaurer et avancer le parent du bloc consommé.
            static constexpr int32 ContainerMax = 32;
            NkGuiLayout     containerSaved[ContainerMax];
            NkVec2          containerStart[ContainerMax] = {};
            int32           containerDepth = 0;
            // Couche overlay forcée (PushOverlay/PopOverlay → dessin AU-DESSUS du contenu).
            int32           overlayDepth   = 0;
            // Modificateur déclenchant le scroll HORIZONTAL via la molette verticale.
            // CONFIGURABLE par l'app (`ctx.hscrollMod = NkGuiKeyMod::Ctrl;`). Défaut =
            // Maj (convention navigateurs/éditeurs). None = molette = horizontal direct.
            NkGuiKeyMod   hscrollMod       = NkGuiKeyMod::Shift;

            // ── Table (BeginTable/EndTable) ───────────────────────────────────
            // Une seule table active à la fois (pas d'imbrication v1). La table vit
            // dans le LAYOUT courant → scrolle naturellement dans un panneau/child.
            NkGuiId      tblId        = NKGUI_ID_NONE;
            uint32       tblFlags     = 0;
            int32        tblCols      = 0;   ///< nb de colonnes
            int32        tblSetup     = 0;   ///< colonnes définies via TableSetupColumn
            int32        tblCurCol    = -1;  ///< colonne courante (cellule)
            int32        tblRowIdx    = -1;  ///< index de ligne (zébrures)
            float32      tblX         = 0.f; ///< x gauche de la table
            float32      tblWidth     = 0.f; ///< largeur totale EFFECTIVE (auto ou override)
            float32      tblAvailW    = 0.f; ///< largeur auto dispo (ContentWidth au BeginTable) = max override
            float32      tblY         = 0.f; ///< y haut de la ligne courante (écran)
            float32      tblRowH      = 0.f; ///< hauteur de ligne
            int32        tblSortCol   = -1;  ///< colonne triée courante (Sortable)
            bool         tblSortAsc   = true;
            bool         tblCellClip  = false; ///< un clip de cellule est-il actif ?
            bool         tblXComputed = false; ///< bornes de colonnes calculées cette frame ?
            float32      tblSetupW[NkGuiTableMaxCols] = {};  ///< largeur demandée (px ; <=0 = étirable)
            float32      tblUserW [NkGuiTableMaxCols] = {};  ///< largeur après resize (0 = non touché)
            float32      tblColX  [NkGuiTableMaxCols + 1] = {}; ///< bornes x résolues
            const char*  tblColLbl[NkGuiTableMaxCols] = {};   ///< libellés d'en-tête
            NkGuiLayout  tblSavedLayout;                      ///< layout externe sauvegardé
            NkVector<NkGuiId>     tblKeys;    ///< largeurs persistantes par table (resize)
            NkVector<NkGuiTableW> tblWidths;

            // Color picker : HSV mémorisé par id (préserve la teinte quand S ou V → 0,
            // sinon la teinte « saute » au noir/blanc). (h[0,360], s[0,100], v[0,100], a[0,1]).
            NkVector<NkGuiId> pickerKeys;
            NkVector<NkVec4>  pickerHSV;

            // Hook de style (override de dessin par widget). Si `styleFn` est défini et
            // renvoie true pour un élément, l'app l'a dessiné → NKGui saute le défaut.
            using NkGuiStyleFn = bool (*)(NkGuiContext&, const NkGuiStyleItem&, void*);
            NkGuiStyleFn  styleFn   = nullptr;
            void*         styleUser = nullptr;

            // Rafale (repeat) — défauts globaux, surchargeables par bouton.
            float32       repeatDelay = 0.25f;   ///< délai initial avant rafale (s)
            float32       repeatRate  = 0.05f;   ///< intervalle entre rafales (s)

            // ── Cycle de vie ──────────────────────────────────────────────────
            bool Init(int32 width, int32 height) noexcept;
            void Shutdown() noexcept;

            // ── Cycle de frame ────────────────────────────────────────────────
            // BeginFrame : APRÈS que l'app ait posé l'input brut (events). Calcule
            // les transitions, réinitialise hotId + la draw list.
            void BeginFrame(float32 dt) noexcept;
            void EndFrame() noexcept;

            // ── Pile d'ID ─────────────────────────────────────────────────────
            void    PushId(const char* s) noexcept;
            void    PushId(const void* p) noexcept;
            void    PopId() noexcept;
            NkGuiId GetId(const char* s) const noexcept;   ///< hash + graine = top de pile

            // ── Layout (curseur immédiat) ─────────────────────────────────────
            void    BeginLayout(const NkRect& region) noexcept;  ///< région de contenu + curseur
            NkRect  NextItemRect(float32 w, float32 h) noexcept; ///< w<=0 = remplir la largeur
            void    SameLine(float32 spacingX = -1.f) noexcept;  ///< item suivant à droite du précédent
            void    Spacing(float32 px = -1.f) noexcept;         ///< saut vertical
            float32 ContentWidth() const noexcept;               ///< largeur restante au curseur
            float32 AvailHeight() const noexcept;                ///< hauteur restante sous le curseur (région)
            float32 ItemHeight() const noexcept;                 ///< hauteur standard d'un widget
            float32 S(float32 px) const noexcept { return px * scale; }  ///< px logiques → px écran (DPI)
            void    Indent(float32 w) noexcept;                  ///< décale le début de ligne (arbres)

            // ── Stockage (arbres / onglets) ───────────────────────────────────
            bool  IsNodeOpen(NkGuiId id) const noexcept;
            void  SetNodeOpen(NkGuiId id, bool open) noexcept;
            int32 GetTabIndex(NkGuiId bar) const noexcept;       ///< -1 si jamais défini
            void  SetTabIndex(NkGuiId bar, int32 idx) noexcept;

            // ── Liste sélectionnable (multi-select + clavier) ─────────────────
            // `mask`/`count` = sélection appartenant à l'app. Gère la navigation
            // clavier (Haut/Bas, Entrée) si la liste a le focus.
            void BeginSelectList(const char* id, bool* mask, int32 count, NkGuiSelectFlags flags) noexcept;
            void ApplySelectClick(int32 idx) noexcept;   ///< appliqué au clic d'un item (single/ctrl/shift)
            void EndSelectList() noexcept;

            // ── Désactivation contextuelle (empilable) ────────────────────────
            void BeginDisabled(bool disabled = true) noexcept;
            void EndDisabled() noexcept;
            bool IsDisabled() const noexcept {
                return disabledDepth > 0 && disabledStack[disabledDepth - 1];
            }

            // ── Popups / overlay (pile) ───────────────────────────────────────
            // Ouvre un popup de PREMIER niveau (combo, menu, menu contextuel) :
            // réinitialise la chaîne. `OpenPopupAt` mémorise en plus une ancre.
            void OpenPopup(NkGuiId id) noexcept { popupStack[0] = id; popupDepth = 1; }
            void OpenPopupAt(NkGuiId id, NkVec2 pos) noexcept { OpenPopup(id); popupPos = pos; }
            void ClosePopup() noexcept { popupDepth = 0; }
            // Ouvre/retient un popup au niveau `level` (sous-menu) en coupant les
            // niveaux plus profonds (un seul sous-menu ouvert par niveau).
            void OpenPopupLevel(NkGuiId id, int32 level) noexcept {
                if (level < 0 || level >= PopupMax) return;
                popupStack[level] = id; popupDepth = level + 1;
            }
            bool IsPopupOpen(NkGuiId id) const noexcept {
                for (int32 i = 0; i < popupDepth; ++i) if (popupStack[i] == id) return true;
                return false;
            }

            // ── Requêtes d'interaction ────────────────────────────────────────
            bool IsHovered(const NkRect& r) const noexcept;
            // Le dernier widget interactif soumis est-il survolé ? (pour SetTooltip).
            bool IsItemHovered() const noexcept { return lastItemHovered; }

            // ⭐ Survol respectant le z-ordre (overlap). Marque hotId de façon
            // « gourmande » (le DERNIER widget soumis sous le pointeur = celui au
            // -dessus gagne) et ne retourne true QUE pour le front-most (via
            // hotIdPrev) → un widget masqué par un autre au-dessus ne capture pas.
            // À appeler par CHAQUE widget interactif avant de réagir.
            bool ItemHoverable(const NkRect& r, NkGuiId id) noexcept;

            // Comportement bouton générique (hot/active/click). Met à jour hotId/
            // activeId/interact. Sans Repeat : retourne true au relâchement dans le
            // rect (sémantique « clic »). Avec Repeat : true à l'appui PUIS en rafale
            // tant que maintenu (délai/cadence = override >=0, sinon défauts du ctx).
            bool ButtonBehavior(NkGuiId id, const NkRect& r,
                                NkGuiButtonFlags flags = NkGuiButtonFlags::None,
                                float32 repeatDelayOverride = -1.f,
                                float32 repeatRateOverride  = -1.f,
                                bool* outHovered = nullptr, bool* outHeld = nullptr) noexcept;
        };

        // ── Contexte courant (pour l'API immédiate) ───────────────────────────
        NKENTSEU_NKGUI_API void          SetCurrentContext(NkGuiContext* ctx) noexcept;
        NKENTSEU_NKGUI_API NkGuiContext* GetCurrentContext() noexcept;

    } // namespace nkgui
} // namespace nkentseu
