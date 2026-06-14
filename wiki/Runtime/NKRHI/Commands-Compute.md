# Commandes et compute

> Couche **Runtime** · NKRHI · Enregistrer le travail GPU dans un **command buffer**
> (`NkICommandBuffer`), le recycler frame après frame avec un **pool** (`NkCommandPool`),
> et piloter le calcul parallèle de haut niveau — compute générique (`NkComputeContext`,
> `NkComputeBuilder`) et même deep learning sur GPU (`NkMLContext`, `NkTensor`).

Un GPU ne s'appelle pas comme une fonction : on ne lui « dit » pas de dessiner un triangle, on
**écrit une liste d'instructions** qu'on lui remet en bloc. Cette liste s'appelle un *command
buffer*, et tout NKRHI tourne autour d'elle. La raison est physique : le CPU et le GPU sont deux
processeurs séparés qui avancent à leur rythme ; les faire dialoguer instruction par instruction
serait catastrophique (chaque aller-retour coûte une éternité à l'échelle d'une frame). On
**enregistre** donc d'abord toutes les commandes d'une frame (`Begin` → draws, copies, barrières →
`End`), puis on **soumet** le tout d'un coup. Le GPU consomme ensuite la liste à son rythme pendant
que le CPU prépare déjà la frame suivante.

Cette page couvre quatre objets qui s'empilent. Au plus bas, `NkICommandBuffer` est l'interface
d'enregistrement — l'objet central du RHI. Juste au-dessus, `NkCommandPool` recycle ces buffers
pour ne pas en allouer un neuf à chaque frame. Encore au-dessus, `NkComputeContext` (et son API
fluide `NkComputeBuilder`) offre une couche **compute** confortable : cache de pipelines, bindings
paresseux, dispatch 1D/2D/3D, barrières automatiques. Enfin, `NkMLContext` bâtit un moteur de
**deep learning** complet (tenseurs, MatMul, convolution, attention, optimiseurs) entièrement en
GLSL compute, sans CUDA.

Ce n'est **pas** une API immédiate à la OpenGL classique où chaque appel parle au pilote sur-le-champ :
ici tout est **différé**. Et ce n'est **pas** non plus un moteur de rendu — NKRHI ne sait rien des
matériaux ou des lumières ; il fournit la mécanique GPU sur laquelle NKRenderer et NKCanvas
construisent.

- **Namespace** : `nkentseu` (les shaders ML : `nkentseu::NkMLShaders`)
- **Headers** :
  `#include "NKRHI/Commands/NkICommandBuffer.h"` ·
  `#include "NKRHI/Commands/NkCommandPool.h"` (ou `NKRHI/Core/NkCommandPool.h`, **un seul**) ·
  `#include "NKRHI/Core/NkComputeContext.h"` ·
  `#include "NKRHI/Core/NkML.h"`

---

## Le command buffer : `NkICommandBuffer`

C'est **l'objet central** de tout le RHI. On ne l'instancie pas soi-même : c'est le device
(`NkIDevice::CreateCommandBuffer` / `DestroyCommandBuffer`) qui le fabrique. Tout ce que le GPU doit
faire pendant une frame — fixer un viewport, lier un pipeline, dessiner, copier une texture, poser
une barrière de synchronisation — passe par ses méthodes. Et tout cela est **enregistré, pas
exécuté** : les appels remplissent la liste, qui ne part au GPU qu'à la soumission.

Son cycle de vie est strict et toujours le même : `Begin()` ouvre l'enregistrement, on émet les
commandes, `End()` le ferme, on soumet (via le device), on attend la *fence* qui signale que le GPU
a fini, puis `Reset()` remet le buffer à blanc pour le réutiliser. On choisit aussi sa **nature** à
la création — `NkCommandBufferType` — selon le travail visé : `NK_GRAPHICS` (draw + compute + copy,
la file généraliste), `NK_COMPUTE` (compute + copy sur une file dédiée, pour le calcul asynchrone) ou
`NK_TRANSFER` (copies seules, sur le moteur DMA, pour les uploads).

```cpp
NkICommandBuffer* cb = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
cb->Begin();
cb->SetClearColor(0.1f, 0.1f, 0.12f);          // AVANT le render pass
cb->BeginRenderPass(rp, fb, area);
cb->SetViewport(vp);
cb->BindGraphicsPipeline(pipeline);
cb->BindVertexBuffer(0, vbo);
cb->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
cb->DrawIndexed(indexCount);
cb->EndRenderPass();
cb->End();
// ... device->Submit(cb) ; attente fence ; cb->Reset() ;
```

Deux idiomes piègent les nouveaux venus. D'abord, **toutes les méthodes ne sont pas garanties** :
beaucoup sont des virtuelles **non-pures** avec une implémentation par défaut **no-op** (sous-passes,
marqueurs de debug, timestamps, clear dynamique, `DrawIndirectCount`). Selon le backend, elles
agissent ou ne font rien — ne supposez jamais leur effet sans le vérifier. Ensuite, `SetClearColor` /
`SetClearDepth` doivent être appelés **avant** `BeginRenderPass` (ils surchargent les valeurs du
`NkRenderPassDesc`, à la manière de `glClearColor`), et la valeur reste **persistante par frame**
jusqu'au prochain appel. Enfin, `UpdateBuffer` est **différé** : la copie est capturée et s'exécute
dans l'ordre, entre les `Bind`/`Draw` qui l'entourent — c'est exactement ce qu'il faut pour mettre à
jour un UBO *par drawcall* sans que la dernière écriture n'écrase tout.

> **En résumé.** `NkICommandBuffer` enregistre (ne pas exécute) le travail GPU. Cycle strict
> `Begin → … → End → Submit → fence → Reset`. Type choisi à la création (`NK_GRAPHICS` / `NK_COMPUTE`
> / `NK_TRANSFER`). Méfiez-vous des no-op par défaut ; `SetClearColor/Depth` avant le render pass ;
> `UpdateBuffer` est différé et ordonné.

---

## Recycler les buffers : `NkCommandPool`

Allouer un command buffer neuf à chaque frame, c'est un `CreateCommandBuffer`/`DestroyCommandBuffer`
par frame — du gaspillage. Le `NkCommandPool` résout ça en gardant un **stock** de buffers : quand on
en a besoin, on en `Acquire()` un (réutilisé s'il y en a un libre, sinon créé) ; quand le GPU a fini
avec lui, on le `Release()` pour le remettre dans le stock. C'est un *object pool* spécialisé, et il
est **thread-safe** (acquire / release / reset sont protégés par un mutex), donc plusieurs threads
peuvent y puiser pour enregistrer des buffers en parallèle.

Le pool distingue deux ensembles : les buffers **libres** (`FreeCount()`) prêts à être réutilisés, et
ceux **en vol** (`InFlightCount()`) actuellement soumis ou en cours d'enregistrement. `Acquire()`
sort un libre — déjà `Reset()` pour vous — et le bascule en vol ; `Release(cb)` fait l'inverse. La
règle d'or : **ne libérer un buffer qu'après que le GPU a fini** (après l'attente de la fence), sinon
il pourrait être réacquis et réinitialisé alors qu'il est encore consommé par le GPU — corruption
assurée.

```cpp
NkCommandPool pool(device, NkCommandBufferType::NK_GRAPHICS);

// chaque frame :
NkICommandBuffer* cb = pool.Acquire();   // recyclé, déjà Reset
cb->Begin(); /* … */ cb->End();
device->Submit(cb);
device->WaitFence(...);                   // le GPU a fini
pool.Release(cb);                         // SEULEMENT maintenant
```

Le pool ne **possède pas** le device — il n'en garde qu'un pointeur. Il est **non-copiable** mais
**déplaçable**. À l'arrêt, `Reset()` détruit physiquement tous les buffers (libres comme en vol) ;
c'est aussi ce que fait le destructeur. `ReleaseAll()` rapatrie tout le monde en libre d'un coup (à
appeler **après un WaitIdle**, quand toute la file est vidée).

> **En résumé.** `NkCommandPool` recycle les `NkICommandBuffer` pour éviter create/destroy par frame.
> `Acquire()` → enregistrer/soumettre → fence → `Release()`. Thread-safe. Ne jamais `Release` avant la
> fence. `Reset()`/destructeur détruisent les buffers ; `ReleaseAll()` après `WaitIdle`.

---

## Le compute confortable : `NkComputeContext` et `NkComputeBuilder`

Faire du compute « à la main » avec un command buffer marche, mais c'est verbeux : créer le pipeline,
gérer les descriptor sets, poser les bonnes barrières, calculer le nombre de groupes… `NkComputeContext`
est une couche de **confort** au-dessus du RHI bas niveau, pensée pour les particules, la physique
GPU, le post-traitement et — en dessous d'elle — le moteur ML. Elle apporte trois choses : un **cache
de pipelines** (par clé `const char*`, pour ne pas recompiler à chaque appel), des **bindings
paresseux** (on accumule buffers et textures, ils ne sont appliqués qu'au moment du dispatch) et des
**barrières + transitions** prêtes à l'emploi.

Le flux est : `Init(device)` une fois, puis par passe `BeginPass(cmd, desc)` (sur un command buffer
**déjà `Begin()`**), on lie un pipeline et des ressources, on dispatch, on ferme avec `EndPass()`. Les
helpers de dispatch font le calcul du nombre de groupes pour vous : `Dispatch1D(count)` pour un tableau
linéaire, `Dispatch2D(w, h)` pour une image, `Dispatch3D(w, h, d)` pour un volume — chacun arrondit au
supérieur selon une taille de tuile.

```cpp
NkComputeContext compute;
compute.Init(device);

cmd->Begin();
compute.BeginPass(cmd);
NkPipelineHandle p = compute.GetOrCompileGLSL("blur", blurSrc);
compute.SetPipeline(p);
compute.BindStorageTexture(0, srcTex);
compute.BindStorageTexture(1, dstTex);
compute.PushConstants(BlurParams{ radius });
compute.Dispatch2D(width, height);       // groupes calculés tout seuls
compute.EndPass();
cmd->End();
```

Pour ceux qui aiment chaîner, `NkComputeBuilder` est une **façade fluide** : `compute` exposé sous
forme d'appels enchaînés qui se terminent par un dispatch. Attention, le builder ne tient qu'une
**référence** au contexte — il ne doit pas lui survivre.

```cpp
NkComputeBuilder(compute)
    .Pipeline(p)
    .Buffer(0, positions)
    .Buffer(1, velocities)
    .Push(SimParams{ dt })
    .Dispatch1D(particleCount);
```

Le piège récurrent du compute, c'est la **synchronisation entre dispatches dépendants** : si un
dispatch écrit un buffer que le suivant lit, il faut une `UAVBarrier` entre les deux, sinon le GPU peut
lancer le second avant que le premier ait fini d'écrire (course mémoire, corruption silencieuse).
`NkComputeContext` fournit `UAVBarrier`, les transitions `TransitionForCompute`/`TransitionForGraphics`
(passer une texture entre l'état compute et l'état lecture graphique), et `FullBarrier` (globale, mais
coûteuse). Côté async, `SubmitAsync` utilise une **file compute dédiée** sur Vulkan/DX12 (vrai
parallélisme avec le rendu) et retombe sur la file principale (synchrone) sur GL/DX11.

> **En résumé.** `NkComputeContext` = compute haut niveau : cache de pipelines (`GetOrCreatePipeline` /
> `GetOrCompileGLSL`), bindings paresseux (`BindBuffer`/`BindStorageTexture`…), dispatch 1D/2D/3D auto,
> barrières (`UAVBarrier`, transitions). `BeginPass` exige `cmd->Begin()`. **Une `UAVBarrier` entre
> dispatches dépendants**. `NkComputeBuilder` = la même chose en API fluide, ne survit pas au contexte.

---

## Le deep learning sur GPU : `NkMLContext`, `NkTensor`, `NkTensorShape`

Au sommet, `NkMLContext` transforme le RHI en moteur de **réseaux de neurones**, écrit à 100 % en
GLSL compute, **sans CUDA ni cuDNN**. La donnée centrale est le `NkTensor` : un tableau
multidimensionnel résidant en mémoire **GPU** (un `NkBufferHandle` sous le capot), décrit par sa forme
`NkTensorShape`. Le contexte offre tout ce qu'attend un praticien : algèbre linéaire (`MatMul`,
`MatMulAdd`, `BatchMatMul`), convolution (`Conv2D`), une dizaine d'**activations** (ReLU, GELU,
Sigmoid, Softmax, Swish…), des **normalisations** (LayerNorm, BatchNorm, RMSNorm), l'**attention**
des Transformers (`ScaledDotProductAttention`), les fonctions de **perte**, le **backward pass**
(gradients) et des **optimiseurs** (AdamW, SGD).

Comme `NkTensor` porte un buffer GPU, il obéit à la **règle dure NKMemory** : tout ce qui se crée se
détruit. `CreateTensor` ↔ `DestroyTensor`, `CreateAdamState` ↔ `DestroyAdamState` — jamais d'oubli,
sous peine de fuite GPU (ou de corruption). Subtilité : `Reshape` **ne copie pas**, il renvoie un
tenseur qui partage le même buffer avec une autre forme (le nombre d'éléments doit coïncider). Et les
fonctions de perte **téléchargent** un scalaire vers le CPU — donc elles **synchronisent** (coûteux,
à ne pas appeler dans une boucle chaude inutilement).

```cpp
NkMLContext ml;
ml.Init(device);

NkTensor x  = ml.CreateTensor({ batch, inDim });
NkTensor W  = ml.CreateTensor({ inDim, outDim }, /*requiresGrad*/ true);
NkTensor b  = ml.CreateTensor({ outDim }, true);
NkTensor h  = ml.CreateTensor({ batch, outDim });
ml.InitHe(W);  ml.FillZero(b);

ml.MatMulAdd(x, W, b, h);     // h = x·W + b
ml.ReLU(h, h);                // activation en place
ml.Sync();                    // attendre la fin des compute shaders

ml.DestroyTensor(x); ml.DestroyTensor(W); /* … */
```

C'est le **seul** des cinq headers à utiliser un fragment de STL : `std::initializer_list`, pour
écrire les formes en accolades (`{ batch, inDim }`) — exception assumée au zéro-STL. Le sous-namespace
`nkentseu::NkMLShaders` expose en prime les **sources GLSL** des kernels (GEMM tuilé, ReLU, Softmax,
LayerNorm, AdamW…) sous forme de `const char*` prêts à compiler, utiles pour inspecter ou réutiliser le
calcul brut.

> **En résumé.** `NkMLContext` = deep learning GPU 100 % GLSL compute. `NkTensor` (buffer GPU + forme
> `NkTensorShape`) suit **Create/Destroy** obligatoire. Couverture complète : MatMul/Conv2D,
> activations, normalisations, attention, pertes, gradients, AdamW/SGD. `Reshape` partage le buffer ;
> les pertes synchronisent ; `std::initializer_list` est la seule entorse au zéro-STL.

---

## Aperçu de l'API

Tous les éléments publics, regroupés par objet. Détaillés un par un dans la « Référence complète ».

### `NkCommandBufferType` (enum) et `NkICommandBuffer`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Type | `NK_GRAPHICS` · `NK_COMPUTE` · `NK_TRANSFER` | File draw+compute+copy / compute+copy dédiée / copy DMA seule |
| Cycle de vie | `Begin` · `End` · `Reset` · `IsValid` · `GetType` | Ouvrir / fermer / réinitialiser l'enregistrement ; validité ; type |
| Render pass | `BeginRenderPass` · `EndRenderPass` · `NextSubpass` | Ouvrir/fermer une passe ; sous-passe (no-op DX/GL) |
| Compute pass | `BeginComputePass` · `EndComputePass` | Passe compute (défaut : marqueur debug + timestamp) |
| Viewport / scissor | `SetViewport` · `SetViewports` · `SetScissor` · `SetScissors` | Zone(s) de dessin ; zone(s) de découpe |
| Clear dynamique | `SetClearColor` (×3 surcharges) · `SetClearDepth` | Surcharge couleur/profondeur **avant** le render pass (persistant/frame) |
| Pipeline / descriptors | `BindGraphicsPipeline` · `BindComputePipeline` · `BindDescriptorSet` · `PushConstants` · `UpdateBuffer` | Lier pipeline/sets ; petites constantes ; écriture buffer **différée** |
| Vertex / index | `BindVertexBuffer` · `BindVertexBuffers` · `BindIndexBuffer` | Lier les tampons de sommets / d'indices |
| Draw | `Draw` · `DrawIndexed` · `DrawIndirect` · `DrawIndexedIndirect` · `DrawIndirectCount` | Dessins direct, indexé, indirect (args GPU), multi-draw count GPU |
| Dispatch | `Dispatch` · `DispatchIndirect` | Lancer un compute (groupes fixes / args GPU) |
| Copies | `CopyBuffer` · `CopyBufferToTexture` · `CopyTextureToBuffer` · `CopyTexture` · `BlitTexture` | Transferts ; blit filtré (resize/mip) |
| Barrières | `Barrier` · `TextureBarrier` · `UAVBarrier` | Barrière groupée ; helpers texture / UAV |
| Mipmaps | `GenerateMipmaps` | Générer la chaîne de mips sur GPU |
| Debug | `BeginDebugGroup` · `EndDebugGroup` · `InsertDebugLabel` | Marqueurs (no-op par défaut) |
| Clear explicite | `ClearTexture` · `ClearBuffer` | Effacer hors render pass (textures storage / buffers) |
| Timestamps | `WriteTimestamp` · `ResetQueryPool` | Mesure GPU (no-op par défaut) |

### `NkCommandPool`

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkCommandPool()` · `NkCommandPool(device, type)` · move · `~NkCommandPool` | Vide / lié / déplaçable ; destruction = `Reset()` |
| Init tardive | `Init(device, type)` | Initialiser un pool construit par défaut |
| Acquisition | `Acquire` · `Release` · `ReleaseAll` | Sortir un buffer (recyclé/créé) ; rendre (après fence) ; tout rendre (après WaitIdle) |
| Cycle | `Reset` | Détruire physiquement tous les buffers |
| Stats | `FreeCount` · `InFlightCount` · `TotalCount` | Nombre de buffers libres / en vol / total |

### `NkComputeContext` et compagnie

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init` · `Shutdown` · `IsReady` | Initialiser / arrêter / prêt ? |
| Passe | `BeginPass` · `EndPass` · `InPass` · `GetCurrentCmd` | Ouvrir/fermer la passe ; en passe ? ; CB courant |
| Cache pipelines | `GetOrCreatePipeline` · `GetOrCompileGLSL` · `EvictPipeline` · `ClearPipelineCache` · `SetPipeline` | Cache par clé ; compiler GLSL inline ; évincer ; vider ; lier |
| Bindings | `BindBuffer` · `BindUniformBuffer` · `BindStorageTexture` · `BindSampledTexture` · `BindDescriptorSet` · `ClearBindings` | SSBO / UBO / image / texture+sampler / set avancé ; reset (paresseux) |
| Push constants | `PushConstants<T>` · `PushConstantsRaw` | Constantes typées / brutes |
| Dispatch | `Dispatch` · `Dispatch1D` · `Dispatch2D` · `Dispatch3D` · `DispatchIndirect` | Lancer (groupes calculés selon dimension) |
| Barrières | `UAVBarrier` (buffer/texture) · `TransitionForCompute` · `TransitionForGraphics` · `FullBarrier` | Synchro entre dispatches ; transitions d'état ; barrière globale |
| Async | `SubmitAsync` · `WaitForFence` · `SyncComputeToGraphics` | Soumettre (file dédiée VK/DX12) ; attendre CPU ; synchro compute→graphics |
| Capacités | `SupportsAsyncCompute` · `SupportsIndirectDispatch` · `MaxGroupSizeX/Y/Z` · `MaxSharedMemoryBytes` · `OptimalGroupSize1D` | Interroger les limites matérielles |
| Stats | `GetLastPassStats` · `ResetPassStats` | Dernière passe ; remettre à zéro |
| Structs | `NkComputePassStats` · `NkComputePipelineCacheEntry` · `NkBoundResource` | Stats passe ; entrée cache ; binding en attente |
| Builder | `NkComputeBuilder` : `Pipeline` · `Buffer` · `Uniform` · `StorageTex` · `SampledTex` · `Push<T>` · `Dispatch[1D/2D/3D]` · `UAVBarrier` | API fluide chaînable (réf. au contexte) |
| Libres | `NkComputeGroups(count, size)` · `NkComputeAlignUp(v, align)` | Nombre de groupes (ceil) ; alignement puissance de 2 |

### `NkMLContext` et ML

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Formes / tenseurs | `NkTensorShape` (+ `NumElements`/`NumBytes`/`Rank`/`[]`/`==`/`ToString`) · `NkTensor` (+ `IsValid`/`Rows`/`Cols`/`Batch`…) | Forme multi-dim ; tenseur GPU |
| Cycle de vie | `Init` · `Shutdown` · `IsReady` | Initialiser / arrêter / prêt ? |
| Gestion tenseurs | `CreateTensor` (×2) · `DestroyTensor` · `Upload` · `Download` | **Create/Destroy** ; transferts CPU↔GPU (float32) |
| Init poids | `FillZero` · `FillOnes` · `FillConst` · `InitXavier` · `InitHe` | Remplissages ; initialisations Xavier / He |
| Algèbre | `MatMul` · `MatMulAdd` · `BatchMatMul` · `Add` · `Mul` · `Transpose` | Produits matriciels, élément-par-élément, transposée |
| Convolution | `Conv2DParams` · `Conv2D` | Convolution 2D (stride/pad/dilation/groups) |
| Activations | `ReLU` · `LeakyReLU` · `GELU` · `Sigmoid` · `Tanh` · `Softmax` · `LogSoftmax` · `Swish` · `SiLU` | Non-linéarités |
| Normalisations | `LayerNorm` · `BatchNorm` · `RMSNorm` | Normalisations de couche / batch / RMS |
| Attention | `ScaledDotProductAttention` | Attention Transformer (softmax(QKᵀ/√d)V) |
| Embedding / dropout | `EmbeddingLookup` · `Dropout` | Table d'embeddings ; dropout (no-op en inférence) |
| Pooling | `MaxPool2D` · `AvgPool2D` · `GlobalAvgPool` | Sous-échantillonnage |
| Forme / copie | `Reshape` (sans copie) · `Copy` | Re-former (buffer partagé) ; copier |
| Pertes | `MSELoss` · `MAELoss` · `CrossEntropy` · `BCELoss` · `CosineSimilarity` | Scalaires CPU (synchro implicite) |
| Backward | `ReLUBackward` · `MatMulBackward` · `LayerNormBackward` · `SoftmaxCrossEntropyBackward` | Gradients |
| Optimiseurs | `AdamConfig`/`AdamState` + `CreateAdamState`/`DestroyAdamState`/`AdamWStep` · `SGDConfig`/`SGDState` + `CreateSGDState`/`SGDStep` | AdamW / SGD (Create/Destroy) |
| Utilitaires | `Sync` · `TensorStats`/`ComputeStats` · `PrintTensor` · `AllClose` · `GetOrCompilePipeline` | Synchro ; stats ; debug ; comparaison ; cache pipeline |
| Shaders GLSL | `NkMLShaders::MatMul/MatMulAdd/ReLU/GELU/Sigmoid/Softmax/LayerNorm/AdamWStep/MSELoss/CrossEntropyBackward` | Sources `const char*` des kernels |

---

## Référence complète

Chaque élément est repris à fond, avec ses usages dans les différents domaines du temps réel — rendu,
ECS, physique, animation, gameplay/IA, audio, UI/2D, IO, GPU générique, threading, outils/éditeur. La
prose à puces remplace ici les tableaux.

### `NkCommandBufferType` — choisir la file

Trois natures de buffer, choisies à la création selon le matériel sous-jacent (les GPU modernes ont
plusieurs files matérielles qui peuvent tourner en parallèle) :

- **`NK_GRAPHICS`** — la file généraliste : draw + compute + copy. C'est celle du rendu classique
  (NKRenderer, NKCanvas) et celle qu'on prend par défaut.
- **`NK_COMPUTE`** — compute + copy sur une **file dédiée**. Sert au calcul **asynchrone** : faire
  tourner une simulation de particules, une passe de physique GPU ou un kernel ML pendant que la file
  graphique dessine la frame. Vrai parallélisme sur Vulkan/DX12.
- **`NK_TRANSFER`** — copies seules, sur le **moteur DMA** dédié. Idéal pour les **uploads** en
  arrière-plan (streaming de textures pour un terrain, chargement d'assets par l'éditeur, mégatexture)
  sans embêter les files graphique et compute.

### `NkICommandBuffer` — cycle de vie

`Begin()` ouvre l'enregistrement (renvoie `false` en échec), `End()` le ferme, `Reset()` remet à
blanc pour réutiliser. `IsValid()` dit si le buffer est utilisable, `GetType()` rappelle sa nature.
La discipline est universelle quel que soit le domaine : **un buffer par tâche d'enregistrement**, et
on respecte `Begin → … → End → Submit → fence → Reset`.

- **Rendu / 2D** — un buffer par frame (ou par vue, par thread de rendu) qui enregistre toute la
  scène.
- **Threading** — l'enregistrement est thread-safe sur **des buffers différents** : on découpe la
  scène en N lots, N threads remplissent N buffers en parallèle, on soumet l'ensemble.
- **Outils / éditeur** — un buffer `NK_TRANSFER` séparé pour streamer les assets pendant l'édition.

### `BeginRenderPass`, `EndRenderPass`, `NextSubpass` — la passe de rendu

`BeginRenderPass(rp, fb, area)` démarre une passe vers un framebuffer sur une zone donnée ;
`EndRenderPass()` la clôt. Tout dessin (`Draw*`) doit vivre **entre** les deux. `NextSubpass()` passe
à la sous-passe suivante — concept Vulkan/Metal (passes multi-étapes en mémoire de tuile, précieux
sur mobile) ; il est **no-op** sur DX/GL, donc ne comptez pas dessus pour la logique de rendu si vous
visez tous les backends.

- **Rendu** — passe G-buffer puis passe d'éclairage ; sur mobile, les sous-passes évitent des
  allers-retours vers la VRAM.
- **UI / 2D** — une passe vers la cible d'écran pour dessiner l'interface par-dessus la scène.

### `BeginComputePass`, `EndComputePass` — la passe compute

Encadrent un bloc de dispatches. Leur **implémentation par défaut** est utilitaire : `BeginComputePass`
ouvre un groupe de debug (couleur bleutée) si `desc.debugName` est renseigné et écrit un timestamp si
`desc.enableTimestamp` ; `EndComputePass` ferme le groupe. Les backends avancés les surchargent. Utile
pour **profiler** une simulation GPU et la repérer dans RenderDoc.

### `SetViewport`, `SetViewports`, `SetScissor`, `SetScissors` — zones de dessin et de découpe

Le **viewport** définit où, dans la cible, la géométrie est projetée ; le **scissor** découpe ce qui
sort d'un rectangle. Les versions plurielles posent plusieurs zones d'un coup (rendu *multi-view*).

- **Rendu** — viewport plein écran ; plusieurs viewports pour le rendu stéréo (VR) ou le *cascaded
  shadow mapping*.
- **UI / 2D** — le scissor est le mécanisme de **clipping** d'un panneau ou d'une liste défilante
  (c'est exactement ce que NKCanvas/NKUI poussent ici sous le capot).
- **Outils / éditeur** — plusieurs viewports pour les vues face/dessus/côté/perspective d'un éditeur 3D.

### `SetClearColor`, `SetClearDepth` — effacement dynamique

Analogues de `glClearColor`/`glClearDepth` : ils **surchargent** la couleur et la profondeur du
`NkRenderPassDesc`, à condition d'être appelés **avant** `BeginRenderPass`. La valeur est **persistante
par frame** jusqu'au prochain appel. `SetClearColor` existe en trois surcharges : la virtuelle
`(r, g, b, a)` (la seule que les backends implémentent), et **deux non-virtuelles** pratiques qui
forwardent — `(NkColorF&)` et `(NkColor&)` (cette dernière via `ToColorF()`). Attention : la virtuelle
de base est **no-op par défaut** — sur un backend qui ne l'implémente pas, l'effacement reste celui du
descripteur.

- **Rendu** — fond dégradé du ciel changeant selon l'heure ; profondeur effacée à 1.0 chaque frame.
- **UI / 2D** — couleur de fond d'un panneau ou d'une scène (cf. la propriété `BackgroundColor` d'une
  scène Pong).

### `BindGraphicsPipeline`, `BindComputePipeline`, `BindDescriptorSet`, `PushConstants`, `UpdateBuffer`

`BindGraphicsPipeline`/`BindComputePipeline` lient l'état GPU complet (shaders, blend, depth…) pour le
dessin ou le calcul. `BindDescriptorSet(set, setIndex, …)` lie un groupe de ressources ; la convention
de `setIndex` structure la fréquence de mise à jour — **0 = par frame** (caméra, temps), **1 = par
passe**, **2 = par matériau** (textures), **3 = par objet** (matrice monde). `dynamicOffsets` sert aux
UBO dynamiques (un même buffer, des fenêtres différentes par draw).

`PushConstants(stages, offset, size, data)` envoie de **petites** données *per-draw* (une matrice,
quelques scalaires) **plus vite** qu'un UBO — pas d'allocation, écriture directe dans la commande.
`UpdateBuffer(buf, dstOffset, size, data)` est la pièce subtile : l'écriture est **différée** (la copie
est capturée et exécutée **dans l'ordre** avec les `Bind`/`Draw` qui suivent). C'est crucial : une
écriture immédiate écraserait la valeur pour **tous** les drawcalls de la frame ; cette version permet
un **UBO par drawcall** correct.

- **Rendu** — set 0 caméra, set 2 matériau, `PushConstants` la matrice monde par objet.
- **ECS** — `UpdateBuffer` le transform de chaque entité, *dans l'ordre*, entre ses binds et son draw.
- **Animation** — push d'un paramètre de mélange (*blend*) par instance.
- **GPU générique** — bind d'un pipeline compute puis des SSBO d'entrée/sortie.

### Vertex et index buffers — `BindVertexBuffer(s)`, `BindIndexBuffer`

`BindVertexBuffer(binding, buf, offset)` lie un tampon de sommets sur un point d'ancrage ;
`BindVertexBuffers(...)` en lie plusieurs d'un coup (géométrie répartie sur plusieurs flux : positions
ici, normales là, instances ailleurs). `BindIndexBuffer(buf, fmt, offset)` lie les indices (avec leur
format `NkIndexFormat`) pour le dessin indexé.

- **Rendu** — un flux positions + un flux attributs + un flux **par instance** (couleur, matrice) pour
  l'*instancing* (forêt d'arbres, foule).
- **2D** — un gros buffer de quads batché pour les sprites/UI.

### Draw calls — `Draw`, `DrawIndexed`, `DrawIndirect`, `DrawIndexedIndirect`, `DrawIndirectCount`

`Draw(vertexCount, instanceCount, …)` dessine sans indices ; `DrawIndexed(indexCount, …)` avec — la
forme habituelle pour les maillages, qui réutilise les sommets partagés (`vertexOffset` décale dans le
tampon de sommets, `firstInstance` dans les instances). Les variantes **indirectes** lisent leurs
arguments dans un **buffer GPU** (`DrawIndirect`, `DrawIndexedIndirect`) : le nombre et les paramètres
de dessin sont produits **par le GPU lui-même** — base du *GPU-driven rendering*. `DrawIndirectCount`
va plus loin (le **nombre** de draws vient aussi d'un buffer) ; c'est une virtuelle **no-op par défaut**
(DX12/Vulkan/Metal).

- **Rendu** — `DrawIndexed` pour chaque maillage ; `instanceCount` élevé pour l'*instancing*.
- **GPU générique / rendu** — un compute *culle* la scène et remplit le buffer d'arguments, puis
  `DrawIndexedIndirect` dessine sans retour CPU (culling GPU, *meshlets*).
- **Gameplay / particules** — `Draw(4, particleCount)` dessine un quad par particule.

### Compute dispatch — `Dispatch`, `DispatchIndirect`

`Dispatch(groupsX, groupsY, groupsZ)` lance un compute shader sur une grille de groupes de threads ;
`DispatchIndirect(argsBuffer, offset)` lit la taille de grille dans un **buffer GPU** (utile quand le
nombre d'éléments à traiter est décidé par un kernel précédent).

- **Physique GPU** — un thread par particule/rigidbody pour intégrer les forces.
- **Animation** — *skinning* GPU : un thread par sommet, lecture des matrices d'os.
- **Audio** — convolution / FFT par blocs sur la file compute.
- **Outils** — génération procédurale de terrain, voxelisation.

### Copies — `CopyBuffer`, `CopyBufferToTexture`, `CopyTextureToBuffer`, `CopyTexture`, `BlitTexture`

La famille de transferts. `CopyBuffer` duplique des octets entre buffers GPU ; `CopyBufferToTexture`
*upload* des pixels d'un buffer *staging* vers une texture (le chemin standard du chargement
d'images) ; `CopyTextureToBuffer` fait l'inverse (lire le résultat GPU, faire une capture d'écran) ;
`CopyTexture` copie texture→texture. `BlitTexture(src, dst, region, filter)` ajoute le **filtrage**
(`NK_LINEAR` par défaut) : il **redimensionne** en copiant — exactement ce qu'il faut pour générer un
niveau de mip ou réduire une cible.

- **Rendu** — `CopyBufferToTexture` pour charger les textures ; `BlitTexture` pour le *downsampling*
  d'un bloom ; `CopyTextureToBuffer` pour une capture.
- **IO / streaming** — staging buffer → texture sur la file `NK_TRANSFER`.
- **Outils / éditeur** — relire le framebuffer pour l'exporter, le *picking* par lecture d'un ID
  rendu.

### Barrières — `Barrier`, `TextureBarrier`, `UAVBarrier`

Les barrières disent au GPU **« attends que ceci soit fini avant de faire cela »** et changent l'état
d'une ressource. `Barrier(bufBarriers, n, texBarriers, m)` est la forme groupée (la plus efficace : on
pose toutes les transitions d'un coup). Deux **helpers non-virtuels** simplifient les cas courants :
`TextureBarrier(tex, before, after, src, dst)` construit une barrière de texture (transition d'état,
ex. *render target* → *shader read*) ; `UAVBarrier(buf)` pose une barrière lecture-écriture sur un
buffer **après un write compute, avant la lecture suivante**.

- **GPU générique** — `UAVBarrier` entre deux dispatches qui se passent un buffer (incontournable,
  sinon course mémoire).
- **Rendu** — `TextureBarrier` pour passer une cible de rendu à l'état échantillonnable (un *render
  target* devient une texture lue par la passe suivante).

### `GenerateMipmaps` — la chaîne de mips sur GPU

`GenerateMipmaps(texture, filter)` produit tous les niveaux de mip d'une texture **sur le GPU** (par
*blits* successifs filtrés). Indispensable au filtrage trilinéaire/anisotrope sans artefacts de moiré.

- **Rendu** — mips d'une texture diffuse chargée ; mips d'une cible utilisée comme source de réflexion.

### Debug markers — `BeginDebugGroup`, `EndDebugGroup`, `InsertDebugLabel`

Marqueurs annotant la liste de commandes pour les outils GPU (RenderDoc, PIX, Nsight) : `BeginDebugGroup`
ouvre un groupe nommé (avec une couleur), `EndDebugGroup` le ferme, `InsertDebugLabel` pose un repère
ponctuel. Tous **no-op par défaut** — sans effet sur les performances de la release.

- **Outils / debug** — encadrer « Ombres », « Opaque », « Transparent », « UI » pour lire la trace GPU
  d'un coup d'œil.

### Clear explicite — `ClearTexture`, `ClearBuffer`

Effacement **hors render pass**. `ClearTexture(tex, value, baseMip, mipCount, baseLayer, layerCount)`
remet une texture (typiquement *storage*) à une valeur, sur une plage de mips/couches ;
`ClearBuffer(buf, value, offset, size)` remplit un buffer d'une valeur 32 bits. Tous deux **no-op par
défaut**.

- **GPU générique** — remettre à zéro un buffer de compteurs / une texture d'accumulation avant un
  nouveau passage compute.

### Timestamps — `WriteTimestamp`, `ResetQueryPool`

`WriteTimestamp(queryIndex)` inscrit l'horloge GPU à un point de la liste ; la différence entre deux
timestamps donne le **temps GPU** d'une portion. `ResetQueryPool(firstQuery, count)` réinitialise les
emplacements de requête avant réécriture. **No-op par défaut**.

- **Outils / profilage** — mesurer le coût GPU réel d'une passe (ombres, post-traitement, simulation).

### `NkCommandPool` — construction et stock

Le pool se construit lié (`NkCommandPool(device, type)`) ou vide puis initialisé tard (`Init(device,
type)`). Il est **non-copiable** (un pool ne se duplique pas) mais **déplaçable** (transfert de
propriété, ex. le sortir d'une fonction). Le destructeur appelle `Reset()`. Il ne **possède pas** le
device, juste un pointeur — sa durée de vie doit donc tenir dans celle du device.

### `Acquire`, `Release`, `ReleaseAll`, `Reset` — le recyclage

`Acquire()` (sous verrou) renvoie un buffer **prêt** : s'il en reste un libre, il le sort et le
**`Reset()`** pour vous ; sinon il en crée un neuf. Le buffer passe « en vol ». `Release(cb)` le
rapatrie en libre — par une **recherche linéaire O(n)** dans les en-vol — et **doit n'être appelé
qu'après la fence** (sinon le buffer pourrait être réacquis et réinitialisé alors qu'il est encore
consommé par le GPU). `ReleaseAll()` rapatrie tout le monde d'un coup (à appeler **après un WaitIdle**).
`Reset()` **détruit physiquement** tous les buffers (no-op si le device est nul) — c'est l'arrêt.

- **Rendu / threading** — un pool par thread de rendu et par frame en vol ; `Acquire` au début de la
  frame, `Release` après l'attente de la fence de cette frame.
- **Outils** — un pool `NK_TRANSFER` dédié au streaming.

### `FreeCount`, `InFlightCount`, `TotalCount` — instrumentation

`FreeCount()` = buffers disponibles, `InFlightCount()` = buffers actuellement soumis/en cours,
`TotalCount()` = somme des deux. Utile pour **diagnostiquer** une fuite (in-flight qui ne redescend
jamais = on oublie de `Release`) ou dimensionner le pool.

### `NkComputeContext` — cycle de vie et passe

`Init(device)` prépare le contexte (renvoie `false` en échec) ; `Shutdown()` libère cache de pipelines
et état ; `IsReady()` indique l'état. La **passe** s'ouvre avec `BeginPass(cmd, desc)` — **précondition :
`cmd->Begin()` déjà appelé** — qui pose un marqueur de debug et démarre un timestamp si demandé, et se
ferme avec `EndPass()` (qui *flush* les bindings restants et clôt le marqueur). `InPass()` dit si on
est dans une passe, `GetCurrentCmd()` rend le command buffer courant.

### Cache de pipelines compute

`GetOrCreatePipeline(key, shader, debugName)` renvoie le pipeline en cache pour cette clé, ou le crée
à partir d'un shader déjà chargé. `GetOrCompileGLSL(key, glslSrc, debugName)` **compile du GLSL inline**
et le met en cache (OpenGL 4.3+, Vulkan via SPIRV-Cross quand disponible) — pratique pour des kernels
écrits sur place. `EvictPipeline(key)` retire et détruit une entrée, `ClearPipelineCache()` vide tout.
`SetPipeline(pipeline)` **lie** un pipeline (dans une passe).

- **GPU générique / outils** — compiler à la volée un kernel de test (`GetOrCompileGLSL`), le mettre
  en cache, et le réutiliser frame après frame sans recompiler.

### Bindings paresseux

Le contexte **accumule** les ressources et ne les applique qu'au **dispatch** (lazy flush) :
`BindBuffer(binding, buf, offset, range)` lie un SSBO (lecture + écriture) ; `BindUniformBuffer(...)`
un UBO (lecture seule, ≤ 64 Ko) ; `BindStorageTexture(binding, tex)` une image *load/store* ;
`BindSampledTexture(binding, tex, sampler)` une texture échantillonnée (lecture seule).
`BindDescriptorSet(...)` est le chemin **avancé** — il **annule et remplace** tous les bindings
accumulés. `ClearBindings()` les remet à zéro sans rien appliquer.

- **Physique** — `BindBuffer` positions/vitesses (SSBO), `BindUniformBuffer` les paramètres de
  simulation.
- **Rendu / post-traitement** — `BindSampledTexture` la source, `BindStorageTexture` la destination
  d'un flou compute.

### Push constants — `PushConstants<T>`, `PushConstantsRaw`

`PushConstants(data)` (template inline) envoie une structure typée (la taille est déduite, garantie
correcte) ; `PushConstantsRaw(data, size, offset)` la forme brute. Idéal pour les **petits paramètres
par dispatch** (rayon de flou, *delta time*, dimensions).

### Dispatch — `Dispatch`, `Dispatch1D/2D/3D`, `DispatchIndirect`

Tous **flushent** d'abord les bindings en attente. `Dispatch(gx, gy, gz)` est la forme brute (en
nombre de **groupes**). Les helpers calculent ce nombre depuis un **compte d'éléments** :
`Dispatch1D(count, groupSize=256)` = `ceil(count/groupSize)` groupes (tableaux linéaires) ;
`Dispatch2D(w, h, tileX=16, tileY=16)` (images) ; `Dispatch3D(w, h, d, …)` (volumes).
`DispatchIndirect(argsBuffer, offset)` lit la grille dans un buffer GPU.

- **Physique / particules** — `Dispatch1D(particleCount)`.
- **Rendu / 2D** — `Dispatch2D(width, height)` pour un filtre image.
- **Volumes** — `Dispatch3D` pour un *fog* volumétrique, une grille de fluide, une SDF.

### Barrières et transitions du contexte compute

`UAVBarrier(buf)` / `UAVBarrier(tex)` synchronisent **après un write, avant la lecture** du même
buffer/texture — l'oubli est la cause numéro un de corruption en compute. `TransitionForCompute(tex,
before)` fait passer une texture vers l'état *unordered access* (pour qu'un compute l'écrive) ;
`TransitionForGraphics(tex, after)` la ramène vers *shader read* (pour que le rendu l'échantillonne).
`FullBarrier()` est une barrière globale tous-stages — **coûteuse**, à réserver au débogage ou aux
transitions rares.

- **GPU générique** — alterner *ping-pong* entre deux buffers avec une `UAVBarrier` entre chaque passe.
- **Rendu** — un compute génère une texture (`TransitionForCompute`), puis le rendu la lit
  (`TransitionForGraphics`).

### Soumission asynchrone — `SubmitAsync`, `WaitForFence`, `SyncComputeToGraphics`

`SubmitAsync(cmd, waitSemaphore, signalSemaphore)` soumet sur une **file compute dédiée** (Vulkan/DX12,
vrai parallélisme avec le rendu ; **synchrone** sur GL/DX11) et renvoie une **fence**. `WaitForFence(fence,
timeoutNanos)` attend côté **CPU** que le GPU ait fini (`UINT64_MAX` = infini). `SyncComputeToGraphics(
computeSignal, graphicsCmd)` fait attendre la file graphique le signal de la file compute.

- **Animation / physique** — lancer le *skinning* ou la simulation en async pendant que le rendu de
  la frame précédente se termine, puis synchroniser avant de dessiner les résultats.

### Capacités matérielles

Toutes `const`, pour s'adapter au GPU : `SupportsAsyncCompute()`, `SupportsIndirectDispatch()`,
`MaxGroupSizeX/Y/Z()`, `MaxSharedMemoryBytes()` (taille de la mémoire partagée par groupe) et
`OptimalGroupSize1D()` (taille de groupe optimale, alignée sur le *warp*/*wavefront* — typiquement 32
ou 64). On les interroge pour **dimensionner** ses kernels au lieu de coder en dur.

### Statistiques — `NkComputePassStats`, `GetLastPassStats`, `ResetPassStats`

`NkComputePassStats` agrège la dernière passe : `dispatchCount`, `totalGroupsX/Y/Z`, et
`gpuMilliseconds` (si `enableTimestamp`). `GetLastPassStats()` la lit, `ResetPassStats()` la remet à
zéro. Pour **profiler** une simulation GPU.

### Structures internes exposées — `NkComputePipelineCacheEntry`, `NkBoundResource`

`NkComputePipelineCacheEntry` (`shader` + `pipeline`) est une entrée du cache. `NkBoundResource`
décrit un **binding en attente** (avant flush) : `binding`, `type` (`NkDescriptorType`), `buffer`,
`texture`, `sampler`, `bufOffset`, `bufRange` (0 = buffer entier). On les croise surtout en débogage de
l'état du contexte.

### `NkComputeBuilder` — l'API fluide

Façade chaînable construite sur une **référence** au contexte (`NkComputeBuilder(ctx)`). Chaque méthode
renvoie `NkComputeBuilder&` et délègue : `Pipeline` → `SetPipeline`, `Buffer`/`Uniform`/`StorageTex`/
`SampledTex` → les binds correspondants, `Push<T>` → `PushConstants`, `UAVBarrier` → la barrière, et
`Dispatch`/`Dispatch1D/2D/3D` → les dispatches (qui restent chaînables pour enchaîner un second
dispatch avec une barrière entre). **Piège** : le builder ne possède rien — il ne doit pas survivre au
contexte référencé.

### Helpers libres — `NkComputeGroups`, `NkComputeAlignUp`

`NkComputeGroups(count, groupSize)` = `(count + groupSize - 1) / groupSize` : le nombre de groupes
arrondi au supérieur, sans débordement. `NkComputeAlignUp(value, alignment)` aligne `value` au multiple
supérieur d'`alignment` (qui **doit** être une puissance de 2) — pour respecter les contraintes
d'alignement GPU (taille de UBO, *padding*).

### `NkTensorShape` — la forme d'un tenseur

Décrit les dimensions (`NkVector<uint32> dims`). Se construit par défaut, depuis une liste
(`{ batch, dim }`, via `std::initializer_list`) ou depuis un `NkVector`. `NumElements()` = produit des
dimensions (0 si vide), `NumBytes()` = `NumElements() × sizeof(float32)` (le type float32 est
implicite), `Rank()` = nombre de dimensions, `operator[]` lit une dimension, `==`/`!=` comparent les
formes, `ToString()` rend `"[d0,d1,…]"` (debug). C'est l'équivalent du *shape* de NumPy/PyTorch.

### `NkTensor` — le tenseur GPU

Wrapper autour d'un `NkBufferHandle` : `buffer` (les données en VRAM), `shape`, `requiresGrad` (s'il
faut accumuler un gradient), `grad` (le buffer du gradient) et `name` (debug). `IsValid()` teste le
buffer, `NumElements()`/`NumBytes()` délèguent à la forme, et trois accesseurs sémantiques l'interprètent
comme une matrice/un batch : `Rows()` (avant-dernière dim si rang ≥ 2), `Cols()` (dernière dim) et
`Batch()` (première dim si rang ≥ 3). **Règle dure** : un tenseur se **crée et se détruit**
(`CreateTensor`/`DestroyTensor`), jamais à la main.

### `NkMLContext` — cycle de vie et tenseurs

`Init(device)` monte le moteur ML sur le device RHI, `Shutdown()` le démonte, `IsReady()` indique
l'état. `CreateTensor(shape, requiresGrad, name)` (et la surcharge `{ dims… }`) alloue un tenseur GPU ;
`DestroyTensor(t)` libère son buffer **et** son gradient — **Create/Destroy obligatoire**. `Upload(t,
data, count)` copie du CPU vers le GPU (count 0 = tout), `Download(t, out, count)` l'inverse ; les deux
supposent du **float32**.

- **IA / ML** — charger des poids pré-entraînés (`Upload`), récupérer une prédiction (`Download`).

### Initialisation des poids

`FillZero`/`FillOnes`/`FillConst(val)` remplissent un tenseur d'une constante (biais à zéro, masques).
`InitXavier` initialise selon √(2/(fan_in+fan_out)) — adapté aux activations symétriques (tanh) ;
`InitHe` selon √(2/fan_in) — l'initialisation **recommandée pour ReLU**. Une bonne init évite que les
activations n'explosent ou ne s'éteignent en profondeur.

### Algèbre linéaire — `MatMul`, `MatMulAdd`, `BatchMatMul`, `Add`, `Mul`, `Transpose`

Le cœur d'un réseau. `MatMul(A, B, C, transposeA, transposeB)` = produit matriciel C = A×B (A:[M,K],
B:[K,N], C:[M,N]), avec transposition optionnelle des opérandes. `MatMulAdd(A, B, bias, C)` ajoute un
biais [N] *broadcast* (la couche dense complète). `BatchMatMul` applique le produit **par batch** (le
bloc d'une couche d'attention). `Add(A, B, C, alpha, beta)` = αA + βB (connexions résiduelles), `Mul`
le produit **élément par élément** (portes, masques), `Transpose` la transposée.

- **IA** — chaque couche dense est un `MatMulAdd` suivi d'une activation ; les résiduels sont des `Add`.

### Convolution — `Conv2DParams`, `Conv2D`

`Conv2DParams` regroupe `strideH/W`, `padH/W`, `dilationH/W`, `groups` (constructeur par défaut qui
remet tout à neutre). `Conv2D(input, weight, bias, output, p)` applique une convolution 2D : input
[N, C_in, H, W], weight [C_out, C_in/groups, kH, kW], bias [C_out], sortie [N, C_out, H_out, W_out].
La brique des **réseaux convolutifs** (vision).

- **IA / vision** — extracteur de caractéristiques d'un classifieur d'images ou d'un détecteur.

### Activations

Toutes prennent (in, out) : `ReLU` (`max(0, x)`, l'activation par défaut), `LeakyReLU(alpha)` (laisse
passer une petite pente négative), `GELU` (lisse, exacte via `x·Φ(x)` — standard des Transformers),
`Sigmoid` (vers [0,1], portes/probabilités binaires), `Tanh` (vers [-1,1]), `Softmax(axis)` (distribution
de probabilités sur un axe — la couche de sortie d'un classifieur), `LogSoftmax(axis)` (sa version
numériquement stable pour la perte), `Swish` (`x·sigmoid(x)`) et `SiLU` (alias de `Swish`).

### Normalisations — `LayerNorm`, `BatchNorm`, `RMSNorm`

`LayerNorm(in, weight, bias, out, eps)` normalise sur la **dernière dimension** (standard des
Transformers, stable quelle que soit la taille de batch). `BatchNorm(..., runningMean, runningVar, ...,
training)` normalise sur le **batch** (réseaux convolutifs ; en inférence il utilise les statistiques
courantes). `RMSNorm(in, weight, out, eps)` est une variante allégée (LLM modernes type LLaMA). Elles
**stabilisent** l'entraînement en gardant les activations dans une plage saine.

### Attention — `ScaledDotProductAttention`

`ScaledDotProductAttention(Q, K, V, out, scale, mask)` calcule `softmax(Q·Kᵀ/√d_k)·V` — le mécanisme
au cœur des **Transformers**. Q/K/V/out sont [batch, heads, seq, d_head] ; `scale` à 0 prend la valeur
par défaut 1/√d_k ; `mask` (optionnel) sert au masquage causal (génération de texte) ou de *padding*.

- **IA / LLM** — chaque bloc d'un Transformer (texte, ou *vision transformer*).

### Embedding et dropout — `EmbeddingLookup`, `Dropout`

`EmbeddingLookup(table, indices, out)` transforme des indices entiers (table [vocab, embed_dim],
indices [batch, seq] en uint32) en vecteurs denses — la première couche d'un modèle de langage.
`Dropout(in, out, rate, training)` éteint aléatoirement une fraction des activations pendant
l'entraînement (régularisation) et est **no-op en inférence**.

### Pooling — `MaxPool2D`, `AvgPool2D`, `GlobalAvgPool`

`MaxPool2D`/`AvgPool2D(in, out, kH, kW, strideH, strideW)` **sous-échantillonnent** une carte de
caractéristiques (réduisent sa résolution en gardant le max ou la moyenne par fenêtre). `GlobalAvgPool`
réduit [N, C, H, W] en [N, C] (moyenne spatiale globale, fréquent avant la couche de classification).

### Forme et copie — `Reshape`, `Copy`

`Reshape(t, newShape)` **ne copie pas** : il renvoie un tenseur qui **partage le même buffer** avec une
forme différente (le nombre d'éléments doit coïncider) — aplatir une carte conv avant une couche dense,
par exemple. `Copy(src, dst)` duplique réellement les données.

### Fonctions de perte — `MSELoss`, `MAELoss`, `CrossEntropy`, `BCELoss`, `CosineSimilarity`

Toutes renvoient un **scalaire CPU** (donc téléchargement implicite, **synchro coûteuse**). `MSELoss`
(erreur quadratique, régression), `MAELoss` (erreur absolue, robuste aux *outliers*), `CrossEntropy`
(classification multi-classes, sur logits + labels), `BCELoss` (classification binaire),
`CosineSimilarity` (alignement de deux vecteurs, embeddings/recherche sémantique).

### Backward pass — `ReLUBackward`, `MatMulBackward`, `LayerNormBackward`, `SoftmaxCrossEntropyBackward`

Les gradients de la rétropropagation. `ReLUBackward(out, gradOut, gradIn)` propage à travers ReLU.
`MatMulBackward(A, B, gradC, gradA, gradB)` : gradA = gradC·Bᵀ, gradB = Aᵀ·gradC. `LayerNormBackward`
rend les gradients d'entrée, de poids et de biais. `SoftmaxCrossEntropyBackward(logits, labels,
gradLogits)` est la version **fusionnée** (et stable) softmax+cross-entropy : grad = (softmax − label) /
taille_de_batch. Ce sont les briques d'une boucle d'entraînement maison.

### Optimiseurs — AdamW et SGD

`AdamConfig` (`lr`, `beta1`, `beta2`, `eps`, `weightDecay`) et `AdamState` (`m`, `v`, `t`) portent la
configuration et l'état momentum/variance/pas. `CreateAdamState(param)` ↔ `DestroyAdamState(state)`
(**Create/Destroy**), `AdamWStep(param, grad, state, cfg)` met à jour les poids **en place** (AdamW,
l'optimiseur par défaut des Transformers). Côté SGD : `SGDConfig` (`lr`, `momentum`, `weightDecay`),
`SGDState` (`velocity`), `CreateSGDState(param)` et `SGDStep(...)`.

### Utilitaires ML — `Sync`, `ComputeStats`, `PrintTensor`, `AllClose`, `GetOrCompilePipeline`

`Sync()` attend la fin de **tous** les compute shaders (à appeler avant de lire un résultat).
`TensorStats` (`min`, `max`, `mean`, `std`) + `ComputeStats(t)` diagnostiquent un tenseur (détecter un
gradient qui explose). `PrintTensor(t, maxElems, prefix)` affiche les premières valeurs (debug).
`AllClose(A, B, atol, rtol)` teste l'égalité approchée — la base des **tests unitaires** ML.
`GetOrCompilePipeline(key, glslSrc)` compile et met en cache un pipeline GLSL (exposé pour les fonctions
amies).

### `NkMLShaders` — les kernels GLSL bruts

Le sous-namespace `nkentseu::NkMLShaders` expose les **sources GLSL** (`#version 430`) des kernels, en
`inline const char*` sans paramètre : `MatMul()` (GEMM tuilé 16×16 en mémoire partagée), `MatMulAdd()`
(GEMM + biais), `ReLU()`, `GELU()` (exact via `erf`), `Sigmoid()`, `Softmax()` (réduction par ligne en
3 passes avec `subgroupShuffle`), `LayerNorm()` (mean/var par ligne), `AdamWStep()`, `MSELoss()`
(réduction par `atomicAdd`) et `CrossEntropyBackward()` (softmax inline + (p−label)/batch). Utiles pour
**inspecter**, modifier ou recompiler le calcul brut hors du contexte ML.

---

### Exemple

```cpp
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Commands/NkCommandPool.h"
#include "NKRHI/Core/NkComputeContext.h"
#include "NKRHI/Core/NkML.h"
using namespace nkentseu;

// 1) Command pool + enregistrement d'une frame de rendu.
NkCommandPool pool(device, NkCommandBufferType::NK_GRAPHICS);
NkICommandBuffer* cb = pool.Acquire();          // recyclé, déjà Reset
cb->Begin();
cb->SetClearColor(0.1f, 0.1f, 0.12f);           // AVANT le render pass
cb->BeginRenderPass(rp, fb, area);
cb->SetViewport(vp);
cb->BindGraphicsPipeline(pipeline);
cb->BindVertexBuffer(0, vbo);
cb->BindIndexBuffer(ibo, NkIndexFormat::NK_UINT32);
cb->DrawIndexed(indexCount);
cb->EndRenderPass();
cb->End();
device->Submit(cb);
device->WaitFence(/* … */);
pool.Release(cb);                                // SEULEMENT après la fence

// 2) Compute fluide : simuler des particules sur GPU.
NkComputeContext compute; compute.Init(device);
cb = pool.Acquire(); cb->Begin();
compute.BeginPass(cb);
NkComputeBuilder(compute)
    .Pipeline(simPipeline)
    .Buffer(0, positions)
    .Buffer(1, velocities)
    .Push(SimParams{ dt })
    .Dispatch1D(particleCount);                 // groupes calculés tout seuls
compute.UAVBarrier(positions);                  // avant la lecture par le rendu
compute.EndPass();
cb->End();

// 3) ML : une couche dense + ReLU sur GPU, 100 % GLSL compute.
NkMLContext ml; ml.Init(device);
NkTensor x = ml.CreateTensor({ batch, inDim });
NkTensor W = ml.CreateTensor({ inDim, outDim }, /*requiresGrad*/ true);
NkTensor b = ml.CreateTensor({ outDim }, true);
NkTensor h = ml.CreateTensor({ batch, outDim });
ml.InitHe(W); ml.FillZero(b);
ml.MatMulAdd(x, W, b, h);                        // h = x·W + b
ml.ReLU(h, h);
ml.Sync();
ml.DestroyTensor(x); ml.DestroyTensor(W);
ml.DestroyTensor(b); ml.DestroyTensor(h);       // Create/Destroy obligatoire
```

---

[← Index NKRHI](README.md) · [Récap NKRHI](../NKRHI.md) · [Couche Runtime](../README.md)
