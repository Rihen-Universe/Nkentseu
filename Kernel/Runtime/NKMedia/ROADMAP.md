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
  3. **TS/M2TS** (MPEG Transport Stream) : format par paquets fixes 188 octets (PID + PES), démuxage
     structurellement différent d'ISOBMFF/EBML (streaming broadcast) — nouveau parseur.
  4. **FLV** (Flash Video) : conteneur simple par tags (audio/vidéo/script), utile pour du contenu
     RTMP/legacy — démuxage rapide à écrire.
  5. **OGG comme conteneur générique vidéo (.ogv)** : le démuxage Ogg (pages/lacing/granule) est
     **déjà livré** pour Ogg-Opus — étendre au **Theora** (vidéo) si rencontré en pratique (rare).
  6. **AIFF** (audio Apple, PCM non compressé la plupart du temps) : proche de WAV, coût faible.
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
