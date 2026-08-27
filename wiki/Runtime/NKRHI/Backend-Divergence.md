# Ce que chaque backend honore vraiment

> Couche **Runtime** · NKRHI · L'abstraction backend **mesurée** : ce que `NkIDevice` et
> `NkICommandBuffer` promettent, ce que chacun des six backends tient réellement, et la liste des
> méthodes **déclarées mais non honorées**.

Cette page existe pour une raison de maintenance : dans NKRHI, une méthode qui ne fait rien est
**indiscernable** d'une méthode qui marche. La plupart des virtuelles optionnelles ont un **corps
par défaut silencieux** (`{}`, `return true`, `return false`) — appeler une capacité absente ne lève
donc aucune erreur, ne journalise rien, et laisse l'appelant croire que le travail a eu lieu.
**Documenter cela n'est pas un luxe : une API présentée comme fonctionnelle alors qu'elle est inerte
recrute le prochain bug.**

Relevé du 24 août 2026, sur `Nkentseu-merge` @ `main`. Tous les chiffres viennent de décomptes
(`grep -c`, `wc`), pas d'estimations.

---

## 1. Les trois interfaces

| Interface | Fichier | `virtual` | pures | **avec corps par défaut** |
|---|---|---:|---:|---:|
| `NkIDevice` | `Core/NkIDevice.h:128` | 86 | 57 | **28** |
| `NkICommandBuffer` | `Commands/NkICommandBuffer.h:32` | 46 | 32 | **13** |
| `NkISwapchain` | `Core/NkISwapchain.h:36` | 17 | 14 | 2 |

Les 57 + 32 méthodes **pures** sont garanties : le code ne compilerait pas sans elles. Ce sont les
**41 virtuelles à corps par défaut** qui portent tout le risque — ce sont exactement celles qu'un
backend peut ne pas implémenter sans que rien ne le signale.

> ⚠️ **Règle de lecture.** Une méthode pure ⇒ les six backends la font. Une méthode à corps par
> défaut ⇒ **vérifiez la table du §4 avant de vous en servir.**

### Domaines de `NkIDevice`

Cycle de vie/frame (`:134-137`, `:322-347`) · swapchain (`:143`, `:254-259`, `:419-425`) ·
command buffers et soumission (`:300-312`, `:448-461`) · render passes et framebuffers
(`:233-246`) · descripteurs et bindless (`:265-287`, `:481-505`) · pipelines (`:214-225`) ·
ressources (`:166-209`) · synchronisation (`:315-319`, `:379-439`) · requêtes/timestamps
(`:355-365`) · capacités (`NkDeviceCaps` `:40-96`, `GetCaps` `:147`) · debug (`:386-410`) ·
interop natif (`:372-375`).

⚠️ **Les barrières ne sont pas sur le device** : elles n'existent que sur `NkICommandBuffer`
(`Barrier` `:235`).

### `NkICommandBuffer` — le patron NVI des draws

`Draw` (`:154`), `DrawIndexed` (`:162`), `DrawIndirect` (`:173`), `DrawIndexedIndirect` (`:179`)
sont **non virtuelles** : elles comptent (`NkCbStats` `:188`) puis délèguent aux pures
`DrawImpl`/`DrawIndexedImpl`/… (`:319-328`). C'est pourquoi les statistiques de dessin sont les
**seules** qui fonctionnent réellement dans tout NKRHI — le compteur est dans la classe de base, pas
dans les backends.

---

## 2. Les six backends — aucun n'est un squelette

| Backend | fichiers | LOC | appels natifs | verdict |
|---|---:|---:|---:|---|
| Vulkan | 5 | 4455 | 176 `vk*` | complet |
| OpenGL | 5 vivants (7 sur disque) | 4794 vivants | 413 `gl*` | complet |
| DirectX 11 | 4 | 2925 | 133 | complet, avec trous |
| DirectX 12 | 4 | 5028 | 105 | complet, avec trous |
| Metal | 4 | 2010 | 117 | complet, avec trous |
| Software | 8 | 7533 | rasterizer CPU | complet |

Tous remplissent l'intégralité de la surface pure — sans quoi ils ne compileraient pas. Les
différences sont **entièrement** dans les virtuelles optionnelles.

⚠️ **Faux positif à connaître.** `Vulkan/NkVulkanDevice.h` déclare la classe **deux fois** : la vraie
à `:128` sous `#ifdef NK_RHI_VK_ENABLED`, et un **squelette de compilation** à `:408-625` sous
`#else`, dont les ~59 overrides sont tous `return false;`/`{}`. Un `grep` qui tombe sur le second
conclura à tort que Vulkan ne fait rien.

**Poids mort mesuré :** `Opengl/NkOpenglDevice copy.hpp` (963 L) et
`Opengl/NkOpenglCommandBuffer copy.hpp` (196 L) ne sont **pas dans le build** — `NKRHI.jenga:58-79`
globe `**.cpp` et `**.h`, jamais `.hpp`. `Tools/NkGizmo/NkGizmo3D.h` fait **0 octet**.
`Core/NkCommandPool.h` et `Commands/NkCommandPool.h` sont **octet pour octet identiques** ; seul
`Commands/` est inclus.

---

## 3. 🔴 Déclaré mais honoré par **aucun** backend

Chacune de ces méthodes n'a que sa déclaration et son corps par défaut dans tout le dépôt.
**Les appeler ne fait rien et ne signale rien.**

| Méthode | Site | Ce que l'appelant croit | Ce qui se passe |
|---|---|---|---|
| `SetDebugName` ×3 | `NkIDevice.h:404,407,410` | ressources nommées dans RenderDoc/PIX | **rien** — 3 références dans tout le dépôt : les 3 déclarations |
| `BeginTimestampQuery`/`EndTimestampQuery`/`GetTimestampResults`/`GetTimestampPeriodNs` | `NkIDevice.h:355-365` | mesure GPU | `return false` / `return 1.f`, toujours |
| `GetLastFrameStats`/`ResetFrameStats` | `NkIDevice.h:395,400` | stats device | renvoie un `static` que personne n'écrit → zéros perpétuels |
| `SavePipelineCache`/`LoadPipelineCache` | `NkIDevice.h:220,225` | cache PSO sur disque | `return false` |
| Bindless (`CreateBindlessHeap`, `WriteBindlessTexture`, …) | `NkIDevice.h:481-505` | tableaux bindless | no-op ; les caps qui les gateraient sont `false` de toute façon |
| `CreateGpuSemaphore`/`DestroySemaphore` | `NkIDevice.h:435,439` | sync inter-queues | no-op |
| `HasDedicatedComputeQueue` | `NkIDevice.h:448` | queue compute dédiée | `false` sur les 6 → `NkComputeContext.cpp:396` rend le **compute asynchrone inatteignable** |
| `SubmitOnQueue` | `NkIDevice.h:454` | soumission sur une queue nommée | le défaut **jette les sémaphores** et soumet sur la queue graphique (`NkComputeContext.cpp:359`) |
| `ClearTexture` | `NkICommandBuffer.h:283` | effacement hors passe | corps qui **journalise** « NON IMPLEMENTE » (`NkICommandBuffer.cpp:64`) |
| `ResetQueryPool`, `NextSubpass`, `DrawIndirectCount` | `NkICommandBuffer.h:311,50,204` | — | no-op |

### `NkISwapchain` — interface entièrement morte

`grep -rn "public NkISwapchain"` → **0 résultat**. Aucune classe n'en dérive. Le header est
**commenté** hors de l'ombrelle (`NkRHI.h:11`). Son unique détenteur est
`NKRenderer/Core/NkRendererImpl.h:191` (`NkISwapchain *mSwapchain = nullptr;` — jamais assigné).
`NkIDevice::CreateSwapchain` renvoie toujours `nullptr`. **17 méthodes, 0 implémentation.**

---

## 4. Honoré par certains backends seulement

C'est ici que se cachent les bugs « ça marche sur ma machine ».

| Méthode | Implémenté | ⚠️ Inerte (corps vide / défaut) |
|---|---|---|
| `Barrier` | Vulkan `NkVulkanCommandBuffer.cpp:354` (98 L) · DX12 `:731` · GL `NkOpenglCommandBuffer.h:376` | **DX11** `NkDirectX11CommandBuffer.h:97` · **Metal** `:66` · **Software** `:93` |
| `UpdateBuffer` | Vulkan `:203` · GL `NkOpenglCommandBuffer.cpp:120` | **DX11, DX12, Metal, Software** — 4 corps vides, tous avec un `// TODO` |
| `BlitTexture` | Vulkan `:337` · Metal `.mm:417` · Software `:619` · GL `:368` | **DX11** `:94` · **DX12** `:722` (« laissé pour implémentation future ») |
| `CopyBufferToTexture` | Vulkan `:303` · DX12 `:634` · Metal `.mm:370` · Software `:569` · GL `:353` | **DX11** `NkDirectX11CommandBuffer.h:84` |
| `EndRenderPass` | VK · DX12 · Metal · Software · GL | **DX11** `NkDirectX11CommandBuffer.h:38` (vide) |
| `GenerateMipmaps` (cmd) | VK · DX11 · GL · Metal · Software | **DX12** `NkDirectX12CommandBuffer.h:92` |
| `GenerateMipmaps` (device) | VK · GL · Software · Metal · DX11 | 🔴 **DX12** `NkDirectX12Device.cpp:1349` — **`return true;` sans rien faire.** Annonce le succès, ne génère aucun mip |
| `DrawIndirectImpl` / `DrawIndexedIndirectImpl` | VK · DX11 · GL · Metal · Software | **DX12** `NkDirectX12CommandBuffer.cpp:593`, `:602` (`// TODO: DrawIndirectSignature`) |
| `DispatchIndirect` | VK · DX11 · GL · Metal · Software | **DX12** `:618` |
| `ClearBuffer` | **Vulkan seul** `:283` | les 5 autres → corps qui journalise |
| `WriteTimestamp` | **OpenGL seul** `NkOpenglCommandBuffer.h:452` | les 5 autres (et voir §6 : même GL ne rend aucun résultat) |
| `RecreateSurface` | **OpenGL seul** `NkOpenglDevice.cpp:2808` | les 5 autres → défaut **`return true`** : prétend avoir réussi |
| `GetFramebufferRenderPass` | Vulkan `:2521` · DX12 `:2061` | GL, DX11, Metal, Software → `return {}` |
| `IsSwapchainSrgb` | Vulkan · OpenGL | DX11, DX12, Metal, Software → `false` |
| offsets dynamiques de `BindDescriptorSet` | VK, DX12 | **OpenGL les ignore** — `NkOpenglDeviceInternal.cpp:327` `(void)dynOff; // TODO` |

### 🔴 Marqueurs de debug : les deux backends qui en profiteraient le plus les jettent

| Backend | Réel ? |
|---|---|
| OpenGL (`glPushDebugGroup`), DX11 (`ID3DUserDefinedAnnotation`), Metal (`pushDebugGroup`), Software (log) | **oui** |
| **Vulkan** `NkVulkanCommandBuffer.cpp:477` | **non** — `vkCmdBeginDebugUtilsLabelEXT` **commenté** (`:479-481`), `EndDebugGroup` `:488` a **0 ligne de corps** |
| **DirectX 12** `NkDirectX12CommandBuffer.cpp:801` | **non** — `PIXBeginEvent` **commenté** (`:803`), `EndDebugGroup` `:810` vide |

C'est la divergence la plus coûteuse du module : RenderDoc et PIX sont précisément les outils de
Vulkan et DX12, et ce sont les deux seuls backends à ne rien leur envoyer.

---

## 5. ⚠️ `NkGraphicsPipelineDesc::samples` — **état daté : `main`, 24/08/2026**

> 📌 **Deux mesures justes, deux branches différentes.** Sur **`feat/rendu-temps-reel`**, un
> correctif dérive `samples` de la render pass côté Vulkan et DX12. Sur **`main` au 24/08/2026** —
> la branche mesurée ici — **ce correctif n'est pas fusionné** : les backends lisent `d.samples`
> directement. Les deux constats sont exacts ; c'est la **date et la branche** qui les départagent.
> Cette page décrit `main`. Quand la fusion aura lieu, ce paragraphe est à réviser.

Sur `main` au 24/08/2026, ce qui est dérivé du render pass, ce sont les **formats RTV/DSV**, jamais
le nombre d'échantillons. Mesuré trois fois indépendamment sur cet arbre.

| Backend | D'où vient le nombre d'échantillons | Preuve |
|---|---|---|
| **Vulkan** | `d.samples` — **pas** le render pass | `NkVulkanDevice.cpp:1735`. Le render pass *est* résolu dans la même fonction (`:1620`) mais n'est utilisé qu'à `:1811` pour `gpci.renderPass`. `rpit->desc.colorAttachments[i].samples` **n'est jamais lu** dans `CreateGraphicsPipeline` (`:1612-1823`) |
| **DX12** | `d.samples` | `NkDirectX12Device.cpp:1772`, dans `BuildGraphicsPSO` (`:1684`). Ce qui vient du render pass, c'est `RenderPassFormats()` (`:1640`) → **formats seulement** ; le nombre d'échantillons n'entre même pas dans `FmtSignature()` (`:1666`) |
| **Metal** | `d.samples` | `NkMetalDevice.mm:736`, posé **avant** la recherche du render pass (`:756`), qui ne sert qu'au `pixelFormat` |
| **OpenGL** | **ignoré** | 0 occurrence de `sample` dans `CreateGraphicsPipeline` (`:2106-2380`) |
| **DX11** | **ignoré** | l'unique occurrence (`:1331`) est `d.rasterizer.multisampleEnable`, un **autre champ** |
| **Software** | **ignoré** | 0 occurrence de `samples` dans tout `Software/` |

**Pourquoi personne ne s'en aperçoit** : `grep -rn "\.samples[ ]*="` → 18 lignes, dont **une seule**
touche un `NkGraphicsPipelineDesc` — `Tools/Grid3D/NkGrid3D.cpp:114`, qui pose la valeur **par
défaut** `NK_S1`. Le champ vaut donc `NK_S1` à tous les sites d'appel du dépôt. La divergence est
**latente** : elle n'apparaîtra qu'au premier pipeline MSAA.

> **Ce qu'il faut en retenir.** Le principe est juste — *le nombre d'échantillons d'un pipeline
> n'est pas un axe libre, c'est une fonction de la passe* — et il est **déjà appliqué sur
> `feat/rendu-temps-reel`** ; simplement **pas encore sur `main`**.
> Tant que `samples` reste un champ libre du desc à côté de `renderPass` (`NkDescs.h:684-685`), rien
> n'empêche de créer un pipeline dont le compte d'échantillons contredit sa passe. Un pipeline
> incohérent est un `VK_ERROR` à la création côté Vulkan, un PSO invalide côté DX12.

---

## 6. Requêtes et timestamps : **la capacité ment**

Recherche à 16 termes sur tout le module (`VkQueryPool`, `vkCmdWriteTimestamp`, `ID3D12QueryHeap`,
`D3D12_QUERY_TYPE_TIMESTAMP`, `MTLCounterSampleBuffer`, `D3D11_QUERY_TIMESTAMP`, `glQueryCounter`…) :
**16 occurrences, et aucun backend ne sait rendre un timestamp GPU.**

- Les 4 méthodes device (`NkIDevice.h:355-365`) ont **zéro override**.
- `WriteTimestamp` a **un** override, OpenGL (`NkOpenglCommandBuffer.h:452`) — et il ne peut rien
  produire : il passe **l'index de l'appelant** comme nom d'objet de requête GL, alors qu'aucun
  `glGenQueries` ne l'a créé, et **aucun code n'appelle `glGetQueryObject*`**. Sur `NK_OPENGL_ES` il
  se réduit à `(void)idx;`.

🔴 **Et pourtant `NkDeviceCaps::timestampQueries` (`NkIDevice.h:80`) est mis à `true` par quatre
backends** : OpenGL (`:1034`), DX12 (`:3022`), Metal (`.mm:1113`), Vulkan sous condition (`:2473`).
Un appelant qui teste `GetCaps().timestampQueries` s'entend répondre « oui » par quatre backends,
puis reçoit `false` de `GetTimestampResults` sur les six.

**C'est la racine d'un défaut visible ailleurs** : `NkRendererStats::gpuTimeMs` reste à zéro
(cf. [NKRenderer/Frame-Contract.md](../NKRenderer/Frame-Contract.md)) — non par oubli, mais parce
qu'aucune requête de timestamp n'existe sous lui.

---

## 7. Capacités : couverture très inégale

`GetCaps()` est pure, donc les six renvoient un vrai `mCaps` — mais sur **54 champs déclarés**
(`NkIDevice.h:40-96`), le nombre réellement renseigné (`grep -c 'mCaps\.'`) varie du simple au
quadruple :

| Vulkan | OpenGL | DX12 | Metal | DX11 | Software |
|---:|---:|---:|---:|---:|---:|
| 31 | 27 | 19 | 17 | 15 | **8** |

- 🔴 **`vramBytes`** : posé par **Vulkan seul** (`:2480`). Les consommateurs
  (`NkDeviceFactory.cpp:132`, `NkML.cpp:37`, `GetContextInfo().vramMB`) voient **0** sur les cinq
  autres — et `NkML.cpp:37` en conclut qu'ils **ne sont pas des GPU**.
- 🔴 **Drapeaux MSAA** : `msaa2x/4x/8x` sont **codés en dur à `true`** par DX11 (`:1754`), DX12
  (`:3021`), Metal (`.mm:1114`) et Vulkan (`:2475`) — **aucun n'interroge le matériel**. Seul OpenGL
  les mesure réellement (`:1040-1043`). `GetContextInfo().maxMSAASamples` annonce donc 8 quel que
  soit le GPU, et 1 sur Software.
- **`bindlessTextures`, `rayTracing`, `meshShaders`, `variableRateShading`** : jamais assignés par
  aucun backend → `false` partout.

> ⚠️ **Conséquence de maintenance.** `GetCaps()` n'est pas une source de vérité. Quatre de ses
> champs les plus utiles sont soit codés en dur, soit renseignés par un seul backend, soit vrais
> alors que la fonction sous-jacente n'existe pas. **Ne gatez pas une fonctionnalité sur `GetCaps()`
> sans vérifier ici que le backend visé l'honore.**

[← Doc NKRHI](README.md) · [← Contrat de frame NKRenderer](../NKRenderer/Frame-Contract.md) · [← Couche Runtime](../README.md)
