# NkNgoma — ce qu'on attend de l'application

### Document 1 — Spécification produit

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 27/09/2026. Origine : la demande de Rodolf pour NkAntenne — *« ça me
> plairait bien d'y ajouter de la composition musicale à la FruityLoops, pour la
> musique et les VFX de nos productions, sauf si tu en fais une seconde
> application »* — et la recommandation d'en faire **une seconde application**
> (NkAntenne 01 §3, décision D2). Rodolf, le même jour : *« est-ce qu'on peut définir
> la deuxième application ? »*, puis ses choix :
>
> | question | réponse de Rodolf |
> |---|---|
> | jusqu'où va le son | **musique + design sonore + mixage de film** |
> | l'IA | **assistant complet** (modèles maison, hors ligne ; l'humain retouche) |
> | l'identité musicale | **tout** : instruments camerounais et africains, afrofuturisme et électronique, orchestral et cinéma, musique de jeu adaptative |
> | le nom | **NkNgoma** — du *ngoma*, le tambour |
>
> Consigne : **ne pas modifier NKScena**. Les frontières avec NKScena sont écrites
> **ici**, du côté de NkNgoma (§2.2) ; ce qui demanderait un geste dans NKScena est
> une **proposition** listée au §15, à discuter plus tard.
>
> **Comme NKScena et NkAntenne, ce dossier ne contient que des documents** : pas de
> `main.cpp`, pas de `.jenga`. L'application naît quand sa condition de naissance est
> remplie (§14, P0).
>
> Compagnons : **02** (interface pour un humain), **03** (interface pour l'agent).

---

## 0. En une phrase

**NkNgoma est le studio sonore de la famille Rihen : on y compose la musique (du
balafon au synthétiseur, de la boucle à la bande originale), on y fabrique les sons
d'un film ou d'un jeu à l'image près, on y mixe le film jusqu'au master — avec une IA
qui propose des notes et des pistes que l'on retouche, et des exports que NKScena,
NkAnima, Nogee et NkAntenne prennent tels quels.**

---

## 1. Positionnement

### 1.1 Les références, et ce qu'on en prend

| référence | ce qu'on en prend | ce qu'on n'en prend pas |
|---|---|---|
| **FL Studio** | le **rack de canaux** et le **séquenceur à pas** ; les **patterns** posés dans une **playlist** ; le **rouleau de piano** le plus rapide du marché ; les raccourcis F5 / F6 / F7 / F9 | la fenêtre flottante pour tout ; le mixeur à 125 pistes figées |
| **Ableton Live** | la **vue session** (des scènes de clips lancées en direct) — elle est la **musique de jeu adaptative** avant la lettre | le modèle de périphériques en chaîne unique |
| **Reaper** | le **routage libre** (toute piste peut être un bus), la légèreté | l'interface austère |
| **Pro Tools / Nuendo / Fairlight (DaVinci)** | le **son à l'image** : vidéo de référence, timecode, bruitages calés, **mixage de film** en stems, surround, sonie normée | la dépendance à un matériel dédié |
| **Wwise / FMOD** | la **musique interactive** : couches, états, transitions, stingers, paramètres de jeu | l'intégration par un SDK tiers — Nogee lit nos fichiers |
| **Kontakt / Decent Sampler** | l'**échantillonneur multi-couches** (vélocité, alternances, articulations) | le format propriétaire |

### 1.2 Ce qui nous distingue

1. **L'Afrique comme premier répertoire, pas comme banque exotique.** Les
   instruments camerounais et africains sont **au premier rang** : échantillonnés par
   nous, joués avec **leurs accordages propres** (un balafon n'est pas tempéré — §6.3),
   avec **leurs rythmes** prêts en motifs (bikutsi, makossa, assiko, bend-skin,
   mangambeu, ambassé bey…). Aucun logiciel du marché ne le fait sérieusement.
2. **Une seule application pour la note, le bruitage et le mix final.** La musique,
   le design sonore et le mixage de film partagent le même moteur, la même timeline,
   les mêmes effets.
3. **La famille, par fichiers.** Une musique de jeu part vers Nogee **avec ses
   règles** ; un bruitage calé sur un événement d'animation vient de NkAnima **avec
   ses repères** ; un mix de film revient à NKScena **en stems**.
4. **L'IA qui écrit des notes, pas du son figé.** Ce qu'elle propose est **éditable**
   : des notes dans un rouleau de piano, des pistes séparées, des réglages — jamais un
   fichier audio qu'on ne peut que garder ou jeter (sauf le bruitage généré, §11.4,
   qui est un échantillon éditable).
5. **Hors ligne.** Tout fonctionne sans réseau, IA comprise.

---

## 2. Frontières

| NkNgoma fait | NkNgoma ne fait pas |
|---|---|
| composer : instruments, patterns, arrangement, enregistrement (voix, instruments), MIDI | le montage **d'images** (NKScena, NkAntenne) |
| le **design sonore** : bruitages, ambiances, effets synthétisés, calés à l'image | la **diffusion en direct** (NkAntenne) |
| le **mixage de film** : stems, bus, surround, binaural, sonie, master | le rendu d'images, l'étalonnage (NKScena) |
| la **musique de jeu adaptative**, écrite et essayée ici | l'**exécution** dans le jeu (Noge, qui lit le fichier) |
| l'**export** vers la famille | la gestion des droits de diffusion (on fournit une musique **à soi**) |

### 2.1 Les briques partagées — rien d'écrit deux fois

| brique | où | usage ici |
|---|---|---|
| **NKAudio** | `Kernel/Runtime/NKAudio` (~3 300 lignes d'en-têtes) : moteur temps réel, 256 voix, **bus hiérarchiques avec sidechain**, effets (délai, réverbération, compresseur, limiteur, biquad, égaliseur 3 bandes, distorsion, chorus), **HRTF**, codecs FLAC / MP3 / OGG / Opus **en lecture**, flux, capture (`NkAudioCapture`), débruiteur (`NkDenoiser`), analyse (RMS, crête, tempo, hauteur, FFT, spectrogramme), générateur (sons, bruit, percussions), backends WASAPI / CoreAudio / ALSA / AAudio / WebAudio | **le moteur**. NkNgoma y ajoute le **graphe de traitement à faible latence** (§5) — **dans NKAudio**, pas dans l'application, pour que Noge et NkAntenne en profitent |
| **la timeline de la famille** | brique de l'Editor Kit, partagée par NkAnima et NKScena | la **playlist** (arrangement) et la piste **image** du son à l'image |
| **NKMedia** | lecture vidéo, `NkH264Encoder` (MP4 + LPCM) | la **vidéo de référence** du son à l'image |
| **NKSpeech** | `NkAudioFeatures` | l'IA (transcription, détection de hauteur, séparation) |
| **le système de greffons** | NKUIDesign doc 3 §20bis | les instruments et effets tiers (§8) |
| **NkAntenne** | doc 01 | le **même mixeur de voies** côté interface (composant `TrancheMixeur`), et les **encodeurs audio** qui manquent aux deux (§12.2) |

### 2.2 NkNgoma et NKScena — le partage, écrit d'ici

NKScena a décidé (sa décision R3) d'avoir **son** son dès son palier P2 : pistes,
mixage de base, VU-mètres, export du film avec son. **NkNgoma ne retire rien à
NKScena** et **ce document ne modifie pas NKScena**. Le partage proposé :

| | NKScena | NkNgoma |
|---|---|---|
| poser un son sur le montage, le caler, régler son volume | ✅ (son métier) | — |
| composer la bande originale | musique témoin | ✅ |
| fabriquer les bruitages et ambiances | importer des fichiers | ✅ (bibliothèque, synthèse, calage à l'image) |
| **mixage final** (stems, surround, sonie, master) | mixage de base | ✅ |
| **l'aller-retour** | exporte une **session son** (§12.3) ; réimporte les **stems** | importe la session, mixe, exporte les stems et le master |

📌 **Le seul point de contact est un fichier** (la session son, §12.3), comme partout
dans la famille. Tant que NKScena ne sait pas l'écrire, NkNgoma importe **une vidéo
de référence et des fichiers audio** — ce que NKScena sait déjà produire (WAV, MP4).
Les gestes que cela demanderait dans NKScena sont listés au §15 (**à discuter**, rien
n'est écrit dans ses documents).

---

## 3. Les quatre métiers, et leur espace

| espace | métier | à la façon de | touche |
|---|---|---|---|
| **Composer** | écrire la musique : rack de canaux, séquenceur à pas, rouleau de piano, playlist | FL Studio | `F6` rack · `F7` rouleau · `F5` playlist |
| **Son à l'image** | bruitages, ambiances, effets sonores calés sur une vidéo, des repères d'animation, un timecode | Pro Tools, Fairlight | `F8` |
| **Mixer** | le mixeur de voies : inserts, départs, bus, automation — de la chanson au film | FL Studio, Nuendo | `F9` |
| **Jeu** | la musique **adaptative** : couches, états, transitions, stingers, essayée avec des paramètres de jeu simulés | Wwise, vue session d'Ableton | `F10` |

Plus un espace **Master** (sonie, limiteur, formats de sortie, stems) ouvert à l'export,
et la **Bibliothèque** (instruments, échantillons, motifs, préréglages) toujours à
portée, à gauche.

📌 **Un seul projet** traverse les quatre espaces : une chanson peut être composée,
puis habillée de bruitages à l'image, mixée, et découpée en couches de jeu **sans
quitter le fichier**.

---

## 4. Composer

### 4.1 Le rack de canaux et le séquenceur à pas

- Un **canal** = un instrument (échantillonneur, synthé, instrument africain, greffon)
  ou un **clip audio**. Le rack les liste, chacun avec sourdine, solo, volume,
  panoramique, et **sa rangée de pas**.
- **Séquenceur à pas** : 16 pas par défaut (réglable de 1 à 64), vélocité, hauteur,
  **décalage fin** (le *swing* par pas), probabilité, répétition (*ratchet*).
- **Mesures composées** : 6/8, 12/8, 7/8, 5/4 et toute **signature écrite** — le
  bikutsi est en 6/8 ou 12/8 selon l'écoute ; un séquenceur qui ne sait que 4/4 le
  massacre.
- **Swing et placement** par canal **et** par motif rythmique : un placement
  « bikutsi » ou « makossa » est un **gabarit de groove** (§6.4), pas un réglage
  magique.

### 4.2 Le rouleau de piano

- Notes (hauteur, début, durée, vélocité, **micro-hauteur** en cents), glisser,
  étirer, quantifier (à la grille, **au groove**, partielle en %), humaniser.
- **Outils** : crayon, pinceau (peindre des notes répétées), sélection, coupe,
  glissando (*slide*), accords (poser un accord nommé), arpégiateur, **échelle
  aimantée** (les notes hors de la gamme choisie sont grisées et évitées).
- **Gammes et accordages** : tempéré par défaut ; **gammes pentatoniques** et **modes**
  africains, et **accordages non tempérés** par instrument (§6.3) — la grille du
  rouleau suit l'accordage de l'instrument, pas le clavier de piano.
- **Pistes fantômes** : les notes des autres canaux, en transparence.
- **Articulations** (pizzicato, legato, étouffé, frappe au bord, frappe au centre)
  sur une rangée dédiée, pour les instruments qui en ont.
- **Automation** sous les notes (volume, filtre, n'importe quel paramètre).

### 4.3 Les patterns et la playlist

- Un **pattern** = des pas et des notes pour plusieurs canaux ; il se **pose** dans la
  **playlist** (la timeline de la famille), se répète, se varie.
- La playlist mélange **patterns**, **clips audio** et **clips d'automation**, sur des
  pistes libres (une piste n'est pas un instrument : comme FL Studio).
- **Marqueurs** (couplet, refrain, pont), **tempo et signature variables** le long du
  morceau, **boucle**.
- **Rendre un pattern en audio** (figer) et **défiger** : non destructif.

### 4.4 Enregistrer

- **Audio** : voix, guitare, instruments acoustiques — par `NkAudioCapture` ; prises
  multiples (*comping* : choisir le meilleur morceau de chaque prise), métronome,
  pré-décompte, **compensation de latence** mesurée (§5.3).
- **MIDI** : clavier, pads, contrôleurs — enregistrement en boucle (*overdub*),
  **enregistrement de pas** (une touche = un pas).
- **Débruitage** à l'entrée (`NkDenoiser` existe).

### 4.5 Les instruments

| instrument | quoi | état |
|---|---|---|
| **Échantillonneur** | multi-couches (vélocité, alternances aléatoires, articulations), boucles, enveloppes, filtres, **accordage par instrument** | ✚ — le cœur ; tous les instruments africains et orchestraux sont des **banques** pour lui |
| **Boîte à rythmes** | pads, kits, choke groups (le charleston ouvert coupé par le fermé) | ✚ |
| **Synthé soustractif** | 3 oscillateurs, filtres, enveloppes, LFO, unisson | ✚ |
| **Synthé FM** | 4 à 6 opérateurs | ✚ (P3) |
| **Synthé à table d'ondes** | tables, morphing | ✚ (P3) |
| **Basse 808 / log drum** | glissé, saturation — l'afrobeats et l'amapiano en vivent | ✚ |
| **Générateur** existant | `AudioGenerator` : sons, bruit, percussions synthétiques | ✅ — sert de base aux synthés et au design sonore |

### 4.6 Les effets

Existants dans NKAudio : **délai, réverbération, compresseur, limiteur, filtre
biquad, égaliseur 3 bandes, distorsion, chorus**, HRTF. À ajouter : **égaliseur
paramétrique** (8 bandes, avec analyseur), **compresseur multibande**, **porte**,
**de-esser**, **saturation**, **flanger / phaser**, **réverbération à convolution**
(réponses impulsionnelles — dont des lieux camerounais enregistrés par nous : une
case, une église, une forêt), **correction de hauteur**, **étirement temporel**,
**sidechain** (déjà dans les bus de NKAudio).

---

## 5. Le moteur — le risque principal

### 5.1 Ce qui existe

NKAudio est un **moteur de jeu** : 256 voix, bus, effets, 3D. Il est **fait pour jouer
des sons**, pas encore pour **traiter un graphe** d'instruments et d'effets à faible
latence, note par note, avec automation à l'échantillon.

### 5.2 Ce qui manque, dans NKAudio

| manque | pourquoi |
|---|---|
| **graphe de traitement** : nœuds (instruments, effets, bus) reliés, ordonnés, calculés **en parallèle** quand ils sont indépendants | un projet de 60 canaux et 150 effets ne tient pas sur un seul cœur |
| **événements à l'échantillon** : notes, automation, horodatés dans le bloc | une note à 1 ms près ; une automation sans escaliers |
| **latence de 3 à 10 ms** aller-retour, taille de bloc réglable (64 à 1024), **mode exclusif** WASAPI, CoreAudio, **JACK / PipeWire** sous Linux | jouer d'un clavier sans décalage perceptible |
| **compensation de latence** des effets (*PDC*) | un effet à anticipation (limiteur) décale sa piste |
| **rendu hors ligne plus rapide que le temps réel**, et **déterministe** | exporter une bande originale de 90 min ; deux rendus **identiques à l'octet** |
| **MIDI** : entrée / sortie des périphériques, fichiers SMF (lecture, écriture), **MIDI 2.0** plus tard | aucun module MIDI n'existe dans le dépôt (relevé du 27/09) |
| **étirement temporel et changement de hauteur** de qualité | caler une boucle sur le tempo, un bruitage sur l'image |

📌 **Tout cela va dans NKAudio** (couche Runtime), **pas** dans NkNgoma : Noge en a
besoin pour la musique adaptative **en jeu** (§9), NkAntenne pour son mixeur.
L'application n'est qu'une **interface** sur ce moteur.

### 5.3 La latence se mesure, elle ne se suppose pas

Un **test de boucle** (sortie reliée à l'entrée, ou micro devant le haut-parleur) mesure
la latence aller-retour réelle et **règle la compensation d'enregistrement**. Le
chiffre est affiché dans la barre d'état, en millisecondes, **mesuré**.

### 5.4 ASIO

Les cartes son professionnelles sous Windows parlent **ASIO**. Depuis le 30/10/2025,
Steinberg publie le **SDK ASIO sous double licence : GPLv3 ou licence propriétaire
Steinberg** (et le SDK VST 3 sous licence MIT). NkNgoma n'étant pas sous GPL, **ASIO
suppose l'accord propriétaire de Steinberg** (gratuit, à signer) ou reste absent. En
attendant, le **mode exclusif WASAPI** donne des latences proches sur le matériel
courant. **Décision D4.**

---

## 6. L'Afrique au premier rang

### 6.1 Les instruments, échantillonnés par nous

| famille | instruments (Cameroun d'abord, puis le continent) |
|---|---|
| **percussions** | **ngoma**, tam-tam, **nkul** (tambour à fente, qui « parle »), djembé, tama (tambour d'aisselle), udu, hochets et sonnailles, cloches doubles (*gankogui*), bâtons |
| **idiophones mélodiques** | **balafon** (plusieurs, chacun **avec son accordage**), **sanza / likembe / mbira**, xylophones de l'Ouest |
| **cordes** | **mvet** (harpe-cithare des Beti), **kora**, ngoni, arc musical, luth |
| **vents** | flûtes peules, trompes, cornes |
| **voix** | chœurs, cris, appels — **enregistrés avec consentement et contrat** (§13) |

- **Plusieurs couches** (vélocités), **plusieurs alternances** (aucune frappe
  identique à la précédente), **articulations** (frappe au centre, au bord, étouffée ;
  pincé, glissé).
- **Chaque banque porte sa fiche** : instrument, facteur, région, musicien, lieu et
  date d'enregistrement, accordage mesuré, licence. *Un échantillon sans provenance
  n'entre pas dans la bibliothèque.*
- **Chantier de production**, pas de code : Rihen a un studio (Rodolf : un associé
  couvre la production studio). C'est **le plus long** et le **plus précieux** de
  l'application — et c'est aussi le **corpus** de l'IA (§11.5).

### 6.2 Afrofuturisme et électronique

Synthés, basses 808 et *log drum*, pads, textures ; **fusion** : un balafon
passé dans une table d'ondes, un nkul qui déclenche un synthé. Des **kits** et des
**préréglages** faits maison, nommés, documentés.

### 6.3 Les accordages — la vraie différence

Un balafon ou une sanza **n'est pas accordé en tempérament égal**, et deux balafons ne
sont pas accordés pareil. NkNgoma :

- mesure l'**accordage réel** d'une banque à l'enregistrement (analyse de hauteur
  existante, `AudioAnalyzer`) et l'enregistre dans sa fiche ;
- donne à chaque instrument **son accordage** (fichier d'échelle : liste d'intervalles
  en cents, compatible avec le format Scala `.scl` pour l'échange) ;
- fait **suivre la grille** du rouleau de piano à cet accordage (les rangées sont les
  lames du balafon, pas les touches d'un piano) ;
- permet de **jouer ensemble** un balafon et un piano : soit le balafon garde son
  accordage (et l'outil **montre** les écarts), soit on **réaccorde** l'un vers l'autre
  — un choix explicite, jamais silencieux.

### 6.4 Les rythmes, en motifs prêts

**Gabarits de groove** (placement et accents, par pas) et **motifs** complets pour :
bikutsi, makossa, assiko, bend-skin, mangambeu, ambassé bey, afrobeat,
highlife, rumba congolaise, ndombolo, coupé-décalé, amapiano. Chacun avec **sa fiche**
(origine, signature, tempo usuel, instruments typiques) — écrite avec des musiciens,
pas devinée.

### 6.5 Orchestral et cinéma

Cordes, cuivres, bois, percussions de film, chœurs. ⚠️ **La question des banques est
d'abord juridique** : enregistrer un orchestre coûte cher ; les banques gratuites ont des
licences variées (certaines interdisent la redistribution dans un logiciel). **Décision
D6** : commencer par des banques **CC0 ou sous licence de redistribution vérifiée**,
puis enregistrer nos propres ensembles (quatuor, fanfare, chorale) au fil des
productions.

---

## 7. Le son à l'image

### 7.1 La référence

- **Vidéo de référence** : un MP4 ou une suite d'images de NKScena, de NkAnima, de
  NkAntenne — lue en synchronisation, **image par image**, avec son **timecode**
  (SMPTE, 24 / 25 / 30 i/s).
- **Repères importés** : les **événements** d'une animation de NkAnima (pas posé,
  impact, porte qui claque) et les **coupes** d'un montage de NKScena — par la session
  son (§12.3 ; leur export est une proposition du §15, à discuter). Ils arrivent comme
  **marqueurs** sur la timeline : un bruitage se cale **sur un repère**, et le suit si
  l'animation change (§12.3).
- **Glisser un son sur un repère** le cale au point de synchronisation du son (son
  **attaque**, pas son début de fichier).

### 7.2 La bibliothèque de bruitages

- Recherche par **mot**, par **catégorie** (UCS, *Universal Category System*, le
  standard libre de nommage des bruitages) et **par ressemblance** (« des sons comme
  celui-ci » — §11).
- **Aperçu** au tempo et à la hauteur du projet ; **glisser** dans la timeline.
- Nos propres **enregistrements de terrain** (marchés, pluie, forêt, circulation de
  Yaoundé et Douala), avec leur fiche de provenance.

### 7.3 Le design sonore

- **Couches** : un impact = sub + claquement + débris + queue de réverbération, réglés
  ensemble, **exportés en un seul son** ou en couches.
- **Synthèse procédurale** d'effets : vent, pluie, feu, eau, souffle, *whoosh*, impacts,
  magie, interfaces (bips, clics), moteurs — **paramétrés** (vitesse, taille, matière),
  donc **animables** par une courbe ou par un repère.
- **Variations** : un son de pas en 12 variantes (hauteur, timbre, attaque), pour que
  le jeu ne répète jamais la même frappe.
- **Doublage et voix** : enregistrement de répliques calées sur l'image (ADR), avec
  boucle sur le passage, décompte, prises multiples.

---

## 8. Mixer — de la chanson au film

### 8.1 Le mixeur

- **Voies** : chaque canal, piste, bus ; **inserts** (effets en série), **départs**
  (vers des bus d'effets), **routage libre** (toute voie peut alimenter toute autre,
  sans boucle), **sidechain** (existant dans NKAudio).
- **Automation** de tout paramètre, lue / écrite / accrochée (*latch*) / au toucher.
- **Groupes** (VCA), **instantanés** de mix (comparer A / B).
- **Mesures** : crête, RMS, **sonie** (LUFS intégrée, court terme, momentanée, *true
  peak*), corrélation de phase, spectre.

### 8.2 Le mixage de film

- **Stems** : dialogues, musique, effets, ambiances (**D / M / E**), et leurs sous-bus ;
  export **séparé** (pour la VF, la VO, les versions sans musique).
- **Formats** : stéréo, **5.1**, **7.1**, et **binaural** pour le casque (HRTF de
  NKAudio). Panoramique surround (joystick, divergence, LFE). **Atmos** et objets :
  **plus tard**, et seulement par un format ouvert (ADM BWF) — pas de dépendance à un
  SDK propriétaire.
- **Sonie** : normes **EBU R128** (−23 LUFS, cinéma et télévision), **−14 LUFS** pour le
  streaming, réglables ; **rapport de sonie** à l'export.
- **Réduction** (*downmix*) 5.1 → stéréo **vérifiée** : on l'écoute, on la mesure.

### 8.3 Le master d'une chanson

Égaliseur, compresseur multibande, limiteur, **sonie cible** par destination
(streaming, radio, vidéo), **comparaison** avec un morceau de référence (écoute
alternée à sonie égale), export multi-formats.

---

## 9. La musique de jeu adaptative

### 9.1 Le modèle

| notion | sens | exemple |
|---|---|---|
| **segment** | un morceau de musique avec ses points de sortie autorisés (fin de mesure, de phrase) | « exploration A » |
| **couche** | une piste qui entre ou sort **selon un paramètre** | les percussions montent avec l'**intensité** |
| **état** | un ensemble de segments et de couches | Exploration, Combat, Tension, Victoire |
| **transition** | comment on passe d'un état à un autre : au prochain temps, à la prochaine mesure, par un **segment de transition**, en fondu | Exploration → Combat : à la mesure, par un roulement de tam-tam |
| **stinger** | un court motif joué **par-dessus** à un événement, calé sur le temps | un accord de balafon quand on ramasse un objet |
| **paramètre** | une valeur du jeu (0 à 1, ou une étiquette) | intensité, santé, heure du jour, lieu |

### 9.2 Écrire et essayer ici

- L'espace **Jeu** montre les états en **graphe** (comme une machine à états
  d'animation) et les couches en **grille** (comme la vue session d'Ableton).
- **Simulation** : des curseurs pour les **paramètres** et des boutons pour les
  **événements** ; on **entend** les transitions telles que le jeu les jouera.
- **Enregistrer une partie simulée** : un parcours de paramètres rejouable (le même
  principe que les tests d'interaction de NKUIDesign) — pour vérifier qu'aucune
  transition ne tombe à contretemps.

### 9.3 Vers Nogee

Export en **un fichier de musique interactive** (§12.2) : les segments en audio, les
règles en données. **Noge** le joue avec le moteur de NKAudio (§5.2) ; Nogee l'importe
comme un asset et relie ses **paramètres** à des variables de jeu. *Ce qu'on entend
dans NkNgoma est ce que le jeu jouera* — même moteur, mêmes règles.

---

## 10. La bibliothèque

- **Instruments** (banques, synthés, greffons), **échantillons**, **boucles**,
  **motifs** (rythmes, accords, mélodies), **préréglages**, **bruitages**, **réponses
  impulsionnelles**, **accordages**, **gabarits de projet** (chanson afrobeat, bande
  originale, ambiance de jeu, podcast).
- Chaque élément porte : **fiche de provenance**, **licence**, tempo et tonalité
  détectés (`AudioAnalyzer`), étiquettes.
- **Recherche** par mot, par étiquette, par tempo / tonalité **compatibles** avec le
  projet, par ressemblance (§11).
- **Aperçu** synchronisé au projet (au tempo, dans la tonalité).
- Rangement **par projet** et **partagé** par la famille (un bruitage de NkAnima est
  trouvable ici, et inversement).

---

## 11. L'IA — un assistant complet, local

Rodolf : **assistant complet** — modèles **maison**, **hors ligne**, **l'humain
retouche**. Les règles de la famille s'appliquent : **aperçu avant d'appliquer**, **une
proposition = une opération annulable**, la **liste des changements** décochable, le
**pourquoi**, un badge **« proposé par l'IA »** jusqu'à la première retouche, **local ou
distant toujours visible**.

### 11.1 Composer avec l'IA — en notes

| demande | ce qui sort |
|---|---|
| « un motif bikutsi à 150, 12/8, balafon et nkul » | un **pattern** : des notes et des pas, dans les canaux choisis |
| « continue cette mélodie sur 8 mesures » | des **notes**, en couleur `@origine.ia`, à la suite |
| « harmonise », « une contre-mélodie », « une ligne de basse » | des notes sur un **nouveau canal** |
| « trois variantes de ce refrain » | trois **patterns** proposés côte à côte, à écouter |
| « arrange en couplet-refrain-pont » | une **playlist** proposée |
| « rends-le plus tendu / plus calme » | des modifications **listées** (tempo, notes, instruments) |

### 11.2 Comprendre l'audio

- **Transcription** : un enregistrement (une mélodie chantée, un balafon joué) → des
  **notes** dans le rouleau de piano — y compris dans un **accordage non tempéré**.
- **Séparation de sources** : un mélange → voix, percussions, basse, reste — en
  **pistes** séparées, éditables.
- **Détection** : tempo, signature, tonalité, accords, sections.

### 11.3 Mixer avec l'IA

**Suggestions** d'égalisation, de compression, de niveaux, de panoramique, de sonie —
proposées comme des **réglages** des effets existants, avec le pourquoi (« la basse et
le nkul se masquent vers 120 Hz »). **Jamais** un « mix automatique » opaque.

### 11.4 Générer du son — l'exception, écrite comme telle

Pour le **design sonore** seulement : « un souffle de dragon, grave, 2 s » → un
**échantillon** (audio). C'est la seule sortie non symbolique ; elle arrive comme un
**clip éditable** (couches si possible), marqué `@origine.ia`, avec les **réglages** qui
l'ont produite pour pouvoir le **refaire en variante**.

### 11.5 Les modèles et leur corpus

- **Modèles maison** : génération symbolique (notes) d'abord — plus petit, plus
  contrôlable, entraînable sur nos données ; transcription et séparation ensuite ;
  génération audio en dernier.
- **Le corpus est notre bibliothèque** (§6) : enregistrements **avec consentement et
  contrat** des musiciens, fiches de provenance, droits d'entraînement **explicites**.
  *Une musique d'un tiers n'entre pas dans le corpus sans ses droits*, même si elle est
  en ligne.
- **Attribution** : un motif proposé par l'IA **ne reproduit** pas un morceau existant
  reconnaissable ; un contrôle de ressemblance avec le corpus le signale.
- **Matériel** : inférence sur le processeur au minimum (petits modèles), accélérée par
  le GPU quand il est là ; **l'application marche sans IA**.

---

## 12. Les fichiers

### 12.1 Le projet

Un projet NkNgoma est **un dossier** : le fichier de projet (texte, lisible, versionnable
— canaux, patterns, notes, playlist, mixeur, automation, états de jeu), les **audios**
enregistrés ou importés à côté, les **rendus** (figés) en cache. Un projet **se déplace**
(chemins relatifs) et **s'archive** en un seul fichier à la demande. **Décision D7** : le
format de projet (proposé : texte à sections, dans l'esprit du `.nkgui`, pour que la
famille lise ce qu'elle a besoin de lire).

### 12.2 Les exports

| export | format | pour | état |
|---|---|---|---|
| audio | **WAV** 16 / 24 / 32 bits flottant, jusqu'à 192 kHz | tout | `NkWavWriter`, `NkAudioExport` ✅ |
| audio compressé | **FLAC**, **OGG Vorbis**, **Opus**, MP3 | diffusion, jeu | ✚ — **les encodeurs n'existent pas** (NKAudio ne fait que décoder) ; FLAC d'abord (sans perte, le plus simple) ; **partagé avec NkAntenne**, qui en a besoin pour son enregistrement |
| stems | un WAV par stem, même longueur, même début | NKScena, mixage externe | ✚ |
| multicanal | WAV 5.1 / 7.1 (ordre de canaux déclaré), binaural | film | ✚ |
| **musique interactive** | fichier de règles + segments audio | **Nogee / Noge** | ✚ (§9.3) |
| **banque de sons de jeu** | variations + règles de tirage | Nogee | ✚ |
| MIDI | **SMF** type 0 / 1 | échange | ✚ |
| **jingle / fond sonore** | WAV + fiche | **NkAntenne** (« Envoyer vers NkAntenne ») | ✚ |
| rapport de sonie | texte / PDF | diffusion, cinéma | ✚ |

### 12.3 La session son — l'aller-retour avec NKScena et NkAnima

Une **session son** est un fichier qui dit : la **vidéo de référence**, le **timecode**
de départ, les **pistes** (fichiers audio et leur place), les **repères** (coupes,
événements d'animation). NkNgoma **l'importe** et **l'exporte**.

- **Ce qui est défini ici** : la lecture et l'écriture par NkNgoma.
- **Ce qui ne l'est pas** : l'écriture par NKScena et NkAnima — **à discuter** (§15),
  sans modifier leurs documents aujourd'hui. En attendant : vidéo + WAV + liste de
  repères en texte (une ligne par repère : timecode, nom) — ce que tout le monde sait
  écrire.
- **Si l'animation change** (un pas déplacé de 3 images), la nouvelle session **met à
  jour** les repères ; les bruitages **calés sur un repère** suivent, les autres ne
  bougent pas — et la liste de ce qui a bougé est montrée.

---

## 13. Droits, consentement, vie privée

- **Chaque échantillon a une fiche de provenance et une licence** ; l'export d'un
  projet liste les éléments tiers et leurs licences (**générateur de crédits**).
- **Les musiciens enregistrés signent** : usage dans la bibliothèque, dans les
  productions, **dans l'entraînement de l'IA** — trois cases distinctes.
- **Voix** : aucune voix de personne réelle n'est **imitée** par l'IA sans son
  consentement écrit.
- **Hors ligne** : aucun projet, enregistrement ou modèle ne quitte la machine sans un
  geste.

---

## 14. Phasage

| palier | contenu | critère de fin |
|---|---|---|
| **P0 — condition de naissance** | dans **NKAudio** : graphe de traitement, événements à l'échantillon, latence ≤ 10 ms en mode exclusif, rendu hors ligne **déterministe** ; dans NkNgoma : un synthé soustractif, un rouleau de piano, lecture, export WAV | une mélodie de 8 mesures jouée au clavier de l'ordinateur, entendue sans décalage mesurable (≤ 10 ms, **mesuré** par le test de boucle), exportée deux fois en WAV **identiques à l'octet** |
| **P1 — composer** | rack de canaux, séquenceur à pas (mesures composées), patterns, playlist (timeline de la famille), mixeur de base, échantillonneur, boîte à rythmes, MIDI (périphériques + SMF) | un morceau en 12/8 de 3 minutes, 12 canaux, composé au séquenceur et au clavier MIDI, exporté |
| **P2 — l'Afrique** | premières banques (balafon, nkul, ngoma, tam-tam, sanza, mvet) **avec fiches**, accordages par instrument, gabarits de groove et motifs (bikutsi, makossa, assiko) | un bikutsi joué avec un balafon **dans son accordage mesuré**, et la grille du rouleau qui suit ses lames |
| **P3 — enregistrer et mixer** | enregistrement audio (prises, compensation mesurée), effets manquants, automation complète, groupes, sonie ; synthés FM et table d'ondes ; encodeur FLAC | une voix enregistrée sur une instrumentale, mixée, masterisée à −14 LUFS mesurés |
| **P4 — son à l'image et film** | vidéo de référence, timecode, repères importés, bibliothèque de bruitages (UCS), design sonore (couches, synthèse procédurale, variations), ADR ; stems D/M/E, 5.1, binaural, EBU R128 | le son complet d'une séquence NkAnima de 30 s : bruitages calés sur ses événements, musique, mix 5.1 **et** stéréo, rapport de sonie conforme R128 |
| **P5 — le jeu** | espace Jeu, états, couches, transitions, stingers, simulation, export vers Nogee ; lecture par **Noge** | une musique à trois états qui passe d'Exploration à Combat **à la mesure** dans NkNgoma **et** dans une scène Nogee, à l'identique |
| **P6 — l'IA** | composition symbolique, transcription, séparation, suggestions de mix ; puis génération de bruitages | « un motif makossa pour ce couplet » → un pattern éditable, en aperçu, en une opération annulable, **hors ligne** |

📌 **P2 commence en même temps que P0 côté studio** : enregistrer les instruments prend
des mois et ne dépend d'aucune ligne de code.

---

## 15. Ce qu'on proposera ailleurs — **à discuter, rien n'est modifié**

| où | proposition | pourquoi |
|---|---|---|
| **NKScena** | un geste « Exporter la session son » et « Réimporter les stems » | l'aller-retour du mixage de film (§2.2, §12.3) sans passer par des fichiers à la main |
| **NkAnima** | exporter les **événements** d'une animation (pas, impacts) comme repères | caler les bruitages et les suivre quand l'animation change |
| **Nogee / Noge** | lire le **fichier de musique interactive** ; relier ses paramètres à des variables de jeu | la musique adaptative jouée par le jeu |
| **NkAntenne** | partager les **encodeurs audio** et le composant `TrancheMixeur` | les deux en ont besoin |
| **NKAudio** | graphe, événements à l'échantillon, faible latence, MIDI, étirement temporel | le moteur de tout ce qui précède, et de Noge |

---

## 16. Règles transverses

1. **Le moteur est dans NKAudio**, l'application n'en est que l'interface.
2. **Ce que l'on entend est ce que l'on exporte** : le rendu hors ligne et la lecture
   passent par le même graphe ; deux rendus sont identiques à l'octet.
3. **La latence et la sonie se mesurent**, elles s'affichent en chiffres mesurés.
4. **Un échantillon sans provenance n'entre pas** dans la bibliothèque.
5. **L'accordage d'un instrument est une donnée**, jamais corrigée en silence.
6. **L'IA écrit des notes et des réglages** ; le son généré est l'exception, marquée.
7. **Rien n'est destructif** : figer se défige, une prise non choisie se garde.
8. **La famille par fichiers** : NKScena, NkAnima, Nogee, NkAntenne reçoivent des
   fichiers, jamais un appel direct.
9. **Thème de la famille** (Rihen UE5), **actions nommées**, **interface en `.nkgui`**
   (docs 02 et 03).

---

## 17. Décisions

| # | question | proposition | état |
|---|---|---|---|
| D1 | le nom | **NkNgoma** | ✅ Rodolf, 27/09 |
| D2 | la portée | musique + design sonore + mixage de film + musique de jeu | ✅ Rodolf, 27/09 |
| D3 | l'IA | assistant complet, local, symbolique d'abord | ✅ Rodolf, 27/09 |
| D4 | ASIO | accord propriétaire Steinberg (gratuit) ; en attendant, WASAPI exclusif | 🔴 |
| D5 | les greffons d'instruments et d'effets tiers | nos **greffons** d'abord ; **CLAP** (licence MIT) et **VST 3** (MIT depuis le 30/10/2025) comme **hôte** ensuite — à écrire sans STL, en réimplémentant les interfaces | 🔴 |
| D6 | les banques orchestrales | CC0 ou redistribution vérifiée, puis enregistrements maison | 🔴 |
| D7 | le format de projet | un dossier ; un fichier texte à sections | 🔴 |
| D8 | le partage avec NKScena | §2.2 : mix final ici, son de montage là-bas, aller-retour par la session son | 🔴 proposé, NKScena **non modifié** |
| D9 | le corpus de l'IA | nos enregistrements, droits d'entraînement explicites ; aucun tiers sans droits | 🔴 |
| D10 | Atmos / objets | plus tard, par ADM BWF (format ouvert) | 🔴 |
