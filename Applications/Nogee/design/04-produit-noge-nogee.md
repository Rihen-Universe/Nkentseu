# Noge et Nogee — ce qu'on attend du moteur et de son éditeur

### Document 4 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 26/09/2026 à partir de la demande de Rodolf : *« Noge est le moteur,
> Nogee l'éditeur du moteur de jeu, style Unreal 5. »*
>
> **Ce document s'appuie sur** l'inventaire mesuré de `Engine/Noge/ROADMAP.md`
> (§7bis, 2026-08-17 ; ses chiffres ont pu bouger depuis), la note de reprise
> `Engine/Noge/HANDOFF.md`, et la règle des éditeurs du ROADMAP §9 (*« les éditeurs
> partagent les briques et le thème, jamais la peinture »*).
>
> **Il remplace**, pour le périmètre et les priorités, les documents
> `Applications/Nogee/design/01`, `02`, `03` (« Aetherion Engine ») : ils restent
> un **réservoir de détails** (ils sont précis sur le Content Browser, le
> Reference Viewer, le Size Map…). En cas de désaccord, **ce document gagne**.
>
> Compagnons : **05** (interface pour un humain), **06** (interface pour
> l'agent). Même famille que NkAnima (07-09) et NKScena (01-03).

---

## 0. En une phrase

**Noge est le moteur de jeu 3D de Rihen — ce qui fait tourner un jeu sur toutes
les plateformes — et Nogee est l'éditeur où l'on construit ce jeu : on y assemble
les niveaux, on y branche le gameplay en C++ ou en graphes visuels, on y joue
instantanément, et on en sort un jeu empaqueté pour Windows, Android, le Web et
les consoles.**

---

## 1. Positionnement

| référence | ce qu'on lui prend |
|---|---|
| **Unreal Engine 5** | **l'éditeur** : sa coquille, son panneau Détails, son Content Browser, le jeu dans l'éditeur (Play In Editor), les Blueprints, l'éditeur de matériaux, le packaging par plateforme |
| **Unity** | la **simplicité** d'entrée : un objet, des composants, un script, ça tourne |
| **Godot** | la **légèreté** : un éditeur qui s'ouvre vite, un projet qui se lit |

Et ce que Noge/Nogee a en propre (vision de `HANDOFF.md`, à ne pas perdre) :

1. **Simplicité** : une petite application 2D/3D s'écrit en incluant
   `<Nkentseu.h>` ; l'éditeur ne cache pas le code, il l'accélère.
2. **Puissance** : aucun plafond — tout le moteur est à nous, jusqu'au RHI et au
   compilateur de shaders.
3. **IA en assistance, supervision humaine, prompt non obligatoire** : l'IA
   propose, l'humain décide, et **tout se fait aussi sans elle**.
4. **Une famille d'outils**, pas un logiciel : Nogee **reçoit** ce que font
   NKCraft, NkAnima, NKScena et NKUIDesign, au lieu de tout refaire moins bien.

---

## 2. Noge, Nogee, et les autres

```
                 ┌───────────── Noge (moteur) ─────────────┐
                 │ ECS · rendu · physique · animation ·     │   ← fait tourner le jeu,
                 │ audio · entrées · réseau · navigation ·  │     dans Nogee ET dans
                 │ IA de jeu · UI de jeu · scripts · E/S    │     le jeu livré
                 └─────────────────────▲────────────────────┘
                                       │
 NKCraft ──(maillages, matériaux)──┤
 NkAnima ──(clips, graphes d'animation,├──▶ Nogee (éditeur) ──(Jenga)──▶ jeu empaqueté
            comportements, VFX)────────┤      niveaux, gameplay,           Windows · Linux ·
 NKScena ──(cinématiques)──────────────┤      scripts, test, build         Android · Web · …
 NKUIDesign ──(interfaces .nkgui)──────┤
 NKCode ──(code C++ du jeu)────────────┘
```

| dans Nogee | ailleurs |
|---|---|
| **projets** et **niveaux** : ouvrir, créer, enregistrer, découper en sous-niveaux | modéliser → **NKCraft** |
| **placer** : acteurs, lumières, volumes, déclencheurs, points d'apparition | animer, riguer, VFX, comportements d'agents → **NkAnima** |
| **monde** : terrain, végétation, eau, ciel (**la même brique que NKScena**) | cinématiques → **NKScena** |
| **gameplay** : composants, modes de jeu, contrôleurs, caméras de jeu, entrées | concevoir les écrans d'interface → **NKUIDesign** |
| **scripts** : C++ (rechargé à chaud) et **Blueprints** (graphes visuels) | écrire le C++ en profondeur → **NKCode** (Nogee ouvre le fichier dans NKCode) |
| **matériaux** : l'éditeur de graphes de matériaux | compiler, empaqueter, déployer → **Jenga** (piloté par Nogee) |
| **physique de jeu** : profils de collision, matières, déclencheurs | |
| **audio de jeu** : sons placés, atténuation, mélanges, musique interactive | |
| **navigation** et **IA de jeu** : maillage de navigation, arbres de comportement | |
| **jouer dans l'éditeur**, simuler, déboguer, profiler | |
| **construire et empaqueter** par plateforme | |

📌 **2D** : le moteur 2D de la maison est **Unkeny** (sur NKCanvas), avec son
propre éditeur. Noge est **3D d'abord**. Ce document ne couvre pas Unkeny.

📌 **Une seule brique de « monde »** (terrain, dispersion, ciel, eau) sert Nogee
**et** NKScena. Deux éditeurs de terrain divergeraient — c'est la règle du
ROADMAP §9.

---

## 3. Noge — le moteur

### 3.1 Les piliers, et leur état réel

L'état est celui **mesuré** par le ROADMAP (août 2026) complété par ce qui a
bougé depuis ; ✅ prouvé (compilé, lié **et** exercé) · 🔶 partiel · 🔴 spécifié
seulement (en-têtes sans corps, ou sans appelant).

| pilier | ce qu'il doit faire | état |
|---|---|---|
| **Application** | boucle, couches (LayerStack), bus d'événements, point d'entrée public `<Nkentseu.h>` | ✅ (le point d'entrée public a longtemps été non essayé — ROADMAP §4) |
| **ECS** | monde, entités, composants, requêtes, systèmes ordonnés (NKECS à archétypes + `Noge/ECS/Systems`) | ✅ |
| **Scène** | hiérarchie, prefabs, **sérialisation par composant** (`.nkscene`) | 🔶 — la sérialisation par composant produisait des archives vides en août |
| **Rendu** | NKRenderer : PBR, IBL, ombres virtuelles, reflets, bloom, ACES, occlusion voxel ; 4 backends GPU à parité (Vulkan, OpenGL, DX11, DX12) + logiciel ; Web | ✅ côté NKRenderer · 🔶 le **pont matériaux** Noge ↔ NkSL (`materialHandle` à 0 — bloqueur n°1 de HANDOFF) |
| **Physique** | corps rigides, liaisons, véhicules, ragdoll, tissu, déclencheurs, requêtes (rayons, formes) | 🔶 — NKPhysics existe (10 000 lignes) ; le **pont ECS** et l'IK des pieds se branchent ces jours-ci |
| **Animation** | lecture de clips, mélange, machines à états, IK, retargeting, facial | 🔶 — `Anim` exercé ; `Facial`, `Anim2D` spécifiés seulement |
| **Audio** | NKAudio : 256 voix, HRTF, bus, flux | ✅ |
| **Entrées** | clavier, souris, tactile, manette ; **actions et axes** configurables | ✅ bas niveau · 🔴 couche d'actions |
| **Réseau** | sockets, UDP fiable, RPC, lobby, **réplication** d'entités | 🔶 — transport présent, réplication ECS non implémentée |
| **Navigation** | maillage de navigation, recherche de chemin, évitement | 🔶 — NKNavigation ≈ 560 lignes, composant d'agent ECS présent |
| **IA de jeu** | arbres de comportement, perception, agents (et les comportements de NkAnima) | 🔶 — `NkAgentComponent`, `NkAgentSystem` ; NKAI pour le reste |
| **UI de jeu** | NKGui + documents `.nkgui` (ceux de NKUIDesign) ; HUD, menus | 🔶 — le monteur `.nkgui` fonctionne, avec ses limites connues |
| **Scripts** | C++ rechargé à chaud ; Blueprints | ✅ C++ (`NkScriptBridge`, exercé par `NkHotReloadDemo`) · 🟡 Blueprint : interpréteur réel, **zéro consommateur**, pas d'éditeur |
| **VFX de jeu** | particules, fluides, eau (NKRenderer) ; effets de NkAnima | 🔶 — systèmes ECS présents (`NkParticleSystem`, `NkWaterSystem`, `NkFluidVolumeSystem`) |
| **E/S et ressources** | import (OBJ, glTF, FBX, DAE, PLY, STL, USDA), cuisson, streaming | ✅ import · 🔴 cuisson par plateforme |
| **Plateformes** | Windows, Linux, Android, Web en production ; macOS, iOS, HarmonyOS prêts ; Xbox partiel ; consoles sous licence | ✅ (fenêtrage et RHI) |
| **XR** | OpenXR, AR | 🔶 — NKXR |

⚠️ **Ce que le ROADMAP a appris à ne plus faire** : *« 3 369 lignes d'en-têtes
soignés, sans corps et sans appelant, se lisent exactement comme des
fonctionnalités livrées. »* Ce document **ne compte comme livré que ce qui est
exercé**. Les 11 modules spécifiés sans corps (Facial, Anim2D, Viewport,
Physics, Selection, Systems, Sculpt, UV, Text, Crowd, et Sequencer jusqu'au 13/09)
ne sont **pas** des acquis.

### 3.2 Le cadre de gameplay

Ce qu'un développeur de jeu attend en ouvrant le moteur, dans les mots du jeu :

| notion | ce que c'est | appui Noge |
|---|---|---|
| **Acteur** | une chose du monde, avec des composants | `NkActor`, `NkGameObject` |
| **Composant** | un morceau de comportement ou de donnée | ECS |
| **Mode de jeu** | les règles de la partie (conditions de victoire, apparition) | à écrire |
| **Contrôleur** | ce qui pilote un acteur : joueur (entrées) ou IA | à écrire |
| **Caméra de jeu** | suivi, bras à ressort, rail, secousse | composants caméra (deux doublons à fusionner — HANDOFF §6) |
| **Instance de jeu** | ce qui survit d'un niveau à l'autre | à écrire |
| **Sauvegarde** | l'état de la partie sur disque | sérialisation par composant |
| **Localisation** | les textes du jeu en plusieurs langues | à écrire |

📌 **Aucun de ces noms n'impose une classe de base** : ce sont des **rôles**
tenus par des composants. Un jeu de plateau (SONGO, NKORA) n'a pas de caméra à
bras, un jeu de tir n'a pas de plateau.

### 3.3 Les scripts — une décision

| piste | proposition | raison |
|---|---|---|
| **C++ rechargé à chaud** | **la voie principale** | prouvée, exécutée, et c'est la langue du moteur |
| **Blueprints** | **la voie visuelle**, construite sur `NKGraph` (le substrat commun) et l'interpréteur `NkBlueprint` existant | c'est ce qu'attend l'utilisateur d'un moteur « style Unreal » |
| **C# (Mono)** | **gelé** | squelette mort (macro jamais définie, aucun `.cs`) ; le relancer coûte un runtime entier |
| **Python** | **gelé** pour le jeu | idem ; Python reste l'outil de **Jenga** et de NKCode, pas du jeu |

**Blueprint et C++ se parlent** : une fonction C++ marquée comme exposée apparaît
comme nœud ; un Blueprint peut hériter d'une classe C++. Un Blueprint peut être
**converti en C++** (lecture seule d'abord, pour apprendre).

---

## 4. Nogee — l'éditeur

### 4.1 Projet et niveaux

- **Lanceur** : projets récents, **nouveau projet depuis un modèle**, ouvrir.
- **Modèles de jeu** : Vide · Troisième personne · Première personne · Vue du
  dessus · Plateforme 3D · Course · **Jeu de plateau** (la maison en a trois :
  SONGO, NKORA, Mú) · Réalité virtuelle.
- **Niveaux** (`.nkscene`) et **sous-niveaux** (chargés à la demande, pour les
  grands mondes).
- **Projet** (`.nkproj`) : réglages, plateformes, cartes d'entrées, profils de
  collision, langue.

### 4.2 Construire un niveau

- **Placer** depuis la palette (formes, lumières, volumes, déclencheurs, points
  d'apparition, caméras, sons, effets) ou depuis le Content Browser (glisser).
- **Transformer** avec les gizmos (déplacement, rotation, échelle ; local / monde ;
  aimantation à la grille, aux surfaces, aux sommets).
- **Grouper**, **aligner**, **dupliquer en réseau** (une rangée de lampadaires).
- **Modes** : Sélection · **Monde** (terrain, végétation, eau — la brique partagée
  avec NKScena) · **Peinture** (couleur de sommets, masques) · **Volumes**
  (navigation, post-traitement, audio, déclencheurs).
- **Blocage rapide** (« greybox ») : formes simples éditables pour dessiner un
  niveau avant d'avoir les modèles. ⚠️ C'est la **seule** modélisation dans
  Nogee ; au-delà, NKCraft.

### 4.3 Les éditeurs d'assets

Chaque type d'asset s'ouvre dans **son éditeur**, dans un nouvel onglet (comme
UE5) :

| éditeur | pour | appui |
|---|---|---|
| **Blueprint** | le comportement visuel d'une classe | `NKGraph` + `NkBlueprint` |
| **Matériau** | le graphe d'un matériau | `NkMatGraph` (existe, a sa démo) |
| **Aperçu de maillage** | collisions, LOD, sockets d'un maillage importé | NKRenderer |
| **Animation (lecture)** | voir un clip, un graphe d'animation, **les ouvrir dans NkAnima** | NKAnima ; l'édition est dans NkAnima (doc 07 D2) |
| **Arbre de comportement** | l'IA de jeu d'un ennemi, d'un PNJ | à écrire, sur `NKGraph` |
| **Son** | un son composé (aléatoire, couches, atténuation) | NKAudio |
| **Interface** | voir un document `.nkgui`, **l'ouvrir dans NKUIDesign** | NKGui |
| **Table de données** | des lignes typées (armes, dialogues, niveaux) | NKSerialization |
| **Physique** | profils de collision, matières physiques | NKPhysics |

📌 **Trois éditeurs de graphes (Blueprint, Matériau, Arbre de comportement) et un
seul substrat** : `NKGraph`. Même navigation, même recherche de nœuds, mêmes
raccourcis. C'est aussi le substrat du compositing de NKScena.

### 4.4 Jouer

- **Jouer dans l'éditeur** (▶) : le jeu démarre **dans la vue**, sans
  compilation longue ; Échap arrête. Options : dans la vue · dans une nouvelle
  fenêtre · **sur l'appareil** (Android, Web) · **plusieurs joueurs** (N fenêtres,
  serveur local).
- **Simuler** : le monde tourne (physique, IA), sans joueur ; on peut sélectionner
  et inspecter en direct.
- **Éjecter** (F8) : pendant le jeu, sortir de la caméra du joueur pour
  inspecter, puis revenir.
- **Garder les changements faits pendant le jeu** : clic droit sur un acteur
  modifié → « Conserver les valeurs de simulation ».
- **Recharger le C++ à chaud** sans arrêter le jeu.

### 4.5 Déboguer et mesurer

- **Journal** filtrable (catégorie, sévérité) et **console** de commandes.
- **Débogueur de Blueprint** : points d'arrêt, chemin d'exécution en surbrillance,
  valeurs au survol.
- **Profileur** : temps par système ECS, par passe de rendu, mémoire (par
  allocateur NKMemory — c'est un avantage de posséder l'allocateur), réseau.
- **Statistiques** dans la vue : images par seconde, appels de dessin,
  triangles, mémoire GPU.
- **Visualiseurs** : collisions, navigation, audio, réseau.

### 4.6 Construire, empaqueter, déployer

- **Plateformes** : un tableau des cibles avec leur **état de préparation** (outils
  présents ou non : SDK Android, JDK, clés de signature…) et ce qui manque.
- **Construire** : Nogee **pilote Jenga** (`jenga build --platform …`) et affiche
  sa sortie ; aucune compilation cachée.
- **Cuire** les ressources par plateforme (textures compressées, shaders
  compilés par NkSL pour la cible).
- **Empaqueter** : exécutable, APK, paquet Web ; **lancer sur l'appareil**.
- **Profils** : Développement (journaux, console) · Expédition (optimisé, sans
  outils).

### 4.7 L'IA dans Nogee

Mêmes règles que les trois autres applications : **commandes existantes**,
**aperçu**, **retouchable**, **marquée**, et **jamais obligatoire**.

| capacité | exemple |
|---|---|
| placer | « une rue de marché avec étals et lampadaires » → acteurs placés, en aperçu |
| Blueprint | « quand le joueur entre dans la zone, ouvre la porte et joue le son » → nœuds proposés |
| expliquer | une erreur de compilation ou de Blueprint → explication et correction proposée |
| optimiser | « pourquoi ce niveau tourne à 40 ips ? » → lecture du profileur, pistes |
| données | « 20 armes équilibrées » → lignes de table proposées |
| test | « joue le niveau et cherche les endroits où l'on reste bloqué » → rapport (agents de NkAnima/NKAI) |

---

## 5. Phasage

**Une tranche qui marche > un scaffold de plus.** Aligné sur les trois horizons du
ROADMAP §8.

| palier | contenu | critère de fin |
|---|---|---|
| **P0 — le socle prouvé** | `<Nkentseu.h>` utilisable ; **sérialisation par composant réelle** ; une **suite de tests exécutable** pour Noge | un niveau de 50 acteurs sauvé, fermé, rouvert, **identique** ; `jenga test` sur Noge passe et échoue quand on casse exprès |
| **P1 — le niveau** | Nogee : projet, niveau, Content Browser, Scène, Détails, Vue, gizmos, annuler, **tout depuis les `.nkgui`** | on construit un niveau de blocage de 20 acteurs, on le sauve, on le rouvre, sans une ligne de code |
| **P2 — jouer** | jouer dans l'éditeur, simuler, éjecter ; C++ rechargé à chaud ; cadre de gameplay (mode de jeu, contrôleur, caméras) ; **entrées par actions** ; physique de jeu via le pont ECS ; audio placé ; **modèle Troisième personne** | le modèle Troisième personne se joue dans l'éditeur ; une modification C++ est rechargée **sans arrêter le jeu** |
| **P3 — construire** | Plateformes, cuisson, empaquetage via Jenga : **Windows, Android, Web** ; profils Développement / Expédition | le même projet produit un exécutable, un APK et un paquet Web depuis Nogee, et les trois se lancent |
| **P4 — les graphes** | éditeur de **Blueprints** (NKGraph + NkBlueprint), débogueur ; éditeur de **Matériaux** (NkMatGraph) ; UI de jeu depuis NKUIDesign | la logique d'une porte à déclencheur écrite en Blueprint, sans C++, et déboguée par point d'arrêt |
| **P5 — le monde vivant** | brique Monde (terrain, végétation, eau, ciel, partagée avec NKScena) ; sous-niveaux ; navigation ; **arbres de comportement** ; animation de NkAnima jouée ; profileur | un PNJ patrouille, voit le joueur, le poursuit sur un terrain de 1 km² chargé par morceaux |
| **P6 — ensemble** | réseau : **réplication** ; jouer à plusieurs dans l'éditeur ; IA assistant ; modèles restants (dont Jeu de plateau) ; XR ; consoles | deux joueurs en réseau local sur le modèle Troisième personne, lancés depuis Nogee |

---

## 6. Règles transverses

1. **Toute action de l'éditeur est une commande** : annulable, rejouable,
   scriptable ; l'interface, le script et l'IA passent par la même porte.
2. **Ne compte comme livré que ce qui est exercé** (leçon du ROADMAP §7bis).
3. **Ce qui tourne dans Nogee est le moteur du jeu livré** : jouer dans l'éditeur
   n'est pas une simulation du jeu, c'est le jeu.
4. **Nogee ne refait pas les autres applications** : il ouvre dans NKCraft,
   NkAnima, NKScena, NKUIDesign, NKCode, et récupère le résultat.
5. **Les éditeurs partagent les briques et le thème, jamais la peinture**
   (ROADMAP §9) ; une brique générique s'écrit dans **NKEditorKit**.
6. **Zéro-STL, mémoire par NKMemory, journal par NKLogger**, conventions du dépôt.
7. **L'interface vient des documents `.nkgui`**, style **Unreal Engine 5, net**,
   thème, jetons et composants **communs** avec NkAnima et NKScena.
8. **Fermer un onglet ne supprime jamais rien.**

---

## 7. Décisions

| # | question | proposition | raison |
|---|---|---|---|
| D1 | scripts C# et Python pour le jeu | **gelés** | squelettes morts ; C++ à chaud + Blueprints suffisent |
| D2 | graphes d'animation de jeu | **édités dans NkAnima**, **lus et joués** dans Nogee | cohérent avec le doc 07 de NkAnima (D2) |
| D3 | cinématiques | **faites dans NKScena**, **déclenchées** dans Nogee | cohérent avec le doc 01 de NKScena (D4) |
| D4 | la brique Monde (terrain, végétation, eau, ciel) | **une** brique de l'Editor Kit, partagée Nogee / NKScena | deux éditeurs de terrain divergeraient |
| D5 | les 11 modules spécifiés sans corps de Noge | chacun **réalisé quand un palier l'exige, ou retiré** — pas conservé « pour plus tard » | un en-tête sans corps se lit comme une fonctionnalité |
| D6 | les doublons de HANDOFF §6 (deux caméras ECS, deux jeux de composants audio…) | **fusionnés avant P2** | le cadre de gameplay s'appuie dessus |
| D7 | 2D | **Unkeny**, hors de ce document | décision existante |
