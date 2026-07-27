# Noge / Nogee — Note de reprise (handoff)

> But de ce document : permettre à un futur intervenant (humain ou modèle) de **reprendre le flambeau** sur Noge (moteur de jeu) et Nogee (éditeur) sans redécouvrir le contexte. Dernière mise à jour : 2026-07-26.

## Vision directrice (à ne pas perdre)

Tout l'écosystème (moteur **Noge**, éditeur **Nogee**, apps **NkAnima**/animation, **NKCode**/IDE, etc.) vise trois choses ensemble :
1. **Simplicité d'utilisation** — API et UX faciles (ex. écrire une petite app 2D/3D en incluant simplement `<Nkentseu.h>` ; d'où l'importance que l'en-tête public compile toujours).
2. **Puissance du moteur** — sans sacrifier les capacités avancées.
3. **Boosté en IA, supervision humaine, prompt NON obligatoire** — l'IA assiste sans imposer le prompt ; on peut à tout moment reprendre la main en manuel.

Règle esthétique éditeur : gizmo/grille/sélection **façon Blender, épuré** (formes pleines, pas de fil de fer ; axes **X=rouge, Y=vert, Z=bleu** ; sélection = liseré silhouette qui épouse le mesh, pas une boîte).

## Conventions dures (obligatoires)

- **ZÉRO STL** : jamais `std::*` ni `<vector>/<string>/<functional>/<chrono>/<cmath>`… → `NkVector`/`NkString`/`NkHashMap`/`NkFunction` (NKContainers), `NkUniquePtr`/`NkSharedPtr` (NKMemory), `NkChrono` (NKTime), fonctions NKMath. Si une brique manque : l'**ajouter au module Nkentseu** concerné (ne pas tomber dans la stdlib). Priorité : (1) réutiliser l'existant Nkentseu → (2) sinon brique externe empaquetée (Externals/submodule) → (3) sinon implémenter la nôtre (la finalité).
- Allocation **uniquement via NKMemory** ; tout `Create` a son `Destroy`.
- **NKLogger** pour tout log (jamais printf/cout).
- Une instruction par ligne ; indentation par namespace.
- Git : **jamais** de mention IA/Claude dans les messages ; committer via `./gitcommit.sh "<msg>" [chemins...]` (identité LeTeguis). Build via `jenga build --target <X> --config Debug --platform Windows` — **toujours bloquant, un seul build à la fois**.

## État actuel (vérifié, 2026-07-26)

- `Noge` : **build vert (38/38)**. `renderdemo` (Applications/Sandbox, Demo3D) : **build vert (29/29)**, backend OpenGL.
- Les **20 en-têtes « spec » de Noge** qui ne compilaient pas (jamais inclus par un .cpp, donc invisibles au build) ont été réparés → **20/20 compilent isolément** (`-fsyntax-only`). Dont l'en-tête public `Nkentseu.h`.
- Déduplication (Tier 0) faite : supprimés `NkComponentRegistry.h` (doublon NKSerialization), `Topology/NkHalfEdge.h`+`NkBooleanOp.h` (demi-arête morte, `renderer::NkEditMesh` fait tout), `Viewport/NkGizmo.h` (→ `renderer::NkGizmo3D`) ; `NkColorRGBA`→`math::NkColor`.
- **Éditeur (dans NKRenderer, réutilisable par Noge/Nogee)** : gizmo Blender **plein** (cônes/cubes/rubans, X=rouge Y=vert Z=bleu, survol, taille écran-constante) ; grille infinie Blender (fines lignes AA, majeures /10, fade distance, axes sol colorés) ; **contour de sélection silhouette** (post-process masque R8 → edge-detect → liseré orange épousant le mesh, **par défaut**, l'AABB/OBB devient opt-in `NK_GIZMO_OBB=1`) ; le contour **suit** l'objet en transform ; **orbite caméra autour du centroïde de la sélection** (sans saut, `OrbitAroundPivot`) ; **fix drift** (delta souris recalculé par frame).
- Commits de la session : `c4a8053e` (Tier 0 + 20 headers + gizmo/grille + drag), `18ba0580` (contour sélection), `01f18539` (contour-suit-transform + orbite sélection + drift).

## Dette connue / à traiter (pour le successeur)

1. **⭐ Bloqueur structurel n°1 — pont matériaux / NkSL.** `materialHandle` reste à 0 partout dans Noge ; NkSL a « zéro `#include` » réel dans Noge (dépendance jenga transitive seulement). Ça bloque tout draw call avec un vrai matériau, le deferred v2 (existe côté NKRenderer, jamais exposé côté Noge) et le futur graphe de matériaux (NKGraph). **PV3DE Phase R3 (rendu réel) en dépend directement.** C'est la suite la plus structurante.
2. **STL résiduelle** dans 5 en-têtes spec (règle zéro-STL à faire respecter) : `ECS/Scripting/Python/NkScriptPython.h`, `ECS/Scripting/CSharp/NkScriptCSharp.h`, `ECS/VisualScript/NkBlueprint.h`, `ECS/VisualScript/NkBlueprintHotReload.h`, `Facial/NkFacialRig.h`. Ils compilent mais gardent du `std::` (vector/function/unordered_map/chrono). Migrer vers NKContainers/NKMemory/NKTime (exception tolérée : frontière FFI stricte Python/Mono).
3. **NKEvent — delta souris périmé (cause racine).** `NkEventState.mouse.deltaX/deltaY` n'est pas remis à 0 par frame (seulement sur reset complet) → `NkInput.MouseDeltaX/Y()` garde sa dernière valeur non nulle quand la souris s'arrête. Contourné par consommateur (gizmos + caméra utilisent un `frameMDX = pos - posPrécédente`). Un **fix systémique dans NKEvent** (reset du delta en fin de frame) serait plus propre et bénéficierait à tous les consommateurs.
4. **NkAABB dupliqué au niveau Kernel** : `renderer::NkAABB` (NKRenderer) et `collision::NkAABB3D` (NKCollision), aucun dans NKMath. Le remonter en source unique dans NKMath bénéficierait à Noge (Viewport, etc.).
5. **Contour de sélection — limites** : masque sans depth-test (style X-ray, pas d'occlusion) ; mono-sélection câblée dans la démo (l'API `SubmitSelection` supporte le multi-objets) ; testé OpenGL uniquement (flip UV VK/DX en place mais non re-testé).
6. **Doublons Tier 1/2** (issus de l'analyse de duplication Noge vs dépendances, non bloquants, « mix du meilleur ») : 2 composants caméra ECS (`ecs::NkCamera` vs `ecs::NkCameraComponent`) ; 2 jeux de composants audio (`Audio/NkAudio.h` vs `NkAudioComponents.h`) ; `ECS/Prefab/NkPrefab.cpp` (JSON + IO fichier faits main → déléguer à NKSerialization + NKFileSystem, modèle = `NkSceneSerializer.cpp` voisin) ; `Modeling/NkMeshModifier`/`NkModifierStack` à fusionner avec la pile sérialisable de `renderer::NkEditMesh` ; `Viewport/NkViewportCamera` à faire déléguer les maths orbit/fly aux contrôleurs NKRenderer (garder ortho/multi-viewport) ; `Modeling/NkProceduralMesh` à partager les générateurs avec `NkMeshSystem` ; `NkSceneSerializer` à faire reposer sur `NKECS::NkEntitySerialization` (niveau composant) ; ragdoll : implémenter `NkRagdollSystem` **via** `physics::NkRagdoll::Build()` ; `NkRay` du Viewport → `collision::NkRay3D`.

## Réutilisation par d'autres applications (NkAnima, PV3DE, Nogee…)

**Les briques éditeur sont dans NKRenderer, pas dans la démo — donc partagées et réutilisables sans duplication :**
- Gizmo : `Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkGizmo.h` (`renderer::NkGizmo3D`).
- Caméra orbite (dont `OrbitAroundPivot`) : `.../Core/NkCameraController.h` (`renderer::NkOrbitCameraController3D`).
- Contour de sélection + grille : `.../Tools/Render3D/NkRender3D.{h,cpp}` (`SetSelectionOutline`, `SubmitSelection`, `SetInfiniteGridEnabled`) + shaders `Resources/NKRenderer/Shaders/{InfiniteGrid,SelMask,SelOutline}`.

Toute app qui rend en 3D via `NkRender3D` peut appeler la MÊME API. Consommateurs actuels de `NkRender3D` : `Applications/{Sandbox, NkAnimaEditor, DemoRW}` (+ PV3DE via Noge). Donc NkAnima et PV3DE sont déjà « branchables » sur ces fonctionnalités.

**Ce qui reste à faire par app = le CÂBLAGE par frame (la « colle »), pas les fonctionnalités** : nourrir le gizmo avec l'input souris, suivre la sélection, soumettre les sélectionnés au contour (`SubmitSelection`), passer le delta souris (recalculé par frame) à la caméra, appeler `gizmo.Draw(drawLine, drawTri)`. Ce pattern existe en clair dans `Applications/Sandbox/src/Demo/Demo3D.cpp` (référence à recopier) et partiellement dans `Applications/Nogee/src/Nogee/Layers/UILayer.h`.

**➡️ Prochaine étape recommandée pour un « drop-in » réel** : packager ce câblage dans un **composant « viewport éditeur » réutilisable** (dans `NKEditorKit` ou `Engine/Noge`), pour que NkAnima/PV3DE/Nogee obtiennent gizmo + grille + contour de sélection + orbite-autour-sélection en quelques lignes au lieu de recopier la colle. C'est aligné avec la vision « simplicité d'utilisation ».

## Direction d'architecture à développer (viewports éditeur + caméras gameplay multiples)

Décision de cap (Rihen, 2026-07-26) — à développer plus tard, documenté ici :

1. **Câbler un composant « viewport éditeur » réutilisable dans NKEditorKit** (2D et 3D). Il encapsule le câblage par frame décrit ci-dessus (caméra outil `NkOrbitCameraController3D` + `NkGizmo3D` + grille + contour de sélection + input souris) pour que Nogee / NkAnima / NKCode instancient un viewport éditeur en quelques lignes. Prévoir le **multi-vues** (quad-view façon Blender).
2. **Distinguer clairement caméra ÉDITEUR vs caméra GAMEPLAY** (pendant de la distinction UI éditeur vs UI de jeu) :
   - Caméra éditeur = outil de navigation/édition, dans NKEditorKit, **jamais shippée**.
   - Caméra gameplay = **composant ECS** (`ecs::NkCameraComponent`), donnée de scène sérialisée, **plusieurs par scène**.
3. **Supporter plusieurs caméras / viewports en gameplay** → minimap, caméra de surveillance (« voir ce que voit une caméra à tel endroit »), split-screen, écran vidéo in-world, picture-in-picture.

**Fondations déjà présentes (le chantier n'est pas from-scratch) :** `NkOffscreenTarget`/`NkRenderTarget` (offscreen RT), et `NkPlanarReflectionSystem` qui **rend déjà la scène depuis un autre point de vue vers une texture** = exactement la brique « ce que voit la caméra C → texture » (minimap = rendre depuis C vers une texture → afficher en UI ou sur un matériau d'écran).

**À faire dans ce chantier :**
- Consolider le **doublon interne de composant caméra ECS** : `ecs::NkCamera` (`ECS/Components/Rendering/NkCamera.h`) ET `ecs::NkCameraComponent` (`.../NkRenderComponents.h`) coexistent → garder un seul canonique (fusionner le meilleur des deux).
- Ajouter une **API propre « rendre la scène depuis caméra C vers cible T »** dans `NkRender3D` (au-dessus de l'offscreen existant) — brique du multi-viewport.
- Un **système de composition de viewports** gameplay : liste (caméra, cible = rect écran ou texture, priorité/active), le renderer rend chaque vue active.
- Perf : plusieurs caméras = plusieurs rendus de scène/frame → prévoir résolution réduite / rafraîchissement à N frames pour minimaps/surveillance.

## Mode édition de maillage façon Blender — état et reste à faire (2026-07-28)

Tout vit dans **NKRenderer** (`Mesh/NkEditMesh.*`, `Core/NkGizmo.h`, `Tools/Render3D/NkRender3D.*`) ; le câblage de démo est dans `Applications/Sandbox/src/Demo/Demo3D.cpp` (cible `renderdemo`, `--demo=2`).

**Livré et vérifié en capture** : wireframe **n-gon** en édition (un quad = 4 arêtes, plus de diagonale de triangulation) ; visualisation Blender (arêtes noires + petits points noirs ; sélection orange, sommet actif blanc, **faces sélectionnées en remplissage orange translucide**) ; **arêtes à couleur interpolée par sommet** (effet « semi-sélectionné ») ; **flushing** de sélection (sommets = source de vérité → arêtes → faces) ; gizmo **solide** identique en objet et en édition, orientations **Global/Local/Normal** (touche `,`), repère Normal aligné sur l'élément ; **extrude** faces/arêtes/sommets avec comportement Blender (offset 0, sélectionne, l'utilisateur transforme) ; **subdivide** ; **soudure topologique** à l'entrée en édition (cube 24→12 arêtes, twins corrects, normales par coin donc pas de lissage) qui débloque tout parcours traversant les faces ; **loop cut** qui fait le tour, avec nombre de coupes ; **4 outils de sélection** (Alt+clic boucle, `B` rectangle, `Ctrl+glisser` lasso, `C` cercle, avec Shift=ajouter / Ctrl=retirer) ; **overlay respectant la profondeur** piloté par le X-ray existant (`editXray`, Alt+Z), le gizmo restant toujours visible.

**Bugs connus à corriger** :
- **Cage d'édition détachée du maillage** sur un objet à transformation non uniforme + translation (visible sur la colonne, objet #18) : la cage flotte sans surface dedans. Pistes : incohérence d'espace entre la matrice du drawcall et celle de la cage/marqueurs ; ou le « lift » anti-z-fighting (`rad * 0.0035f`) calculé en local puis déformé par une échelle non uniforme.
- **Boucle Alt+clic incomplète** : sur un cube (coins de valence 3) la boucle dérive (7 arêtes au lieu des 4 de l'anneau). Règles de valence de Blender à appliquer (arrêt aux pôles), pour cube ET grille de quads.
- **Wireframe hors édition encore triangulé** : le mode d'affichage wireframe passe par le rasteriseur, qui ne connaît que les triangles. Le correctif propre demande un **batch GPU persistant d'arêtes n-gon par primitive + rendu instancié** (mesuré : ~2048 arêtes par sphère × 83 objets = >100 000 lignes/frame si on passe par le chemin immédiat `DrawDebugLine`, non viable). Limite intrinsèque : un mesh purement triangulé n'a pas de diagonales à cacher (vrai aussi dans Blender).
- **Scale** : calculé sur le delta brut de souris ; Blender utilise un **ratio de distances écran au pivot** (s'éloigner agrandit, traverser le centre inverse). Vérifier aussi que la **poignée centrale** reste pickable après le resserrement du pick.

**Fonctionnalités demandées, pas encore faites** (backlog priorisé) :
1. Ombrage **Flat / Smooth**.
2. **Points de pivot** façon Blender : Median Point, Individual Origins, 3D Cursor, Active Element, Bounding Box Center.
3. **Bevel** sommet et arête ; **Inset Faces** ; **Edge Split** ; **Spin**.
4. **Variantes d'extrude** : Region, Manifold, Along Normals, Individual, To Cursor.
5. **Proportional editing** (influence dégressive avec courbes).
6. **Symétrie de maillage** sur 1..3 axes.
7. **Snapping d'éléments** (vertex / edge / face / centres / volume) avec repère visuel — aujourd'hui seul un snap de grille existe (`Ctrl`).
8. **Séparer (P) / Joindre (Ctrl+J) / Merge (M) / Rip (V)** — la soudure doit rester un choix utilisateur, flux Blender « on commence en édition, on finit en objet ».
9. **Loop cut interactif** : aperçu au survol montrant où la coupe passera, molette = nombre de coupes, clic confirme, Échap annule.
10. **Knife** (couteau le long d'un tracé) — un bisect par plan existe déjà (`K`).

**Cap applicatif décidé** : une fois cette base saine, **extraire le câblage viewport dans NKEditorKit** (composant réutilisable avec une **table de raccourcis** configurable, pas de raccourcis en dur), puis créer l'application **NK3DModeler** par-dessus, avec GUI et raccourcis identiques à Blender. NkAnima, PV3DE et Nogee héritent du même socle — pas de duplication.

## Comment travailler / vérifier

- **Vérifier qu'un en-tête compile isolément** (utile pour les spec-headers jamais buildés) : extraire les flags d'un .cpp Noge depuis `.nkcode/compile_commands.json` (34 `-I`, 8 `-D` ; convertir les `\` en `/` pour un fichier-réponse clang), puis `clang++ -std=c++17 @rsp -fsyntax-only -x c++ <header>`. Racine d'include Noge = `Engine/Noge/src` → chemins `Noge/Sousdossier/Fichier.h`.
- **Captures renderdemo** (backend GL) pour juger le visuel : `--demo=2` (sinon démarre sur une autre démo) ; env : `NK_FIX_CAM=1 NK_CAM_DIST=<d> NK_CAPTURE=<frame> NK_MAXFRAMES=<n> NK_CAPTURE_PATH=Captures/x.png` ; sélection headless : `NK_GIZMO_SHOW=<0|1|2>` (déclenche `gizmo.Select`) + `NK_GIZMO_OBJ=<idx>` (14 = une sphère) ; `NK_GIZMO_OBB=1` (réactive l'AABB), `NK_OUTLINE_ONLY=1` (masque le gizmo, contour seul), `NK_OUTLINE_THICK=<px>`, `NK_SEL_TEST_XFORM=1` (applique une transform figée à l'objet sélectionné, pour tester que le contour suit), `NK_GRID_CLEAN=1` (grille seule, sans damier ni axes debug épais).

## Pointeurs

- Roadmaps : `Engine/Noge/ROADMAP.md` (par piliers/phases), `Applications/PV3DE/ROADMAP.md` (R0 fait = migration Noge + compile ; R3 = rendu réel, dépend du pont matériaux).
- Le gizmo/grille/contour vivent dans **NKRenderer** (`Core/NkGizmo.h`, `Core/NkCameraController.h`, `Tools/Render3D/NkRender3D.*`, `Resources/NKRenderer/Shaders/{InfiniteGrid,SelMask,SelOutline}`), pas dans Noge : Noge/Nogee **réutilisent** ces briques (ne pas dupliquer).
