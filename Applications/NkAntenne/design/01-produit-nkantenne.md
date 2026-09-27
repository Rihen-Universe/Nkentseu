# NkAntenne — ce qu'on attend de l'application

### Document 1 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 27/09/2026 à partir de la demande de Rodolf : *« une application de
> streaming, de capture d'écran et de fenêtre, en vidéo et/ou audio, utilisant le
> réseau local ou distant, se connectant à YouTube, Facebook, TikTok ou autre ;
> avec un design épuré, joli, thème sombre GitHub Dark Pro et GitHub Light Pro ;
> toujours sur Nkentseu. On pourra faire du montage vidéo, de la compression vidéo
> et audio, du découpage, du speed running, et avoir un lecteur vidéo et audio. »*
>
> **« NkAntenne » est un nom provisoire** (« à l'antenne » = en direct). Les noms
> évidents sont pris ou ambigus : `NKStream` est déjà un module du noyau
> (`Kernel/System/NKStream`), `NkCast` se confond avec l'arbre syntaxique de
> ConquerorLab (`NkcAst`). **Décision D1.**
>
> **Comme NKScena le 13/09, ce dossier ne contient que des documents** : pas de
> `main.cpp`, pas de `.jenga`. L'application naît quand sa condition de naissance
> est remplie (§14, P0).
>
> Compagnons : **02** (interface pour un humain), **03** (interface pour l'agent).

---

## 0. En une phrase

**NkAntenne est le studio du créateur : il capture l'écran, une fenêtre, un jeu,
une caméra et le son, les compose en scènes, les diffuse en direct vers YouTube,
Facebook, TikTok, Twitch ou un autre ordinateur du réseau, les enregistre, puis
permet de les monter, de les compresser et de les revoir — sans quitter
l'application.**

---

## 1. Positionnement

| référence | ce qu'on lui prend |
|---|---|
| **OBS Studio** | les **scènes** et les **sources**, le mode **studio** (aperçu / programme), le **mixeur**, les **filtres**, la diffusion multi-plateforme |
| **Streamlabs** | les **superpositions** prêtes (alertes, chat, compteurs) |
| **CapCut / Shotcut** | le **montage rapide** pour réseaux sociaux (vertical, sous-titres automatiques, vitesse) |
| **HandBrake** | la **file de compression** avec préréglages et taille cible |
| **VLC** | la **lecture** de tout, vidéo et audio |
| **Loom / ScreenPal** | l'**enregistrement de tutoriel** : zoom qui suit le curseur, clics visibles |

Et ce que NkAntenne a en propre :

- **Les superpositions sont des documents `.nkgui`**, conçus dans **NKUIDesign** :
  un bandeau, un compteur de spectateurs, une alerte d'abonné sont des interfaces
  comme les autres — avec leurs états, leurs animations, leurs liaisons de données.
  Aucun autre outil de diffusion n'a un vrai outil de design derrière ses
  superpositions.
- **Un seul fichier du direct au montage** : les pistes audio sont **enregistrées
  séparées**, les **marqueurs** posés pendant le direct deviennent des **points de
  coupe** au montage.
- **Le réseau local d'abord** : deux PC (l'un joue, l'autre diffuse), un téléphone
  comme caméra — sans service extérieur.
- **Tout est à nous** (Nkentseu, NKMedia, NKAudio, NKNetwork), et c'est un
  **gros consommateur** du noyau : il en éprouvera les encodeurs, le réseau et
  l'audio temps réel mieux que n'importe quel banc.

**Public** : créateurs de contenu, formateurs (cours filmés, tutoriels), joueurs,
Rihen Studio lui-même (making-of, démonstrations des applications, cours de
Polytech), et les écoles / églises / associations qui diffusent un événement.

---

## 2. Frontières

| dans NkAntenne | ailleurs |
|---|---|
| **capturer** : écran, fenêtre, zone, jeu, caméra, micro, audio système, audio d'une application | **film d'animation 3D** (scènes, caméras, rendu) → **NKScena** |
| **composer** : scènes, sources, superpositions, transitions, filtres | **concevoir** une superposition complexe → **NKUIDesign** (NkAntenne l'**importe** et la **règle**) |
| **diffuser** : plateformes, serveurs RTMP / SRT, réseau local | **le lecteur grand public** (visionnage collectif, streaming adaptatif, mode Drama) → **NkVideoPlayer** (spécifié le 21/07) |
| **enregistrer** : fichiers vidéo / audio, pistes séparées | **composer de la musique** → une **application à part** (§3) |
| **monter** ce qu'on a enregistré : couper, découper, vitesse, titres, sous-titres, audio | |
| **compresser** : file de conversion vidéo et audio | |
| **lire** : vidéo et audio (médiathèque du créateur) | |

### 2.1 Trois briques partagées, pour ne rien écrire deux fois

| brique | partagée avec | pourquoi |
|---|---|---|
| **la timeline** (Editor Kit) | NkAnima, NKScena | c'est son **troisième** consommateur — la règle des deux consommateurs est dépassée : elle **doit** être une brique |
| **le lecteur** (`NkVideoReader`, horloge maîtresse audio, rattrapage, seek H.264 — tout est livré dans NkVideoPlayer) | NkVideoPlayer | NkAntenne **emprunte** la lecture ; les fonctions grand public restent chez NkVideoPlayer |
| **le mixeur audio et ses VU-mètres** (rôle `Meter`, P14) | NKScena | même besoin, même composant |

### 2.2 Le montage de NkAntenne et celui de NKScena

| | NKScena | NkAntenne |
|---|---|---|
| ce qu'on monte | des **plans 3D** (des scènes, vivantes) | des **enregistrements** (des fichiers) |
| un plan trop court | on **rallonge la scène** | on ne peut pas inventer d'images |
| cible | un film | une vidéo YouTube, un Short, un TikTok, un cours |

**Même timeline, deux métiers.** Un créateur qui veut monter un film d'animation
va dans NKScena ; celui qui veut couper son direct d'hier va dans NkAntenne.

---

## 3. La composition musicale : une seconde application — ma recommandation

Rodolf : *« ça me plairait bien d'y ajouter de la composition musicale à la
FL Studio, pour la musique et les VFX de nos productions — sauf si tu en fais une
seconde application. »*

**Je recommande une seconde application**, pour quatre raisons :

1. **Un autre moteur** : un séquenceur musical exige un **graphe audio temps réel**
   à latence de quelques millisecondes, des instruments, des greffons d'effets, le
   MIDI, l'automation au pas près. NkAntenne a un **mixeur de diffusion** : même
   mot, autre métier.
2. **Un autre écran** : rouleau de piano, séquenceur à pas, patterns, liste de
   lecture, rack d'instruments — rien de cela ne ressemble à des scènes et des
   sources.
3. **Un autre public dans l'instant** : on compose **avant**, on diffuse
   **pendant**. Les mélanger chargerait NkAntenne de ce que personne n'utilise
   pendant un direct.
4. **Plus de consommateurs** : la musique composée sert **NKScena** (bande
   originale), **Nogee** (musique de jeu), **NkAnima** (VFX sonores) **et**
   NkAntenne (jingles, fonds sonores). Une application à part les sert tous, **par
   fichiers**, comme le reste de la famille.

**Ce qui les relie** : le **même moteur audio** (NKAudio), le **même format** de
piste et d'export, et un geste **« Envoyer vers NkAntenne »** (un jingle devient
une source sonore, une boucle devient un fond). **Décision D2** ; ses
spécifications peuvent suivre ces trois documents.

---

## 4. La capture

### 4.1 Les sources

| source | ce qu'elle capture | appui existant |
|---|---|---|
| **Écran** | un moniteur entier (plusieurs écrans : choisir) | — (voir §4.2) |
| **Fenêtre** | une fenêtre d'application, même recouverte | `PrintWindow` / `BitBlt` servent déjà la capture de NKCraft (fenêtre propre) |
| **Zone** | un rectangle de l'écran, déplaçable | — |
| **Jeu** | une application plein écran, à faible latence | — |
| **Caméra** | webcam, carte de capture | **NKCamera** (Media Foundation, V4L2, Camera2, AVFoundation, getUserMedia) |
| **Téléphone** | la caméra d'un téléphone, par le réseau local | NKCamera (Android) + NKNetwork (`NkDiscovery`, `NkReliableUDP`) — le principe est déjà décidé pour NKCraft (« téléphone pilotant une caméra ») |
| **Micro** | une entrée audio | `NkAudioCapture` |
| **Audio système** | ce que l'ordinateur joue | — (boucle de sortie : WASAPI loopback, PulseAudio / PipeWire monitor, ScreenCaptureKit) |
| **Audio d'une application** | le son **d'une seule** application | — (Windows 10 2004+, PipeWire) |
| **Média** | un fichier vidéo ou audio (en boucle ou non) | `NkVideoReader`, lecteurs audio |
| **Image**, **diaporama** | une image, une suite d'images | NKImage (12 codecs) |
| **Texte** | un texte (police, contour, ombre, défilement) | NKFont |
| **Superposition** | un document **`.nkgui`** conçu dans NKUIDesign, avec ses liaisons (compteurs, chat, alertes) | NKGui + monteur `.nkgui` |
| **Réseau** | le flux d'un autre ordinateur (NkAntenne, SRT) | NKNetwork |
| **Chronomètre** | un chronomètre / compte à rebours ; **si « speed running » voulait dire la course de jeu**, un chronomètre à **étapes** (splits) comme LiveSplit — **à confirmer, D6** | — |
| **Couleur**, **dégradé** | un fond | — |

### 4.2 La capture d'écran par système — l'état réel

**C'est le premier chantier technique**, et il est honnête de dire qu'il est vide :
aujourd'hui, Nkentseu capture **ses propres fenêtres** (lecture du tampon, ou
`PrintWindow` pour NKCraft), **pas l'écran d'un autre programme** en continu.

| système | voie à prendre | remarque |
|---|---|---|
| **Windows** | **Windows.Graphics.Capture** (fenêtre ou écran, Windows 10 1903+) ; **duplication de sortie DXGI** (écran entier, très rapide, reste sur le GPU) | les deux rendent une **texture GPU** : la conversion et l'encodage doivent rester sur le GPU pour tenir 60 i/s |
| **Linux** | **PipeWire + portail XDG** (Wayland, avec autorisation de l'utilisateur) ; `XShm` / `XComposite` (X11) | sous Wayland, **l'utilisateur choisit** ce qu'il partage : c'est le système qui le décide, pas nous |
| **macOS** | **ScreenCaptureKit** (12.3+) | autorisation « Enregistrement de l'écran » demandée par le système |
| **Android** | **MediaProjection** | autorisation à chaque session |
| **iOS** | **ReplayKit** (extension de diffusion) | contraintes fortes du système |
| **Web** | `getDisplayMedia` | l'utilisateur choisit à chaque fois |

📌 Ces API du système sont au même rang que DX12 ou Vulkan : **on les appelle, on
ne les réécrit pas** ; ce qui est à nous, c'est **ce qu'on fait des images**.

### 4.3 Les scènes

- Une **scène** = une liste de **sources** posées (position, taille, recadrage,
  rotation, ordre, visibilité).
- **Collections de scènes** (« Jeu », « Cours », « Podcast ») et **profils**
  (réglages de diffusion).
- **Mode studio** : **Aperçu** (ce qu'on prépare) à gauche, **Programme** (ce qui
  passe à l'antenne) à droite, bouton **Transition** entre les deux.
- **Transitions** : coupe, fondu, glissement, volet, **stinger** (une vidéo avec
  transparence), et **transition `.nkgui`** (animée dans NKUIDesign).
- **Raccourcis globaux** : changer de scène, couper un micro, poser un marqueur —
  **même quand NkAntenne n'a pas le focus** (on est dans son jeu).

### 4.4 Les filtres

| famille | filtres |
|---|---|
| **image** | recadrage, correction de couleur, **LUT** (`.cube`, déjà lu par NkVideoPlayer), netteté, flou, masque d'image, **incrustation** (fond vert / bleu), **suppression de fond par IA** (sans fond vert), mise à l'échelle |
| **audio** | gain, **porte de bruit**, **suppression de bruit** (`NkDenoiser` existe), compresseur, limiteur, égaliseur, **ducking** (la musique baisse quand on parle), décalage de synchronisation |

---

## 5. Le mixeur

- Une **tranche** par source audio : **VU-mètre** avec crête et **écrêtage signalé**,
  fader, sourdine, **écoute** (casque seul / casque et sortie / sortie seule),
  filtres.
- **Pistes d'enregistrement** : chaque source peut aller sur **une ou plusieurs
  des 6 pistes** du fichier (voix, jeu, musique, invités…). **Le direct** reçoit le
  mélange ; **l'enregistrement** garde les pistes séparées → au montage, on
  rebaisse la musique sans toucher à la voix.
- **Niveaux cibles** : un indicateur **LUFS** (volume perçu) avec la cible de la
  plateforme choisie.

---

## 6. La diffusion

### 6.1 Les destinations

| destination | comment | remarque |
|---|---|---|
| **YouTube** | serveur + **clé de diffusion** (RTMP / RTMPS) | la clé se trouve dans YouTube Studio |
| **Facebook** | serveur + clé (**RTMPS**) | Facebook exige le chiffré |
| **Twitch**, **Kick** | serveur + clé (RTMP / RTMPS) | |
| **TikTok** | serveur + clé **si le compte y a accès** | l'accès à la diffusion par clé dépend de l'éligibilité du compte |
| **Personnalisé** | n'importe quel serveur **RTMP(S)** ou **SRT** | un serveur de l'école, de l'église, de Rihen |
| **Réseau local** | un autre NkAntenne (ou un récepteur SRT) sur le même réseau, **découvert tout seul** | `NkDiscovery` : on ne tape pas d'adresse |

⚠️ **Les règles des plateformes changent** (protocoles acceptés, chiffrement
obligatoire, éligibilité, débits maximaux) : **chaque destination est à vérifier
contre la documentation officielle au moment de l'implémenter**, et les
préréglages sont **des fichiers de données** mis à jour sans recompiler.

- **Plusieurs destinations à la fois** (YouTube **et** Facebook **et** TikTok) :
  **un seul encodage** quand les réglages le permettent, un encodage par
  destination sinon (TikTok vertical, YouTube horizontal — voir §6.3).
- **Comptes liés** (plus tard, P6) : se connecter à la plateforme (OAuth) pour
  **régler le titre, la description, la vignette**, créer l'événement, et **lire le
  chat** dans NkAntenne. Cela demande d'**enregistrer une application** auprès de
  chaque plateforme et de passer leurs vérifications — c'est pourquoi la **clé de
  diffusion** vient d'abord : elle marche partout, sans accord.

### 6.2 Les protocoles

| protocole | pour | état dans Nkentseu |
|---|---|---|
| **RTMP** (sur TCP) + conteneur **FLV** | la plupart des plateformes | ❌ à écrire (sockets : ✅) |
| **RTMPS** (RTMP sur TLS) | Facebook, et de plus en plus les autres | ❌ RTMP + **TLS** : `NKNetwork` sait **déjà** se lier à **NKMbedTLS** (option de construction, `Externals/Libs/NKMbedTLS`) — voir D4 |
| **SRT** | serveurs personnalisés, réseau local et distant fiable | ❌ à écrire (`NkReliableUDP` en est proche) |
| **Protocole local NkAntenne** | deux NkAntenne sur le même réseau | ❌ sur `NkReliableUDP` + `NkDiscovery` + `NkBitStream` |
| **HLS** (sortie) | publier soi-même sur un serveur web | ❌ plus tard |

### 6.3 Le vertical et l'horizontal

**TikTok, Shorts, Reels** veulent du **9:16** ; YouTube, Twitch, du **16:9**.
NkAntenne permet une **scène double** : une disposition horizontale **et** une
verticale **des mêmes sources** (la caméra en haut, le jeu en bas en vertical), et
les diffuse **en même temps** chacune vers sa plateforme.

### 6.4 La santé du direct

En permanence, en bas de la fenêtre : **débit** envoyé (et cible), **images
perdues** (réseau / encodage / rendu, séparément), **processeur**, **GPU**, **durée**
du direct, **état** de chaque destination (connectée, reconnexion, erreur).
**Adaptation automatique** du débit si le réseau faiblit (option). **Reconnexion
automatique** si la connexion tombe, **sans** couper l'enregistrement local.

---

## 7. L'encodage — le cœur, et le risque principal

### 7.1 Ce qui existe

| brique | état |
|---|---|
| **H.264** encodeur **from scratch** (`NkH264Encoder`) | ✅ baseline, images I **et P** (GOP réglable), QP fixe ; écrit un flux Annex-B **décodable par ffmpeg, VLC, les navigateurs** |
| conteneurs | ✅ MP4, MOV, WebM, AVI, suite d'images ; **mux audio + vidéo** synchronisé (0 ms mesuré) |
| audio | ✅ **décodeurs** AAC, Opus, MP3, OGG Vorbis, FLAC, AMR ; **encodage : WAV et LPCM (dans MP4) seulement** |

### 7.2 Ce qui manque pour diffuser

| manque | pourquoi c'est bloquant |
|---|---|
| **contrôle de débit** (CBR / VBR contraint) | une plateforme attend un **débit constant** ; un QP fixe donne un débit qui explose à chaque scène agitée |
| **vitesse** : 1080p à 60 i/s **en temps réel** | un encodeur logiciel from scratch, baseline, sans SIMD ni parallélisme, **ne tiendra pas** ce rythme à court terme — il faut le **mesurer** tôt (P0) |
| **encodeur AAC** | RTMP / FLV transporte presque toujours de l'**AAC** ; on ne sait que le **décoder** |
| **encodeur Opus** | pour SRT, le réseau local, WebM — `NkOpusCodec` **ne fait que décoder** (vérifié le 27/09 : une seule fonction, `Decode`) |
| **encodeurs FLAC, MP3** | pour l'audio seul et la compression audio — les codecs du noyau **décodent** ; seul le **WAV / LPCM** s'écrit aujourd'hui |

### 7.3 La proposition

**Deux voies d'encodage, choisies par l'utilisateur, la plus rapide par défaut :**

1. **Encodeurs matériels, par les API du système** : Media Foundation (et au-delà,
   les SDK des fabricants NVENC / AMF / Quick Sync), VA-API (Linux), VideoToolbox
   (Apple), MediaCodec (Android). Même statut que la capture (§4.2) : **on appelle
   le système**. C'est la voie qui tient 1080p60 et 4K **sans charger le
   processeur** — indispensable quand le même PC fait tourner un jeu.
2. **L'encodeur Nkentseu** (`NkH264Encoder`) : amélioré en **contrôle de débit**,
   **parallélisme** (tranches), **SIMD** — la voie toujours disponible, et le
   terrain de progrès du noyau.

**Décision D3.** Sans la voie 1, le direct en 1080p60 n'existera pas avant très
longtemps.

---

## 8. L'enregistrement

- **Formats** : MP4 (par défaut), MOV, **MKV** et **MP4 fragmenté** — ces deux
  derniers **survivent à un plantage** : un fichier MP4 ordinaire dont l'index n'a
  pas été écrit est **illisible**. Proposition : enregistrer en **MP4 fragmenté**
  par défaut, et **remuxer** en MP4 ordinaire à l'arrêt (sans réencoder).
- **Pistes audio séparées** (§5), **qualité** indépendante de celle du direct
  (enregistrer en haute qualité **pendant** qu'on diffuse en débit réduit).
- **Tampon de relecture** : garder les **N dernières secondes** en mémoire, et un
  raccourci les **enregistre** (« clippe ça ! »).
- **Marqueurs** pendant le direct ou l'enregistrement (raccourci global) → ils
  réapparaissent **dans la timeline** du montage.
- **Captures d'écran** (image fixe) en un raccourci.
- **Audio seul** : enregistrer un podcast — **WAV** tout de suite ; FLAC, Opus, AAC quand leurs encodeurs existeront (§7.2).

---

## 9. Le montage

Sur **la timeline partagée** (§2.1), avec les métiers d'un **montage rapide de
créateur** :

| famille | fonctions |
|---|---|
| **couper** | rasoir, rogner, **supprimer un morceau et resserrer** (sans trou), déplacer, dupliquer |
| **découper automatiquement** | **les silences** (seuil, marge), **les changements de scène**, **aux marqueurs** posés pendant le direct |
| **vitesse** (« speed running » au sens montage — D6) | accéléré, ralenti, **rampes** (accélérer puis ralentir), **time-lapse**, marche arrière, figer une image ; **le son** : hauteur conservée ou non |
| **tutoriel** | **zoom qui suit le curseur**, surlignage du curseur, **clics visibles**, touches pressées affichées |
| **titres et sous-titres** | titres, cartons, **sous-titres automatiques** (NKSpeech : reconnaissance vocale **hors ligne**), styles de sous-titres « réseaux sociaux » (mot par mot) |
| **format** | **recadrage vertical automatique** (9:16) qui suit le sujet ; 1:1 ; 16:9 |
| **audio** | pistes séparées de l'enregistrement, **suppression de bruit**, **normalisation** au niveau de la plateforme (LUFS), ducking, fondus |
| **image** | correction de couleur, LUT, flou d'une zone (masquer une donnée personnelle **qui apparaît à l'écran**) |
| **transitions**, **superpositions `.nkgui`** | les mêmes qu'en direct |
| **chapitres** | des marqueurs → les chapitres de la description YouTube |

**Export** : des **préréglages par plateforme** (YouTube 1080p / 4K, Shorts,
TikTok, Instagram, un fichier pour WhatsApp < 16 Mo…), et la **file de
compression** (§10).

---

## 10. La compression

Une **file de conversion**, façon HandBrake :

- **Entrées** : fichiers vidéo, audio, ou un export de montage.
- **Préréglages** : par plateforme, par usage (archive haute qualité, envoi rapide,
  aperçu), par appareil.
- **Taille cible** : « **tenir en 25 Mo** » → le débit est calculé (deux passes).
- **Audio** : débit, canaux, **extraire l'audio** d'une vidéo, convertir entre WAV,
  FLAC, MP3 (encodeur à écrire), Opus, AAC.
- **Découper** : garder un intervalle, **découper en morceaux** de N minutes ou de N
  Mo (pour les messageries).
- **Estimation** : la **taille** et la **durée** estimées **avant** de lancer, puis
  l'écart mesuré après (la même discipline que le rendu de NKScena).
- **En lot**, en tâche de fond, avec pause / reprise.

---

## 11. Le lecteur

**Deux lecteurs, un moteur** : NkAntenne a une **médiathèque du créateur** (ses
enregistrements, ses montages, ses exports) avec un lecteur **vidéo** et un lecteur
**audio** ; **NkVideoPlayer** reste le **lecteur grand public**.

| NkAntenne — médiathèque | NkVideoPlayer |
|---|---|
| lire ses fichiers : vidéo, audio, image par image | lire tout, partout |
| forme d'onde, **A-B en boucle**, vitesse, **image par image**, marqueurs | streaming adaptatif (HLS / DASH) |
| **« Ouvrir au montage »**, **« Compresser »**, **« Envoyer »** | visionnage collectif, mode Drama, profils |
| pistes audio séparées, choix de la piste | multilingue, doublage IA |

Le **lecteur audio** : liste de lecture, forme d'onde, boucle, vitesse, niveaux,
métadonnées ; lit WAV, FLAC, MP3, OGG, Opus, AAC (tous **décodés** par le noyau
aujourd'hui).

---

## 12. L'IA — locale

Mêmes règles que la famille : **commandes éditables**, **aperçu**, **jamais
obligatoire**, **modèles NKAI locaux**, licence qui voyage avec les poids.

| capacité | où |
|---|---|
| **suppression de bruit** du micro | direct et montage |
| **suppression / flou du fond** de la caméra sans fond vert | direct |
| **sous-titres automatiques** (FR / EN d'abord), traduction | montage ; **en direct** plus tard |
| **découpe des silences**, des hésitations (« euh ») | montage |
| **moments forts** d'un long direct (pics de son, de chat, de mouvement) → clips proposés | montage |
| **recadrage vertical** qui suit le visage ou l'action | montage |
| **titre, description, chapitres** proposés à partir des sous-titres | export |

---

## 13. Sécurité et vie privée

- **Les clés de diffusion sont des secrets** : **chiffrées** au repos, **jamais**
  écrites dans un journal, **masquées** à l'écran (œil pour les montrer),
  **jamais** dans une capture de NkAntenne lui-même. Une clé qui fuit, c'est
  quelqu'un d'autre qui diffuse sur votre chaîne.
- **Exclure des fenêtres de la capture** (gestionnaire de mots de passe,
  messagerie) : liste d'exclusion, respectée **même en capture d'écran entier**
  quand le système le permet.
- **Mode privé** : masquer les notifications du système pendant un direct.
- **Indicateur permanent** « EN DIRECT » / « ENREGISTREMENT » : on ne diffuse jamais
  sans le savoir.
- **Droits d'auteur** : un avertissement sur la **musique** utilisée (les
  plateformes coupent ou démonétisent) — la seconde application (§3) fournit une
  musique **à soi**.

---

## 14. Phasage

**Une tranche qui marche > un scaffold de plus.**

| palier | contenu | critère de fin |
|---|---|---|
| **P0 — condition de naissance** | capture d'**écran** et de **fenêtre** sous Windows (Windows.Graphics.Capture) ; **enregistrement local** MP4 fragmenté (H.264 + audio) ; **mesure** de l'encodeur Nkentseu en 1080p30 / 60 | 10 minutes d'écran + micro enregistrées ; le fichier se lit dans VLC **et** dans NkVideoPlayer ; **le processus tué à la 5ᵉ minute laisse un fichier lisible** ; le nombre d'images par seconde encodées **publié** |
| **P1 — le studio** | scènes, sources (écran, fenêtre, caméra, micro, audio système, image, texte, média), mixeur et VU-mètres, filtres de base, mode studio ; encodeur matériel (D3) ; **encodeur AAC** | un tutoriel de 30 min : écran + caméra en incrustation + micro débruité, **sans image perdue** |
| **P2 — le direct** | **RTMP** puis **RTMPS** ; YouTube, Facebook, Twitch, personnalisé ; **santé du direct**, reconnexion ; marqueurs | 1 heure en direct sur YouTube **et** Facebook simultanément, **zéro coupure**, enregistrement local intact |
| **P3 — le réseau local** | deux NkAntenne sur le réseau (découverte, SRT / protocole local), téléphone comme caméra ; **superpositions `.nkgui`** | un PC joue, un autre diffuse ; latence mesurée publiée |
| **P4 — la médiathèque et le montage** | lecteur vidéo / audio ; timeline partagée ; couper, découper les silences, vitesse, titres, sous-titres automatiques, vertical ; export par plateforme | le direct d'une heure devient **un Short de 45 s sous-titré** en moins de 10 minutes |
| **P5 — la compression** | file, préréglages, taille cible, audio, découpe en morceaux, estimation | « 25 Mo » demandés, **fichier de 24,x Mo** obtenu ; écart d'estimation publié |
| **P6 — plus loin** | comptes liés (titre, chat, alertes), TikTok, scène double 16:9 + 9:16, tampon de relecture, IA avancée, Linux / macOS / Android | |

---

## 15. Règles transverses

1. **Ne jamais perdre un enregistrement** : fichier fragmenté, disque plein signalé
   **avant** qu'il soit plein, reconnexion du direct **sans** toucher au fichier
   local.
2. **Ne jamais diffuser sans le savoir** : l'état « EN DIRECT » est visible partout.
3. **Les clés sont des secrets.**
4. **Toute action est une commande** (annulable au montage ; raccourcis globaux au
   direct).
5. **Chaque destination est vérifiée contre la documentation de la plateforme**,
   et ses réglages sont des **données**.
6. **Les API du système** (capture, encodage matériel) **s'appellent** ; ce qui est à
   nous, c'est ce qu'on fait des images et du son.
7. **Zéro-STL, NKMemory, NKLogger**, conventions du dépôt ; **interface en
   `.nkgui`**.
8. **Thème GitHub Dark Pro / GitHub Light Pro** (demande de Rodolf) — voir doc 02.

---

## 16. Décisions

| # | question | proposition |
|---|---|---|
| D1 | le **nom** | provisoire « NkAntenne » ; à choisir par Rodolf (éviter `NKStream`, module existant) |
| D2 | la **composition musicale** | **une seconde application**, qui partage NKAudio et exporte vers NkAntenne, NKScena, Nogee, NkAnima (§3) |
| D3 | les **encodeurs matériels** par les API du système | **oui**, en voie par défaut ; l'encodeur Nkentseu reste la voie toujours disponible |
| D4 | **TLS** (RTMPS) : NKMbedTLS (bibliothèque tierce, déjà branchable dans NKNetwork) ou TLS from scratch | **NKMbedTLS pour P2** (la sécurité d'un TLS écrit à la main est un chantier de plusieurs mois et un risque réel) ; un TLS maison, s'il est voulu, **après** et derrière le même interrupteur |
| D5 | le **lecteur** | la médiathèque du créateur dans NkAntenne, **même moteur** que NkVideoPlayer, qui reste le lecteur grand public |
| D6 | « **speed running** » | compris comme **la vitesse au montage** (accéléré, ralenti, rampes, time-lapse) ; **si c'était la course de jeu** (chronomètre à étapes affiché pendant le direct), c'est une **source** à ajouter (§4.1) — à confirmer |
| D7 | le **thème** | **GitHub Dark Pro / Light Pro**, comme demandé — sa version sombre **existe déjà** dans le dépôt (ConquerorLab, `NkcLabTheme.h`), et les **jetons** de la famille rendent ce choix gratuit : mêmes noms, autres valeurs |
