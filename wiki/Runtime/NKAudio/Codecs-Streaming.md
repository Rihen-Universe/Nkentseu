# Codecs et streaming audio

> Couche **Runtime** · NKAudio · Transformer des **fichiers** en son : décoder un buffer
> complet (FLAC, MP3, OGG Vorbis) en PCM avec `NkFLACCodec` / `NkMP3Codec` /
> `NkOGGVorbisCodec`, ou le lire **au fil de l'eau** via l'interface `IAudioStream` et le
> player threadé `AudioStreamPlayer`.

Dès qu'un son ne tient pas commodément en mémoire — une musique de fond de plusieurs minutes,
une ambiance qui tourne en boucle, un dialogue long — deux questions se posent : **comment
lire le format du fichier** (FLAC, MP3, OGG…) et **quand le décoder** (tout d'un coup, ou un
petit morceau à la fois). NKAudio sépare nettement ces deux préoccupations. Les *codecs*
répondent à la première : ils prennent un buffer d'octets et rendent du **PCM float32
interleaved normalisé `[-1.0, 1.0]`**. Le *streaming* répond à la seconde : il fournit le son
par paquets de frames, à la demande, sans jamais charger tout le fichier.

Le compromis central tient en une phrase : **décoder en entier est simple mais consomme
autant de RAM que dure le son ; streamer économise la mémoire mais demande un thread décodeur
et un tampon.** Les petits effets (`SFX`, clics d'interface) gagnent à être décodés une fois
puis rejoués depuis la RAM ; les longues pistes (musique, ambiances) gagnent à être streamées.
Cette page vous apprend à choisir et à câbler les deux.

Un dernier fil rouge à garder en tête dès maintenant — il revient à chaque section : **qui
possède quoi**. Les codecs vous rendent un `AudioSample` que *vous* libérez. Les streams ont
deux régimes d'*ownership* différents (l'un via `delete`, l'autre transféré au player). Tout y
est dit explicitement.

- **Namespace** : `nkentseu::audio`
- **Headers** : `NKAudio/Codecs/FLAC/NkFLACCodec.h`, `NKAudio/Codecs/MP3/NkMP3Codec.h`,
  `NKAudio/Codecs/OGG/NkOGGVorbisCodec.h`, `NKAudio/Streaming/NkAudioStream.h`,
  `NKAudio/Streaming/NkAudioStreamPlayer.h`

---

## Décoder un fichier d'un coup : les codecs

Les trois codecs partagent **exactement le même idiome**, et c'est voulu : un unique point
d'entrée statique `Decode(data, size, allocator = nullptr)`, qui prend un buffer d'octets en
mémoire et renvoie un `AudioSample` **par valeur**. Pas de constructeur à appeler, pas d'objet
à garder vivant — ce sont des classes purement statiques, sans état.

```cpp
// On a déjà chargé le fichier en mémoire (NkFile, ressource embarquée, réseau…)
AudioSample music = NkOGGVorbisCodec::Decode(bytes, byteCount);
if (music.frameCount == 0) {
    // échec : AudioSample vide, jamais d'exception
}
```

Le résultat est toujours du **PCM float32 interleaved normalisé `[-1.0, 1.0]`** — la même
représentation pour les trois formats, quelle que soit la profondeur d'origine. C'est ce qui
permet de mélanger sans réfléchir un FLAC 24 bits, un MP3 et un OGG dans le même bus : tout
arrive dans la même unité.

En cas d'échec, **aucune exception** n'est levée : la fonction renvoie un `AudioSample`
**vide**. C'est la seule façon de signaler l'erreur — il n'y a pas de code retour booléen à
côté. On teste donc systématiquement la validité du résultat (par exemple `frameCount != 0`)
avant de l'utiliser.

L'argument `allocator` (toujours optionnel, `nullptr` par défaut) désigne l'allocateur
NKMemory qui servira à réserver le buffer PCM de sortie ; `nullptr` signifie « l'allocateur
global par défaut ». Et c'est **vous** qui libérez ce buffer ensuite, via `AudioLoader::Free()`
ou `sample.Free()`.

Ce n'est **pas** une API de streaming : `Decode` charge et décode **tout** le fichier en RAM
d'un seul coup. Pour un son court c'est idéal ; pour une longue musique, préférez la section
streaming plus bas.

> **En résumé.** Les trois codecs exposent le même `static Decode(const uint8*, usize,
> memory::NkAllocator* = nullptr) noexcept`, rendent du PCM float32 interleaved normalisé,
> signalent l'échec par un `AudioSample` **vide** (jamais d'exception), et laissent la
> libération à l'appelant. `nullptr` = allocateur global.

---

## Les trois formats : FLAC, MP3, OGG Vorbis

`NkFLACCodec` décode le **FLAC**, un format **lossless** (sans perte) : le PCM reconstruit est
bit-pour-bit identique à l'original. C'est le format des masters et des sons où la qualité
prime sur la taille. Le buffer doit commencer par le magic `"fLaC"`.

`NkMP3Codec` décode le **MPEG-1/2 Layer 3** (port from-scratch de minimp3, CC0). Le buffer
peut commencer par un tag **ID3v2**, géré automatiquement. C'est le format compressé le plus
répandu, mais il vient avec des **limites précises** à connaître : les couches **Layer 1 et
Layer 2 sont silencieusement ignorées** (un fichier MPEG Layer 2 produira du vide sans
erreur), il n'y a **pas de SIMD**, **pas de streaming incrémental** (tout le buffer en RAM), et
**aucune API de seek** — `SeekToFrame` / `SeekToTime` n'existent pas. Pour repositionner un
MP3, il faut passer par un stream (voir plus bas), pas par le codec.

`NkOGGVorbisCodec` décode l'**OGG Vorbis** (port from-scratch de stb_vorbis v1.22, domaine
public). Le buffer commence par le magic `"OggS"`. C'est le plus polyvalent des trois côté
décodage : mono, stéréo et **multi-canal jusqu'à 8 canaux**, fréquences de **8 kHz à 192 kHz**,
**VBR**, floor type 1, residue 0/1/2.

L'encodage OGG, en revanche, **n'est pas disponible** : la méthode `Encode` est entièrement
commentée dans le header. Par conséquent, la struct de configuration `NkOGGVorbisEncoderConfig`
(champs `quality`, `bitrateNominal`, `bitrateMin`, `bitrateMax`) est bien **exposée** mais n'a
**aucun consommateur public** pour l'instant — c'est une spec en attente, pas une API à
utiliser.

> **En résumé.** FLAC = lossless (magic `"fLaC"`). MP3 = compressé répandu (tag ID3v2 toléré)
> **mais** Layer 1/2 ignorés, pas de SIMD, pas de seek, pas de streaming incrémental côté
> codec. OGG Vorbis = polyvalent (multi-canal ≤ 8, 8–192 kHz, VBR), **décodage seul** —
> l'`Encode` et donc `NkOGGVorbisEncoderConfig` ne sont pas utilisables aujourd'hui.

---

## Lire au fil de l'eau : `IAudioStream`

Quand le son est trop long pour qu'on veuille le tenir entier en RAM, on bascule sur un
**stream**. Le modèle est *pull* (« tirer ») : ce n'est pas le stream qui pousse le son, c'est
**l'appelant qui demande N frames** quand il en a besoin, et le stream les fournit.
`IAudioStream` est l'interface abstraite qui formalise ce contrat — toutes ses méthodes sont
virtuelles pures et `noexcept`.

```cpp
IAudioStream* s = OpenAudioStream("musics/theme.wav");
float buf[1024 * 2];                       // 1024 frames stéréo
int n = s->ReadFrames(buf, 1024);          // jusqu'à 1024 frames effectivement lues
// ... mélanger buf dans la sortie ...
if (s->IsEOF()) { /* fini */ }
delete s;                                  // ownership : delete, PAS NKMemory
```

`ReadFrames(outBuf, maxFrames)` est le cœur : il écrit jusqu'à `maxFrames` frames interleaved
float32 dans `outBuf` et **retourne le nombre réellement écrit**. Un retour à `0` signale soit
la fin du flux, soit une erreur — on distingue les deux via `IsEOF()`. `Seek(frameIdx)`
repositionne la lecture (`0` = début) et rend `true` si le format le permet. Le reste décrit le
flux : `GetFrameCount()` (nombre total de frames, ou `-1` si inconnu / flux infini),
`GetSampleRate()`, `GetChannels()`, `IsEOF()`.

La fabrique libre `OpenAudioStream(path)` détecte le format depuis le chemin ou la signature
du fichier et renvoie l'implémentation adéquate (ou `nullptr` si elle échoue). **Attention à
l'ownership** : ce pointeur se libère via **`delete`**, pas via NKMemory — c'est une exception
notable au reste du moteur, propre à la couche streaming.

> **En résumé.** `IAudioStream` est un contrat *pull* : on appelle `ReadFrames` pour tirer des
> frames float32 interleaved, `0` = EOF (à confirmer avec `IsEOF()`) ou erreur ; `Seek`,
> `GetFrameCount` (`-1` = inconnu), `GetSampleRate`, `GetChannels` décrivent le flux.
> `OpenAudioStream(path)` ouvre le bon stream — à libérer par **`delete`**.

---

## Deux implémentations : `WavStream` et `MemoryStream`

`WavStream` lit un **WAV depuis le disque par morceaux** (*chunked*) : il garde le fichier
ouvert (via `NkFile`, ce qui gère automatiquement l'`AAssetManager` sur Android) et ne lit que
ce qu'on lui demande. C'est l'implémentation à **faible empreinte mémoire** — idéale pour une
longue piste WAV non compressée qu'on ne veut surtout pas dérouler entièrement. On l'ouvre soit
directement avec `Open(path)` (qui renvoie `true` si succès), soit indirectement via
`OpenAudioStream`. Ses accesseurs `GetFrameCount` / `GetSampleRate` / `GetChannels` / `IsEOF`
sont inline et renvoient les valeurs lues dans l'en-tête WAV.

`MemoryStream` fait l'inverse : il **enveloppe un `AudioSample` déjà décodé en RAM** et le
présente comme un stream. C'est le pont entre les deux mondes — on décode un FLAC / MP3 / OGG
**en entier** avec son codec, puis on enveloppe le résultat dans un `MemoryStream` pour le
faire jouer par la même mécanique de streaming que le reste (ou pour boucler un SFX). Son
constructeur est `explicit MemoryStream(AudioSample sample)` et il **prend possession** de
l'`AudioSample` : il le libérera lui-même, via l'allocateur du sample, à sa destruction. On ne
double-libère donc **pas** le sample après l'avoir confié au `MemoryStream`.

Ce choix résout aussi le cas du MP3 : le codec MP3 n'ayant **pas de seek**, on décode le MP3 en
entier puis on l'enveloppe dans un `MemoryStream`, qui, lui, sait repositionner la lecture dans
le buffer RAM.

> **En résumé.** `WavStream` = lecture chunked d'un WAV (faible RAM, `Open(path)` ou via
> `OpenAudioStream`, gère Android). `MemoryStream` = wrapper d'un `AudioSample` déjà décodé
> (`explicit`, **prend possession** du sample) — le moyen de streamer un FLAC/MP3/OGG décodé,
> ou de boucler un SFX, et la parade au MP3 sans seek.

---

## Le player streamé : `AudioStreamPlayer`

`AudioStreamPlayer` est la pièce qui rend le streaming *jouable* sans bloquer le thread audio.
Le problème : décoder coûte du temps, et le thread audio (celui qui remplit la carte son) ne
doit **jamais** bloquer, sous peine de craquements. La solution classique, qu'implémente ce
player, est un schéma **producteur / consommateur** : un **thread décodeur interne** (le
producteur) lit le stream et remplit un **ring buffer SPSC** ; le thread audio (le
consommateur) appelle `ReadFrames` pour **pomper** ce ring buffer, en lecture **lock-free**.

```cpp
AudioStreamPlayer player;
player.Init(44100, 2);                              // config de SORTIE (backend audio)
player.Play(OpenAudioStream("musics/theme.ogg"), /*loop=*/true);
// ... dans le callback du thread audio :
int n = player.ReadFrames(mixBuf, frames);         // lock-free, jamais bloquant
// ...
player.Shutdown();                                 // stop + join du thread + libération
```

`Init(sampleRate, channels, ringBufferFrames = 88200)` configure la **sortie** : le
`sampleRate` est celui du backend audio, `channels` vaut 1 ou 2, et `ringBufferFrames` est la
taille du tampon (défaut `88200`, soit ≈ 2 s à 44,1 kHz — une marge confortable pour absorber
les hoquets du décodage). `Shutdown` arrête tout, joint le thread worker et libère les
ressources.

`Play(stream, loop = false)` démarre la lecture et **prend possession** du stream : le player
le supprimera lui-même (via `delete`) à `Stop()` ou `Shutdown()`. **Ne libérez donc jamais
vous-même un stream que vous avez passé à `Play`.** Si un stream tourne déjà, il est arrêté et
remplacé. `loop = true` re-seek au début à chaque fin, pour une boucle infinie. `Stop()`
arrête et libère le stream courant.

`Pause()` / `Resume()` suspendent et reprennent le décodage (le thread worker continue de
tourner mais ne décode plus pendant la pause). `SetVolume(v)` / `GetVolume()` appliquent un
scalaire à la sortie (`1.0` = neutre). `IsPlaying()` renvoie vrai uniquement si le player est
actif **et** non en pause.

Point capital sur les threads : **`ReadFrames` doit être appelé depuis le thread audio** (c'est
le consommateur lock-free), tandis que le producteur est le thread worker interne. Ne mélangez
pas les rôles.

> **En résumé.** `AudioStreamPlayer` = thread décodeur + ring buffer SPSC pour streamer sans
> bloquer l'audio. `Init` (config de sortie), `Play` (prend possession du stream, `loop`
> optionnel), `Stop`/`Shutdown`, `Pause`/`Resume`, `SetVolume`/`GetVolume`, `IsPlaying`.
> `ReadFrames` se pompe **depuis le thread audio** (lock-free) ; le worker interne produit.

---

## Aperçu de l'API

Tous ces éléments vivent au niveau du namespace `nkentseu::audio` (aucun type imbriqué, aucun
enum). Chacun est détaillé dans la « Référence complète » qui suit.

### Codecs (décodage de buffer complet)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| FLAC | `NkFLACCodec::Decode(data, size, alloc=nullptr)` | Décode un buffer FLAC lossless (`"fLaC"`) → `AudioSample` float32. |
| MP3 | `NkMP3Codec::Decode(data, size, alloc=nullptr)` | Décode un MPEG-1/2 Layer 3 (tag ID3v2 toléré) → `AudioSample`. |
| OGG | `NkOGGVorbisCodec::Decode(data, size, alloc=nullptr)` | Décode un OGG Vorbis (`"OggS"`, multi-canal) → `AudioSample`. |
| Config OGG | `NkOGGVorbisEncoderConfig` | Struct d'encodage (`quality`, `bitrateNominal/Min/Max`) — **sans consommateur** (`Encode` indisponible). |

### Streaming — interface et fabrique (`NkAudioStream.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Interface | `IAudioStream` (abstraite, `noexcept`) | Contrat *pull* de lecture de frames. |
| Lecture | `ReadFrames(outBuf, maxFrames)` | Lit jusqu'à N frames float32 interleaved ; retour = frames écrites, `0` = EOF/erreur. |
| Navigation | `Seek(frameIdx)` | Repositionne (`0` = début) ; `true` si succès. |
| Description | `GetFrameCount()` | Total de frames, `-1` si inconnu/infini. |
| Description | `GetSampleRate()`, `GetChannels()`, `IsEOF()` | Fréquence, canaux, fin atteinte ? |
| Fabrique | `OpenAudioStream(path)` | Ouvre le bon stream selon le format ; `nullptr` si échec — libérer par **`delete`**. |

### Streaming — implémentations concrètes

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| WAV disque | `WavStream`, `WavStream::Open(path)` | Lecture chunked d'un WAV (faible RAM, gère Android). |
| RAM | `MemoryStream`, `explicit MemoryStream(AudioSample)` | Enveloppe un `AudioSample` décodé (**prend possession**). |
| Overrides | `ReadFrames`/`Seek`/`GetFrameCount`/`GetSampleRate`/`GetChannels`/`IsEOF` | Implémentations de `IAudioStream` (inline pour les getters). |

### Player streamé (`NkAudioStreamPlayer.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `Init(sampleRate, channels, ringBufferFrames=88200)`, `Shutdown()` | Configure la sortie + ring buffer / arrête et libère. |
| Lecture | `Play(stream, loop=false)`, `Stop()` | Démarre (**prend possession** du stream) / arrête. |
| Pause | `Pause()`, `Resume()` | Suspend / reprend le décodage. |
| Pompage | `ReadFrames(outBuf, maxFrames)` | Pompe le ring buffer **depuis le thread audio** (lock-free). |
| Volume | `SetVolume(v)`, `GetVolume()` | Scalaire de sortie (`1.0` = neutre). |
| État | `IsPlaying()` | Actif **et** non en pause ? |

---

## Référence complète

Chaque élément est repris ici en détail, avec ses usages dans les différents domaines — jeu,
outils, médical, simulation. Les pièges d'ownership et de threading sont signalés à chaque fois
qu'ils s'appliquent.

### Choisir : décoder ou streamer

Le seul vrai critère est **la durée du son et la fréquence de réutilisation** :

- **Son court, rejoué souvent** (clic d'interface, impact, pas de course) → décoder une fois
  avec le codec, garder l'`AudioSample`, le rejouer depuis la RAM (éventuellement via
  `MemoryStream` pour boucler).
- **Son long, joué une fois ou en fond** (musique, ambiance, dialogue) → streamer via
  `OpenAudioStream` + `AudioStreamPlayer`, pour ne jamais charger tout le fichier.
- **Format MP3 qu'on doit pouvoir repositionner** → décoder en entier puis envelopper dans un
  `MemoryStream` (le codec MP3 n'a pas de seek ; le `MemoryStream`, si).

### Les codecs à fond

**L'idiome uniforme.** Les trois — `NkFLACCodec`, `NkMP3Codec`, `NkOGGVorbisCodec` — n'exposent
qu'une fonction utile : `static AudioSample Decode(const uint8* data, usize size,
memory::NkAllocator* allocator = nullptr) noexcept`. On ne construit jamais d'instance. On
passe le **buffer entier déjà en mémoire** (le fichier doit commencer par son magic respectif :
`"fLaC"`, un tag ID3v2 ou des frames MPEG pour le MP3, `"OggS"` pour l'OGG), et on récupère un
`AudioSample` **par valeur**. Le PCM produit est **float32 interleaved normalisé `[-1.0,
1.0]`**, indépendamment de la profondeur d'origine — ce qui uniformise le mixage.

**Gestion de l'échec.** Aucune exception, aucun code d'erreur séparé : un échec se traduit par
un **`AudioSample` vide**. On le détecte avant usage (typiquement via `frameCount`). C'est
robuste pour les pipelines d'assets qui chargent en masse : un fichier corrompu rend du vide,
pas un crash.

**Libération.** Le buffer PCM est alloué via l'`allocator` fourni (`nullptr` = allocateur
global NKMemory). C'est l'**appelant** qui libère, via `AudioLoader::Free()` ou `sample.Free()`
— jamais avec `std::free`/`delete[]`, sous peine de corruption de tas (mélange allocateur custom
/ heap CRT).

Cas d'usage, par domaine :

- **Jeu** — banques de SFX décodés au chargement d'un niveau (FLAC pour la qualité, OGG pour
  la taille), gardés en RAM et rejoués des centaines de fois.
- **Outils / éditeur** — prévisualisation d'un asset audio importé : on décode pour afficher la
  forme d'onde et écouter, sans pipeline de streaming.
- **Médical (PV3DE)** — clips de voix / visèmes décodés une fois, déclenchés en synchronisation
  avec l'animation faciale.
- **Simulation** — lots de signaux échantillonnés chargés en bloc pour analyse hors-ligne.

### `NkOGGVorbisEncoderConfig` — une spec en attente

La struct est un **agrégat** à champs publics avec valeurs par défaut : `quality = 0.4f`
(plage `[-0.1, 1.0]`, `0.4` ≈ 128 kbps stéréo), `bitrateNominal = -1` (si `> 0`, mode **CBR**
au bitrate donné en bps), `bitrateMin = -1`, `bitrateMax = -1`. **Mais** la méthode `Encode`
qu'elle devait paramétrer est **commentée dans le header** : elle n'existe pas dans l'API
publique. La struct est donc présente, compilable, documentée — et **sans aucun consommateur**
aujourd'hui. À considérer comme une intention d'API, pas comme une fonctionnalité disponible :
NKAudio **décode** l'OGG mais ne l'**encode pas**.

### `IAudioStream` à fond — le contrat *pull*

L'interface formalise une lecture **à la demande**, toutes méthodes virtuelles pures et
`noexcept` :

- **`ReadFrames(outBuf, maxFrames)`** — le cœur du flux. Écrit jusqu'à `maxFrames` frames
  interleaved float32 et **retourne le nombre réellement écrit**. Un retour `0` signifie EOF ou
  erreur : on tranche avec `IsEOF()`. C'est typiquement appelé par le moteur de mixage à chaque
  bloc audio.
- **`Seek(frameIdx)`** — repositionne au frame indiqué (`0` = début), `true` si le stream le
  permet. Sert au *scrubbing* (déplacer la tête de lecture d'une musique), au redémarrage propre
  d'une boucle, au saut à un marqueur.
- **`GetFrameCount()`** — total de frames, ou **`-1`** si inconnu (flux infini, source en
  direct). Toujours tester ce `-1` avant de calculer une barre de progression.
- **`GetSampleRate()` / `GetChannels()`** — décrivent le format du flux ; indispensables pour
  savoir s'il faut rééchantillonner ou downmixer vers la sortie.
- **`IsEOF()`** — la fin a-t-elle été atteinte ? La seule façon fiable de distinguer un
  `ReadFrames` qui rend `0` pour cause de fin d'un `0` pour cause d'erreur.

### `OpenAudioStream` — la fabrique et son ownership

`IAudioStream* OpenAudioStream(const char* path) noexcept` détecte le format depuis le chemin
ou la signature du fichier, instancie l'implémentation adaptée et la renvoie (ou `nullptr` en
cas d'échec). **Le piège majeur** : ce pointeur se libère par **`delete`**, et non via NKMemory
— le streaming alloue ici hors du système d'allocateurs custom, contrairement aux codecs. À
retenir comme une exception locale à la règle générale du moteur. (Exception dans l'exception :
si vous donnez ce stream à `AudioStreamPlayer::Play`, c'est le player qui le `delete` — voir
plus bas.)

### `WavStream` à fond — le WAV chunked

`WavStream` lit un WAV **par morceaux** depuis le disque, ce qui maintient une empreinte
mémoire minimale même pour de longs fichiers. Il s'appuie sur `NkFile`, donc il gère
transparemment l'`AAssetManager` Android (lecture depuis les assets empaquetés). On l'ouvre par
`Open(path)` (renvoie `true` si succès) ou, plus simplement, on le laisse instancier par
`OpenAudioStream`. Ses getters `GetFrameCount` / `GetSampleRate` / `GetChannels` / `IsEOF` sont
inline et reflètent l'en-tête WAV lu à l'ouverture. C'est l'outil de référence pour une
**musique de fond en WAV non compressé** qu'on ne veut pas dérouler en RAM, ou pour un long
enregistrement (dialogue, narration).

### `MemoryStream` à fond — le pont RAM

`MemoryStream` présente un `AudioSample` **déjà décodé** comme un `IAudioStream`. Son rôle est
double : (1) faire jouer par la mécanique de streaming un son qu'on a décodé en entier avec un
codec (FLAC/MP3/OGG), et (2) **boucler** un SFX gardé en RAM. Son constructeur,
`explicit MemoryStream(AudioSample sample)`, **prend possession** de l'`AudioSample` : il le
libérera lui-même via son allocateur à la destruction — ne le re-libérez donc pas de votre
côté. Il n'a pas de constructeur par défaut (il faut toujours un sample). C'est aussi la
**parade au MP3 sans seek** : décodez le MP3 en entier, enveloppez-le dans un `MemoryStream`,
et vous récupérez un `Seek` fonctionnel (sur le buffer RAM).

Les getters inline (`GetFrameCount` → `nk_int64(mSample.frameCount)`, `GetSampleRate` →
`mSample.sampleRate`, `GetChannels` → `mSample.channels`, `IsEOF` → position courante vs
`frameCount`) révèlent au passage les champs publics attendus d'un `AudioSample` : `frameCount`,
`sampleRate`, `channels` (définis dans `NKAudio.h`, hors de cette page).

### `AudioStreamPlayer` à fond — streamer sans craquer

C'est l'orchestrateur du streaming temps réel. Modèle **producteur / consommateur** : un
**thread décodeur interne** lit le `IAudioStream` et remplit un **ring buffer SPSC** float32 ;
le **thread audio** appelle `ReadFrames` pour vider ce tampon, en **lecture lock-free**. Ainsi
le décodage (potentiellement lent, surtout pour l'OGG/MP3) ne bloque jamais la sortie son.

- **`Init(sampleRate, channels, ringBufferFrames = 88200)`** — configure la **sortie** :
  `sampleRate` aligné sur le backend audio, `channels` 1 ou 2, et la taille du ring buffer en
  frames (défaut `88200` ≈ 2 s à 44,1 kHz, marge pour absorber les à-coups). Renvoie `true` si
  l'initialisation réussit. Un ring plus grand = plus de latence mais plus de robustesse face
  aux pics de charge CPU.
- **`Play(stream, loop = false)`** — démarre la lecture et **prend possession** du `stream`
  (le `delete` à `Stop`/`Shutdown`). **Ne libérez jamais vous-même un stream confié à `Play`.**
  Si un stream tourne déjà, il est arrêté et remplacé proprement. `loop = true` re-seek à 0 à
  chaque fin pour une boucle infinie (musique de menu, ambiance). Renvoie `true` si succès.
- **`Stop()`** — arrête la lecture et libère le stream courant.
- **`Pause()` / `Resume()`** — suspendent et reprennent le décodage. Le thread worker continue
  de tourner pendant la pause mais ne produit plus ; pratique pour un menu pause de jeu sans
  démonter le player.
- **`ReadFrames(outBuf, maxFrames)`** — pompe jusqu'à `maxFrames` frames depuis le ring buffer
  et retourne le nombre écrit. **À appeler depuis le thread audio** (c'est le consommateur
  lock-free) ; ne pas l'appeler depuis un autre thread.
- **`SetVolume(v)` / `GetVolume()`** — scalaire appliqué à la sortie (`1.0` = neutre, `0.0` =
  silence). Utile pour un fondu logiciel ou un mute.
- **`IsPlaying()`** — `true` seulement si le player est actif **et** non en pause.
- **`Shutdown()`** — stoppe, **joint** le thread worker et libère toutes les ressources. À
  appeler avant de détruire le player.

Cas d'usage, par domaine :

- **Jeu** — musique de fond en boucle (`Play(stream, true)`), fondu entre deux pistes via
  `SetVolume`, pause synchronisée avec le menu via `Pause`/`Resume`.
- **Médical (PV3DE)** — narration ou consigne longue streamée pendant que le moteur garde la
  main sur l'animation.
- **Outils** — lecteur de prévisualisation d'un long asset, avec barre de progression bâtie sur
  `GetFrameCount` (en gérant le cas `-1`).

### Les idiomes et pièges transversaux

- **Ownership divergent — la règle d'or.** Les codecs rendent un `AudioSample` que **vous**
  libérez (`AudioLoader::Free()` / `sample.Free()`, allocateur NKMemory). `OpenAudioStream`
  rend un `IAudioStream*` à libérer par **`delete`**. Et `AudioStreamPlayer::Play` **prend
  possession** du stream : ne le `delete` jamais vous-même après `Play` — le player le fait à
  `Stop`/`Shutdown`. `MemoryStream` prend de même possession de son `AudioSample`.
- **Allocateur par défaut.** Partout, `memory::NkAllocator* allocator = nullptr` signifie
  « allocateur global par défaut ».
- **Échec silencieux des codecs.** Pas d'exception, pas de booléen : un échec = `AudioSample`
  vide. Toujours tester avant usage.
- **MP3 limité.** Pas de seek, pas de streaming incrémental, Layer 1/2 ignorés sans erreur. Pour
  repositionner, passer par un `MemoryStream`.
- **OGG décodage seul.** `Encode` indisponible → `NkOGGVorbisEncoderConfig` exposé mais inerte.
- **Threading du player.** `ReadFrames` se pompe **depuis le thread audio** (lock-free) ; le
  producteur est le thread worker interne. Ne pas mélanger les rôles.

---

### Exemple

```cpp
#include "NKAudio/Codecs/OGG/NkOGGVorbisCodec.h"
#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
using namespace nkentseu::audio;

// 1) SFX court : décoder une fois, garder en RAM.
AudioSample click = NkOGGVorbisCodec::Decode(clickBytes, clickSize);
if (click.frameCount != 0) {
    // ... utiliser click ... puis :
    click.Free();                          // libération par l'appelant
}

// 2) Musique longue : streamer en boucle sans charger tout le fichier.
AudioStreamPlayer music;
music.Init(44100, 2);                      // config de la sortie audio
music.Play(OpenAudioStream("musics/theme.ogg"), /*loop=*/true);
music.SetVolume(0.8f);

// 3) Dans le callback du thread audio : pomper le ring buffer (lock-free).
int n = music.ReadFrames(mixBuffer, blockFrames);

// 4) À l'arrêt : Shutdown joint le thread et libère le stream (pas de delete manuel).
music.Shutdown();
```

---

[← Index NKAudio](README.md) · [Récap NKAudio](../NKAudio.md) · [Couche Runtime](../README.md)
