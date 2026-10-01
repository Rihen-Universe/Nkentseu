#pragma once
// -----------------------------------------------------------------------------
// @File    NkTimelineModel.h
// @Brief   LA FRISE — pistes par objet et par propriete, images-cles, courbes,
//          lecture. Le composant que les pages Animation des editeurs partagent.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (2026-10-01, feuille de route d'Unkeny R21/R22/R35)
// =============================================================================
//  Rihen : « on doit avoir la page Animation ou l'on met les keyframes et tout ».
//  Le depot avait le MODELE d'une frise sans interface (NKAnima,
//  `Edit/NkAnimationEditor.h` : poses-cles d'un squelette) et deux interfaces
//  maison : la « Timeline » de NkAnimaEditor (des ronds sur une ligne, une seule
//  piste) et le sequenceur de Noge (des canaux sans dessin). Aucune frise ne se
//  PARTAGEAIT. Celle-ci vit dans le kit pour qu'UnkenyEditor, NkAnimaEditor et
//  Nogee peignent la meme, a la maniere de la Dope Sheet et du Curve Editor
//  d'Unreal 5 (spec. NkAnimaEditor/design/06 §5 et §6).
//
// =============================================================================
//  CE QUI SE PARTAGE, CE QUI RESTE CHEZ L'HOTE
// =============================================================================
//  Le kit ne tire NI NKAnima NI NKECS (NKEditorKit.jenga) : il ne sait pas ce
//  qu'est un clip, un composant ou une entite. Il porte donc :
//    - LE MODELE D'EDITION : des pistes de cles (temps, jusqu'a 4 valeurs, une
//      interpolation), la selection multiple, l'accrochage aux images, le
//      curseur et la lecture, le zoom, et ANNULER / REFAIRE ;
//    - LE DESSIN ET LES GESTES : feuille d'images (losanges) et editeur de
//      courbes, regle, transport, colonne des pistes avec leurs valeurs.
//  L'hote garde le SENS : il remplit les pistes (une piste = une propriete
//  qu'il sait lire et ecrire), relit le modele quand `changed` part, et ECRIT
//  les valeurs dans ses objets. Deux points de greffe le relient au dessin :
//    - `evaluer`   : l'interpolation de l'HOTE (NKAnima chez Unkeny), pour que
//                    la courbe peinte soit celle que le jeu jouera. Sans elle,
//                    le modele interpole lui-meme (lineaire, paliers) ;
//    - `lireValeur`: la valeur VIVANTE de la propriete (« ◆ » d'une piste pose
//                    une cle de ce que l'objet montre, comme Unreal).
//
//  ⚠️ LES CODES D'INTERPOLATION SONT CEUX DE NKAnima (anim::NkInterpMode), dans
//     le meme ordre : l'hote passe de l'un a l'autre par un simple transtypage.
//     Le kit ne peut pas inclure NKAnima pour le verifier ; l'hote le fait
//     (static_assert dans UnkenyEditor, NkEditeurPageAnimation.cpp).
//
// =============================================================================
//  OU AJOUTER QUELQUE CHOSE
// =============================================================================
//   • une LONGUEUR      -> `kMetrics` de `NkTimelineDecl()`, jamais un litteral
//   • une COULEUR       -> `kTokens` + un champ de `NkTimelineStyle`
//   • une INTERPOLATION -> `NkTimelineInterp` ET NKAnima (meme ordre) + son nom
//   • un EVENEMENT      -> `kEvents` + un champ de `NkTimelineResult`
//   • un GENRE de valeur-> `NkTimelineValueKind` (et son pas de frottement)
//
// =============================================================================
//  LA FRISE « UNREAL 5 / BLENDER » (2026-10-01 au soir, demande de Rihen)
// =============================================================================
//  « le style actuel ne me plait pas : inspire-toi d'Unreal 5 ou de Blender ».
//  Le dessin suit le Sequencer d'UE5 et la Dope Sheet / le NLA de Blender :
//    - colonne des pistes en ARBRE repliable (« Bras/Main » est sous « Bras »,
//      lui-meme sous l'objet anime), une icone par genre, et par ligne les
//      boutons ◁ ◆ ▷ (cle precedente, cle au curseur, cle suivante), muet,
//      solo et verrou ; « + Piste » en tete ;
//    - regle a deux etages : les SECONDES en haut, les IMAGES en bas, la
//      PLAGE DE LECTURE (poignees verte et rouge) et les MARQUEURS ;
//    - cles dont la FORME dit l'interpolation (carre = palier, losange =
//      lineaire, rond = courbe, losange arrondi = douceur, hexagone =
//      dynamique), sections teintees sous les cles, barres de PALIER ;
//    - pistes de CLIPS (NLA de Blender, sections d'animation d'UE5) : des
//      clips poses bout a bout, qui se chevauchent, avec leurs fondus ;
//    - editeur de courbes avec POIGNEES DE TANGENTE (auto, unifiee, cassee,
//      plate), comme le Curve Editor d'UE5 ;
//    - boite de TRANSFORMATION des cles choisies (ses bords mettent a
//      l'echelle), copier / coller, barre de defilement du bas dont les bords
//      zooment, zoom a la molette ADOUCI.
//  Rien de l'ancien modele n'est retire : les champs et methodes d'avant
//  gardent leur sens, les nouveaux s'ajoutent.
//
//  CE QU'IL NE FAIT PAS : il n'a pas de clavier (`NkComponentInput` n'en porte
//  aucun). Suppr, Espace, Ctrl+Z sont a l'hote, qui appelle `DeleteSelection`,
//  `TogglePlay`, `Undo`. Il ne nomme pas les proprietes : « + Propriete » est
//  RAPPORTE (`addTrackRequested`), l'hote ouvre son propre choix.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		/// L'interpolation d'une cle vers la suivante. MEME ORDRE QUE
		/// anim::NkInterpMode (NKAnima) -- voir l'en-tete.
		enum class NkTimelineInterp : uint8 {
			Palier = 0,	 ///< la valeur tient jusqu'a la cle suivante
			Lineaire,	 ///< droite
			Courbe,		 ///< Catmull-Rom (NKAnima) ; le repli du kit la trace lineaire
			Entree,		 ///< acceleration au depart
			Sortie,		 ///< deceleration a l'arrivee
			EntreeSortie,
			Rebond,
			Elastique,
			Recul,
			Count
		};
		/// Le nom court d'une interpolation (barre d'outils, infobulles).
		const char *NkTimelineInterpName(uint8 interp);

		/// Comment une piste se LIT : elle decide des couleurs des canaux, du pas
		/// de frottement et de l'interpolation par defaut.
		enum class NkTimelineValueKind : uint8 {
			Nombre = 0, ///< un ou plusieurs nombres (position, rotation, echelle)
			Couleur,	///< rgba 0..1 : une pastille de la couleur dans la colonne
			Palier,		///< booleen, entier, enumeration, reference d'asset : pas de glissement
			/// (01/10 soir) Une piste de CLIPS non lineaires (NLA de Blender,
			/// sections d'animation d'UE5) : pas de cles, des `clips`.
			Clips,
			Count
		};

		/// La TANGENTE d'une cle « Courbe » (Curve Editor d'UE5). MEME ORDRE QUE
		/// anim::NkTangentMode (NKAnima).
		enum class NkTimelineTangent : uint8 {
			Auto = 0, ///< calculee (Catmull-Rom sur les temps), comme NKAnima
			Unifiee,  ///< posee a la main ; entree = sortie (poignees alignees)
			Cassee,	  ///< posee a la main ; entree et sortie independantes
			Plate,	  ///< pente nulle (un sommet ou un creux qui ne deborde pas)
			Count
		};
		const char *NkTimelineTangentName(uint8 tangent);

		struct NkTimelineKey {
				float32 time = 0.f;
				float32 v[4] = {0.f, 0.f, 0.f, 0.f};
				uint8 interp = (uint8)NkTimelineInterp::Lineaire;
				/// La selection est PORTEE PAR LA CLE : elle survit au tri qui suit un
				/// deplacement (un indice, lui, aurait change de cle).
				bool selected = false;
				/// (01/10 soir) La tangente (lue par l'interpolation Courbe) et, quand
				/// elle est posee a la main, ses PENTES par canal (valeur par seconde).
				uint8 tangent = (uint8)NkTimelineTangent::Auto;
				float32 tanIn[4] = {0.f, 0.f, 0.f, 0.f};
				float32 tanOut[4] = {0.f, 0.f, 0.f, 0.f};
				/// L'identite que l'HOTE y range (NkAnimaEditor : le rang d'origine
				/// d'une pose-cle). Le kit la recopie sans la lire.
				nk_uint64 user = 0;
		};

		/// UN CLIP pose sur une piste de clips (NLA). Le kit ne sait pas ce qu'il
		/// joue : `name` est resolu par l'hote, `sourceLength` est sa duree propre.
		struct NkTimelineClip {
				nk_uint64 id = 0;
				NkString name;
				float32 start = 0.f;		///< debut sur la frise (s)
				float32 length = 1.f;		///< duree sur la frise (s)
				float32 sourceLength = 1.f; ///< la duree propre du clip
				float32 offset = 0.f;		///< le temps du clip au debut de la section (rognage)
				float32 rate = 1.f;			///< vitesse de lecture
				float32 blendIn = 0.f;		///< fondu d'entree (s)
				float32 blendOut = 0.f;		///< fondu de sortie (s)
				float32 weight = 1.f;		///< influence
				bool loop = true; ///< au-dela de sa duree : boucle, sinon tient la derniere pose
				bool selected = false;
				float32 End() const {
					return start + length;
				}
		};

		/// Un MARQUEUR de la regle (les « Marked Frames » d'UE5, les marqueurs de
		/// Blender) : un temps et un nom.
		struct NkTimelineMarker {
				nk_uint64 id = 0;
				float32 time = 0.f;
				NkString name;
				bool selected = false;
		};

		struct NkTimelineTrack {
				/// L'IDENTITE, opaque au composant et stable. `0` est reserve.
				nk_uint64 id = 0;
				/// Le GROUPE : l'objet dont c'est une propriete (« » = l'objet anime
				/// lui-meme, « Bras/Main » = un descendant). Les pistes d'un meme
				/// objet se suivent sous une ligne d'en-tete qui resume leurs cles.
				NkString object;
				/// La propriete, « Composant.champ » : la cle que l'hote relit.
				NkString property;
				/// Le libelle montre (vide = `property`).
				NkString label;
				NkTimelineValueKind kind = NkTimelineValueKind::Nombre;
				uint8 channels = 1; ///< 1..4 valeurs par cle
				/// Le nom de chaque canal (« X », « Y »...). Vide = « X Y Z W » ou « R G B A ».
				NkString channelNames[4];
				/// Triees par temps.
				NkVector<NkTimelineKey> keys;
				/// Montree dans l'editeur de courbes, et chacun de ses canaux.
				bool showCurve = true;
				bool showChannel[4] = {true, true, true, true};
				/// (01/10 soir) MUET : l'hote ne l'applique pas (`TrackActive`).
				bool muted = false;
				/// SOLO : des qu'une piste est solo, seules les solos jouent.
				bool solo = false;
				/// VERROU : ses cles et ses clips ne se choisissent ni ne bougent.
				bool locked = false;
				/// Piste de CLIPS (kind == Clips) : ses clips, tries par debut.
				NkVector<NkTimelineClip> clips;
				/// Piste de clips : son influence, son mode (additif = s'ajoute a ce
				/// qui est dessous, sinon le remplace) et son MASQUE (nom donne par
				/// l'hote, vide = tout le corps / toutes les proprietes).
				float32 weight = 1.f;
				bool additive = false;
				NkString mask;
		};

		/// Une cle COPIEE : sa piste et son temps relatif a la premiere copiee.
		struct NkTimelineClipboardKey {
				nk_uint64 track = 0;
				NkTimelineKey key;
		};

		/// Un INSTANTANE pour Annuler / Refaire : les pistes et la duree. Les
		/// frises d'une animation tiennent en quelques centaines de cles : copier
		/// tout est plus sur que de journaliser chaque geste (et c'est ce que fait
		/// NkAnimationEditor, en plus fin, pour ses poses).
		struct NkTimelineSnapshot {
				NkVector<NkTimelineTrack> tracks;
				float32 duration = 1.f;
				NkVector<NkTimelineMarker> markers;
				float32 rangeStart = 0.f, rangeEnd = -1.f;
		};

		// ── LE MODELE ───────────────────────────────────────────────────────────
		struct NkTimelineModel {
				NkVector<NkTimelineTrack> tracks;
				float32 duration = 1.f; ///< secondes ; la frise se grise au-dela
				float32 fps = 30.f;
				float32 cursor = 0.f; ///< secondes
				bool playing = false;
				bool loop = true;
				float32 speed = 1.f;
				bool curveMode = false; ///< faux = feuille d'images, vrai = courbes
				bool snap = true;		///< accrocher les temps aux images
				/// La fenetre de temps visible (zoom et defilement).
				float32 viewStart = -0.05f;
				float32 viewEnd = 1.1f;
				/// La fenetre de VALEURS de l'editeur de courbes. `curveAutoFit` la
				/// recale sur les cles visibles a chaque dessin, tant qu'on n'a pas
				/// zoome a la main.
				float32 valueMin = -1.f;
				float32 valueMax = 1.f;
				bool curveAutoFit = true;
				int32 rowScroll = 0;	 ///< la premiere ligne montree de la colonne
				nk_uint64 activeTrack = 0; ///< la piste choisie dans la colonne (0 = aucune)
				/// Les objets REPLIES (leurs pistes ne sont pas montrees).
				NkVector<NkString> collapsed;
				/// (01/10 soir) Le nom de la racine de l'arbre (vide = « (objet anime) »).
				NkString rootLabel;
				/// Incremente a CHAQUE changement des cles : l'hote relit le modele
				/// quand il a bouge (en plus de `NkTimelineResult::changed`).
				uint32 revision = 0;

				// --- (01/10 soir) La plage de lecture, les marqueurs, les clips -------
				/// La PLAGE DE LECTURE (poignees verte et rouge de la regle) : la
				/// lecture boucle dedans. `rangeEnd < 0` = la duree.
				float32 rangeStart = 0.f;
				float32 rangeEnd = -1.f;
				NkVector<NkTimelineMarker> markers;
				nk_uint64 nextMarkerId = 1;
				nk_uint64 nextClipId = 1;
				/// La piste de clips et le clip choisis (inspecteur de l'hote).
				nk_uint64 activeClip = 0;
				/// Le presse-papiers des cles (Copier / Coller).
				NkVector<NkTimelineClipboardKey> clipboard;
				/// L'HOTE TIENT LES CLES (NkAnimaEditor : des poses-cles de tout le
				/// squelette). Vrai : « + Cle », « ◆ », le double-clic, Suppr.,
				/// Coller, Annuler et Refaire ne touchent plus le modele, ils sont
				/// RAPPORTES (`keyRequested`, `deleteRequested`...) ; deplacer et
				/// interpoler restent faits ici, l'hote relit.
				bool hostKeys = false;
				bool hostCanUndo = false, hostCanRedo = false;
				/// Le ZOOM ADOUCI : la molette pose une cible, la vue la rejoint en
				/// quelques images (`GlideStep`).
				float32 viewTargetStart = 0.f, viewTargetEnd = 0.f;
				bool viewGlide = false;

				// --- Annuler / Refaire ---------------------------------------------
				NkVector<NkTimelineSnapshot> undoStack;
				NkVector<NkTimelineSnapshot> redoStack;
				uint32 undoLimit = 64;

				// --- Le geste en cours (etat du composant, l'hote n'y touche pas) ----
				uint8 gesture = 0; ///< NkTimelineGesture
				bool gestureMoved = false;
				bool gestureSnapshot = false;
				float32 gestureX0 = 0.f, gestureY0 = 0.f;
				float32 gestureT0 = 0.f, gestureV0 = 0.f;
				nk_uint64 gestureTrack = 0;
				int32 gestureKey = -1;
				int32 gestureChannel = -1;
				nk_uint64 gestureClip = 0;	   ///< le clip tenu (pistes de clips)
				nk_uint64 gestureMarker = 0;   ///< le marqueur tenu
				bool gestureTangentOut = true; ///< la poignee tenue : sortie (vrai) ou entree
				float32 gestureA = 0.f, gestureB = 0.f, gestureC = 0.f; ///< valeurs d'origine du geste
				NkVector<float32> gestureTimes;	 ///< temps d'origine des cles choisies
				NkVector<float32> gestureValues; ///< valeurs d'origine (courbes : le canal tenu)
				float32 gestureViewStart = 0.f, gestureViewEnd = 0.f;
				float32 gestureValueMin = 0.f, gestureValueMax = 0.f;

				// --- Les pistes ---------------------------------------------------
				NkTimelineTrack *Track(nk_uint64 id);
				const NkTimelineTrack *Track(nk_uint64 id) const;
				int32 TrackIndex(nk_uint64 id) const;
				/// Ajoute une piste (ANNULABLE). Si `id` existe deja, la rend telle quelle.
				NkTimelineTrack &AddTrack(nk_uint64 id, const NkString &object, const NkString &property,
										  NkTimelineValueKind kind, uint8 channels);
				bool RemoveTrack(nk_uint64 id); ///< ANNULABLE
				bool IsCollapsed(const NkString &object) const;
				void ToggleCollapsed(const NkString &object);
				/// (01/10 soir) L'ARBRE des objets : « Bras/Main » a pour parent
				/// « Bras », dont le parent est « » (l'objet anime). Une piste est
				/// CACHEE si son objet ou l'un de ses ancetres est replie.
				static NkString ParentObject(const NkString &object);
				static uint32 ObjectDepth(const NkString &object);
				/// `object` est-il `ancestor` ou sous lui ? (« » contient tout.)
				static bool IsUnderObject(const NkString &object, const NkString &ancestor);
				bool IsHidden(const NkString &object) const;
				/// Muet / solo : la piste doit-elle JOUER ? L'hote ne l'applique pas sinon.
				bool AnySolo() const;
				bool TrackActive(const NkTimelineTrack &track) const;
				/// Bascules (ANNULABLES) d'une piste...
				void ToggleMute(nk_uint64 track);
				void ToggleSolo(nk_uint64 track);
				void ToggleLock(nk_uint64 track);
				/// ...ou de toutes celles d'un objet et de ses descendants.
				/// `flag` : 0 muet, 1 solo, 2 verrou.
				void SetObjectFlag(const NkString &object, uint8 flag, bool on);
				/// Vrai si TOUTES les pistes de l'objet (et dessous) l'ont.
				bool ObjectFlag(const NkString &object, uint8 flag) const;

				// --- Le temps -------------------------------------------------------
				float32 FrameDuration() const {
					return fps > 0.f ? 1.f / fps : 1.f / 30.f;
				}
				/// `t` sur l'image la plus proche si l'accrochage est actif.
				float32 Snap(float32 t) const;
				int32 FrameOf(float32 t) const;
				void SetCursor(float32 t);
				/// Avance la lecture de `dt` secondes (boucle ou s'arrete au bout).
				/// Rend vrai si le curseur a bouge.
				bool Advance(float32 dt);
				void TogglePlay() {
					playing = !playing;
				}
				/// Le temps de la cle precedente / suivante (toutes pistes), ou `t`.
				float32 PreviousKeyTime(float32 t) const;
				float32 NextKeyTime(float32 t) const;
				/// La vue sur [0, duree] avec une marge.
				void FrameAll();
				/// Zoome de `factor` (< 1 rapproche) autour de `pivot` (secondes).
				void ZoomTime(float32 pivot, float32 factor);
				/// (01/10 soir) La plage de lecture effective, et son reglage (ANNULABLE).
				float32 PlayStart() const;
				float32 PlayEnd() const;
				void SetPlayRange(float32 start, float32 end);
				/// Avance le curseur de `frames` images (negatif = recule), dans [0, duree].
				void StepFrame(int32 frames);
				/// Le zoom ADOUCI : pose la cible (la vue la rejoint par `GlideStep`).
				void ZoomTimeSmooth(float32 pivot, float32 factor);
				/// Un pas du glissement de la vue (`k` dans ]0,1] : la part du chemin
				/// faite a cette image). Rend vrai si la vue a bouge.
				bool GlideStep(float32 k);
				/// Fait suivre la vue au curseur pendant la lecture (comme UE5).
				void FollowCursor();

				// --- Les cles (toutes ANNULABLES) -------------------------------------
				/// Pose une cle a `t` (accroche) : remplace celle qui est deja a cette
				/// image, sinon l'insere a sa place. Rend son indice.
				/// `undoable` faux : sans instantane (un geste qui en a deja pris un).
				int32 SetKey(nk_uint64 track, float32 t, const float32 *values, uint8 interp = 255, bool undoable = true);
				/// L'indice de la cle de `track` a l'image de `t` (demi-image de tolerance), -1 sinon.
				int32 KeyAt(const NkTimelineTrack &track, float32 t) const;
				bool DeleteSelection();
				/// Decale les cles choisies de `dt` (accroche). Utilise par le geste.
				void MoveSelection(float32 dt);
				void SetSelectionInterp(uint8 interp);
				/// L'interpolation commune des cles choisies, 255 si elles different ou
				/// s'il n'y en a pas.
				uint8 SelectionInterp() const;
				/// (01/10 soir) Met a l'echelle les temps des cles choisies autour de
				/// `pivot` (la boite de transformation d'UE5). Sans instantane.
				void ScaleSelection(float32 pivot, float32 factor);
				/// Le premier et le dernier temps choisis ; faux s'il n'y a rien.
				bool SelectionTimeRange(float32 &t0, float32 &t1) const;
				/// Copier / Couper / Coller. Coller pose les cles copiees a partir
				/// de `t` (la premiere copiee tombe sur `t`), sur LEURS pistes.
				uint32 CopySelection();
				bool CutSelection();	 ///< ANNULABLE
				bool PasteAt(float32 t); ///< ANNULABLE ; choisit les cles collees
				/// La tangente des cles choisies (ANNULABLE), et la commune (255 sinon).
				void SetSelectionTangent(uint8 tangent);
				uint8 SelectionTangent() const;
				/// Les PENTES effectives de la cle `k` d'une piste, canal `ch` : la
				/// posee, la plate, ou l'automatique (Catmull-Rom sur les temps).
				void KeySlopes(const NkTimelineTrack &track, uint32 k, uint32 ch, float32 &in, float32 &out) const;

				// --- (01/10 soir) Les marqueurs ---------------------------------------
				nk_uint64 AddMarker(float32 t, const NkString &name = NkString()); ///< ANNULABLE
				bool RemoveMarker(nk_uint64 id);									///< ANNULABLE
				NkTimelineMarker *Marker(nk_uint64 id);

				// --- (01/10 soir) Les clips des pistes de clips (NLA) -----------------
				/// Pose un clip (ANNULABLE) et rend son identite (0 si la piste n'est pas
				/// une piste de clips). `sourceLength < 0` : la duree sur la frise.
				nk_uint64 AddClip(nk_uint64 track, const NkString &name, float32 start, float32 length,
								  float32 sourceLength = -1.f);
				NkTimelineClip *Clip(nk_uint64 track, nk_uint64 clip);
				NkTimelineClip *FindClip(nk_uint64 clip, nk_uint64 *track = nullptr);
				/// Le clip de `track` sous `t` (le dernier s'ils se chevauchent), 0 sinon.
				nk_uint64 ClipAt(const NkTimelineTrack &track, float32 t) const;
				void SelectClip(nk_uint64 clip, bool additive);
				uint32 ClipSelectionCount() const;
				/// Trie les clips de chaque piste par debut.
				void SortClips();
				/// Le POIDS d'un clip a `t` : son influence, adoucie par ses fondus
				/// (0 hors du clip). Le meme calcul que NKAnima (NkClipStrip::WeightAt).
				static float32 ClipWeightAt(const NkTimelineClip &clip, float32 t);
				/// Le temps DU CLIP a `t` (rognage, vitesse, boucle) -- NkClipStrip::LocalTime.
				static float32 ClipLocalTime(const NkTimelineClip &clip, float32 t);

				// --- La selection ---------------------------------------------------
				void SelectKey(nk_uint64 track, int32 key, bool additive);
				void ClearSelection();
				void SelectAll();
				/// Les cles des pistes [trackFirst, trackLast] (indices) dans [t0, t1].
				void SelectInBox(int32 trackFirst, int32 trackLast, float32 t0, float32 t1, bool additive);
				uint32 SelectionCount() const;

				// --- L'evaluation de REPLI (sans `evaluer` de l'hote) -----------------
				/// Lineaire entre deux cles, ou palier (piste Palier ou cle Palier).
				/// Rend faux si la piste n'a aucune cle.
				bool EvaluateFallback(const NkTimelineTrack &track, float32 t, float32 out[4]) const;

				// --- Annuler / Refaire ----------------------------------------------
				/// Memorise l'etat AVANT un changement (vide la pile de Refaire).
				void PushUndo();
				bool Undo();
				bool Redo();
				bool CanUndo() const {
					return !undoStack.Empty();
				}
				bool CanRedo() const {
					return !redoStack.Empty();
				}
				/// Trie les cles de chaque piste et fond celles d'une meme image (la
				/// choisie l'emporte). Appele a la fin d'un deplacement.
				void Normalize();
				void Touch() {
					++revision;
				}
		};

		/// Les gestes du composant (NkTimelineModel::gesture).
		enum class NkTimelineGesture : uint8 {
			None = 0,
			Scrub,	   ///< la regle tenue : le curseur suit
			MoveKeys,  ///< des cles tenues
			Box,	   ///< un cadre de selection
			Pan,	   ///< Alt + glisser : la vue suit
			ScrubValue, ///< une valeur de la colonne frottee
			// (01/10 soir)
			ScaleKeys,	   ///< un bord de la boite de transformation
			MoveClip,	   ///< un clip tenu par son corps
			TrimClipStart, ///< son bord gauche
			TrimClipEnd,   ///< son bord droit
			ClipBlendIn,   ///< sa poignee de fondu d'entree
			ClipBlendOut,  ///< sa poignee de fondu de sortie
			MoveMarker,	   ///< un marqueur de la regle
			RangeStart,	   ///< la poignee verte de la plage de lecture
			RangeEnd,	   ///< la poignee rouge
			ScrollThumb,   ///< le pouce de la barre de defilement
			ScrollEdgeL,   ///< son bord gauche (zoom)
			ScrollEdgeR,   ///< son bord droit (zoom)
			MoveTangent	   ///< une poignee de tangente (courbes)
		};

		// ── LE STYLE ────────────────────────────────────────────────────────────
		struct NkTimelineStyle {
				uint16 panelBg = 0;	   ///< fond
				uint16 headerBg = 0;   ///< barre d'outils, regle, en-tetes d'objet
				uint16 rowAltBg = 0;   ///< une piste sur deux
				uint16 border = 0;
				uint16 grid = 0;	   ///< lignes d'images
				uint16 text = 0;
				uint16 textMuted = 0;
				uint16 accent = 0;	   ///< piste active, boutons enfonces
				uint16 textOnAccent = 0;
				uint16 key = 0;		   ///< losange au repos
				uint16 keySelected = 0; ///< losange choisi (ambre)
				uint16 playhead = 0;   ///< curseur de lecture (rouge, spec. 06 §5)
				uint16 channel[4] = {0, 0, 0, 0}; ///< X rouge, Y vert, Z bleu, W
				uint16 buttonBg = 0;
				uint16 inputBg = 0;
				/// (01/10 soir) Roles FACULTATIFS : 0 = un repli tire des autres.
				uint16 rangeIn = 0;	 ///< poignee de debut de plage (vert, UE5)
				uint16 rangeOut = 0; ///< poignee de fin de plage (rouge)
				uint16 marker = 0;	 ///< marqueurs (ambre)
				uint16 clip = 0;	 ///< corps d'un clip (NLA)
				uint16 clipAlt = 0;	 ///< un clip additif
				uint16 tangent = 0;	 ///< poignees de tangente (#d29922, spec. 05 §1)
				/// La source des nombres (nul = les defauts de la declaration).
				const NkComponentInstance *values = nullptr;
		};

		/// Les deux greffes (voir l'en-tete). Toutes deux facultatives.
		struct NkTimelineHooks {
				void *user = nullptr;
				/// La valeur de `track` a `t` par l'interpolation de l'hote. Faux = le
				/// repli du modele.
				bool (*evaluate)(void *user, const NkTimelineTrack &track, float32 t, float32 out[4]) = nullptr;
				/// La valeur VIVANTE de la propriete de `track`. Faux = celle de la
				/// courbe au curseur.
				bool (*readLive)(void *user, const NkTimelineTrack &track, float32 out[4]) = nullptr;
		};

		/// Les rectangles RAPPORTES (le banc y vise ses gestes, l'hote y pose ses
		/// infobulles). Indices de `NkTimelineButton`.
		enum class NkTimelineButton : uint8 {
			First = 0,
			PrevKey,
			Play,
			NextKey,
			Last,
			Loop,
			Mode,	   ///< feuille d'images <-> courbes
			Snap,
			FrameAll,
			AddKey,	   ///< une cle au curseur sur la piste active
			Delete,
			Undo,
			Redo,
			AddTrack,  ///< « + Piste »
			// (01/10 soir)
			PrevFrame, ///< image precedente
			NextFrame, ///< image suivante
			Copy,
			Paste,
			AddMarker,	  ///< un marqueur au curseur
			Speed,		  ///< la vitesse de lecture (0,25x .. 2x, spec. 04 §17)
			TangentFirst, ///< une case par tangente (NkTimelineTangent::Count de suite)
			InterpFirst = TangentFirst + (uint8)NkTimelineTangent::Count, ///< puis une case par interpolation
			Count = InterpFirst + (uint8)NkTimelineInterp::Count
		};

		struct NkTimelineRow {
				nk_uint64 track = 0; ///< 0 = une ligne d'en-tete d'objet
				NkString object;
				float32 y = 0.f, h = 0.f;
				uint32 depth = 0; ///< (01/10 soir) la profondeur dans l'arbre
				/// Les zones de la colonne (lignes de piste) : « ◆ » (une cle au curseur)
				/// et la case de chaque canal (frotter = une cle au curseur).
				NkPaintRect keyButton;
				NkPaintRect channel[4];
				/// (01/10 soir) Muet, solo, verrou ; cle precedente / suivante.
				NkPaintRect muteButton, soloButton, lockButton;
				NkPaintRect prevKeyButton, nextKeyButton;
		};

		// ── LE RESULTAT ─────────────────────────────────────────────────────────
		struct NkTimelineResult {
				/// Les cles ont change (le modele est deja a jour, `revision` a bouge).
				bool changed = false;
				/// Le curseur a bouge (frotte, lu, saute a une cle).
				bool cursorChanged = false;
				/// « + Propriete » presse : l'hote propose ses proprietes.
				bool addTrackRequested = false;
				/// Une piste a ete retiree par son « − » (deja fait, annulable).
				bool trackRemoved = false;
				/// Annuler / Refaire passes par la barre (deja faits).
				bool undone = false;
				bool redone = false;
				// --- (01/10 soir) ---
				/// Muet / solo / verrou bascules (deja faits, annulables).
				bool flagsChanged = false;
				/// Les marqueurs ou la plage de lecture ont change.
				bool markersChanged = false;
				/// Des clips ont change (poses, deplaces, rognes, fondus).
				bool clipsChanged = false;
				/// `hostKeys` : les gestes RAPPORTES a l'hote qui tient les cles.
				bool keyRequested = false;	  ///< une cle a `requestTime` (piste `requestTrack`, 0 = la active)
				bool deleteRequested = false; ///< supprimer la selection
				bool pasteRequested = false;  ///< coller a `requestTime`
				bool copyRequested = false;	  ///< copier la selection
				bool undoRequested = false;
				bool redoRequested = false;
				/// Une piste de clips demande un clip (son « + », ou un double-clic
				/// dans son vide) a `requestTime` : l'hote propose ses animations.
				bool addClipRequested = false;
				nk_uint64 requestTrack = 0;
				float32 requestTime = 0.f;
				nk_uint64 hoveredClip = 0;
				nk_uint64 hoveredMarker = 0;
				/// Survols, pour les infobulles de l'hote.
				nk_uint64 hoveredTrack = 0;
				int32 hoveredKey = -1;
				int32 hoveredButton = -1;
				/// La geometrie, pour le banc et l'hote.
				NkPaintRect buttons[(uint8)NkTimelineButton::Count];
				NkPaintRect list;	///< la colonne des pistes
				NkPaintRect ruler;	///< la regle
				NkPaintRect area;	///< les losanges ou les courbes
				NkVector<NkTimelineRow> rows;
				NkPaintRect transport; ///< (01/10 soir) la barre de lecture du bas
				NkPaintRect scrollbar; ///< la barre de defilement de la vue
		};

		/// Le temps -> x dans `area`, et l'inverse : la geometrie que le dessin,
		/// les gestes ET le banc partagent.
		inline float32 NkTimelineTimeToX(const NkTimelineModel &m, const NkPaintRect &area, float32 t) {
			const float32 span = m.viewEnd - m.viewStart > 1e-6f ? m.viewEnd - m.viewStart : 1e-6f;
			return area.x + (t - m.viewStart) / span * area.w;
		}
		inline float32 NkTimelineXToTime(const NkTimelineModel &m, const NkPaintRect &area, float32 x) {
			const float32 w = area.w > 1e-3f ? area.w : 1e-3f;
			return m.viewStart + (x - area.x) / w * (m.viewEnd - m.viewStart);
		}
		inline float32 NkTimelineValueToY(const NkTimelineModel &m, const NkPaintRect &area, float32 v) {
			const float32 span = m.valueMax - m.valueMin > 1e-6f ? m.valueMax - m.valueMin : 1e-6f;
			return area.y + area.h - (v - m.valueMin) / span * area.h;
		}
		inline float32 NkTimelineYToValue(const NkTimelineModel &m, const NkPaintRect &area, float32 y) {
			const float32 h = area.h > 1e-3f ? area.h : 1e-3f;
			return m.valueMin + (area.y + area.h - y) / h * (m.valueMax - m.valueMin);
		}

		// ── LA SIGNATURE TYPE, A SIX ARGUMENTS ──────────────────────────────────
		NkTimelineResult NkDrawTimeline(NkComponentPaint &p, const NkComponentInput &in, const NkPaintRect &rect,
										NkTimelineModel &m, const NkTimelineStyle &s, const NkTimelineHooks &hooks);

		// ── LA DECLARATION ──────────────────────────────────────────────────────
		inline const NkComponentDecl &NkTimelineDecl() {
			static const NkParamDecl kParams[] = {
				{"show_values", "Valeurs dans la colonne", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_summary", "Resume des cles sur l'en-tete d'objet", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
			};
			static const NkTokenDecl kTokens[] = {
				{"panel_bg", "panel_bg", "fond de la frise"},
				{"header_bg", "panel_header", "barre d'outils, regle, en-tetes d'objet"},
				{"row_alt_bg", "input_bg", "une piste sur deux"},
				{"border", "border", "separations"},
				{"grid", "grid_line", "lignes d'images"},
				{"text", "text", "libelles"},
				{"text_muted", "text_muted", "valeurs, graduations"},
				{"accent", "accent_ui", "piste active, boutons enfonces"},
				{"key", "text_muted", "losange au repos"},
				{"key_selected", "accent_sel", "losange choisi"},
				{"playhead", "status_err", "curseur de lecture"},
				{"channel_x", "axis_x", "canal X / R"},
				{"channel_y", "axis_y", "canal Y / G"},
				{"channel_z", "axis_z", "canal Z / B"},
				{"range_in", "status_ok", "debut de la plage de lecture"},
				{"range_out", "status_err", "fin de la plage de lecture"},
				{"marker", "accent_sel", "marqueurs"},
				{"clip", "type_anim", "un clip (NLA)"},
				{"clip_alt", "node_action_header", "un clip additif"},
				{"tangent", "accent_sel", "poignees de tangente"},
			};
			static const NkMetricDecl kMetrics[] = {
				{"toolbar_h", 30.f, "hauteur de la barre d'outils"},
				{"ruler_h", 34.f, "hauteur de la regle (secondes en haut, images en bas)"},
				{"list_w", 420.f, "largeur de la colonne des pistes (une couleur y montre ses quatre canaux)"},
				{"row_h", 24.f, "hauteur d'une piste"},
				{"group_h", 24.f, "hauteur d'un en-tete d'objet"},
				{"clip_row_h", 30.f, "hauteur d'une piste de clips"},
				{"indent", 14.f, "retrait d'un niveau de l'arbre"},
				{"key_r", 5.5f, "demi-diagonale d'un losange"},
				{"hit_r", 7.f, "rayon de prise d'une cle"},
				{"button_w", 26.f, "largeur d'un bouton a icone"},
				{"button_gap", 2.f, "ecart entre deux boutons"},
				{"group_gap", 10.f, "ecart entre deux groupes de boutons"},
				{"pad", 6.f, "marge interieure"},
				{"value_w", 46.f, "largeur d'une valeur dans la colonne"},
				{"label_min_px", 46.f, "ecart minimal entre deux graduations nommees"},
				{"drag_px", 3.f, "distance avant qu'un appui devienne un glisser"},
				{"stroke_w", 1.f, "epaisseur d'un trait"},
				{"curve_w", 1.6f, "epaisseur d'une courbe"},
				{"curve_samples_px", 4.f, "un echantillon de courbe tous les N pixels"},
				{"playhead_w", 2.f, "epaisseur du curseur de lecture"},
				{"scrub_px", 0.01f, "valeur ajoutee par pixel frotte (nombre)"},
				{"scrub_color_px", 0.004f, "valeur ajoutee par pixel frotte (couleur)"},
				{"wheel_zoom", 0.85f, "facteur de zoom par cran de molette"},
				{"transport_h", 30.f, "hauteur de la barre de lecture du bas"},
				{"icon_w", 18.f, "largeur d'un bouton de piste (muet, solo, verrou, cles)"},
				{"marker_h", 9.f, "hauteur d'un drapeau de marqueur"},
				{"range_w", 7.f, "largeur d'une poignee de plage"},
				{"edge_px", 6.f, "prise d'un bord (clip, boite, barre de defilement)"},
				{"tangent_len", 42.f, "longueur d'une poignee de tangente"},
				{"round", 3.f, "arrondi d'une section, d'un clip, d'un bouton"},
				{"glide", 0.35f, "part du chemin faite par image par le zoom adouci"},
			};
			static const NkHookDecl kHooks[] = {
				{"evaluate", "(user, piste, t, out[4]) -> bool", "l'interpolation de l'hote (celle que le jeu jouera)"},
				{"read_live", "(user, piste, out[4]) -> bool", "la valeur vivante de la propriete"},
			};
			static const NkArgDecl kArgsTrack[] = {
				{"track", NkArgKind::Int, nullptr, 0},
			};
			static const NkEventDecl kEvents[] = {
				{"onChange", "Cles modifiees", "poser, deplacer, supprimer, interpoler, annuler -- le modele est deja a jour",
				 nullptr, 0, true},
				{"onCursor", "Curseur", "le curseur de lecture a bouge", nullptr, 0, true},
				{"onAddTrack", "Ajouter une propriete", "« + Propriete » presse ; l'hote propose les siennes", nullptr, 0,
				 false},
				{"onRemoveTrack", "Piste retiree", "le « − » d'une piste (deja fait, annulable)", kArgsTrack, 1, true},
				{"onFlags", "Muet / solo / verrou", "une bascule de piste (deja faite, annulable)", kArgsTrack, 1, true},
				{"onMarkers", "Marqueurs", "un marqueur ou la plage de lecture a change", nullptr, 0, true},
				{"onClips", "Clips", "un clip pose, deplace, rogne ou fondu (pistes de clips)", nullptr, 0, true},
				{"onHostKey", "Cle demandee", "hostKeys : l'hote pose, supprime, colle, annule lui-meme", nullptr, 0,
				 false},
			};
			static const NkComponentDecl kDecl = {
				/* name        */ "timeline",
				/* title       */ "Frise",
				/* summary     */ "pistes en arbre (muet, solo, verrou), cles en formes d'interpolation, courbes et "
								  "tangentes, pistes de clips (NLA), plage de lecture, marqueurs, copier / coller, "
								  "annuler / refaire",
				/* params      */ kParams, (uint16)(sizeof(kParams) / sizeof(kParams[0])),
				/* variants    */ nullptr, 0,
				/* tokens      */ kTokens, (uint16)(sizeof(kTokens) / sizeof(kTokens[0])),
				/* metrics     */ kMetrics, (uint16)(sizeof(kMetrics) / sizeof(kMetrics[0])),
				/* hooks       */ kHooks, (uint16)(sizeof(kHooks) / sizeof(kHooks[0])),
				/* events      */ kEvents, (uint16)(sizeof(kEvents) / sizeof(kEvents[0])),
				/* role        */ "grid",
				// Les pistes viennent du modele : pas d'arbre d'elements statique (meme
				// mur que tree_view et tab_strip, ecrit dans leurs en-tetes).
				/* elements    */ nullptr, 0,
				/* provenance  */ NkProvenance{NkAuthorKind::AI, false, false},
			};
			return kDecl;
		}

		inline float32 NkTimelineMetric(const NkTimelineStyle &s, const char *name) {
			return s.values ? s.values->Metric(name) : NkTimelineDecl().Metric(name);
		}
		inline float32 NkTimelineParam(const NkTimelineStyle &s, const char *name) {
			return s.values ? s.values->Param(name) : NkTimelineDecl().Param(name);
		}

	} // namespace editorkit
} // namespace nkentseu
