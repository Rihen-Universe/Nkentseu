# `NkIEditorRenderer` doit-il avoir son propre système de rendu ?

> Couche **Runtime** · NKRenderer ↔ **Engine/NKEditorKit** · Question posée par Rodolf.
> Réponse mesurée le **27/08/2026** sur `main`.

## Verdict — **oui, nécessaire**, et pour une raison qui n'est pas celle qu'on croit

**Ce n'est pas une question de méthodes en double. C'est le sens de la dépendance.**

`NkIEditorRenderer` abstrait **deux piles graphiques disjointes** ; `NkRenderer` n'en couvre qu'une,
et ne peut pas couvrir l'autre sans casser la règle de couches du dépôt.

> 🔴 **Mais la vraie conclusion de cette page est ailleurs, et elle vaut d'être lue en premier :**
> **la frontière à durcir n'est pas la fusion — c'est la collision de vocabulaire.**
>
> Les deux interfaces ont un `BeginFrame`/`EndFrame` **aux sémantiques opposées** : celui de
> l'éditeur **soumet et présente** dans `EndFrame()`, celui de la façade **non**
> ([Frame-Contract.md](Frame-Contract.md)). Personne ne risque de fondre les deux classes par
> accident ; en revanche **tout le monde risque de lire l'une pour l'autre** — c'est déjà arrivé :
> `NK3DModeler` a été lu comme « un troisième motif de `NkRenderer` » alors que c'est une autre
> classe, et la conclusion qu'on en tirait aurait fait « corriger » du code correct.
>
> Le coût du remède est connu et petit — renommer les points d'entrée de l'un des deux
> (§ 5.1) : **2 implémentations, 2 sites d'injection, 1 usage direct.** Le coût de ne rien faire est
> une relecture erronée de plus, à chaque fois qu'un nouvel arrivant ouvre `NK3DModeler/main.cpp`.

---

## 1. La raison décisive : deux implémentations, deux piles sans intersection

| Implémentation | Déclaration | Pile réelle (vérifiée par ses `#include` et ses membres) |
|---|---|---|
| `editorkit::NkEditorCanvasRenderer` | `Engine/NKEditorKit/src/NKEditorKit/NkEditorCanvasRenderer.h:24` | **NKCanvas** |
| `nkgui::NkEditorRHIRenderer` | `Integrations/NKGui/NkEditorRHIRenderer.h:47` | **NKRHI** (directement — **pas** NKRenderer) |

Mesuré : `NKCanvas.jenga:65` déclare
`["NKWindow", "NKFont", "NKImage", "NKStream", "NKTime", "NKGlad", "NKThreading"]` —
**NKCanvas ne dépend ni de NKRHI ni de NKRenderer.** Il porte ses propres contextes
GL/VK/DX11/DX12/Metal/Software sous `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/`.

⚠️ **Piège de lecture** : `NkEditorCanvasRenderer` manipule des types du namespace
`nkentseu::renderer` (`NkRenderWindow`, `NkGuiCanvasBackend`) — mais ce namespace est celui de
**NKCanvas**, pas de NKRenderer (`NkRenderWindow.h:45` ouvre `namespace renderer`). Le nom du
namespace ment sur le module.

Et `NkEditorRHIRenderer` **n'inclut jamais `NKRenderer/NkRenderer.h`** : son en-tête dit
« NKRHI/NKRenderer » (`:11`) mais c'est de la prose ; mesuré, il est assis sur NKRHI seul.

### Ces deux piles sont vivantes toutes les deux, aujourd'hui

| Application | Implémentation |
|---|---|
| NKCode (l'IDE), NKUIDesign, ConquerorLab, NKEditorKitDemo | **NKCanvas** (chemin par défaut, `NkEditorShell.cpp:143`) |
| NkAnimaEditor, Nogee, NK3DModeler | **NKRHI** |

**4 applications tournent sur le chemin NKCanvas.** Fondre `NkIEditorRenderer` dans `NkRenderer`
(qui prend un `NkIDevice*`, `NkRenderer.h:55` — objet qui **n'existe pas** dans NKCanvas)
supprimerait ce chemin et forcerait ces 4 applications sur NKRHI.

---

## 2. Le sens de la dépendance — mesuré sur les `.jenga` et les `#include`, pas sur les intentions

### Ce que les `.jenga` déclarent

- **`Engine/NKEditorKit/NKEditorKit.jenga:25-31`** : `["NKPlatform", "NKCore", …, "NKCanvas", "NKFont", "NKImage", "NKGui"]` — **ni `NKRenderer` ni `NKRHI`**. Sa propre docstring (`:12`) : *« 2D pur (NKCanvas/NKUI) : AUCUNE dependance NKRenderer (3D) »*.
- **`Kernel/Runtime/NKRenderer/NKRenderer.jenga:31-34`** : **ni `NKEditorKit`, ni `NKGui`, ni `NKCanvas`**.
- **`Integrations/NKGui/NKGuiIntegration.jenga:37-45`** : **ni `NKEditorKit` ni `NKRenderer`**.

### Ce que les `#include` disent (comptés)

| Direction | Compte |
|---|---|
| `Engine/NKEditorKit/` → `NKRenderer/` | **0** |
| `Engine/NKEditorKit/` → `NKRHI/` | **0** |
| `Kernel/Runtime/NKRenderer/` → `NKEditorKit/` | **0** |
| `Kernel/Runtime/NKRenderer/` → `NKGui/` | **0** |
| `Kernel/Runtime/NKRenderer/` → `NKCanvas/` | **0** |
| Mentions `editorkit` dans `Kernel/Runtime/NKRenderer/` | **2**, toutes deux **du markdown** — dont `FINISH_PLAN.md:4` qui dit *« **Ne pas modifier** : Engine/NKEditorKit »* |

**Le graphe est mesurablement acyclique, et propre dans les deux sens.**

⚠️ Une seule arête existe, et elle est **volontairement hors build** : `NkEditorRHIRenderer.h:33`
inclut `NKEditorKit/NkIEditorRenderer.h`, mais c'est un fichier **header-only qu'aucun `.cpp` de
`NKGuiIntegration` ne compile** — la lib elle-même ne dépend pas de NKEditorKit, seul le
*consommateur* doit le faire. C'est écrit noir sur blanc dans `NKGuiIntegration.jenga:15-17`.

### Pourquoi la fusion est interdite

| Module | Chemin | Couche |
|---|---|---|
| `NKRenderer`, `NKCanvas`, `NKRHI` | `Kernel/Runtime/` | Kernel / Runtime |
| `NKEditorKit` | `Engine/` | Engine |
| `NKGuiIntegration` | `Integrations/` | glue |

`ARCHITECTURE.md:53` : *« chaque couche ne connaît que les couches situées en dessous d'elle »*.
Engine peut dépendre de Kernel/Runtime ; **Kernel/Runtime ne peut pas dépendre d'Engine**.

Donc déplacer `NkIEditorRenderer` dans `Kernel/Runtime/NKRenderer` obligerait NKRenderer à dépendre
soit de `Engine/NKEditorKit` (inversion interdite), soit de `NKGui` (dépendance aujourd'hui **à 0**,
car `SubmitDrawList` prend un `nkgui::NkGuiDrawList`). Dans les deux cas on remonte le Kernel dans
l'Engine.

---

## 3. Ce que l'interface éditeur fait et que la façade ne sait pas faire

`NkIEditorRenderer` a **11 virtuelles pures** ; `NkRenderer` en a ~50. L'intersection est **2**
méthodes (`Shutdown`, `IsValid`) plus 3 partielles. Trois méthodes **n'ont aucun équivalent** :

| Méthode éditeur | Équivalent façade | Constat |
|---|---|---|
| `Init(NkWindow&, NkEditorGfxApi)` `:172` | — | 🔴 **Rien.** L'éditeur **crée** device+swapchain (`NkEditorRHIRenderer.h:98` `CreateWithFallback`). `NkRenderer::Create(NkIDevice*, cfg)` **reçoit** un device : mesuré sur **25 sites d'appel**, tous passent un device construit avant. `NkRenderer.h:5` : *« Seul lien avec la plateforme : NkSurfaceDesc »* — la façade ne voit jamais de fenêtre |
| `SubmitDrawList(const nkgui::NkGuiDrawList&, w, h)` `:182` | — | 🔴 **Rien.** `grep SubmitDrawList` / `DrawList` / `nkgui` dans `NKRenderer/src/` → **0, 0, 0**. Le plus proche est `NkRender2D`, une API **immédiate** (`FillRect`, `DrawSprite`), pas une liste **retenue** |
| `UploadFontGray8(texId, …)` `:187` | — | 🔴 **Rien.** Les 17 occurrences de « Gray8 » dans NKRenderer sont toutes des **formats de cible** `NK_R8_UNORM` (SSAO demi-résolution, masque de sélection), aucun chemin d'upload de pixels gris |
| `UploadImageRGBA(texId, …)` `:188` | `NkTextureLibrary::UploadColorTexture` | ⚠️ **Partiel et inaccessible** : la méthode est **privée** (`NkTextureLibrary.h:181`), et l'éditeur indexe par le `uint32 texId` porté par la draw list, alors que la bibliothèque rend des `NkTexHandle` opaques. Aucun pont |

`NkRenderer::SetUIOverlayCallback` (`:142`) n'est pas un substitut : c'est un
`NkFunction<void(NkICommandBuffer*)>` — **un command buffer brut rendu à l'appelant**, dont le
commentaire (`:38-42`) dit qu'il existe *« sans que NKRenderer connaisse NKUI »*. Et il n'est
atteignable que si un render graph tourne — ce qui n'est pas le cas du chemin NKCanvas.

---

## 4. Elles ne se concurrencent pas : elles se **composent**

`NkEditorRHIRenderer::GetDevice()` (`:268`) donne son `NkIDevice` à `NkRenderer::Create` pour les
viewports 3D offscreen — `NkViewport3D.cpp:735`, `AnimBridge.cpp:697` — et `SetPreUI()` (`:280`)
ouvre un créneau dans le command buffer de l'éditeur pour cette passe 3D. C'est la règle
« une fenêtre = une pile » (`NK3DModeler.jenga:63-67`). **C'est exactement le flux C**
([Frame-Contract.md](Frame-Contract.md)).

---

## 5. La frontière à rendre infranchissable

Puisque la réponse est « nécessaire », ce qui compte est que la séparation **ne puisse pas dériver** :

1. 🔴 **Le vrai risque n'est pas la fusion, c'est la collision de vocabulaire.** Les deux interfaces
   ont un `BeginFrame`/`EndFrame` **aux sémantiques opposées** — celui de l'éditeur soumet et
   présente dans `EndFrame`, celui de la façade non ([Frame-Contract.md](Frame-Contract.md)).
   C'est précisément ce qui a fait lire `NK3DModeler` comme un « troisième motif de `NkRenderer` »
   alors que c'est une autre classe. **Renommer les points d'entrée de l'un des deux** (par exemple
   `NkIEditorRenderer::EndFrame` → `EndAndPresentFrame`) supprimerait la confusion à la racine, pour
   le coût d'un renommage sur **2 implémentations et 2 sites d'injection** (`NkAnimaEditor/main.cpp:79`,
   `NogeeShell.cpp:683`) plus l'usage direct de `NK3DModeler/main.cpp:2018`.
2. **Garder la dépendance à 0 et la rendre vérifiable.** Les 5 compteurs du §2 valent 0 aujourd'hui ;
   un contrôle de build qui échoue si l'un devient non nul coûte quelques lignes et fige l'acquis.
3. ⚠️ **Une justification du code est déjà périmée** — à corriger pour éviter qu'elle serve d'argument
   plus tard : `NkIEditorRenderer.h:30-32` justifie l'enum neutre `NkEditorGfxApi` en affirmant que
   NKCanvas et NKRHI définiraient chacun leur `NkGraphicsApi`, *« qui se dupliquent dans le namespace
   nkentseu et ne peuvent cohabiter dans un meme TU »*. **Mesuré : il n'existe qu'UNE définition**
   (`graphics::NkGraphicsApi`, `NKPlatform/NkCGXDetect.h:246`) ; NKCanvas ne fait que l'aliaser
   (`NkRenderer2DFactory.h:27`) et NKRHI n'en déclare aucune concurrente. L'argument de duplication
   est **mort**. Ce qui justifie encore `NkEditorGfxApi`, ce sont le vocabulaire CLI unifié
   (`NkEditorGfxApiName` `:59`) et le **contrat de refus par plateforme**
   (`NkEditorGfxApiSupported` `:133`) — qui n'ont aucun équivalent dans NKRenderer.

---

## 6. Coût d'une unification, si elle était tout de même voulue

Chiffré, pour que le refus soit un arbitrage et non un réflexe :

| Poste | Coût mesuré |
|---|---|
| Supprimer le chemin NKCanvas | **4 applications** à porter sur NKRHI (NKCode, NKUIDesign, ConquerorLab, NKEditorKitDemo) |
| Faire entrer NKGui dans NKRenderer | dépendance **0 → 1**, pour `SubmitDrawList(nkgui::NkGuiDrawList)` |
| Inverser Kernel↔Engine | interdit par `ARCHITECTURE.md:53` — sinon déplacer NKEditorKit sous `Kernel/Runtime/` |
| Ajouter à la façade | `Init(window, api)`, `SubmitDrawList`, `UploadFontGray8`, + un pont `texId` → `NkTexHandle` |
| Gain | **2 méthodes** cesseraient d'être en double (`Shutdown`, `IsValid`) |

> **Deux méthodes gagnées contre quatre applications portées et une règle de couches enfreinte.**
> La séparation n'est pas un accident historique : c'est le seul agencement qui laisse le graphe
> acyclique.

[← Contrat de frame](Frame-Contract.md) · [← Surface publique](API-Surface.md) · [← Doc NKRenderer](README.md)
