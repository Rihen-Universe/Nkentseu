# PV3DE — l'interface, pour un humain

### Document 2 — Spécification d'interface côté humain

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Ce document dit **quoi**, **où** et **pourquoi**. Le **03** le traduit pour
> l'agent. Le **01** dit ce que PV3DE sait faire. **Si 02 et 03 se contredisent,
> 02 gagne.**

---

## 1. Deux visages, un thème

PV3DE a **deux publics qui ne regardent pas l'écran de la même façon** :

| | **l'apprenant** | **le formateur** |
|---|---|---|
| ce qu'il fait | **soigne** un patient | **écrit** des cas, **suit** des apprenants |
| ce qu'il regarde | **le patient**, d'abord et surtout | des formulaires, des tableaux, des graphes d'évolution |
| l'interface | **la salle** : le patient occupe l'écran, les outils se font discrets et **contextuels** | **l'atelier** : panneaux amarrables, style UE5, comme les autres éditeurs de la famille |

Le **thème, les jetons et les composants** sont ceux de la famille (palette de
NKCraft 02 §1.1 : gris `#141414` / `#212121` / `#2B2B2B`, bleu d'état `#1177D1`,
ambre `#F2980E`). Mais **la salle n'est pas un éditeur** : pas de docking visible,
pas de barres d'outils denses. **La règle de la salle** :

> **Le patient ne doit jamais être caché par l'interface.** Un panneau qui couvre
> le visage du patient pendant qu'il parle fait manquer ce qu'il exprime.

### 1.1 Les couleurs d'information

**Communes à la famille** : violet hachuré (proposition de l'IA — ici, **dans
l'éditeur de cas seulement**), ambre `@info.alerte`, rouge `@info.erreur`.

**Propres à PV3DE** :

| jeton | sens | couleur | où | pourquoi |
|---|---|---|---|---|
| `@moniteur.fc` | fréquence cardiaque | vert | moniteur | **convention des moniteurs** hospitaliers : l'apprenant apprend à les lire |
| `@moniteur.spo2` | saturation | cyan | moniteur | idem |
| `@moniteur.pa` | pression artérielle | rouge | moniteur | idem |
| `@moniteur.fr` | fréquence respiratoire | jaune | moniteur | idem |
| `@moniteur.temp` | température | blanc | moniteur | idem |
| `@alarme.haute` / `@alarme.moyenne` | alarme du moniteur | rouge clignotant / jaune | moniteur | un vrai moniteur sonne et clignote ; l'apprenant doit **réagir** |
| `@grille.fait` · `@grille.partiel` · `@grille.manque` · `@grille.critique` | item de la grille | vert · ambre · gris · **rouge** | débriefing **seulement** | lire son évaluation d'un coup d'œil |
| `@etape.faite` · `@etape.courante` · `@etape.a_venir` | étape de la séance | vert · `@accent` · gris | barre des étapes | se repérer, sans être contraint |
| `@cas.valide` · `@cas.relecture` · `@cas.brouillon` | statut d'un cas | vert · ambre · gris | bibliothèque de cas, éditeur | un cas non validé ne sort pas en Examen |

⚠️ **Pendant une séance, aucune couleur ne doit donner la réponse.** Pas de zone du
corps qui rougit « parce qu'elle est pathologique », pas d'item qui verdit quand
on pose la bonne question. **Les couleurs de la grille n'existent qu'au
débriefing.** En mode **Entraînement**, les **indices** sont des **phrases**
demandées explicitement, jamais des couleurs.

📌 **Jamais la couleur seule** : les constantes ont leur **nom et leur unité** ;
les items de la grille leur **symbole** (✓ ◐ — ✕).

---

## 2. L'accueil

- **Qui êtes-vous ?** Deux grandes cartes : **Apprenant** · **Formateur**. (Pas de
  comptes en ligne : un **profil local** par personne, choisi dans une liste,
  avec un code facultatif — les salles n'ont pas de réseau.)
- **Langue** : Français · English — en haut à droite, toujours visible.
- **Mention**, en pied de page, sur tous les écrans d'accueil : *« Simulation à but
  pédagogique — ne constitue pas un avis médical. »*

### 2.1 L'accueil de l'apprenant

- **Continuer** la dernière séance (si interrompue).
- **Cas proposés** par le formateur (une liste à faire).
- **Bibliothèque de cas** : cartes (motif de consultation — **jamais le
  diagnostic** —, âge et sexe du patient, niveau, durée, spécialité), filtres.
- **Mes progrès** : séances passées, tendance par compétence (interrogatoire,
  examen, raisonnement, prise en charge, communication).
- Pour démarrer : choisir un cas → choisir le **mode** (Entraînement · Examen) →
  **Entrer dans la salle**.

### 2.2 L'accueil du formateur

- **Mes cas** (brouillons, en relecture, validés) · **Nouveau cas** · **Relectures
  en attente** (pour les relecteurs) · **Promotions** · **Séance de
  démonstration**.

---

## 3. La salle (l'apprenant)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ◀ Quitter │ ① Accueil ─ ② Interrogatoire ─ ③ Examen ─ ④ Examens ─ ⑤ Hypothèses│
│           │   ─ ⑥ Prise en charge ─ ⑦ Bloc ─ ⑧ Synthèse      ⏱ 00:12 (sim 01:40)│
├──────────────────────────────────────────────────────────┬───────────────────┤
│                                                          │ ♥ 112  SpO₂ 91 %  │
│                                                          │ PA 96/58  FR 26   │
│                  LE PATIENT (vue 3D)                     │ T° 38,9           │
│                                                          │ ─────────────────│
│        « J'ai mal… là… ça serre… »  (sous-titre)         │  (moniteur)       │
│                                                          │                   │
├──────────────────────────────────────────────────────────┴───────────────────┤
│ [🎤 Maintenir pour parler]  [⌨ Écrire une question…            ]  [📋 Dossier] │
│ Actions de l'étape : [Se présenter] [Expliquer] [Rassurer] …                   │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 La barre des étapes

Les étapes de la séance (01 §3), cliquables **dans n'importe quel ordre** ; celle
en cours en `@accent`, les étapes **touchées** en vert discret (touchées, **pas**
réussies — la réussite ne se dit qu'au débriefing). À droite : **temps réel** et
**temps simulé** (s'ils diffèrent), et en Entraînement **⏸ Pause** et **⏩ Accélérer**.
En **Examen** : un **compte à rebours**, et ni pause ni accélération.

### 3.2 Le patient

- **Plein centre, grand.** Caméra **à hauteur du soignant** : debout au pied du
  lit, assis face au patient, penché pour examiner — elle suit l'étape et le
  geste ; l'apprenant peut la bouger (clic droit maintenu) et la **recentrer** (F).
- **Sous-titres** de ce que dit le patient (activés par défaut ; aident aussi en
  langue seconde).
- Le patient **regarde** l'apprenant quand il lui parle (NkGazeController).

### 3.3 Parler au patient

- **Maintenir pour parler** (bouton ou **barre d'espace** maintenue) : la parole
  est transcrite, affichée, puis le patient répond.
- **Écrire** : le champ de saisie, **Entrée** pour envoyer.
- **Actions de communication** en boutons (varient selon l'étape) : Se présenter ·
  Expliquer ce que je vais faire · Demander le consentement · Rassurer ·
  Reformuler · Annoncer · Répondre à une question du patient.
- **La transcription** de toute la conversation est dans le **Dossier** (§3.6).

### 3.4 Examiner (étape ③)

Quand l'étape Examen est active, la barre du bas devient **la trousse** :

```
│ Trousse : [🩺 Stéthoscope] [✋ Palper] [👆 Percuter] [🔦 Lampe] [🔨 Marteau]  │
│           [🌡 Thermomètre] [💉 Glycémie] [⌚ Tensiomètre] [👂 Otoscope] …     │
│ Installer : [Allongé] [Assis] [Debout] [Sur le côté]   Découvrir : [Thorax ▾]  │
```

1. Choisir un **instrument** (ou un geste).
2. **Survoler le corps** : la région visée s'**entoure** (contour neutre, **pas**
   une couleur de pathologie) avec son nom (« Fosse iliaque droite », « Foyer
   mitral »).
3. **Cliquer** : le geste se fait (la main ou l'instrument se pose — NkAnima), le
   patient **réagit**, et le **résultat** s'affiche dans une bulle et s'écrit dans
   le **Dossier** (« Palpation FID : défense, douleur vive »).
4. **Auscultation** : on **entend** le son du foyer (casque recommandé) tant que le
   stéthoscope est posé ; « respirez fort » est une action.
5. **Découvrir** une région le demande d'abord **au patient** si on ne l'a pas
   fait : une **carte de consentement** apparaît (« Vous n'avez pas expliqué ni
   demandé — le patient hésite »), avec **Demander** · **Passer outre** (noté).

### 3.5 Le moniteur

À droite, repliable **mais pas cachable** dès qu'il est branché : FC, SpO₂, PA, FR,
T°, avec **courbes** (ECG, pléthysmographie) et **alarmes** (son et clignotement)
aux couleurs des vrais moniteurs. Avant qu'on branche le patient, le moniteur
n'affiche **rien** (on n'a pas les constantes sans les prendre).

### 3.6 Le Dossier (panneau latéral, à la demande)

S'ouvre **par-dessus le moniteur** (jamais par-dessus le patient). Onglets :

| onglet | contenu |
|---|---|
| **Conversation** | la transcription horodatée |
| **Examen** | les résultats des gestes, par région |
| **Examens** | catalogue, **prescrire**, résultats reçus (avec l'heure), en attente (avec un sablier) ; images (radio, ECG) en grand au clic |
| **Hypothèses** | ma liste, que je **classe** (glisser), avec ma certitude ; **Diagnostic retenu** |
| **Prescriptions** | médicament (recherche), dose, voie, fréquence ; gestes (voie veineuse, oxygène, sonde…) ; **orientation** (hospitaliser, bloc, sortie) ; tout est horodaté |
| **Notes** | un bloc-notes libre |
| **Synthèse** | l'**observation médicale**, en sections imposées par le cas (identité, motif, histoire, antécédents, examen, synthèse, hypothèses, conduite à tenir) |

En **Entraînement** : un bouton **Indice** (« Qu'est-ce que j'ai oublié ? »),
qui répond **par une phrase** et est **compté** au débriefing.

### 3.7 Le bloc opératoire (étape ⑦)

Quand le cas l'exige et que l'apprenant a décidé d'opérer, la salle **devient** le
bloc :

- **Check-list de sécurité** (OMS) à cocher, **avant** l'incision : oublier un
  point est une erreur (critique pour certains).
- **Anesthésie** : choix et surveillance — le **moniteur** prend la place d'honneur.
- **Plateau d'instruments** en bas (bistouri, pinces, écarteurs, ciseaux, aiguille
  et fil, aspiration, compresses…).
- **Les étapes de l'intervention** sur un fil vertical à gauche (voie d'abord,
  exploration, geste, fermeture) : l'étape en cours, les faites, **sans montrer
  les suivantes** en mode Examen.
- **La vue** montre le champ opératoire, l'**anatomie par couches** au fil de
  l'intervention.
- **Sutures** (niveau 2) : on **trace** les points à la souris ou au stylet ; un
  indicateur dit l'espacement et la régularité **après** le geste.

### 3.8 La fin de séance

**Terminer la séance** (le bouton dit ce qu'on n'a pas fait : « Synthèse non
rédigée — terminer quand même ? ») → **le débriefing**.

---

## 4. Le débriefing

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ Douleur thoracique — 54 ans — Entraînement — 32 min      [Rejouer] [Exporter]│
├─────────────────────────────┬────────────────────────────────────────────────┤
│  SCORE  68 / 100            │  ERREURS CRITIQUES                             │
│  Interrogatoire   ●●●●○ 80% │  ✕ Aspirine non donnée avant 30 min             │
│  Examen           ●●●○○ 60% │                                                │
│  Raisonnement     ●●●●○ 75% │  CE QUE VOUS AVEZ BIEN FAIT                    │
│  Prise en charge  ●●○○○ 45% │  ✓ Allergies demandées d'emblée …              │
│  Communication    ●●●●● 95% │  CE QUE VOUS AVEZ MANQUÉ                       │
│                             │  — Irradiation de la douleur non recherchée …   │
├─────────────────────────────┴────────────────────────────────────────────────┤
│ Frise : ●──●───●──●─────●───●──●────●  (chaque acte, cliquable → relecture)  │
├──────────────────────────────────────────────────────────────────────────────┤
│ Vos hypothèses  ↔  Le diagnostic différentiel du cas  │  Votre parcours ↔ Expert│
└──────────────────────────────────────────────────────────────────────────────┘
```

- **Score par compétence**, puis la **grille détaillée** (dépliable).
- **Erreurs critiques** d'abord, en rouge.
- **Frise** : cliquer un point **rejoue** ce moment (patient, paroles, constantes).
- **Hypothèses ↔ corrigé** : ses hypothèses (dans l'ordre et au moment où il les a
  écrites) à côté du **diagnostic différentiel du cas** — c'est **ici**, et
  seulement ici, qu'il apparaît.
- **Parcours ↔ expert** : deux colonnes alignées par étape.
- **Ce que le patient a ressenti**.
- **Observation** : la sienne à côté du modèle.
- **Exporter** : PDF (et la trace pédagogique FHIR, 01 §1).

---

## 5. L'atelier du formateur

Style **éditeur de la famille** (UE5) : panneaux amarrables, Détails à catégories.

### 5.1 L'éditeur de cas

Un **cas** s'édite dans un espace à onglets (un par partie du cas, 01 §8) :

| onglet | ce qu'on y fait |
|---|---|
| **Identité et apparence** | nom fictif, âge, sexe, langue, métier ; **choisir ou générer le personnage** (générateur de NKCraft), aperçu 3D |
| **Personnalité** | les cinq grands traits (curseurs), tolérance à la douleur, expressivité, capacités (pleurer, refuser, minimiser…), description libre du rôle ; **Essayer** : parler au patient ainsi réglé |
| **Histoire** | ce que le patient sait et dit, **par thème** (motif, histoire, antécédents, traitements, allergies, mode de vie, contexte) ; pour chaque fait : **comment il le dit**, et s'il le dit **spontanément** ou **seulement si on le lui demande** |
| **Examen** | une **carte du corps** (régions cliquables sur le patient 3D) ; pour chaque région × geste : le **résultat** (texte, image, **son**, réaction du patient) ; « normal » par défaut |
| **Examens complémentaires** | catalogue ; pour chaque examen : résultat, délai, image |
| **Évolution** | un **graphe d'états** (éditeur de nœuds, substrat NKGraph) : états, transitions (temps, actes), constantes par état ; un **simulateur** pour essayer |
| **Bloc** | le graphe d'étapes de l'intervention, instruments attendus, erreurs possibles |
| **Corrigé** | diagnostic, différentiel (le moteur actuel **propose** depuis la base de pathologies — `NkDiagnosticEngine` — et le formateur **ajuste**), parcours d'expert, points d'enseignement |
| **Grille** | items (avec leurs **formulations reconnues** FR/EN pour les questions), poids, **erreurs critiques** |
| **Validation** | statut, relecteur, commentaires de relecture, **sources** |

- **Essayer le cas** (bouton permanent) : jouer le cas soi-même, en apprenant.
- **Assistant IA** (le panneau commun de la famille, modèle local) : « propose une
  histoire cohérente pour un paludisme grave chez un enfant de 6 ans » → **carte de
  proposition** (violet hachuré) ; **rien n'est validé par l'IA** : tout ce qu'elle
  écrit passe en **« à relire »**.

### 5.2 Promotions et suivi

Tableau des apprenants × cas (score, date, erreurs critiques) ; filtre par
compétence ; ouvrir le débriefing d'un apprenant ; **exporter** les résultats
(CSV, PDF).

### 5.3 Séance de démonstration

Le formateur joue le cas **en plein écran projeté** ; un bouton **Montrer le
corrigé** (pour la classe), **Pause** pour commenter, **Dessiner** sur l'image.

---

## 6. Réglages

Langue (FR / EN) · micro (choix, test, **sensibilité**) · casque (test des sons
d'auscultation) · **sous-titres** · **qualité graphique** (trois niveaux) ·
**modèle IA** (règles · petit · grand, avec ce que la machine permet — 01 §9.1) ·
profils locaux · **mention légale**.

---

## 7. Gestes et raccourcis

| touche | effet |
|---|---|
| Espace (maintenu) | parler au patient |
| Entrée | envoyer la question écrite |
| 1 … 8 | aller à l'étape |
| D | ouvrir / fermer le Dossier |
| F | recentrer la caméra sur le patient |
| clic droit maintenu | tourner autour du patient |
| molette | s'approcher / s'éloigner |
| Échap | reposer l'instrument |
| P | pause (Entraînement) |
| H | indice (Entraînement ; compté) |

---

## 8. Toujours vrai

- **Le patient n'est jamais caché** par l'interface.
- **Rien à l'écran ne donne la réponse** pendant la séance ; la grille et le
  corrigé n'existent qu'au débriefing.
- **Tout acte est horodaté** et rejouable.
- **Le consentement se demande** ; son oubli se note.
- **La mention « simulation pédagogique »** est sur l'accueil et sur chaque rapport.
- **Tout fonctionne hors ligne.**
- **Une action indisponible est grisée avec sa raison.**
