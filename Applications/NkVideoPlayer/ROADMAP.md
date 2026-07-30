# NkVideoPlayer — lecteur audio/vidéo de référence — ROADMAP

> Application de démonstration/référence pour `NKMedia` (lecture) + `NKAudio/Streaming`
> (audio streamé) + `NKCanvas` (affichage). Sert aussi de patron pour l'agent NKCode.
> Actuellement **100% clavier**, sans UI graphique — voir plan UI ci-dessous (demande
> Rihen, 2026-07-21) : porter sur **NKGui + NKEditorKit** pour une interface façon VLC.

| Brique | Statut | Contenu |
|---|---|---|
| 1. Lecture vidéo | ✅ | `NkVideoReader` (MJPEG/AVI/MOV/MP4 H264 Main+High) → `NkTexture` |
| 2. Sync A/V | ✅ | horloge maîtresse = audio streamé ; rattrapage à budget de temps ; **0 décalage mesuré** (voir NKMedia/ROADMAP.md) |
| 3. Contrôles clavier | ✅ | pause/stop/seek pas-à-pas/vitesse/boucle/étalonnage couleur/LUT |
| 4. Seek H264 | ✅ | `NkVideoReader::SeekFrame` fonctionne désormais pour H264 (voir NKMedia/ROADMAP.md) — **plus de bloquant pour le scrubber UI** |
| 5. Resynchronisation active | ✅ | retard \>1,5s → saut direct via SeekFrame au lieu de rattraper image par image |
| 6. **UI graphique (NKGui + NKEditorKit)** | ⬜ | à construire — voir plan détaillé ci-dessous |

## Livré
- **Lecteur clavier complet** (`src/main.cpp`) : `NkVideoReader` → `NkTexture` (NKCanvas),
  audio streamé (`ContainerAudioStream` + `AudioStreamPlayer`, voir `[[project_nkaudio_streaming_player]]`),
  horloge maîtresse audio, rattrapage vidéo à budget de temps (`NkChrono`, 150ms/tick),
  étalonnage couleur (6 presets) + LUT `.cube` trilinéaire, contrôles : espace=pause,
  S=stop, flèches=seek pas-à-pas, haut/bas=vitesse, C=grade, U=LUT, L=boucle.
- **Sync A/V robuste (2026-07-21)** : deux bugs de copie profonde (au lieu de move) trouvés
  et corrigés dans `NKMedia/Video/NkVideoReader.cpp` (DPB + buffer de réordonnancement POC) —
  lag vidéo/audio mesuré à **0 en continu**. Détail complet : `Kernel/Runtime/NKMedia/ROADMAP.md`
  section « Bugs / limitations connues » + mémoire `[[project_nkmedia_video_reader]]`.
- **`NkVideoReader::SeekFrame` réparé pour H264 (2026-07-21)** : localise l'IDR précédant la
  cible et repositionne l'état de réordonnancement POC. Validé par `NkVideoReadTest --seektest`.
  Débloque le scrubber UI (ci-dessous) ET permet la resynchronisation active.
- **Resynchronisation active (2026-07-21, demande explicite Rihen : "jamais de décalage, robuste
  à tout moment")** : si le retard vidéo/audio dépasse ~1,5s de contenu, le lecteur saute
  directement au voisinage de la cible via `SeekFrame` (coût = distance au GOP) plutôt que de
  laisser le rattrapage frame-par-frame lutter indéfiniment contre un débit de décodage
  insuffisant. Complète (ne remplace pas) le rattrapage à budget de temps normal.
  ⚠️ **Honnêteté** : ceci borne le décalage maximal observable (auto-correction après ~1,5s de
  retard cumulé) — ce n'est PAS une preuve mathématique de débit temps réel garanti pour tout
  contenu/matériel (un décodeur H264 scalaire ne peut pas l'offrir), mais un filet de sécurité
  actif qui rattrape au lieu de laisser diverger. Testé 20s sans crash sur film réel (chemin de
  resync non déclenché sur ce contenu — le débit normal suffit déjà après les fix perf).

## En cours / À venir

### ⭐ Plan UI graphique NKGui + NKEditorKit (demande Rihen, 2026-07-21) — façon VLC

**Objectif** : remplacer les contrôles 100% clavier par une vraie interface graphique
(barre de contrôle : lecture/pause/stop, barre de progression cliquable/glissable, volume,
vitesse, boucle, étalonnage/LUT, plein écran), dans l'esprit VLC — sans réinventer un
framework UI : **NKGui** (immediate-mode, widgets `Button/SliderFloat/ProgressBar/Image/
ImageButton/ColorButton` déjà livrés dans `Kernel/Runtime/NKGui/src/NKGui/Widgets/
NkGuiWidgets.h`) pour les contrôles, **NKEditorKit** (`Engine/NKEditorKit`, coquille
éditeur partagée — chrome de fenêtre, docking, cf. `[[project_nkeditorkit]]`) pour
l'ossature de fenêtre/layout si un habillage plus riche qu'une simple barre est voulu
(playlist latérale, panneau d'info piste, etc. — optionnel, v1 peut rester une simple
overlay NKGui par-dessus la surface vidéo).

**Découpage proposé** :
1. **Surface vidéo** : la texture existante (`frameTex`, mise à jour via `pushFrame()`)
   s'affiche via le widget `NKGui::Image(ctx, texId, w, h)` au lieu du sprite NKCanvas nu
   — ou les deux coexistent (NKCanvas pour le rendu vidéo plein cadre, NKGui en overlay
   pour les contrôles par-dessus, pattern déjà utilisé ailleurs dans le moteur pour les
   HUD/overlays NKGui sur rendu NKCanvas/NKRHI).
2. **Barre de contrôle** (bas d'écran, auto-masquée après quelques secondes d'inactivité
   façon lecteurs vidéo standards) : `ButtonEx`/`ImageButton` play-pause-stop, `SliderFloat`
   pour la position (0..durée) ET pour le volume, affichage temps courant/durée totale,
   bouton boucle, menu déroulant vitesse (0.1x..4x, déjà supporté côté moteur), menu
   étalonnage (6 presets + toggle LUT, déjà supporté côté moteur).
3. **Binding position ↔ scrubber** : `SliderFloat` lu/écrit sur `mediaClock` — glisser le
   curseur appelle `syncAudioTo(nouvelle_position)` + `reader.SeekFrame(...)`. **Prêt** :
   `SeekFrame` fonctionne désormais pour H264 (livré ci-dessus), plus de blocage technique.
   ⚠️ Le seek reste une APPROXIMATION granularité-GOP (repart de l'IDR précédente, pas d'un
   accès frame-exact instantané) — comportement normal pour tout lecteur H264, à refléter dans
   l'UX (ex. léger délai visible en glissant loin d'une image clé, comme dans VLC/YouTube).
4. **Plein écran / redimensionnement** : déjà géré côté rendu (letterbox `sprite.SetScale`),
   l'UI NKGui doit juste se recaler à la taille fenêtre (`ctx` connaît déjà la taille via
   NKGui backend, cf. démos NKGui existantes).

**Ordre de travail suggéré** (le préalable SeekFrame étant réglé) : (a) barre de contrôle NKGui
minimale (play/pause/stop/volume/temps) → (b) scrubber (position + drag-seek) → (c) polish
(auto-hide, plein écran, playlist si besoin via NKEditorKit docking).

## Dépendances
`NKMedia` (lecture vidéo), `NKAudio` + `NKAudio/Streaming` (audio streamé), `NKCanvas`/`NKRHI`
(affichage texture), `NKWindow`/`NKEvent` (fenêtre/clavier). Futur : `NKGui` (contrôles),
`NKEditorKit` (chrome/docking, optionnel).
