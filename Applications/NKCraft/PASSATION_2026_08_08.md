# NK3DModeler — passation du 8 août 2026

Document destiné à **l'agent qui reprend le chantier**. Il dit ce qui a été fait,
ce qui est **cassé et pourquoi**, et ce qui reste — sauvegarde de fichiers,
matériaux, modélisation. Écrit après une session où Rihen a **perdu du travail
trois fois** : la priorité absolue est que cela n'arrive plus.

Lire d'abord `CLAUDE.md` (règles du dépôt) puis ce fichier.

---

## 0. Règles non négociables (rappel)

- **Français** partout : réponses, commentaires, journaux, interface.
- **Aucune STL** : `NkString`, `NkVector`, `NKMemory`, `math::Nk*`.
- **Jamais réécrire une source en PowerShell** (`Get-Content`/`Set-Content`
  double-encodent l'UTF-8). Uniquement Edit/Write, ou remplacement **au niveau
  octet** via `[IO.File]::ReadAllBytes`/`WriteAllBytes`.
- **Fermer NK3DModeler.exe avant de compiler** (verrou de lien) — autorisation
  durable de Rihen.
- **Builder Debug ET Release**, vérifier **29/29** :
  `jenga build --target NK3DModeler --config Release` puis `--config Debug`.
  Ne **jamais** juger sur `jenga build` sans `--target` : le build global échoue
  depuis longtemps sur `NkRHIDemoText` (une démo qui appelle une API de texte
  jamais livrée — `NkFontLibrary`, `NkTextShaper` : zéro occurrence dans le
  dépôt). Ce n'est pas une régression.
- **Lancer depuis la racine du dépôt**, sinon shaders absents et fenêtre noire.
- **Valider avec Rihen après chaque correctif**, un point à la fois.
- **Ne jamais piloter la souris/le clavier de Rihen.**
- Commentaires : expliquer le **POURQUOI**, jamais le QUOI.
- **Ne jamais surévaluer.** Dire ce qui ne marche pas. Un chantier « écrit mais
  non éprouvé » n'est **pas** livré.

---

## 1. LE DÉFAUT CENTRAL — à corriger AVANT tout le reste

> ### RÈGLE ABSOLUE (Rihen, 8 août 2026)
>
> **FERMER UN ONGLET NE DOIT JAMAIS SUPPRIMER QUOI QUE CE SOIT.**
> Ni une scène, ni un mesh, ni une texture, ni un matériau, ni aucun autre type
> de fichier. **Un onglet est une VUE sur un asset, jamais l'asset lui-même.**
> Fermer une vue ferme la vue — le fichier reste dans le projet, visible dans le
> navigateur, réouvrable. Aucune exception, aucun type d'asset excepté.
>
> C'est aujourd'hui **violé** : voir ci-dessous.

> **Les onglets sont la seule source de vérité des scènes. Fermer un onglet
> supprime la scène.**

`NkSceneCapture` (`Project/NkModelerScene.h`) n'écrit que les onglets **ouverts** :

```cpp
for (int32 t = 0; t < st.sceneCount && t < 8; ++t) {
    if (st.sceneTabKind[t] != 0) continue;   // ...puis capture
```

Conséquences **constatées par Rihen, preuves dans son fichier** :

- il ferme `Scene_2`, enregistre → la scène **disparaît du fichier**, mais ses
  nœuds y restent **orphelins** (`"document": 1` sans scène de rang 1 —
  vérifiable dans `C:/Users/Rihen/NK3DModeler/MonProjet/MonProjet.nk3dm`) ;
- les cartes du navigateur sont **virtuelles** : `NkBrowserSyncScenes`
  (`Shell/NkModelerScreens.h`) les dérive des onglets ouverts, et **rien n'est
  persisté** — `grep -c browser Project/NkModelerScene.h` renvoie **0**. Fermer
  l'onglet perd donc la carte, et rouvrir le projet ne la retrouve pas.

**Ce que Rihen demande** (formulé par lui, à respecter) :

> « Un `.nk3dm` contient les données de deux manières au choix : soit des **liens**
> vers les scènes et données, soit il **contient directement** ces scènes. Dans
> les deux cas on doit **toujours** avoir la représentation de la scène dans le
> projet ; ce n'est que pour l'export en élément unique que tout est dans le
> `.nk3dm` seul. »

**Direction à prendre** :

1. Une **liste de scènes du projet** indépendante des onglets ouverts. Un onglet
   devient une *vue* sur une scène, pas la scène elle-même. Fermer un onglet ne
   doit **rien** supprimer.
2. Le **navigateur est persisté** dans le `.nk3dm` (dossiers, cartes, parenté).
   Aujourd'hui il naît vide à chaque lancement.
3. Le `.nk3dm` gagne les **deux modes** (décidés, documentés dans `ROADMAP.md`) :
   - **lié** : JSON, assets en fichiers séparés, chemins relatifs ;
   - **empaqueté** : conteneur binaire, tout dans un fichier.
   Une **seule extension** ; l'en-tête tranche (cf. §2).

---

## 2. Décisions d'architecture déjà prises (ne pas re-débattre)

### Format du projet

- **`.nk3dm` = un dossier + un fichier** à sa racine. Aucune arborescence
  imposée : le navigateur du modeleur gère déjà les dossiers.
- **Tous les chemins stockés sont RELATIFS** à la racine du projet.
- **L'extension est un INDICE, l'en-tête est la VÉRITÉ**
  (`CONVENTIONS_FICHIERS.md`).
- **Mode empaqueté : NE PAS ajouter de type binaire à `NKS1`.** Le format
  `.nkasset` (`NKSerialization/Asset/`, `NkAssetMetadata.h`) fait déjà
  exactement ce qu'il faut :
  `[Header:40][MetadataSize:4][Metadata:NKS1][PayloadSize:8][Payload:octets bruts]`
  avec `payloadOffset`/`payloadSize`/`payloadCRC`. **Les octets bruts vivent À
  CÔTÉ de l'archive, jamais dedans** — ce qui permet la lecture paresseuse, évite
  de dupliquer des centaines de Mo en mémoire, et sépare les CRC.
  **Travail réel** : `NkAssetFileHeader` ne porte qu'UN payload ; généraliser le
  même patron avec une **table de N entrées** (décalage, taille, CRC).
- **Pourquoi NKS1 et pas un conteneur maison** : `NkNativeFormat` sérialise un
  **`NkArchive`** — la structure que le JSON écrit déjà. Le **même**
  `NkSceneCapture` alimente les deux écritures : aucune divergence possible.

### Règle des extensions (gravée dans `CONVENTIONS_FICHIERS.md`)

> **L'extension dit ce que le fichier PRODUIT, jamais comment il est écrit dedans.**

- `.nkmat` couvre le matériau **simple ET nodal** : le nodal est une façon de
  décrire, pas une nature d'asset.
- Test décisif : deux fichiers qu'on dépose au même endroit avec le **même
  effet** partagent leur extension ; sinon ils en changent.
- **Aucun nom n'est gravé avant le premier octet écrit** (`.nkgeo`, `.nkmot`…
  attendent leur chantier).

### Transformations

**T + R + S séparés, jamais de matrice 4×4 libre** : le cisaillement n'est pas
représentable et se perdrait en silence.

---

## 3. Bugs corrigés cette session (avec leur leçon)

| Bug | Cause réelle | Leçon |
|---|---|---|
| **Réouverture = travail perdu** | `NkProjectLoad` (`NkModelerProject.cpp`) n'écrivait **jamais** son paramètre `scene` : signature et doc présentes, corps muet. La restauration recevait une archive vide, puis le premier enregistrement écrasait le fichier. | Une signature n'est pas une implémentation. **Vérifier le corps**, pas le prototype. |
| **JSON illisible au rechargement** | `NkStringView(const char (&str)[N])` prenait `N-1` **sans regarder le contenu** : un `char nom[64]` partait entier — zéros + mémoire non initialisée — dans le fichier. | Corrigé à la racine (`NkStringView.h`) : la vue s'arrête au premier NUL. Toute une classe de bugs éliminée. |
| **Bouton « Enregistrer » inerte** | `btn("tb.save", …)` peignait l'icône et **jetait** la valeur de retour. | Un bouton qui ne teste pas son retour n'existe pas. |
| **Ctrl+S armait l'échelle** | La touche arrive par **deux voies** : le shell (`main.cpp`) et un rappel d'événement dans `NkDemo3D.cpp`. Seule la première avait la garde. | Répétition du bug « Ctrl+V collait deux fois ». **Toujours chercher la seconde voie.** |
| **Pastille jamais visible** | Elle était dessinée **dans** le bloc conditionné par « plus d'une scène » — le cas courant (une scène) l'excluait. | |
| **Pastille non allumée au gizmo** | Le shell énumérait les mutations une à une ; le gizmo lui échappait. Corrigé par `Demo3DHostAnyGizmoDragging()` + front descendant. | Surveiller **un point de passage**, pas N sites. |
| **Intérieur des objets éclairé** | La grille d'occlusion (GI voxel) ne se construisait **que** sous `if (st->giTest)` — jamais en scène utilisateur. `voxAO` valait 1 partout. | |
| **Carré sombre au sol (rendu)** | Corollaire : en faisant enfin construire la grille, l'occludeur du **sol de la démo** (boîte 16×16 à y=0.05) y entrait. Corrigé par un test de visibilité. | |
| **Ombre décollée du pied** | Deux causes cumulées : le sol infini était **enterré de 2 mm**, et le biais normal valait ~1 cm sur une face d'omni. | Le contraste que Rihen a fourni (plan maillé correct / sol infini décalé) a désigné le coupable. |

**Rejeté explicitement** : le **culling des faces avant** dans la passe d'ombre.
Il colle parfaitement le contact, mais **éclaire l'intérieur des objets fermés**
(dans un modeleur la caméra y entre). Un long commentaire dans `NkRender3D.cpp`
l'explique — **ne pas le retenter**.

---

## 4. Ce qui reste — SAUVEGARDE DE FICHIERS (priorité 1)

### 4.0 Le périmètre exact (Rihen, 8 août 2026)

> « Le prochain agent doit recommencer à travailler sur la sauvegarde de **tous
> les types de fichiers actuellement pris en charge**, que ce soit sous forme de
> **fichiers séparés** ou en **blob dans un fichier de projet** : fichier de
> projet, fichier de scène, fichier de mesh, fichier de texture, fichier de
> matériau, fichier de mesh procédural, etc. »

Ce n'est donc **pas** un chantier « sauvegarde de scène » mais un chantier
**persistance de tous les assets**, avec pour **chaque type** les deux modes :

| Type | Extension | Mode lié (fichier séparé) | Mode empaqueté (blob) |
|---|---|---|---|
| Projet | `.nk3dm` | JSON + références | conteneur + table de payloads |
| Scène | `.nkscene` | fichier propre, référencé | blob dans le `.nk3dm` |
| Maillage | `.nkmesh` | fichier propre | blob |
| Texture | `.nktex` / `.nktexc` | fichier propre | blob |
| Matériau | `.nkmat` / `.nkmati` | fichier propre | blob |
| Mesh procédural | (nom à décider **au moment d'écrire le premier octet**) | recette, pas résultat | blob |

**Contraintes qui valent pour TOUS les types** :

- **Un seul code de capture par type**, alimentant les **deux** écritures (JSON
  lié / binaire empaqueté). Deux chemins d'écriture divergeraient — c'est la
  raison même du choix `NkArchive` (§2).
- **Chemins relatifs** à la racine du projet, sans exception.
- **Version de format** dans chaque en-tête, relue à l'ouverture.
- **Honnêteté du format** : un champ qui dit ce qui est réellement couvert et ce
  qui ne l'est pas (le patron `serialisee` / `couvre` / `nonCouvert` du `.nk3dm`
  est le bon). **Ne jamais laisser croire qu'un contenu est sauvegardé s'il ne
  l'est pas.**
- **Bascule lié ↔ empaqueté sans perte**, dans les deux sens.
- Et la règle absolue du §1 : **fermer une vue ne supprime jamais le fichier**.

### 4.1 Les étapes

1. **Découpler assets et onglets** (§1) — bloquant, tout le reste en dépend.
   Un registre d'assets du projet, indépendant des vues ouvertes.
2. **Persister le navigateur** dans le `.nk3dm` : dossiers, cartes, parenté,
   et le lien carte ↔ scène.
3. **Restaurer l'onglet actif** : `sceneActive` est écrit **et** relu
   (`NkModelerScene.h`), mais Rihen constate que ce n'est pas la bonne scène qui
   s'ouvre. À diagnostiquer une fois §1 fait — le symptôme vient probablement de
   la disparition de scènes, pas du champ lui-même.
4. **Sélecteur de fichiers PERSONNALISÉ** — exigence de Rihen :
   > « Si ce n'est pas présent on doit avoir un sélecteur de dossier dont la
   > racine c'est notre navigateur de projet pour choisir où sauvegarder ; ça
   > doit être **custom** et permettre de **créer des dossiers**. »
   - **Ne PAS utiliser `NkDialogs::`** (natif Win32) : son filtre est **cassé**
     (`Win32PrepareFilter` construit `result += ")\0"` — un littéral C s'arrête
     au premier zéro, le motif arrive vide). NK3DModeler est la **seule**
     application du dépôt à l'utiliser encore.
   - **Utiliser `Engine/NKEditorKit/src/NKEditorKit/NkFilePicker.h`** (831 lignes,
     déjà partagé). NKCode s'en sert : `struct NkCodeDialogs : public
     NkFilePickerState` (`Applications/NKCode/src/NKCode/Shell/Dialogs.h`).
     Voir aussi `NkExplorer.h` (1780 lignes) et `NkDirBrowser.h`.
   - **Corriger `NkDialogs` quand même** (bug réel), mais **ne plus l'employer**.
5. **Mode empaqueté** du `.nk3dm` (§2), une fois le mode lié solide.
6. **Test de bout en bout obligatoire** avant de déclarer quoi que ce soit :
   créer 2 scènes → modifier → enregistrer → **fermer l'application** → rouvrir →
   *tout* doit revenir, y compris les scènes non ouvertes.
7. **Test « fermer une vue »**, à passer pour **chaque type d'asset** : créer
   l'asset → l'ouvrir dans un onglet → **fermer l'onglet** → l'asset doit rester
   dans le navigateur → le rouvrir → son contenu doit être intact. Puis
   enregistrer, fermer l'application, rouvrir : il doit toujours être là.
8. **Test de bascule** : un projet en mode lié passé en empaqueté puis reconverti
   en lié doit redonner exactement le même contenu.

---

## 5. Ce qui reste — MATÉRIAUX

Ordre **décidé par Rihen** : *« dès la passe sur les matériaux on doit commencer
par les implémenter avant de continuer le branchement »* — parce que chaque
paramètre ajouté après coup change l'`ObjectUBO`, donc oblige à revalider les
cinq backends.

**0. La physique de surface d'abord**

- **Exposer `clearcoat` et `subsurface`** : `pbr.frag.nksl` les calcule **déjà**
  (lignes ~429 et ~438), `NkMaterial` les expose (`SetClearcoat`,
  `SetSubsurface`) — **le panneau ne les propose pas**. Gain le moins cher.
- **Transmission + IOR** — *rien* dans le shader. C'est le « S » de BSDF
  (*scattering*) : sans elle, **pas de verre, pas d'eau**. Manque le plus visible.
- **Anisotropie** (métal brossé, cheveux), **Sheen** (tissus, velours).

**Vocabulaire** : « BSDF » **n'entre pas** dans le panneau simple (couleur,
rugosité, métallique, vernis, diffusion). Il entre **avec l'éditeur nodal**, car
« mélanger deux matériaux » se dit *mélanger deux BSDF* — un nœud de mélange n'a
de sens que si ce qu'il mélange est une fonction de distribution (mélanger deux
rugosités ne donne pas la rugosité du mélange).

**Ensuite** :

1. **Types de surface** (`NkMaterialType`) — n'exposer que ceux que le renderer
   **dessine réellement**, chacun vérifié à l'écran (règle des stubs).
2. **MÉLANGER les matériaux** — masque, hauteur, usure. Shaders `Layered*`
   existants dans `Resources/NKRenderer/Shaders/`.
3. **Matériaux NODAUX** — **passent obligatoirement par NKGraph** (décision du
   dépôt : un seul substrat de graphe pour Blueprint, matériaux, texturing
   procédural, motion). **Ne pas construire un graphe en silo.**
4. **Sauvegarde `.nkmat` / `.nkmati`** via `NkMaterialAsset`/`NkMaterialLibrary`.

**Déjà en place** : quatre canaux de texture (couleur, normale, ORM, émissif),
une seule paire de fonctions indexée par canal (`Demo3DHostProjMatMap` /
`SetMap`) — quatre copies auraient divergé au premier correctif.

---

## 6. Ce qui reste — MODÉLISATION

- **Import** : 7 formats **déjà écrits et câblés** dans `NkMeshSystem::Import`
  (glTF/GLB, OBJ, STL, PLY, FBX, DAE, USD/USDA) — **aucune API exposée à l'hôte**,
  les entrées de menu « Importer »/« Exporter » ne font rien.
  Comportement décidé (cf. `IMPORT_EXPORT_VISION.md`) :
  - dépôt sur le **navigateur** = extraire et ranger les assets ;
  - dépôt sur la **vue 3D ou la hiérarchie** = importer dans le dossier courant
    **puis ajouter à la scène** ;
  - import **par copie**, en gardant le chemin d'origine pour réactualiser ;
  - deux gestes : bouton **et** glisser-déposer.
- **Export** : rien n'existe. OBJ d'abord, puis glTF.
- **Vision plus lointaine** (Rihen) : audio, vidéo, **SVG → extrusion 3D**,
  **plan de maison → bâtiment**.
- **Renommer ET découper `NkDemo3D.cpp`** — décidé le 6 août : **15 642 lignes**
  (+4 000 sur une semaine) nommées « Demo » alors que c'est le cœur de
  l'application. À faire **une fois la sauvegarde validée et poussée**, jamais
  pendant un chantier fonctionnel. Découper **par domaine** (vue, outils,
  enregistrement/vidéo, matériaux) ; la façade `NkDemo3DHost.h` reste la porte
  d'entrée unique et devient `NkModelerHost.h`.

---

## 7. Chantier rendu — GARÉ (décision de Rihen : la sauvegarde d'abord)

Ne pas y retourner sans son accord.

**Fait, compilé, techniques standard** : biais du plan récepteur dans le PCF
(`NkShadowAtlas.glsli`, avec repli si déterminant quasi nul et extrapolation
bornée), biais normal en **texels du tile** (0.15), garde des faces d'omni
dimensionnée sur le noyau PCF réel, douceur affectant enfin Vulkan/GL comme DX11,
occludeurs utilisateur dans le GI voxel.

**Restant, constaté par Rihen** :

- **Bandes lumineuses** à l'intérieur d'un objet fermé = **résolution de la
  grille voxel** (64×32×64) : les cônes enjambent les parois minces. Pistes :
  décaler l'origine des cônes d'un demi-voxel, ou gonfler légèrement les
  occludeurs.
- **Écart d'ombre résiduel** : probablement la **pénombre PCF** (le noyau étale
  le bord). Vrai remède : durcissement au contact (**PCSS**).
- **`Build/ShaderCache` a été vidé** le 7 août : si sa clé ne hache pas les
  `#include` résolus, des tests ont pu tourner sur d'anciens binaires de shader.
  **À vérifier en premier** si un correctif de shader « ne fait rien ».

---

## 8. Méthode de travail imposée (Rihen)

- **Un point à la fois**, validé par lui avant de passer au suivant.
- **Faire tourner, pas seulement compiler.** « Ça compile » ne prouve rien.
- **Vérifier les affirmations des agents** : un agent a rapporté « 704 paires sur
  disque » — elles n'existaient nulle part. Tout artefact annoncé (fichier,
  binaire, chiffre) est **vérifiable** : le vérifier avant de le transmettre.
- **Diagnostiquer par le journal** (`logs/app.log`) et par des **tests isolés**
  hors application.
- **Lire le fichier réel de Rihen** plutôt que raisonner sur le code : c'est ce
  qui a fini par désigner `NkProjectLoad` (après deux fausses pistes).

---

## 9. État du dépôt au 8 août 2026

- **Rien n'est poussé** depuis `1a2d9551` (cours NkCanvas/NKGui). Plusieurs
  centaines de fichiers modifiés en attente.
- **Ne jamais pousser** : `CARNET.private.md`, `CLAUDE.md`,
  `PRINCIPES_CONCEPTION.private.md`, `ETAT_TRAVAUX.md`.
- **Exclure** `qwen2_lora_r8.nkla` (77 Mo d'artefact d'entraînement à la racine).
- Projets de test de Rihen : `C:/Users/Rihen/NK3DModeler/MonProjet` et
  `MonProjet2`. Copies de sûreté créées : `.bak` (état corrompu d'origine) et
  `.ecrase` (état après perte). **Ne pas les supprimer sans son accord.**

### NKAI (autre chantier, pour information)

- Jalons 5 et 6 **atteints et prouvés** : inférence 7B GPU (9/9), LoRA réel
  (16/16, gradient vérifié par différences finies à 1,46 × 10⁻⁴, validation
  1,6613 → 0,6292).
- `NKQwen2Train` : entraînement **reprenable** (NKLA v2 avec moments d'Adam,
  écriture atomique sur deux emplacements alternés).
- **Reste** : le corpus Nkentseu (jalon 7), et **porter LoRA dans le chemin
  rapide** (`NkQwen2Gpu`) — `NKQwen2Ask` met **823 s pour 120 tokens** faute de
  KV-cache, contre 0,5 s/token pour `NKQwen2Chat`.

---

## 10. Prompt à donner au prochain agent

> Tu reprends le développement de **NK3DModeler**, le modeleur 3D de Nkentseu
> (dépôt `D:\Projets\2026\Nkentseu\Nkentseu`). Travaille et réponds **en
> français**.
>
> **Lis d'abord `CLAUDE.md`, puis
> `Applications/NK3DModeler/PASSATION_2026_08_08.md`** : il contient l'état
> exact, les décisions déjà prises (à ne pas re-débattre) et les pièges vérifiés.
>
> **Contexte à connaître** : Rihen a **perdu du travail trois fois** dans la
> session précédente. La sauvegarde a été réparée en partie, mais **un défaut
> d'architecture demeure** : les onglets sont la seule source de vérité des
> scènes, donc **fermer un onglet supprime la scène**, et le navigateur de projet
> n'est **pas persisté**. C'est ta **priorité absolue** (§1 et §4 de la
> passation).
>
> **Règle absolue posée par Rihen** : **fermer un onglet ne doit JAMAIS supprimer
> quoi que ce soit** — ni une scène, ni un mesh, ni une texture, ni un matériau,
> ni aucun autre type de fichier. Un onglet est une **vue** sur un asset, jamais
> l'asset lui-même.
>
> **Le chantier n'est pas « sauvegarder une scène » mais « persister TOUS les
> types de fichiers pris en charge »**, chacun dans les **deux** modes : fichier
> séparé, ou blob dans le fichier de projet. Types concernés : projet, scène,
> mesh, texture, matériau, mesh procédural, et les suivants. Le détail est au
> §4.0 de la passation — lis-le avant d'écrire une ligne.
>
> **Ordre de travail décidé par Rihen** :
> 1. Persistance de tous les types de fichiers — découpler assets et onglets,
>    persister le navigateur, sélecteur de fichiers **personnalisé** basé sur
>    `NKEditorKit/NkFilePicker.h` (surtout **pas** `NkDialogs::`, cassé).
> 2. Import/export des modèles (7 formats déjà écrits, aucun exposé).
> 3. Matériaux — **la physique de surface d'abord** (clearcoat et subsurface sont
>    déjà calculés par le shader mais invisibles ; puis transmission + IOR,
>    anisotropie, sheen), ensuite le mélange, ensuite le nodal **via NKGraph**.
>
> **Règles impératives** : aucune STL ; jamais de PowerShell pour réécrire une
> source ; fermer l'application avant de compiler ; builder **Debug ET Release**
> en vérifiant **29/29** avec `--target NK3DModeler` (le build global échoue pour
> une raison connue et documentée) ; commentaires expliquant le **pourquoi**.
>
> **Méthode exigée** : un point à la fois, validé par Rihen avant le suivant.
> **Faire tourner, pas seulement compiler.** Pour la persistance, deux tests font
> foi et aucun ne se déduit du code : *(1)* créer deux scènes, modifier,
> enregistrer, **fermer l'application**, rouvrir, et **tout** retrouver — y
> compris les scènes qui n'étaient pas ouvertes ; *(2)* pour chaque type d'asset,
> l'ouvrir, **fermer son onglet**, et vérifier qu'il est toujours dans le
> navigateur, intact et réouvrable. Ne jamais surévaluer un résultat : dire
> clairement ce qui ne marche pas.
