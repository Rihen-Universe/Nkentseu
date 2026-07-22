# NKMedia — conteneurs & codecs média (audio/vidéo) — ROADMAP

> Module Runtime **from-scratch, zero-STL** (`namespace nkentseu::media`) pour **lire et écrire** des
> formats conteneurs (MP4/ISOBMFF, WebM/Matroska, …) et, à terme, décoder/encoder leurs **codecs**
> (audio : AAC, Opus, Vorbis ; vidéo : VP8/VP9, H.264…). Motivé par un besoin réel : le corpus vocal
> (getUserMedia navigateur) arrive en **WebM/MP4**, que NKAudio (WAV/MP3/OGG/FLAC) ne sait pas lire ;
> et plus largement pour la **lecture/écriture vidéo in-engine** (cutscenes, enregistrement, caméra).
>
> ⚠️ **Honnêteté d'échelle** : un décodeur vidéo complet (H.264/VP9) from-scratch est un travail énorme.
> On construit **par couches**, chaque brique testable. Tant que les décodeurs natifs n'existent pas, un
> **repli par transcodage externe (ffmpeg)** dépanne (préparation de dataset, offline).

| Brique | Statut | Contenu |
|---|---|---|
| 1. Probe / démux conteneurs | ✅ | détecte le conteneur (MP4/WebM/WAV/OGG/MP3/FLAC), liste **pistes + codecs + params** (parseurs ISOBMFF + EBML) |
| 2. Extraction de paquets | ✅ | sort les **paquets audio encodés** + timestamps : MP4 (`stbl` **et fMP4 `moof/traf/trun`**), WebM (SimpleBlock/Cluster) |
| 3. Décodeur audio **Opus/CELT** | ✅ | CELT mono FONCTIONNEL — reproduit ffmpeg (**onde 0.98, RMS 0.99** vs ffmpeg après 2 fixes trouvés à l'oracle : init `oldBandE=0` au lieu de -28 ; **SCALEOUT** ÷32768 manquant en sortie float qui écrêtait le PCM). Reste résidu précision DFT directe sur tonales pures |
| 3bis. Décodeur audio **Opus/SILK** | ✅ | SILK mono FONCTIONNEL — **BIT-EXACT vs libopus/ffmpeg (onde 1.0000, spectro 0.9999, RMS 0.995)**. 9 sous-briques (gains, NLSF→LPC, LTP, excitation shell-code, synthèse DSP, en-tête, index, decode_frame, top-level), ~11 self-tests. Validé via oracle libopus compilé+patché (dump valeurs intermédiaires) : gains/LPC/signalType identiques trame par trame. **Resampler 8/12/16→48 kHz livré (up2 HQ + FIR frac) : sortie 48 kHz onde 0.99998 vs ffmpeg.** |
| 3ter. **Dispatcher Opus** (`NkOpusDecoder`) | 🔶 | Route le TOC → **CELT-only ✅ + SILK-only ✅ (bit-exact, identique au direct)** → PCM 48 kHz. Harnais `NKMediaTest --opus`. Reste : **mode hybride** (config 12-15 : SILK bande basse + CELT bande haute, nécessite une bande de départ dans NkCeltDecoder) ; **calibrer l'échelle de sortie CELT** (~3× trop fort, invisible en corrélation) |
| 3quater. **Fichiers .opus (Ogg-Opus, RFC 7845)** + câblage NKAudio | ✅ | **Conteneur Ogg livré (2026-07-12)** : probe (pistes opus/vorbis depuis les pages BOS) + démux (pages → paquets, lacing, `granule`) ; `NkOpusFile` (OpusHead pre-skip/gain + décode + trim granule ; **hybride/stéréo refusés proprement** → retirer le garde quand 3ter sera fini). Harnais `--oggopus`, self-test forge Ogg (34/34). **Validé vs ffmpeg** : SILK-WB corr 0.999985 (lag +2 = délai resampler), CELT corr 0.965 lag 0. **NKAudio branché** : `AudioFormat::OPUS` décodé via `NkOpusCodec` (détection OggS+OpusHead), test `NkMicRecord --decode in.opus out.wav`. NB : NKAudio dépend de NKMedia → 10 jengas d'apps mis à jour |
| 4. Décodeur audio AAC-LC | ✅ | *(table périmée, tenue à jour dans « Livré » ci-dessous)* MP4 → PCM, stéréo complet, bit-exact vs ffmpeg |
| 5. Muxers (écriture) | 🔶 EN COURS | **AVI (RIFF) ✅ + MOV/MP4 (ISOBMFF) ✅** ; puis WebM, WAV |
| 6. Vidéo (décode) | ✅ | *(table périmée)* **H.264 Main+High bit-exact** (MP4/MOV/3GP/MKV) ; VP8/VP9/AV1/H.265 restent à faire — voir « Bugs / limitations connues » |
| 7. **Vidéo (encode/création)** | 🔶 EN COURS | **`NkVideoWriter` : création vidéo from-scratch (SANS ffmpeg) ✅** — RAW BGR (pixel-perfect) + **MJPEG** (via codec JPEG NKImage) + **MPEG-1 Video (VRAI codec DCT, I + P-frames = compression INTER-FRAME) ✅** ; conteneurs **AVI**, **MOV/MP4**, flux élémentaire **.m1v** ; + **`NkImageSequenceWriter`** (séquence PNG/JPEG/BMP/TGA/QOI, workflow Blender). Validé lisible par ffmpeg/VLC (RAW pixel-parfait, MJPEG 0.99, MPEG-1 I≈33dB P≈30dB, **16× plus compact que MJPEG** sur contenu écran). Motion **half-pel** (interpolation bilinéaire + f_code) ✅. Prochaine brique codec : H.263 → **H.264** (même machinerie DCT/VLC/motion). Puis audio A/V |
| 8. **Décodeur vidéo VP8** | ✅ | **DÉCODEUR COMPLET (clé + inter) : 325 images BIT-EXACTES vs ffmpeg sur 6 flux** (dont altref invisibles, golden frames, 4 GOPs, SPLITMV, filterLevel 0-8, résolutions impaires). Décodeur booléen, en-têtes, modes intra+inter, MV (near/nearest/new/split), MC 6-tap, résidus, WHT+IDCT, filtre de boucle. Restes mineurs : segmentation MB, partitions multiples, versions 1-3 (refus propre). **À brancher dans NkVideoReader** (WebM/IVF). |

## Livré
- **Brique 1 (2026-07-10)** — `NkMediaProbe` (`NkMediaProbe.{h,cpp}`) : détection de conteneur + parseurs
  **ISOBMFF** (boîtes MP4 : `ftyp/moov/trak/mdia/minf/stbl/stsd/hdlr`, fourcc du sample entry, params audio)
  et **EBML** (Matroska/WebM : `Segment/Tracks/TrackEntry`, `CodecID`, `Audio` SamplingFrequency/Channels,
  `Video` PixelWidth/Height). Renvoie `NkMediaInfo { container, tracks[] }`. Testé HEADLESS (détection des
  magies + varints EBML) **et sur le corpus réel** (Bassa `.mp3` → **MP4/AAC**, ghomala' `.mp3` → **WebM/Opus**).

- **Brique 2 (2026-07-10)** — `NkMediaDemux` : extraction des **paquets audio encodés**. MP4 : tables `stbl`
  (stsz/stco/co64/stsc/stts+mdhd) **ET fMP4 fragmenté** (`moof/traf/tfhd/trun`, cas MediaRecorder navigateur) ;
  WebM : `SimpleBlock`/`Block` des Clusters + horodatage. Validé sur le corpus : ghomala' WebM/Opus **41 paquets
  (0→2399 ms)**, Bassa fMP4/AAC **359 paquets (0→7637 ms)**. `NKMediaTest <fichier>` affiche pistes + paquets.

- **Brique 3 — décodeur Opus (RFC 6716), étape 1 (2026-07-10)** — `Codecs/Opus/NkOpusPacket` : analyse de
  l'octet **TOC** (mode SILK/Hybrid/CELT, bande passante, durée, mono/stéréo) + **découpage en trames** (codes
  0-3, VBR/CBR, padding). Testé HEADLESS (table de config + 4 découpages) **et sur le corpus** : 1er paquet
  ghomala' = **config 31 (CELT fullband 20 ms), mono, 3 trames** → 60 ms/paquet × 41 = 2.46 s (cohérent).
  - **Étape 2 — range coder (2026-07-10)** — `Codecs/Opus/NkOpusRange` : port fidèle du codeur entropique
    (libopus entdec/entenc) — décodeur (`Decode/Update`, `DecodeBitLogp`, `DecodeIcdf`, `DecodeBits`, `DecodeUint`)
    + encodeur (pour le test). **Aller-retour PROUVÉ** (encode→decode = identité : icdf, bits bruts, uint, cdf).
    C'est l'épine dorsale de tout le décodage Opus.
  - **Étape 3 — CELT (en cours, 2026-07-10)** — `Codecs/Opus/Celt/`. Primitives posées + **vérifiées** :
    **`NkCeltLaplace`** (coder de Laplace, aller-retour prouvé — base énergie grossière) ; **`NkCeltBands`**
    (table `eband5ms`, 21 bandes → bins MDCT selon LM) ; **`NkCeltMdct`** (MDCT/IMDCT directe + fenêtre sinus,
    **reconstruction TDAC parfaite** < 1e-3) ; **`NkCeltEnergy`** (énergie grossière : prédiction 2D + Laplace,
    tables `e_prob_model`/`pred_coef`/`beta_coef` ; **aller-retour prouvé** LM 0-3 × intra/inter) + **énergie
    fine** (raffinement par bits bruts par bande, `UnquantFine` ; aller-retour prouvé, coexiste avec le range
    coder) + **PVQ/CWRS** (`NkCeltPvq` : `V(n,k)` + codage combinatoire vecteur↔index, cwrs.c ; **aller-retour
    EXHAUSTIF prouvé** — tous les index de plusieurs (n,k) + range coder) + **bits↔pulses** (`NkCeltRate` :
    `PulsesToBits`/`BitsToPulses`, cœur de l'allocation ; monotonie + aller-retour + budget prouvés ;
    ⚠️ approx log2 flottant, cache bit-exact rate.c à raffiner) + **allocation, cœur** (`NkCeltAlloc` : table
    `band_allocation` 11×21 + **bissection sur la qualité** → ligne tenant dans le budget + `bits1`/`bits2`/`thresh`/
    tilt `trim` ; + **`InterpFine`** = bissection fine ALLOC_STEPS=6 → **bits/bande** (budget cohérent+monotone
    prouvés)) + **forme de bande** (`NkCeltVq::AlgUnquant` : pulses → **normalise** (norme unité×gain) →
    **exp_rotation** spreading → masque collapse ; **norme conservée** prouvée, cœur de `quant_all_bands`).
    + **denormalise** (`NkCeltDenorm` : forme×2^(E+eMean) → spectre ; table `eMeans` ; ‖bande‖=2^(E+eMean)
    prouvé). **Chemin signal DSP complet** : shape → denorm → IMDCT. ⏳ Reste CELT (orchestration pure) :
    drapeaux ec skip/intensity + split pulses/énergie fine → `quant_all_bands` (boucle bandes, folding/stéréo/
    split) → anti-collapse → PCM. **Deemphasis** livré (`NkCeltDeemphasis` : IIR 1-pôle sortie, aller-retour
    préemph/deemph + réponse impulsionnelle prouvés). **Toute la boîte à outils CELT est faite+prouvée.**
    **`quant_all_bands` LIVRÉ** (`NkCeltQuantBands`) : décodage des bandes MONO complet — `compute_theta`
    (angle θ de split → imid/iside/delta), `quant_partition`/`quant_band` (split récursif via `haar1` +
    hadamard entrelacé/désentrelacé + recombinaison T/F), `AlgUnquant` aux feuilles, **folding** (LCG /
    spectre replié) + gestion `norm`/`lowband`/collapse masks. Aller-retour structurel testé (spectre fini).
    **Orchestration `celt_decode` COMPLÈTE (chemin MONO)** (`NkCeltDecoder`, 2026-07-10) : état persistant
    (énergie coarse+fine, historique anti-collapse, buffer glissant de synthèse, deemphasis, graine LCG) +
    **chaîne bout-en-bout** : flags ec exacts → `unquant_coarse_energy` → **`tf_decode`** → spread (icdf) →
    **`init_caps`** → **dynalloc** (boosts) → trim (icdf) → **`compute_allocation` décodage** (bissection +
    skip flags ec + répartition + split pulses/énergie fine + priorité + rééquilibrage, `interp_bits2pulses`) →
    `unquant_fine_energy` → **`quant_all_bands`** → réservation/lecture **anti-collapse** → **`unquant_energy_finalise`**
    → **denormalise** → **IMDCT CELT** (DFT directe reproduisant `clt_mdct_backward` : pré-rotation → FFT →
    post-rotation → repli TDAC fenêtré ; twiddles `cos(2π(i+.125)/N)`, fenêtre `sin(π/2·sin²(...))`) → overlap-add
    (buffer glissant type `decode_mem`) → deemphasis → PCM. Chemin SILENCE → PCM zéro conservé.
    **Résultat mesuré vs ffmpeg** (`NKOpusRef`, ghomala' réel + tone synthétique 4 fréquences) : **le décodeur
    reproduit fidèlement la sortie de ffmpeg** — corrélation d'onde **0.94** (ghomala') / **0.96** (tone),
    spectrogramme log-magnitude **0.996**. **✅ DÉCODEUR CELT MONO FONCTIONNEL.** Deux bugs de tables corrigés
    (2026-07-10, trouvés via test tone contrôlé encodé par ffmpeg) : (1) `e_prob_model[LM=3]` (modèle de
    proba Laplace de l'énergie grossière, trames 20 ms) était faux → énergies aplaties ; (2) `band_allocation`
    lignes 6-10 (qualité) fausses → allocation de pulses fausse → forme spectrale X fausse. Piège : la
    **consommation de bits reste exacte même avec ces bugs** (`quant_all_bands` lit jusqu'à la fin de trame),
    donc la synchro ec ne les révèle pas — il faut comparer le **contenu décodé** vs une référence. Reste
    (finition) : le petit écart 0.94→1.0 (précision DFT directe vs kiss_fft, trames transitoires). Puis
    SILK + hybride.
  ⏳ Puis **SILK** (LPC, LTP) → mode hybride → PCM float32.

- **Encodeur H.264 baseline from-scratch (SANS ffmpeg) — `Codecs/Video/H264/`** :
  - **Brique 1 (2026-07-10)** — `NkH264BitWriter` (bits MSB-first + Exp-Golomb `ue/se` + NAL Annex-B avec
    anti-émulation `0x03`) ; `NkH264Transform` (transformée entière 4×4 directe/inverse, Hadamard 4×4 DC,
    quant/dequant pilotés QP via tables `MF/normAdjust`). Round-trip testé.
  - **Brique 2a (2026-07-10)** — `NkH264Cavlc` : codage entropique des résidus (`coeff_token` par contexte
    `nC`, `trailing_ones`, niveaux prefix/suffix adaptatif, `total_zeros`, `run_before`). Tables décodées de
    minih264, vérifiées vs standard.
  - **Brique 2b (2026-07-11) — PREMIER FLUX `.h264` RÉELLEMENT DÉCODABLE** — `NkH264Encoder` :
    SPS + PPS (baseline, CAVLC, 4:2:0) + slice IDR (I) + macroblocs **I_16×16** (prédiction intra V/H/DC,
    choix par SAD) + **résidu complet** luma (DC Hadamard + AC 4×4) **et chroma** (DC 2×2 + AC) codé CAVLC
    + reconstruction cohérente au bit près avec le décodeur (prédiction des voisins). **Validé par ffmpeg**
    (décode 12 trames sans erreur comme `h264 (Baseline) yuv420p` ; **PSNR luma 31.6 dB, RGB couleur 29.7 dB**
    à QP26 — ce qui prouve le CAVLC **bit-exact**). Bug clé corrigé : échelle DC (facteur `weightScale ×16`
    manquant en déquant + ordre Hadamard-inverse-puis-échelle) qui faisait dériver la prédiction (effondrement
    à 0). `NKMediaTest` écrit un `h264_test.h264` de démonstration.
  - **Qualité vidéo — enquête + fixes (2026-07-12)** : artefacts « comme défectueux » signalés sur les
    captures NK_RECORD. **Cause racine trouvée** : le SPS n'écrivait **PAS de VUI** → le décodeur supposait
    **limited-range (16-235)** alors que l'encodeur produit du YUV **pleine plage** → ré-étirement → couleurs
    délavées/contraste faux. **Fix** : VUI `video_full_range_flag=1` + BT.601 (`ffprobe` confirme
    `color_range=pc`). + **QP défaut `NkVideoRecorder` 24→20** (moins de macroblocking). Harnais objectif
    **`NKMediaTest --vidquality`** (motif synthétique → H264 + MJPEG + réf brute pour PSNR). Constats mesurés :
    H264 (QP20, VUI) = **46-54 dB** RGB (bon). **Bug MAJEUR trouvé + corrigé côté `NKImage` (JPEG)** : la
    FDCT « rapide » avait une **partie IMPAIRE fausse** (fréquences 1,3,5,7 aux mauvaises positions) → bords/
    damiers/texte détruits → **MJPEG à 4-8 dB** sur le haute-fréquence. Réécrite en **DCT-II séparable exacte**
    (vérifiée au coeff près vs DCT directe + via ffmpeg oracle). **Après fix** : NKImage MJPEG **41-57 dB**
    (égale/dépasse ffmpeg). → Le **mode MJPEG** est désormais une option qualité PROPRE, prête à brancher dans
    `NkVideoRecorder` (codec param dans `Begin()`) pour des captures sans macroblocking inter-frame.
  - **Brique 3a (2026-07-11) — Intra_4×4 (9 modes)** — `NkH264Encoder::EncodeMbIntra4x4` : macroblocs
    **I_4×4** avec les **9 modes de prédiction** (Vertical, Horizontal, DC, Diagonal-Down-Left/Right,
    Vertical-Right, Horizontal-Down, Vertical-Left, Horizontal-Up ; §8.3.1.2, formules entières exactes),
    disponibilité des échantillons haut-droite par bloc (Z-scan), **prédiction du mode** voisin
    (prev_intra4x4_pred_mode_flag / rem, min des modes gauche+haut), `coded_block_pattern` Intra (table 9-4),
    résidu LumaLevel4x4 CAVLC (contexte nC partagé avec I_16×16), reconstruction en cascade (chaque bloc
    prédit depuis les voisins reconstruits). **Choix par macrobloc I_16×16/I_4×4** selon l'activité (zone
    plate → I_16×16 compact ; texture → I_4×4). **Validé ffmpeg** : flux mixte I_16×16 + I_4×4 décodé sans
    erreur (dégradé lisse + damier fin reconstruits, PSNR ~28 dB à QP26).
  - **Brique 3c (2026-07-11) — filtre de déblocage en boucle (§8.7)** — `NkH264Encoder::DeblockFrame` :
    tables α/β (8-16) + tC0 (8-17), filtre luma (bS=4 fort / bS<4 normal) et chroma, appliqué par
    macrobloc (bords verticaux puis horizontaux), signalé par tranche (`deblocking_filter_control_present_flag`
    + `disable_deblocking_filter_idc`). Comme toutes les MB sont intra, bS = 4 (bord de MB) ou 3 (interne).
    **Validé BIT-À-BIT** : la reconstruction déblocquée de l'encodeur est **identique (PSNR = ∞ sur Y/U/V)**
    à la sortie décodée par ffmpeg → reconstruction intra ET filtre exacts au bit près (prérequis des
    références P correctes). Hook `EnableReconDump` pour la comparaison.
  - **Brique 3b (2026-07-11) — P-slices (inter-frame), BIT-À-BIT** — trames **P** (`EncodePFrame`) :
    compensation de mouvement **quart-pel luma** (6-tap demi-pel + moyenne quart-pel, §8.4.2.2.1) et
    **1/8-pel chroma bilinéaire**, **prédiction MV médiane** (§8.4.1.3), **P_Skip** (§8.4.1.1) via `mb_skip_run`,
    macrobloc **P_L0_16×16** (MVD, `coded_block_pattern` Inter, résidu LumaLevel4x4), **référence = trame
    précédente déblocquée**, estimation de mouvement entière→demi→quart-pel. Déblocage étendu aux **forces de
    bord inter** (bS 0/1/2 selon coeffs/Δmv, par segment 4×4). **Validé BIT-À-BIT** : reconstruction de
    l'encodeur = sortie ffmpeg (**PSNR = ∞ sur Y/U/V**, flux I + 11 P). Compression réelle : **P ~10× plus
    petites que l'I** (74-147 o vs 1277 o). ffmpeg reconnaît `I` puis `P`.
    - **Bugs clés corrigés** (via comparaison bit-à-bit `EnableReconDump`) : (1) prédiction du mode Intra_4×4
      — un voisin indisponible force `dcPredModePredictedFlag` → predMode=2 (je faisais `min(voisin, 2)`) ;
      (2) déblocage — les bords inter demandent bS 0/1/2, pas 3/4 (méthode isole : ∞ déblocage OFF).
  - **H.264 baseline INTRA+INTER complet et bit-exact** (I_16×16, I_4×4, P_16×16, P_Skip, CAVLC, déblocage).
  - **Muxing MP4 (2026-07-11)** — `NkMp4H264Writer` (`Video/Containers/`) : conteneur ISOBMFF avec entrée
    d'échantillon **avc1** + box **avcC** (SPS/PPS), NAL **longueur-préfixées** (4 o), tables stbl
    (stsd/stts/stsc/stsz/stco) + **stss** (images clés IDR). `NkH264Encoder` détecte l'extension `.mp4`/`.mov`
    et mux directement (découpe le flux Annex-B → SPS/PPS vers avcC, VCL vers échantillons). **Validé
    ffprobe** : `codec=h264, yuv420p, 12 trames`, décodé sans erreur → **`.mp4` cliquable** (VLC, navigateur,
    QuickTime). Le flux élémentaire `.h264` reste disponible.
  - **Mux A/V (2026-07-11)** — `NkMp4H264Writer` gère une **2ᵉ piste audio LPCM** ('sowt', 16 bits LE) :
    `SetAudio` + `AppendAudioPcm`, bloc PCM contigu dans mdat, `stbl` audio (stsd 'sowt'/stts/stsc/stsz/stco)
    + `smhd` + hdlr 'soun' ; `moov` à 2 `trak` (vidéo track_id=1 + audio track_id=2), échelle de temps film
    commune (1000 ms). `NkH264Encoder::SetAudioTrack`/`WriteAudioPcm` exposent l'audio. **Validé ffprobe** :
    MP4 à **2 flux** — `h264` + `pcm_s16le 44100 Hz mono`, mêmes 0.48 s → **vidéo + son synchronisés**
    (démo : bip 440 Hz). Pas d'encodeur audio requis (PCM). → 1ère vraie **vidéo sonore from-scratch**.
  - **Multi-pistes + langues + sous-titres (2026-07-11)** — `NkMp4H264Writer` gère **N pistes audio** (choix
    de **langue** via tag ISO-639-2 `mdhd` + `alternate_group`=1) et **N pistes sous-titres `tx3g`** (texte
    3GPP timed-text, langue + `alternate_group`=2, échantillons avec trous vides pour le timing).
    `NkH264Encoder::AddAudioTrack(rate,ch,lang)` / `AddSubtitleTrack(lang)` / `AddSubtitle(idx,texte,startMs,
    durMs)`. **Validé ffprobe** : MP4 à **5 pistes** = vidéo + audio `fre`+`eng` + sous-titres `fre`+`eng` ;
    extraction SRT bit-correcte (texte + timing + style). → **menu langues audio/sous-titres** dans VLC.
  - **Langues illimitées (2026-07-11)** — pistes audio/sous-titres en **nombre illimité** (`NkVector`
    dynamique, plus de cap) et **toute langue sans restriction** : la box **`elng`** (Extended Language Tag,
    BCP-47) porte le tag complet en plus du champ compact `mdhd`. **Validé** : MP4 à **7 pistes** avec
    sous-titres `fre/eng/bbj (ghomala') / bas (bassa)` — langues africaines correctement taggées et extraites.
  - ⏳ Reste : partitions inter fines (16×8/8×16/8×8), intra-en-P ; audio compressé (AAC/Opus) ; HLS/DASH
    (sélection de résolution 360p→4K = segmentation + manifeste, chantier séparé).

- **Pipeline de conversion vidéo (2026-07-11)** — `NkVideoConverter` (`Video/NkVideoConverter.{h,cpp}`) :
  architecture **source (décodage) → RGB → sink (réencodage)**, format de sortie déduit de l'extension
  (`.mp4`/`.mov`→H.264, `.avi`→MJPEG, `.m1v`→MPEG-1). Réutilise **NKImage** (décodage) + **NkVideoWriter** /
  **NkH264Encoder** (encodage). Deux entrées : **séquence d'images** (`ImageSequenceToVideo`, tout format
  NKImage) et **vidéo MJPEG AVI** (`MjpegAviToVideo` : parse le RIFF, décode chaque JPEG, réencode). **Validé
  ffprobe** : séquence PNG → H.264 MP4, et **transcode MJPEG AVI → H.264 MP4** (24 Ko → 3.6 Ko, **~6.7× plus
  compact**, contenu correct). Extensible : ajouter une source/sink = brancher un décodeur/encodeur.

- **Enregistreur A/V — capture du rendu moteur (2026-07-11)** — `NkVideoRecorder` (`Video/NkVideoRecorder.
  {h,cpp}`) : enregistre le rendu (NKRenderer/NKCanvas) **+ le son** en MP4. API `Begin / AddAudio(langue) /
  AddSubtitleTrack(langue) / PushVideo(pixels, fmt, flipVertical) / PushAudio / AddSubtitle / End`. Gère le
  retournement vertical (framebuffers bottom-up OpenGL) et RGBA32/RGB24/BGR24. Côté moteur, ajout de
  **`NkRenderWindow::CaptureToImage(NkImage&)`** (NKCanvas) : readback DX11 du backbuffer **en mémoire** (au
  lieu d'un fichier) → à passer directement à `PushVideo`. **Validé** : capture simulée (framebuffer RGBA +
  son) → `engine_capture.mp4` à **5 pistes** (vidéo H.264 + 2 audio fr/en + 2 sous-titres fr/ghomala'),
  pistes synchronisées. Boucle type : `Display()` → `CaptureToImage` → `PushVideo` ; mix NKAudio → `PushAudio`.
  **Multi-backend** : le recorder est agnostique de la source (prend des pixels CPU). La capture moteur existe
  déjà **sur les 6 backends** — via `NkOffscreenTarget::ReadbackPixels` / `NkAIRenderingTarget` (NKRenderer)
  qui passent par les primitives NKRHI unifiées `CopyTextureToBuffer`+`ReadBuffer`
  (**DX12/OpenGL/Vulkan/Metal/Software**), et via la voie **native DX11** (`NkRenderWindow::CaptureToImage`,
  vérifiée pixel-perfect) car le `CopyTextureToBuffer` DX11 est un stub. → `ReadbackPixels(buf)` /
  `CaptureToImage(img)` → `recorder.PushVideo(...)`, quel que soit le backend.
  **Unification DX11 (2026-07-11)** — `CopyTextureToBuffer` DX11 **implémenté** (émulé : copie texture→staging
  texture sur le deferred context, puis transfert des pixels dans le buffer readback au `Execute` ; MapBuffer
  passe en `MAP_READ` pour les buffers `NK_READBACK`, borné à ce cas → upload/rendu inchangés). Désormais les
  **6 backends** capturent par la MÊME voie NKRHI (`NkOffscreenTarget::ReadbackPixels`) → `recorder.PushVideo`.
  Compile OK ; à valider sur GPU réel (code non testable en headless).
  **Démo GPU live (2026-07-11)** — `NKViewportDemo` : mode enregistrement **opt-in** (`NK_RECORD=1`) — rend la
  scène 3D (caméra auto-rotative), capture chaque backbuffer présenté via `CaptureToImage`, et enregistre
  ~4 s (120 trames) dans `viewport_capture.mp4` (vidéo H.264 + audio 440 Hz + sous-titre), puis quitte.
  Compile OK ; à lancer sur GPU réel (`NK_RECORD=1 NKViewportDemo.exe`).
  **Recorder threadé + démo renderdemo multi-backend (2026-07-11)** : `NkVideoRecorder` encode le H.264 sur
  un **thread de fond** (file + worker → l'app reste **fluide** pendant la capture ; l'encodage naïf ne bloque
  plus). `NkRenderWindow::CaptureToImage` (DX11 natif) + `CopyTextureToBuffer` DX11 implémenté → capture
  unifiée sur **les 6 backends**. Démo : `NK_RECORD=1 renderdemo --demo=2 [--backend=dx11|dx12|opengl|vulkan|
  sw]` rend dans un offscreen readable (`SetFinalColorTarget`) → `ReadbackPixels` (voie NKRHI unifiée) →
  encodage threadé → `viewport_capture.mp4`. Compile+link OK ; à valider sur GPU (écran noir pendant la
  capture car le rendu final est redirigé vers l'offscreen — cosmétique, la vidéo est correcte). ⏳ Reste :
  tap audio direct dans NKAudio (démo = tonalité) ; re-blit offscreen→swapchain pour un aperçu visible.

- **Décodeur vidéo VP8 (RFC 6386) — chantier démarré (2026-07-21)**, `Codecs/Video/VP8/`. Décision
  explicite Rihen : après H.264 + les 5 conteneurs additionnels (3GP/MKV/TS/FLV/AIFF, tous réutilisant
  le décodeur H.264 existant), VP8 est le point d'entrée choisi pour un VRAI nouveau codec vidéo
  (plus simple que VP9/AV1 — pas de CABAC, codeur arithmétique binaire direct à probabilité explicite
  — et plus utile en pratique que Theora/OGV, abandonné depuis longtemps au profit de VP8/VP9).
  Construit **brique par brique comme H.264**, honnêteté d'échelle : ceci est le DÉBUT d'un chantier
  multi-session, pas un décodeur fonctionnel.
  - **Brique 1 — décodeur booléen** (`NkVp8BoolDecoder.h`, §7.3) : codeur arithmétique binaire à
    probabilité EXPLICITE (0..255) fournie par l'appelant à chaque décision — contrairement à la CABAC
    H.264 (machine à états + tables de transition par contexte), **pas de table d'état normative à
    transcrire** pour le cœur du décodeur (réduit la surface des "bugs de table" qui ont coûté ×5
    sessions sur H.264). `GetBool(prob)` / `GetFlag()` / `GetLiteral(n)` / `GetSignedLiteral(n)` /
    `GetTree(tree,probs)` (arbres de probabilités §8.2, utilisés partout dans la suite : modes de
    prédiction, vecteurs de mouvement, tokens résiduels).
  - **Brique 2 — frame tag** (`NkVp8Decoder.h`, §9.1) : en-tête NON compressé (3 octets communs +
    7 de plus si image clé : start code `0x9D 0x01 0x2A`, dimensions 14 bits + échelle d'affichage
    2 bits). ⚠️ Piège documenté : le bit `key_frame` du bitstream est **inversé** (0 = image clé).
  - **Validé** (`NkVideoReadTest --vp8header <fichier.ivf>`, nouveau harnais IVF minimal — conteneur
    brut dédié aux codecs, pas de démuxage NKMedia requis) : fichier VP8 réel généré par
    `ffmpeg -c:v libvpx` — dimensions du frame tag **EXACTEMENT identiques** à l'en-tête IVF ET à
    `ffprobe` (176×144) ; `firstPartSize` plausible (611 o) ; le décodeur booléen s'initialise et
    décode `color_space`/`clamping_type` (2 premiers bits du header compressé) sans planter.
  - **Brique 3 — en-tête compressé COMPLET** (§9.2-9.11, `NkVp8Decoder.cpp`) : segmentation
    optionnelle (§9.3), type+niveau+netteté du filtre de boucle + ajustements par mode/référence
    (§9.4), nombre de partitions de coefficients (§9.5 — ⚠️ lu ENTRE le filtre de boucle et la
    quantification, ordre PAS évident depuis la seule prose RFC, vérifié contre le décodeur de
    référence libvpx `vp8_decode_frame`), quantification (§9.6), rafraîchissement des tampons de
    référence (§9.7), **mise à jour des probabilités de token** (§13.4 — mute `NkVp8FrameContext.
    coefProbs` EN PLACE via la table normative `kVp8CoefUpdateProbs`), skip de coefficients (§9.10),
    et (images non-clés) probabilités de mode/référence inter + mise à jour ymode/uv_mode/mv (§9.9,
    §17.2). **Tables normatives EXTRAITES PAR SCRIPT** (`scratchpad/vp8ref/extract.py`, PAS à la
    main) depuis le source de référence libvpx (`webmproject/libvpx`, BSD) : `kVp8DefaultCoefProbs`
    + `kVp8CoefUpdateProbs` (4×8×3×11 = 1056 entrées chacune), `kVp8KfBModeProb` (10×10×9),
    `kVp8YModeProb`/`kVp8KfYModeProb`/`kVp8UvModeProb`/`kVp8KfUvModeProb`/`kVp8BModeProb`,
    `kVp8DefaultMvContext`/`kVp8MvUpdateProbs` (2×19), `kVp8SubMvRefProb2`/`3`, `kVp8MbsplitProbs` —
    dans `NkVp8Tables.inc` (fichier généré, régénérable, jamais édité à la main). ⭐ **Piège trouvé
    dans le SCRIPT d'extraction lui-même** (pas dans les données) : les commentaires source du type
    `/* Block Type ( 0 ) */`/`/* Coeff Band ( 7 )*/` contiennent des nombres littéraux qui polluaient
    l'extraction par regex si non retirés AVANT le parsing numérique (décalait toute la séquence,
    corruption silencieuse détectée par des assertions de sanity intégrées au script comparant les
    premières/dernières valeurs connues du fichier source). ⭐⭐ **2e piège, dans le GÉNÉRATEUR de
    C++ imbriqué** : le générateur ajoutait un niveau d'accolades FANTÔME (`= {` littéral en plus de
    l'accolade déjà fournie par la fonction récursive d'imbrication) → `excess elements in scalar
    initializer` dès le tout premier élément à la compilation — trouvé en isolant le problème sur un
    cas jouet `[2][2][2][2]` compilé avec `-fsyntax-only` (méthode : ne PAS deviner depuis l'erreur
    du compilateur sur les 1056 valeurs réelles, réduire au plus petit cas reproductible).
  - **Validé** (`NkVideoReadTest --vp8header`, harnais étendu) sur le même fichier VP8 réel : tous
    les champs de l'en-tête dans des plages valides (baseQ=12 cohérent avec `-qmin 10 -qmax 42` de
    l'encodage, filterLevel=8, log2Partitions=0, refreshEntropy/Last=1 comme attendu sur une image
    clé), et surtout la position finale du décodeur booléen (283 octets) reste **À L'INTÉRIEUR** de
    la 1ère partition (611 octets) avec une marge plausible pour le décodage des modes par macrobloc
    (~99 MB restants pour ce flux 176×144) — cohérence structurelle forte, pas encore une preuve
    bit-exacte (pas de décodage complet à comparer à ce stade).
  - **Brique 4 — modes de macrobloc (image clé, §11)** : pour chaque MB, identifiant de segment
    (si la carte de segmentation est mise à jour), drapeau `mb_skip_coeff`, mode luma (arbre+probas
    **"kf"**, ⚠️ DIFFÉRENTS de ceux des images inter : l'arbre kf place `B_PRED` en PREMIÈRE feuille,
    l'autre en dernier — les confondre décode des modes plausibles mais faux, sans erreur visible),
    puis si `B_PRED` les **16 modes de sous-bloc 4×4** conditionnés par le contexte (mode du
    sous-bloc AU-DESSUS × mode du sous-bloc À GAUCHE → table 10×10×9), et enfin le mode chroma.
    Le contexte des sous-blocs **traverse les frontières de macrobloc** → stockage du champ de modes
    complet, en tableau **AVEC BORDURE** (1 colonne à gauche + 1 ligne au-dessus, laissées à zéro)
    pour que les voisins hors image vaillent naturellement DC_PRED/B_DC_PRED sans test de bord —
    même astuce que libvpx (`mi = mip + stride + 1`). Les **arbres de modes** (§8.2) sont eux aussi
    extraits par script, avec résolution PROGRAMMATIQUE des enums symboliques (`-B_DC_PRED`, `-V_PRED`…)
    depuis `blockd.h` : un réordonnancement amont de ces enums (qui indexent les tables de probas)
    casserait la génération au lieu de passer inaperçu.
  - ⭐ **Validation brique 4 — le contrôle le plus fort disponible sans pixels de référence** :
    le décodage complet (frame tag → en-tête compressé → modes de TOUS les macroblocs) doit consommer
    la 1ère partition **EXACTEMENT**, sans déborder. Un seul champ mal ordonné, un mauvais arbre ou
    une table de probabilités fausse ferait diverger la consommation de bits et raterait la borne.
    ⚠️ Le compteur d'octets seul ne suffit pas (il est plafonné à la taille du tampon, donc "tout
    consommé" et "débordé silencieusement" sont indiscernables) → ajout d'un compteur de
    **dépassement** (`overreadBytes`) au décodeur booléen. **Résultat sur 4 fichiers VP8 réels
    ffmpeg** (résolutions, contenus et débits différents — 176×144 mandelbrot, 320×240 testsrc2,
    176×144 smptebars bas débit, et **100×60 non-multiple de 16** qui valide l'arrondi
    `mbCols/mbRows`) : **611/611, 884/884, 212/212, 356/356 octets, dépassement = 0 partout**.
    Cohérence sémantique en prime : les distributions de modes suivent la qualité comme attendu
    (fichier haute qualité → 69/99 MB en `B_PRED` ; bas débit → seulement 15/99 `B_PRED` et 70/99
    macroblocs sautés), et la somme des sous-modes 4×4 vaut exactement 16 × (nombre de MB `B_PRED`).
  - **Brique 5 — coefficients résiduels / tokens (§13)** : décodage des 25 blocs 4×4 par macrobloc
    (16 Y + 4 U + 4 V + 1 Y2), depuis la **2ème partition** (distincte de celle qui porte l'en-tête
    et les modes). Chaque coefficient est lu avec un contexte à trois dimensions — type de bloc
    (Y-avec-DC / Y2 / chroma / Y-sans-DC), **bande** (position dans le bloc), et **valeur du
    coefficient précédent** (0 / 1 / >1) — plus un contexte d'entropie « bloc voisin non vide »
    (somme du voisin du dessus et de gauche, 0..2) qui exige un contexte persistant **par colonne
    de macrobloc** et un contexte « à gauche » remis à zéro à chaque ligne. Grandes magnitudes
    codées par catégories (`DCT_VAL_CATEGORY1..6`) avec bits supplémentaires à probabilités
    constantes. ⚠️ Subtilité normative du saut de macrobloc (`mb_skip_coeff`) : la remise à zéro du
    contexte efface Y/U/V mais **ne touche à l'entrée Y2 que si le macrobloc possède un bloc Y2**
    (donc pas en `B_PRED`) — sinon elle garde sa valeur pour le macrobloc suivant.
  - ⭐⭐ **Bug trouvé grâce à la variété du contenu de test — sentinelle manquante (même FAMILLE
    que les bugs de table du H.264)** : les probabilités des bits supplémentaires des catégories
    3..6 sont parcourues par une boucle `while (*tab)` terminée par une **sentinelle 0**. Mises à
    plat en lignes de largeur 11, la catégorie 6 (qui a **exactement** 11 probabilités) n'avait
    **plus aucune place pour sa sentinelle** → lecture hors bornes, désynchronisation totale du
    flux. **Invisible sur le premier fichier de test** (basse qualité, baseQ=12 : les grandes
    valeurs de catégorie 6 n'apparaissent jamais, coefficients max ±41) et révélé uniquement en
    testant du contenu **haute qualité** (baseQ=4), où les symptômes étaient sans ambiguïté :
    coefficients absurdes (±32765 alors que le maximum théorique VP8 est ~±2114) et dépassement de
    partition de 13 à 192 octets. Fix : lignes de largeur 12 + assertion dans le script de
    génération garantissant qu'il reste toujours au moins une sentinelle. ⭐ **Leçon** : tester une
    seule qualité/résolution aurait laissé passer ce bug jusqu'à la comparaison pixel, où il aurait
    été bien plus coûteux à localiser.
  - **Validé** sur les **4 mêmes fichiers VP8 réels** (résolutions, contenus et débits différents) :
    la 2ème partition est elle aussi consommée **EXACTEMENT** — 5075/5075, 6833/6833, 437/437,
    1918/1918 octets, **dépassement = 0 partout** ; tous les macroblocs traités (décodés + sautés
    = mbCols×mbRows) ; et les plages de coefficients sont désormais toutes plausibles
    (±41, ±344, ±208, ±97 — largement dans le maximum théorique). **Les DEUX partitions d'une
    image clé sont donc maintenant parsées intégralement et sans dérive.**
  - ⭐⭐⭐ **Brique 6 — RECONSTRUCTION : une image clé VP8 se décode BIT-EXACT vs ffmpeg.**
    Déquantification (§14.1, tables `dc_qlookup`/`ac_qlookup` extraites par script ; le DC du bloc
    Y2 est **doublé** et son AC multiplié par 155/100 borné à 8 — compensation d'échelle de la
    transformée de Walsh ; le DC chroma est **plafonné à 132**) ; transformées inverses (**WHT 4×4**
    reconstruisant les 16 DC luma depuis le bloc Y2, + **IDCT 4×4** en virgule fixe 16 bits, chacune
    avec son chemin rapide « DC seul ») ; **prédiction intra** complète — 16×16 et chroma 8×8
    (DC/V/H/TM, le DC ayant 4 variantes selon la disponibilité des voisins, dont la valeur normative
    **128** quand ni le haut ni la gauche n'existent) et **4×4 B_PRED, les 10 modes** ; puis
    reconstruction (prédiction + résidu, avec saturation). ⚠️ En `B_PRED` chaque sous-bloc est
    prédit **puis reconstruit avant le suivant** (cascade), ses voisins servant de référence.
    ⚠️ Astuce reprise de la référence : quand un bloc Y2 existe, ses DC déjà déquantifiés sont
    écrits dans les blocs luma et le facteur DC de ces blocs est forcé à **1** pour ne pas les
    re-multiplier.
  - ⭐⭐ **Bug trouvé et corrigé — extension du bord DROIT.** Premier essai : **17 pixels faux sur
    115200** (chroma déjà parfaite). Le diagnostic a été immédiat parce que le harnais affiche les
    coordonnées ET le contexte (macrobloc, mode) des pixels divergents : les 17 étaient **tous** dans
    la dernière colonne de macroblocs, dans le sous-bloc le plus à droite, et **uniquement** sur les
    modes `B_LD_PRED`/`B_VE_PRED` — c'est-à-dire exactement les modes qui lisent des échantillons
    **en haut à droite**, hors image à cet endroit. La référence réplique le dernier pixel réel dans
    4 pixels de bordure à la fin de chaque ligne de macroblocs, et **uniquement sur les deux
    dernières lignes** (seule la ligne 15 sera lue comme « au-dessus » par la ligne suivante).
    ⭐ **Leçon** : faire afficher au harnais le CONTEXTE des pixels faux (position, macrobloc, mode)
    plutôt qu'un simple compteur transforme un « 17 pixels faux » opaque en diagnostic immédiat.
  - ⭐ **Validé BIT-EXACT vs ffmpeg sur DEUX flux indépendants** dont le filtre de boucle est
    désactivé (`filterLevel == 0`, seul cas comparable tant que le filtre n'est pas écrit) :
    **0 / 115200** et **0 / 38016 pixels différents**, contenus et résolutions différents. Sur les
    trois flux à `filterLevel > 0`, l'écart résiduel **suit exactement l'intensité du filtre**
    (niveau 1 → écart max 2 ; niveau 5 → max 4 ; niveau 8 → max 7), signature caractéristique d'un
    déblocage manquant et non d'une erreur de décodage.
  - ⭐⭐ **Brique 7 — FILTRE DE BOUCLE (§15) : les 5 flux de test sont BIT-EXACTS, tous niveaux
    de filtre confondus.** Filtre **normal** (luma + chroma : filtre fort 6 pixels sur les arêtes
    de macrobloc, filtre 4 pixels sur les arêtes internes 4/8/12, masques `FilterMask`/`HevMask`,
    arithmétique recentrée `^0x80` en signed char conservée à l'identique de la référence pour
    l'exactitude bit à bit) et filtre **simple** (luma seulement — codé mais non exercé par nos
    flux). Seuils dérivés du niveau et de la netteté (§15.2 : `interior`, `blim = 2·lvl+interior`,
    `mblim = 2·(lvl+2)+interior`), seuil de variance d'arête via la table **image clé** (≥40→2,
    ≥15→1, sinon 0 — différente de celle des images inter). Niveau par macrobloc : niveau de base
    + deltas par référence/mode si activés (B_PRED a son propre delta de mode). ⚠️ Subtilité
    reprise de la référence : le « skip » vu par le filtre est le skip **EFFECTIF** — un macrobloc
    non sauté au bitstream mais dont **tous** les coefficients sont nuls (`eobTotal == 0`) saute
    quand même ses arêtes internes (il filtre néanmoins ses bords de macrobloc). Passe finale en
    ordre raster, séquence par MB : arête verticale MB → verticales internes → horizontale MB →
    horizontales internes. **Validé BIT-EXACT du premier coup sur les 5 flux** (0 pixel d'écart :
    115200 + 3×38016 + 9000 pixels), y compris les trois flux à `filterLevel` 1, 5 et 8 dont
    l'écart résiduel de la brique 6 a disparu exactement comme prédit ; les deux flux
    `filterLevel = 0` restent bit-exacts (non-régression).
  - ⭐⭐⭐ **Brique 8 — IMAGES INTER : LE DÉCODEUR VP8 EST COMPLET, 325 IMAGES BIT-EXACTES vs
    ffmpeg sur 6 flux.** Décodage des modes/MV (§16-17, miroir de `read_mb_modes_mv`) : référence
    (last/golden/altref via `prob_intra`/`prob_last`/`prob_gf`), recherche des voisins
    nearest/near avec compteurs de contexte (`kVp8ModeContexts[6][4]`), fusion above-left/nearest
    et échange near/nearest, `mv_bias` (sign bias par référence), cascade
    ZEROMV→NEAREST→NEAR→{NEWMV,SPLITMV}, lecture des composantes MV (arbre court 8 feuilles +
    bits longs 9..4 avec **bit 3 parfois implicite** — piège §17.2), **SPLITMV** complet
    (partitions 16×8/8×16/8×8/4×4, sous-MV avec probabilités dépendant des voisins
    gauche/dessus, remplissage par partition AVANT la suivante qui peut s'y référer).
    **Compensation de mouvement** : filtre **6-tap séparable** (2 passes AVEC clamp intermédiaire,
    copié à l'identique — un offset nul donne le filtre exactement neutre), copie directe en
    plein-pel, chroma = MV moitié **arrondie vers zéro** (formule `+= 1|(v>>31)` de la référence) ;
    SPLITMV : luma par copies **clampées UMV**, chroma = **moyenne des 4 MV BRUTS** de chaque
    quadrant (`build_4x4uvmvs` : les MV clampés servent au luma seulement, subtilité vérifiée dans
    la référence) avec son propre clamp UV. **Gestion des tampons de référence** (§9.7) dans
    l'ordre exact de `swap_frame_buffers` : copies arf puis gf (la 2e voyant l'effet de la 1re),
    puis rafraîchissements ; bordures étendues de 30 pixels par réplication (la MC peut lire hors
    image, MV clampés ≤19 px + 3 taps). **Repli `refresh_entropy_probs == 0`** : sauvegarde prise
    APRÈS le reset image-clé et AVANT les mises à jour du header (⚠️ premier jet buggé : la
    sauvegarde était prise après le parse, donc après mutation — corrigé avant même le premier
    test). Intra en frame inter : arbres INTER (`kVp8YModeTree`, B_PRED en dernière feuille) et
    probas 4×4 FIXES non contextuelles. Filtre de boucle inter : table HEV **inter** (≥40→3,
    ≥20→2, ≥15→1) + `mode_lf_lut` complet (B_PRED→0, ZEROMV/intra→1, NEAREST/NEAR/NEW→2,
    SPLITMV→3) + deltas par référence. **Validé BIT-EXACT, 325/325 images sur 6 flux** : les 5
    flux existants ré-encodés en séquences complètes (5×25 images, 1 clé + 24 inter chacun,
    filterLevel 0/1/5/8, résolution impaire comprise) **et** un flux de torture 100 images avec
    **altref invisibles** (`-auto-alt-ref 1 -lag-in-frames 16`, frames `show_frame=0` décodées
    mais non affichées), golden frames et 4 GOPs — **0 pixel d'écart partout, du premier coup**
    pour le chemin inter (seul le repli d'entropie avait un bug, corrigé avant exécution).
    Nouveau harnais `NkVideoReadTest --vp8seq <ivf> <ref.yuv>` (état persistant multi-images).
  - **Reste (finitions, refus propre en attendant)** : segmentation par macrobloc (aucun flux de
    test ne l'active) ; partitions de tokens multiples (>1) ; versions de bitstream 1-3
    (bilinéaire/fullpel). **Prochaine étape naturelle : brancher le décodeur dans `NkVideoReader`**
    (WebM/VP8 et IVF) pour lire les `.webm` VP8 dans `NkVideoPlayer`.

## En cours / À venir

*(MAJ 2026-07-19 — la section précédente était périmée : Opus/CELT+SILK ✅, AAC-LC stéréo ✅
(CPE/M/S/IS/PNS/TNS, corr 1.000000 vs ffmpeg), décodeur H264 Main+High COMPLET bit-exact avec
déblocage ✅, NkVideoReader avec réordonnancement POC ✅ — voir « Livré ».)*

- ✅ **Sync A/V dans NkVideoPlayer** (2026-07-20, commits 88c3dcb2 puis a801de04) : l'audio DU
  conteneur cadence la vidéo. **Étape 2 (a801de04) : audio STREAMÉ, jamais tout en RAM** —
  `ContainerAudioStream` (NKAudio/Streaming) décode l'AAC paquet par paquet à la demande, via
  `AudioStreamPlayer` (ring buffer SPSC + thread décodeur, déjà présent dans NKAudio mais jamais
  réellement exercé en prod) + `AudioEngine::PlayProcedural`. **Bug latent trouvé et corrigé** dans
  `AudioStreamPlayer::ReadFrames` : coupait jusqu'à ~2s d'audio déjà décodé en fin de lecture
  (confondait "producteur a fini de décoder" avec "plus rien à jouer") — `IsFinished()` ajouté pour
  distinguer les deux. Validé : pipeline complet corr=1.000000 vs ffmpeg, RAM bornée (paquets =
  quelques centaines de Ko même pour un long film, plus ring de ~2s), WavStream non régressé.
- ✅ **Codecs audio MP4 additionnels** (2026-07-20, commit 805e0c93) : `ContainerAudioStream` gère
  désormais **PCM** non compressé (twos/sowt/lpcm, 8/16/24-bit, streamé comme l'AAC — paquets de
  taille variable) en plus de l'AAC ; **MP3 embarqué** (rare) via repli RAM (`NkMP3Codec` n'a pas
  d'API incrémentale) + concaténation des paquets démuxés. ⭐ Bug de détection trouvé et corrigé au
  passage : le fourcc `mp4a` ne veut PAS dire AAC — un MP3-en-MP4 (`ffmpeg -c:a mp3 -f mp4`, cas
  réel) utilise le même fourcc ; le vrai codec est dans la boîte `esds` (objectTypeIndication),
  désormais parsée. Validé : PCM corr=1.000000 (LE et BE) ; MP3-en-MP4 correctement routé (corr
  identique à un .mp3 autonome via le même décodeur — écart de précision **pré-existant** de
  `NkMP3Codec`, hors scope, pas une régression) ; AAC non régressé (corr=1.000000).
- ✅ **Lecture d'un vrai film (2026-07-20)** : test réel sur un fichier 34 min (720×360, H264 High
  profile, 23.98 fps, AAC 48 kHz) → **audio correct** (streaming validé en conditions réelles) mais
  **vidéo "hyper lente"** signalée par Rihen. Diagnostic : PAS un bug de synchro — le décodeur H264
  décode réellement à **~8,4 fps** sur ce contenu (mesuré : 210 frames en 25,1s réelles, chrono
  bash autour d'un run borné), contre 23,98 fps requis → **~3× trop lent pour le temps réel**
  (décodeur scalaire, jamais optimisé perf, seulement validé bit-exact sur petits clips synthétiques
  jusqu'ici). Le cap de rattrapage fixe (`catchup < 4` décodes/tick, chaque décodée ET affichée)
  faisait **diverger** l'écart vidéo/audio sans limite. **Fix appliqué** (`NkVideoPlayer/main.cpp`) :
  rattrapage borné par un **budget de TEMPS** (`NkChrono`, 150 ms/tick, s'adapte à la vitesse réelle
  du décodeur) + upload/dessin (`pushFrame`) appelé **UNE SEULE FOIS** par tick avec la dernière image
  atteinte (économise le coût de rendu des images intermédiaires, de toute façon aussitôt remplacées).
  Dégrade proprement (le retard vidéo/audio reste borné par le débit réel de décodage) au lieu de
  diverger. ⭐ **Root cause réelle trouvée et corrigée le lendemain (2026-07-21, voir Bugs ci-dessous)** :
  ce n'était PAS le décodeur qui était trop lent, mais un bug de copie profonde évitable dans la
  gestion du DPB du lecteur — fix → **6 fps → 30 fps** (×5) sur le même film.
- **Conteneurs supplémentaires** (cap validé Rihen 2026-07-19, PLUS TARD) : **MKV/WebM (EBML)**
  en priorité, puis TS/M2TS, FLV, OGG. Le gros du travail = démuxage (le décodeur H264 est prêt) ;
  VP8/VP9/AV1 seraient de nouveaux décodeurs.
- **Plan détaillé conteneurs + codecs additionnels (2026-07-20)** — prochain chantier, à démarrer
  par le moins coûteux :
  1. ✅ **3GP/3G2 LIVRÉ (2026-07-21)** — confirmé quasi gratuit comme prévu : le chemin vidéo
     (`NkVideoReader::Open`) détecte déjà TOUT fichier ISOBMFF par la magie de boîte `ftyp` SANS
     regarder le `major_brand` ni l'extension → un `.3gp`/`.3g2` H.264+AAC s'ouvre et se décode
     **sans aucun changement de code**. Seul gap réel trouvé : `NKAudio::OpenAudioStream` (piste
     audio streamée) filtre par **liste blanche d'extensions** (`mp4/m4a/m4v/mov/webm/mkv`) qui
     n'incluait ni `3gp` ni `3g2` → une piste audio de 3GP tombait en échec silencieux malgré un
     conteneur/codec 100% supportés. **Fix** : ajout de `"3gp"`/`"3g2"` à la liste
     (`NKAudio/Streaming/NkAudioStream.cpp`). **Validé** sur un fichier `.3gp` généré par ffmpeg
     (H.264 baseline + AAC, `-brand 3gp5`) : vidéo 50/50 images décodées, audio 89088/89088 frames
     lues exactement (`NkAudioDemo --direct-pull`, IsEOF propre), lecture bout-en-bout dans
     `NkVideoPlayer` sans erreur. **AMR-NB/WB** (codec audio 3GP legacy, alternative à AAC sur les
     très vieux téléphones) **non géré** — nouveau décodeur, reporté (rare en pratique aujourd'hui,
     la plupart des `.3gp` réels/modernes utilisent AAC comme le fichier de test).
  2. ✅ **MKV/WebM (EBML) — piste VIDÉO H264 LIVRÉE (2026-07-21)** (le pendant audio Opus/Vorbis
     existait déjà). `NkVideoReader::Open` reconnaît désormais la magie EBML (`0x1A45DFA3`) et
     `ParseWebm` (nouveau, `Video/NkVideoReader.cpp`) : trouve la 1ère piste `TrackType==1` dans
     `Segment/Tracks/TrackEntry`, lit `CodecID` (0x86) + `CodecPrivate` (0x63A2) + dimensions
     (PixelWidth/Height) — **découverte clé** : `CodecPrivate` de `V_MPEG4/ISO/AVC` est EXACTEMENT
     l'`AVCDecoderConfigurationRecord` (mêmes octets que la boîte `avcC` ISOBMFF, juste sans le
     wrapper de boîte) → **le décodage H264 lui-même est intégralement réutilisé, zéro changement**
     (`ParseAvcCBytes`/`ScanH264Keyframes` factorisés hors de `ParseMov` pour être partagés).
     Parcourt ensuite les `Cluster`/`SimpleBlock`/`BlockGroup+Block` (miroir de
     `NkMediaDemux::WalkClusters`, l'équivalent déjà livré pour l'audio) pour bâtir la table des
     paquets vidéo ; `fps` dérivé des horodatages RÉELS des blocs (EBML n'a pas d'équivalent à
     `stts`). **VP8/VP9/AV1** (codecs vidéo natifs les plus courants en `.webm` proprement dit,
     par opposition aux rips `.mkv` H264) → **échec propre** (`ParseWebm` renvoie `false`, pas de
     décodeur — honnête, pas de faux positif). **Validé** : `.mkv` H264 baseline (I+P, 50/50
     images, checksums **identiques** au même contenu encapsulé en 3GP/ISOBMFF, confirmant le
     partage de code) ET H264 High avec B-frames/réordonnancement POC (75/75 images) ; `SeekFrame`
     fonctionne aussi (même index de mots clés que MOV) ; `.webm` VP9 natif échoue proprement (pas
     de crash) ; lecture bout-en-bout `NkVideoPlayer` sur les deux OK. **Reste** : H.265/VP8/VP9/AV1
     en MKV/WebM restent des codecs à décoder (gros chantiers séparés, voir plus bas) ; le lacing
     EBML (regroupement de plusieurs trames dans un seul bloc, quasi jamais utilisé pour la vidéo)
     n'est pas géré — un fichier qui l'utiliserait produirait un paquet trop gros, échec de décodage
     propre plutôt que silencieux.
  3. ✅ **TS/M2TS LIVRÉ (2026-07-21)** — vrai nouveau parseur cette fois (structurellement différent
     d'ISOBMFF/EBML : paquets fixes 188 octets, PID + PSI/PAT/PMT + PES), contrairement à 3GP/MKV
     qui réutilisaient l'infrastructure existante. `NkVideoReader::Open` détecte le sync byte 0x47
     à intervalle régulier (188, ou 192 pour la variante M2TS/BDAV avec préfixe 4 octets — les deux
     foulées testées par `DetectTsPacketSize`, ⚠️ variante 192 codée mais **pas testée sur un vrai
     fichier BDAV**, seulement sur `mpegts` standard 188 octets généré par ffmpeg y compris avec
     l'extension `.m2ts`). `ParseTs` (nouveau) : PAT (PID 0) → PID du PMT → PMT → PID vidéo
     (`stream_type` 0x1B = H.264 seulement ; HEVC 0x24, MPEG-2 0x02… échouent proprement, pas de
     décodeur). Réassemble les PES de la piste vidéo (paquets TS fragmentés → payload ES contigu,
     `payload_unit_start_indicator` délimite les PES), extrait l'ES après l'en-tête PES via
     `NkH264Decoder::SplitNalsAnnexB` (déjà publique, réutilisée telle quelle). **Différence clé
     avec MOV/WebM** : le flux H264-en-TS est **Annex-B EN BANDE** (SPS/PPS répétés dans le flux,
     comme tout broadcast — pas de boîte `avcC`/`CodecPrivate` hors bande) → `frames[i]` stocke un
     Annex-B **déjà complet et autonome** par image (SPS/PPS mis en cache et PRÉFIXÉS si le PES ne
     les répète pas lui-même) ; `Decode()` détecte `backend==TS` et saute la conversion AVCC→Annex-B
     utilisée par MOV/WebM (nouvelle branche dédiée, `ScanH264Keyframes` rendue elle aussi
     bi-convention AVCC/Annex-B selon `backend`). `bytes` (fichier TS brut) est **remplacé** par
     l'ES Annex-B reconstruit — `frames[i]` y pointe, comme pour les autres conteneurs. **Validé** :
     `.ts` H264 baseline I+P 50/50 images (checksums **identiques** au même contenu en 3GP/MKV,
     confirme le partage intégral du décodeur) et High+B-frames 75/75 images ; `SeekFrame`
     fonctionne ; `.ts` avec vidéo MPEG-2 échoue proprement (pas de crash) ; lecture bout-en-bout
     `NkVideoPlayer` OK. **Reste** : `fps` figé à 25 (pas d'exploitation simple du PCR/PTS 90 kHz,
     `stream_type` audio (AAC 0x0F, MPEG 0x03/0x04, AC-3 0x81…) ignoré — piste audio TS non gérée
     (contrairement à MOV/WebM/3GP dont l'audio passe par `NKAudio::OpenAudioStream`), lacing
     PES multi-images non géré (rare).
  4. ✅ **FLV LIVRÉ (2026-07-21)** — le plus rapide des trois derniers, presque aussi gratuit que
     3GP/MKV : conteneur par tags séquentiels simple (pas de table d'échantillons), et **H264-en-FLV
     utilise EXACTEMENT le même `AVCDecoderConfigurationRecord`** (`AVCPacketType==0`, tag vidéo) **ET
     le même format NALU longueur-préfixé** (`AVCPacketType==1`) que la boîte `avcC` ISOBMFF — donc
     `ParseAvcCBytes` et le chemin de décodage AVCC existant (identique à MOV, PAS la variante Annex-B
     de TS) sont réutilisés **sans aucun changement**. `ParseFlv` (nouveau) : vérifie la magie `FLV`,
     saute le header (taille variable via le champ `header_size`), parcourt les tags séquentiels
     (`TagType`+`DataSize`+`Timestamp`+payload+`PreviousTagSize`), ne retient que les tags vidéo
     (`TagType==9`) avec `CodecID==7` (AVC) — Sorenson H.263/VP6/Screen Video (legacy Flash, très
     répandus historiquement) **échouent proprement** (pas de décodeur). `fps` dérivé des horodatages
     RÉELS des tags (FLV les porte nativement en millisecondes, pas de conversion d'échelle requise
     contrairement à EBML/TimecodeScale). **Validé** : `.flv` H264 baseline 50/50 images (checksum
     **identique** aux quatre autres conteneurs — MP4/3GP/MKV/TS — même contenu, **cinq conteneurs
     bit-exact cohérents entre eux**, confirmation forte de correction) + High+B-frames 75/75 ;
     `SeekFrame` fonctionne ; `.flv` Sorenson H.263 (`flv1`) échoue proprement ; lecture bout-en-bout
     `NkVideoPlayer` OK. **Reste** : audio FLV (AAC/MP3/Nellymoser…) non géré, script tag `onMetaData`
     (AMF, contient parfois un `framerate` exact) non exploité (repli sur l'estimation par
     horodatages, suffisant en pratique).
  5. **OGG comme conteneur générique vidéo (.ogv)** : le démuxage Ogg (pages/lacing/granule) est
     **déjà livré** pour Ogg-Opus — étendre au **Theora** (vidéo) si rencontré en pratique (rare).
  6. ✅ **AIFF LIVRÉ (2026-07-21)** — audio uniquement (pas de piste vidéo dans ce format), NKAudio
     pas NKMedia. Nouveau `AiffStream` (`Kernel/Runtime/NKAudio/src/NKAudio/Streaming/
     NkAudioStream.{h,cpp}`), miroir de `WavStream` déjà livré : entête `FORM`…`AIFF` (IFF), chunk
     `COMM` (au lieu de `fmt `, avec un `sampleRate` en flottant étendu IEEE 80-bit — décodeur ad
     hoc écrit, sans `<math.h>`) et chunk `SSND` (au lieu de `data`). ⚠️ **Piège AIFF-spécifique** :
     tout est **BIG-ENDIAN** (contrairement à WAV, LE) et le **PCM 8-bit est SIGNÉ** (contrairement
     au 8-bit WAV qui est NON signé) — géré explicitement, PCM 8/16/24/32-bit. AIFC (variante
     compressée) détectée et refusée proprement (magie `AIFC` ≠ `AIFF`). Câblé dans
     `OpenAudioStream` (extensions `aiff`/`aif`). **Bug latent trouvé et corrigé au passage** (pas
     lié à AIFF) : `NkAudioDemo::RunDirectPullTest`/`RunStreamingTest` appelaient `delete stream;`
     (CRT brut) sur un `IAudioStream*` alloué par `OpenAudioStream` via
     `memory::NkGetDefaultAllocator().New<T>()` — mélange allocateur custom/CRT, **heap corruption
     silencieuse jusqu'à la fin du process** (`Critical error detected c0000374` en sortie,
     symptôme classique de corruption différée) ; ne s'était jamais manifesté visiblement avant
     (WAV/3GP testés dans cette même session via ce harnais, corruption déjà présente mais crash
     retardé jusqu'en fin de process, donc invisible sans un run sous `gdb`). Fix : utiliser le même
     allocateur pour libérer (`memory::NkGetDefaultAllocator().Delete(stream)`), conforme à la
     règle dure NKMemory du projet. **Validé** : 8/16/24-bit (mono et stéréo) comparés à ffmpeg
     (conversion en WAV 16-bit commun) — **maxdiff=1 sur TOUS les échantillons**, cohérent avec la
     précision déjà établie ailleurs dans le pipeline audio float32 (arrondi du aller-retour
     int↔float32, pas une régression).
  - **Codecs vidéo à décoder (nouveaux, gros chantiers)**, par ordre de proximité avec l'existant :
    - **H.265/HEVC** : évolution directe de H.264 (même famille CABAC/transformées/déblocage en
      plus complexe — CTU/CU/PU/TU au lieu de MB fixes, plus de modes intra, SAO) — le décodeur
      H.264 bit-exact déjà livré est la meilleure base de départ.
    - **VP8/VP9** (Google, libvpx) : famille différente (pas de CABAC, arithmétique booléenne
      simple pour VP8 ; VP9 plus proche de HEVC en complexité) — nécessaire pour WebM complet.
    - **AV1** : le plus gros chantier (spec récente, complexité élevée, film grain synthesis,
      OBU/tile-based) — à ne considérer qu'après HEVC/VP9.
    - **MPEG-2 vidéo** : legacy (DVD/diffusion), plus simple que H.264 (pas de CABAC/déblocage en
      boucle obligatoire) — utile seulement si du contenu MPEG-2 réel doit être lu.
- **📺 Capture d'écran système** (cap validé Rihen 2026-07-19, PLUS TARD) : capturer une **fenêtre
  précise** du système OU le **bureau entier** (façon Google Meet / OBS), sur **toutes les
  plateformes prises en charge** (Windows : DXGI Desktop Duplication + `PrintWindow`/WGC ;
  Linux : XShm/PipeWire ; macOS : ScreenCaptureKit ; Android/iOS : MediaProjection/ReplayKit).
  Sortie = frames RGBA → brancher sur NkVideoWriter (enregistrement) et/ou NKNetwork (diffusion).
  Distinct de NKCamera (caméras physiques) — probablement un `NkScreenCapture` dans NKMedia.
- **H264 cas rares** (basse priorité) : Direct temporel, B en CAVLC, multi-slices, POC type 1,
  entrelacé, 4:2:2/10-bit ; résidu CQM+deblock-ON invisible (PSNR 50-81) documenté.
- **Encodeur H264 Main/High** (optionnel) : CABAC/B/8×8 à l'encodage pour des NK_RECORD plus compacts.

## Bugs / limitations connues
- ✅ **CORRIGÉ (2026-07-20/21) — le décodeur H264 n'était PAS le coupable de la lenteur réelle.**
  Diagnostic initial (trouvé sur un film 720×360 High 23.98 fps) : **~8,4 fps** de débit mesuré
  (mesure fiable : chrono mur autour d'un run borné, PAS le champ `t=` de `NkVideoReadTest` qui
  affiche le PTS vidéo, pas le temps de décodage réel — piège rencontré une fois). Hypothèse initiale
  (décodeur scalaire/non-SIMD trop lent) **INVALIDÉE par profilage** : une instrumentation temporaire
  (`NkChrono` par phase, retirée après diagnostic) a montré que le cœur du décodeur (boucle MB CABAC +
  déblocage) ne coûte que **~4-6 ms/frame** — le vrai coupable était **`NkVideoReader::ReadFrame`**
  (`Video/NkVideoReader.cpp`) : la gestion du DPB (liste de références) **COPIAIT EN PROFONDEUR** (au
  lieu de déplacer) jusqu'à 16 `NkH264Frame` (plans Y/Cb/Cr + grilles de mouvement, ~380 Ko/image)
  **DEUX FOIS par frame décodée** (une fois en construisant `newDpb`, une fois en l'assignant par
  copie à `h264Dpb`) — coût mesuré **~80 ms/frame et CROISSANT** avec le nombre de frames (churn heap
  pathologique), \>90% du temps total. **Fix** : `traits::NkMove` (`NKCore/NkTraits.h`) aux 3 points
  de copie + réordonnancement (conversion YUV→RGBA faite AVANT le déplacement de `f`, puisqu'elle lit
  encore `f.y/f.cb/f.cr`). Coût DPB post-fix : **~0,5 ms/frame, STABLE**. **Résultat mesuré** (même
  film, même méthode) : **6 fps → 30 fps** (×5, dépasse le 23,98 fps requis par le contenu) — validé
  bit-exact non régressé (35/35 sur deux flux témoins Main/High, I+P+B, avec et sans déblocage).
  En passant, une optimisation par cache (`McLumaRect`, `Codecs/Video/H264/NkH264IntraDecoder.cpp`)
  du filtre de compensation de mouvement quart-pel a aussi été appliquée (élimine des recalculs
  redondants du filtre horizontal 6-tap) — bit-exact confirmée mais **AUCUN gain mesurable** (le
  compilateur éliminait déjà cette redondance par CSE sur les lambdas pures) ; gardée car
  correcte et sans risque, mais **le vrai gain vient entièrement du fix DPB ci-dessus**.
  ⭐ **Leçon de méthode** : ne jamais optimiser à l'aveugle sur une hypothèse plausible — profiler
  D'ABORD (ici, une instrumentation `NkChrono` grossière par phase a immédiatement montré que 95%+ du
  temps était HORS du décodeur, dans la couche lecteur/gestion mémoire, pas dans le calcul pixel).
  Le lecteur (`NkVideoPlayer`) garde son fix de rattrapage à budget de temps (ci-dessus) en filet de
  sécurité pour tout contenu qui resterait malgré tout trop lourd (résolutions/profils plus exigeants).
- ✅ **CORRIGÉ (2026-07-21) — 2e bug de copie profonde, trouvé par échantillonnage `gdb`, PAS par
  hypothèse.** Après le fix DPB ci-dessus, Rihen a re-testé et signalé "c'est toujours lent" — la
  divergence vidéo/audio ne partait plus en vrille mais **continuait de croître** (~4 frames/s de
  retard, mesuré). Le fix DPB touchait `NkH264Decoder`/le DPB de référence, **PAS** le chemin de
  sortie du lecteur. Méthode : lancer `NkVideoPlayer` en tâche de fond, l'attacher avec `gdb -p <pid>`
  et prendre ~8 relevés de pile (`thread apply all bt`) espacés de ~0,6 s pendant qu'il tourne — **6/8
  relevés du thread principal étaient dans `NkVector<uint8>::operator=` appelé DIRECTEMENT depuis
  `NkVideoReader::ReadFrame`**, hors du décodeur. Cause : le buffer de réordonnancement POC (`h264Reorder`,
  `NkVector<NkH264Frame>`… en réalité `NkVector<NkVideoFrame>`, chacune portant un buffer RGBA COMPLET
  ~1 Mo à 720×360) était **COPIÉ (pas déplacé)** à 3 endroits : `out = h264Reorder[best]` (l'image
  extraite pour affichage), le décalage de compaction (`h264Reorder[k-1] = h264Reorder[k]`, jusqu'à
  ~4 fois), et le `PushBack` initial après décodage — jusqu'à **4-5 copies de ~1 Mo PAR IMAGE SORTIE**.
  **Fix** : `traits::NkMove` aux 3 points. **Résultat mesuré** : lag vidéo/audio **= 0, en continu sur
  14 s** (`idx == target` à chaque échantillon, contre un retard croissant avant ce fix). Bit-exact
  non régressé (ce fix ne touche que des déplacements de `NkVideoFrame`, aucune logique de décodage).
  ⭐ **Leçon de méthode #2** : quand l'instrumentation `NkChrono` par phase ne couvre pas TOUT le
  chemin (ici, elle couvrait `Impl::Decode` mais pas la boucle de réordonnancement dans `ReadFrame`
  qui l'appelle), un échantillonnage par débogueur (`gdb -p <pid>` + `bt` répété, sans même recompiler
  en Debug — les symboles Release suffisaient) révèle la fonction chaude SANS avoir à deviner où
  ajouter le prochain timer. À réutiliser en premier réflexe si un profilage par phase laisse un
  écart inexpliqué entre le temps mesuré et le temps réel observé.
- ✅ **CORRIGÉ (2026-07-21) — `NkVideoReader::SeekFrame` ne fonctionnait PAS pour le codec H264**
  (bug préexistant, découvert en creusant le bug de copie ci-dessus) : la fonction ne modifiait que
  `mImpl->cursor`, un champ utilisé par les codecs MJPEG/RAWRGB/séquences — le chemin H264
  (`ReadFrame`) utilise un état de réordonnancement POC totalement séparé
  (`h264DecodeCursor`/`h264OutCount`/`h264Reorder`/`h264GopBase`) que `SeekFrame` n'initialisait
  jamais → no-op silencieux. **Fix** : nouvel index `Impl::h264Keyframe` (`NkVector<bool>` parallèle
  à `frames`, un sample H264 est marqué clé s'il contient un NAL de type 5/IDR — scanné une seule
  fois à l'ouverture, pas de décodage, juste les en-têtes NAL) ; `SeekFrame` localise la DERNIÈRE
  IDR à un index de décodage ≤ la cible (approximation décodage≈affichage, exacte aux limites de
  GOP), y repositionne `h264DecodeCursor`/`h264OutCount`, vide `h264Reorder`/`h264ReorderKey` — le
  redécodage en avant jusqu'à la cible (coût normal = distance au GOP précédent) reste à la charge
  de l'appelant (boucle de rattrapage), comme pour tout lecteur H264. **Validé** par un nouveau
  harnais `NkVideoReadTest --seektest <fichier> <index>` (décode séquentiel de référence PUIS
  seek+redécodage, compare les sommes de contrôle) : cible 0/50/200/500/1000 sur le film réel →
  toutes retombent sur l'image exacte (±1, artefact attendu de la condition d'arrêt `CurrentIndex()
  < target`, identique à la convention déjà utilisée par la boucle de rattrapage du lecteur).
  Débloquait le scrubber UI (voir `Applications/NkVideoPlayer/ROADMAP.md`) **et** la
  resynchronisation active ci-dessous.
- ✅ **Resynchronisation active dans `NkVideoPlayer` (2026-07-21, demande explicite Rihen : "on ne
  dois pas avoir de decalage... le systeme doit etre robuste a tout moment").** Le rattrapage à
  budget de temps (150 ms/tick, voir plus haut) empêche le blocage mais n'empêchait PAS un retard
  qui croît lentement sans borne si le débit de décodage reste, même de peu, sous le débit requis
  (dépend du contenu/matériel — un décodeur scalaire ne peut PAS garantir mathématiquement un débit
  minimal pour tout contenu). Maintenant que `SeekFrame` fonctionne pour H264 (ci-dessus) : si le
  retard dépasse **~1,5 s de contenu** (`fps*1.5` images), le lecteur saute DIRECTEMENT au voisinage
  de la cible via `SeekFrame` (coût = distance au GOP, bien moindre qu'un rattrapage frame-par-frame
  décodant-puis-jetant des dizaines/centaines d'images) au lieu de laisser le rattrapage normal
  lutter indéfiniment — ramène le décalage à quelques images en une seule opération, que le
  rattrapage normal referme ensuite. Best-effort (un échec, ex. cible hors bornes en fin de flux,
  retombe simplement sur le chemin normal inchangé). Testé 20 s sans crash sur le film réel (le
  chemin de resync ne s'est pas déclenché sur ce contenu, le débit normal suffisant déjà — voir
  fix perf ci-dessus).

## Dépendances
Foundation (NKCore/NKMemory/NKContainers/NKMath) + NKStream/NKFileSystem (I/O). Consommateurs visés :
NKAudio (codecs audio), NKSpeech (corpus voix), NKImage/NKRHI (frames vidéo), NKCamera (capture).
