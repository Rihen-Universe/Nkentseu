# NKScena — ce qu'on attend de l'application

### Document 1 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 26/09/2026 à partir de la demande de Rodolf du même jour :
> *« notre application de réalisation de film d'animation : scène, décor,
> montage, post-processing, bref la totale, avec des rendus temps réel et
> précalculés au choix, pour une qualité optimale et une durée de rendu précise —
> mais pour un début, temps réel. »*
>
> **Ce document étend** `Applications/NKScena/ROADMAP.md` (13/09) **sans le
> contredire** : la définition de départ (« NKScena pose le temps sur une scène que
> d'autres ont construite ») reste vraie, et sa **condition de naissance** reste
> la première marche (§11, P0). Ce qui change : le périmètre s'élargit du seul
> séquenceur à **toute la réalisation**, et deux questions ouvertes du ROADMAP
> reçoivent une réponse (§12).
>
> Documents compagnons : **02** (interface pour un humain), **03** (interface
> pour l'agent qui écrit les `.nkgui`). Même famille que les documents 07-09 de
> NkAnima, dont ils reprennent le thème, les jetons et les composants.

---

## 0. En une phrase

**NKScena est le studio de réalisation de Rihen : on y prépare le film (texte,
découpage, storyboard), on y installe le décor, la lumière et les caméras, on y
place les acteurs animés dans NkAnima, on y monte les plans, on y mixe le son, on
y étalonne l'image — et on rend le film en temps réel ou en précalculé, en sachant
à l'avance combien de temps ça prendra.**

---

## 1. Positionnement

| famille | ce qu'on lui prend |
|---|---|
| **Unreal Engine (Sequencer, Movie Render Queue)** | la réalisation **en temps réel** dans un moteur ; le plan comme unité ; la file de rendu |
| **Blender (VSE, compositeur)** | la chaîne complète dans un seul outil ; le compositing par nœuds |
| **DaVinci Resolve** | la **montée en puissance** d'un même projet du montage à l'étalonnage au mixage, par pages |
| **Storyboarder / Toon Boom Storyboard** | la préproduction : texte → planches → animatique |

Et ce qu'aucun ne fait ensemble :

- **le montage est vivant** : un plan monté n'est pas une vidéo figée, c'est une
  **référence** vers la scène 3D. On modifie la caméra d'un plan, le montage se
  met à jour. On change le montage, la scène le sait ;
- **deux rendus, un seul projet** : chaque plan se rend en **temps réel** ou en
  **précalculé**, et on peut passer de l'un à l'autre sans rien refaire ;
- **la durée de rendu est une donnée** : NKScena **estime** avant de rendre, **tient
  un budget** si on lui en donne un, et **mesure** son erreur après ;
- **l'IA maison** (NKAI) propose un découpage, un cadrage, un premier montage, un
  étalonnage — toujours en **commandes** éditables.

**Public** : le réalisateur, le chef opérateur (caméra et lumière), le monteur,
l'étalonneur, le mixeur — souvent **une seule personne** dans un petit studio.
L'application doit servir cette personne seule **et** une équipe.

---

## 2. Frontières

```
NKCraft ──(décors, accessoires, matériaux)──┐
                                                ├──▶ NKScena ──▶ le film (vidéo + son)
NkAnima ──(acteurs animés, clips, caches VFX,   │      │
           comportements de foule)──────────────┘      └──▶ cinématiques de jeu ──▶ Nogee / Noge
```

| dans NKScena | hors de NKScena |
|---|---|
| **préproduction** : texte, découpage, storyboard, animatique | modéliser un décor ou un accessoire → **NKCraft** |
| **décor** : assembler, poser, disperser (végétation, rochers), terrain, ciel, atmosphère | animer un corps, une physique, un effet → **NkAnima** |
| **lumière** : sources, soleil, ciel, IBL, volumétrie, jour/nuit | écrire un comportement de foule → **NkAnima** |
| **acteurs** : placer les personnages et leurs clips, **placer** les foules | la logique de jeu → **Nogee** |
| **caméras** : caméras physiques, rigs, visée, caméra virtuelle | |
| **plans** : le séquenceur de chaque plan (acteurs, caméra, lumière, effets, son, événements) | |
| **animation simple** : clés sur caméras, lumières, décor (une porte, un véhicule sur un rail) | |
| **montage** : plans, coupes, transitions, versions | |
| **son** : dialogues, bruitages, musique, mixage, spatialisation | |
| **post-production** : étalonnage, compositing, effets image, titres, sous-titres | |
| **rendu** : temps réel et précalculé, file de rendu, passes, formats | |
| **revue** : lecture, comparaison de versions, annotations | |

📌 **La limite de l'« animation simple »** : dès qu'il y a un **squelette**, une
**physique** ou un **effet**, c'est NkAnima. Une porte qui s'ouvre sur deux clés,
NKScena ; une porte arrachée par une explosion, NkAnima. Un bouton **« Ouvrir dans
NkAnima »** envoie l'élément et le **récupère mis à jour** (§6).

📌 **NKScena n'invente pas de format de scène** (ROADMAP §2) : elle ouvre et
enregistre des **scènes Noge**, et ajoute ce qui lui est propre (plans, montage,
réglages de rendu) dans son projet.

---

## 3. L'unité de travail : Film → Séquence → Plan → Prise

| niveau | ce que c'est | exemple |
|---|---|---|
| **Film** | le projet | *« Le Pêcheur de Kribi »* |
| **Séquence** | une unité de lieu et de temps | « Séq. 3 — La plage, aube » |
| **Plan** | une caméra, une durée, **une scène 3D** (ou une variante) | « 3-040 — plan large, Kofi arrive » |
| **Prise** | une version d'un plan (autre cadrage, autre jeu, autre lumière) | « 3-040 prise B » |

- Chaque plan porte un **état** : à faire · en cours · à revoir · validé.
- Les plans d'une même séquence **partagent la scène** (le décor, la lumière de
  base) et peuvent la **surcharger** (une lumière de plus pour un gros plan) sans
  toucher aux autres.
- Le **montage** range des **prises** de plans sur une timeline.

📌 C'est cette structure, et non la timeline seule, qui rend le montage vivant :
le montage pointe vers une prise, la prise vers une scène.

---

## 4. Préproduction

1. **Texte** : un éditeur de scénario (format de scène : intitulé, action,
   personnage, dialogue, didascalie). Import de texte brut. Les **personnages**
   et les **lieux** sont reconnus.
2. **Découpage** : le texte est découpé en séquences et en plans (à la main, ou
   **proposé par l'IA**). Chaque plan reçoit : valeur de cadre (très gros plan →
   plan d'ensemble), angle, mouvement, durée estimée, personnages, lieu.
3. **Storyboard** : une **planche** par plan — dessin libre (tablette), image
   importée, **ou capture d'une vue 3D** du décor avec des mannequins posés
   (le storyboard 3D). Notes et flèches de mouvement par planche.
4. **Animatique** : les planches **à la bonne durée**, avec le dialogue (voix
   provisoire : enregistrement ou **synthèse vocale**, existant : `NKSpeech/NkTTS`)
   et la musique témoin. C'est déjà un premier **montage**, dans la même
   timeline que le film final.
5. **Du storyboard au plan 3D** : chaque planche devient un plan 3D ; la planche
   reste visible en **surimpression** dans la vue caméra pour caler le cadrage.

---

## 5. Décor, lumière, atmosphère

### 5.1 Décor

- **Assembler** des éléments venus de NKCraft (glisser depuis la
  Bibliothèque), avec aimantation au sol et aux surfaces.
- **Terrain** (existant : `NkTerrainHeightMap`, `NkTerrainSplat`,
  `NkTerrainSable`) : sculpter le relief, peindre les matières.
- **Dispersion** : peindre de la végétation, des rochers, des débris — densité,
  échelle, orientation, variation, exclusion près des chemins. La **réponse au
  vent** est celle réglée dans NkAnima (doc 07, D4).
- **Ciel et atmosphère** : modèle de ciel physique (existant : Hosek-Wilkie,
  Prague), heure et lieu → position du soleil, nuages, brouillard, brume
  hauteur, **volumétrie** (rayons de lumière).
- **Eau** : plans d'eau et océan viennent de NkAnima (caches ou océan
  procédural), NKScena les **place**.

### 5.2 Lumière

- Sources : soleil, ponctuelle, spot, surfacique, ciel (IBL, existant :
  `NkIBLCompute`), émissive.
- **Préréglages de chef opérateur** : éclairage trois points, contre-jour,
  heure dorée, nuit bleue, intérieur tungstène.
- **Liens de lumière** : une lumière n'éclaire que certains éléments (la lumière
  d'ambiance du gros plan n'éclaire que le visage).
- **Température de couleur** en kelvins, intensité en unités physiques (lux,
  lumens) **et** en valeur artistique.
- Tout s'anime dans le plan (le soleil se lève sur la séquence).

---

## 6. Acteurs

- **Placer** un personnage de NkAnima dans la scène, et lui **assigner** ses
  clips sur la piste du plan (existant : `NkClipOnTrack`, `NkNLATrack`).
- **Marques au sol** : les positions de départ et d'arrivée d'un acteur, visibles
  dans la vue ; le clip est recalé sur la marque.
- **Ouvrir dans NkAnima** : envoie l'acteur **dans le contexte du plan** (décor,
  caméra, durée) ; au retour, le plan est mis à jour. Tant qu'un acteur est
  ouvert dans NkAnima, il est **marqué** dans NKScena.
- **Foules** : placer un comportement de foule (écrit dans NkAnima) dans une zone,
  avec un nombre, une densité, des chemins.
- **Caches VFX** : placer un effet cuit dans NkAnima (explosion, fumée, vague)
  et le caler dans le temps.

---

## 7. Caméras

- **Caméra physique** : focale, capteur (préréglages : Super 35, plein format,
  Imax), **ouverture** et distance de mise au point (profondeur de champ),
  **obturateur** (flou de mouvement), rapport d'image (1,85 · 2,39 · 16/9 · 4/3).
- **Mise au point** : manuelle, sur un point cliqué, **suivi d'un élément**
  (les yeux d'un personnage), **bascule** animée entre deux cibles.
- **Rigs** : grue, travelling sur rail, steadicam, épaule (tremblement
  réglable), drone, orbite ; chaque rig est un objet de la scène qu'on anime.
- **Visée** : la caméra regarde une cible ; **cadrage par règles** (tiers,
  horizon, espace de regard) en guide dans la vue.
- **Caméra virtuelle** : piloter la caméra avec un téléphone ou un casque
  (existant : `NKXR`, `NkArSession`) — on filme la scène 3D comme avec une
  vraie caméra. Palier P6.
- **Plusieurs caméras par plan** (un champ-contrechamp tourné en une fois) ;
  la caméra active se choisit dans le montage.

---

## 8. Le séquenceur de plan et le montage

### 8.1 Le séquenceur de plan

Une timeline **par plan** : pistes d'acteurs (clips), de caméra, de lumière,
d'effets, de son, d'**événements** (déclencher un effet, changer une lumière),
de notes. Clés, courbes, couches. Existant : `NkSequence`, `NkTrack`,
`NkCameraTrack` (coupes `Cut` / `Blend` / `Wipe`).

📌 **La timeline est partagée avec NkAnima** (ROADMAP §5.2) : **une** brique de
l'Editor Kit, deux usages. Deux timelines divergeraient.

### 8.2 Le montage

- Une timeline **de film** : pistes vidéo (les prises de plans) et pistes audio.
- Coupe, raccord, **trim** (rogner à l'entrée et à la sortie **sans perdre** les
  images de la prise), glisser, insérer, écraser, transitions (fondu, fondu au
  noir, volet), **vitesse** (ralenti, accéléré — en temps réel, un ralenti est
  **rendu** avec plus d'images, pas interpolé).
- **Montage vivant** : un plan monté **suit** sa scène ; s'il a changé depuis le
  dernier rendu, il est marqué **périmé**.
- **Rallonger un plan au montage rallonge le plan 3D** (avec confirmation) : les
  animations continuent, la caméra aussi. C'est impossible avec une vidéo ; c'est
  naturel avec une scène.
- **Versions de montage** (Montage 1, Montage 2 — Director's cut), comparaison.
- **Marqueurs** et **notes** de revue, avec timecode.

---

## 9. Son

- Pistes : **dialogues**, **bruitages**, **ambiances**, **musique**.
- Import (existant : `NKAudio` et `NKMedia` lisent WAV, MP3, OGG Vorbis, FLAC,
  Opus, AAC).
- **Enregistrement** de voix provisoire (existant : `NkAudioCapture`).
- **Mixage** : volume, panoramique, sourdine, solo, **bus** (dialogues, effets,
  musique → master), effets (égaliseur, compresseur, réverbération), automation
  par clés, **VU-mètres** avec crête et écrêtage signalé.
- **Spatialisation** : un son peut être **attaché** à un élément de la scène
  (le pas de l'acteur) et suivre la caméra (existant : HRTF dans `NKAudio`).
- **Synchronisation labiale** : les dialogues du film sont ceux que NkAnima a
  employés (doc 07 M9) ; un décalage se voit et se corrige.
- Export : stéréo, 5.1.

---

## 10. Post-production

### 10.1 Étalonnage

- Par plan et par séquence (un étalonnage de séquence, des corrections par plan).
- **Roues** lift / gamma / gain, exposition, contraste, saturation, température,
  teinte, courbes, **LUT** (`.cube` ; un exemple existe déjà dans le dépôt),
  **correspondance de plans** (appliquer le look d'un plan à un autre).
- **Scopes** : forme d'onde, vecteurscope, histogramme.
- Espace de travail couleur **ACES** (le tonemapper ACES existe dans NKRenderer).

### 10.2 Compositing

- **Éditeur de nœuds** (sur `NKGraph`, le substrat commun) sur les **passes de
  rendu** : beauté, diffus, spéculaire, émission, profondeur, normales, vitesse,
  **identifiants d'objets et de matériaux** (masques), occlusion.
- Opérations : fusion, masque, flou, défocalisation, lueur, correction de
  couleur, incrustation, brouillard de profondeur.
- En temps réel, le compositing s'applique **en direct** dans la vue ; en
  précalculé, sur les passes enregistrées.

### 10.3 Effets image

Bloom, lueur, grain de film, vignettage, aberration chromatique, reflets
d'objectif, flou de mouvement, profondeur de champ — la plupart existent dans
`NkPostProcessStack`.

### 10.4 Titres et génériques

Cartons, titres animés, **générique défilant**, sous-titres (import / export
SRT), **filigrane** de version (« Montage 2 — non étalonné »).

---

## 11. Le rendu

### 11.1 Deux voies, un projet

| | **Temps réel** | **Précalculé** |
|---|---|---|
| **moteur** | NKRenderer en rastérisation : PBR, IBL, ombres virtuelles, reflets planaires, occlusion voxel, bloom, ACES | même scène, rendue **image par image sans contrainte de temps** |
| **qualité** | celle de la vue | **optimale** : anticrénelage par sur-échantillonnage, profondeur de champ et flou de mouvement **par accumulation** (vrais, pas des filtres), ombres et volumétrie à haute résolution ; puis **lancer de rayons** (phase R de NKRenderer) avec **débruiteur** (`NkDenoiserSystem`) |
| **durée** | celle du film (ou presque) | de secondes à minutes par image |
| **usage** | animatique, revue, cinématiques, films au style temps réel | image finale de film |

- Le choix se fait **par projet**, avec **surcharge par plan** (un plan difficile
  en précalculé, le reste en temps réel).
- **Aucun travail n'est à refaire** en passant de l'un à l'autre : même scène,
  même lumière, mêmes matériaux ; seuls les **réglages de qualité** changent.
- **Préréglages** : Brouillon · Revue · Temps réel final · Précalculé moyen ·
  Précalculé maximal.

📌 **Pour un début, temps réel** (demande de Rodolf) : les paliers P1 à P3 ne
rendent qu'en temps réel. Le précalculé arrive en P4, le lancer de rayons en P6.

### 11.2 La durée de rendu : estimer, tenir, mesurer

C'est la demande « une durée de rendu précise ». Trois mécanismes :

1. **Estimer** : avant de rendre, NKScena rend un **échantillon** de chaque plan
   (la première image, celle du milieu, et **la plus lourde** — la plus d'objets,
   de lumières, d'effets à l'écran), en mesure le temps et **extrapole** au plan
   entier, avec une **marge** affichée (« 2 h 10 ± 12 min »).
2. **Tenir un budget** : l'utilisateur donne une durée (« je veux le film rendu
   en 8 heures »). NKScena **propose** des réglages qui tiennent dans le budget,
   plan par plan (nombre d'échantillons, résolution des ombres…), **en aperçu**
   et **modifiables** — jamais imposés.
3. **Mesurer** : pendant le rendu, l'estimation se **corrige** image après image
   (temps restant). Après le rendu, NKScena affiche **l'écart entre l'estimé et le
   réel**, et s'en sert pour estimer mieux la fois suivante.

🎯 **Objectif mesurable** : écart estimation / réel **< 10 %** sur un film
entier, publié après chaque rendu.

### 11.3 La file de rendu

- Des **travaux** (un plan, une séquence, le film, une version de montage),
  chacun avec ses réglages, sa sortie, son estimation.
- Pause, reprise **à l'image près** (un rendu interrompu ne recommence pas),
  priorité, rendu **la nuit** (heure de début).
- Plus tard (P6) : **plusieurs machines** (ferme de rendu).

### 11.4 Sorties

| sortie | formats | existant |
|---|---|---|
| suites d'images | PNG, EXR (passes), JPEG, WebP | `NkImageSequenceWriter`, `NkRenderOutput::Format` (Noge) |
| vidéo | MP4 (H.264), MOV, WebM, AVI | `NkMp4H264Writer`, `NkMovWriter`, `NkWebmWriter`, `NkAviWriter`, `NkH264Encoder` |
| audio | WAV (et le son dans la vidéo) | `NkWavWriter`, `NkAudioExport` |
| sous-titres | SRT | à écrire |
| cinématique de jeu | séquence Noge | sérialisation Noge |

---

## 12. L'IA dans NKScena

**Mêmes règles que NKCraft et NkAnima** : l'IA produit des **commandes**
existantes, en **aperçu**, **retouchables**, et ce qu'elle a fait **reste
marqué**.

| capacité | entrée | sortie |
|---|---|---|
| découpage | le texte | séquences et plans proposés |
| storyboard | un plan découpé | une planche (image, ou vue 3D posée) |
| cadrage | « plan américain sur Kofi, contre-plongée » | une caméra placée, focale et hauteur |
| lumière | « aube froide, contre-jour » | des lumières réglées |
| premier montage | le texte + les prises | une timeline proposée |
| étalonnage | une image de référence | un étalonnage proposé |
| voix provisoire | les dialogues | une piste de synthèse vocale |
| budget de rendu | une durée cible | des réglages proposés (§11.2) |
| débruitage | un rendu précalculé bruité | un rendu propre (débruiteur) |

Modèles : **NKAI**, en local ; même règle de **licence qui voyage avec les poids**
que dans NkAnima (doc 07 §6.2).

---

## 13. Phasage

**Pour un début, temps réel.** Chaque palier finit sur un critère mesurable.

| palier | contenu | critère de fin |
|---|---|---|
| **P0 — condition de naissance** (ROADMAP §3) | une séquence se **sauve et se relit** (`NkSequence::SaveToFile` / `LoadFromFile`) | une séquence de 3 pistes sauvée, fermée, rouverte, **identique** (comparaison octet par octet de la seconde sauvegarde) |
| **P1 — le plan en temps réel** | projet Film → Séquence → Plan ; ouvrir une scène Noge ; placer décor et acteurs de NkAnima ; caméra physique (focale, mise au point, profondeur de champ en temps réel) ; séquenceur de plan ; rendu temps réel en suite d'images **et** MP4 | un plan de 10 s rendu en MP4, dont **chaque image** est identique à l'aperçu à la même image (témoin par couleur nommée, comme la maison le fait) |
| **P2 — le montage vivant et le son** | timeline de montage, coupes, trim, transitions simples, versions ; **périmé** quand un plan change ; pistes audio, mixage de base, VU-mètres ; export du film avec son | modifier la caméra d'un plan monté le marque périmé ; le re-rendre met le montage à jour **sans toucher aux autres plans** |
| **P3 — le look** | ciel, atmosphère, dispersion, lumières et préréglages ; étalonnage (roues, LUT, scopes) ; effets image ; titres, sous-titres ; **préréglages Temps réel final** | une séquence de 3 plans étalonnée, avec titre et sous-titres, exportée en MP4 1080p |
| **P4 — le précalculé** | rendu précalculé (accumulation : AA, profondeur de champ, flou de mouvement vrais) ; passes EXR ; compositing par nœuds ; file de rendu ; **estimation, budget, mesure** | écart estimation / réel **< 10 %** sur une séquence ; reprise d'un rendu interrompu à l'image près |
| **P5 — la préproduction et l'IA** | texte, découpage, storyboard (2D et 3D), animatique ; IA : découpage, cadrage, lumière, premier montage, étalonnage par référence | un texte de 2 pages devient une animatique de 90 s, puis des plans 3D, sans ressaisie |
| **P6 — la haute qualité et l'équipe** | lancer de rayons + débruiteur (phase R de NKRenderer) ; caméra virtuelle (NKXR) ; ferme de rendu ; revue en équipe | un plan en lancer de rayons débruité, dans le budget annoncé |

---

## 14. Règles transverses

1. **Toute action est une commande** : annulable, rejouable, scriptable ;
   l'interface, le script et l'IA passent par la même porte.
2. **Le montage pointe vers des scènes, jamais vers des vidéos figées.**
3. **Un rendu sait s'il est périmé.**
4. **Aucun travail n'est à refaire pour passer du temps réel au précalculé.**
5. **La durée de rendu est estimée, puis mesurée, et l'écart est publié.**
6. **Fermer un onglet ne supprime jamais rien.**
7. **Zéro-STL, mémoire par NKMemory**, conventions du dépôt.
8. **L'interface vient des documents `.nkgui`**, dans le **style Unreal Engine 5,
   net**, avec le **même thème, les mêmes jetons et les mêmes composants** que
   NkAnima (document 02 §1).

---

## 15. Décisions

### 15.1 Tranchées par ce document (questions ouvertes du ROADMAP §5)

| # | question | réponse | raison |
|---|---|---|---|
| R1 | NKScena : application distincte ou espace de Nogee ? | **application distincte** | décision des trois applications (NKCraft, NkAnima, NKScena) ; le périmètre n'est plus un séquenceur mais un studio |
| R2 | timeline partagée avec NkAnima ? | **oui**, une brique de l'Editor Kit | deux timelines divergeraient |
| R3 | le son ? | **dans NKScena**, dès P2 | un film sans son ne se monte pas longtemps |

⚠️ **R1 et R2 sont des propositions** tant que Rodolf ne les a pas validées : le
ROADMAP les réserve à Rodolf.

### 15.2 Encore ouvertes

| # | question | proposition |
|---|---|---|
| D1 | extensions de fichiers | `.nkfilm` (projet), `.nkplan` (plan), `.nkmontage` (version de montage) — la scène reste une scène Noge |
| D2 | un mode **Facile** comme dans NkAnima ? | oui, **après P5** : « écris ton histoire, choisis tes acteurs, NKScena propose le film » — il s'appuie sur l'IA de P5 |
| D3 | le storyboard dessiné : dans NKScena, ou une petite application à part ? | dans NKScena : l'animatique **est** un montage |
| D4 | cinématiques de jeu : NKScena les exporte-t-elle vers Noge ? | oui, en séquence Noge (la même que NKScena lit) |
