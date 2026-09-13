# Les 25 capacités `A-DATER` — arbitrées le 24/08 : « tout implémenter »

> **À qui c'est destiné.** À Rodolf, et à personne d'autre. Les 25 lignes classées
> `dette-datee | A-DATER` dans `config/capacites.list` portent une échéance qui
> **lui appartient** : le vérificateur les nomme à chaque passage et refuse
> d'inventer une date à sa place. Ce document existe parce que « les 24 dates »
> ne voulait rien dire tant que personne n'avait dit **ce que chacune promet** et
> **ce qu'on perd à l'abandonner**.
>
> **Comment s'en servir.** Le document est trié par la **quatrième colonne** —
> *si on l'abandonne* — du plus facile au plus difficile. Les **groupes A à C**
> (13 lignes) se tranchent d'affilée par oui/non. Les **groupes D à F**
> (12 lignes) demandent un vrai arbitrage, et c'est là qu'il faut réfléchir.
> Chaque ligne porte une **recommandation** : la réponse attendue est
> « oui » ou « non », jamais une date inventée depuis rien.

---

## Ce que ce document est, et ce qu'il n'est pas

| | |
|---|---|
| **il est** | un relevé d'appelants **comptés un par un** dans l'arbre, à la commande, avec fichier et ligne |
| **il n'est pas** | un jugement sur la valeur produit d'une capacité. Cette part est à Rodolf, elle n'est pas mesurable ici. |

⚠️ **Je n'ai daté aucune ligne.** *Dater à la place de Rodolf serait produire un
chiffre crédible et faux — précisément ce qu'on traque.*

⚠️ **Le plafond du cliquet, lui, a bougé — et il faut le lire ici plutôt que dans
un diff.** Le 23/08 il valait `# PLAFOND-A-DATER = 24` et je n'y avais pas touché.
**Le 24/08 je l'ai relevé à 25**, délibérément, parce que la correction d'un défaut
de mon propre détecteur a fait apparaître quinze capacités qu'il ne voyait pas —
dont une que la règle (a) interdit de classer autrement. **Le raisonnement entier
est en bas de ce document**, section « Pourquoi elles sont 25 et non plus 24 ».
*Un contrôle qui modifie sa propre donnée sans que personne ne le voie a cessé
d'être un contrôle : c'est pour ça que ce paragraphe existe.*

**Ce que j'ai changé dans `config/capacites.list`, et rien d'autre : les notes
des 24 lignes.** Elles portaient `non-examine` — la mention qui existe
précisément pour qu'on ne prenne pas un remplissage de premier jour pour un
jugement. Je les ai examinées : **laisser le mot serait une fausse déclaration
dans le fichier dont le métier est de ne pas en porter**, et la politique du
fichier demande explicitement à l'agent qui passe de corriger sa ligne. Chaque
note dit maintenant le compte d'appelants, ce qui casse, et la recommandation.
**Au 23/08, aucune classe, aucune date, aucun plafond n'avait bougé** : les 24
restaient `dette-datee | A-DATER`, `verif_capacites.sh` rendait `0`, et le cliquet
lisait 24. *(Mesure : `non-examine` passe de 117 à 102 lignes.)*
**Au 24/08 : une classe a bougé** (`NkRendererConfig::vsync`, refusée en
`vision-assumee` par le détecteur lui-même) **et le plafond avec elle, 24 → 25.**
Onze lignes se sont ajoutées au fichier, dont dix en `vision-assumee`.
`verif_capacites.sh` rend toujours `0`. *(Mesure : `non-examine` remonte à 111 —
les nouvelles ne sont pas examinées une par une, et le mot le dit.)*

### La référence de la mesure, et son retard

```
branche : feat/verificateur   HEAD b381b605
main    : 9b31cc8f (ConquerorLab + NK3DModeler, poussée aujourd'hui)
retard  : 0 commit
```

⚠️ La branche était **21 commits derrière `main`** au début de ce travail.
Compter des appelants sur cette photo aurait produit des chiffres **crédibles et
faux** — ma propre leçon, appliquée à moi-même. J'ai fusionné `main`, **puis**
mesuré. *(Contre-vérifié : les 17 comptes bruts sont identiques avant et après
la fusion. Mais je ne pouvais pas le **savoir** avant de fusionner, et c'est
exactement le point.)*

### Périmètre du comptage

Comptés : `Kernel/`, `Engine/`, `Applications/`, `Sandbox/`, `Spark/`,
`Integrations/`, `Tools/`, extensions `.h .hpp .cpp .inl`.
**Exclus** : `Build/_jenga-embed/` (copie embarquée de Jenga) et les fichiers
`* copy.*`, dont `preuve_copies_mortes.sh` établit qu'**aucun build ne les
compile**. Une écriture dans `NkOpenglDevice copy.hpp` **n'est pas** une
écriture : la compter aurait gonflé sept des lignes ci-dessous.

⚠️ **Homonymes écartés à la main.** Sept des noms suivis désignent plusieurs
symboles distincts. Les comptes ci-dessous portent **uniquement** sur le symbole
de `config/capacites.list` :

| nom | le symbole suivi | les homonymes écartés |
|---|---|---|
| `msaaSamples` | `NkRendererConfig::msaaSamples` | `NkContextDesc`, `NkOpenGLDesc`, `NKCanvas/NkContextDesc`, `Noge::NkCamera` — **tous honorés**, eux |
| `vsync` | `NkSwapchainDesc::vsync` | `NkContextDesc::vsync` (lu par DX11/DX12), `NkRendererConfig::vsync` (écrit à l'exécution) |
| `blendEnable` | `NkBlendAttachment::blendEnable` | les états internes du backend Software (`NkSoftwareDevice.h`, `NkSWFastPath.h`, `NkSWRasterCore.h`) et les `VkPipelineColorBlendAttachmentState` |
| `geometryShaders` | `NkDeviceCaps::geometryShaders` | `NkSLTargetCaps::geometryShaders` (`NkSLFeatures.h:34`) |
| `multiViewport` | `NkDeviceCaps::multiViewport` | une variable locale `NkUIMultiViewportManager multiViewport` dans `Applications/Sandbox/.../Base04/amodifier.h` |
| `ssr` | `NkPostConfig::ssr` | `NkDeferredPass.h:42` (champ homonyme, **lui aussi jamais lu**) |
| `dof` | `NkPostConfig::dof` | un `float *dof` local dans `Applications/NKRebasinTransformer/src/main.cpp` |

---

# Groupe A — le détecteur s'est trompé (1 ligne)

**Rien à dater. La bonne réponse est de corriger le classement.**

## A1 · `NkBlendAttachment::blendEnable`

| | |
|---|---|
| **quoi** | « Ce plan de couleur mélange-t-il sa sortie avec ce qui est déjà à l'écran, au lieu de l'écraser ? » — le drapeau qui fait la différence entre une vitre et un mur. |
| **où** | `Kernel/Runtime/NKRHI/src/NKRHI/Core/NkDescs.h:374`. Points d'entrée : les préréglages `NkBlendAttachment::Alpha()` (:389), `PreMultAlpha()` (:399) et le troisième (:409). |
| **qui s'en sert** | **6 lectures réelles, sur 5 backends** : `NkDirectX11Device.cpp:1357`, `NkDirectX12Device.cpp:1761`, `NkOpenglDeviceInternal.cpp:283` (branche GL ES) **et** `:296` (branche desktop), `NkSoftwareDevice.cpp:2163`, `NkVulkanDevice.cpp:1749`. |
| **si on l'abandonne** | ⚠️ **La transparence disparaît sur les cinq backends.** Toute l'interface, tout le texte, toutes les particules deviennent opaques. |

**Pourquoi il était dans le stock.** D2 cherche la voie d'accès à partir d'une
déclaration de type (`NkBlendAttachment a;`). Les cinq backends écrivent
`auto &a = d.blend.attachments[i];` — **le type n'apparaît pas dans la ligne**.
Le détecteur n'a pas vu la voie, donc pas les lectures, et a conclu
« écrit-jamais-lu ». C'est la **quatrième fois** de ce chantier qu'un instrument
accuse le sujet à la place de son propre montage.

> 📌 **Recommandation : `implementee`** — reclasser, avec en note les six lieux
> de lecture ci-dessus. Sort du stock `A-DATER` : **24 → 23**, dans le sens
> autorisé par le cliquet.

---

# Groupe B — personne n'appelle, l'abandon ne casse rien (7 lignes)

**Sept « rien ». Sept oui/non d'affilée.**

## B1 · `NkDescriptorSetLayoutDesc::isBindless`

| | |
|---|---|
| **quoi** | « Ce jeu de ressources est-il un grand tableau que le shader indexe librement (*bindless*), plutôt qu'une liste fixe ? » |
| **où** | `NkDescs.h:763`. Point d'entrée : `NkDescriptorSetLayoutDesc::AddBindless()` (:773). |
| **qui s'en sert** | **0 lecture. 1 écriture**, à l'intérieur d'`AddBindless()` — et **`AddBindless()` n'a aucun appelant dans tout l'arbre.** Toute la chaîne bindless est inatteignable. |
| **si on l'abandonne** | **Rien.** Aucun code ne construit de layout bindless. |

> 📌 **Recommandation : `vision-assumee`** — il n'y a pas de drapeau à `true` dans
> du code exécuté (l'écriture vit dans une fonction que personne n'appelle), donc
> la ligne de partage R3-a autorise ce classement. Le bindless est une ambition
> NKRHI, pas une dette.

## B2 · `NkRenderPassDesc::hasResolve`

| | |
|---|---|
| **quoi** | « À la fin de cette passe, faut-il fondre les N échantillons MSAA en une seule image lisible ? » — l'étape sans laquelle le MSAA ne se voit pas. |
| **où** | `NkDescs.h:600`. Point d'entrée : `NkRenderPassDesc::SetResolve()` (:620), qui écrit aussi `resolveAttachment` (:601). |
| **qui s'en sert** | **0 lecture. 1 écriture**, dans `SetResolve()` — et **`SetResolve()` n'a aucun appelant.** |
| **si on l'abandonne** | **Rien ne casse aujourd'hui**, parce que rien ne l'utilise. ⚠️ Mais l'abandonner, c'est **écrire noir sur blanc que NKRHI ne sait pas résoudre un MSAA** — ce qui est déjà vrai, et se lit mieux dit que tu. |

> 📌 **Recommandation : `dette-datee`, à dater** — c'est la seule des sept où
> « rien ne casse » cache une capacité qui **manque** au lieu d'une qui est de
> trop. À rapprocher de E1 (`msaaSamples`) et du travail MSAA du chantier rendu :
> **même sujet, trancher les deux ensemble.**

## B3 · `NkSwapchainDesc::vsync`

| | |
|---|---|
| **quoi** | « La présentation attend-elle le balayage de l'écran ? » — pour un swapchain créé **explicitement par l'application** (cas multi-fenêtre). |
| **où** | `NkDescs.h:878`. Point d'entrée : `NkIDevice::CreateSwapchain()` (`NkIDevice.h:474`). |
| **qui s'en sert** | **0 lecture, 0 écriture.** Et le point d'entrée est mort : `CreateSwapchain()` **retourne `nullptr` en dur**, a **0 surcharge** et **0 appelant**. |
| **si on l'abandonne** | **Rien.** |

⚠️ **Et il y a pire que « jamais lu » ici.** `NkSwapchainDesc` est **déclarée
deux fois** dans le même `namespace nkentseu`, avec des défauts **différents** :

| | `NkDescs.h:871` | `NkISwapchain.h:25` |
|---|---|---|
| `imageCount` | **3** | **2** |
| `colorFormat` | `NK_BGRA8_UNORM` | `NK_RGBA8_SRGB` |
| `samples` | présent | **absent** |

Ce serait une erreur de compilation (`NkISwapchain.h` inclut `NkDescs.h`) — sauf
que **`NkISwapchain.h` n'est inclus par personne** : son unique `#include`, dans
`NkRHI.h:11`, est **en commentaire**. Le fichier ne compile jamais, donc le
conflit ne se voit jamais. **Le jour où quelqu'un décommente cette ligne, il ne
tombera pas sur un bug de vsync : il tombera sur une redéfinition.**

> 📌 **Recommandation : `vision-assumee`** pour le champ, **et ouvrir une ligne
> à part pour la double déclaration**, qui est un défaut d'un autre genre et
> d'une autre gravité. Le multi-fenêtre RHI est une ambition non écrite ; on
> l'assume.

## B4 · `NkIBLConfig::enabled`

| | |
|---|---|
| **quoi** | « L'éclairage venu de l'environnement (ciel, HDR) est-il actif ? » — l'interrupteur général de l'IBL. |
| **où** | `Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkRendererConfig.h:255`. |
| **qui s'en sert** | **0 lecture, 0 écriture. Aucune occurrence de `.ibl.enabled` dans tout l'arbre.** ⚠️ Et le contraste est net : **les onze autres champs** de `NkIBLConfig` sont lus par `NkRendererImpl.cpp:355-380` (`iblStrength`, `drawSkybox`, `irradianceMapSize`, `specularMapSize`, `prefilterMipCount`, `brdfLUTSize`, `useHDR`, `hdrPath`, `skyTop`, `horizon`, `ground`). **`enabled` est le seul trou de la structure.** |
| **si on l'abandonne** | **Rien.** Mettre `ibl.enabled = false` ne fait rien aujourd'hui — l'IBL tourne quand même. |

> 📌 **Recommandation : `dette-datee`, à dater — et l'échéance peut être courte.**
> C'est **un `if` dans `NkRendererImpl`**, dans une fonction qui lit déjà les onze
> voisins. Le coût de l'implémentation est plus faible que celui de la discussion.

## B5 · `NkPostConfig::dof`

| | |
|---|---|
| **quoi** | « La profondeur de champ est-elle active ? » — le flou photographique de ce qui n'est pas dans le plan de mise au point. |
| **où** | `NkRendererConfig.h:193`. Écrit par le préréglage `ForCinematic()` (:469). |
| **qui s'en sert** | **0 lecture. 1 écriture**, dans `ForCinematic()` — et **`ForCinematic()` n'a aucun appelant** (les préréglages réellement appelés sont `ForEditor` → NK3DModeler, et `ForArchviz` → `Sandbox/.../MultiStyle.cpp:263`). |
| **si on l'abandonne** | **Rien à l'exécution.** ⚠️ Mais il faut assumer que **`Shaders/PostProcess/DOF/` existe sur disque** et n'est monté par aucune passe : `NkPostProcessStack.cpp` ne construit que Tonemap, Bloom, SSAO, SSAOBlur, AutoExposure, TAA, FXAA. |

> 📌 **Recommandation : `vision-assumee`** — aucun code exécuté ne met ce drapeau
> à `true`, la ligne de partage R3-a l'autorise. **Et écrire dans la note que les
> shaders DOF sont des fichiers non montés**, pour que le prochain qui les trouve
> ne croie pas la fonctionnalité livrée.

## B6 · `NkPostConfig::motionBlur`

| | |
|---|---|
| **quoi** | « Les objets rapides laissent-ils une traînée, comme sur une photo à obturateur lent ? » |
| **où** | `NkRendererConfig.h:197` (avec `motionBlurShutter` :198). Écrit par `ForCinematic()` (:470). |
| **qui s'en sert** | **0 lecture. 1 écriture**, dans `ForCinematic()`, **sans appelant**. |
| **si on l'abandonne** | **Rien à l'exécution.** Même réserve qu'en B5 : `Shaders/PostProcess/MotionBlur/` contient **des shaders GL *et* VK écrits**, versionnés en double (`Kernel/…` et `Resources/…`), que **rien ne compile**. |

> 📌 **Recommandation : `vision-assumee`**, avec la même note sur les shaders
> orphelins.

## B7 · `NkDeviceCaps::logicOp`

| | |
|---|---|
| **quoi** | « La carte sait-elle combiner la couleur écrite et la couleur en place par une opération **logique** (XOR, AND…) au lieu d'un mélange arithmétique ? » |
| **où** | `Kernel/Runtime/NKRHI/src/NKRHI/Core/NkIDevice.h:84`. |
| **qui s'en sert** | **0 lecture. 2 écritures** : `NkDirectX11Device.cpp:1746` (`true` en dur), `NkVulkanDevice.cpp:2469` (`feats.logicOp`, requête réelle). DX12, OpenGL et Software ne l'écrivent pas → ils rapportent `false` par défaut. |
| **si on l'abandonne** | **Rien.** ⚠️ Et c'est le seul des drapeaux `NkDeviceCaps` dont la capacité **n'est même pas exprimable** : il n'existe **aucun** `NkLogicOp`, aucun champ d'opération logique dans `NkBlendDesc`, nulle part dans NKRHI. Le drapeau annonce une possibilité que **l'API ne sait pas demander**. |

> 📌 **Recommandation : `abandonner` (le champ part, ou passe à `false` partout).**
> *EXPRIMABLE n'est pas HONORABLE* — ici c'est le degré au-dessous : **ce n'est
> même pas exprimable.** Un rapport de capacité qui répond à une question que
> l'API ne pose pas est du bruit.

---

# Groupe C — jamais lu, mais quelqu'un écrit pour de vrai (4 lignes)

**La différence avec le groupe B : ici, du code livré règle ce champ et croit
obtenir quelque chose.** Rien ne casse à l'abandon, mais quelqu'un sera détrompé.

## C1 · `NkRendererConfig::debugOverlay`

| | |
|---|---|
| **quoi** | « Afficher le bandeau de diagnostic par-dessus le rendu (FPS, compteurs, passes). » |
| **où** | `NkRendererConfig.h:413`. Écrit par le préréglage `ForEditor()` (:534). |
| **qui s'en sert** | **0 lecture.** **1 écriture — et son appelant est réel** : `Applications/NK3DModeler/src/NK3DModeler/Viewport/NkViewport3D.cpp:726` appelle `NkRendererConfig::ForEditor(...)`. **NK3DModeler demande donc un overlay de débogage à chaque lancement, et ne l'obtient jamais.** Aucun sous-système d'overlay n'existe côté NKRenderer. |
| **si on l'abandonne** | **Rien ne casse.** NK3DModeler continue exactement comme aujourd'hui — il ne perd rien, puisqu'il n'a jamais rien reçu. |

> 📌 **Recommandation : `abandonner` (retirer le champ et la ligne de `ForEditor`).**
> Un préréglage « éditeur » qui coche une case sans effet est une promesse faite
> au lecteur du code. Si l'overlay revient un jour, il reviendra avec son
> sous-système et son propre drapeau.

## C2 · `NkRendererConfig::voxelAOEnabled`

| | |
|---|---|
| **quoi** | « Allouer le système d'occlusion ambiante par voxels ? » — le champ dont le commentaire promet, textuellement : *« false = sous-système NON alloué (gratuit) et `GetVoxelAO()` renvoie `nullptr` »*. |
| **où** | `NkRendererConfig.h:404` (commentaire du contrat aux :400-403). |
| **qui s'en sert** | **0 lecture.** **2 écritures, toutes deux dans du code livré** : `Applications/NkAnimaEditor/src/NkAnimaEditor/AnimBridge.cpp:691` (`= false`) et `Applications/Sandbox/src/Demo/main.cpp:214` (`= true`). |
| **si on l'abandonne** | **Rien ne casse** — mais ⚠️ **il faut réparer le commentaire dans le même geste.** `NkRendererImpl.cpp:226` alloue `NkVoxelAOSystem` **inconditionnellement** dès que le bloc de sous-systèmes concerné est actif. **NkAnimaEditor demande explicitement de ne pas payer ce coût, et le paie.** Le contrat écrit dans l'en-tête est **faux aujourd'hui**, indépendamment de ce que tu décides pour le champ. |

> 📌 **Recommandation : `dette-datee`, à dater.** C'est un `if` d'une ligne autour
> de `NkRendererImpl.cpp:226`, et c'est le **seul** des 24 où un consommateur
> **demande une économie et paie quand même**. À défaut de l'implémenter :
> **supprimer le commentaire de contrat le jour même**, parce qu'une supposition
> consignée comme fait se propage avec l'autorité d'un fait.

## C3 · `NkPostConfig::ssr`

| | |
|---|---|
| **quoi** | « Les surfaces réfléchissantes renvoient-elles l'image de la scène elle-même (réflexions en espace écran) ? » |
| **où** | `NkRendererConfig.h:246`. |
| **qui s'en sert** | **0 lecture. 6 écritures**, dont **4 dans du code exécuté** : `Sandbox/src/DemoNkentseu/Base06/MultiStyle.cpp:274` (`= true`) et `Sandbox/src/Demo/main.cpp:342, :371, :390` (`= false` × 3) ; plus 2 dans les préréglages `ForCinematic()` (:472, sans appelant) et `ForArchviz()` (:489, **appelé** par `MultiStyle.cpp:263`). |
| **si on l'abandonne** | **Rien ne casse.** ⚠️ Mais c'est la ligne du groupe où l'abandon coûte le plus en **travail déjà fait** : `Shaders/PostProcess/SSR/` contient **`ssr.frag.gl.glsl` et `ssr.frag.vk.glsl` écrits et versionnés en double** — et aucune passe C++ ne les monte. Le champ homonyme `NkDeferredPass.h:42` n'est **pas lu non plus**. |

> 📌 **Recommandation : `dette-datee`, à dater.** Trois consommateurs le règlent
> déjà, les shaders des deux backends existent : ce qui manque est la passe C++,
> pas l'algorithme. C'est le plus « avancé » des trois post-traitements creux
> (SSR / DOF / motion blur) — si tu n'en dates qu'un, date celui-ci.

## C5 · `NkRendererConfig::vsync` — **la 25e, et elle n'existait pas hier**

| | |
|---|---|
| **quoi** | « Le rendu attend-il le balayage de l'écran avant de présenter une image ? » — la synchronisation verticale, réglée depuis la config du *renderer*. |
| **où** | `NkRendererConfig.h:369`. |
| **qui s'en sert** | **0 lecture. 24 écritures**, dont **six écrivent `true` par la voie `rcfg`** dans du code livré : `Sandbox/src/DemoNkentseu/Base01/` `main9.cpp:178`, `main11.cpp:73`, `main12.cpp:72`, `main13.cpp:72`, `main14.cpp:72`, `main15.cpp:70`. Et `NkRendererImpl.cpp:1643` l'**écrit** aussi (`mCfg.vsync = e`) sans que personne ne le relise. |
| **si on l'abandonne** | **Rien ne casse.** Les six démos continuent exactement comme aujourd'hui : elles n'ont jamais rien reçu. |

⚠️ **Les homonymes sont nombreux, et EUX sont bien lus.** `dxCfg.vsync`
(`NkDirectX11Device.cpp:123`, `NkDirectX12Device.cpp:110`), `desc.dx11/dx12.vsync`
(NKCanvas), `context.config.vsync` (`NkContext.cpp:1395`), `winCfg.vsync` et
`contextConfig.vsync` (`NkWindowConfig`, `NkContextDesc`). **La vsync du moteur
fonctionne — par ces chemins-là.** Celui de `NkRendererConfig` est le seul qui ne
mène nulle part.

> 📌 **Recommandation : `abandonner`, ou honorer en même temps que
> `NkRendererConfig::msaaSamples` (E1) et `NkRendererConfig::pipeline`.** Même
> structure, même silence : trois champs de la config du *renderer* que du code
> livré règle et que le *renderer* ne lit pas.

⚠️ **Pourquoi cette ligne ne pouvait pas être classée `vision-assumee`.** La règle (a)
l'interdit dès qu'un drapeau annonce `true` à du code, et **le détecteur a refusé
le classement** que je lui avais donné. Il avait raison : six fichiers livrés
écrivent `rcfg.vsync = true`. Les deux seules autres sorties étaient
`implementee` (faux) et `morte-a-retirer` (une décision produit qui n'est pas la
mienne). D'où le **+1 sur le cliquet**, expliqué ci-dessous.

## C4 · `NkDeviceCaps::shaderFloat16`

| | |
|---|---|
| **quoi** | « Les shaders peuvent-ils calculer en demi-précision (`half`, 16 bits) ? » — moitié moins de bande passante mémoire, pour de l'IA ou du post-traitement. |
| **où** | `NkIDevice.h:76`. |
| **qui s'en sert** | **0 lecture. 1 écriture** : `NkVulkanDevice.cpp:2474`, `mCaps.shaderFloat16 = true; // approximation`. Aucun autre backend ne l'écrit → tous rapportent `false`. |
| **si on l'abandonne** | **Rien ne casse aujourd'hui.** ⚠️ **Mais c'est le drapeau le plus proche de mentir des 24 après le chronométrage.** Le commentaire `// approximation` dit lui-même que **la valeur n'est pas mesurée** : le pilote n'est jamais interrogé (pas de `VK_KHR_shader_float16_int8`). Or NKSL **sait émettre du `half`** (`NkSLTypes.h:297-300` : GLSL→`float16_t`, HLSL→`half`, MSL→`half`). Le jour où un appelant consultera ce drapeau sur Vulkan, il recevra `true` **sans qu'on ait vérifié quoi que ce soit** — et `false` sur DX11/DX12, où HLSL supporte pourtant `half`. **Faux dans les deux sens.** |

> 📌 **Recommandation : `abandonner` — passer à `false` en retirant la ligne
> Vulkan.** *Une supposition consignée comme mesure se propage avec l'autorité
> d'une mesure.* Une capacité **non interrogée** vaut `false`. Le jour où NKSL
> aura besoin du `half`, on écrira la vraie requête, sur les cinq backends.

---

# Groupe D — le drapeau n'est jamais lu, mais la capacité existe (6 lignes)

**⚠️ Lire ce paragraphe avant les six lignes — il change ce que « abandonner »
veut dire.**

`NkDeviceCaps` **n'est pas une structure morte** : `GetCaps()` compte
**29 sites d'appel réels, dans 15 fichiers**, dont **17 décident** d'un chemin
de code et 12 ne font que journaliser la VRAM. Les champs qui décident :
`computeShaders` (`NkComputeContext.cpp:19`, `NkML.cpp:22`, `NkTensorGpu.cpp:293`,
`NkGpuProbe:33`, `NkComputeNkSL:55`), `maxComputeGroupSizeX/Y/Z` et
`maxComputeSharedMemory` (`NkComputeContext.cpp:406-427`), `indirectDispatch`
(`NkComputeContext.cpp:402`), `vramBytes` et `maxTextureDim2D`
(`NkIDevice::GetContextInfo`, `NkIDevice.h:208`).

⚠️ **Correction de mon propre chiffre, faite avant publication.** J'avais écrit
« 45 fois ». **45 est le résultat brut d'un `grep GetCaps()`** — il comptait
`NkMeshSystem::GetCapsule`, le `FnGetCaps` XInput de `NkWin32Gamepad.h`, et des
fichiers `* copy` qu'aucun build ne compile. **Un compte brut présenté comme une
mesure est exactement ce que ce chantier traque**, et il était sur le point de
sortir sous ma signature. Le nombre juste est **29**, et il reste plus que
suffisant pour l'argument.

C'est **un rapport de capacités vivant**, dont ces six champs sont la portion que
**personne n'a encore eu besoin de consulter**. La conséquence est directe :

> **Passer ces six drapeaux à `false` ferait mentir le rapport dans l'autre sens.**
> `false` veut dire « la carte ne sait pas faire », et pour cinq d'entre eux c'est
> faux. Ce n'est pas une sortie honnête ; c'en est une deuxième.

**La vraie question, pour les six, est une seule : `NkDeviceCaps` est-il une API
publique que les applications ont le droit de lire ?**

- **Si OUI** — et les 29 appels le suggèrent — alors ces six champs sont
  **`implementee`** : ils rapportent honnêtement, le moteur ne les consulte pas
  parce qu'il n'en a pas besoin, et il n'y a **aucune dette**. **Une seule
  réponse « oui » retire six lignes du stock : 24 → 18.**
- **Si NON**, ce sont six champs de décoration à supprimer.

Elles restent listées individuellement ci-dessous parce que **la première ne se
range pas avec les cinq autres**.

## D1 · `NkDeviceCaps::tessellationShaders`

| | |
|---|---|
| **quoi** | « La carte sait-elle subdiviser les triangles à la volée sur le GPU ? » (terrains, surfaces lisses) |
| **où** | `NkIDevice.h:63`. |
| **qui s'en sert** | **0 lecture. 4 écritures** : DX11 `:1740` (`true`), DX12 `:3005` (`true`), OpenGL `:1030` (`true`), Vulkan `:2463` (`feats.tessellationShader`, requête réelle). |
| **si on l'abandonne** | ⚠️ **Rien — et c'est la ligne du groupe D où c'est vrai.** Les étages `NK_TESS_CTRL` / `NK_TESS_EVAL` existent bien (GL `:3237`, `:3239` ; VK `:1648`, `:2085`, `:2838`) et `patchControlPoints` est honoré par OpenGL (`NkOpenglDeviceInternal.cpp:315`) — **mais il n'existe pas un seul fichier `.tesc` ou `.tese` dans tout l'arbre.** Aucune tessellation n'est écrite nulle part. |

> 📌 **Recommandation : `vision-assumee`** — la seule des six à sortir du groupe.
> Trois backends annoncent `true` en dur pour une capacité que **rien n'utilise
> et que rien n'est écrit pour utiliser**.

## D2 · `NkDeviceCaps::geometryShaders`

| | |
|---|---|
| **quoi** | « La carte sait-elle créer de la géométrie à la volée entre le vertex et le fragment ? » (points → quads pour les particules) |
| **où** | `NkIDevice.h:64`. ⚠️ Ne pas confondre avec `NkSLTargetCaps::geometryShaders` (`NkSLFeatures.h:34`) — voir la note en fin de document. |
| **qui s'en sert** | **0 lecture. 4 écritures** : DX11 `:1741`, DX12 `:3006`, OpenGL `:1029` (`true` en dur), Vulkan `:2464` (`feats.geometryShader`, requête réelle). |
| **si on l'abandonne** | **Rien ne casse aujourd'hui.** ⚠️ Nuance mesurée : le chemin est **implémenté de bout en bout** — `NkShaderLibrary.cpp:325-337` compile bien un étage `NK_GEOMETRY` — **mais son unique porte d'entrée, `NkShaderLibrary::LoadVGF()`, n'a aucun appelant.** Les **cinq** `particles.geom.*` (DX11, DX12, GL ×2, VK) sont donc des **fichiers écrits que rien ne charge**. |

> 📌 **Recommandation : `implementee`** si `NkDeviceCaps` est une API publique
> (réponse commune du groupe D). Le drapeau, lui, dit vrai.

## D3 · `NkDeviceCaps::drawIndirect`

| | |
|---|---|
| **quoi** | « Le GPU peut-il lancer ses propres dessins depuis un tampon qu'il a lui-même rempli, sans repasser par le CPU ? » |
| **où** | `NkIDevice.h:67`. |
| **qui s'en sert** | **0 lecture. 5 écritures** : DX11 `:1743`, DX12 `:3008`, OpenGL `:1031` (`true`), Software `:732` (**`false`** — honnête), Vulkan `:2466` (`true`). |
| **si on l'abandonne** | **Rien ne casse.** `DrawIndirect` / `DrawIndexedIndirect` sont **réellement implémentés** (`NkICommandBuffer.h:173-182` + DX11, DX12 — avec un `TODO` sur la signature de commande — et Vulkan), mais **aucun appelant dans tout l'arbre**. ⚠️ Comparaison utile : le champ **voisin** `indirectDispatch` est, lui, **lu** (`NkComputeContext.cpp:402`). Même famille, même structure : l'un sert, l'autre pas encore. |

> 📌 **Recommandation : `implementee`** (réponse commune du groupe D).

## D4 · `NkDeviceCaps::multiViewport`

| | |
|---|---|
| **quoi** | « Peut-on dessiner dans plusieurs rectangles d'écran en une seule passe ? » (vues multiples d'éditeur, VR à deux yeux) |
| **où** | `NkIDevice.h:82`. |
| **qui s'en sert** | **0 lecture. 5 écritures** : DX11 `:1744`, DX12 `:3009`, OpenGL `:1032`, Software `:733` (**`false`**), Vulkan `:2467` (`feats.multiViewport`). |
| **si on l'abandonne** | **Rien ne casse.** `SetViewports(vps, n)` existe sur DX11 (`:132`), DX12 (`:172`) et Metal — **0 appelant**. |

> 📌 **Recommandation : `implementee`** (réponse commune du groupe D).

## D5 · `NkDeviceCaps::independentBlend`

| | |
|---|---|
| **quoi** | « Chaque cible de rendu peut-elle avoir son propre réglage de mélange, ou toutes partagent-elles le même ? » |
| **où** | `NkIDevice.h:83`. |
| **qui s'en sert** | **0 lecture. 5 écritures** : DX11 `:1745`, DX12 `:3010`, OpenGL `:1033`, Software `:734` (**`true`**), Vulkan `:2468` (`feats.independentBlend`). |
| **si on l'abandonne** | ⚠️ **Rien ne casse — et c'est la ligne du groupe où le drapeau est le plus clairement inutile.** DX11 **et** DX12 **utilisent déjà** la capacité **sans la consulter** : `NkDirectX11Device.cpp:1356` et `NkDirectX12Device.cpp:1758` écrivent tous deux `IndependentBlendEnable = d.blend.attachments.Size() > 1;` en dur. Vulkan construit de même un tableau de mélange par attachement. La capacité est **exercée**, le drapeau ne sert de garde à personne. |

> 📌 **Recommandation : `implementee`**, avec en note **la ligne 1356** comme
> preuve de l'usage réel — c'est exactement ce que la classe `implementee` exige :
> *dire où est la lecture*. Ici la « lecture » est un usage inconditionnel, ce qui
> est une réponse plus forte, pas plus faible.

## D6 · `NkDeviceCaps::textureCompressionBC`

| | |
|---|---|
| **quoi** | « La carte lit-elle les textures compressées de la famille BC (le format desktop standard) ? » |
| **où** | `NkIDevice.h:85`. |
| **qui s'en sert** | **0 lecture. 4 écritures** : DX11 `:1747`, DX12 `:3011`, OpenGL `:1035` (`true`, commenté « sur desktop »), Vulkan `:2470` (`feats.textureCompressionBC`). |
| **si on l'abandonne** | **Rien ne casse aujourd'hui** — mais ⚠️ c'est le drapeau du groupe D **qui a le plus de chances d'être lu bientôt** : `NK_BC1_RGB_UNORM`, `NK_BC3`, `NK_BC5`, `NK_BC7` existent dans `NkGPUFormat` (`NkTypes.h:131-138`) et DX11 les traduit (`:1836-1840`). Le premier chargeur de texture qui devra choisir entre BC et non-compressé **posera cette question**. |

> 📌 **Recommandation : `implementee`** (réponse commune du groupe D).

---

# Groupe E — l'arbitrage produit (1 ligne)

## E1 · `NkRendererConfig::msaaSamples`

| | |
|---|---|
| **quoi** | « Combien d'échantillons par pixel pour l'anticrénelage matériel ? » — l'unique réglage entier de MSAA au niveau du renderer. |
| **où** | `NkRendererConfig.h:370`. |
| **qui s'en sert** | **0 lecture, 0 écriture — pas une seule dans tout l'arbre.** ⚠️ **Attention aux homonymes, c'est le piège de cette ligne** : les `msaaSamples` **réellement honorés** sont ceux de `NkContextDesc`/`NkOpenGLDesc` (lus par NKCanvas et NKWindow : `NkDX11Context.cpp:278`, `NkOpenGLContext.cpp` ×7, `NkContext.cpp` ×6) et celui de `Noge::NkCamera` (`NkCamera.h:62`). Les sept `desc.opengl.msaaSamples = 4` des démos et de Pong2 **ne touchent pas** ce champ-ci. |
| **si on l'abandonne** | **Rien ne casse à l'exécution** — mais c'est le seul des 24 où l'abandon est un **choix d'architecture**, pas un ménage : on retire **l'unique point d'entrée entier documenté du MSAA au niveau NKRenderer**, et on acte que le MSAA se règle uniquement au niveau du contexte. ⚠️ **Deux bancs le désignent nommément dans leur exposé** : `Applications/NkMsaaContractCheck/src/main.cpp:19` (« *`NkRendererConfig::msaaSamples` est un uint32 nu* ») et `Applications/NkMsaaDeviceCheck/src/main.cpp:215`. |

📌 **Contexte du jour, à intégrer à la décision.** Le chantier rendu a corrigé le
MSAA **sur les quatre backends**, mais son correctif **plafonne** 3 et 7 au lieu
de les **refuser** — `NkMsaaDeviceCheck` reste donc rouge, **et il a raison de
l'être**. Un `uint32` nu accepte 3 et 7 ; un type qui n'accepte que les comptes
valides ne le peut pas. C'est le même sujet que cette ligne.

> 📌 **Recommandation : `dette-datee`, à dater — et à trancher en même temps que
> B2 (`hasResolve`) et le correctif MSAA du chantier rendu.** Les trois sont un
> seul sujet : *régler le MSAA, le refuser quand il est invalide, le résoudre à
> la fin de la passe.* Les dater séparément fera trois demi-décisions.

---

# Groupe F — le chronométrage GPU : cinq lignes, une seule décision (5 lignes)

**⚠️ Ne pas les trancher séparément.** Le drapeau et les quatre virtuelles
forment **un seul mécanisme** : le drapeau annonce que le chronométrage est
disponible, les quatre virtuelles sont censées le fournir. Répondre
différemment aux cinq produirait un état incohérent.

**Et c'est le plus dangereux des 24, pour une raison qui n'est pas le nombre
d'appelants** — il est zéro partout. C'est la **forme de l'échec** : les quatre
virtuelles ne rendent pas d'erreur, elles rendent **des valeurs plausibles**.

## F1 · `NkDeviceCaps::timestampQueries`

| | |
|---|---|
| **quoi** | « La carte sait-elle horodater le travail GPU ? » — la base de toute mesure « combien de millisecondes a coûté cette passe ». |
| **où** | `NkIDevice.h:80`. |
| **qui s'en sert** | **0 lecture. 3 écritures** : DX12 `:3022` (`true`), OpenGL `:1034` (`true`), Vulkan `:2473` (`props.limits.timestampComputeAndGraphics`, requête réelle). DX11 et Software ne l'écrivent pas. |
| **si on l'abandonne (`false`)** | **Rien ne casse.** Personne ne le lit. |

## F2 · `NkIDevice::BeginTimestampQuery`

| | |
|---|---|
| **quoi** | « Poser une marque temporelle sur le GPU **avant** le travail à mesurer. » |
| **où** | `NkIDevice.h:410`. **Corps vide.** |
| **qui s'en sert** | **0 surcharge, 0 appelant.** Aucun des six backends ne la redéfinit. |
| **si on l'abandonne** | **Rien.** |

## F3 · `NkIDevice::EndTimestampQuery`

| | |
|---|---|
| **quoi** | « Poser la marque **après** le travail. » |
| **où** | `NkIDevice.h:413`. **Corps vide.** |
| **qui s'en sert** | **0 surcharge, 0 appelant.** |
| **si on l'abandonne** | **Rien.** |

## F4 · `NkIDevice::GetTimestampResults`

| | |
|---|---|
| **quoi** | « Relire les marques posées, une fois le GPU passé dessus. » |
| **où** | `NkIDevice.h:416`. **`return false;` en dur.** |
| **qui s'en sert** | **0 surcharge, 0 appelant.** |
| **si on l'abandonne** | **Rien.** ⚠️ Note de forme : `false` ne distingue pas « les résultats ne sont pas encore prêts » de « ce n'est pas implémenté ». Un appelant correct **réessaierait en boucle**, pour toujours. |

## F5 · `NkIDevice::GetTimestampPeriodNs` — **la plus dangereuse des 24**

| | |
|---|---|
| **quoi** | « Combien de nanosecondes vaut un tic de l'horloge GPU ? » — le facteur par lequel on **multiplie** les marques pour obtenir une durée. |
| **où** | `NkIDevice.h:420`. **`return 1.f;` en dur.** |
| **qui s'en sert** | **0 surcharge, 0 appelant.** |
| **si on l'abandonne** | **Rien ne casse** — et pourtant c'est celle qu'il ne faut pas laisser en l'état. ⚠️ **`1.f` n'est pas une valeur d'erreur : c'est un nombre crédible.** Un appelant qui multiplie ses tics par `1.f` obtient un chiffre **plausible, précis d'apparence, et faux** — sur Vulkan la vraie valeur est `props.limits.timestampPeriod`, `1.0` sur beaucoup de matériels mais **pas sur tous**. Le résultat ne ressemblera jamais à un bug : il ressemblera à une mesure. **C'est exactement la famille de défaut qui a motivé tout ce chantier**, logée dans une valeur de repli. |

> 📌 **Recommandation pour les cinq : `abandonner` — passer le drapeau à `false`
> partout, et remplacer `return 1.f;` par une valeur qui ne peut pas se
> confondre avec une mesure (`0.f`, ou mieux : rendre l'échec explicite).**
>
> **Pourquoi abandonner plutôt que dater.** Implémenter le chronométrage GPU sur
> six backends est un vrai chantier (pools de requêtes, synchronisation, lecture
> différée d'une à deux frames). Le dater engage plusieurs jours. **Le mettre à
> `false` coûte cinq lignes et supprime le risque intégralement** — parce que
> personne n'en dépend aujourd'hui. Et le jour où le profilage GPU deviendra un
> besoin, il s'écrira avec ses vraies requêtes, pas par-dessus un `1.f`.
>
> ⚠️ **Si tu préfères dater** (le profilage GPU est utile, et il viendra) :
> **passe quand même `GetTimestampPeriodNs` à une valeur non crédible dès
> aujourd'hui, avant la date.** Les deux gestes sont indépendants, et le second
> est celui qui protège.

---

# Récapitulatif — 24 lignes, 6 réponses possibles

| # | capacité | recommandation | effet sur le stock |
|---|---|---|---|
| **A1** | `NkBlendAttachment::blendEnable` | **`implementee`** — le détecteur s'est trompé | −1 |
| **B1** | `NkDescriptorSetLayoutDesc::isBindless` | `vision-assumee` | −1 |
| **B2** | `NkRenderPassDesc::hasResolve` | **dater** (avec E1) | — |
| **B3** | `NkSwapchainDesc::vsync` | `vision-assumee` (+ ouvrir la double déclaration) | −1 |
| **B4** | `NkIBLConfig::enabled` | **dater, échéance courte** (un `if`) | — |
| **B5** | `NkPostConfig::dof` | `vision-assumee` | −1 |
| **B6** | `NkPostConfig::motionBlur` | `vision-assumee` | −1 |
| **B7** | `NkDeviceCaps::logicOp` | **abandonner** (pas même exprimable) | −1 |
| **C1** | `NkRendererConfig::debugOverlay` | **abandonner** | −1 |
| **C2** | `NkRendererConfig::voxelAOEnabled` | **dater** (+ réparer le commentaire aujourd'hui) | — |
| **C3** | `NkPostConfig::ssr` | **dater** (le plus avancé des trois) | — |
| **C4** | `NkDeviceCaps::shaderFloat16` | **abandonner** (`false`) | −1 |
| **D1** | `NkDeviceCaps::tessellationShaders` | `vision-assumee` | −1 |
| **D2** | `NkDeviceCaps::geometryShaders` | `implementee` * | −1 |
| **D3** | `NkDeviceCaps::drawIndirect` | `implementee` * | −1 |
| **D4** | `NkDeviceCaps::multiViewport` | `implementee` * | −1 |
| **D5** | `NkDeviceCaps::independentBlend` | `implementee` * | −1 |
| **D6** | `NkDeviceCaps::textureCompressionBC` | `implementee` * | −1 |
| **E1** | `NkRendererConfig::msaaSamples` | **dater** (avec B2 + le correctif MSAA) | — |
| **F1** | `NkDeviceCaps::timestampQueries` | **abandonner** (`false`) | −1 |
| **F2** | `NkIDevice::BeginTimestampQuery` | **abandonner** | −1 |
| **F3** | `NkIDevice::EndTimestampQuery` | **abandonner** | −1 |
| **F4** | `NkIDevice::GetTimestampResults` | **abandonner** | −1 |
| **F5** | `NkIDevice::GetTimestampPeriodNs` | **abandonner** (+ `1.f` → valeur non crédible) | −1 |

`*` = conditionné à **une seule réponse** : *`NkDeviceCaps` est-il une API
publique que les applications ont le droit de lire ?* Un « oui » règle D2 à D6
d'un coup.

**Si toutes les recommandations sont suivies : 24 → 5 dettes réellement datées**
(B2, B4, C2, C3, E1), dont deux forment **un seul sujet MSAA** (B2 + E1). Le
cliquet ne descend que dans le sens autorisé ; il n'a pas été touché par ce
document.

---

---

## Pourquoi elles sont 25 et non plus 24 — le cliquet a été relevé, délibérément

⚠️ **Le 23/08 j'ai écrit que je n'avais pas touché au plafond. Le 24/08 je l'ai
relevé, de 24 à 25.** C'est un geste délibéré, il apparaît dans le diff de
`config/capacites.list`, et voici sa raison entière.

**Le cliquet repose sur une prémisse :** *« le stock est un héritage qui ne se
renouvelle pas »*. Elle ne tenait que tant que le détecteur voyait **tout** le
stock. **Il ne le voyait pas.**

Le chantier rendu a trouvé un onzième paramètre déclaré non honoré —
`NkRendererConfig::pipeline`. La première chose à faire n'était pas de le classer :
c'était de vérifier que **mon détecteur l'attrapait**. Il ne l'attrapait pas. Deux
défauts, mesurés et corrigés le 24/08 :

1. **Les chaînes de caractères n'étaient pas amputées.** Le commentaire l'était —
   la ligne au-dessus le dit explicitement, « une mention dans un commentaire n'est
   pas une lecture ». Mais `logger.Info("[NkRender3D] Shadow pipeline create: …")`
   portait le jeton `pipeline`, et ce jeton comptait comme une **lecture** du champ.
   *La même faute que celle qu'ampute la ligne au-dessus, dans un second vêtement :
   du texte pris pour du code.*
2. **Le repli « ce fichier nomme le type » attribuait au champ les variables
   locales homonymes.** `NkRender3D.cpp` nomme `NkRendererConfig` une fois et
   déclare partout un `NkPipelineHandle pipeline` local. Chaque
   `cmd->BindGraphicsPipeline(pipeline)` comptait comme une lecture du champ.

**Et une seule lecture suffisait à clore le dossier** — c'est même une optimisation
assumée du détecteur. Un champ creux devenait donc invisible à cause d'une variable
locale homonyme dans un autre fichier.

**Mesure de la correction : 128 candidats → 143.**

⚠️ **Et quatre de ces quinze étaient des FAUX POSITIFS** — trouvés en
contre-vérifiant à la main, pas par un outil. `NkBufferBarrier::srcStage`,
`::dstStage` et leurs deux jumeaux de `NkTextureBarrier` sont **réellement lus**
(`NkVulkanCommandBuffer.cpp:400-401` et `:456-457`). La ligne

```cpp
srcStage |= toStage(bb[i].srcStage);   // une LOCALE à gauche, une VRAIE LECTURE à droite
```

contient les deux, et le test d'écriture — qui cherchait `nom … =` **n'importe où
sur la ligne** — attrapait la locale. Un champ lu ressortait « écrit, jamais lu ».
*Le troisième défaut du jour, et il est né de la correction des deux premiers.*
Corrigé le même jour, par la même règle : **hors du fichier déclarant, une
écriture de membre porte elle aussi un `.` ou un `->` juste devant le nom.**

**Reste 139 candidats — onze capacités qui existaient déjà et que l'outil ne
montrait pas :**

| | |
|---|---|
| `NkDrawIndexedIndirectArgs::firstIndex` · `::firstInstance` · `::indexCount` · `::vertexOffset` | `NkDrawIndirectArgs::firstVertex` · `::vertexCount` |
| `NkFramebufferDesc::layers` | `NkIDevice::mipLevel` |
| `NkRendererConfig::hdr` · `::pipeline` | `NkRendererConfig::vsync` ← **la 25e** |

Dix se classent `vision-assumee` sans difficulté. **La onzième ne le peut pas** :
la règle (a) interdit `vision` dès qu'un drapeau annonce `true` à du code, et six
démos livrées écrivent `rcfg.vsync = true`.

> **Le cliquet s'oppose à la dérive silencieuse, pas à la décision.** Ce +1 n'est
> pas une capacité nouvelle : c'est de l'héritage qui était invisible. Le compter
> comme une dérive reviendrait à préférer un chiffre stable à un chiffre vrai.

⚠️ **Et ce que ça dit du chiffre 24 lui-même :** il n'a jamais mesuré « les
capacités creuses du dépôt ». Il mesurait **les capacités creuses que mon outil
savait voir**. La différence est de quinze, et personne ne pouvait la connaître
avant de la chercher.

## Deux limites de ce relevé, dites aussi fort que le reste

**1. Le détecteur ne regarde que trois en-têtes.** `ENTETES_D2` vaut
`NkDescs.h`, `NkIDevice.h`, `NkRendererConfig.h`. En cherchant les homonymes
de D2, j'ai trouvé **`NkSLTargetCaps::geometryShaders`** (`NkSLFeatures.h:34`) :
**6 écritures, 0 lecture** — exactement le même défaut, dans un fichier que le
détecteur **ne lit pas**. Il n'est donc dans aucun classement, et son absence
du rapport se lit « rien à signaler ». *Ce n'est pas un défaut du classement,
c'est la portée de l'instrument* — mais il faut le dire, sinon les 24 se lisent
comme un inventaire complet, ce qu'ils ne sont pas.

**2. « 0 appelant » est un fait sur cet arbre, pas sur le dépôt.** La mesure a
été refaite après fusion de `main` à jour (`9b31cc8f`), retard 0. Elle ne dit
rien des branches non fusionnées : si une branche en cours implémente le
chronométrage GPU ou monte la passe SSR, **ce document l'ignore**. C'est la
même limite que celle du vérificateur de bancs, et elle vaut ici mot pour mot.

---

# L'arbitrage du 24/08 : « tout implémenter » — et ce que ça coûte, mesuré

> **Rodolf, 24/08** : *« tout implémenter progressivement sauf si tu juges que ça
> ne vaut pas la peine, mais mieux tout implémenter. »*

⚠️ **Ce document était écrit pour l'objectif inverse.** Il optimisait pour
**réduire le stock de dettes** (24 → 5) : la moitié de ses recommandations
disaient « abandonner » ou « vision ». Rodolf optimise pour **construire le
moteur**. Les deux sont légitimes ; c'est la sienne qui compte. **Les
recommandations des fiches ci-dessus sont donc périmées** — chaque ligne de
`config/capacites.list` porte désormais son arbitrage en tête de sa note, et son
ancienne recommandation a été retirée. *Une recommandation d'abandon laissée sous
une décision de construction recruterait le prochain lecteur contre la décision.*

## 🔴 Les deux réserves n'ont pas pu être enregistrées comme demandé

`logicOp` et `shaderFloat16` devaient passer en `vision-assumee`. **Le détecteur a
refusé, code 4, et il a raison :**

```
NkDirectX11Device.cpp:1746   mCaps.logicOp = true;          (en dur)
NkVulkanDevice.cpp:2469      mCaps.logicOp = feats.logicOp;
NkVulkanDevice.cpp:2474      mCaps.shaderFloat16 = true;    // approximation
```

> **Un « non » posé ne suffit pas tant que le code continue de promettre « oui ».**

Décider de ne pas implémenter `logicOp` ne retire rien au fait que DX11 répond
**oui** à qui le demande à l'exécution. La réserve n'est donc pas un classement :
c'est une **tâche**, petite, dans les backends — passer `DX11:1746` et `VK:2474` à
`false`. **Hors de mon périmètre.** Les deux lignes restent `dette-datee | A-DATER`
et **le cliquet ne bouge pas : 25.** Il descendra de deux le jour où les drapeaux
se tairont.

📌 C'est la première fois que la règle (a) mord sur un cas que personne n'avait
anticipé : elle a été écrite pour empêcher de classer « vision » un **mensonge**,
elle vient d'empêcher de classer « vision » un **renoncement honnête mais
incomplet**. C'est le même service.

---

## Le coût de l'ordre proposé — mesuré, ou marqué « non mesuré »

| | chantier | fichiers | backends | API publique |
|---|---|---|---|---|
| **1** | B4 `IBLConfig::enabled` | **1** | **0** | inchangée |
| **2** | D2-D6 `NkDeviceCaps` | **4** (+1 à revoir) | DX11, DX12, GL, VK | inchangée |
| **3** | C5+B3 `vsync` | **2** | Vulkan seul | inchangée |
| **4** | C1 `debugOverlay` | **non mesuré** | — | non mesuré |
| **5** | F1-F5 chronométrage GPU | **8 à 10** | les 4 | inchangée |
| **6** | B5, B6 `dof`, `motionBlur` | **2** | 0 (shaders déjà là ×5) | inchangée |
| **7** | D1, B1 tessellation, bindless | D1 non mesuré · B1 **tout** | tous | D1 inchangée · **B1 change** |

### 1 · B4 `NkIBLConfig::enabled` — une ligne, et rien d'autre

`InitEnvironment()` (`NkRendererImpl.cpp:363`) lit **onze** champs voisins de
`mCfg.ibl` et ne lit jamais `enabled`. Le correctif est un `if` en tête.

**Ce que j'ai vérifié avant de l'appeler « un simple `if` »** — parce que ma fiche
le disait déjà, et qu'une affirmation non mesurée m'a déjà coûté un chiffre cette
semaine :

- `mEnvironment` devient nul si on saute l'init. Ses **trois** déréférencements
  dans `NkRender3D.cpp` sont **tous gardés** : `if (mEnv)` (:388),
  `if (!mDevice || !mEnv) return` (:450), `mEnv && …` (:2496). **Aucun nu.**
- `GetEnvironment()` (`NkRenderer.h:97`, API publique) rend déjà un pointeur qui
  **peut** être nul (`mEnvironment.Reset()` :614) et a **0 appelant** dans l'arbre.
- Les deux appelants de `InitEnvironment()` (:202 et :540) traitent `true` comme
  « rien à faire » — un retour anticipé `true` est donc correct.

⚠️ **Et une chose que la fiche ne disait pas** : **0 écriture** de `ibl.enabled`
aujourd'hui. Personne ne le met à `false`, donc le `if` ne changera rien pour
personne **tant qu'un banc ne l'exercera pas**. *Livrer le `if` sans le banc, ce
serait ajouter une capacité dont on ne saura pas si elle marche.*

### 2 · D2-D6 `NkDeviceCaps` — 19 littéraux à transformer en interrogations

```
DX11   6 sites : 1740 1741 1743 1744 1745 1747
DX12   6 sites : 3005 3006 3008 3009 3010 3011
GL     6 sites : 1029 1030 1031 1032 1033 1035
VK     1 site  : 2466 (drawIndirect = true, le seul en dur de Vulkan)
```

📌 **Vulkan interroge déjà pour cinq des six** (`feats.tessellationShader`,
`feats.geometryShader`, `feats.multiViewport`, `feats.independentBlend`,
`feats.textureCompressionBC`). **Le patron existe** : c'est de la recopie
disciplinée, pas de la conception.

`NkSoftwareDevice.cpp` écrit des littéraux **honnêtes** (`false` pour
`drawIndirect` :732 et `multiViewport` :733) — à laisser. ⚠️ Sauf
`independentBlend = true` (:734), qui est une affirmation et pas une mesure : à
examiner à part.

### 3 · C5+B3 `vsync` — la machinerie existe, le fil n'est pas branché

La vsync **fonctionne** au niveau RHI — mais elle est alimentée par
`NkContextDesc`, pas par `NkRendererConfig` :

```
NkDirectX11Device.cpp:123   mVsync = dxCfg.vsync;               deja honore
NkOpenglDevice.cpp:416      init.context.opengl.swapInterval    deja honore
NkVulkanDevice.cpp:783      VK_PRESENT_MODE_FIFO_KHR            FORCE EN DUR
```

Manque : **le routage** dans `NkRendererImpl`, et **le choix du present mode**
côté Vulkan — dont la liste des modes disponibles est déjà interrogée
(`:776-778`) puis **ignorée**. DX11 et OpenGL n'ont rien à faire une fois le
routage posé.

**B3** (`NkSwapchainDesc::vsync`) reste **bloqué** tant que
`NkIDevice::CreateSwapchain` (`:474`) rend `nullptr` en dur.

### 4 · C1 `debugOverlay` — non mesuré, et c'est la réponse

**Aucun sous-système d'overlay n'existe côté NKRenderer.**
(`RenderInputDebugOverlay()` de NKEvent est autre chose : du débogage d'entrées.)
C'est une **construction**, pas un branchement — je ne peux pas la chiffrer sans
la concevoir, et je ne l'estime pas de tête.

⚠️ **Elle est mal placée en position 4** : les positions 1 à 3 sont des
branchements de quelques lignes, celle-ci est un chantier. Je la déplacerais
après le groupe F.

### 5 · F1-F5 chronométrage GPU — tout est à écrire, et rien n'est en trop

```
NkIDevice.h:410-421   les 4 virtuelles existent deja
surcharges dans les backends ......................... 0
vkCmdWriteTimestamp / D3D12_QUERY_TYPE_TIMESTAMP /
D3D11_QUERY_TIMESTAMP / glQueryCounter dans l arbre ... 0
```

**API publique inchangée** — les quatre virtuelles sont déjà là, il n'y a que des
surcharges à écrire. Mais il n'existe **aucune** ligne de code de requête
temporelle dans le dépôt : pool, reset, résolution, conversion, tout est à faire,
sur quatre backends.

`GetTimestampPeriodNs()` rend `1.f`. La vraie valeur est à portée **partout** :
`props.limits.timestampPeriod` (VK), `GetTimestampFrequency` (DX12), requête
disjoint (DX11), `GL_TIMESTAMP` (GL). *C'est la ligne qui rend un nombre crédible
et faux à l'instrument censé mesurer le GPU.*

### 6 · B5, B6 `dof`, `motionBlur` — les shaders sont écrits, la passe manque

Les deux ont leurs shaders pour **cinq backends** (DX11, DX12, GL, MSL, VK).
⚠️ Et ils sont **versionnés en double** :
`Resources/NKRenderer/Shaders/PostProcess/{DOF,MotionBlur}/` **et**
`Kernel/Runtime/NKRenderer/src/NKRenderer/Shaders/PostProcess/{DOF,MotionBlur}/`.

Manque : la passe C++ dans `NkPostProcessStack` — **exactement le travail que SSR
vient de recevoir le 23/08**. Le patron existe et il est frais. C'est le meilleur
rapport entre ce qui est déjà écrit et ce qui reste à écrire.

### 7 · D1, B1 — le plus lourd, et les deux ne se ressemblent pas

**D1 tessellation** — plomberie partielle, mesurée : `NK_PATCH_LIST` (DX12:3341,
3355 ; GL:3222), `NK_TESS_CTRL` / `NK_TESS_EVAL` (GL:3237,3239),
`patchControlPoints` (`NkDescs.h:687`), et **NkSL connaît déjà les étages**
(`NkGLSLCompiler.cpp:35-37`, `NkSLCodeGenAdvanced.cpp:277-297`).
**NON MESURÉ** : si le chemin est complet de bout en bout.

**B1 bindless** — `isBindless` et `NkBindlessHeapDesc` ne vivent que dans **trois
en-têtes** (`NkIDevice.h`, `NkDescs.h`, `NkTypes.h`). **Aucune implémentation de
backend.** C'est de la **surface pure** : tout est à écrire, et c'est **le seul
des sept qui change l'API publique**.

---

## Ce que je n'ai pas fait

**Je n'ai touché à aucun backend.** D2→D6 et F1→F5 vivent où le chantier du
retournement Y travaille en ce moment. Ce document classe, chiffre et écrit ;
l'implémentation part quand la zone est libre.

---

## 🔴 Correction du 27/08 — deux chiffrages de la veille étaient faux, dont un entièrement

Le chantier rendu a mesuré deux faits que j'avais manqués. **Les deux touchent le
tableau ci-dessus**, et l'un d'eux l'invalide sur une ligne entière.

### C1 `debugOverlay` — j'avais écrit « aucun sous-système n'existe ». C'est faux.

**`NkOverlayRenderer` existe** : `Tools/Overlay/NkOverlayRenderer.{h,cpp}`,
instancié par `NkRendererImpl::InitOverlay()` (`:418-431`), exposé par l'API
publique `NkRenderer.h:92 GetOverlay()`, et appelé depuis **21 fichiers
d'application** (NK3DModeler, NKARDemo, NKXRDemo, et 18 démos Sandbox).

> ⚠️ **J'ai cherché le MOT `debugOverlay`, pas la CAPACITÉ « overlay ».** C'est la
> faute que ce chantier traque depuis le premier jour — *chercher un nom n'est pas
> chercher un usage* — et je l'ai commise dans un chiffrage destiné à Rodolf.

**Ce que C1 est réellement** : le sous-système est déjà là et déjà piloté — par le
drapeau de sous-système `NK_SS_OVERLAY` (`NkRendererImpl.cpp:245`), **pas** par
`mCfg.debugOverlay`. `debugOverlay` est donc un **second interrupteur redondant que
personne ne lit**. `ForEditor()` pose les deux ; l'overlay marche, et `debugOverlay`
n'y est pour rien.

| | |
|---|---|
| fichiers | **1** (`NkRendererImpl.cpp`) |
| backends | **0** |
| API publique | **inchangée** |
| ⚠️ préalable | **une question de conception, pas de code** : que veut dire `debugOverlay` à côté de `NK_SS_OVERLAY` ? Piloter l'**affichage** du panneau pendant que le drapeau pilote l'**existence** du renderer — ou disparaître comme redondant. |

**Elle n'est donc pas mal placée en position 4 : elle est même moins chère que je
ne le disais.** Ce qui coûte, c'est l'arbitrage, pas l'implémentation.

### F1-F5 — le fait du chantier rendu est confirmé, et il est pire qu'une dette

```
gpuTimeMs declare a 0.f dans TROIS structures :
    NkRendererTypes.h:669      NkRenderGraph.h:110      NkIDevice.h:446
affecte quelque part ................................. NULLE PART
lu pour etre AFFICHE ....... NkOverlayRenderer.cpp:50  (« GPU:%.2fms »)
                             NkRenderGraph.cpp:792 et 795
```

> **Trois champs qui valent zéro par construction, lus pour être montrés à un
> utilisateur.** `GPU:0.00ms` s'affiche, et rien dans le dépôt ne peut le rendre
> autre chose.

**Mesure de mon côté, et elle diffère de la sienne** — je le dis plutôt que de
recopier son chiffre : `mCaps.timestampQueries` est écrit par **trois** backends,
pas quatre — `DX12:3022` et `GL:1034` en dur à `true`, `VK:2473` par vraie
interrogation. DX11 et Software ne l'écrivent jamais (il reste `false`). Et je
compte **21 fichiers** d'application, là où il compte 15 applications : *nous ne
comptons pas la même chose, et aucun des deux chiffres n'annule l'autre.*

⚠️ **Ce que ça change au classement de F1-F5** : rien sur le coût (8 à 10 fichiers,
4 backends, API inchangée), **tout sur l'urgence**. Ce n'était pas une dette
théorique : c'est un chiffre faux montré à un utilisateur sur 21 sites d'appel.

### Pourquoi mon détecteur ne l'avait pas vu — une limite de périmètre, pas de règle

`NkRendererTypes.h` et `NkRenderGraph.h` **ne sont pas dans `ENTETES_D2`**. Le
périmètre est une **donnée déclarée**, pas un oubli — mais il a un angle mort, et
le voici chiffré : **l'élargir coûterait +141 champs à examiner (349 → 490,
+40 %)**. *Proposé, chiffré, non fait.*

---

## ⚠️ Le compte des fichiers morts passe de 28 à 35 — et personne n'a écrit dix fichiers

**Rien n'est apparu.** Le compte a changé parce que **l'outil regardait au mauvais
endroit**, et voici exactement lequel.

`preuve_copies_mortes.sh` choisissait ses candidats par un motif appliqué au **nom
de fichier** :

```python
re.search(r" copy( \d+)?\.(h|hpp|cpp|inl|c)$", f)     # le NOM
```

Or `Applications/Pong copy/` est un **dossier** copie — et ses **dix** sources
portent des noms parfaitement normaux : `Apps.cpp`, `PongGame.h`,
`Renderer/NkRasterizer.h`… **Le motif appliqué au nom en voyait zéro.**

Le motif porte désormais sur le **chemin** :

```
avant   28 fichiers examines   28 prouves morts   13 s
apres   38 fichiers examines   35 prouves morts   16 s
```

| | |
|---|---|
| **+10** | les sources de `Applications/Pong copy/`, invisibles jusqu'ici |
| **+1** | `PBRGame.cpp`, déclaré à la main (nom normal, mort quand même) |
| **−4** | 3 fichiers désormais « encore compilés » + l'arithmétique du recouvrement |

📌 **Les 3 « encore compilés » sont la bonne nouvelle** : ils prouvent que la
preuve **discrimine toujours**. Un outil qui déclare tout mort ne prouve plus rien.

> **Ces 35 fichiers étaient déjà morts hier. Seul le regard a changé.**

⚠️ **Et la vraie parade a été mesurée puis refusée** : prouver les **810** fichiers
d'`Applications/` prend 6 min 41 s — acceptable — mais rend **449 morts**, dont
`ConquerorAIABI.h` qui est **inclus par 13 fichiers**. La preuve de l'outil ne
tient que sur des copies. *Élargir la population sans réécrire la preuve n'est pas
une amélioration, c'est une invalidation.*
