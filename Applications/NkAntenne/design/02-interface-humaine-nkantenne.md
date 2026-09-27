# NkAntenne — l'interface, pour un humain

### Document 2 — Spécification d'interface côté humain

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 27/09/2026.
> Ce document dit **quoi**, **où** et **pourquoi**. Le **03** le traduit pour
> l'agent. Le **01** dit ce que l'application sait faire. **Si 02 et 03 se
> contredisent, 02 gagne.**

---

## 1. Le style : épuré, GitHub Dark Pro / GitHub Light Pro

Rodolf veut **un design épuré et joli**, en **GitHub Dark Pro** et **GitHub Light
Pro**. C'est un autre registre que les éditeurs de la famille (NKCraft, NkAnima,
Nogee : denses, style UE5) — et c'est voulu : NkAntenne s'utilise **pendant un
direct**, souvent d'un œil, parfois sous pression. **Il doit se lire d'un coup
d'œil.**

### 1.1 Les règles d'épure

| règle | pourquoi |
|---|---|
| **l'image d'abord** : l'aperçu du direct occupe l'espace ; tout le reste l'entoure | c'est ce que voient les spectateurs |
| **peu de panneaux visibles à la fois** ; le reste dans des **tiroirs** qui s'ouvrent au besoin | un direct n'est pas le moment de chercher un bouton parmi quarante |
| **grandes cibles** pour les gestes du direct (≥ 40 px) : Démarrer, Enregistrer, Marqueur, sourdines | on les touche sans regarder, parfois sur un écran tactile |
| **angles doux** (rayon 6 px), **espacements généreux** (12 à 16 px), **ombres légères** sur les éléments flottants | l'épure GitHub : des surfaces calmes, des bords nets |
| **une seule couleur vive à la fois** : le rouge du direct | tout le reste est neutre ; quand le rouge apparaît, il veut dire une chose |
| **tout icône a un mot** (sauf les commandes du lecteur : ▶ ⏸ ⏮ ⏭, universelles) | règle de la famille |

### 1.2 Le thème — les jetons de la famille, les valeurs de GitHub

Les **noms** de jetons sont ceux de la famille (`@fond.app`, `@texte`, `@accent`…) ;
les **valeurs** sont celles de **GitHub Dark Pro / Light Pro**. La version sombre
**existe déjà dans le dépôt**, appliquée à NKGui par ConquerorLab
(`NkcLabTheme.h`, `ApplyGitHubDarkPro`, avec un **rayon de 6 px** et des marges
12 × 7 — exactement l'épure voulue ici) : **on la reprend telle quelle**. Un même
document `.nkgui` s'affiche donc en UE5 dans NKCraft et en GitHub ici **sans
changer une ligne** — c'est tout l'intérêt des jetons.

| jeton | Dark Pro (repris de `NkcLabTheme.h`) | Light Pro (GitHub Primer, **à poser**) | rôle |
|---|---|---|---|
| `@fond.creux` | `#010409` | `#F6F8FA` | creux : pistes, champs encastrés, fond des onglets |
| `@fond.app` | `#0D1117` | `#FFFFFF` | le fond |
| `@fond.panneau` · `@fond.entete` | `#161B22` | `#F6F8FA` | surfaces, en-têtes |
| `@fond.bouton` | `#21262D` | `#F6F8FA` | boutons au repos |
| `@fond.survol` · `@bord` | `#30363D` | `#EAEEF2` · `#D0D7DE` | survol · filets |
| `@texte` | `#E6EDF3` | `#1F2328` | texte |
| `@texte.faible` | `#8B949E` | `#656D76` | secondaire |
| `@accent` | `#58A6FF` | `#0969DA` | liens, focus, état de l'interface |
| `@accent.plein` | `#1F6FEB` | `#0969DA` | **bouton principal** (Démarrer le direct), bouton actif |

⚠️ **Le thème clair n'existe pas encore dans le dépôt** : ses valeurs sont celles de
GitHub Primer, à **poser** dans le thème (et à vérifier au contraste, avec
l'éditeur de jetons de NKUIDesign — doc 19 C). Le **ConquerorLab** et **NkAntenne**
partageront alors le même thème GitHub : **un seul fichier**, pas deux copies.

### 1.3 Les couleurs d'information — et elles seules

| jeton | sens | Dark / Light | où |
|---|---|---|---|
| `@antenne.direct` | **EN DIRECT** | `#F85149` / `#CF222E` | bouton « Arrêter le direct », badge EN DIRECT, liseré de l'aperçu Programme |
| `@antenne.enregistrement` | **ENREGISTREMENT** | `#F85149` / `#CF222E` + **point pulsant** | bouton « Arrêter l'enregistrement », badge ● REC |
| `@antenne.pause` | enregistrement en pause | `#D29922` / `#9A6700` | badge ❚❚ |
| `@antenne.pret` | destination connectée, prête | `#3FB950` / `#1A7F37` | pastille de destination |
| `@antenne.reconnexion` | destination qui se reconnecte | `#D29922` / `#9A6700` | pastille, barre de santé |
| `@antenne.erreur` | destination en échec | `#F85149` / `#CF222E` | pastille, notification |
| `@sante.bon` · `.moyen` · `.mauvais` | santé du direct (débit, images perdues, processeur) | vert · ambre · rouge | barre de santé |
| `@son.ecretage` | le son sature | rouge | VU-mètres (commun avec NKScena) |
| `@son.sourdine` | micro coupé | `#D29922` / `#9A6700` + icône barrée | tranche du mixeur, bouton de sourdine |
| `@studio.apercu` · `@studio.programme` | aperçu (on prépare) · programme (à l'antenne) | `@accent` · `@antenne.direct` | liserés des deux vues en mode studio |
| `@plateforme.*` (YouTube, Facebook, TikTok, Twitch, Kick…) | **la plateforme** d'une destination | **leurs couleurs de marque, en pastille seulement** | cartes de destinations |

⚠️ **Direct et enregistrement partagent le rouge**, mais **jamais sans leur mot et
leur forme** : « EN DIRECT » (badge plein), « ● REC » (point qui pulse). Les deux
peuvent être actifs ensemble, et on doit voir **les deux**.

⚠️ **Les couleurs de marque des plateformes** : **seulement** en petite pastille
d'identification, **jamais** comme couleur de bouton ou de fond — l'interface reste
à nous, et les chartes des plateformes encadrent l'usage de leurs logos (à
vérifier avant publication).

---

## 2. L'accueil (premier lancement)

Un **assistant de démarrage**, trois questions, pas plus :

1. **Que voulez-vous faire ?** — Diffuser en direct · Enregistrer (tutoriel, cours,
   jeu) · Les deux. *(Règle les réglages de sortie.)*
2. **Quoi capturer ?** — Tout l'écran · Une fenêtre · Un jeu · **+ Caméra** · **+
   Micro**. *(Crée une première scène.)*
3. **Où diffuser ?** (si diffusion) — cartes YouTube · Facebook · TikTok · Twitch ·
   Autre · **Plus tard**.

Puis **un test de performance** de 10 secondes : quelle définition et quelle
cadence la machine tient, avec **quel encodeur** — et la proposition de réglages
qui en découle (modifiable).

---

## 3. La fenêtre et ses quatre espaces

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ◉ NkAntenne   [ Studio │ Montage │ Compression │ Médiathèque ]    ☀/☾  ⚙  ? │  56
├─────────────────────────────────────────────────────────────────────────────┤
│                                 (l'espace)                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **En haut** : le nom, **un sélecteur à quatre segments** (les espaces), la bascule
  **Sombre / Clair**, les Réglages, l'Aide. **Pas de barre de menus** visible par
  défaut (épure) : **Alt** la fait apparaître (Windows, Linux), et toutes ses
  commandes sont dans la **palette** (Ctrl+K).
- **Pendant un direct ou un enregistrement**, un **badge EN DIRECT / ● REC avec la
  durée** reste **dans la barre du haut, quel que soit l'espace** : on monte une
  vidéo pendant qu'on enregistre, on voit toujours qu'on enregistre.

---

## 4. L'espace Studio

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ ◉ NkAntenne  [Studio│Montage│Compression│Médiathèque]   ● EN DIRECT 00:42:17 │
├──────────────────────────────────────────────────────────────┬──────────────┤
│                                                              │  COMMANDES   │
│                                                              │ ┌──────────┐ │
│                 PROGRAMME (ce qui est à l'antenne)           │ │■ Arrêter │ │
│                 liseré rouge pendant le direct               │ │ le direct│ │
│                                                              │ └──────────┘ │
│                                                              │ [● Enregistrer]│
│                                                              │ [⚑ Marqueur] │
│                                                              │ [⧉ Studio]   │
│                                                              │ [◷ Clip 30 s]│
├───────────────┬──────────────────┬───────────────────────────┴──────────────┤
│ SCÈNES      + │ SOURCES        + │ MIXEUR                                   │
│ ▸ Accueil     │ 👁 🔒 Caméra      │ Micro   ▮▮▮▮▮▯▯▯  🔊 🎙 ⚙             │
│ ● Jeu         │ 👁 🔒 Jeu          │ Bureau  ▮▮▮▯▯▯▯▯  🔊 ⚙                │
│ ▸ Pause       │ 👁 🔒 Alerte .nkgui│ Musique ▮▮▯▯▯▯▯▯  🔊 ⚙                │
├───────────────┴──────────────────┴──────────────────────────────────────────┤
│ ↑ 6 000 kb/s ● · images perdues 0,0 % ● · processeur 23 % ● · GPU 41 % ●   │
│ YouTube ● connecté · Facebook ● connecté                    ⏱ 00:42:17     │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.1 La vue Programme (et Aperçu en mode studio)

- **Grande**, centrée, au format de sortie (16:9, ou 9:16 si la sortie est
  verticale ; **les deux côte à côte** en scène double).
- **Liseré rouge** tant qu'on est **en direct** : ce cadre est **ce que le monde voit**.
- On **manipule les sources directement** dans la vue : cliquer une source la
  sélectionne (poignées), glisser la déplace, Alt+glisser recadre, les bords
  s'aimantent aux bords et au centre.
- **Mode studio** (⧉) : la vue se dédouble — **Aperçu** à gauche (liseré bleu : on
  prépare, ce n'est pas à l'antenne), **Programme** à droite (liseré rouge), et entre
  les deux le bouton **Transition ▸** avec le choix de la transition et sa durée.

### 4.2 Les commandes (colonne de droite)

**Les gestes du direct, grands, dans l'ordre d'usage :**

| bouton | au repos | actif |
|---|---|---|
| **Démarrer le direct** | bouton principal (`@accent`) ; ▾ choisit les destinations | **■ Arrêter le direct** (fond rouge) + **confirmation** (« Arrêter le direct sur YouTube et Facebook ? ») |
| **● Enregistrer** | bouton secondaire | **■ Arrêter l'enregistrement** (rouge) + **❚❚ Pause** à côté |
| **⚑ Marqueur** | pose un marqueur (direct et enregistrement) | petite confirmation « Marqueur 3 posé » |
| **⧉ Mode studio** | bascule | bleu quand actif |
| **◷ Clip 30 s** | enregistre les 30 dernières secondes (tampon de relecture) | visible si le tampon est activé |

**Arrêter un direct demande toujours confirmation** ; **le démarrer non** (on veut
être rapide), mais un **compte à rebours de 3 secondes** annulable s'affiche sur la
vue Programme.

### 4.3 Scènes, Sources, Mixeur (bande du bas)

- **Scènes** : une liste ; clic = passer la scène à l'antenne (ou à l'aperçu en mode
  studio) ; **+** ; clic droit : renommer, dupliquer, supprimer, **raccourci**.
- **Sources** : la liste de la scène, **œil** (visible), **cadenas** ; **+** ouvre le
  **choix de source** (grille illustrée : Écran, Fenêtre, Zone, Jeu, Caméra,
  Téléphone, Micro, Audio système, Audio d'une application, Média, Image, Texte,
  Superposition `.nkgui`, Réseau, Chronomètre, Couleur) ; double clic = ses
  **propriétés** ; clic droit : **Filtres…**, Transformer ▸, Ordre ▸.
- **Mixeur** : une ligne par source audio — **VU-mètre** (vert → ambre → rouge, crête,
  écrêtage), **fader**, **🔊 écoute**, **🎙 sourdine** (grande cible), **⚙** (filtres,
  pistes d'enregistrement, décalage). Le mixeur peut passer **en vertical** (tranches
  de console) par son menu.

### 4.4 La barre de santé (tout en bas)

**Débit** envoyé, **images perdues** (réseau / encodage / rendu — survol pour le
détail), **processeur**, **GPU**, **durée**, et **une pastille par destination**
(vert connecté, ambre reconnexion, rouge échec — clic : le détail et **Réessayer**).
Chaque indicateur a **sa pastille de santé** (`@sante.*`), mais **un seul** passe au
rouge à la fois dans le résumé : le plus grave.

### 4.5 Les tiroirs

S'ouvrent **par-dessus** la bande du bas ou sur le côté, et se referment seuls :

| tiroir | contenu |
|---|---|
| **Destinations** | cartes des destinations (plateforme, nom, **clé masquée**, serveur, état), **+ Destination**, **tester la connexion** |
| **Filtres** (d'une source) | la pile de filtres de la source, avec aperçu en direct |
| **Transitions** | la liste, la durée, les stingers, les transitions `.nkgui` |
| **Chat** (P6) | le chat des plateformes liées, fusionné, avec la plateforme en pastille |
| **Statistiques** | le détail de la santé, par destination |

---

## 5. Les propriétés d'une source — exemples

- **Écran** : quel écran ; capturer le curseur ; **mode** (rapide GPU / compatibilité).
- **Fenêtre** : la fenêtre (liste avec **vignettes**), **« suivre la fenêtre si elle
  change de titre »**, capturer le curseur, **audio de cette application** (case).
- **Caméra** : périphérique, définition, cadence, **suppression du fond** (IA),
  **flou du fond**, recadrage, miroir.
- **Micro** : périphérique, **suppression de bruit**, porte de bruit, gain.
- **Superposition `.nkgui`** : le document, ses **liaisons** (spectateurs, dernier
  abonné, chat, chronomètre…) avec leur valeur de test, **« Ouvrir dans
  NKUIDesign »**.

---

## 6. L'espace Montage

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ [Studio│Montage│Compression│Médiathèque]                  Exporter ▾        │
├────────────────┬───────────────────────────────────────┬────────────────────┤
│ MÉDIAS       + │                                       │ PROPRIÉTÉS         │
│ [vignettes]    │              LECTURE                  │ (clip choisi :     │
│ Direct 26/09   │         [ 16:9 │ 9:16 │ 1:1 ]         │  vitesse, volume,  │
│ Cours 3        │                                       │  couleur, zoom…)   │
├────────────────┴───────────────────────────────────────┴────────────────────┤
│ ✂ Rasoir  ⇤ Rogner  ⌫ Supprimer et resserrer │ 🔇 Couper les silences │ ⚡ Vitesse │ Aa Sous-titres │
│ V2 ▕  titre ▏                                                                │
│ V1 ▕▓▓▓▓▓▓ direct ▓▓▓▓▓▓▓▓▓▕▓▓▓▓ direct ▓▓▓▓▓▕    ⚑ ⚑   (marqueurs du direct) │
│ A1 ▕~~~~ voix ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~▕                               │
│ A2 ▕~~~~ jeu  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~▕                               │
│ A3 ▕~~~~ musique ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~▕                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **La timeline partagée** avec NkAnima et NKScena (mêmes gestes : J K L, I / O,
  rasoir, rogner).
- **Barre d'outils du montage**, en mots : Rasoir · Rogner · **Supprimer et
  resserrer** · **Couper les silences** · **Vitesse** · **Sous-titres** · Titre ·
  Transition · **Zoom sur le curseur** (tutoriels).
- **Couper les silences** : un panneau (seuil en dB, durée minimale, marge avant /
  après) avec **aperçu** : les morceaux à retirer sont **hachurés** sur la timeline
  **avant** d'appliquer.
- **Vitesse** : un curseur (0,1× à 16×), **ou** une **courbe** de vitesse sur le
  clip (rampes) ; « conserver la hauteur du son ».
- **Sous-titres** : **Générer** (reconnaissance locale, FR / EN) → une piste de
  sous-titres éditable mot à mot ; **styles** (classique, « réseaux sociaux » mot par
  mot, karaoké).
- **Format** : 16:9 · 9:16 · 1:1 au-dessus de la lecture ; en 9:16, **Recadrage
  automatique** (suit le sujet) avec des images-clés modifiables.
- **Les pistes audio séparées** de l'enregistrement (A1 voix, A2 jeu, A3 musique)
  arrivent **telles quelles**.
- **Exporter ▾** : préréglages par plateforme, ou **« Vers la compression… »**.

---

## 7. L'espace Compression

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ FILE                                             │ RÉGLAGES                  │
│ ▸ direct_26-09.mp4   → YouTube 1080p   ██░ 64 %  │ Préréglage : WhatsApp ▾    │
│ ▸ cours_3.mp4        → Taille 25 Mo    en file   │ Taille cible : [25] Mo     │
│ ▸ podcast.wav        → Audio MP3 192   ✓ 12 Mo   │ Vidéo : H.264 · 720p · 30  │
│                                                  │ Audio : AAC 128 kb/s       │
│ [+ Ajouter des fichiers]  (ou glisser ici)       │ Garder : 00:00 → 01:30     │
│                                                  │ Découper en morceaux de…   │
│ Estimation : 24,6 Mo · 1 min 40                  │                            │
│ [▶ Lancer]  [❚❚]                                 │                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **Glisser des fichiers** dans la file ; chaque ligne : fichier → préréglage,
  progression, résultat (taille obtenue).
- **Réglages** du travail choisi : préréglage, **taille cible** *ou* qualité,
  vidéo, audio, intervalle à garder, **découpe en morceaux**, **extraire l'audio**.
- **Estimation** avant de lancer ; **écart mesuré** après (dans la ligne).

---

## 8. L'espace Médiathèque

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 🔍 Rechercher      [Tout │ Vidéos │ Audio │ Directs │ Exports]   ▦ ☰          │
├─────────────────────────────────────────────────────────────────────────────┤
│  [vignette]   [vignette]   [vignette]   [vignette]                           │
│  Direct 26/09 Cours 3      Short 45 s   Podcast 12                           │
│  1 h 02       32 min       0:45         48 min (audio)                       │
└─────────────────────────────────────────────────────────────────────────────┘
```

- **Grille** (ou liste) des enregistrements, montages, exports, audio ; filtres ;
  recherche **dans les sous-titres** quand ils existent.
- **Double clic** → le **lecteur** plein cadre :
  - **vidéo** : ▶ ⏸, barre de progression **avec les marqueurs**, vitesse, **A-B en
    boucle**, image par image (, .), **piste audio** (voix / jeu / musique), sous-titres,
    plein écran ;
  - **audio** : **forme d'onde** pleine largeur, ▶ ⏸, vitesse, boucle, **liste de
    lecture** ;
  - en bas : **Ouvrir au montage** · **Compresser** · **Montrer dans le dossier**.

---

## 9. Réglages

Catégories à gauche, contenu à droite (épuré, une question par ligne) : **Général**
(langue, thème, démarrage) · **Diffusion** (destinations, débit, **adaptation
automatique**, reconnexion) · **Sortie** (définition, cadence, **encodeur** : matériel
/ Nkentseu, qualité) · **Enregistrement** (dossier, format, pistes, tampon de
relecture) · **Audio** (périphériques, échantillonnage, écoute) · **Vidéo** (base,
sortie, filtre de mise à l'échelle) · **Raccourcis** (globaux ou non) · **Vie privée**
(fenêtres exclues, mode privé, clés) · **Avancé**.

---

## 10. Raccourcis

| touche | effet | portée |
|---|---|---|
| Ctrl+K | palette de commandes | fenêtre |
| Alt | afficher la barre de menus | fenêtre |
| Ctrl+1 … 4 | Studio · Montage · Compression · Médiathèque | fenêtre |
| **à définir par l'utilisateur** | démarrer / arrêter le direct, l'enregistrement, marqueur, clip, sourdine micro, changer de scène | **globaux** (fonctionnent dans le jeu) — **aucun par défaut** pour démarrer ou arrêter un direct : un accident coûterait trop cher |
| Espace | lecture / pause (Montage, Médiathèque) | fenêtre |
| J K L, I O, C | montage (communs à la famille) | timeline |

---

## 11. Toujours vrai

- **On voit toujours si l'on est EN DIRECT et si l'on ENREGISTRE**, dans tous les
  espaces.
- **Arrêter un direct se confirme** ; le démarrer montre un compte à rebours
  annulable.
- **Une clé de diffusion n'apparaît jamais en clair** sans un geste (l'œil).
- **Le rouge veut dire « à l'antenne »** ; il n'est employé pour rien d'autre, sauf
  l'erreur (qui a son mot et son icône).
- **Rien ne coupe l'enregistrement local** : ni une destination qui tombe, ni un
  changement d'espace.
- **Tout icône a un mot**, sauf les commandes universelles du lecteur.
