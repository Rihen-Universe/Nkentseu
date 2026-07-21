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
| 4. **UI graphique (NKGui + NKEditorKit)** | ⬜ | à construire — voir plan détaillé ci-dessous |
| 5. Seek H264 (scrubber) | 🚫 | **bloquant pour l'UI** : `NkVideoReader::SeekFrame` ne fonctionne PAS pour H264 (bug pré-existant, voir Bugs) |

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
3. **Binding position ↔ scrubber** : `SliderFloat` lu/écrit sur `mediaClock` (actuellement
   piloté par `streamPlayer.GetPositionSeconds()`) — glisser le curseur doit appeler
   `syncAudioTo(nouvelle_position)` + `reader.SeekFrame(...)`, **mais voir blocage ci-dessous**.
4. **Plein écran / redimensionnement** : déjà géré côté rendu (letterbox `sprite.SetScale`),
   l'UI NKGui doit juste se recaler à la taille fenêtre (`ctx` connaît déjà la taille via
   NKGui backend, cf. démos NKGui existantes).

**⚠️ BLOQUANT identifié (2026-07-21) — le scrubber (glisser pour avancer/reculer) NE PEUT
PAS fonctionner tant que `NkVideoReader::SeekFrame` reste cassé pour H264** : la fonction
ne touche QUE `mImpl->cursor` (champ utilisé par les codecs MJPEG/RAWRGB/séquences), alors
que le chemin H264 utilise un état de réordonnancement POC totalement séparé
(`h264DecodeCursor`/`h264OutCount`/`h264Reorder`/`h264GopBase`, voir `NKMedia/Video/
NkVideoReader.cpp`) que `SeekFrame` n'initialise jamais. Aujourd'hui, appeler `SeekFrame`
sur un flux H264 ne fait RIEN de perceptible (juste un champ mort) — seul le seek au
DÉMARRAGE (position 0, état déjà vierge) et le rebouclage (`SeekFrame(0)` en fin de
lecture, même raison : état déjà proche de 0) fonctionnent PAR COÏNCIDENCE. Un vrai seek
H264 nécessite : trouver l'IDR précédant la cible (table `stss`/keyframes déjà extraite au
probe), réinitialiser `h264DecodeCursor` à cet IDR, vider `h264Reorder`/`h264ReorderKey`,
réinitialiser `h264GopBase`/le DPB, puis décoder en avance jusqu'à la cible (coût = distance
à l'IDR précédent, potentiellement plusieurs dizaines de frames sur un GOP long — normal,
tous les lecteurs H264 ont ce comportement). **À corriger AVANT ou EN MÊME TEMPS que le
scrubber UI** (sinon glisser la barre ne fera rien, régression perçue immédiatement par
l'utilisateur). Documenté aussi dans `Kernel/Runtime/NKMedia/ROADMAP.md`.

**Ordre de travail suggéré** : (a) fixer `SeekFrame` H264 d'abord (prérequis dur, sinon
UI à moitié fonctionnelle) → (b) barre de contrôle NKGui minimale (play/pause/stop/volume/
temps, PAS de scrubber tant que (a) n'est pas fait) → (c) scrubber une fois (a) livré →
(d) polish (auto-hide, plein écran, playlist si besoin via NKEditorKit docking).

## Bugs
- 🚫 **`NkVideoReader::SeekFrame` cassé pour H264** (bloquant pour le scrubber UI ci-dessus) —
  voir description complète ci-dessus et dans `Kernel/Runtime/NKMedia/ROADMAP.md`.

## Dépendances
`NKMedia` (lecture vidéo), `NKAudio` + `NKAudio/Streaming` (audio streamé), `NKCanvas`/`NKRHI`
(affichage texture), `NKWindow`/`NKEvent` (fenêtre/clavier). Futur : `NKGui` (contrôles),
`NKEditorKit` (chrome/docking, optionnel).
