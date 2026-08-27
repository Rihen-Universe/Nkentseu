# Ce qui rendrait NKRenderer et NKRHI plus faciles à faire évoluer

> Couche **Runtime** · Jugement demandé explicitement par Rodolf, à l'issue du relevé du
> 24 août 2026. **Pas une refonte** : les quatre changements dont l'absence coûte le plus cher
> aujourd'hui, chacun adossé à un défaut **mesuré**, pas à un principe de conception.

Contexte : ces deux modules ne souffrent pas d'un manque de fonctionnalités. Ils souffrent d'une
chose précise — **rien ne distingue, à l'usage, une API qui marche d'une API qui ne fait rien**. Les
quatre points ci-dessous attaquent tous ce même problème, par ordre de coût constaté.

---

## 1. Rendre impossible l'ordre de frame incorrect

**Le défaut mesuré.** `Present()` exécute le graphe, ferme le command buffer et **soumet** ;
`EndFrame()` ne fait que clore la frame device. Les noms disent l'inverse de ce que fait le code. Sur
la façade, **3 sites** l'ont écrit à l'envers — dont `Engine/Noge/src/Noge/Core/NkApplication.cpp:122-123`,
c'est-à-dire **le framework applicatif du moteur lui-même**, celui que toute application future
recopiera. Ses commentaires enseignent activement l'erreur (« `EndFrame(); // soumet le render
graph` »). Sous Vulkan, cet ordre fait que **rien n'est jamais soumis ni présenté**
(`NkVulkanDevice.cpp:2273` retourne immédiatement car `EndFrame` a posé `mFrameAcquired = false`), et
la swapchain est recréée à chaque frame — **sans une seule erreur pilote**. Sur DX11/GL, ça marche
par accident, ce qui a laissé le défaut vivre.

**Ce qui coûterait le moins cher.** Un état de frame explicite dans `NkRendererImpl`
(`Idle → Recording → Submitted`), avec :
- `EndFrame()` qui **journalise une erreur** si l'état n'est pas `Submitted` (au lieu de fermer une
  frame non soumise) ;
- `Present()` qui journalise si l'état n'est pas `Recording`.

Ce n'est pas une garde théorique : elle aurait transformé un écran noir Vulkan silencieux en une
ligne de log explicite. **Alternative plus radicale et plus sûre** : fusionner les deux en un unique
`EndFrame()` qui fait `Present()` puis la clôture device — c'est exactement le contrat que
`NkEditorRHIRenderer::EndFrame` applique déjà (flux B), et il n'a jamais produit ce défaut.

> ⚠️ Le renommage seul (`Present` → `SubmitAndPresent`) aiderait la lecture mais ne protégerait pas :
> ce sont les 3 sites existants qu'il faut empêcher de se reproduire, pas les futurs lecteurs qu'il
> faut mieux informer.

---

## 2. Supprimer, ou marquer, ce qui est déclaré et non honoré

**Le défaut mesuré.** Le relevé a recensé, dans les deux modules :

- **NKRenderer** — 3 accesseurs de sous-système à 0 site d'appel (`GetShaders`, `GetAnimation`,
  `GetSimulation`) ; `SetVSync` qui écrit un champ que rien ne lit ; **11 champs de
  `NkRendererConfig` sur 22 jamais lus** ; 6 drapeaux `NkSubsystemFlags` inertes dont
  `NK_SS_OFFSCREEN`, **activé par 6 sites applicatifs et testé par aucun** ;
  **11 champs de `NkRendererStats` sur 16 jamais écrits**.
- **NKRHI** — `NkISwapchain` : **17 méthodes, 0 implémentation, 0 classe dérivée** ;
  `SetDebugName` ×3 : 3 références dans tout le dépôt, les 3 déclarations ; les 4 méthodes de
  timestamp : 0 override ; toute l'API bindless : 0 override.

Le coût n'est pas l'encombrement. Il est que **chaque ligne placebo se recopie**.
`cfg.Enable(NK_SS_OFFSCREEN)` a été écrit **6 fois** par des développeurs qui croyaient
raisonnablement activer quelque chose.

**Ce qui coûterait le moins cher.** Deux gestes, dans cet ordre :
1. **Supprimer** ce qui n'a ni implémentation ni appelant — `NkISwapchain`, `NK_SS_RAYTRACING`,
   `NK_SS_GPU_CULLING`, les champs morts de `NkRendererConfig`. Ce qui n'existe pas ne recrute
   personne.
2. Pour ce qui doit **rester** en attendant son implémentation (bindless, timestamps), un marqueur
   uniforme et **cherchable** — un `// [NON IMPLEMENTE]` en tête de déclaration, plus un `logger.Warnf`
   **une seule fois par processus** dans le corps par défaut. Le précédent existe déjà dans le
   dépôt : `NkICommandBuffer::ClearTexture` (`NkICommandBuffer.cpp:64`) journalise « NON IMPLEMENTE ».
   C'est le bon patron ; il n'est simplement appliqué qu'à 2 méthodes sur ~41.

---

## 3. Ne jamais afficher un chiffre qui n'est pas mesuré

**Le défaut mesuré.** `NkRendererStats::gpuTimeMs` n'est **jamais écrit** — le commentaire de
`NkRendererImpl.cpp:1513-1515` l'assume. Mais `NkOverlayRenderer.cpp:50` l'**imprime** :
`"GPU:%.2fms"`. Résultat : **15 applications affichent en permanence `GPU: 0.00 ms`** à l'écran, et
ce zéro a l'air d'une mesure. La cause racine est un étage plus bas : **aucun des six backends NKRHI
ne sait produire un timestamp GPU** — et pourtant `NkDeviceCaps::timestampQueries` est mis à `true`
par **quatre** d'entre eux (OpenGL, DX12, Metal, Vulkan). Un appelant qui teste la capacité s'entend
répondre « oui », puis reçoit `false` de `GetTimestampResults` sur les six.

C'est le même motif que celui déjà relevé sur les matériaux (une trentaine de setters de surface sans
effet en PBR) et sur les compteurs `Draw/Tris/Batches`, qui ont été affichés des mois **avant** d'être
alimentés.

> 📌 **Décision de Rodolf (24/08/2026) : le chronométrage GPU sera implémenté, pas abandonné.**
> Ce relevé est l'argument qui l'a motivée — **quinze applications affichent un chiffre faux à un
> utilisateur**. La suite ci-dessous décrit donc un *chemin vers l'implémentation*, pas un
> renoncement.

**Ce qui coûterait le moins cher, dans l'ordre :**
1. **Immédiat, quelques lignes** : `NkDeviceCaps::timestampQueries` doit passer à `false` sur les
   quatre backends qui le posent à `true` — **tant que la mesure n'existe pas**. Une capacité qui
   ment est pire qu'une capacité absente : elle empêche justement d'écrire le code qui teste la
   capacité avant de s'en servir.
2. **Affichage** : l'overlay n'imprime une valeur que si elle a été écrite (drapeau de validité, ou
   `--` à défaut). Un cadran vide se comprend ; un cadran bloqué sur zéro se diagnostique pendant
   des heures.
3. **Implémentation** : les 4 méthodes device (`NkIDevice.h:355-365`) n'ont **aucun override**, et
   `WriteTimestamp` n'en a qu'un (OpenGL) qui ne peut rien produire — il passe l'index de l'appelant
   comme nom d'objet de requête GL sans qu'aucun `glGenQueries` ne l'ait créé, et **aucun code
   n'appelle `glGetQueryObject*`**. Le point de départ le moins coûteux est Vulkan
   (`VkQueryPool` + `vkCmdWriteTimestamp`, et `VkPhysicalDeviceLimits::timestampPeriod` donne déjà
   la conversion en nanosecondes) ; DX12 suit avec `ID3D12QueryHeap`. Une fois **un seul** backend
   capable de rendre un résultat, `gpuTimeMs` cesse d'être un mensonge sur ce backend, et la
   capacité peut repasser à `true` **pour lui seul**.

---

## 4. Lier ce qui est déjà lié — le cas `samples`

**Le défaut mesuré.** `NkGraphicsPipelineDesc` (`NkDescs.h:684-685`) porte **côte à côte** deux
champs qui ne sont pas indépendants : `samples` et `renderPass`. Le nombre d'échantillons d'un
pipeline **est une fonction de sa passe** — mais rien dans le code ne l'impose : Vulkan
(`NkVulkanDevice.cpp:1735`), DX12 (`NkDirectX12Device.cpp:1772`) et Metal (`NkMetalDevice.mm:736`)
lisent tous `d.samples` **directement depuis le desc**, et GL, DX11 et Software l'**ignorent
complètement**. Ce qui *est* correctement dérivé de la passe, ce sont les formats RTV/DSV
(`RenderPassFormats()`, `NkDirectX12Device.cpp:1640`) — la mécanique existe donc déjà, elle n'a
simplement pas été étendue au compte d'échantillons.

⚠️ **Correction d'une croyance répandue** : il circule que « Vulkan et DX12 dérivent désormais
`samples` de leur render pass ». **Mesuré trois fois indépendamment sur cet arbre : c'est faux.**
Seuls les *formats* le sont.

**Pourquoi ça ne casse pas encore** : `grep -rn "\.samples[ ]*="` donne 18 lignes, dont **une seule**
touche un `NkGraphicsPipelineDesc` — `Tools/Grid3D/NkGrid3D.cpp:114`, qui pose la valeur **par
défaut**. Le champ vaut `NK_S1` partout. La divergence est **latente** : elle se réveillera au
premier pipeline MSAA, sous forme de pipeline invalide côté Vulkan et DX12.

**Ce qui coûterait le moins cher.** Retirer `samples` de `NkGraphicsPipelineDesc` et le **dériver**
de `renderPass` dans chaque backend, à côté d'où les formats le sont déjà. Tant qu'un champ peut
contredire un autre champ de la même structure, quelqu'un finira par écrire la contradiction —
et il le fera le jour où le MSAA sera activé, c'est-à-dire au pire moment.

---

## 1bis. Une ligne mécanique qui rendrait le mauvais ordre impossible ou bruyant

Demande explicite : *existe-t-il une garde mécanique ?* Aujourd'hui l'inversion donne — quand Vulkan
fonctionnera — un écran noir **sans une seule erreur pilote**. Quatre candidats, chiffrés.

> ⚠️ **Aucune de ces gardes n'a été « faite rougir ».** Je ne les ai pas construites (hors périmètre :
> `NkRendererImpl` appartient à un autre chantier) et je n'en ai donc vu aucune passer au rouge.
> Ce qui suit est un **devis**, pas un résultat. La colonne « comment la faire rougir » dit
> exactement quelle expérience validerait chacune — c'est ce travail-là qui reste à faire, et il
> doit précéder l'adoption.

| # | Garde | Coût | Ce qu'elle attrape | Comment la faire rougir |
|---|---|---|---|---|
| **G1** | **Machine à états de frame** : un membre `enum { Idle, Recording, Submitted }` dans `NkRendererImpl` ; `Present()` exige `Recording`, `EndFrame()` exige `Submitted`, sinon `logger.Errorf` | ~15 lignes, 1 fichier (`NkRendererImpl.{h,cpp}`). L'infrastructure existe déjà : la classe porte `mInitialized` (`:202`) et `mFrameCounter` (`:200`) | **Les 3 sites** de ce chantier, à la première frame, avec un message nommant le fichier | Remettre l'ordre inversé sur un banc, lancer, **voir la ligne d'erreur** — puis rétablir. ~10 min sur `r2d01` sous OpenGL |
| **G2** | **Fusionner** : `EndFrame()` fait `Present()` puis la clôture device ; `Present()` devient privé ou obsolète | ~44 sites d'appel corrects à toucher + 3 inversés | **La classe entière de défauts** — l'ordre cesse d'exister comme choix | Vérifier qu'aucun des 44 ne dépend d'un travail entre `Present` et `EndFrame`. **Non vérifié** — c'est le préalable |
| **G3** | **Renommer** `Present()` → `SubmitAndPresent()` | mécanique, 47 sites | Rien à l'exécution — **aide la lecture, ne protège pas** | Sans objet : ne peut pas rougir. À ne pas compter comme une garde |
| **G4** | **Garde côté RHI** : `NkVulkanDevice::EndFrame` journalise quand il prend le chemin « frame abandonnée » (`:2402`) au lieu de recréer la swapchain en silence | ~3 lignes, 1 fichier — **mais `NKRHI` appartient au chantier backends** | Toute frame non soumise, **quelle qu'en soit la cause** — donc aussi les vrais abandons | Provoquer un abandon de frame (fenêtre réduite) et **voir la ligne**. Attention : si les vrais abandons sont fréquents, la garde devient du bruit — à mesurer avant |

**Recommandation : G1 d'abord.** C'est la seule qui soit à la fois bon marché, locale à un fichier,
et qui transforme un écran noir silencieux en une ligne nommant le fautif. G4 la complète utilement
mais appartient à un autre chantier. G2 est la vraie solution structurelle — l'ordre ne peut plus
être faux s'il n'y a plus d'ordre à choisir — mais elle demande d'abord l'audit des 44 sites
corrects, qui n'est pas fait.

⚠️ **Et une remarque qui vaut pour les quatre** : la garde n'aurait **rien changé cette semaine**. Sur
`main`, Vulkan et DX12 ne démarrent pas (§ [Frame-Contract.md 4bis](Frame-Contract.md)), et sous
OpenGL l'inversion est inoffensive. Une garde d'exécution ne parle que sur un chemin qui s'exécute :
**c'est la relecture qui a trouvé ces 3 sites, pas un symptôme.** G1 protège l'avenir, elle n'aurait
pas révélé le passé.

---

## Le fil commun

Les quatre points sont le même défaut vu sous quatre angles : **le code ne dit pas ce qu'il ne fait
pas**. Un ordre d'appel faux ne lève rien ; une capacité absente répond `true` ; un compteur non
alimenté s'affiche quand même ; deux champs qui doivent s'accorder sont laissés libres.

Aucun des quatre ne demande une refonte. Trois d'entre eux — la capacité `timestampQueries`, les
déclarations mortes, l'affichage de `gpuTimeMs` — sont des corrections de **quelques lignes** dont le
seul mérite est d'arrêter de mentir à l'appelant. C'est précisément ce qui rend un module reprenable
par quelqu'un d'autre.

> ⚠️ **Ce que ce jugement ne couvre pas.** Je n'ai pas compilé ni exécuté quoi que ce soit : tout
> vient de la lecture du code et de décomptes de sites. Je n'ai pas audité les setters de
> `NkMaterial` (un relevé existant fait référence), ni les shaders `.nksl`, ni les fichiers `.jenga`
> au-delà du glob de build d'OpenGL. Le comportement au **runtime** des trois sites d'ordre inversé
> est **déduit** de la lecture des backends, pas observé sur machine — mais la chaîne
> `mFrameAcquired`/`mFrameSubmitted` est courte et sans branche cachée.

[← Contrat de frame](Frame-Contract.md) · [← Surface publique](API-Surface.md) · [← Divergence NKRHI](../NKRHI/Backend-Divergence.md)
