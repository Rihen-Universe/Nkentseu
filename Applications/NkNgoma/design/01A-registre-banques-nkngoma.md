# NkNgoma — registre des banques gratuites

### Annexe A du document 1 (spécification produit)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 28/09/2026.
> Origine : Rodolf, le 28/09 — *« on devait télécharger certaines banques gratuites et
> s'en servir pour créer la nôtre pour éviter les problèmes de licence ? … oui, fais-le,
> écume bien Internet pour ça. »*
>
> **Ce que c'est** : l'inventaire des sources gratuites d'instruments, de bruitages, de
> réponses impulsionnelles et de données d'entraînement, **classées selon ce qu'on a le
> droit d'en faire**. Il sert la décision **D6** (01 §6.5, §17), la bibliothèque (01 §10),
> les bruitages (01 §7.2) et le corpus de l'IA (01 §11.5).
>
> **Comment il a été fait** : environ **150 sources** examinées le 28/09/2026. Chaque
> licence a été lue **à la source** (page du projet, fichier `LICENSE` du dépôt,
> conditions d'utilisation officielles), pas sur un blog. Quand la source n'a pas pu être
> lue, la ligne dit **non vérifié** — et une ligne non vérifiée **n'entre pas** dans la
> bibliothèque.
>
> ⚠️ **Ce n'est pas un avis juridique.** Une licence peut changer : on garde celle **du
> jour du téléchargement**, avec sa copie (§8). Avant la version commerciale, une
> relecture par un juriste des sources retenues est prudente.

---

## 0. Les trois usages, et la lecture du registre

| usage | question | exemple |
|---|---|---|
| **U1 — produire** | peut-on utiliser le son dans **nos** musiques, films, jeux ? | un bruitage Sonniss dans un film de Rihen |
| **U2 — redistribuer** | peut-on **mettre le son** (tel quel ou modifié) **dans la bibliothèque de NkNgoma**, livrée à nos utilisateurs ? | un piano CC0 dans la bibliothèque |
| **U3 — entraîner** | peut-on **entraîner notre IA** dessus, pour un usage commercial ? | des notes de violon pour le modèle symbolique |

| signe | sens |
|---|---|
| ✅ | oui, sans condition |
| ✅© | oui, **avec crédit** (générateur de crédits, 01 §13) |
| ⚠️ | oui, **avec une obligation lourde** (partage à l'identique, GPL…) — lire la note |
| ❌ | non |
| ❔ | non précisé par la licence, ou **non vérifié** — traité comme **non** |

**Une source n'entre dans la bibliothèque que si U2 est ✅ ou ✅©**, et dans le corpus
de l'IA que si U3 est ✅ ou ✅©.

---

## 1. Les règles d'admission

| famille de licence | U1 | U2 | U3 | règle NkNgoma |
|---|---|---|---|---|
| **CC0**, domaine public, Unlicense, WTFPL | ✅ | ✅ | ✅ | **admis** |
| **CC-BY** (3.0, 4.0), **MIT** | ✅ | ✅© | ✅© | **admis avec crédit** ; pour l'IA, on **publie la liste** des sources d'entraînement |
| **CC-BY-SA** | ✅© | ⚠️ | ⚠️ | **à part** : une banque dérivée doit rester CC-BY-SA ; pas dans une banque fermée ; pas dans le corpus tant que la portée sur un modèle n'est pas tranchée (D-A2) |
| **GPL** sur des sons | ✅ | ⚠️ | ❔ | **à part** : livrer le texte de la licence et les fichiers sources ; jamais mélangé à du contenu fermé (D-A3) |
| « **libre de droits** » sans redistribution (la plupart des packs gratuits commerciaux) | ✅ | ❌ | ❌ ou ❔ | **production seulement** (§4) : dans nos films et nos jeux, **jamais** dans la bibliothèque |
| **NC** (non commercial), **ND**, « recherche seulement », « usage personnel » | ❌ | ❌ | ❌ | **exclu** |
| **aucune licence écrite** | ❌ | ❌ | ❌ | **exclu** (tous droits réservés) |
| **CC Sampling Plus 1.0** (licence retirée) | ✅ transformé | ❌ | ❔ | **exclu** de la bibliothèque |

### 1.1 Freesound — la source la plus riche, et la plus piégeuse

- **La licence est son par son** (CC0, CC-BY, CC-BY-NC, et l'ancienne Sampling+) : un
  « pack » n'a pas une licence unique. On **filtre par l'API** :
  `filter=license:"Creative Commons 0"` puis `license:"Attribution"` ; on **écarte**
  « Attribution NonCommercial » et « Sampling+ ».
- **L'IA** : la FAQ de Freesound dit que l'entraînement est permis « tant que les
  licences Creative Commons de chaque son sont respectées » ; pour les sons CC-BY, elle
  exige de **publier la liste du jeu d'entraînement**.
- **Nouveau depuis juillet 2026** : chaque auteur peut déclarer une **préférence
  d'IA générative** — *aucune préférence*, *modèles open source seulement*, *modèles
  open source non commerciaux seulement*, *pas d'IA générative* (et une exclusion
  propre aux voix). **NkNgoma la respecte** : seuls les sons « aucune préférence »
  entrent dans le corpus d'un modèle commercial. On l'enregistre dans la fiche.

---

## 2. Le socle — ce qu'on télécharge en premier

| domaine | sources | licence | pourquoi |
|---|---|---|---|
| **orchestre** | **VSCO 2 CE** · **VCSL** · **University of Iowa MIS** | CC0 · CC0 · « sans restriction » | cordes, cuivres, bois, percussions, harpe, clavecin, orgue — sans crédit, utilisables pour l'IA |
| **pianos** | **Salamander Grand V3** (ou Accurate-Salamander) · **Headroom Piano** · **Splendid Grand** · **Osiris** · **Upright KW** | CC-BY 3.0 · CC-BY 4.0 · domaine public · CC0 · CC0 | un piano phare avec crédit, des pianos sans crédit en repli |
| **batteries acoustiques** | **Virtuosity Drums** · kits **Karoryfer** (Big Rusty, Swirly, Unruly, Frankensnare) · **Naked Drums** · **DRSKit**, **MuldjordKit** | CC0 · CC0 · CC-BY 4.0 · CC-BY 4.0 | jazz, rock, variété |
| **guitares, basses, cordes, sax** | **Karoryfer** (guitares, basses, violoncelle, String Cyborgs, Weresax) · **Greg Sullivan E-Pianos** | CC0 · CC-BY 3.0 | le rack « variété » |
| **banque General MIDI** | **MuseScore_General** / **FluidR3_GM** | MIT (garder la notice) | lire n'importe quel fichier MIDI dès le premier jour |
| **Afrique** (le peu qui existe) | **VCSL** (kalimbas, mbiras, balafon, slit drum…) · **Karoryfer Gogodze Phu Vol I** (tambours bobobo du Ghana) · **balafon de Bobo-Dioulasso** (Musical Artifacts #420) · packs **Freesound CC0** de djembé | CC0 · CC0 · WTFPL · CC0 | un départ — le vrai fonds africain sera **enregistré par nous** (§6) |
| **bruitages** | **Kenney** · **The Free Firearm Sound Library** · **Freesound CC0** · **NPS Sound Gallery** · **NASA** | CC0 · CC0 · CC0 · domaine public · domaine public | un fonds de départ livrable tel quel |
| **réponses impulsionnelles** | **OpenAIR** (espaces CC-BY) · **BUT ReverbDB** · **Aachen AIR** | CC-BY 4.0 · CC-BY 4.0 · MIT | salles, églises, pièces ; binaural |
| **motifs de batterie** | **Groove MIDI Dataset** · **Expanded Groove (E-GMD)** | CC-BY 4.0 | des grooves joués par des batteurs, en MIDI, livrables et entraînables |
| **corpus IA — notes** | **PDMX** (sous-ensemble `no_license_conflict`) · **GiantMIDI-Piano** · Groove / E-GMD | CC-BY 4.0 | le modèle symbolique (01 §11.1) |
| **corpus IA — audio** | **NSynth** · **MusicNet** · **FSD50K** (clips CC0 / CC-BY) · Freesound CC0 | CC-BY 4.0 · CC-BY 4.0 · par clip · CC0 | timbres, transcription, reconnaissance de sons |

---

## 3. Le registre complet

### 3.1 Instruments acoustiques — orchestre, claviers, cordes, vents

| nom | contenu | licence (telle qu'écrite) | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| VSCO 2 Community Edition | orchestre de chambre, ~3 Go, WAV + SFZ | CC0 1.0 (« do whatever you want ») | ✅ | ✅ | ✅ | versilian-studios.com/vsco-community · github.com/sgossner/VSCO-2-CE | **socle** |
| VCSL (Versilian Community Sample Library) | très large : pianos, clavecin, orgues, claviers à lames, percussions, cordes, vents, idiophones du monde | CC0 1.0 (« even make commercial software ») | ✅ | ✅ | ✅ | github.com/sgossner/VCSL | **socle** ; classé selon Hornbostel-Sachs |
| University of Iowa MIS | notes isolées, plusieurs nuances : bois, cuivres, cordes, piano, guitare, percussions | « may be downloaded and used for any projects, without restrictions » | ✅ | ✅ | ✅ | theremin.music.uiowa.edu/MIS.html | déclaration permissive, pas une licence formelle ; la cartographie est à faire |
| bigcat instruments (la plupart) | pianos, clavecin, orgue, clavicordes, cordes, vents, cuivres, percussions | CC0 (échantillons Versilian, instruments bigcat) | ✅ | ✅ | ✅ | bigcatinstruments.blogspot.com | vérifier **article par article** |
| bigcat — Mihai Sorohan Choir | chœur | licence propre : « not for commercial sample libraries » | ✅ | ❌ | ❔ | idem | exclu de la bibliothèque |
| No Budget Orchestra (bigcat) | cordes, cuivres, bois | CC-BY-SA 4.0 (cité par VPO) | ✅© | ⚠️ | ⚠️ | bigcatinstruments.blogspot.com | **non vérifié** à la source (boucle de redirection) |
| Salamander Grand Piano V3 | Yamaha C5, 16 couches | CC-BY 3.0 | ✅© | ✅© | ✅© | github.com/sfzinstruments/SalamanderGrandPiano · archive.org | créditer **Alexander Holm** |
| Accurate-Salamander | Salamander remasterisé, 48 k / 24 bits | CC-BY (même licence que l'original) | ✅© | ✅© | ✅© | ir.isas.jaxa.jp/~cyamauch/AccurateSalamander | créditer Holm **et** Yamauchi |
| Splendid Grand Piano (Akai) | Steinway, léger | domaine public (déclaré dans le README) | ✅ | ✅ | ✅ | github.com/sfzinstruments/SplendidGrandPiano | pas de fichier LICENSE : risque faible |
| Osiris Piano | piano | CC0 1.0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/Osiris_Piano | |
| Upright Piano KW (FreePats) | piano droit | CC0 1.0 | ✅ | ✅ | ✅ | freepats.zenvoid.org | |
| YDP Grand Piano (FreePats) | piano à queue | CC-BY 3.0 | ✅© | ✅© | ✅© | freepats.zenvoid.org | |
| Headroom Piano (Bengt Nilsson) | Yamaha C3 | CC-BY 4.0 | ✅© | ✅© | ✅© | github.com/sfzinstruments/BengtNilsson.HeadroomPiano | garder le nom d'origine de l'instrument |
| Greg Sullivan E-Pianos | trois pianos électriques | CC-BY 3.0 | ✅© | ✅© | ✅© | github.com/sfzinstruments/GregSullivan.E-Pianos | comble le manque de piano électrique |
| FreePats Church Organ | orgue (enregistré depuis l'émulateur Aeolus) | CC0 1.0 | ✅ | ✅ | ✅ | freepats.zenvoid.org | orgue **synthétisé**, pas un vrai orgue |
| FreePats Concert Harp | harpe (dérivée de VCSL) | CC0 1.0 | ✅ | ✅ | ✅ | freepats.zenvoid.org | les autres sets FreePats : CC0, CC-BY ou GPL — vérifier chacun |
| Karoryfer — guitares et basses (Emilyguitar, Shinyguitar, Black & Green, Meatbass, Sneakybass, Pastabass, Big Little Bass, Black & Blue) | guitares, basses | CC0 1.0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/karoryfer.* · shop.karoryfer.com/pages/free-samples | seulement les titres **gratuits** (les payants sont exclus) |
| Karoryfer — cordes (Bigcat Cello, String Cyborgs, D. Smolken Double Bass) | violoncelle, ensemble, contrebasse | CC0 1.0 | ✅ | ✅ | ✅ | idem | contrebasse : CC0 d'après l'index seulement |
| Karoryfer Weresax | saxophone | CC0 1.0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/karoryfer.weresax | |
| jlearman SteelDrum | steel pan | Unlicense (domaine public) | ✅ | ✅ | ✅ | github.com/sfzinstruments/jlearman.SteelDrum | |
| aliexpress-erhu | erhu | CC0 1.0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/aliexpress-erhu | |
| tonejs-instruments | 19 instruments (piano, cordes, vents, cuivres, guitares, harpe, orgue, harmonium) | échantillons CC-BY 3.0 ; code MIT | ✅© | ✅© | ✅© | github.com/nbrosowsky/tonejs-instruments | qualité modeste ; provenance « domaine public » listée |
| Sonatina Symphonic Orchestra | orchestre complet | CC Sampling Plus 1.0 | ✅ transformé | ❌ | ❔ | github.com/peastman/sso | licence retirée ; sources en partie d'origine inconnue → **exclu** |
| Virtual Playing Orchestra | orchestre assemblé (SSO, VSCO2, Iowa, Freesound…) | licences **mélangées** | ✅ | ❌ | ❔ | virtualplaying.com | **exclu** : prendre ses sources CC0 directement |
| Philharmonia Orchestra samples | notes isolées d'orchestre | licence propre : pas « as is… as samples or as a sampler instrument » | ✅ | ❌ | ❔ | philharmonia.co.uk/resources/sound-samples | production seulement |
| Maestro Concert Grand (Mats Helgesson) | Yamaha CF-3 | « publié avec la permission de l'auteur », sans licence | ❔ | ❌ | ❔ | github.com/sfzinstruments/MatsHelgesson.MaestroConcertGrandPiano | **exclu** |
| jRhodes3d / jRhodes3c | Rhodes | CC BY-NC / CC BY-NC-SA 4.0 | ✅ | ❌ | ❌ | github.com/sfzinstruments/jlearman.* | NC ; l'auteur propose une licence commerciale sur demande |
| Spitfire LABS (devenu Splice INSTRUMENT) | nombreux instruments | conditions Splice / EULA Spitfire | ✅ | ❌ | ❌ | splice.com/terms · spitfireaudio.com (EULA) | interdit **explicitement** l'entraînement d'IA |
| Pianobook | milliers d'instruments de la communauté | EULA Pianobook | ✅ transformé | ❌ | ❔ | pianobook.co.uk/terms-conditions | production seulement |
| Musical Artifacts | agrégateur SF2 / SFZ | par article | ❔ | ❔ | ❔ | musical-artifacts.com | **non vérifié** (403) : lire chaque page |
| Ivy Audio (Piano in 162…) | pianos | licence non affichée | ✅ probable | ❔ | ❔ | ivyaudio.com | **non vérifié** — exclu jusqu'à lecture du fichier de licence |

### 3.2 Batteries acoustiques

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| Virtuosity Drums | kit jazz, 6 micros | CC0 1.0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/virtuosity_drums | **socle** |
| Karoryfer — Big Rusty, Swirly, Unruly, Frankensnare | kits, caisses claires | CC0 1.0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/karoryfer.* | Swirly, Unruly, Frankensnare : CC0 d'après l'index |
| Naked Drums (Wilkinson Audio) | kit multi-micros, 10 alternances | CC-BY 4.0 | ✅© | ✅© | ✅© | github.com/sfzinstruments/WilkinsonAudio.NakedDrums | |
| DRSKit, MuldjordKit (DrumGizmo) | kits rock | CC-BY 4.0 | ✅© | ✅© | ✅© | github.com/sfzinstruments/DrumGizmo.* | créditer Lars Muldjord et al. |
| CrocellKit (DrumGizmo) | kit métal, 15 micros, ~5,5 Go | CC-BY 4.0 | ✅© | ✅© | ✅© | drumgizmo.org | à convertir en SFZ |
| Salamander Drumkit | kit acoustique | CC-BY-SA 3.0 | ✅© | ⚠️ | ⚠️ | archive.org/details/SalamanderDrumkit | partage à l'identique |
| Sam's Sonor | kit Sonor | CC-BY-SA 4.0 | ✅© | ⚠️ | ⚠️ | github.com/sfzinstruments/SamsSonor | partage à l'identique |
| SM Drums | kit détaillé, 2,2 Go | « Public Domain » (index seulement) | ✅ | ❔ | ❔ | smmdrums.wordpress.com | **non vérifié** : aucune licence chez l'auteur |
| Hydrogen GMRockKit | kit GM | GPL (dans `drumkit.xml`) | ✅ | ⚠️ | ❔ | github.com/hydrogen-music/hydrogen | GPL sur de l'audio : tenir à part |

### 3.3 Banques General MIDI (SoundFonts)

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| MuseScore_General / FluidR3_GM | banque GM complète | MIT (Frank Wen ; Cowgill ; Collins) | ✅ | ✅© | ✅© | ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General · people.debian.org (copyright FluidR3) | **socle GM** ; garder la notice MIT |
| GeneralUser GS v2 | GM/GS, 259 instruments | licence propre v2.0 : usage « without restriction… in your software projects » | ✅ | ✅ | ❔ | github.com/mrbumpy409/GeneralUser-GS | l'auteur n'est « pas sûr à 100 % » de l'origine des échantillons : **risque de provenance** |
| FreePats GM | banques GM | GPL-3+ avec exception pour les compositions | ✅ | ⚠️ | ❔ | freepats.zenvoid.org/licenses.html | obligations GPL |
| Arachno SoundFont | GM | « All rights reserved… reproduction forbidden » | ✅ probable | ❌ | ❌ | arachnosoft.com | **exclu** |
| Timbres of Heaven | GM/GS/XG | « personal use, don't share » (source secondaire) | ❔ | ❌ | ❌ | midkar.com | **non vérifié** ; exclu |

### 3.4 Électronique, boîtes à rythmes, boucles, préréglages

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| Freesound (sous-ensemble CC0 / CC-BY) | coups de 808/909, boucles, one-shots | par son | ✅ / ✅© | ✅ / ✅© | ✅ / ✅© | freesound.org/help/faq | filtre de licence **et** préférence IA (§1.1) |
| VCSL (synthé TX81Z…) | échantillons de synthé | CC0 | ✅ | ✅ | ✅ | github.com/sgossner/VCSL | pas de clones de 808/909 |
| 99Sounds | packs gratuits | licence propre | ✅ | ❌ (« even for free ») | ❔ | 99sounds.org/license | interdit aussi les applis de bruit / sommeil |
| Looperman | boucles d'utilisateurs | conditions Looperman | ✅ | ❌ | ❌ | looperman.com/help/terms | interdit l'entraînement d'IA |
| MusicRadar SampleRadar | kits 808, packs par genre | « royalty-free… don't re-distribute » | ✅ | ❌ | ❔ | musicradar.com (SampleRadar) | production seulement |
| Samples From Mars (gratuits) | 808/909, machines matérielles | « may not re-distribute… in any way » | ✅ | ❌ | ❌ | samplesfrommars.com/pages/terms-conditions | la FAQ exclut l'usage dans une appli |
| Goldbaby (gratuits) | machines analogiques | conditions Goldbaby | ✅ | ❌ | ❌ | goldbaby.co.nz | interdit **explicitement** l'IA |
| Legowelt | synthés, machines | « free to… use in your productions » | ✅ | ❔ | ❔ | legowelt.org/samples | pas de droit de redistribution → non |
| Ableton (packs gratuits) | échantillons, préréglages | EULA Ableton | ✅ transformé | ❌ | ❔ | ableton.com/en/eula | « creation of other sound packs… is prohibited » |
| Reverb Drum Machines | 50+ boîtes à rythmes | non affichée | ❔ | ❔ | ❔ | reverb.com | **non vérifié** : lire le fichier dans l'archive |
| Bedroom Producers Blog (gratuits) | 808, kicks, boucles | par pack | ❔ | ❔ | ❔ | bedroomproducersblog.com | **non vérifié** |
| TidalCycles Dirt-Samples | one-shots variés | **aucune licence** | ❔ | ❌ | ❌ | github.com/tidalcycles/Dirt-Samples | exclu |
| Hydrogen TR808EmulationKit | 808 émulée | « undefined license » | ❔ | ❔ | ❔ | github.com/hydrogen-music/hydrogen | exclu |
| Surge XT (préréglages, tables d'ondes) | préréglages | dépôt GPL-3 ; licence du contenu d'usine **en discussion** (ticket #6741 : CC-BY-SA 4.0 proposée) | ✅ | ⚠️ | ❔ | github.com/surge-synthesizer/surge | attendre la décision, ou demander aux mainteneurs |
| Vital (version gratuite) | 75 préréglages, 25 tables | EULA Vital : pas de redistribution | ✅ | ❌ | ❔ | vital.audio/eula | exclu ; le nom « Vital » non plus |

### 3.5 Afrique

| nom | instruments | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| VCSL | kalimba (Kenya, Tanzanie), mbira dzaVadzimu Nyamaropa et Mavembe/Gandanga (Zimbabwe), nyunga nyunga (Mozambique), balafon, slit drum, agogo, cabasa, darbouka, conga, tambour sur cadre | CC0 | ✅ | ✅ | ✅ | github.com/sgossner/VCSL | **la meilleure source** ; origine du balafon non documentée ; le « slit drum » n'est probablement pas un nkul ; la mbira dzaVadzimu a une importance rituelle chez les Shona — la présenter avec respect |
| Karoryfer — Gogodze Phu Vol I | cinq tambours **bobobo** du Ghana + un cajón | CC0 | ✅ | ✅ | ✅ | github.com/sfzinstruments/karoryfer.gogodze-phu-vol-i | la famille la plus proche d'un « ngoma » en accès libre ; musicien non nommé |
| Musical Artifacts #420 — petit balafon (Burkina Faso) | balafon pentatonique, do3–mi5 | WTFPL | ✅ | ✅ | ✅ | musical-artifacts.com/artifacts/420 | instrument traditionnel de la région de Bobo-Dioulasso ; enregistrement amateur |
| Freesound — Infinita08, « Djembe Drum Sample Library » | djembé (tons, bords, étouffés, roulements) | CC0 (vérifié sur un son) | ✅ | ✅ | ✅* | freesound.org/people/Infinita08/packs/26031 | *vérifier chaque son et sa préférence IA |
| Freesound — Ancient.Sounds, « Djembe Sample/Loops Pack v.2 » | boucles de djembé, 120–135 BPM | CC0 (vérifié sur un son) | ✅ | ✅ | ✅* | freesound.org/people/Ancient.Sounds/packs/27430 | joué par « Manuel.Ed » ; voir aussi le pack 27427 |
| Freesound — domingus, « Djembe Samples » | djembé | CC0 (vérifié sur un son) | ✅ | ✅ | ✅* | freesound.org/people/domingus/packs/3456 | musicien non nommé |
| Freesound — Sassaby, « Marimba / Balafon Samples » | marimba ghanéen (type gyil) | CC0 (vérifié sur un son) | ✅ | ✅ | ✅* | freesound.org/people/Sassaby/packs/29681 | peu de sons |
| Freesound — memexikon, « shekere.wav » | shekere | CC0 | ✅ | ✅ | ✅* | freesound.org/people/memexikon/sounds/641 | un seul son |
| Freesound — marini24, « balafon » | balafon | CC-BY 4.0 (vérifié sur un son) | ✅© | ✅© | ✅© | freesound.org/people/marini24/packs/6325 | minidisc, qualité modeste |
| Freesound — pjcohen, udu | udu | CC-BY 4.0 | ✅© | ✅© | ✅© | freesound.org/people/pjcohen/packs/23375 | mal étiqueté « Middle-East » : l'udu est **igbo (Nigeria)** — corriger dans la fiche |
| Freesound — pjcohen, agogo | agogo (à défaut de gankogui) | CC-BY 4.0 | ✅© | ✅© | ✅© | freesound.org/people/pjcohen/packs/23370 | étiqueté samba : ce n'est pas un gankogui |
| Freesound — iainmccurdy, « Kalimba » | kalimba diatonique en do | CC-BY 4.0 | ✅© | ✅© | ✅© | freesound.org/people/iainmccurdy/packs/38025 | accord occidental, pas traditionnel |
| Freesound — arioke, « Kalimba Samples » | kalimba 11 lames | Sampling+ | ✅ transformé | ❌ | ❔ | freesound.org/people/arioke/packs/3759 | demander à l'auteur de passer en CC0 / CC-BY |
| Kaggle — « Africa talking drum » | tambour parlant yoruba, 1 050 extraits (8 enregistrements + augmentations) | ODbL (base) / DbCL (contenu) | ✅ | ⚠️ | ✅ | kaggle.com/datasets/ogunmusireseyi/africa-talking-drum | 8 sources seulement, provenance et consentement inconnus : bon pour des essais, pas comme fonds |
| Wikimedia Commons (catégories Kora, Djembe, Balafon, Talking drum) | kora, djembé… | licences libres fichier par fichier (pas de NC sur Commons) | ✅ | ✅ / ⚠️ (BY-SA) | ✅ / ⚠️ | commons.wikimedia.org/wiki/Category:Kora | **non vérifié** fichier par fichier |
| Ableton / Santuri Safari (Afrique de l'Est) | zeze, adungu, endere, tambours ohangla | non affichée | ❔ | ❔ | ❔ | ableton.com (article Santuri Safari) | **non vérifié** ; musiciens nommés (Msafiri Zawose…) : **consentement** avant toute IA |
| Musical Artifacts #3112 (balafon), #4039 (kalimba) | balafon, kalimba | non lue (403) | ❔ | ❔ | ❔ | musical-artifacts.com | **non vérifié** |
| Pianobook (kalimba, djembés, udu, M'bira…) | kalimba, djembé, udu | EULA Pianobook | ✅ transformé | ❌ | ❔ | pianobook.co.uk | production seulement |
| Decent Samples — Gourd Kalimba/Mbira | kalimba sur calebasse | EULA : pas de redistribution | ✅ | ❌ | ❔ | decentsamples.com (EULA) | production seulement |
| Samplephonics — The Wizard of Mbira | mbira, boucles | « distribution in any form… prohibited » | ✅ | ❌ | ❔ | samplephonics.com | production seulement |
| Sample Focus | djembé, tambour parlant, kora… | pas de redistribution ; **pas d'IA** | ✅ | ❌ | ❌ | samplefocus.com/license | production seulement |
| Pixabay (sons) | tambour parlant et autres | pas de redistribution « standalone » ; **pas d'apprentissage automatique** | ✅ | ❌ | ❌ | pixabay.com/service/terms | production seulement |
| SampleSwap | tambour parlant | « no guarantee… free and clear of any copyright » | ❔ | ❔ | ❔ | sampleswap.org | provenance inconnue → **exclu** |
| Musical Artifacts #236 — HS African Percussion | percussions | « copyright » | ❌ | ❌ | ❌ | données musical-artifacts | **exclu** |
| IEMP Malian Jembe | ensembles de djembé du Mali | CC-BY-NC-ND 4.0, pas d'échantillonnage | ❌ | ❌ | ❌ | osf.io/m652x | recherche seulement |

### 3.6 Bruitages et ambiances

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| Freesound (CC0 / CC-BY) | ~600 000 sons | par son | ✅ / ✅© | ✅ / ✅© | ✅ / ✅© | freesound.org | §1.1 |
| Kenney (audio) | interface, jeu, impacts, RPG | CC0 (« even in commercial projects ») | ✅ | ✅ | ✅ | kenney.nl/support | **socle** |
| The Free Firearm Sound Library | armes, mécanismes, ambiances | CC0 (« without royalty or credit ») | ✅ | ✅ | ✅ | opengameart.org/content/the-free-firearm-sound-library | **socle** |
| NPS Sound Gallery | nature, animaux, parcs nationaux américains | domaine public (crédit demandé) | ✅ | ✅ | ✅ | nps.gov/subjects/sound/gallery.htm | |
| NASA | lancements, missions, espace | pas de droit d'auteur aux États-Unis | ✅ | ✅ | ✅ | nasa.gov (brand center) | pas de logo, pas d'aval de la NASA sous-entendu |
| OpenGameArt | sons de jeu | par fichier : CC0, CC-BY, OGA-BY, CC-BY-SA, GPL | ✅ | ✅ / ✅© / ⚠️ | ✅ / ✅© | opengameart.org/content/faq | garder CC0, CC-BY, OGA-BY |
| SoundBible | ~2 000 sons | par son (domaine public, CC-BY, autres) | ✅ | ✅ / ✅© seulement pour PD et CC-BY | ✅ / ✅© | soundbible.com/about.php | vérifier chaque page |
| Wikimedia Commons (audio) | divers | licences libres par fichier | ✅ | ✅ / ⚠️ | ✅ / ⚠️ | commons.wikimedia.org | **non vérifié** fichier par fichier |
| Library of Congress | enregistrements historiques | par article | ❔ | ❔ | ❔ | loc.gov/legal | seulement « No known restrictions » |
| Internet Archive | dépôts divers | par article, sans garantie | ❔ | ❔ | ❔ | help.archive.org/help/rights | seulement CC0 / domaine public marqués |
| xeno-canto | oiseaux, animaux | CC par enregistrement (beaucoup de BY-NC-SA) | ❔ | ❔ | ❔ | xeno-canto.org | **non vérifié** ; garder BY / BY-SA / CC0 |
| Sonniss GDC Game Audio Bundles | dizaines de Go de bruitages pro | « royalty free… commercially usable » | ✅ | ❌ (pas « as sound effects », même modifiés) | ❌ (« AI/ML training is strictly prohibited ») | sonniss.com/gdc-bundle-license | **production seulement** — excellent pour nos films et jeux |
| Pixabay (sons) | bruitages | licence Pixabay | ✅ | ❌ | ❌ | pixabay.com/service/terms | production seulement |
| Mixkit (Envato) | bruitages | licence Mixkit | ✅ | ❌ | ❔ | mixkit.co/terms | clause « produit concurrent » risquée pour une appli de son |
| ZapSplat (gratuit) | grand catalogue | licence standard, crédit « ZapSplat » | ✅© | ❌ (même dans une appli) | ❌ (explicite) | zapsplat.com/license-type/standard-license | production seulement |
| Free To Use Sounds | terrain, ambiances | crédit obligatoire | ✅© | ❌ | ❌ (licence IA à part, payante) | freetousesounds.com/license-agreement | production seulement |
| 99Sounds | packs de design sonore | licence propre | ✅ | ❌ | ❔ | 99sounds.org/license | production seulement |
| Sound Jay | bruitages | conditions Sound Jay | ✅ | ❌ | ❌ (explicite) | soundjay.com/tos.html | production seulement |
| Adobe Audition SFX | ~10 000 bruitages | EULA Adobe | ✅ | ❌ | ❔ | adobe.com | production seulement |
| Soundimage.org | bruitages, ambiances | « free to use with attribution » | ✅© | ❔ | ❔ | soundimage.org | demander à l'auteur pour U2 |
| Orange Free Sounds | bruitages, boucles | non nommée | ✅ probable | ❔ | ❔ | orangefreesounds.com | **non vérifié** ; beaucoup de pages en BY-NC |
| YouTube Audio Library | bruitages | « copyright-safe » pour YouTube | ✅ YouTube | ❔ | ❔ | support.google.com/youtube | **non vérifié** |
| BBC Sound Effects (RemArc) | ~33 000 sons d'archives | RemArc : personnel, éducatif, recherche | ❌ | ❌ | ❌ | sound-effects.bbcrewind.co.uk/licensing | **exclu** (licence payante pour le commercial) |
| Macaulay Library (Cornell) | animaux | « personal and noncommercial use » | ❌ | ❌ | ❌ | birds.cornell.edu | licence payante |

### 3.7 Réponses impulsionnelles (réverbération)

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| OpenAIR (université d'York) | salles, églises, pièces | CC **par espace** (souvent CC-BY 4.0) | ✅© | ✅© si CC-BY | ✅© si CC-BY | openair.hosted.york.ac.uk | lire la ligne de licence de **chaque** espace |
| BUT ReverbDB | pièces réelles, parole retransmise | CC-BY 4.0 | ✅© | ✅© | ✅© | speech.fit.vut.cz/software/but-speech-fit-reverb-database | |
| Aachen AIR | réponses binaurales de pièces | MIT | ✅ | ✅© (notice) | ✅ | iks.rwth-aachen.de | orienté parole et binaural |
| EchoThief | ~115 lieux (grottes, escaliers…) | le résultat convolué est libre ; la redistribution n'est pas accordée | ✅ | ❌ | ❔ | licence PDF livrée avec le téléchargement | production seulement |
| Voxengo IR | salles, plaques | « royalty-free… » mais pas de profit sur leur diffusion | ✅ | ❌ | ❔ | voxengo.com/impulses | production seulement |
| Samplicity Bricasti M7 | réverbe matérielle Bricasti | « free of charge and as-is » | ✅ probable | ❔ | ❔ | samplicity.com | production seulement |
| MIT IR Survey | 271 lieux du quotidien | aucune licence affichée | ❔ | ❔ | ❔ | mcdermottlab.mit.edu | **non vérifié** |
| Fokke van Saane | réverbes classiques | site hors ligne | ❔ | ❔ | ❔ | — | **non vérifié** ; exclu |

### 3.8 Jeux de données pour l'IA

**Symbolique (notes, MIDI)**

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| Groove MIDI Dataset | batterie jouée, MIDI + audio | CC-BY 4.0 | ✅© | ✅© | ✅© | magenta.tensorflow.org/datasets/groove | **socle** des motifs de batterie |
| Expanded Groove (E-GMD) | batterie MIDI + audio rendu | CC-BY 4.0 | ✅© | ✅© | ✅© | magenta.tensorflow.org/datasets/e-gmd | créditer Google |
| PDMX | ~250 000 partitions MuseScore du domaine public ou CC | CC-BY 4.0 | ✅© | ✅© | ✅© | zenodo.org/records/14648209 | 12,29 % de licences incohérentes : n'utiliser que `no_license_conflict` |
| GiantMIDI-Piano | piano classique transcrit | CC-BY 4.0 | ✅© | ✅© | ✅© | github.com/bytedance/GiantMIDI-Piano | transcrit depuis YouTube |
| Lakh MIDI / Slakh2100 | ~176 000 MIDI ; multipistes rendues | CC-BY 4.0 (la **compilation**) | ⚠️ | ⚠️ | ⚠️ | colinraffel.com/projects/lmd · slakh.com | beaucoup de **reprises de chansons protégées** : la licence ne couvre pas les œuvres — relecture juridique d'abord |
| POP909 | arrangements de pop chinoise | MIT (le dépôt) | ⚠️ | ⚠️ | ⚠️ | github.com/music-x-lab/POP909-Dataset | œuvres protégées : même réserve |
| JSB Chorales / Nottingham | chorals de Bach ; airs populaires | aucune licence | ❔ | ❔ | ❔ | github.com/czhuang/JSB-Chorales-dataset · abc.sourceforge.net/NMD | refaire depuis une édition du domaine public (corpus music21) |
| MAESTRO | piano, MIDI + audio | CC BY-NC-SA 4.0 | ❌ | ❌ | ❌ | magenta.tensorflow.org/datasets/maestro | recherche seulement |
| MetaMIDI | 436 000 MIDI | accès « for research purposes » | ❌ | ❌ | ❌ | zenodo.org/records/5142664 | exclu |

**Audio**

| nom | contenu | licence | U1 | U2 | U3 | source | note |
|---|---|---|---|---|---|---|---|
| NSynth | 305 979 notes de 1 006 instruments | CC-BY 4.0 | ✅© | ✅© | ✅© | magenta.tensorflow.org/datasets/nsynth | 16 kHz, 4 s : pour l'IA, pas comme instrument |
| MusicNet | 330 enregistrements classiques + notes | CC-BY 4.0 | ✅© | ✅© | ✅© | zenodo.org/records/5120004 | transcription |
| FSD50K | 51 000 extraits Freesound, 200 classes | CC-BY (jeu) ; par extrait CC0 / BY / BY-NC / Sampling+ | ✅ / ✅© | ✅ / ✅© | ✅ / ✅© | zenodo.org/records/4060432 | écarter ~7 800 extraits NC / Sampling+ |
| ESC-10 | 400 extraits d'environnement | CC-BY 3.0 | ✅© | ✅© | ✅© | github.com/karolpiczak/ESC-50 | ESC-50 (le reste) est **NC** |
| FMA (Free Music Archive) | 106 000 morceaux | par morceau (choix de l'artiste) | ❔ | ❔ | ✅ seulement CC0 / CC-BY | github.com/mdeff/fma | filtrer ; beaucoup de NC / ND |
| Clotho | ~5 000 extraits + légendes | audio : licences Freesound ; **légendes : non commerciales** | par extrait | par extrait | audio par extrait ; légendes ❌ | zenodo.org/records/4783391 | pas pour un modèle texte → son commercial |
| AudioSet, VGGSound | étiquettes de vidéos YouTube | CC-BY 4.0 (étiquettes seules) | ❌ | ❌ | étiquettes seulement | research.google.com/audioset | aucun audio licencié |
| MTG-Jamendo, MUSDB18, MedleyDB, UrbanSound8K, WavCaps, TAU/DCASE, IEMP | musique, multipistes, sons urbains | non commercial / recherche seulement | ❌ | ❌ | ❌ | (dépôts respectifs) | **exclus** |

---

## 4. Production seulement — jamais dans la bibliothèque, jamais dans le corpus

Excellents pour **nos films, nos jeux, nos musiques** (U1), mais leur licence interdit la
redistribution, et souvent l'IA :

**Sonniss GDC** · ZapSplat · Pixabay · Mixkit · Free To Use Sounds · 99Sounds · Sound
Jay · Adobe Audition SFX · Philharmonia · Pianobook · Decent Samples · Samplephonics ·
Sample Focus · Spitfire LABS / Splice · MusicRadar SampleRadar · Samples From Mars ·
Goldbaby · Ableton · Looperman · Legowelt · EchoThief · Voxengo · Samplicity.

📌 Dans NkNgoma, ces sources peuvent être **importées par l'utilisateur** dans **sa**
bibliothèque personnelle : c'est **sa** licence, pas la nôtre. La fiche de provenance
porte alors la mention « **production seulement — ne pas partager** », et l'application
**refuse** de les inclure dans un export de banque ou dans un corpus d'entraînement.

---

## 5. Exclus

| source | raison |
|---|---|
| BBC Sound Effects (RemArc) · Macaulay Library | usage personnel ou non commercial |
| MAESTRO · MTG-Jamendo · MUSDB18 · MedleyDB · MetaMIDI · UrbanSound8K · WavCaps · TAU/DCASE · IEMP · ESC-50 | non commercial ou recherche seulement |
| jRhodes3d / 3c | NC (licence commerciale possible sur demande) |
| Sonatina · Virtual Playing Orchestra | Sampling+ retirée, licences mêlées, provenance incertaine |
| Arachno · Timbres of Heaven · Maestro Concert Grand | pas de droit de redistribution |
| TidalCycles Dirt-Samples · Hydrogen TR808 · SampleSwap · HS African Percussion | pas de licence, ou provenance inconnue |
| Vital (contenu d'usine) | EULA : pas de redistribution |

---

## 6. Les manques — ce qu'on devra enregistrer nous-mêmes

Aucune source libre et utilisable n'existe pour :

- **mvet**, **nkul** (le « slit drum » de VCSL n'en est pas un), **ngoma** et tambours
  camerounais (seuls les bobobo ghanéens sont libres) ;
- **kora**, **ngoni**, **arc musical**, **flûtes peules**, **trompes et cornes** ;
- **sanza / likembe** d'Afrique centrale (VCSL n'a que des lamellophones d'Afrique de
  l'Est et australe) ;
- les **grooves** makossa, bikutsi, assiko, highlife, afrobeat, avec droits de
  redistribution **et** d'entraînement.

**Couverture maigre** : tambour parlant (8 sources de provenance inconnue), gankogui
(un agogo de samba à la place), shekere (un seul son), udu (un pack CC-BY).
**Suffisant pour démarrer** : djembé, balafon (burkinabè et ghanéen, pas camerounais),
mbira et kalimba d'Afrique de l'Est et australe.

➜ C'est exactement le **chantier de studio** du 01 §6.1 : il ne dépend d'aucune ligne de
code et peut commencer **maintenant**. Chaque musicien signe les trois autorisations du
01 §13 (bibliothèque, productions, entraînement de l'IA) — **nommé, rémunéré,
crédité**.

---

## 7. Qui contacter

| institution | ce qu'elle peut apporter |
|---|---|
| **CERDOTOLA** (Yaoundé) — Laboratoire des ressources orales, archive ALORA | le partenaire naturel pour le **mvet** et les traditions épiques ; ⚠️ utiliser le **domaine principal** cerdotola.org : un sous-domaine sert actuellement un site de jeux d'argent |
| **Ministère des Arts et de la Culture** ; **Musée des civilisations de Dschang** | musiciens, facteurs d'instruments, lieux d'enregistrement |
| **SONACAM** (société de gestion collective) | droits des œuvres et des interprètes camerounais nommés |
| **ILAM** — International Library of African Music (Rhodes University, Afrique du Sud) | archives Hugh Tracey ; accueille des propositions de projets éducatifs ; a sa propre politique d'IA ; pratique le « retour numérique » aux communautés |
| **Smithsonian Folkways** (service des licences) | licence payante d'enregistrements africains |
| **CREM-CNRS** / Musée de l'Homme | fonds camerounais importants (écoute seulement aujourd'hui) ; demandes de réutilisation |
| **BnF** (fonds Charles Duvelle, Ocora) | réutilisation commerciale **payante** |
| **British Library / Endangered Archives Programme** | renvoie vers chaque porteur de projet |
| **Auteurs Freesound** (ex. arioke pour la kalimba) | demander directement un passage en CC0 ou CC-BY |

📌 **Les archives ethnographiques ne sont pas des banques** : même payées, elles
contiennent des musiciens et des communautés nommés. On ne les met dans le corpus de
l'IA qu'avec un **accord qui leur revient** (crédit, partage des bénéfices), et jamais
d'une façon qui les dénature (conditions de l'EAP, pratique de l'ILAM).

---

## 8. La procédure d'import — ce que fait NkNgoma

1. **Télécharger depuis la source primaire** (jamais un miroir), noter l'URL, la date,
   la version.
2. **Copier le texte de la licence** dans le dossier de la banque (`LICENSE.txt`) et
   calculer une **empreinte** de l'archive téléchargée : c'est la preuve de la licence
   **du jour**.
3. **Remplir la fiche de provenance** (01 §6.1, §10) :

   | champ | exemple |
   |---|---|
   | source, URL, date, version | VCSL · github.com/sgossner/VCSL · 28/09/2026 · commit … |
   | licence exacte | CC0 1.0 |
   | U1 / U2 / U3 | ✅ / ✅ / ✅ |
   | crédit à afficher | — (ou « Alexander Holm — CC-BY 3.0 ») |
   | préférence IA (Freesound) | aucune / open source / non commercial / pas d'IA |
   | instrument, région, musicien, accordage mesuré | balafon · Bobo-Dioulasso · inconnu · à mesurer |
   | remarques | « étiquette d'origine fausse : udu = Igbo, Nigeria » |

4. **Corriger les étiquettes** fausses (udu « Middle-East », agogo « samba ») : la fiche
   garde l'étiquette d'origine **et** la nôtre.
5. **Mesurer l'accordage** des instruments africains (01 §6.3) : il n'est presque jamais
   documenté.
6. **Relire** : une seconde personne vérifie la licence avant l'entrée dans la
   bibliothèque livrée.
7. **Crédits** : le générateur de crédits (01 §13) lit les fiches ; l'écran « À propos »
   de NkNgoma liste les sources CC-BY et MIT ; le **corpus de l'IA publie sa liste**
   (exigence Freesound pour CC-BY).
8. **Revue annuelle** des licences des sources suivies (une licence peut changer pour
   les téléchargements futurs).

---

## 9. Décisions

| # | question | proposition | état |
|---|---|---|---|
| D-A1 | le socle du §2 | le télécharger et le ficher en premier ; c'est la bibliothèque de **P1** (01 §14) | 🔴 proposé |
| D-A2 | CC-BY-SA dans la bibliothèque et le corpus | bibliothèque : oui, **dans une banque à part** restée CC-BY-SA ; corpus : **non** tant que la portée du partage à l'identique sur un modèle n'est pas tranchée par un juriste | 🔴 proposé |
| D-A3 | contenu sous GPL (FreePats GM, Hydrogen) | **non** dans la bibliothèque livrée ; l'utilisateur peut l'importer lui-même | 🔴 proposé |
| D-A4 | Lakh, Slakh, POP909 | **non** pour le corpus tant qu'un juriste n'a pas tranché (reprises d'œuvres protégées) | 🔴 proposé |
| D-A5 | sources « production seulement » | importables par l'utilisateur dans **sa** bibliothèque, marquées, jamais exportées ni entraînées | 🔴 proposé |
| D-A6 | le fonds africain | lancer les **enregistrements** (§6) avec contrats à trois cases ; contacter CERDOTOLA et le Musée de Dschang | 🔴 à décider par Rodolf |
| D-A7 | relecture juridique | un juriste relit le §1 et les sources du §2 avant la version commerciale | 🔴 |
