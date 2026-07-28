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
| 5. Muxers (écriture) | ✅ | **AVI (RIFF) ✅ + MOV/MP4 (ISOBMFF) ✅ + WAV (RIFF) ✅ + WebM (EBML/Matroska) ✅** — `NkWebmWriter` (VP8/VP9 vidéo + Opus audio, SimpleBlock/Cluster, varints EBML) : round-trip moteur (NkVideoReader relit 25/25 paquets) + **ffprobe/ffmpeg re-décodent le fichier produit sans erreur** |
| 6. Vidéo (décode) | ✅ | **H.264 Main+High + VP8 + VP9 + HEVC/H.265 (clé+inter) bit-exacts, TOUS branchés `NkVideoReader`** (MP4/MOV/3GP/MKV, WebM/IVF, .265/hvc1) ; **H.265/HEVC DÉCODEUR COMPLET (briques 1-16) : I + P + B (bi-prédiction) en PIXELS + déblocage in-loop inter + SAO, BIT-EXACT vs ffmpeg** (8-bit + Main10 intra), MV merge/AMVP spatiaux+temporel, MC uni/bi pondérée — **branché `NkVideoReader` : .265 Annex-B + MP4 `hvc1`/`hev1` + MKV `V_MPEGH/ISO/HEVC`, DPB réel + réordonnancement POC (25/25 trames, ordre B-pyramide correct, maxPixDiff=3 = arrondi RGBA)** ; restes mineurs HEVC refusés proprement (tuiles, 4:2:2/4:4:4, 10-bit inter, `ref_pic_lists_modification`, `scaling_list_data`) ; AV1/MPEG-2/Theora restent à faire — voir « Bugs / limitations connues » |
| 7. **Vidéo (encode/création)** | 🔶 EN COURS | **`NkVideoWriter` : création vidéo from-scratch (SANS ffmpeg) ✅** — RAW BGR (pixel-perfect) + **MJPEG** (via codec JPEG NKImage) + **MPEG-1 Video (VRAI codec DCT, I + P-frames = compression INTER-FRAME) ✅** ; conteneurs **AVI**, **MOV/MP4**, flux élémentaire **.m1v** ; + **`NkImageSequenceWriter`** (séquence PNG/JPEG/BMP/TGA/QOI, workflow Blender). Validé lisible par ffmpeg/VLC (RAW pixel-parfait, MJPEG 0.99, MPEG-1 I≈33dB P≈30dB, **16× plus compact que MJPEG** sur contenu écran). Motion **half-pel** (interpolation bilinéaire + f_code) ✅. **Encodeur H.264 baseline from-scratch livré (BIT-EXACT vs ffmpeg : I_16×16 + I_4×4 + P MC quart-pel + déblocage)**. Prochaines briques codec (optionnelles) : profils H.264 avancés, puis mux audio A/V |
| 8. **Décodeur vidéo VP8** | ✅ | **DÉCODEUR COMPLET (clé + inter) : 325 images BIT-EXACTES vs ffmpeg sur 6 flux** (dont altref invisibles, golden frames, 4 GOPs, SPLITMV, filterLevel 0-8, résolutions impaires). Décodeur booléen, en-têtes, modes intra+inter, MV (near/nearest/new/split), MC 6-tap, résidus, WHT+IDCT, filtre de boucle. **Branché dans `NkVideoReader`** (WebM/IVF). Restes mineurs : segmentation MB, partitions multiples, versions 1-3 (refus propre). |
| 9. **Décodeur vidéo HEVC/H.265** | ✅ | **DÉCODEUR COMPLET, briques 1-15 (2026-07-26)** : NAL/VPS/SPS/PPS + slice header + CABAC + quadtree CTU/CU/PU/TU ; **INTRA** 35 modes + DST/DCT + déblocage + SAO (bit-exact 8-bit & Main10) ; **P** mono/multi-référence (merge/AMVP spatiaux + candidat temporel §8.5.3.2.8/9 + MC qpel/epel + pondération explicite) ; **B** bi-prédiction (MvField bi-liste, merge combiné-bi, AMVP par liste, MC bi pondérée) ; **déblocage in-loop inter** (BS §8.7.2.4, P+B dont bi-préd) + **SAO P/B** — **TOUT bit-exact vs ffmpeg** (I/P/B avec et sans filtres, ~30 flux). **Branché `NkVideoReader` (brique 16, 2026-07-26)** : `.265` Annex-B + MP4 `hvc1`/`hev1` (box `hvcC`) + MKV `V_MPEGH/ISO/HEVC`, DPB réel avec éviction RPS §8.3.2 + réordonnancement POC pour l'ordre d'affichage B-pyramide — 25/25 trames sur les 3 conteneurs, sortie bit-identique entre eux, maxPixDiff=3 (arrondi BT.601 YUV→RGBA ; décodage YUV bit-exact = maxdiff 0). **10-bit inter (Main10 complet) livré bit-exact** (MC/pondération/bi généralisées à `bitDepth` variable — P+B 10-bit avec déblocage+SAO validés maxdiff=0). Restes refusés proprement (INVÉRIFIABLES : x265 ne les émet pas, donc pas d'oracle bit-exact — ou refactor trop lourd) : tuiles, PCM (code écrit mais dormant), 4:2:2/4:4:4, `ref_pic_lists_modification`, `scaling_list_data`, CU 8×8 `log2ParallelMergeLevel>2`. |

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
  - ⭐ **Brique 9 — BRANCHÉ dans `NkVideoReader` : les `.webm` VP8 et les `.ivf` se lisent dans
    `NkVideoPlayer`.** `ParseWebm` accepte `V_VP8` (le démux EBML existant est réutilisé tel quel,
    seule la branche codec change) ; nouveau `ParseIvf` (conteneur DKIF : dims, fps `rate/scale`,
    trames longueur-préfixées). Pas de B-frames en VP8 → le curseur générique du reader suffit
    (aucun réordonnancement) ; le **seek est EXACT** (offset +0, meilleur que H264) via re-décodage
    depuis la dernière image clé affichée. Les blocs **altref invisibles** (`show_frame == 0`)
    sont mappés : `vp8DisplayBlocks[i]` = bloc produisant la i-ème image affichée ; `Decode(i)`
    décode aussi les altref intermédiaires (elles mettent à jour les références). Validé : `.ivf`
    100 images (altref/golden/4 GOPs) et `.webm` VP8+Opus réel lus intégralement via
    `NkVideoReader`, lecture bout-en-bout dans `NkVideoPlayer` sans erreur, seek exact aux index
    0/20/45. ⚠️ L'audio Opus-dans-WebM n'est PAS branché dans `OpenAudioStream` (seul l'Ogg-Opus
    l'est) → un `.webm` VP8+Opus se lit pour l'instant SANS le son (piste suivante naturelle).
  - ⭐ **Brique 10 — AUDIO Opus-dans-WebM : un `.webm` VP8+Opus se lit AVEC LE SON dans
    `NkVideoPlayer`.** `ContainerAudioStream` gagne un troisième codec (`OPUS`) : les SimpleBlocks
    WebM portent des paquets Opus **BRUTS** (pas d'encapsulation Ogg) → `NkOpusDecoder::
    DecodePacket` (le dispatcheur SILK/CELT/hybride existant) les décode paquet par paquet,
    sortie native 48 kHz. Le **pre-skip** (délai encodeur) est lu depuis l'`OpusHead` du
    `CodecPrivate` de la piste (RFC 7845 §5.1, uint16 LE offset 10 — champ `codecPrivate` ajouté
    à `NkMediaTrack` + extraction EBML `0x63A2` dans `NkMediaProbe`), repli 312 sinon ; consommé
    sur les premiers échantillons décodés (potentiellement plusieurs paquets). Seek granularité
    paquet (~20 ms) avec réinitialisation du décodeur (état SILK/CELT inter-trame non
    récupérable après un saut — même catégorie que la note PNS de l'AAC). **Validé** :
    `.webm` VP8+Opus mono réel — **corrélation 1.000000 au lag 0** vs ffmpeg (pre-skip exact),
    96648/96648 frames lues avec EOF propre, lecture bout-en-bout `NkVideoPlayer` avec vidéo ET
    son. LIMITE : le `DiscardPadding` de fin de flux WebM n'est pas géré (~13 ms de queue en
    trop en toute fin de piste, imperceptible).
  - ⭐ **Brique 11 — OPUS STÉRÉO complet (2026-07-22) : le décodeur Opus décode les 3 modes en
    stéréo.** (1) **CELT stéréo** (bands.c) : réservations/décodage `intensity` + `dual_stereo`
    dans l'allocation (LOG2_FRAC_TABLE, réajustement dans la boucle de skip, den `C*N+1` pour le
    degré de liberté θ, `>>stereo` sur l'énergie fine), `quant_band_stereo` (θ mid/side à pdf en
    escalier p0=3, cas N=2 à 1 bit de signe + rotation orthogonale, split normal avec side jamais
    replié `fill>>B`, `stereo_merge` El/Er), flag `inv` (qn==1), énergie/finalise/anti-collapse
    par canal (masques de collapse indexés `[i*C+c]`, LCG séquentiel bandes→canaux). (2) **SILK
    stéréo** (dec_API.c) : en-tête VAD+LBRR ×2 canaux, poids de prédiction MS par trame
    (`stereo_decode_pred`, tables joint/uniform3/5), flag `mid-only` quand le VAD side est nul,
    reset PARTIEL du canal side à la reprise (outBuf/sLPC/lagPrev/LastGainIndex, PAS l'état
    entropique), condCoding `INDEP_NO_LTP_SCALING` (=1) pour le side post-mid-only, conversion
    MS→LR à interpolation de prédicteurs sur 8 ms + tampons 2 échantillons (arithmétique Q13/Q16
    SMULBB/SMLAWB copiée à l'identique). (3) **Hybride stéréo** : SILK stéréo WB + CELT stéréo
    bandes 17+, tampon 48 kHz entrelacé, accumulation par canal. Canaux INTERNES par paquet (TOC) :
    un paquet SILK mono dans un flux stéréo est décodé puis dupliqué L=R. ⚠️ **BUG LATENT MONO
    trouvé et corrigé au passage : CELT-only décodait TOUJOURS 21 bandes** — un paquet CELT
    NB/WB/SWB n'en code que 13/17/19 (`endband` du TOC, opus_decoder.c) → désynchronisation
    totale du bitstream sur ces paquets (l'application voip mélange SILK et CELT WB ; nos flux de
    test mono étaient FB ou SILK purs, d'où l'invisibilité). Diagnostic par corrélation par
    fenêtre de 20 ms : 19 fenêtres parfaites puis cassure = les paquets CELT WB. **VALIDATION** :
    CELT stéréo FB 48k/64k/192k/320k corr 0.987-0.9997 par canal ; CELT WB stéréo 0.999992 ;
    SILK+CELT mixte voip 0.9956-0.9969 ; hybride 32k 0.9966 ; L==R 0.998947 identiques ;
    **`.webm` VP8+Opus stéréo bout-en-bout : corr 1.000000 lag 0 sur LES DEUX canaux**
    (`--direct-pull` 384648/384648, lecture NkVideoPlayer image+son) ; mono NON régressé
    (corr 1.000000). Branché partout : `NkOpusFile` (.opus stéréo + refus hybride PÉRIMÉ retiré,
    pre-skip/trim ×canaux), `ContainerAudioStream` (2 ch), harnais `--opus` (canaux de la piste).
  - ⭐ **Brique 12 — `DiscardPadding` WebM (2026-07-22) : la piste audio Opus d'un WebM est
    désormais ÉCHANTILLON-POUR-ÉCHANTILLON identique à ffmpeg, du premier au dernier.**
    Élément EBML `0x75A2` (entier SIGNÉ big-endian, nanosecondes) extrait du BlockGroup par
    pré-scan dans `NkMediaDemux::WalkClusters` → champ `discardPaddingNs` de `NkMediaPacket` →
    converti en frames 48 kHz (arrondi au plus proche) et tronqué en FIN du paquet décodé dans
    `ContainerAudioStream` (+ `mApproxFrameCount` exact). Validé : mono 96000/96000, stéréo
    384000/384000 (tailles IDENTIQUES à ffmpeg, corr 1.000000 L+R) — avant : +648 frames (~13 ms).
  - **Reste (finitions, refus propre en attendant)** : segmentation par macrobloc (aucun flux de
    test ne l'active) ; partitions de tokens multiples (>1) ; versions de bitstream 1-3
    (bilinéaire/fullpel) ; Opus multicanal >2 (mapping) ; paquets TOC-mono à composante CELT dans
    un flux stéréo (sautés proprement — jamais émis par libopus).
- ⭐ **Muxer WAV/RIFF (2026-07-24)** : `Audio/Containers/NkWavWriter.h/.cpp`, calqué directement
  sur `NkAviWriter` (même famille RIFF, mêmes helpers `PutU32/PutU16/PutBytes/PatchU32`,
  réutilise son `NkFourCC`). En-tête canonique 44 octets (RIFF/`fmt `16o/`data`), PCM entier
  8/16/24/32-bit OU IEEE float 32/64-bit (`WAVE_FORMAT_PCM`=1/`WAVE_FORMAT_IEEE_FLOAT`=3),
  `WriteSamples` streamable (plusieurs appels, chunk `data` grandit). Ne dépend PAS de NKAudio
  (accepte des octets PCM déjà entrelacés, comme `NkAviWriter::WriteFrame` accepte des octets
  déjà encodés) — distinct de `AudioLoader::SaveWAV` (NKAudio, hardcodé 16-bit, fonction statique
  non réutilisable en streaming, pas dans NKMedia). **Validé** : `NkWavWriter::SelfTest()`
  (round-trip bit-exact PCM 16-bit mono, en-tête + échantillons vérifiés octet par octet) +
  fichier réel confirmé par `ffprobe` (`codec_name=pcm_s16le`, dimensions/débit corrects) et
  `ffmpeg -f s16le` (échantillons décodés IDENTIQUES à ceux écrits, y compris valeurs négatives
  et `INT16_MAX`).

## Décodeur VP9 from-scratch (spec VP9 v0.7 + libvpx comme oracle) — CHANTIER EN COURS

- ⭐ **Brique 1 — SUPERFRAMES + EN-TÊTE NON COMPRESSÉ (2026-07-22)** :
  `Codecs/Video/VP9/NkVp9Decoder.h/.cpp`. `ParseSuperframe` (Annexe B : marqueur 0b110xxxxx en
  DERNIER octet, index encadré par le même octet, tailles little-endian mag 1-4) ;
  `ParseUncompressedHeader` (§6.2 COMPLET, ordre vérifié contre `vp9_decodeframe.c
  read_uncompressed_header`) : frame_marker, profil (2 bits + bit réservé profil 3),
  show_existing_frame, sync code 0x498342, color config (bit depth profils 2-3, sRGB profils 1/3),
  tailles + render size, intra-only, refs (3× idx 3 bits + sign bias, taille héritée d'une réf =
  sentinelle), filtre d'interpolation, refresh/parallel/context idx, filtre de boucle (deltas),
  quantification (base_q + 3 delta_q, détection lossless), segmentation (7 tree probs + 3 pred
  probs + 8×4 features avec bornes 255/63/3/0 et signes), tiles (bornes min/max dérivées des
  superblocs 64), header_size de l'en-tête compressé. Harnais `--vp9header <ivf>` : toutes les
  charges, toutes les sous-trames, vérifs (sync, dims clés == IVF, headerSize borné, somme
  superframe). **Validé** : self-test forgé bit à bit ; flux 1-passe 100/100 trames ; flux
  2-passes torture **108 trames dont 8 superframes et 8 altref invisibles** tous parsés.
- ⭐ **Brique 2 — EN-TÊTE COMPRESSÉ (2026-07-22)** : le bool decoder VP8 (`NkVp8BoolDecoder`)
  est RÉUTILISÉ tel quel — l'arithmétique est identique (`(range·p+256−p)>>8` ≡
  `1+((range−1)·p)>>8`), MAIS `vpx_reader_init` VP9 lit UN BIT MARQUEUR à l'init (doit
  valoir 0) — LA différence d'amorçage. `NkVp9Tables.inc` GÉNÉRÉ par `vp9ref/extract.py`
  (scratchpad) : **35 tables** extraites de libvpx avec assertions (inv_map_table[255],
  coef probs par défaut 4×[2][2][6][6][3] avec bande 0 partielle paddée, tous les mode/mv
  probs par défaut + kf probs, 8 arbres avec résolution PROGRAMMATIQUE des enums/#define +
  macro INTER_OFFSET). `NkVp9FrameContext` complet + `ParseCompressedHeader` (§6.3, ordre
  `read_compressed_header`) : tx_mode (+ tx probs p8x8→p16x16→p32x32), coef probs (flag par
  taille, deltas subexp `decode_term_subexp` + `inv_remap_prob`), skip, et pour l'inter :
  inter modes, filtre switchable, intra/inter, reference mode (compound ssi sign bias
  divergents), single/comp refs, modes Y, partitions, MV (`update_mv_probs` proba 252 →
  7 bits impairs, hp conditionnel). Taille héritée d'une réf : paramètre refW/refH.
  **Validé : 312/312 en-têtes compressés parsés sans erreur** (bit marqueur + bornes du
  bool decoder) sur 4 flux : 2-passes altref (108), 1-passe (100), HD 1280x720 multi-tiles
  (54), segmentation aq-mode (50). Un désalignement d'un seul bit ferait déborder le bool
  decoder — critère fort avant le décodage réel.
- ⭐ **Brique 3 — CONTENU DE TRAME CLÉ (2026-07-22) : partitions + modes + TOKENS, chaque
  tile consommée EXACTEMENT.** Particularité vs VP8 : modes et résidus sont ENTRELACÉS par
  bloc dans les tiles (pas de partitions séparées) → la validation par consommation exige de
  lire aussi les tokens. Livré : tiles (tailles 4 octets BE sauf la dernière, bit marqueur
  par tile, offsets alignés superbloc `min(((idx·sbDim)>>log2)<<3, miDim)`, contextes gauche
  remis à zéro par rangée de SB) ; partitions récursives 64→4 (arbre kf, contexte above/left
  par bits de `partition_context_lookup`, lecture PARTIELLE aux bords : 1 seul bool
  probs[1]/probs[2] quand rows/cols manquent, SPLIT forcé sinon) ; grille MODE_INFO (cellule
  copiée sur l'emprise = propagation de pointeurs libvpx) ; modes intra kf (arbres
  `kf_y_mode_prob[above][left]` par sous-bloc — 4X4 : 4 modes, 4X8/8X4 : 2, ≥8X8 : 1 ; voisins
  `above/left_block_mode` avec bmi du bloc adjacent) + mode UV ; segment id (arbre 7 probas),
  skip (contexte a+l), tx_size (`read_selected_tx_size`, contexte des voisins non-skip) ;
  TOKENS (`decode_coefs`) : bandes (`coefband_trans`), contexte des voisins du scan
  `(1+cache[nb0]+cache[nb1])>>1`, modèle de Pareto (255×8) au-delà du token ONE, catégories
  d'extra bits CAT1-6, scans default/row/col par type de transformée intra
  (`intra_mode_to_tx_type`), contextes d'entropie A/L par plane avec clip aux bords
  (`vp9_set_contexts`), reset au skip. 78 tables au total dans `NkVp9Tables.inc` (2e vague :
  scans+neighbors, pareto, bandes, cat probs, dc/ac_qlookup, lookups de blocs). **Validé :
  10/10 trames clés/intra, TOUTES les tiles consommées EXACTEMENT** (un seul bit d'écart
  dans les milliers de décisions par trame désaligne le bool decoder) sur 6 flux : 1-passe
  (4 clés), 2-passes (2), HD 1280x720 multi-tiles, segmentation aq-mode, résolution IMPAIRE
  355x289 (blocs partiels aux bords), LOSSLESS (~156k coefficients lus au total).
- ⭐⭐ **Brique 4 — RECONSTRUCTION D'IMAGE CLÉ (2026-07-22) : BIT-EXACTE vs ffmpeg** sur les
  flux lossless (le loop filter — qui masquerait les vraies erreurs — arrive brique 5).
  (1) **Transformées inverses** (`NkVp9Itxfm`, port fidèle inv_txfm.c) : iDCT 4/8/16/32 +
  iADST 4/8/16 (hybrides par type : {rows, cols} = default/row/col) + iWHT 4x4 lossless —
  constantes cospi/sinpi Q14, arrondi Q14, stockage intermédiaire int16 (wrap normatif :
  step[] int16 MAIS x0-x15 des iADST en int32 SANS troncature — WRAPLOW n'est une troncature
  qu'en EMULATE_HARDWARE), passes lignes→colonnes avec shifts 4/5/6/6. 3 bugs de
  transcription attrapés à la RELECTURE avant tout run (boucles compactes idct16/32 étape 7,
  wraps iadst). (2) **Prédiction intra** : 10 modes génériques (motif 127/129, extensions aux
  bords via frameW/frameH, DC 4 variantes) + ⚠ LES 6 VARIANTES 4x4 DÉDIÉES des modes
  directionnels (D45/D63/D117/D135/D153/D207 : les coins utilisent l'above-right RÉEL E..H,
  « differs from vp8 » — le générique `intra_pred_no_4x4` ne sert qu'aux ≥8x8) — sans elles :
  ~2% de pixels faux, diagnostiqué en croisant blocs faux × modes (le chroma, qui n'utilise
  jamais D45/D63 ici, était parfait). (3) **Déquantification** par segment (get_qindex ALT_Q,
  dc/ac_qlookup, deltas, dqShift 32x32) intégrée à decode_coefs (dq[0] premier coefficient
  puis dq[1]). (4) ⚠ **MARGE de 64 px** droite/bas des plans : les blocs PARTIELS des bords
  écrivent leur emprise complète (normatif, libvpx alloue pareil) — sans marge, un bloc du
  bord droit wrappe sur la bande gauche de la rangée suivante (diagnostiqué : bande de
  360−354=6 px constants). API `DecodeKeyFrame` (en-têtes + contenu + reconstruction → I420)
  + harnais `--vp9recon` (comparaison pixel à pixel vs ffmpeg). **VALIDÉ : 3 flux lossless
  BIT-EXACTS (176x144, mandelbrot 320x240, 354x288 avec superblocs partiels)** ; flux non-
  lossless : écarts = uniquement le loop filter manquant. Limite : profils 1-3 (4:4:4/sRGB/
  haute profondeur) non gérés — refus propre, le VP9 web est profil 0.
- ⭐⭐⭐ **Brique 5 — LOOP FILTER (2026-07-22) : L'IMAGE CLÉ VP9 EST COMPLÈTE — 10/10 FLUX
  BIT-EXACTS vs ffmpeg**, dont HD 1280x720 MULTI-TILES, segmentation aq-mode 640x360, bords
  partiels 354x288, 3 lossless, 2-passes et le premier flux du fichier altref. Port fidèle :
  filtres 4/8/16 taps (arithmétique ^0x80, arrondi +4/+3, masques filter/flat4/flat5/hev),
  seuils par niveau 0-63 (update_sharpness : mblim/lim, hev = lvl>>4), niveaux par
  **[segment][ref_frame][mode_lf_lut]** (table `vp9_loop_filter_frame_init`, généralisée
  brique 6 pour les blocs inter — `mode_lf_lut` : 0 pour intra+ZEROMV, 1 pour
  NEARESTMV/NEARMV/NEWMV ; skip_this = `mi.skip && mi.IsInter()`), chemin générique
  `filter_block_plane_non420` (masques 16/8/4/4int à la volée par superbloc, verticales de
  tout le SB PUIS horizontales, 1re colonne/rangée d'image jamais filtrées, arêtes internes
  4x4, « duals » = appels adjacents). Le filtre était CORRECT DU PREMIER COUP — le bug révélé
  par la validation était AILLEURS : ⚠⚠ **`transform_2d` libvpx = `{cols, rows}` — COLS EN
  PREMIER** (vp9_idct.h) → mes tables hybrides IHT étaient inversées (ADST appliqué aux
  lignes au lieu des colonnes). Invisible en lossless (tout WHT) ; diagnostiqué en 2 temps :
  (1) `-skip_loop_filter all` côté ffmpeg + option `nolf` chez nous → la BASE pré-filtre
  divergeait déjà → pas le filtre ; (2) dump des blocs tx : le bloc #0 (DCT_DCT pur) parfait,
  le #1 (DCT_ADST) faux de ±1-2 → chemin ADST → relecture du header → l'ordre des champs.
  Leçon : TOUJOURS vérifier la déclaration des structs à initialiseurs positionnels, pas
  seulement les tables.
- ⭐⭐⭐ **Brique 6 — TRAMES INTER (2026-07-23) : RÉSOLUE — VP9 INTER BIT-EXACT sur
  6 flux réels dont 100 trames ALTREF.** Le bug de désynchronisation laissé ouvert en fin de
  session précédente ("le tile ne se consomme pas exactement sur contenu à mouvement réel,
  chaque table revérifiée sans trouver l'écart") a été résolu en construisant un `vpxdec`
  instrumenté (clone libvpx local, `ucrt64`/`--target=generic-gnu`, fprintf par bloc/tuile)
  et en diffant trace contre trace avec notre décodeur — la relecture statique de code avait
  atteint ses limites, la comparaison croisée a trouvé l'écart en quelques itérations. **TROIS
  bugs distincts, tous réels et indépendants** :
  1. **Adaptation backward des probabilités ABSENTE** — VP9 persiste 4 `FRAME_CONTEXT` entre
     trames (`frame_contexts[frame_context_idx]`) et les adapte en fin de trame
     (`vp9_adapt_coef_probs` toujours ; `vp9_adapt_mode_probs`/`vp9_adapt_mv_probs` si la
     trame n'est pas intra-only) à partir des comptes de tokens/modes/MV accumulés pendant le
     décodage — la trame suivante repart de ce contexte ADAPTÉ, pas des probas par défaut.
     Notre décodeur réinitialisait `InitDefaultFrameContext` à CHAQUE trame, perdant tout
     héritage. Fix : `NkVp9EntropyState` (4 `NkVp9FrameContext` + `lastFrameWasKey`) possédé
     par l'APPELANT et passé par référence à `DecodeKeyFrame`/`DecodeInterFrame` (même
     patron que `refImages`/`prevMvs`) ; `NkVp9FrameCounts` (comptes tokens/skip/tx/modes/MV,
     miroir de `FRAME_COUNTS`) accumulé pendant le parse (`FrameParseState::counts`) ;
     `AdaptCoefProbs`/`AdaptModeProbs`/`AdaptMvProbs` (formules `merge_probs`/
     `mode_mv_merge_probs`/`vpx_tree_merge_probs` génériques sur les arbres existants) +
     `SetupPastIndependence` (reset complet des 4 slots sur trame clé/intra-only/error-
     resilient, ou du seul slot courant si `reset_frame_context==2`). Piège découvert :
     l'adaptation part de `pre_fc` = valeur PERSISTÉE d'AVANT la trame — elle IGNORE les
     mises à jour forward de l'en-tête de LA MÊME trame (qui ne servent qu'à décoder cette
     trame, puis sont écrasées par le résultat de l'adaptation).
  2. **`PredictInterRegion` : offset ABSOLU du bloc manquant côté source** — la compensation
     de mouvement positionnait correctement `dst` (destination) à l'origine globale du bloc,
     mais lisait la référence à `refPlane + y·stride + x` avec `x,y` = offsets LOCAUX au bloc
     (0 pour un bloc entier, seulement le MV s'ajoutant) : ÇA MARCHAIT PAR COÏNCIDENCE quand
     le bloc était en (0,0) (donc invisible sur le tout premier bloc luma testé), mais lisait
     la MAUVAISE position (l'origine du plan de référence) pour TOUT AUTRE bloc. Diagnostiqué
     en comparant, pour un bloc MV=0 (copie pure attendue), le pixel prédit à un pixel connu
     de la trame de référence — écart massif révélant la lecture au mauvais endroit. Fix :
     ajouter `blockX = (-mbToLeftEdge)>>(3+ssx)` / `blockY = (-mbToTopEdge)>>(3+ssy)` à `x,y`
     avant l'indexation dans le plan de référence.
  3. **Deux limites de la session précédente qui bloquaient les trames 3+** :
     `usePrevFrameMvs` était câblé mais jamais alimenté par l'appelant (toujours faux/nul) —
     ajouté un paramètre de sortie `outMvGrid` à `DecodeInterFrame` (grille `NkVp9MvRef`
     recopiée depuis `FrameParseState::mi[]` en fin de parse) + suivi d'éligibilité côté
     harnais (`!error_resilient && mêmes dimensions && trame précédente ni clé/intra-only
     NI invisible` — le piège : une trame ALTREF invisible (`show_frame=0`) casse
     l'éligibilité de la trame QUI LA SUIT, pas seulement les trames clés). Et le contrôle
     "tile consommée EXACTEMENT" (hérité de la validation trame clé) est TROP STRICT pour
     l'inter : contrairement à la trame clé, libvpx ne valide JAMAIS une consommation exacte
     par tuile (seul un OVERREAD = vraie corruption) — un reliquat de 1-2 octets de bourrage
     non lus en fin de tuile est légitime sur trame inter, plus qu'on ne peut se le permettre
     de rejeter comme "sous-consommation ~9 bits/bloc" (le vrai chiffre observé n'était qu'1
     octet, sur une tuile par ailleurs bit-exacte bloc par bloc).
  **Nouvelle API** `NkVp9EntropyState` (état persistant, possédé par l'appelant) +
  `DecodeKeyFrame(..., entropy, ...)` / `DecodeInterFrame(..., entropy, ..., outMvGrid=nullptr)`
  — signatures étendues, tous les appelants du dépôt mis à jour (`NkVideoReadTest` seul).
  Harnais `--vp9multi <ivf> <ref.yuv>` (NkVideoReadTest) : décode le flux ENTIER (DPB 8 slots
  via `refFrameIdx`/`refreshFrameFlags`, `show_existing_frame` géré, comparaison pixel de
  CHAQUE trame affichée dans l'ordre d'affichage). **Validé BIT-EXACT vs ffmpeg sur 8 flux,
  443 trames inter au total** : `vp9_mov64` (10/10, altref), `vp9_p2test` (20/20, altref
  invisible), `vp9_arf` (**100/100**, gros flux altref réaliste), `vp9_2p` (100/100, 2 GOP/2
  clés), `vp9_hd` (50/50, 1280x720), `vp9_static`/`vp9_odd` (10/10 chacun), **`vp9_seg`
  (50/50, flux segmentation — voir ci-dessous)**. **Régression key-frame confirmée non
  cassée** : 10/10 flux clés toujours bit-exacts (`--vp9recon`), self-tests verts.

  **Segmentation temporelle (`vp9_seg`) — RÉSOLUE dans un 2e passage** (même session,
  après un premier rapport marquant ce cas comme « limite connue »). Deux bugs distincts,
  DIFFÉRENTS de l'hypothèse initiale (« carte de segments non implémentée ») :
  1. **`read_inter_segment_id`/`read_intra_segment_id` absents** — le code ne faisait que
     `segId=0; if (segEnabled && segUpdateMap) segId=lire l'arbre;`, ignorant totalement la
     PRÉDICTION TEMPORELLE (`predicted_segment_id`, lu depuis la carte de la trame
     précédente via un MIN sur l'emprise du bloc — `dec_get_segment_id`) et le cas
     `!segUpdateMap` (copie pure sans lecture de bit). Fix : `NkVp9EntropyState::
     lastFrameSegMap` (carte persistante, sortie via un nouveau paramètre `outSegMap` sur
     `ParseOrDecodeKeyContent`/`ParseOrDecodeInterContent`, miroir de `outMvGrid`) +
     `DecGetSegmentId`/`SetSegmentId`/`CopySegmentId`/`GetPredContextSegId` +
     `ReadIntraSegmentId`/`ReadInterSegmentId` (port fidèle, y compris le bit
     `seg_id_predicted` sous `segTemporalUpdate` avec son propre contexte de proba
     `segPredProbs[above.segIdPredicted + left.segIdPredicted]`).
  2. **Le vrai bloquant du test `vp9_seg`** (diagnostiqué en dumpant les champs d'en-tête :
     `updateMap=0` dès la 1re trame inter) — **`segFeatureEnabled`/`segFeatureData`/
     `segAbsDelta` (deltas de quantizer/filtre par segment) ne PERSISTAIENT PAS entre
     trames** quand `update_data=0` : `ReadSegmentation` ne les réécrit QUE si
     `update_data=1`, sinon `NkVp9FrameHeader` fraîchement construit à chaque trame les
     laissait à leur défaut struct (0/faux), perdant les deltas légitimement signalés par
     la trame clé. **Même classe de bug que `lfRefDeltas`/`lfModeDeltas`** (déjà repéré
     comme non persistant dans une session antérieure, jamais corrigé — à vérifier/fixer
     de la même façon si un flux l'exerce un jour). Fix : `segUpdateData` exposé dans
     `NkVp9FrameHeader`, 3 champs persistés dans `NkVp9EntropyState`, `SyncSegmentFeatures`
     (restaure depuis l'état persistant si `!segUpdateData`, sinon sauvegarde les valeurs
     fraîches) appelée juste après `SetupPastIndependence` dans `DecodeKeyFrame`/
     `DecodeInterFrame`. Reset (comme la carte) inconditionnel dès que
     `SetupPastIndependence` s'exécute (intra-only/error-resilient), PAS soumis à
     `resetFrameContext`.
  Les deux bugs coexistaient et masquaient l'un l'autre au diagnostic superficiel — corriger
  seulement le premier (prédiction temporelle du segment_id) ne suffisait PAS à faire passer
  `vp9_seg` (diff identique avant/après), il fallait les deux.
- **Aucune limite connue restante** sur le décodeur VP9 clé+inter profil 0 4:2:0 8-bit,
  résolution fixe (mêmes dimensions sur toutes les références). **À venir** (hors scope
  actuel, pas des bugs) : résolution scalée des références (tailles différentes par ref, non
  gérée — cas rare) ; profils 1-3 (High/4:4:4/10-12 bits).

- ⭐⭐⭐ **VP8/VP9 BRANCHÉS dans `NkVideoReader`** (2026-07-24) : les deux décodeurs
  complets dormaient sans être accessibles depuis le lecteur haut niveau (seuls les harnais
  dédiés `--vp9recon`/`--vp9multi` les exerçaient). VP8 était en fait DÉJÀ branché depuis une
  session antérieure (`.webm` + `.ivf`) — seul **VP9 restait à câbler**, suivant le même
  patron (`Codec::VP9` ajouté, détection `V_VP9`/`VP90` dans `ParseWebm`/`ParseIvf`).
  **Différence structurelle clé avec VP8/H264** : VP9 encode des **SUPERFRAMES** — un seul
  bloc conteneur (SimpleBlock EBML ou trame IVF) peut contenir jusqu'à 8 sous-trames VP9
  concaténées (Annexe B). `frames[]` (table des blocs bruts, réutilisée telle quelle) ne
  suffit donc plus comme unité de décodage : nouvelle table `vp9Units[]` (une entrée par
  SOUS-TRAME, via `NkVp9Decoder::ParseSuperframe` + `ParseUncompressedHeader` au scan,
  refFrameIdx/refreshFrameFlags/dims mis en cache pour ne jamais reparser pendant `Decode()`)
  + `vp9DisplayUnits[]` (sous-suite des unités AFFICHÉES — `show_frame` OU
  `show_existing_frame` —, en ordre d'affichage). `Impl` porte désormais l'état persistant
  VP9 complet : `NkVp9EntropyState vp9Entropy`, **DPB à 8 slots explicites**
  `NkVp9Image vp9RefSlots[8]` (contrairement à VP8 dont les 3 slots LAST/GOLDEN/ALTREF sont
  gérés EN INTERNE par le décodeur — VP9 laisse `refImages[3]` à la charge de l'appelant),
  suivi d'éligibilité `use_prev_frame_mvs` (`vp9PrevEligibleBase`/`vp9PrevWidth/Height`/
  `vp9MvGrid`, chaînant `outMvGrid`↔`prevMvs` d'un appel à l'autre). Pas de réordonnancement
  d'affichage à gérer (contrairement à H264/POC) : comme VP8, l'ordre d'affichage est une
  sous-suite monotone de l'ordre de décodage — `ReadFrame`/`SeekFrame` n'ont eu besoin
  d'AUCUNE modification, ils retombent déjà sur le chemin générique à curseur (`m->cursor`)
  partagé avec VP8/MJPEG/RAWRGB. `show_existing_frame` (réaffiche un slot déjà décodé, PAS de
  nouvelle décode) traité comme un cas à part, distinct de l'altref invisible VP8.
  **⚠ Bug trouvé PENDANT le branchement** (pas une régression du décodeur lui-même) :
  `vp9PrevEligibleBase` ne vérifiait QUE `!isKeyOrIntraOnly`, oubliant la condition
  `cm->last_show_frame` de la formule `use_prev_frame_mvs` — **exactement le même piège déjà
  trouvé et corrigé UNE FOIS dans le harnais `--vp9multi`** pendant la session précédente,
  mais pas répliqué dans ce nouveau code de branchement (copier-coller incomplet plutôt
  qu'oubli de la règle elle-même). Symptôme : `vp9_hd`/`vp9_2p`/`vp9_p2test` (tous avec des
  trames altref invisibles ou 2 GOP) s'arrêtaient silencieusement à 1/50, 13/100, 2/20 images
  (`ReadFrame` retourne juste `false`, sans message d'erreur) ; `vp9_mov64`/`vp9_arf`/`vp9_seg`
  (sans ce piège dans leur structure) fonctionnaient déjà. Fix : `vp9PrevEligibleBase =
  !vu.isKeyOrIntraOnly && vu.showFrame;`. **Leçon** : un correctif trouvé une fois dans UN
  harnais de test doit être vérifié dans TOUT autre code qui réimplémente la même logique
  (pas de fonction partagée entre `--vp9multi` et `NkVideoReader::Impl::Decode` ici — dette
  à surveiller si un 3e endroit venait à réimplémenter cette éligibilité).
  **Validé BIT-EXACT-ÉQUIVALENT** (maxPixDiff=3, tolérance de conversion YUV→RGBA déjà connue,
  0 image mal ordonnée) vs référence RGBA ffmpeg sur les 8 flux (`mov64`/`arf`/`seg`/`hd`
  testés explicitement, tous les autres passent par le même chemin de code) — IVF ET WebM
  (remux `.webm` de `arf.ivf`, checksum RGBA identique aux deux conteneurs, confirmant le
  partage intégral du décodage) ; `SeekFrame` fonctionne exactement (offset 0 à 4 positions
  testées, dont fin de flux) via le même mécanisme générique que VP8 (pas de logique dédiée
  nécessaire). **Régression confirmée non cassée** : self-tests, VP8 (smoke test 100/100),
  H264 (smoke test 100/100), 10/10 flux clés VP9, 8/8 flux inter VP9 (`--vp9multi`/
  `--vp9recon`) tous toujours verts.

- ⭐ **Persistance `lfRefDeltas`/`lfModeDeltas` (loop filter) — vérifiée et corrigée
  PROACTIVEMENT** (2026-07-24, sur demande explicite après le fix de segmentation temporelle,
  qui avait révélé EXACTEMENT la même classe de bug ailleurs dans le code). Confirmé : même
  défaut que la segmentation — `ReadLoopFilter` ne réécrit `hdr.lfRefDeltas[i]`/
  `lfModeDeltas[i]` QUE si le bit de mise à jour de CETTE entrée est à 1 (imbriqué sous
  `delta_update`), mais `hdr` repart des valeurs par défaut struct (`{1,0,-1,-1}`/`{0,0}`) à
  CHAQUE trame — perdant les deltas légitimement signalés par une trame antérieure et jamais
  re-signalés depuis. **Différence avec la segmentation** : granularité PAR ENTRÉE (4 refs +
  2 modes indépendamment), pas un flag global `update_data` — nécessite un tableau
  `lfRefDeltaUpdated[4]`/`lfModeDeltaUpdated[2]` (nouveau, `NkVp9FrameHeader`) plutôt qu'un
  simple bouléen, et une fonction `SyncLoopFilterDeltas` (miroir de `SyncSegmentFeatures`,
  entrée par entrée : mise à jour cette trame → sauvegarde dans `NkVp9EntropyState` ; pas mise
  à jour → restauration depuis l'état persistant) appelée au même endroit
  (`DecodeKeyFrame`/`DecodeInterFrame`, juste après `SetupPastIndependence`). Reset
  (`set_default_lf_deltas`) sur trame clé/intra-only/error-resilient, même gate que la
  segmentation. **Aucun flux de test actuel n'exerce cette combinaison précise** (deltas
  signalés puis PAS re-signalés sur une trame où ils affectent réellement le filtre) — fix
  appliqué à titre PRÉVENTIF (même risque que la segmentation, coût de fix minime, cohérence
  du modèle de persistance) ; **régression confirmée intégralement non cassée** (les 8/8 flux
  inter + 10/10 flux clés restent bit-exacts, comme attendu si le bug n'était effectivement
  jamais exercé jusqu'ici).

## Décodeur HEVC/H.265 from-scratch — CHANTIER EN COURS (briques 1-10 : INTRA complet + P mono-référence en pixels)

*(2026-07-24)* Démarré comme suite logique directe de H.264 (« évolution directe … la base de
départ la plus naturelle », cf. section précédente) — même famille bit-exacte, complexité
supérieure (CTU/CU/PU/TU au lieu de MB fixes, CABAC uniquement, plus de modes intra, SAO).
Nouveau module `Kernel/Runtime/NKMedia/src/NKMedia/Codecs/Video/HEVC/` (`NkHevcDecoder.h/.cpp`),
zero-STL, `nkentseu::media`.

- ⭐ **Brique 1 livrée et validée** : découpage NAL Annex-B (en-tête **2 octets** contre 1 en
  H.264 : `forbidden_zero_bit(1) + nal_unit_type(6) + nuh_layer_id(6) + nuh_temporal_id_plus1(3)`,
  §7.3.1.2) + parsing structurel `VPS`/`SPS`/`PPS` (§7.3.2.x) via Exp-Golomb — **réutilise
  directement `NkH264BitReader`** (même convention MSB-first `ue(v)`/`se(v)` bit-identique à
  H.264, même anti-émulation `00 00 03`→`00 00`) : pas de duplication de bit reader.
  - `profile_tier_level()` (§7.3.3) implémenté EXACTEMENT (bloc général 88 bits de
    compatibility/constraint flags fixes quel que soit le profil + niveau 8 bits, puis les
    mêmes 88+8 bits par sous-couche si signalés) — nécessaire pour rester synchrone dans le
    RBSP même si seuls `general_profile_idc`/`general_level_idc` sont exposés (pas les
    sous-couches, hors périmètre brique 1 : x265 par défaut ne signale pas de sous-couches
    temporelles, chemin non exercé par les flux de test — noté comme limite connue).
  - `ParseSps` : dimensions (`pic_width/height_in_luma_samples`), profil/niveau, chroma,
    profondeurs de bits, fenêtre de conformance (`conformance_window` — crop en unités
    `SubWidthC`/`SubHeightC`, PAS en pixels directs, §7.4.3.2.1) — s'arrête avant
    `st_ref_pic_set()`/`scaling_list_data()`/`vui_parameters()` (hors scope structurel).
  - `ParsePps` : tuiles, QP init, flags de contrôle CU/tuile — s'arrête avant
    `deblocking_filter_control()`/`scaling_list_data()`/`pps_extension()`.
  - **Validation sur 2 vrais flux ffmpeg/libx265** (`--hevcheader`, harnais diagnostic ajouté à
    `NkVideoReadTest`, comparaison manuelle vs `ffprobe`) :
    1. 322×242 4:2:0 8-bit profil Main (résolution impaire, exerce le crop de conformance) :
       SPS brut 328×248 (aligné CTU), `conf_win` l=0 r=3 h=0 b=3 → crop réel
       `r×SubWidthC=3×2=6` / `b×SubHeightC=3×2=6` → **322×242 EXACT vs `ffprobe`**
       (`width`/`height`/`coded_width`/`coded_height`) ; profil=1 (Main) et niveau=2.0
       (`level_idc`=60=`ffprobe level`) corrects.
    2. 1280×720 4:2:0 10-bit profil Main10, niveau 3.1 : profil=2 (Main10) et niveau=3.1
       (`level_idc`=93) **EXACTS vs `ffprobe`** (`profile=Main 10`, `level=93`), dimensions
       exactes (pas de crop, résolution déjà alignée CTU), profondeur de bits 10/10 correcte.
  - ⭐ **Bug trouvé et corrigé pendant la validation** : le harnais `--hevcheader` affichait le
    niveau `X.YY` via `(level_idc % 30) * 10 / 3` (ex. niveau 3.1 → `level_idc`=93 →
    affichait `3.10` au lieu de `3.1`) — erreur de formule (le chiffre décimal du niveau HEVC
    s'obtient par `(level_idc % 30) / 3`, pas `* 10 / 3` : les niveaux sont espacés de 3 en
    `level_idc` pour chaque incrément de 0.1). Cosmétique (harnais diagnostic uniquement,
    aucun impact sur le parsing lui-même côté `NkHevcDecoder`) mais corrigé immédiatement par
    cohérence — reconfirmé bit-exact sur les 2 flux après fix.
  - `NkHevcDecoder::SelfTest()` (NAL synthétique 2 NALs VPS+SPS pour `SplitNalsAnnexB`, PUIS
    VPS/SPS/PPS/slice IDR RÉELS de libx265 pour valider `ParseVps`/`ParseSps`/`ParsePps` —
    mêmes octets que le flux 322×242 validé vs `ffprobe`) intégré à la suite `NkVideoReadTest`
    sans argument (5/5 self-tests OK : AVI MJPEG, H264, CAVLC, VP9, HEVC).
- ⭐ **Brique 2 livrée et validée** : `slice_segment_header()` (§7.3.6.1), sous-ensemble
  structurel — `first_slice_segment_in_pic_flag`, `no_output_of_prior_pics_flag` (NAL IRAP
  16-23), `slice_pic_parameter_set_id`, `dependent_slice_segment_flag`/`slice_segment_address`
  (si pas la 1re slice), `slice_type` (I/P/B), `pic_output_flag`, `colour_plane_id` (4:4:4
  séparé), et — si PAS IDR — `slice_pic_order_cnt_lsb` + `short_term_ref_pic_set_sps_flag`.
  S'ARRÊTE juste après (avant `short_term_ref_pic_set()`/`ref_pic_lists_modification()`/
  `pred_weight_table()`/deblocking overrides — chacun un chantier propre).
  - `slice_segment_address` a une largeur en bits **dépendante du contenu**
    (`Ceil(Log2(PicSizeInCtbsY))`, §7.4.7.1) → a nécessité d'étendre `ParseSps` avec les champs
    manquants pour dériver `PicSizeInCtbsY` : boucle `sps_sub_layer_ordering_info` (consommée,
    pas exposée) puis `log2_min_luma_coding_block_size_minus3`/`log2_diff_max_min_luma_coding_
    block_size` (nouveaux champs `NkHevcSps::log2MinCbSizeY`/`log2DiffMaxMinCbSizeY`) — Ctb
    LogSize = somme des deux, `PicSizeInCtbsY` = ⌈largeur/CtbSize⌉ × ⌈hauteur/CtbSize⌉.
  - **Validé sur les 2 mêmes flux réels** (`--hevcheader` étendu pour afficher la 1re slice
    ET la 1re slice NON-IDR d'un fichier) :
    - 1re slice (IDR, NAL type 20) : `premiere_dans_image=1 type=I idr=1 poc_lsb=0
      rps_depuis_sps=0` — cohérent (POC/RPS jamais lus pour une IDR, restent aux valeurs par
      défaut de la structure, marqués "valides seulement si !isIdr").
    - 1re slice NON-IDR (NAL type 1 = TRAIL_R, sur les 2 flux) : `type=P idr=0 poc_lsb=4` —
      exerce pour la première fois le chemin `slice_pic_order_cnt_lsb`/`short_term_ref_pic_
      set_sps_flag` (jamais emprunté par une IDR, donc PAS couvert par le self-test synthétique
      seul) ; POC=4 en 2e position de DÉCODAGE est cohérent avec la structure B-pyramid par
      défaut de x265 (`bframes=4` : l'ancre P décode en 2e, les B intermédiaires après,
      affichage 0,1,2,3,4,... ≠ ordre de décodage 0,4,1,2,3,...).
- ⭐ **Brique 3 livrée et validée BIT-EXACT vs oracle `trace_headers` (2026-07-24)** : slice
  header COMPLET jusqu'à `slice_qp_delta` inclus + tous les syntax elements SPS/PPS qu'il
  gate. Oracle de validation : **`ffmpeg -bsf:v trace_headers`** (dump champ par champ du
  parse de référence ffmpeg) — **50/50 slices identiques** (25 par flux : type I/P/B, POC lsb,
  RPS `num_negative/positive_pics`, merge cand, **QP absolu**) sur les 2 flux réels x265.
  Le QP qui matche prouve la synchronisation bit-exacte de TOUT l'en-tête (dernier champ lu).
  - **`short_term_ref_pic_set()`** (§7.3.7) avec la dérivation inter-RPS COMPLÈTE (§7.4.8,
    éq. 7-59 à 7-71 : reconstruction des listes S0/S1 triées par décalage `deltaRps` de celles
    du jeu de référence) — struct `NkHevcShortTermRps` (deltas POC cumulés + flags used).
    Les deux chemins : jeux candidats du SPS (`num_short_term_ref_pic_sets` + index par slice)
    ET RPS inline par slice (cas x265 par défaut : num=0, chaque slice porte le sien —
    `stRpsIdx == numRps`, seul cas où `delta_idx_minus1` est présent).
  - **SPS complété** : TU-tree sizes, `scaling_list_enabled` (refus propre si data présente),
    AMP, **SAO enabled** (gate des flags par slice), PCM, RPS candidats, refs long terme,
    **`sps_temporal_mvp_enabled`**, strong intra smoothing — s'arrête avant VUI (informatif).
  - **PPS complété jusqu'au bout** (hors extensions) : `weighted_pred/bipred` (exposés),
    loop filter across slices, contrôle de déblocage (override/disabled/beta/tc), scaling
    list (refus), `lists_modification_present`, parallel merge level, slice header extension.
  - **Slice header** : RPS inline/index, refs long terme, `slice_temporal_mvp_enabled`,
    SAO luma/chroma, listes de références (override ou défauts PPS), `mvd_l1_zero`,
    `cabac_init`, collocated ref, **`pred_weight_table()` consommée** (§7.3.6.3),
    `five_minus_max_num_merge_cand`, `slice_qp_delta` → QP absolu borné.
  - ⭐ **Piège trouvé à la validation** : premier passage = les 25 slices B passaient mais
    TOUTES les P échouaient — **x265 active `--weightp` par défaut** → `weighted_pred_flag=1`
    → le refus propre initial sur `pred_weight_table()` bloquait toutes les P. Table
    implémentée (consommée, poids pas encore utilisés — brique décodage inter) avec
    l'hypothèse documentée Main/Main10 mono-couche (les flags `luma/chroma_weight_lX_flag`
    sont toujours présents ; la condition §7.3.6.3 qui les ferait sauter n'existe qu'en
    SHVC/SCC). Après fix : 50/50 bit-exact.
- ⭐ **Brique 4 livrée et validée (2026-07-25) : fin du slice header + CABAC initialisé** —
  `NkHevcCabac.h/.cpp` + queue du slice header jusqu'au `byte_alignment()`.
  - **Fin du slice header** : offsets QP chroma par slice, overrides de déblocage (défauts
    PPS puis override), `slice_loop_filter_across_slices`, **points d'entrée tuiles/WPP**
    (§7.4.7.1 — x265 par défaut = WPP via `entropy_coding_sync`), extension d'en-tête,
    `byte_alignment()` **vérifié STRICTEMENT** (bit à 1 obligatoire + zéros : excellent
    détecteur de désynchronisation — 1 champ mal lu au-dessus = 1 chance sur 2 d'échouer) →
    expose `dataByteOffset` (départ de `slice_data()` dans le RBSP dé-émulé, exposé aussi
    via le nouveau `NkHevcDecoder::DeemulateRbsp` public). ⚠ Les offsets de points d'entrée
    sont dans le domaine NAL ÉMULÉ (les octets 00 00 03 comptent, §7.4.7.1) — conversion à
    faire à la brique WPP.
  - **CABAC (§9.3)** : le MOTEUR arithmétique est STRICTEMENT identique à H.264 (mêmes
    tables 9-44/9-45, même init 510/9 bits, mêmes DecodeDecision/Bypass/Terminate) →
    **`NkCabacEngine` de NkH264Cabac.h réutilisé TEL QUEL** (déjà validé bit-exact via le
    décodage H.264 Main/High complet). Spécifique HEVC fourni : init des contextes §9.3.2.2
    (`initValue` 8 bits → (m,n) → même formule `NkCabacInitOne` qu'H.264), choix d'initType
    par slice (I→0 ; P→cabac_init?2:1 ; B→cabac_init?1:2), et **tables d'initValues 3×179**
    (enum d'offsets CHAÎNÉS `kHevcCtx*` — zéro arithmétique manuelle).
  - ⭐ **Anti-piège « table normative mal transcrite » (leçon H.264 ×5) appliqué en amont** :
    les 3×179 initValues sont alignées sur la référence ffmpeg (`libavcodec/hevc/cabac.c`,
    téléchargée) et **cross-checkées NUMÉRIQUEMENT** par script (extraction des deux tables
    depuis les deux fichiers sources + diff élément par élément) : **537/537 valeurs
    identiques**. Le self-test embarque les sommes de contrôle par initType.
  - **Validation** : `NkHevcCabacState::SelfTest()` (7/7 suite) = layout 179 contextes +
    sommes vs ffmpeg + formule d'init (spot-checks calculés à la main : 154→équiprobable,
    200@QP33→état 14) + **moteur known-answer** (init/8 bypass/4 decisions/terminate sur
    octets fixes, valeurs attendues calculées par une implémentation Python INDÉPENDANTE de
    l'algorithme de la spec). Sur flux réels : **50/50 slices** avec en-tête complet jusqu'à
    l'alignement, init CABAC valide partout (offset 9 bits < 510), et **nombre de points
    d'entrée WPP IDENTIQUE à l'oracle `trace_headers`** (25×3 pour 322×242 = 4 rangées de
    CTU 64 ; 25×11 pour 1280×720 = 12 rangées — cohérence géométrique confirmée).
- ⭐ **Brique 5 livrée et validée STRUCTURELLEMENT (2026-07-25) : syntaxe CTU INTRA complète**
  (`NkHevcCtu.cpp`, `ParseSliceDataIntra`) — la TOTALITÉ de `slice_segment_data()` d'une
  slice I est décodée au-dessus du CABAC : `sao()` (merge/type/offsets/bande/contour),
  `coding_quadtree` (split_cu avec contexte des profondeurs voisines), `coding_unit` intra
  (part_mode 2N×2N/N×N, **modes luma réels via la dérivation MPM §8.4.2** — indispensables
  car le SCAN des résidus 4×4/8×8 dépend du mode intra —, mode chroma DM/table),
  `transform_tree` (split contexté, cbf luma/chroma par profondeur), `transform_unit`
  (cu_qp_delta_abs TR+EG0 par groupe de quantification) et **`residual_coding` COMPLET**
  (last_sig prefixes/suffixes, sous-blocs 4×4 avec `coded_sub_block_flag` contexté par
  voisins, `sig_coeff_flag` avec les cartes de contexte composées par scan, greater1
  (jeux de contextes + compteur c1)/greater2, signes bypass avec **sign hiding**, restes
  **Rice/EGk** avec mise à jour du paramètre sur le NIVEAU COMPLET). **WPP intégral** :
  conversion des entry points du domaine NAL émulé vers le RBSP dé-émulé (positions des
  00 00 03 enregistrées à la dé-émulation), ré-init moteur par rangée, **restauration des
  contextes sauvés après le 2e CTB de la rangée au-dessus** (§9.3.1).
  - Tables de scan **GÉNÉRÉES par le procédé normatif §6.5.3** (diag up-right 2×2/4×4/8×8 +
    inverses + raster/composé horizontal) — vérifiées IDENTIQUES aux tables littérales de
    ffmpeg par script AVANT écriture du C++ (zéro risque de transcription). Dérivations de
    contexte des résidus alignées sur `libavcodec/hevc/cabac.c` (validé bit-exact).
  - **Validation structurelle type tiles VP9** : sur les 2 slices I réelles x265,
    **24/24 CTU** (6×4, 322×242) et **240/240 CTU** (20×12, 720p Main10) consommés avec
    `end_of_slice_segment_flag` UNIQUEMENT au dernier CTU et `end_of_subset_one_bit`=1 à
    CHAQUE fin de rangée (4 et 12 rangées), écart consommation vs entry points ≤ 1 octet
    (prélecture moteur attendue). À travers ~16 Ko de bins, un seul bin mal décodé
    n'importe où aurait désynchronisé les terminaisons — probabilité de faux positif
    infinitésimale. Stats : 650/2106 CU, 628/1503 TU, 2970/7360 coeffs non nuls,
    60/219 cu_qp_delta (cohérent avec `--qg-size 32` par défaut de x265).
  - SPS/PPS complétés au passage (champs exposés au lieu de consommés) : tailles d'arbre TU
    (`log2MinTbSizeY`/`log2DiffMaxMinTbSizeY`), profondeurs TU inter/intra,
    `diff_cu_qp_delta_depth`, `transquant_bypass_enabled`.
- ⭐⭐ **Brique 6 livrée et validée PIXELS BIT-EXACTS vs ffmpeg (2026-07-25) : reconstruction
  INTRA complète** — `DecodeSliceIntra` (même moteur que la brique 5, plus la reconstruction
  par TU, activée quand une `NkHevcFrame` est fournie). **10/10 trames BIT-EXACTES du
  premier coup** sur 2 flux intra-only x265 encodés SANS déblocage/SAO (méthode H.264 : la
  reconstruction pure d'abord, les filtres en boucle en brique suivante) : 5 trames 322×242
  8-bit + 5 trames 1280×720 **Main10** (10-bit), pire écart 0 sur ~14 Mo d'échantillons
  (comparaison Y/Cb/Cr recadrés conformance vs `ffmpeg -f rawvideo`).
  - **QP par groupe de quantification** (§8.6.1) : `qPY_PRED` = moyenne des voisins
    gauche/dessus (disponibles seulement dans le MÊME CTB, sinon `qPY_PREV` = QP du dernier
    CU du QG précédent, reset SliceQpY en début de slice ET de rangée WPP), delta signé,
    formule modulaire avec `QpBdOffsetY` (12 en Main10) ; **QP chroma** via offsets PPS/slice
    + table 4:2:0 (30..43 → `kQpC`) + `QpBdOffsetC`.
  - **Déquant** (§8.6.3) : `levelScale[qP%6] << (qP/6)`, m=16 (pas de scaling lists),
    `bdShift = bitDepth + log2 − 5`, écrêtage 16 bits, arithmétique 64 bits.
  - **Transformées inverses** (§8.6.4) : DST-VII 4×4 (luma intra) + DCT 4-32 par
    sous-échantillonnage de la matrice 32×32 normative (transcrite de la référence ffmpeg),
    deux passes (colonnes shift 7, lignes shift 20−bitDepth) avec écrêtage 16 bits
    intermédiaire. ⚠ Piège attrapé À LA RELECTURE (avant tout test) : matrice DST
    transposée + formule modulaire du QP avec un offset de trop (sans effet en 8-bit,
    faux en 10-bit) — vérifiés contre les papillons TR_4x4_LUMA de ffmpeg.
  - **Prédiction intra 35 modes** (§8.4.4.2) : échantillons de référence par disponibilité
    RÉELLE (cartes « TU reconstruit » séparées luma/chroma, granularité 4×4 — l'ordre
    z/TU natif du décodage donne exactement la disponibilité normative), substitution
    §8.4.4.2.2 (scan bas-gauche→coin→droite), lissage [1 2 1] + **lissage fort bilinéaire
    32×32** (seuil `1<<(bd−5)`), Planar/DC (+ filtre de bord DC luma), angulaire (tables
    d'angles/angles inverses + extension de référence par projection, filtres de bord des
    modes 10/26) — port fidèle de `pred_template.c` (validé bit-exact).
  - Reconstruction PAR TU dans l'ordre de décodage (prédiction TOUJOURS, résidu si cbf ;
    chroma du cas 4×4 au blkIdx 3), signes réellement appliqués (y compris **sign hiding** :
    signe caché = parité de la somme des niveaux du sous-bloc).
- ⭐⭐ **Brique 7 livrée et validée BIT-EXACT vs ffmpeg (2026-07-25) : déblocage + SAO
  d'application** — les flux STANDARD (filtres activés par défaut) décodent maintenant
  pixel-parfait, y compris les 2 flux de test historiques (`test_hevc.265`/`test_hevc_hd10.265`)
  qui servent depuis la brique 1 et qui ACTIVENT ces filtres.
  - **Déblocage** (§8.7.2, BS=2 partout car décodeur 100% intra) : cartes d'arêtes TU sur
    grille 8×8 (luma) / 8×8 chroma (marquées pendant `transform_tree`, jamais les arêtes 4×4
    internes), 2 passes IMAGE ENTIÈRE (toutes les verticales, PUIS toutes les horizontales —
    lit les échantillons déjà filtrés verticalement, ordre normatif), tables `tC`/`β`
    (Table 8-12, décalées `<<(bitDepth−8)`), décision forte/faible par segment de 4 lignes,
    filtre chroma (QP moyen des blocs luma co-localisés, offset PPS SEULEMENT — l'offset de
    slice ne s'applique PAS au déblocage, piège classique évité en amont).
  - **SAO** (§8.7.3) : paramètres capturés PENDANT le parsing CABAC de `sao()` (merge
    left/up propage la structure du CTB voisin), bande (table de 32 offsets indexée par les
    5 bits de poids fort + position) et contour (4 classes de direction, table `edge_idx`,
    échantillon hors image = non modifié). **Appliqué sur une COPIE de l'image DÉBLOQUÉE**
    (source figée, jamais la destination en cours d'écriture — sinon un CTB contaminerait
    son voisin non encore traité).
  - **Validation** : les 2 flux historiques **redeviennent utiles** — 1re trame de chacun
    bit-exacte vs `ffmpeg -f rawvideo` (mêmes fichiers de référence que la validation
    `--hevcheader`/`trace_headers` depuis la brique 1) — PLUS 2 nouveaux flux intra-only
    générés cette fois AVEC filtres par défaut (5×322×242 8-bit + 5×1280×720 Main10) :
    **10/10 + 2/2 trames bit-exactes**, non-régression confirmée sur les flux SANS filtres
    de la brique 6 (10/10 toujours OK). Réussi dès le premier build.
- ⭐⭐⭐ **Brique 8 livrée et validée (2026-07-25) : syntaxe CU INTER structurelle complète
  (skip/merge/AMVP), sur les slices P ET B réelles** — `ParseSliceDataIntra` (nom historique
  gardé) accepte désormais I/P/B en mode PARSE SEUL (`frame == nullptr`) ; la reconstruction
  pixel (`DecodeSliceIntra`) reste I-only (brique suivante = MC). Couvre : `cu_skip_flag`
  (contexte voisins gauche/dessus, carte dédiée), `pred_mode_flag` (CU intra MIXÉ dans une
  slice P/B, cas réel rencontré et géré), `part_mode` inter complet (2Nx2N/2NxN/Nx2N/NxN/AMP —
  `DecodePartMode`, port fidèle de `ff_hevc_part_mode_decode`), `prediction_unit()` par PU
  (merge_flag/merge_idx, ou AMVP : `inter_pred_idc` (B seulement, contexte par profondeur CU),
  `ref_idx_lx` — **1 SEUL jeu de contextes partagé L0/L1** (vérifié dans ffmpeg : `ref_idx_l1`
  n'est JAMAIS référencé, reproduit à l'identique), `mvd_coding()` (greater0/greater1 + préfixe
  unaire bypass + suffixe + signe — la longueur est elle-même dans le flux, pas
  précalculable), `mvp_lx_flag` (1 contexte partagé L0/L1)), `rqt_root_cbf` (inféré à 1 sans
  lecture pour 2Nx2N+merge).
  ⭐ **Aucune dérivation de candidats merge/AMVP ni résolution de MV n'est nécessaire à ce
  stade** : ces calculs ne changent AUCUN bit consommé (seule leur VALEUR servirait à la MC,
  future brique) — insight qui a permis de scoper cette brique à la seule SYNTAXE.
  - ⭐⭐ **2 bugs trouvés et corrigés — spécifiques à `transform_tree` pour CU INTER**, invisibles
    tant qu'on ne teste que l'intra (briques 5-7) : (1) **split de transformée FORCÉ implicite**
    quand `max_transform_hierarchy_depth_inter == 0` ET le CU inter n'est PAS 2Nx2N ET
    profondeur 0 (`inter_split` de la référence ffmpeg) — jamais lu comme bit, la transformée
    DOIT alors s'aligner sur les limites de PU ; (2) **`cbf_luma` INFÉRÉ à 1** (pas lu) pour un
    CU inter à profondeur 0 SANS cbf chroma. Les deux découverts par **bissection empirique**
    (traces CU-par-CU comparant la position d'octet consommée) après qu'une relecture manuelle
    exhaustive du code n'ait rien trouvé — la vraie cause n'est apparue qu'en comparant
    directement au code source de `hls_transform_tree` (ffmpeg), pas en re-dérivant depuis la
    spec. Corrigé aussi au passage : `max_transform_hierarchy_depth_INTER` était utilisé nulle
    part (toujours la variante intra) — sans effet dans NOS flux de test (les deux valent 0)
    mais latent pour d'autres flux.
  - ⭐ **Bug préexistant (brique 6) découvert en creusant** : `StartQuantGroup` lisait `qpMap`
    (alloué SEULEMENT si `frame`) SANS le garder derrière `if (frame)` — lecture hors-borne
    silencieuse en Release (indice hors-bornes toléré par UB, aucun effet sur le nombre de
    bits consommés donc jamais remarqué), mais **assert immédiat en build Debug**. Trouvé en
    lançant le harnais sous `gdb`/build Debug pour CETTE brique — corrigé (gate `if (frame)`).
    Leçon : **valider aussi en Debug** (asserts actifs), pas seulement en Release.
  - **Validation** : les 2 flux réels (322×242 + 1280×720 Main10, 25 slices chacune, GOP mixte
    I/P/B avec AMP désactivé, WPP actif) passent **25/25 en structurel**, en Debug (asserts) ET
    Release. **Non-régression pixel confirmée sur TOUTES les briques précédentes** (6/7) :
    10/10 + 10/10 trames intra bit-exactes (sans et avec filtres), 2/2 flux historiques
    bit-exacts — les fixes de `transform_tree` ne touchent QUE le chemin inter, zéro
    changement pour l'intra.
- ⭐ **Brique 9 livrée et validée (2026-07-25) : POC réel + construction RefPicList0/1**
  (bookkeeping pur, AUCUNE image manipulée) — `NkHevcDecoder::ComputePoc`/`BuildRefPicLists`,
  fonctions statiques autonomes réutilisables telles quelles par le futur branchement
  `NkVideoReader` (même split architectural que H.264 (`h264Dpb`)/VP9 (`vp9RefSlots[8]`),
  confirmé par relecture des deux avant d'écrire cette brique : le décodeur bas niveau ne
  stocke JAMAIS d'image, il reçoit des références déjà résolues — ici on prépare juste la
  liste des POC à résoudre, la résolution POC→pixels sera le rôle du lecteur).
  - `ComputePoc` (§8.3.1, PicOrderCntVal) : cas simplifié mono-couche (`TemporalId` toujours
    0, pas de sous-couches — vrai pour tous nos flux x265, `maxSubLayersMinus1=0`) : IDR→0 ;
    sinon dérivation `PicOrderCntMsb` par comparaison au POC complet de la dernière image
    décodée (`prevPocTid0`, mis à jour par l'appelant après CHAQUE image), avec le
    rebasculement ±`MaxPicOrderCntLsb` normatif en cas de wraparound du LSB.
  - `BuildRefPicLists` (§8.3.2/8.3.4) : `PocStCurrBefore/After` = POC courant + deltas déjà
    résolus du RPS (brique 3, `usedS0`/`usedS1`) ; listes temporaires L0=before+after,
    L1=after+before (B seulement) ; **repli MODULO normatif** si `NumPicTotalCurr <
    num_ref_idx_active` (ex. juste après l'IDR : 1 seule réf disponible mais 3 demandées →
    répétition `[POC,POC,POC]`, comportement spec-correct, pas un bug).
  - **Validation** : self-test (IDR→0, cas réels de nos flux `poc_lsb=4`→POC4/`poc_lsb=2`
    après→POC2, wraparound synthétique `lsb 14→2` = `POC 14→18`, RefPicList P avec repli
    modulo, RefPicList B avant/après) + **sur les 2 flux réels (25 slices chacun)** : structure
    de GOP en pyramide B EXACTEMENT plausible — I(POC0)→P(POC4,L0=[0])→B(POC2,L0=[0],L1=[4])
    →... jusqu'à B(POC22,L0=[21,19],L1=[23,24]) en fin de GOP, voisinage POC cohérent partout.
    25/25 + 25/25 slices OK (Debug asserts + Release), **non-régression totale confirmée**
    (7/7 self-tests, pixels intra 10+10+1+1 trames toujours bit-exacts).
- ⭐⭐⭐ **Brique 10 livrée et validée BIT-EXACT vs ffmpeg (2026-07-25) : slices P mono-référence
  en PIXELS** — `NkHevcDecoder::DecodeSliceP` (`num_ref_idx_l0_active==1` exigé, refus propre
  sinon) : dérivation des VECTEURS DE MOUVEMENT réels + compensation de mouvement, scopées
  volontairement à la 1re P après l'IDR (candidat temporel naturellement indisponible —
  aucune trame précédente n'a de champ de MV stocké, ffmpeg non plus à cet endroit précis).
  - **Fusion spatiale** (§8.5.3.2.2) : positions A1/B1/B0/A0/B2, exclusions de partition
    (2e PU des partitions empilées horizontalement/verticalement), élagage anti-doublon
    (paires EXACTES du spec : B1~A1, B0~B1, A0~A1, B2~{A1,B1}), repli MV nul. **AMVP spatial**
    (§8.5.3.2.6/7) : groupe gauche A0 sinon A1, groupe haut B0 sinon B1 sinon B2, dédoublonné —
    réf UNIQUE cette brique donc AUCUNE mise à l'échelle temporelle possible (tout voisin
    inter dispo utilise nécessairement la même réf, la passe "non mise à l'échelle" du spec
    suffit toujours). Voisinage (§6.4.1) simplifié EXACTEMENT (pas une approximation, vérifié
    par dérivation) pour le cas mono-tuile/mono-slice : `cand_left=(x0>0)`, `cand_up=(y0>0)`,
    `cand_up_left=(x0>0 && y0>0)` ; `cand_up_right_sap`/`cand_bottom_left` gardent leur
    dépendance exacte aux limites de CTB/image.
  - **Compensation de mouvement** (§8.5.4.2.2, port fidèle de `h2656_inter_template.c`) :
    interpolation qpel luma 8 taps / epel chroma 4 taps séparables (4:2:0 : `mv&7` directement,
    `hshift=vshift=1` → aucune mise à l'échelle de fraction supplémentaire), précision
    intermédiaire 14-bit (2 passes H puis V, `>>6` après chaque filtre), pondération explicite
    si `pps.weightedPred` (§8.5.3.3.4.2/8.5.4.2.3, poids/offsets déjà résolus depuis la
    brique 3). Échantillonnage hors-image = étendu par bord (clamp direct dans l'échantillonneur,
    équivalent à `emulated_edge_mc` sans buffer intermédiaire). `mvd_coding()` : formule de
    valeur EGk (k=1) dérivée par concordance de consommation de bits avec la brique 8
    (`abs_mvd = (1<<k) + suffixe`).
  - ⭐ **Bug trouvé et corrigé — disponibilité intra cassée pour un voisin INTER** : une P-slice
    peut mélanger CU intra et inter (`pred_mode_flag`), mais `lumaRecon`/`chromaRecon` (cartes
    de disponibilité §8.4.4.2.1 consommées par `PredictIntra`) n'étaient marquées QUE par le
    chemin intra depuis la brique 6 — un voisin inter fraîchement reconstruit par MC restait vu
    comme ABSENT par une CU intra adjacente, déclenchant la substitution/padding à tort (blocs
    plats à valeur constante erronée, symptôme observé : `maxdiff=127/128` alors que les valeurs
    de poids semblaient plausibles). Trouvé en isolant le bug via un flux SANS pondération
    (contrôle) qui échouait À L'IDENTIQUE, prouvant que la pondération n'était pas en cause,
    puis en localisant les pixels fautifs (aplats constants = signature classique de
    substitution intra). Fix : `ApplyMotionCompensation` marque désormais `lumaRecon`/
    `chromaRecon` sur le rectangle PU, comme le fait `MarkLumaRecon`/`MarkChromaRecon` côté intra.
  - **Validation** : 4 flux ffmpeg/libx265 dédiés (générés dans cette brique, `bframes=0:ref=1`,
    SANS déblocage/SAO — reconstruction PURE comparée, même précédent que la brique 6 avant la
    brique 7) — petit (96×64, 3 coeffs), riche+AMP (320×240, 20 CTU, 801 coefficients non
    nuls), et 2 flux à fondu de luminosité (160×128) l'un SANS l'autre AVEC pondération
    explicite (`weightp=1`, confirmé "Weighted P-Frames: Y:100.0% UV:100.0%" par x265) :
    **4/4 trames P bit-exactes vs `ffmpeg -f rawvideo`**, Debug (asserts) ET Release,
    **non-régression totale** (intra briques 6-7 : 10/10 toujours bit-exact ; structurel
    briques 1-5+8-9 : 25/25 + 25/25 slices OK sur les 2 flux réels ; 7/7 self-tests).
- ⭐⭐⭐ **Brique 11+12 livrées et validées BIT-EXACT vs ffmpeg (2026-07-26) : P multi-référence
  + candidat TEMPOREL AMVP/merge, chaînage P-sur-P complet**. Brique 11 (multi-référence L0,
  commit `85932da3`) avait laissé un chaînage P-sur-P divergent, dont la cause a été identifiée
  (cf. entrée précédente historique ci-dessous) grâce à un **cross-check indépendant par
  mini-parseur HEVC Python** (CABAC + navigation CU transcrits depuis `ffmpeg_hevc_cabac.c`,
  aucun code partagé) qui a prouvé que le CABAC/mvd était déjà correct — la vraie cause étant
  l'ABSENCE du **candidat temporel** (§8.5.3.2.8/9), en réalité requis dès la 2e trame P (pas
  seulement une amélioration future) dès que la référence est elle-même une trame inter avec
  un vrai champ de MV stocké.
  - **Champ de MV persistant** (`NkHevcFrame::mvColX/Y/RefPoc/Valid`, par bloc 4×4) : rempli
    par `DecodeSliceP`/`DecodeSliceIntra` juste après reconstruction de CHAQUE trame, consommé
    par la trame SUIVANTE qui l'utilise comme "collocated picture" (`refsL0[collocated_ref_idx]`,
    toujours L0 : ce décodeur ne traite que du P). La compression normative à 16×16 (note
    §8.3.1) n'a pas besoin d'être implémentée explicitement : la position d'échantillonnage
    est TOUJOURS ré-alignée sur 16 avant indexation, donc garder le champ complet 4×4 donne un
    résultat identique. Vide pour une trame I (aucun MV) : candidat temporel authentiquement
    indisponible en la référençant, sans cas particulier à coder.
  - **`DeriveTemporalCand`** (§8.5.3.2.8/9) : position bas-droite du PU d'abord (repliée si hors
    CTB courant ou hors image), sinon position centrale ; bloc INTRA côté colPic → candidat
    indisponible. Mise à l'échelle via `ScaleMv` (déjà écrit brique 11, vérifié exact vs
    `ffmpeg_hevc_mvs.c::mv_scale`) avec `td=colPic.poc-colRefPoc`, `tb=curPoc-targetPoc`.
  - **Câblage merge** (§8.5.3.2.1) : ajouté APRÈS les 4 spatiaux (A1/B1/B0/A0 + B2 conditionnel),
    AVANT le repli MV nul, toujours vers `refsL0[0]` (`refIdxL0Col=0` normatif). **Câblage
    AMVP** (§8.5.3.2.6) : ajouté seulement si les 2 candidats spatiaux ne suffisent pas déjà,
    mis à l'échelle vers le refIdx cible signalé par le bitstream.
  - **Validation** : chaînage complet (`--hevcinter`, DPB minimal + vraies listes de référence
    résolues) sur 4 flux dédiés — **3/4 flux BIT-EXACTS bout-en-bout** (37 trames P chaînées
    au total, dont le cas hevc_p_test qui divergeait depuis la brique 11), Debug (asserts) ET
    Release, **non-régression totale** (intra briques 6-7 : 4/4 flux toujours bit-exacts).
    Contrôle négatif : désactiver le candidat temporel fait RÉGRESSER (pas disparaître) la
    divergence — passe de 9/14 à 1/14 bit-exact sur le 4e flux, confirmant que le candidat
    temporel corrige bien un vrai manque et n'est pas la cause du reste.
- ⭐⭐⭐ **Brique 13 livrée et validée BIT-EXACT vs ffmpeg (2026-07-26) : voisin `CandUpRight`
  hors-image sur les CU forcé-scindés en bord droit d'image** — root cause du reste laissé par
  la brique 12 (flux `hevc_p_wp.265`, divergence chroma dès la 10e trame P). Diagnostic mené en
  4 étapes : 1) tracé de tous les PU inter de la trame fautive (instrumentation temporaire) pour
  localiser le PU couvrant le pixel qui diverge → CU `(128,64,32,32)` merge idx=0 mv=(0,0) ; 2)
  vérification manuelle (extraction directe des pixels de la référence YUV brute + application
  à la main de la formule de pondération explicite `§8.5.3.3.4.3`) confirmant que la formule et
  le pixel source `mv=(0,0)` étaient individuellement corrects mais que le VRAI MV attendu était
  `(0,-8)` — les lignes de la trame cible correspondent EXACTEMENT à la trame précédente décalée
  d'1 ligne chroma (donc -8 en 1/4-pel luma), pas à un décalage nul ; 3) analyse du voisinage
  spatial merge/AMVP pour ce CU précis : `CandUpRight(x0=128,y0=64,nPbW=32)` — `x0b+nPbW=32 ≠
  ctb(64)` (le CU ne touche pas un bord de CTB, car il est plus PETIT que son CTB nominal 64×64
  suite à un split FORCÉ par la fenêtre d'image, la colonne de CTU 128-159 n'ayant que 32 pixels
  valides sur 160) → la branche générique retournait `y0>0` = **vrai**, SANS jamais vérifier que
  `x0+nPbW=160` est justement `== picW` (donc **hors image**) ; 4) le voisin fantôme B0 en
  `(160,63)` indexait dans le champ de MV via `px=160>>2=40`, exactement égal à `minPuWidth`
  (40 pour une image de 160 px) — un ALIASING SILENCIEUX (`idx = py*minPuWidth+px` déborde
  d'exactement une colonne, retombant sur `(px=0, py+1)`) qui renvoie un MV plausible mais FAUX
  au lieu de crasher ou d'être détecté par les asserts Debug.
  - **Fix** : `CandUpRight` vérifie maintenant `x0+nPbW>=picW || y0<=0` en tout premier (candidat
    hors image = indisponible, §6.4.1), avant même la logique d'alignement CTB — la branche
    générique devient alors un simple `return true` (le `y0>0` est déjà garanti par le nouveau
    garde-fou). Symétrique de la vérification déjà présente et correcte dans `CandBottomLeft`.
  - **Validation** : **4/4 flux P dorénavant BIT-EXACTS bout-en-bout** (51 trames P chaînées au
    total), Debug (asserts) ET Release, non-régression totale (intra 4/4 flux toujours
    bit-exacts). Le bug était latent dans les 3 autres flux aussi (même condition générique de
    voisinage) mais invisible par coïncidence de contenu/partitionnement (jamais sélectionné par
    `merge_idx`/`mvp_lx_flag` à ces endroits précis) — donc un vrai risque resté caché, pas
    spécifique à la pondération explicite qui n'a fait que l'exposer en premier. Détail complet :
    mémoire `project_nkmedia_hevc_p_multiref_bug`.
- ⭐⭐⭐ **Brique 14 livrée et validée BIT-EXACT vs ffmpeg (2026-07-26) : slices B (bi-prédiction)
  en PIXELS** — `NkHevcDecoder::DecodeSliceB` (RefPicList0 ET RefPicList1 résolues par l'appelant).
  Le PARSING B était déjà fait depuis la brique 8 (inter_pred_idc, refIdx/mvd/mvp L1,
  pred_weight_table L0+L1, collocated) ; cette brique ajoute la RECONSTRUCTION complète.
  - **Champ de MV bi-liste** : `MvField` complet (predFlag L0/L1, mv[2], refIdx[2]) par bloc 4×4,
    persisté dans `NkHevcFrame::mvColL1X/Y` + `mvColRefPocL0/L1` + `mvColPredFlag` (le champ
    colocalisé porte les DEUX listes + leurs POC pour la mise à l'échelle temporelle cross-trame).
  - **Merge bi** (§8.5.3.2.2-4) : candidats spatiaux (MvField complet du voisin, élagage
    `CompareMvRefIdx` sur les listes actives), temporel 2-passes L0/L1, **candidats combinés
    bi-prédictifs** (table `kHevcL0L1CandIdx[12][2]`, appariement l0/l1 de POC/MV distincts),
    candidats nuls bi, règle 8×4/4×8 (BI→L0 si nPbW+nPbH==12).
  - **Candidat temporel bi-liste** (§8.5.3.2.8/9) : `DeriveTemporalColocatedMv(listX,refIdx)` avec
    sélection `collocated_from_l0`, `check_diffpicount` (low-delay), `check_mvset`/`mv_scale`.
  - **AMVP par liste** (§8.5.3.2.6/7) : `DeriveAmvpLx` — chaque voisin peut fournir le candidat via
    SA liste L0 OU L1 (`MpMx`/`MpMxLt`, match sur POC pointé), `mvd_l1_zero`.
  - **MC bi** (§8.5.3.3.3/4) : intermédiaires 14-bit par liste (`ComputeInterp`), combinaison
    non pondérée `(predL0+predL1+64)>>7` ou pondérée explicite (`pps.weightedBipred`,
    `log2Wd`/w0/w1/o0/o1). Le chemin uni P validé bit-exact n'est PAS touché (généralisation
    `FinalizeSample`/`ApplyMotionCompensation` par (listX, refIdx), identique pour L0 en 8-bit).
  - **Validation** : 7 flux B dédiés (b-pyramid, multi-réf L0+L1, `weightb=1` pondération bi, AMP,
    no-temporal-mvp) — **toutes les trames B bit-exactes** (maxdiff=0), Debug (asserts) ET Release.
    **Non-régression totale** : P 4/4, intra 4/4 toujours bit-exacts. Harnais `--hevcinter` étendu
    (décode P ET B, résolution POC→pointeurs L0/L1). Implémentée dans un worktree isolé puis
    ré-validée intégralement dans le dépôt principal avant intégration.
- ⭐⭐⭐ **Brique 15 livrée et validée BIT-EXACT vs ffmpeg (2026-07-26) : déblocage in-loop INTER
  (Boundary Strength §8.7.2.4) + SAO branché sur P et B** — jusqu'ici déblocage/SAO n'étaient
  appliqués qu'aux slices I ; maintenant I, P ET B sont filtrées (gate final `sliceType==kHevcSliceI`
  → `if (frame)`, le déblocage restant gaté par `deblockingFilterDisabled` et le SAO par
  `saoLuma/saoChroma` normatifs).
  - **Dérivation BS §8.7.2.4** (`DeriveDeblockBs`, transcription de `ff_hevc_deblocking_boundary_
    strengths`/`boundary_strength`) : aux frontières TU par segment 4×4 — `bs=2` si un côté intra
    (`predFlag==0`), sinon `bs=1` si cbf luma non nul d'un côté, sinon dérivé du MV (`bs=1` si
    refs — comparées par POC POINTÉ, pas refIdx — diffèrent ou `|Δmv|>=4` en 1/4-pel, sinon 0).
    **Cas bi-prédiction** (`BoundaryStrengthMv`, croisement L0/L1) désormais couvert grâce au champ
    `MvField` bi de la brique 14. Frontières PU internes d'un TU inter traitées SANS le test cbf.
  - **Nouvelles cartes 4×4** : `bsVert`/`bsHoriz` (BS 0/1/2 par bloc) remplacent les cartes d'arêtes
    booléennes ; `cbfLuma4` (cbf luma par min-TU). `DeriveDeblockBs` appelée aux feuilles de
    `transform_tree` (après cbf) ET sur CU skip / CU inter sans résidu (`rqtRootCbf==0`).
  - **Consommation** : `LumaDeblockSeg` prend `bs` → `tc = kTcTable[qp + 2*(bs-1) + tcOff]` (le `+2`
    intra codé en dur devient `2*(bs-1)`) ; décision strong/weak et beta inchangés. Luma filtré si
    bs≥1, **chroma uniquement si bs==2** (comme ffmpeg — le chroma lit les mêmes cartes BS luma).
    Ordre normatif conservé (toutes arêtes verticales puis toutes horizontales).
  - **Validation** : 7 flux dédiés AVEC filtres — P déblocage seul, P déblocage+SAO (dont 320×240),
    B déblocage seul, B déblocage+SAO, B `weightb`+AMP+pyramide (exerce le cas BS bi) — **tous
    bit-exacts** (maxdiff=0), Debug (asserts) ET Release. **Non-régression totale** : intra 4/4,
    P/B SANS filtres tous bit-exacts. Implémentée en worktree isolé puis ré-validée intégralement
    dans le dépôt principal.
- **Suite (brique 16, PAS commencée)** : branchement `NkVideoReader` — Phase A `.265` Annex-B I+P+B
  (nouveau `ParseHevcAnnexB` + détection ES brut, chemin curseur ou réordonnancement POC pour B),
  puis Phase B MP4 (`hvc1`/`hev1` + box `hvcC` — `ParseHvcCBytes` distinct de l'avcC) et MKV
  (`V_MPEGH/ISO/HEVC`). DPB réel avec éviction par RPS §8.3.2. Un VRAI fichier x265 (B + déblocage
  + SAO par défaut) est désormais décodable correctement. Règle CU 8×8 forcé-2Nx2N si
  `log2ParallelMergeLevel>2`. Restes mineurs (refus propre en place) : tuiles, PCM, 4:2:2/4:4:4,
  `ref_pic_lists_modification()`, `scaling_list_data()`, bit depth >8 pour l'inter (10-bit MC pas
  encore branché).

## Reste à faire — synthèse (MAJ 2026-07-26)

Le CŒUR de NKMedia est **fonctionnellement complet** : lire/décoder/jouer les formats du monde réel
(MP4/MKV/WebM/3GP/TS/FLV/AVI en **H.264/HEVC/VP8/VP9** + **AAC/Opus/MP3**) est bit-exact et branché
dans `NkVideoReader`/`NkVideoPlayer`. **HEVC est désormais lisible bout-en-bout** (.265/MP4-hvc1/MKV,
brique 16 livrée 2026-07-26). Ce qui reste, par ordre d'utilité réelle :

1. **Perf temps-réel** (le gros chantier restant si objectif lecteur fluide HD/4K) : les décodeurs
   sont scalaires. **H.264 : 1re passe d'optim livrée (2026-07-26)** — MC luma/chroma chemin rapide
   sans-clamp (template `if(CLAMP)`) + SSE2 chroma + dédup bS déblocage → **décodeur ×1,25 (−20% de
   cycles), ~38→45 fps en 720p**, bit-exactness préservée (19/19 flux octet-identiques). ⚠️
   Découverte : l'artefact « H264 lent » venait d'une copie profonde du DPB **dans le harnais de
   test** ; le vrai chemin `NkVideoReader` utilise déjà `NkMove` (pas de copie). Levier restant #1 =
   CABAC + parsing coefficients (~51% du décodage, bit-serial — risqué). **HEVC : décodeur ~×2,5
   cumulé vs baseline scalaire.** Passe 1 (2026-07-26) : ×2,17 (−54% cycles, ~67→148 fps 720p) via
   NkMove/NkCopy sur les copies (snapshot SAO valait 35% du décodage !) + chemins MC sans-clamp.
   Passe 2 (2026-07-26) : **SIMD SSE2/AVX2 (dispatch runtime) sur la MC 8-tap luma/4-tap chroma
   séparable** → **−14% de cycles/frame supplémentaires** (720p, mesuré `QueryThreadCycleTime`).
   IDCT/SAO restés SCALAIRES par choix délibéré (IDCT accumule en int64 pour éviter un dépassement
   int32 sur DCT-32/QP extrêmes non couverts par les tests ; SAO = ROI plus faible + gather
   data-dépendant coûteux à vectoriser correctement) — « en cas de doute, garde le scalaire ».
   **Bit-exact préservé sur les 17 flux de test, Release ET Debug, aux deux passes.** ⭐ Fix moteur
   transversal appliqué : **`NkVector<T>` trivialement-copiable copie désormais via `NkCopy`
   (memcpy AVX2)** au lieu de PushBack élément-par-élément (Kernel/Foundation/NKContainers) —
   bénéficie à tout code copiant de gros vecteurs POD, pas seulement HEVC. Restant : SIMD IDCT/SAO
   (int64, prudence dépassement), CABAC (risqué), déblocage luma, multithread.
2. **HEVC — features de bord restantes** (INVÉRIFIABLES faute d'oracle x265, ou refactor lourd —
   pas des bugs) : tuiles, 4:2:2/4:4:4, PCM (code dormant écrit), `ref_pic_lists_modification`,
   `scaling_list_data`, CU 8×8 `log2ParallelMergeLevel>2`. **10-bit inter (Main10) ✅ livré.**
3. **Nouveaux codecs (optionnels, chacun un chantier dédié, TOUS from-scratch)** :
   **Theora/OGV ✅ livré (2026-07-26)** — `NkTheoraDecoder` from-scratch (VP3, conteneur Ogg réutilisé) :
   **INTRA keyframes 25/25 BIT-EXACT** (vérifié) + **INTER luma bit-exact tous modes** + INTER chroma
   bit-exact SAUF sous-mode `INTER_MV_FOUR` (arrondi MV chroma 4:2:0, ~1% pixels chroma concernés,
   documenté). iDCT VP3 entière bit-exacte, filtre de boucle, 80 arbres Huffman. Harnais `--theora`.
   ⚠️ Oracle limité : le décodeur Theora natif de ffmpeg est bogué sur l'inter libtheora → inter validé
   sur flux courts isolés. **AV1 : reconstruction PIXEL intra key frame LIVRÉE (2026-07-26)** — `NkAv1Decoder` from-scratch :
   OBU parsing + sequence/frame headers + symbol decoder CDF §8.2 (fondation) **+ partition tree
   complet + tous les modes intra (DC/8 angles directionnels/Smooth×3/Paeth/filter-intra/CfL) +
   décodage coefficients (txb_skip/eob/coeff_base/coeff_br golomb/dc_sign) + transformées inverses
   (DCT/ADST/IDTX/WHT lossless, 4×4→32×32) + déquantification + déblocage §7.14 — BIT-EXACT vs
   ffmpeg sur 6 flux divers** (couleur plate, lossless WHT+palette, lossy DCT/ADST+déblocage,
   contenu complexe, dimensions non alignées superbloc). Tables normatives (127, scan/CDF/quant/tx)
   transcrites **mécaniquement depuis le TEXTE de la spec AOMedia** (`NkAv1Tables.inc`, script
   dédié) — pas du code tiers, même pratique que les tables DCT/quant déjà transcrites pour
   H264/HEVC/VP9. Bug latent trouvé+corrigé : `UpdateCdf` (adaptation CDF, sens inversé). Harnais
   `--av1` compare les pixels automatiquement. Restes (refus propre, non exercés par les tests) :
   couleurs de palette, Intra Block Copy, CDEF, loop restoration, superres, tout l'INTER.
   ⛔ **AMR-NB/WB : NON FAISABLE from-scratch, constat définitif (2026-07-26, 2 tentatives)** —
   1re tentative : port déguisé d'opencore-amr (Apache-2.0) détecté et rejeté (violait le
   from-scratch + introduisait une licence tierce). 2e tentative : implémentation VRAIMENT from-
   scratch (pipeline ACELP complet écrit depuis TS 26.090, aucun code tiers lu ni copié, vérifié) —
   **structurellement correcte mais numériquement du bruit (corrélation ≈ -0,011, pas de la
   parole)**. Cause RACINE, pas un manque d'effort ni de temps : contrairement à H264/HEVC/AV1/
   VP9/Theora/MPEG-2 (spec **textuelle** ITU-T/ISO/AOMedia publiant les tables séparément du code),
   **la spec normative 3GPP AMR (TS 26.073) EST le code source de référence — aucun texte
   indépendant ne publie les tables** (dictionnaire LSF split-VQ, dictionnaire de gains, filtre
   d'interpolation pitch). Sans ces tables exactes, aucune implémentation from-scratch ne peut
   produire un décodeur fonctionnel, quelle que soit la présentation du code (renommer des
   variables/restructurer du code copié reste une œuvre dérivée, ça ne « blanchit » rien). Rejeté
   définitivement, aucun code AMR dans le dépôt. Ne PAS retenter sous la même contrainte from-
   scratch — seule voie possible : obtenir légitimement les tables normatives licenciées (décision
   de projet à part, hors périmètre from-scratch actuel).
   **MPEG-2 vidéo ✅ livré (2026-07-26)** — `NkMpeg2Decoder` (I/P/B, DPB forward+backward, demi-pel,
   réordonnancement B) : **bit-exact sur contenu flat/basse-fréquence, ±1 sur haute-fréquence**
   (tolérance de conformité IDCT IEEE-1180 permise par la norme — 4/25 trames I à maxdiff=0, toutes
   les autres ≤1). Refusés proprement : entrelacé, table VLC B-15 (`intra_vlc_format=1`), 4:2:2/4:4:4,
   quantif non-linéaire (chemin écrit, non validé faute de flux ffmpeg). Harnais `--mpeg2`. ⚠️ Bug
   latent identifié : table `kAcLevel` incomplète dans `NkMpeg1Tables.cpp` (inoffensif — l'encodeur
   MPEG-1 n'émet jamais les runs 28-31).
4. **Expansion encodeur (optionnel)** : profils H.264 avancés au-delà du baseline déjà livré.

*(Muxer WebM ✅ + MPEG-2 décode ✅ livrés 2026-07-26.)*

*(Historique — MAJ 2026-07-19 : Opus/CELT+SILK ✅, AAC-LC stéréo ✅ (CPE/M/S/IS/PNS/TNS, corr
1.000000 vs ffmpeg), décodeur H264 Main+High COMPLET bit-exact avec déblocage ✅, NkVideoReader avec
réordonnancement POC ✅ — voir « Livré ».)*

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
    - **H.265/HEVC** : **DÉMARRÉ (2026-07-24)** — brique 1 (structure NAL/VPS/SPS/PPS) livrée
      et validée vs ffprobe, voir section dédiée ci-dessus. Reste : CABAC, quadtree CTU/CU/PU/TU,
      35 modes intra, inter merge/AMVP, transformées DST/DCT variables, déblocage+SAO.
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
