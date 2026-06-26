#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiWidgets.h
// @Brief   Widgets immédiats NKGui (Phase 2 : amorce — Button, Panel). Les
//          labels texte (NKFont) et le reste des widgets arrivent en Phase 3.
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------

#include "NKGui/NkGuiExport.h"
#include "NKGui/Core/NkGuiContext.h"

namespace nkentseu {
    namespace nkgui {

        // Fond de panneau (rectangle thème + bord).
        NKENTSEU_NKGUI_API void PanelBackground(NkGuiContext& ctx, const NkRect& r) noexcept;

        // Texte à une position (coin haut-gauche). Utilise ctx.font. col par défaut
        // = thème. Renvoie la largeur dessinée.
        NKENTSEU_NKGUI_API float32 TextAt(NkGuiContext& ctx, const NkVec2& topLeft, const char* s) noexcept;
        NKENTSEU_NKGUI_API float32 TextAt(NkGuiContext& ctx, const NkVec2& topLeft, const char* s, const NkColor& col) noexcept;

        // Bouton standard : true au clic (relâche dans le rect). `label` = identité
        // (texte rendu en Phase 3).
        NKENTSEU_NKGUI_API bool Button(NkGuiContext& ctx, const char* label, const NkRect& r) noexcept;

        // Bouton étendu : flags (ex. Repeat) + délai/cadence de rafale PAR BOUTON
        // (-1 = défauts du contexte ctx.repeatDelay / ctx.repeatRate).
        NKENTSEU_NKGUI_API bool ButtonEx(NkGuiContext& ctx, const char* label, const NkRect& r,
                                         NkGuiButtonFlags flags,
                                         float32 repeatDelay = -1.f, float32 repeatRate = -1.f) noexcept;

        // Raccourci : bouton à rafale (Repeat) avec délai/cadence optionnels.
        NKENTSEU_NKGUI_API bool RepeatButton(NkGuiContext& ctx, const char* label, const NkRect& r,
                                             float32 repeatDelay = -1.f, float32 repeatRate = -1.f) noexcept;

        // ── Widgets AUTO-LAYOUT (positionnés via ctx.layout / NextItemRect) ──────
        // À utiliser entre BeginPanel/EndPanel (ou après ctx.BeginLayout).
        NKENTSEU_NKGUI_API void Text(NkGuiContext& ctx, const char* s) noexcept;
        // Texte avec RETOUR À LA LIGNE : coupe aux mots (repli caractère), respecte les
        // '\n'. `wrapWidth <= 0` → largeur de contenu restante. Avance le curseur de
        // N lignes. Base des libellés longs, tooltips multi-lignes, multi-ligne InputText.
        NKENTSEU_NKGUI_API void TextWrapped(NkGuiContext& ctx, const char* text,
                                            float32 wrapWidth = -1.f) noexcept;
        NKENTSEU_NKGUI_API bool Button(NkGuiContext& ctx, const char* label) noexcept;   // rect auto
        NKENTSEU_NKGUI_API bool Checkbox(NkGuiContext& ctx, const char* label, bool& value) noexcept;
        // Case à cocher TRI-ÉTAT. Clic : Off→On, On→Off, Mixed→On (sémantique
        // standard ; l'état Mixed est en général posé par l'app, ex. select-all).
        NKENTSEU_NKGUI_API bool CheckboxTristate(NkGuiContext& ctx, const char* label, NkGuiCheck& state) noexcept;
        // Case TRI-ÉTAT COMPACTE (sans libellé) — à composer dans une ligne d'arbre
        // (CheckBox3 + SameLine + TreeNodeEditable/SelectableEditable). Bascule
        // Off↔On (Mixed→Off) et retourne true au clic ; l'app applique alors la
        // cascade via NkGuiTreeCascade si elle le souhaite.
        NKENTSEU_NKGUI_API bool CheckBox3(NkGuiContext& ctx, const char* idStr, NkGuiCheck& state) noexcept;

        // ── Helpers OPTIONNELS de sélection hiérarchique (modèle hybride) ──────
        // NKGui ne stocke PAS la hiérarchie (façon ImGui) : l'app possède `states[]`
        // (1 NkGuiCheck par nœud) + `parent[]` (index du parent, -1 = racine). Ces
        // utilitaires agissent sur ces tableaux quand l'app le décide (ex. Alt+clic).
        // Cascade RÉCURSIVE : met `node` + toute sa sous-arborescence à `value`.
        NKENTSEU_NKGUI_API void NkGuiTreeCascade(NkGuiCheck* states, const int32* parent, int32 count,
                                                 int32 node, NkGuiCheck value) noexcept;
        // Agrège l'état tri-état des nœuds internes (On/Off/Mixed) depuis leurs
        // enfants, bottom-up. Les feuilles ne sont pas modifiées.
        NKENTSEU_NKGUI_API void NkGuiTreeRecomputeMixed(NkGuiCheck* states, const int32* parent, int32 count) noexcept;
        NKENTSEU_NKGUI_API bool SliderFloat(NkGuiContext& ctx, const char* label,
                                            float32& value, float32 vmin, float32 vmax) noexcept;

        // ── Graphiques (data-viz) ─────────────────────────────────────────────
        // Barre de progression. `fraction` ∈ [0,1] ; `overlay` = texte centré (nullptr
        // → pourcentage auto). Hauteur = ItemHeight, pleine largeur du contenu.
        NKENTSEU_NKGUI_API void ProgressBar(NkGuiContext& ctx, float32 fraction,
                                            const char* overlay = nullptr) noexcept;
        // Courbe (PlotLines) / histogramme (PlotHistogram) d'un tableau de valeurs.
        // `minV>=maxV` → échelle AUTO (min/max des données). `height<=0` → 60px.
        // AMÉLIORATION vs ImGui : la valeur sous le curseur s'affiche en direct (survol).
        NKENTSEU_NKGUI_API void PlotLines(NkGuiContext& ctx, const char* label,
                                          const float32* values, int32 count,
                                          float32 minV = 0.f, float32 maxV = 0.f, float32 height = 0.f) noexcept;
        NKENTSEU_NKGUI_API void PlotHistogram(NkGuiContext& ctx, const char* label,
                                              const float32* values, int32 count,
                                              float32 minV = 0.f, float32 maxV = 0.f, float32 height = 0.f) noexcept;

        // ── Couleur (ColorButton / ColorEdit / ColorPicker) ───────────────────
        // `col` = RGBA float ∈ [0,1] (4 composantes ; NoAlpha → 3 lues/écrites).
        // Pastille de couleur cliquable. `w/h <= 0` → tailles par défaut.
        NKENTSEU_NKGUI_API bool ColorButton(NkGuiContext& ctx, const char* idStr, const float32* col,
                                            float32 w = 0.f, float32 h = 0.f,
                                            NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;
        // Sélecteur complet : carré Saturation/Valeur + barre de Teinte (+ barre Alpha).
        // Glisser dans chaque zone modifie la couleur. La TEINTE est préservée même au
        // noir/blanc (HSV mémorisé par id). Retourne true si la couleur a changé.
        NKENTSEU_NKGUI_API bool ColorPicker4(NkGuiContext& ctx, const char* label, float32* col,
                                             NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;
        // Pastille + code hexadécimal sur une ligne ; clic sur la pastille → ouvre le
        // ColorPicker en popup (overlay). Retourne true si la couleur a changé.
        NKENTSEU_NKGUI_API bool ColorEdit4(NkGuiContext& ctx, const char* label, float32* col,
                                           NkGuiColorFlags flags = NkGuiColorFlags::None) noexcept;

        // ── Image / Icône (quad texturé) ──────────────────────────────────────
        // Dessine la texture `texId` (id backend) à la taille w×h. `tint` multiplie
        // (blanc = telle quelle) ; `uv0/uv1` = sous-région (atlas/sprite). Auto-layout.
        NKENTSEU_NKGUI_API void Image(NkGuiContext& ctx, uint32 texId, float32 w, float32 h,
                                      NkColor tint = NkColor{ 255, 255, 255, 255 },
                                      NkVec2 uv0 = NkVec2{ 0.f, 0.f }, NkVec2 uv1 = NkVec2{ 1.f, 1.f }) noexcept;
        // Bouton-image (icône cliquable) : fond + image centrée + survol. Retourne true au clic.
        NKENTSEU_NKGUI_API bool ImageButton(NkGuiContext& ctx, const char* idStr, uint32 texId,
                                            float32 w, float32 h, NkColor tint = NkColor{ 255, 255, 255, 255 },
                                            NkVec2 uv0 = NkVec2{ 0.f, 0.f }, NkVec2 uv1 = NkVec2{ 1.f, 1.f }) noexcept;
        // Champs numériques façon ImGui. DragFloat/DragInt : GLISSER sur le champ pour
        // changer la valeur (vitesse `speed`/pixel) + DOUBLE-CLIC pour saisir au clavier.
        // InputFloat/InputInt : idem + boutons -/+ (pas `step`). `dir` = direction du
        // glissement (Horizontal défaut / Vertical / Both). Pendant le survol/glisser,
        // le widget pose `ctx.wantCursor` (↔ ou ↕) que l'app peut appliquer. Bornés
        // [vmin, vmax]. Retourne true si la valeur a changé.
        NKENTSEU_NKGUI_API bool DragFloat(NkGuiContext& ctx, const char* label, float32& v,
                                          float32 speed = 0.1f, float32 vmin = -1.0e30f, float32 vmax = 1.0e30f,
                                          NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;
        NKENTSEU_NKGUI_API bool DragInt(NkGuiContext& ctx, const char* label, int32& v,
                                        float32 speed = 0.25f, int32 vmin = -2147483640, int32 vmax = 2147483640,
                                        NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;
        NKENTSEU_NKGUI_API bool InputFloat(NkGuiContext& ctx, const char* label, float32& v,
                                           float32 step = 1.f, float32 vmin = -1.0e30f, float32 vmax = 1.0e30f,
                                           NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;
        NKENTSEU_NKGUI_API bool InputInt(NkGuiContext& ctx, const char* label, int32& v,
                                         int32 step = 1, int32 vmin = -2147483640, int32 vmax = 2147483640,
                                         NkGuiDragDir dir = NkGuiDragDir::Horizontal) noexcept;
        // Champ de saisie mono-ligne (focus, caret UTF-8, défilement, blink).
        // `buf`/`bufSize` = tampon C éditable. Retourne true à la pression d'Entrée.
        NKENTSEU_NKGUI_API bool InputText(NkGuiContext& ctx, const char* label,
                                          char* buf, int32 bufSize) noexcept;
        // Variante avec FLAGS (mot de passe, décimal, hexa, majuscules, sans espace,
        // lecture seule) + limite de CARACTÈRES (`maxChars`, -1 = pas de limite, en
        // codepoints — distinct de bufSize qui borne les octets).
        NKENTSEU_NKGUI_API bool InputTextEx(NkGuiContext& ctx, const char* label, char* buf, int32 bufSize,
                                            NkGuiInputFlags flags, int32 maxChars = -1) noexcept;
        // Champ de saisie MULTI-LIGNE dans `rect` : Entrée = saut de ligne, flèches
        // ↑/↓ entre lignes, clic 2D, auto-scroll vers le caret + molette + scrollbar.
        // Flags (ReadOnly/filtres) + maxChars. Retourne true si le texte a changé.
        NKENTSEU_NKGUI_API bool InputTextMultiline(NkGuiContext& ctx, const char* idStr, char* buf, int32 bufSize,
                                                   const NkRect& rect,
                                                   NkGuiInputFlags flags = NkGuiInputFlags::None,
                                                   int32 maxChars = -1) noexcept;
        NKENTSEU_NKGUI_API void Separator(NkGuiContext& ctx) noexcept;

        // ════════════════ CONTENEURS DE LAYOUT ════════════════
        // Empilent un sous-flux au curseur courant ; End* restaure le parent et l'avance du
        // bloc consommé. Composables (HBox dans une cellule de Grid, VBox dans un HBox…).
        // `gap < 0` (défaut) = garder l'espacement courant ; sinon impose l'espacement.
        NKENTSEU_NKGUI_API void BeginVBox (NkGuiContext& ctx, float32 gap = -1.f) noexcept;  ///< empilement vertical
        NKENTSEU_NKGUI_API void EndVBox   (NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void BeginHBox (NkGuiContext& ctx, float32 gap = -1.f) noexcept;  ///< alignement horizontal
        NKENTSEU_NKGUI_API void EndHBox   (NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void BeginGrid (NkGuiContext& ctx, int32 columns, float32 gap = -1.f) noexcept;  ///< grille N colonnes
        NKENTSEU_NKGUI_API void EndGrid   (NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void BeginGroup(NkGuiContext& ctx) noexcept;   ///< groupe (traite N widgets comme 1 bloc)
        NKENTSEU_NKGUI_API void EndGroup  (NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void BeginFlow (NkGuiContext& ctx, float32 gap = -1.f) noexcept;  ///< horizontal avec retour ligne auto (tags, toolbars)
        NKENTSEU_NKGUI_API void EndFlow   (NkGuiContext& ctx) noexcept;
        // FLEX À POIDS (flex-grow / stretch / fill). `sizes[i] > 0` = px fixes ; `sizes[i] < 0` =
        // poids (-1 = poids 1) qui se partagent l'espace RESTANT. Single-pass exact (total connu).
        // BeginRow : rangée horizontale, hauteur `height` (<=0 = ItemHeight), les widgets remplissent
        //            chaque cellule (largeur = cellule, hauteur étirée).
        NKENTSEU_NKGUI_API void BeginRow(NkGuiContext& ctx, float32 height, const float32* sizes, int32 count, float32 gap = -1.f) noexcept;
        NKENTSEU_NKGUI_API void EndRow  (NkGuiContext& ctx) noexcept;
        // BeginColumn : colonne verticale sur `totalHeight` (<=0 = hauteur restante de la région →
        //               « prend tout ce qui reste »), largeur `width` (<=0 = pleine largeur).
        NKENTSEU_NKGUI_API void BeginColumn(NkGuiContext& ctx, float32 width, const float32* sizes, int32 count, float32 totalHeight = -1.f, float32 gap = -1.f) noexcept;
        NKENTSEU_NKGUI_API void EndColumn  (NkGuiContext& ctx) noexcept;
        // ÉCHELLE UI (DPI/HiDPI) : SetUiScale multiplie padding/rounding/espacement ; l'app recharge
        // la police à tailleBase*scale. Scaled(px) = px * ctx.scale (pour tailles explicites).
        NKENTSEU_NKGUI_API void    SetUiScale(NkGuiContext& ctx, float32 s) noexcept;
        NKENTSEU_NKGUI_API float32 Scaled    (NkGuiContext& ctx, float32 px) noexcept;
        NKENTSEU_NKGUI_API void SpringRight(NkGuiContext& ctx, float32 width) noexcept;  ///< pousse les items suivants à droite (réserve `width`)
        // Poignée de redimensionnement : glisse *value (px) entre min/max. vertical=true → barre verticale (glisse X).
        NKENTSEU_NKGUI_API bool Splitter(NkGuiContext& ctx, const char* idStr, const NkRect& handle, bool vertical,
                                         float32* value, float32 minV, float32 maxV) noexcept;
        // Stack : enfants superposés dans une boîte width×height ; ancrer le prochain via StackAnchor.
        NKENTSEU_NKGUI_API void BeginStack (NkGuiContext& ctx, float32 width, float32 height) noexcept;
        NKENTSEU_NKGUI_API void StackAnchor(NkGuiContext& ctx, int32 anchor) noexcept;  ///< 0=HG 1=HC 2=HD 3=CG 4=C 5=CD 6=BG 7=BC 8=BD
        NKENTSEU_NKGUI_API void EndStack   (NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void Spacer(NkGuiContext& ctx, float32 w, float32 h) noexcept;  ///< espace invisible
        // Couche overlay : dessin AU-DESSUS du contenu courant entre Push/Pop (badges, surbrillances).
        NKENTSEU_NKGUI_API void PushOverlay(NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void PopOverlay (NkGuiContext& ctx) noexcept;
        // En-tête repliable (accordéon) : true si ouvert → l'app dessine le contenu de la section.
        NKENTSEU_NKGUI_API bool CollapsingHeader(NkGuiContext& ctx, const char* label) noexcept;

        // Arbre repliable : true si ouvert (dessiner les enfants puis TreePop).
        NKENTSEU_NKGUI_API bool TreeNode(NkGuiContext& ctx, const char* label) noexcept;
        NKENTSEU_NKGUI_API void TreePop(NkGuiContext& ctx) noexcept;

        // Nœud d'arbre RENOMMABLE : clic ouvre/ferme, double-clic (ou Maj+Entrée)
        // édite le libellé inline dans `label` (tampon mutable). `idStr` = id STABLE.
        // Entrée valide, Échap annule. À refermer par TreePop si ouvert.
        NKENTSEU_NKGUI_API bool TreeNodeEditable(NkGuiContext& ctx, const char* idStr,
                                                 char* label, int32 bufSize,
                                                 bool allowRename = true) noexcept;

        // Élément SÉLECTIONNABLE (ligne pleine largeur). Surligné si `selected`,
        // survol mis en évidence. Retourne true au clic. Respecte BeginDisabled
        // (non cliquable + grisé) → permet de décider du sélectionnable par contexte.
        NKENTSEU_NKGUI_API bool Selectable(NkGuiContext& ctx, const char* label, bool selected) noexcept;

        // Comme Selectable mais RENOMMABLE : double-clic (ou Maj+Entrée pendant
        // survol) édite le libellé inline dans `label` (tampon mutable). `idStr` =
        // id STABLE. Entrée valide, Échap annule. Pour feuilles d'arbre / listes.
        // `allowRename=false` refuse le renommage de CET élément (décision dynamique
        // par appelant) tout en gardant la sélection.
        NKENTSEU_NKGUI_API bool SelectableEditable(NkGuiContext& ctx, const char* idStr,
                                                   char* label, int32 bufSize, bool selected,
                                                   bool allowRename = true) noexcept;

        // Item d'une liste sélectionnable (entre ctx.BeginSelectList / EndSelectList).
        // Distingue survol / sélection (fond bleu) / FOCUS clavier (liseré accent).
        NKENTSEU_NKGUI_API bool SelectItem(NkGuiContext& ctx, const char* label) noexcept;

        // Item sélectionnable RENOMMABLE : double-clic (ou Maj+Entrée sur l'item
        // focalisé) ouvre une édition inline dans le tampon `label`. `idStr` est un
        // identifiant STABLE (indépendant du libellé édité). Entrée valide, Échap
        // annule. Retourne true au clic de sélection.
        NKENTSEU_NKGUI_API bool SelectItemEditable(NkGuiContext& ctx, const char* idStr,
                                                   char* label, int32 bufSize,
                                                   bool allowRename = true) noexcept;

        // Barre d'onglets : dessine les onglets, gère la sélection (persistante par
        // `id`), retourne l'index sélectionné. Le contenu se dessine APRÈS l'appel.
        NKENTSEU_NKGUI_API int32 TabBar(NkGuiContext& ctx, const char* id,
                                        const char* const* labels, int32 count) noexcept;
        // Variante : `enabled[i] == false` => onglet grisé et NON sélectionnable
        // (enabled peut être nullptr = tous actifs).
        NKENTSEU_NKGUI_API int32 TabBarEx(NkGuiContext& ctx, const char* id,
                                          const char* const* labels, int32 count,
                                          const bool* enabled) noexcept;
        // Onglets RENOMMABLES : `labels` = tableau de tampons MUTABLES (chacun de
        // taille `labelBufSize`), double-clic sur un onglet l'édite inline. `enabled`
        // et `allowRename` (nullptr = défauts) permettent de griser / refuser le
        // renommage onglet par onglet, dynamiquement.
        NKENTSEU_NKGUI_API int32 TabBarEditable(NkGuiContext& ctx, const char* id,
                                                char* const* labels, int32 count, int32 labelBufSize,
                                                const bool* enabled = nullptr,
                                                const bool* allowRename = nullptr) noexcept;

        // Conteneur : fond + barre de titre + ouvre la région de layout (clip).
        // (Niveau unique pour l'instant ; l'imbrication viendra avec une pile.)
        NKENTSEU_NKGUI_API bool BeginPanel(NkGuiContext& ctx, const char* title, const NkRect& r) noexcept;
        NKENTSEU_NKGUI_API void EndPanel(NkGuiContext& ctx) noexcept;

        // ── Fenêtres FLOTTANTES (multi-fenêtres façon ImGui) ──────────────────
        // Fenêtre déplaçable (barre de titre) + redimensionnable (coin bas-droit) +
        // repliable (chevron) + fermable (`open` → croix). Plusieurs fenêtres
        // coexistent dans la fenêtre OS, avec Z-ORDER : cliquer en passe une devant
        // (occlusion gérée). Position/taille/repli PERSISTÉS par titre. Appeler End
        // UNIQUEMENT si Begin retourne true (convention NKGui, comme BeginPanel).
        //   if (Begin(ctx,"Inspecteur",&open)) { …widgets… EndWindow(ctx); }
        // (sortie en fenêtre OS séparée = multi-viewport, viendra avec les backends.)
        // Pos/taille de la PROCHAINE fenêtre, appliquées à sa CRÉATION (FirstUseEver) —
        // l'utilisateur peut ensuite la bouger/redimensionner. Idéal pour le layout par
        // défaut des panneaux d'un éditeur.
        NKENTSEU_NKGUI_API void SetNextWindowPos (NkGuiContext& ctx, float32 x, float32 y) noexcept;
        NKENTSEU_NKGUI_API void SetNextWindowSize(NkGuiContext& ctx, float32 w, float32 h) noexcept;
        NKENTSEU_NKGUI_API bool Begin(NkGuiContext& ctx, const char* title, bool* open = nullptr,
                                      NkGuiWindowFlags flags = NkGuiWindowFlags::None) noexcept;
        NKENTSEU_NKGUI_API void EndWindow(NkGuiContext& ctx) noexcept;

        // ── DOCKING : ancrer des fenêtres ensemble (splits + onglets) ─────────
        // Région où ancrer des fenêtres. À appeler AVANT les Begin des fenêtres
        // dockables. Glisser une fenêtre flottante par sa barre de titre sur le
        // DockSpace → boussole (centre = onglet, bords = split) ; relâcher = ancre.
        // Glisser un onglet vers le bas/dehors = désancre (redevient flottante).
        // Splitters draggables entre régions ancrées. (Mode v1.)
        NKENTSEU_NKGUI_API void DockSpace(NkGuiContext& ctx, const char* idStr, const NkRect& rect) noexcept;
        // Ancre une fenêtre dans le DockSpace PAR PROGRAMME (layout par défaut au boot).
        // `zone` : 0 = onglet, 1 = gauche, 2 = droite, 3 = haut, 4 = bas (sur la 1re feuille).
        NKENTSEU_NKGUI_API void DockBuilderDock(NkGuiContext& ctx, const char* windowTitle, int32 zone) noexcept;
        // DockSpace qui remplit TOUT le viewport (responsive : suit la taille de la
        // fenêtre OS → le dock + les fenêtres ancrées s'agrandissent avec elle).
        // `topMargin` = hauteur réservée en haut (ex. barre de menus).
        NKENTSEU_NKGUI_API void DockSpaceOverViewport(NkGuiContext& ctx, float32 topMargin = 0.f) noexcept;
        // Bouton « + » d'onglet (opt-in : l'app pose `ctx.dockTabAddButton = true`).
        // Après DockSpace, `DockTabAddRequest` renvoie la FEUILLE où « + » a été cliqué
        // (-1 = aucun) ; l'app crée alors une fenêtre et l'ancre via `DockAddTab`.
        // #3 : fusionne `winTitle` dans `hostTitle` PAR PROGRAMME → `hostTitle` devient
        // un hôte de dock flottant à onglets (layout par défaut au boot).
        NKENTSEU_NKGUI_API void DockWindowIntoWindow(NkGuiContext& ctx, const char* hostTitle, const char* winTitle) noexcept;
        NKENTSEU_NKGUI_API int32 DockTabAddRequest(NkGuiContext& ctx) noexcept;
        NKENTSEU_NKGUI_API void  DockAddTab(NkGuiContext& ctx, const char* windowTitle, int32 node) noexcept;

        // ── Zone défilable + ListBox ──────────────────────────────────────────
        // Région à défilement VERTICAL : clippe son contenu à `rect`, gère la molette
        // (survol) + une scrollbar draggable a droite, scroll persistant par `idStr`.
        // L'app dessine ses widgets entre Begin/EndChild ; ce qui déborde défile.
        // (v1 : un seul niveau, pas d'imbrication.) Brique du multi-ligne, longues listes…
        // `horizontal=true` active aussi le défilement HORIZONTAL (scrollbar basse +
        // Maj/molette horizontale), pour du contenu à largeur fixe plus large que la vue.
        NKENTSEU_NKGUI_API bool BeginChild(NkGuiContext& ctx, const char* idStr,
                                           const NkRect& rect, bool border = true,
                                           bool horizontal = false) noexcept;
        NKENTSEU_NKGUI_API void EndChild(NkGuiContext& ctx) noexcept;
        // ListBox = cadre défilable ; l'app dessine ses Selectable()/SelectItem() dedans.
        NKENTSEU_NKGUI_API bool BeginListBox(NkGuiContext& ctx, const char* idStr, const NkRect& rect) noexcept;
        NKENTSEU_NKGUI_API void EndListBox(NkGuiContext& ctx) noexcept;

        // ── Table (colonnes redimensionnables, zébrures, en-tête) ─────────────
        // Idiome ImGui amélioré « à notre sauce » :
        //   if (BeginTable(ctx,"t",3, Borders|RowBg|Resizable)) {
        //       TableSetupColumn(ctx,"Nom",   0);     // 0 = colonne étirable
        //       TableSetupColumn(ctx,"Type", 90);     // largeur fixe px
        //       TableSetupColumn(ctx,"Taille",70);
        //       TableHeadersRow(ctx);
        //       for (…) { TableNextRow(ctx);
        //                 TableNextColumn(ctx); Text(ctx,nom);
        //                 TableNextColumn(ctx); Text(ctx,type);
        //                 TableNextColumn(ctx); Text(ctx,taille); }
        //       EndTable(ctx);
        //   }
        // AMÉLIORATIONS vs ImGui : colonnes redimensionnables PAR DÉFAUT (glisser les
        // séparateurs d'en-tête, largeurs persistées par id) ; la table vit dans le
        // LAYOUT courant (donc scrolle naturellement si placée dans un panneau/child).
        // La table occupe toute la largeur du contenu au curseur. Tri = à venir.
        NKENTSEU_NKGUI_API bool BeginTable(NkGuiContext& ctx, const char* idStr, int32 columns,
                                           NkGuiTableFlags flags = NkGuiTableFlags::Borders) noexcept;
        // Définit une colonne (libellé + largeur souhaitée px ; <=0 = étirable). À
        // appeler `columns` fois juste après BeginTable, avant TableHeadersRow.
        NKENTSEU_NKGUI_API void TableSetupColumn(NkGuiContext& ctx, const char* label, float32 width = 0.f) noexcept;
        // Dessine la ligne d'en-tête (libellés) + gère le redimensionnement des colonnes.
        NKENTSEU_NKGUI_API void TableHeadersRow(NkGuiContext& ctx) noexcept;
        // Démarre une nouvelle ligne (fond alterné si RowBg).
        NKENTSEU_NKGUI_API void TableNextRow(NkGuiContext& ctx, float32 minHeight = 0.f) noexcept;
        // Passe à la cellule suivante (revient à la 1re colonne de la ligne suivante
        // si on était au bout). Ouvre la région de layout sur la cellule : les widgets
        // dessinés ensuite remplissent la cellule. Retourne true (cellule visible).
        NKENTSEU_NKGUI_API bool TableNextColumn(NkGuiContext& ctx) noexcept;
        // Positionne explicitement sur la colonne `n` de la ligne courante.
        NKENTSEU_NKGUI_API bool TableSetColumnIndex(NkGuiContext& ctx, int32 n) noexcept;
        NKENTSEU_NKGUI_API void EndTable(NkGuiContext& ctx) noexcept;
        // Tri (flag Sortable) : renvoie la colonne triée (`*outCol`) + le sens (`*outAscending`),
        // et true si un tri est actif (sinon col=-1). À appeler APRÈS TableHeadersRow ; l'app
        // trie ses données puis dessine les lignes dans l'ordre. La sélection est persistée.
        NKENTSEU_NKGUI_API bool TableGetSortColumn(NkGuiContext& ctx, int32* outCol, bool* outAscending) noexcept;
        // Contenu de cellule TEXTE, OPTIONNELLEMENT éditable par double-clic. À appeler
        // dans une cellule (après TableNextColumn). `idStr` = id STABLE et unique par
        // cellule (≠ texte édité). `editable=false` → simple affichage centré ; `true` →
        // survol surligné + double-clic = éditeur inline (TextEditField, Entrée valide,
        // Échap annule). Retourne true quand l'édition est validée (valeur modifiée).
        NKENTSEU_NKGUI_API bool TableCellText(NkGuiContext& ctx, const char* idStr,
                                              char* buf, int32 bufSize, bool editable = false) noexcept;

        // ── Popups / overlay ──────────────────────────────────────────────────
        // Liste déroulante. `preview` = texte affiché dans le champ ; `itemCount` =
        // nb d'éléments (dimensionne le popup). Si ouvert, retourne true : l'app
        // dessine ses Selectable() à l'intérieur (clic → fermer via ctx.ClosePopup())
        // puis appelle EndCombo. Le popup est rendu DANS LA COUCHE OVERLAY (au-dessus).
        NKENTSEU_NKGUI_API bool BeginCombo(NkGuiContext& ctx, const char* label,
                                           const char* preview, int32 itemCount) noexcept;
        NKENTSEU_NKGUI_API void EndCombo(NkGuiContext& ctx) noexcept;
        // Primitif de popup générique (fermé au clic-dehors / Échap). Ouvre avec
        // ctx.OpenPopup(ctx.GetId(idStr)). À refermer par EndPopup si true.
        NKENTSEU_NKGUI_API void EndPopup(NkGuiContext& ctx) noexcept;

        // ── Menus (barre + sous-menus imbriqués + contextuel) ─────────────────
        // Barre de menus horizontale (titres via BeginMenu). À fermer par EndMenuBar.
        NKENTSEU_NKGUI_API bool BeginMenuBar(NkGuiContext& ctx, const NkRect& rect) noexcept;
        NKENTSEU_NKGUI_API void EndMenuBar(NkGuiContext& ctx) noexcept;
        // Menu déroulant. Appelé dans une MenuBar (titre) OU dans un autre menu
        // (SOUS-MENU, flyout à droite) — imbrication à profondeur arbitraire. Si
        // ouvert, retourne true : l'app dessine MenuItem()/BeginMenu()/Separator()
        // ou TOUT widget (graphismes custom) à l'intérieur, puis appelle EndMenu.
        NKENTSEU_NKGUI_API bool BeginMenu(NkGuiContext& ctx, const char* label) noexcept;
        NKENTSEU_NKGUI_API void EndMenu(NkGuiContext& ctx) noexcept;
        // Élément de menu cliquable (+ raccourci affiché à droite, optionnel).
        // Retourne true au clic (et ferme la chaîne de menus).
        NKENTSEU_NKGUI_API bool MenuItem(NkGuiContext& ctx, const char* label,
                                         const char* shortcut = nullptr, bool enabled = true) noexcept;
        // Menu CONTEXTUEL (clic droit) : l'app appelle
        // ctx.OpenPopupAt(ctx.GetId(idStr), mousePos) au clic droit, puis dessine
        // le contenu entre BeginPopupMenu/EndPopupMenu (mêmes MenuItem/BeginMenu).
        NKENTSEU_NKGUI_API bool BeginPopupMenu(NkGuiContext& ctx, const char* idStr) noexcept;
        NKENTSEU_NKGUI_API void EndPopupMenu(NkGuiContext& ctx) noexcept;

        // Infobulle au survol (couche overlay, au-dessus de tout). À appeler quand le
        // dernier widget est survolé : `if (ctx.IsItemHovered()) SetTooltip(ctx, "…");`.
        NKENTSEU_NKGUI_API void SetTooltip(NkGuiContext& ctx, const char* text) noexcept;

    } // namespace nkgui
} // namespace nkentseu
