# NKScena — l'interface, pour un humain

### Document 2 — Spécification d'interface côté humain (produit / UX)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Ce document dit **quoi**, **où** et **pourquoi**. Le **03** le traduit pour
> l'agent qui écrit les `.nkgui`. Le **01** dit ce que l'application sait faire.
> **Si 02 et 03 se contredisent, 02 gagne.**

---

## 1. Le style — le même que NkAnima

NKScena emploie **exactement** le style, le thème et les jetons de NkAnima
(document 08 de NkAnima, §1) : **Unreal Engine 5, net** — gris neutres très
sombres, densité, angles à 2 px, onglets à liseré, panneau Détails à catégories
repliables, champs X/Y/Z rouge/vert/bleu, tiroir de Bibliothèque qui monte du
bas, thème Sombre par défaut et Clair disponible.

📌 **Un utilisateur qui passe de NkAnima à NKScena ne doit rien réapprendre** :
même coquille, mêmes panneaux Scène / Détails / Bibliothèque / Journal, même
Vue 3D, mêmes raccourcis de navigation, **même séquenceur** (décision R2 du
document 01).

### 1.1 Les couleurs d'information propres à NKScena

En plus de celles de NkAnima (orange « écrit une clé », rouge « clé auto »,
axes, violet hachuré « proposition non appliquée », ambre « à regarder », rouge
vif « erreur »), et **seulement** celles-ci :

| jeton | sens | couleur | pourquoi |
|---|---|---|---|
| `@info.antenne` | **caméra active** du plan, et plan en cours de lecture dans le montage | rouge « antenne » | convention des régies : ce qui passe à l'image |
| `@etat.a_faire` | plan à faire | gris | l'état d'avancement d'un plan se lit d'un coup d'œil dans toutes les listes |
| `@etat.en_cours` | plan en cours | bleu | idem |
| `@etat.a_revoir` | plan à revoir | ambre | idem |
| `@etat.valide` | plan validé | vert | idem |
| `@info.perime` | **rendu périmé** : la scène a changé depuis le dernier rendu | ambre + hachures | un plan monté qui ne montre plus la scène doit se voir |
| `@rendu.temps_reel` | pastille **TR** | cyan | on sait en permanence comment un plan sera rendu |
| `@rendu.precalcule` | pastille **PC** | violet foncé | idem |
| `@info.budget_ok` / `@info.budget_depasse` | estimation dans / hors budget | vert / rouge | une durée qui dépasse doit se voir avant de lancer |
| `@son.ecretage` | le son sature | rouge | on ne l'entend pas toujours ; il faut le voir |
| marqueurs | couleur **choisie par l'utilisateur** | 8 teintes | c'est son information à lui |

📌 **Jamais la couleur seule** : les états portent aussi une **lettre** ou une
**icône** (○ à faire, ◐ en cours, ! à revoir, ✓ validé ; TR / PC).

---

## 2. L'écran d'accueil

Même forme que NkAnima : Récents · Nouveau · Ouvrir · Apprendre. **Nouveau
film** demande : nom, dossier, **images par seconde** (24 par défaut), **format
d'image** (1,85 · 2,39 · 16/9), **résolution** (1080p, 2K, 4K), **mode de rendu
par défaut** (Temps réel, cochée par défaut — « pour un début, temps réel »).

---

## 3. La coquille

```
┌────────────────────────────────────────────────────────────────────────────┐
│ Fichier Édition Film Plan Caméra Lumière Montage Son Image Rendu IA  …      │
├────────────────────────────────────────────────────────────────────────────┤
│ [Espace : Plan ▾] │ Séq. 3 ▸ 3-040 ▸ prise B ▾ │ outils │ ▶ │ TR ● │ ⏱ 2h10 │
├───────────┬───────────────────────────────────────────────┬────────────────┤
│  Scène    │          Vue 3D  /  Vue caméra                │   Détails      │
│           │   ┌ barre de vue ┐        [cadre 2,39]        │                │
├───────────┴───────────────────────────────────────────────┴────────────────┤
│ Séquenceur du plan (ou Montage, selon l'espace)                            │
├────────────────────────────────────────────────────────────────────────────┤
│ Bibliothèque · Journal · > commande        24 ips · 3-040 · rendu : au repos│
└────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 La barre d'outils

1. **Sélecteur d'espace** (liste déroulante, comme NkAnima).
2. **Fil d'Ariane du plan** : Séquence ▸ Plan ▸ Prise, chacun cliquable (liste des
   frères). **C'est le repère principal** : on sait toujours dans quel plan on
   travaille.
3. **Outils de la vue** : Sélectionner · Déplacer · Tourner · Échelle ·
   **Marque au sol** · **Mise au point** (clic dans la vue) · **Pinceau de
   dispersion** (espace Décor).
4. **Transport** : les mêmes cinq boutons que NkAnima + boucle + champ de
   **timecode** (HH:MM:SS:II).
5. **Mode de rendu du plan** : pastille **TR** / **PC** cliquable.
6. **Estimation de rendu** du plan ou du film : « ⏱ 2 h 10 » (vert si dans le
   budget, rouge sinon) ; clic → espace Rendu.
7. À droite : Assistant IA, propositions en attente, **Vue caméra / Vue libre**
   (bascule, pavé 0).

### 3.2 La Vue 3D, et la Vue caméra

- **Vue libre** : comme NkAnima.
- **Vue caméra** : ce que voit la caméra active, avec :
  - le **cadre** du format (bandes hors cadre assombries) ;
  - **guides** : tiers, centre, zone de sécurité titres, horizon ;
  - la **planche de storyboard** du plan en surimpression (opacité réglable) ;
  - la **mise au point** : plan net dessiné, cible de suivi ;
  - un liseré rouge **antenne** quand c'est la caméra active.
- **Aperçu de rendu** : la Vue caméra peut afficher le rendu **temps réel final**
  (tous effets et étalonnage) ou, en P4, un **rendu précalculé progressif** de
  l'image courante.

### 3.3 Scène, Détails, Bibliothèque, Journal

**Identiques à NkAnima** (même composants). Types propres à NKScena dans la
Scène : décor, terrain, dispersion, ciel, lumière, caméra, rig, acteur, foule,
cache VFX, son spatialisé, marque au sol.

---

## 4. Les espaces de travail

| espace | pour | disposition par défaut |
|---|---|---|
| **Préparation** | texte, découpage, storyboard, animatique | Texte · Découpage · Storyboard · Montage (animatique) |
| **Décor** | assembler, terrain, dispersion, ciel, atmosphère | Vue · Scène · Détails · Bibliothèque (ouverte) |
| **Lumière** | sources, préréglages, liens de lumière | Vue caméra · Scène (filtré : lumières) · Détails · Liens de lumière |
| **Plan** | acteurs, caméras, séquenceur du plan | Vue caméra · Vue libre · Scène · Détails · Séquenceur du plan · Liste des plans |
| **Montage** | le film | Moniteur source · Moniteur programme · Chutier (prises) · Timeline de montage |
| **Son** | mixage | Timeline de montage (audio) · Mixeur · Détails |
| **Étalonnage** | la couleur | Visionneuse · Bande des plans · Roues et courbes · Scopes · Galerie de looks |
| **Compositing** | passes et nœuds | Visionneuse · Graphe de nœuds · Détails du nœud · Passes |
| **Rendu** | sortir le film | File de rendu · Réglages · Estimation · Historique |

---

## 5. Les espaces, un par un

### 5.1 Préparation

- **Texte** : éditeur de scénario. Chaque ligne a un **type** (Intitulé, Action,
  Personnage, Dialogue, Didascalie, Transition), choisi par Tab/Entrée comme dans
  les logiciels de scénario. Les personnages et lieux reconnus sont soulignés ;
  clic → leur fiche.
- **Découpage** : un tableau des plans — numéro, séquence, **état**, valeur de
  cadre, angle, mouvement, durée, personnages, lieu, note. Bouton **Découper par
  IA** (propositions en aperçu). Glisser pour réordonner.
- **Storyboard** : une grille de **planches** (vignette, numéro, durée, dialogue
  sous la vignette). Double clic → la planche en grand : dessiner (pinceau,
  gomme, calques), importer une image, ou **Capturer la vue 3D**. Flèches de
  mouvement (caméra, personnage).
- **Animatique** : la timeline de montage, avec les planches comme clips.
  **Générer l'animatique** pose les planches à leur durée avec les voix
  provisoires.
- **Planche → Plan 3D** : crée le plan, sa caméra, et garde la planche en
  surimpression.

### 5.2 Décor

- **Bibliothèque ouverte** à gauche ou en bas : glisser un élément dans la Vue.
  Aimantation au sol et aux surfaces (Maj pour l'enlever).
- **Terrain** : pinceaux relief (monter, creuser, lisser, aplanir) et matière.
- **Pinceau de dispersion** : choisir une **palette d'éléments** (herbe, arbustes,
  cocotiers…), peindre ; densité, échelle min/max, orientation aléatoire,
  **gomme**, et zones d'exclusion.
- **Ciel et atmosphère** (sélection du ciel dans la Scène) : lieu (latitude),
  date, **heure** (le soleil suit), nuages, brouillard, brume, volumétrie.

### 5.3 Lumière

- **Préréglages de chef opérateur** en vignettes : un clic place et règle les
  lumières **autour de la sélection** (en aperçu, violet, jusqu'à **Appliquer**).
- **Détails d'une lumière** : type, **température (K)**, intensité (unités
  physiques ou artistique), taille, ombres, volumétrie, **liens** (qui elle
  éclaire).
- **Liens de lumière** : un tableau lumières × éléments, cases à cocher.
- **Solo de lumière** : ne voir que l'effet d'une lumière.

### 5.4 Plan

- **Liste des plans** de la séquence (bande de vignettes, avec état et pastille
  TR/PC). Clic → on travaille dans ce plan.
- **Caméra** (sélection d'une caméra) — Détails :
  - **Objectif** : focale (avec préréglages 18 · 24 · 35 · 50 · 85 · 135 mm),
    capteur, format ;
  - **Mise au point** : distance, **cible suivie**, ouverture (f/1,4 → f/22),
    aperçu du plan net ;
  - **Obturateur** : angle (180° par défaut) → flou de mouvement ;
  - **Rig** : aucun, grue, rail, steadicam, épaule, drone, orbite ; réglages du
    rig (tremblement, amortissement) ;
  - **Visée** : cible, décalage ;
  - toutes ces propriétés ont un **losange de clé** (bascule de mise au point
    animée, zoom).
- **Acteurs** : glisser un personnage, poser ses **marques au sol** (outil
  Marque), assigner ses clips sur sa piste. **Ouvrir dans NkAnima** (bouton et
  clic droit).
- **Séquenceur du plan** : le séquenceur de NkAnima, avec des pistes propres :
  Caméras (coupes entre caméras du plan), Lumières, Événements, Son, Notes.

### 5.5 Montage

```
┌──────────────────────┬──────────────────────┬───────────────────┐
│  Moniteur SOURCE     │  Moniteur PROGRAMME  │  Chutier          │
│  (la prise choisie)  │  (le film à la tête) │  séq. › plans ›   │
│  [entrée] [sortie]   │                      │  prises (vignettes│
│  ▶ 00:00:04:12       │  ▶ 00:12:31:08       │   état, TR/PC)    │
├──────────────────────┴──────────────────────┴───────────────────┤
│ Montage 2 ▾ │ outils : sélection · rasoir · trim · glisser │ ⧉ │
│ V2 ▕▔▔▔▔▔▔▕                                                      │
│ V1 ▕ 3-030 ▕ 3-040B ▕▒▒3-050▒▒▕ 3-060 ▕   (▒ = périmé)          │
│ A1 ▕~~dialogues~~~~~~~~~~~~~~~▕                                  │
│ A2 ▕~~ambiance~~~~~~~~~~~~~~~~~~~~~~~▕                           │
│ A3 ▕~~musique~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~▕                     │
└──────────────────────────────────────────────────────────────────┘
```

- **Chutier** : l'arbre Séquence › Plan › Prise, en vignettes animées au survol.
- **Moniteur source** : la prise choisie ; points d'**entrée** (I) et de
  **sortie** (O) ; **Insérer** (,) et **Écraser** (.) vers la timeline.
- **Moniteur programme** : le film à la tête de lecture.
- **Timeline** : outils Sélection (V), **Rasoir** (C), **Trim** (T), Glisser (Y) ;
  transitions par glisser depuis la Bibliothèque ; **vitesse** par clic droit ;
  **marqueurs** (M) ; **Versions** de montage en liste déroulante ; **Comparer**
  deux versions (⧉).
- Un clip **périmé** est hachuré ambre ; clic droit → **Rendre à nouveau**.
- Rallonger un clip au-delà de sa prise : **« Rallonger le plan 3D ? »**
  (confirmation, puis le plan s'allonge).
- Double clic sur un clip → **ouvre le plan** dans l'espace Plan à la même image.

### 5.6 Son

- **Mixeur** : une **tranche** par piste, puis les **bus** (Dialogues, Effets,
  Musique) et le **Master** : fader, panoramique, sourdine, solo, **VU-mètre**
  (crête, écrêtage rouge), inserts d'effets (égaliseur, compresseur,
  réverbération).
- **Automation** : les faders et panoramiques ont un losange de clé ; les
  courbes se voient sur les pistes audio.
- **Son spatialisé** : attacher un son à un élément de la scène (sélection) ; il
  suit la caméra.
- **Enregistrer** une voix provisoire sur la piste choisie.

### 5.7 Étalonnage

- **Visionneuse** (l'image du plan, avec avant/après en volet).
- **Bande des plans** du montage : un plan = une vignette ; sélectionner
  plusieurs plans pour les corriger ensemble.
- **Roues** Lift / Gamma / Gain (+ Décalage), avec leurs curseurs de luminance ;
  exposition, contraste, saturation, température, teinte ; **courbes** (RVB,
  teinte/saturation) ; **LUT** (liste + import `.cube`).
- **Niveaux** : correction de **séquence** puis correction de **plan** (le plan
  hérite de la séquence).
- **Scopes** : forme d'onde, vecteurscope, histogramme, au choix.
- **Galerie de looks** : enregistrer un look, l'appliquer, **Correspondance** (le
  look d'un plan appliqué à la sélection).

### 5.8 Compositing

- **Graphe de nœuds** (même éditeur que les autres graphes de la maison :
  `NKGraph`) : nœuds Passe (beauté, profondeur, normales, identifiants…), Fusion,
  Masque, Flou, Défocalisation, Lueur, Couleur, Brouillard de profondeur, Sortie.
- **Visionneuse** : la sortie du nœud sélectionné (ou de la Sortie).
- **Passes** : la liste des passes que le rendu produit ; cocher une passe
  l'ajoute au rendu.
- En **temps réel**, le graphe s'évalue en direct dans la Vue caméra ; en
  **précalculé**, sur les passes EXR.

### 5.9 Rendu

```
┌───────────────────────────────────────────┬────────────────────────────┐
│ FILE DE RENDU                             │ RÉGLAGES DU TRAVAIL        │
│ ☐ Film — Montage 2   PC  ⏱ 7h40  ████░ 62%│ Mode : ● TR  ○ PC          │
│ ☐ Séq. 3             TR  ⏱ 0h06  ✓        │ Préréglage : Revue ▾       │
│ ☐ 3-050 prise B      PC  ⏱ 1h12  en file  │ Résolution · ips · plage   │
│                                           │ Sortie : MP4 ▾  dossier    │
│ [Estimer] [Lancer] [Pause] [Reprendre]    │ Passes (EXR) ☐ ☐ ☐         │
│ Début : maintenant ▾                      │                            │
├───────────────────────────────────────────┴────────────────────────────┤
│ ESTIMATION                              BUDGET : [ 8 h 00 ] [Proposer] │
│  plan     images  s/image   total   marge      écart dernier rendu     │
│  3-030      96     4,2      6 min   ±0,5       +4 %                    │
│  3-040B    144    38,0     1h31   ±9 min       —                       │
│  …                                   TOTAL 7h40 ±34 min  ✓ dans budget  │
└─────────────────────────────────────────────────────────────────────────┘
```

- **File** : les travaux, leur mode, leur **estimation**, leur progression, leur
  état ; glisser pour changer la priorité.
- **Estimer** rend les images-échantillons et remplit le tableau.
- **Budget** : saisir une durée, **Proposer** → des réglages proposés par plan
  (cartes de proposition, en aperçu) ; **Appliquer** les inscrit.
- Pendant un rendu : **temps restant** corrigé en continu ; l'image en cours
  s'affiche.
- **Historique** : chaque rendu passé avec **estimé / réel / écart**. C'est là
  que se mesure l'objectif « écart < 10 % ».

---

## 6. Gestes et raccourcis

Tous ceux de NkAnima (navigation, sélection, transport, annuler), plus :

| touche | effet |
|---|---|
| 0 (pavé) | Vue caméra / Vue libre |
| Ctrl+0 | la caméra devient celle de la vue |
| Maj+F | mise au point sur le point cliqué |
| M | marqueur (montage) / marque au sol (plan) |
| I / O | point d'entrée / de sortie (montage) |
| , / . | insérer / écraser dans la timeline |
| V · C · T · Y | sélection · rasoir · trim · glisser (montage) |
| J · K · L | lecture arrière · pause · lecture avant (montage, répéter pour accélérer) |
| Ctrl+R | rendre le plan courant |
| Ctrl+Maj+R | ouvrir la file de rendu |
| Page préc. / suiv. | plan précédent / suivant |

📌 **Les raccourcis du montage (I, O, T, V, C, Y, J, K, L, M) ne valent que
quand la timeline de montage ou un moniteur a le focus.** Ailleurs, I pose une
clé et T prend l'outil Trajectoire, comme dans NkAnima. Le panneau qui a le focus
porte le liseré bleu d'onglet : on sait toujours quelle signification s'applique.

---

## 7. Toujours vrai

- **Le fil d'Ariane dit toujours** dans quel plan et quelle prise on travaille.
- **Tout plan montre son état et son mode de rendu**, partout où il apparaît.
- **Un rendu périmé est hachuré ambre**, partout.
- **Une estimation dépassant le budget est rouge avant de lancer.**
- **Une proposition non appliquée est violette et hachurée** (comme NkAnima).
- **Une action indisponible est grisée avec sa raison.**
- **Fermer un onglet ne supprime rien.**

---

## 8. Les menus

| menu | contenu |
|---|---|
| **Fichier** | Nouveau film · Ouvrir · Récents ▸ · Enregistrer · Enregistrer sous · Importer ▸ (scène Noge, décor, acteur NkAnima, audio, vidéo de référence, texte, LUT, sous-titres) · Exporter ▸ (film, séquence, plan, audio, sous-titres, cinématique Noge) · Quitter |
| **Édition** | Annuler · Refaire · Historique · Copier · Coller · Dupliquer · Supprimer · Renommer · Préférences |
| **Film** | Nouvelle séquence · Nouveau plan · Nouvelle prise · État du plan ▸ · Mode de rendu du plan ▸ · Réglages du film |
| **Plan** | Ajouter une caméra · Ajouter un acteur · Marque au sol · Ouvrir dans NkAnima · Rallonger / raccourcir · Plan précédent / suivant |
| **Caméra** | Vue caméra · Caméra depuis la vue · Mise au point ▸ · Rig ▸ · Format ▸ · Guides ▸ · Caméra virtuelle |
| **Lumière** | Ajouter ▸ · Préréglages ▸ · Liens de lumière · Solo · Ciel et atmosphère |
| **Montage** | Insérer · Écraser · Rasoir · Transition ▸ · Vitesse · Marqueur · Versions ▸ · Comparer · Rendre à nouveau les périmés |
| **Son** | Ajouter une piste · Enregistrer · Bus ▸ · Effets ▸ · Spatialiser · Normaliser |
| **Image** | Étalonnage ▸ (roues, courbes, LUT, correspondance) · Scopes ▸ · Effets image ▸ · Titres ▸ · Sous-titres ▸ · Compositing |
| **Rendu** | Rendre le plan · Ajouter à la file ▸ · Estimer · Budget · File de rendu · Historique |
| **IA** | Assistant · Découper le texte · Storyboard · Cadrer · Éclairer · Premier montage · Étalonner d'après une image · Voix provisoire · Budget de rendu |
| **Fenêtre** | Espaces ▸ · chaque panneau · Réinitialiser la disposition |
| **Aide** | Raccourcis · Documentation · Journal · Crédits et licences · À propos |
