#pragma once
// -----------------------------------------------------------------------------
// @File    NkStateGraphModel.h
// @Brief   LE GRAPHE D'ETATS — les etats (et sous-machines) d'un controleur
//          d'animation, les transitions qui les relient, leurs conditions sur
//          des parametres. Le composant des pages Animateur des editeurs.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CE FICHIER EXISTE (2026-10-01, feuille de route d'Unkeny R22/R35)
// =============================================================================
//  Rihen : « la page Animateur ou l'on fait les relations entre les
//  animations ». NKAnima porte la machine a etats HIERARCHIQUE
//  (anim::NkAnimStateMachine) et son fichier, le `.nkanimctl` ; personne ne la
//  DESSINAIT. Ce composant la montre comme l'Animator d'Unity et la State
//  Machine de l'AnimGraph d'Unreal 5 (spec. NkAnimaEditor/design/06 §7) :
//  « Entree » vers l'etat d'entree, « N'importe quel etat », des fleches avec
//  le resume de leurs conditions, un fil d'Ariane pour les sous-machines, un
//  panneau de parametres a gauche, les details de la selection a droite.
//
// =============================================================================
//  CE QUI SE PARTAGE, CE QUI RESTE CHEZ L'HOTE
// =============================================================================
//  Le kit ne tire pas NKAnima. Le modele ci-dessous est la machine telle qu'on
//  l'EDITE (des identites stables, des positions, annuler / refaire) ; l'hote
//  la CONVERTIT de et vers anim::NkAnimStateMachine (UnkenyEditor :
//  NkEditeurPageAnimateur.cpp). Les codes de genre de parametre et de condition
//  sont ceux de NKAnima, DANS LE MEME ORDRE (anim::NkAnimStateMachine::
//  NkParamKind et NkCondKind) : l'hote transtype, et le verifie.
//
//  Pendant l'APERCU (Jouer), l'hote pose `liveState` (la feuille active),
//  `liveNext` et `liveFade` (le fondu en cours), `firedTransition` (la derniere
//  qui a tire) et `live = true` : les parametres edites changent alors la
//  valeur VIVANTE (`paramValueChanged`), plus le defaut.
//
//  (2026-10-01 soir) LE MELANGE (NKAnima, Blend/NkAnimMix.h) :
//    - un etat peut etre un ARBRE DE MELANGE (le « Blend Space » d'UE5) : des
//      animations placees sur un axe (1D, un parametre) ou un plan (2D, deux),
//      montrees dans le noeud, editees dans l'inspecteur ;
//    - des COUCHES : chacune est une machine a elle (le haut du corps qui tire
//      pendant que les jambes courent), avec son poids, son mode (remplace /
//      additif), son masque et un parametre de poids. Une couche est rangee
//      comme une sous-machine CACHEE de la racine (`layerRoot`) : ses etats, ses
//      transitions et son entree en heritent sans rien de neuf ; le panneau de
//      gauche liste « Base » et les couches, un clic en ouvre une ;
//    - la COURBE du fondu de chaque transition (lineaire, douce, entree,
//      sortie), dans le meme ordre qu'anim::NkFadeCurve.
//
//  CE QU'IL NE FAIT PAS : pas de clavier. Renommer un etat ou un parametre est
//  RAPPORTE (`renameRequested` + le rectangle du nom) : l'hote, qui a le
//  clavier, y pose son champ -- meme partage que tree_view et tab_strip.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentDecl.h"
#include "NKEditorKit/Components/NkComponentInstance.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		/// MEME ORDRE QUE anim::NkAnimStateMachine::NkParamKind.
		enum class NkGraphParamKind : uint8 { Bool = 0, Float, Trigger, Count };
		/// MEME ORDRE QUE anim::NkAnimStateMachine::NkCondKind.
		enum class NkGraphCondOp : uint8 {
			IsTrue = 0,	 ///< booleen vrai
			Greater,	 ///< reel > seuil
			Less,		 ///< reel < seuil
			IsFalse,	 ///< booleen faux
			Trigger,	 ///< declencheur pose (consomme au tir)
			TimeInState, ///< l'etat source est actif depuis au moins `seuil` s
			Count
		};
		const char *NkGraphParamKindName(uint8 kind);
		/// « > », « < », « vrai »... ; `Count` -> « ? ».
		const char *NkGraphCondOpName(uint8 op);

		struct NkGraphParam {
				NkString name;
				uint8 kind = (uint8)NkGraphParamKind::Float;
				float32 defaultValue = 0.f;
				float32 value = 0.f; ///< la valeur VIVANTE pendant l'apercu
		};

		struct NkGraphCondition {
				NkString param;
				uint8 op = (uint8)NkGraphCondOp::Greater;
				float32 threshold = 0.f;
		};

		/// (01/10 soir) Une animation placee dans un arbre de melange.
		struct NkGraphBlendSample {
				NkString clip;
				float32 x = 0.f, y = 0.f;
		};

		struct NkGraphNode {
				nk_uint64 id = 0;
				NkString name;
				nk_uint64 parent = 0; ///< 0 = la racine, sinon une sous-machine
				bool subMachine = false;
				/// L'animation jouee (son NOM, resolu par l'hote) ; vide = etat vide.
				NkString clip;
				/// L'etiquette libre de l'etat (Unkeny : l'index du clip de sprite).
				int32 tag = 0;
				/// Sous-machine : son etat d'entree (0 = son premier enfant).
				nk_uint64 entry = 0;
				float32 x = 0.f, y = 0.f; ///< position dans le graphe (unites du graphe)
				// --- (01/10 soir) l'ARBRE DE MELANGE ---
				bool blendTree = false;
				uint8 blendDims = 1; ///< 1 ou 2
				NkString paramX, paramY;
				NkVector<NkGraphBlendSample> samples;
				// --- (01/10 soir) la RACINE D'UNE COUCHE (sous-machine cachee) ---
				bool layerRoot = false;
				float32 layerWeight = 1.f;
				bool layerAdditive = false;
				NkString layerMask;		   ///< un des `masks` proposes par l'hote (vide = tout)
				NkString layerWeightParam; ///< un parametre reel qui multiplie le poids (vide = aucun)
		};

		struct NkGraphTransition {
				nk_uint64 id = 0;
				nk_uint64 from = 0; ///< ignore si `any`
				nk_uint64 to = 0;
				bool any = false;	  ///< « depuis n'importe quel etat » de `scope`
				nk_uint64 scope = 0;  ///< 0 = la racine
				NkVector<NkGraphCondition> conditions; ///< ET
				float32 fade = 0.25f;
				int32 priority = 0;
				uint8 curve = 0; ///< (01/10 soir) la courbe du fondu, MEME ORDRE QU'anim::NkFadeCurve
		};

		/// La place des deux pseudo-noeuds d'un niveau.
		struct NkGraphPseudo {
				nk_uint64 machine = 0;
				float32 entryX = 0.f, entryY = 0.f;
				float32 anyX = 0.f, anyY = 0.f;
		};

		struct NkStateGraphSnapshot {
				NkVector<NkGraphNode> nodes;
				NkVector<NkGraphTransition> transitions;
				NkVector<NkGraphParam> params;
				NkVector<NkGraphPseudo> pseudos;
				nk_uint64 rootEntry = 0;
		};

		/// Les identites RESERVEES des pseudo-noeuds (jamais celles d'un etat).
		static constexpr nk_uint64 kNkGraphEntryId = ~(nk_uint64)0;
		static constexpr nk_uint64 kNkGraphAnyId = ~(nk_uint64)0 - 1;
		static constexpr nk_uint64 kNkGraphUpId = ~(nk_uint64)0 - 2; ///< « ↑ niveau parent »

		// ── LE MODELE ───────────────────────────────────────────────────────────
		struct NkStateGraphModel {
				NkVector<NkGraphNode> nodes;
				NkVector<NkGraphTransition> transitions;
				NkVector<NkGraphParam> params;
				NkVector<NkGraphPseudo> pseudos;
				nk_uint64 rootEntry = 0; ///< 0 = le premier etat de la racine
				/// Les ANIMATIONS proposees (noms), fournies par l'hote : un clic en
				/// cree un etat qui la joue.
				NkVector<NkString> clips;
				/// (01/10 soir) Les MASQUES proposes par l'hote pour les couches (Unkeny :
				/// les objets nommes sous l'entite, « Torse » couvre « Torse/Bras »).
				NkVector<NkString> masks;
				nk_uint64 nextId = 1;

				// --- La vue -------------------------------------------------------
				nk_uint64 level = 0; ///< le niveau regarde (0 = racine)
				float32 panX = 0.f, panY = 0.f, zoom = 1.f;
				nk_uint64 selectedNode = 0; ///< un etat, ou un pseudo-noeud (k...Id)
				nk_uint64 selectedTransition = 0;
				/// « Relier » : le prochain glisser depuis un etat trace une transition.
				bool linkMode = false;

				// --- L'apercu (pose par l'hote pendant Jouer) -------------------------
				bool live = false;
				nk_uint64 liveState = 0;
				nk_uint64 liveNext = 0;
				float32 liveFade = 0.f;
				nk_uint64 firedTransition = 0;
				float32 firedAge = 99.f; ///< secondes depuis le tir (l'hote l'avance)

				uint32 revision = 0;
				NkVector<NkStateGraphSnapshot> undoStack, redoStack;
				uint32 undoLimit = 64;

				// --- Le geste en cours (etat du composant) ----------------------------
				uint8 gesture = 0; ///< NkStateGraphGesture
				bool gestureMoved = false;
				bool gestureSnapshot = false;
				float32 gestureX0 = 0.f, gestureY0 = 0.f;
				float32 gestureA = 0.f, gestureB = 0.f; ///< position d'origine (noeud ou vue)
				nk_uint64 gestureNode = 0;
				int32 gestureParam = -1;
				float32 gestureValue = 0.f;
				int32 gestureCondition = -1;

				// --- Les noeuds -----------------------------------------------------
				NkGraphNode *Node(nk_uint64 id);
				const NkGraphNode *Node(nk_uint64 id) const;
				int32 NodeIndex(nk_uint64 id) const;
				const NkGraphNode *FindByName(const NkString &name, nk_uint64 parent) const;
				/// Un nom libre DANS `parent` (« Etat », « Etat 2 »...).
				NkString UniqueName(const NkString &base, nk_uint64 parent) const;
				/// Ajoutent (ANNULABLE) et rendent l'identite.
				nk_uint64 AddState(const NkString &name, nk_uint64 parent, float32 x, float32 y,
								   const NkString &clip = NkString());
				nk_uint64 AddSubMachine(const NkString &name, nk_uint64 parent, float32 x, float32 y);
				/// Retire un etat (ses enfants, ses transitions). ANNULABLE.
				bool RemoveNode(nk_uint64 id);
				bool RenameNode(nk_uint64 id, const NkString &name); ///< ANNULABLE ; faux si le nom est pris
				/// L'etat d'entree EFFECTIF de `machine` (0 = racine) : l'explicite, ou le
				/// premier enfant.
				nk_uint64 EntryOf(nk_uint64 machine) const;
				void SetEntry(nk_uint64 machine, nk_uint64 state); ///< ANNULABLE
				/// « Sol/marche ».
				NkString PathOf(nk_uint64 id) const;
				/// Le representant de `id` au niveau `level` : lui-meme ou l'ancetre
				/// dont le parent est `level` ; 0 s'il n'est pas sous ce niveau.
				nk_uint64 RepresentativeAt(nk_uint64 id, nk_uint64 level) const;
				bool IsUnder(nk_uint64 id, nk_uint64 machine) const;

				// --- Les transitions ------------------------------------------------
				NkGraphTransition *Transition(nk_uint64 id);
				const NkGraphTransition *Transition(nk_uint64 id) const;
				nk_uint64 AddTransition(nk_uint64 from, nk_uint64 to);  ///< ANNULABLE
				nk_uint64 AddAnyTransition(nk_uint64 scope, nk_uint64 to); ///< ANNULABLE
				bool RemoveTransition(nk_uint64 id);					   ///< ANNULABLE
				/// Ajoute une condition (le premier parametre et son operateur naturel
				/// si `param` est vide). ANNULABLE.
				bool AddCondition(nk_uint64 transition, const NkString &param = NkString(), uint8 op = 255,
								  float32 threshold = 0.f);
				bool RemoveCondition(nk_uint64 transition, uint32 index);
				/// Le resume d'une transition : « vitesse > 0.10 ET auSol ».
				NkString Summary(const NkGraphTransition &t) const;

				// --- Les parametres -------------------------------------------------
				int32 ParamIndex(const NkString &name) const;
				/// Ajoute un parametre au nom libre (« Parametre », « Parametre 2 »...). ANNULABLE.
				int32 AddParam(const NkString &name, uint8 kind, float32 defaultValue = 0.f);
				bool RemoveParam(uint32 index); ///< ANNULABLE (les conditions qui le citent partent aussi)
				bool RenameParam(uint32 index, const NkString &name); ///< ANNULABLE ; les conditions suivent
				/// L'operateur naturel d'un genre (bool : vrai ; reel : > ; declencheur).
				static uint8 DefaultOp(uint8 kind);
				/// L'operateur suivant valable pour `kind` (le clic sur l'operateur).
				static uint8 NextOp(uint8 kind, uint8 op);

				// --- Les pseudo-noeuds ----------------------------------------------
				NkGraphPseudo &PseudoOf(nk_uint64 machine);
				/// Place les pseudo-noeuds d'un niveau qui n'en a pas encore : a gauche
				/// et au-dessus de ses etats.
				void PlacePseudos(nk_uint64 machine);

				// --- Annuler / Refaire ----------------------------------------------
				void PushUndo();
				bool Undo();
				bool Redo();
				bool CanUndo() const {
					return !undoStack.Empty();
				}
				bool CanRedo() const {
					return !redoStack.Empty();
				}
				void Touch() {
					++revision;
				}
				/// Recentre la vue sur les etats du niveau regarde.
				void FrameLevel(float32 canvasW, float32 canvasH);

				// --- (01/10 soir) Le melange --------------------------------------
				/// Une COUCHE neuve (ANNULABLE) : rend sa racine (le niveau a ouvrir).
				nk_uint64 AddLayer(const NkString &name);
				bool IsLayerRoot(nk_uint64 id) const;
				/// La couche de `id` (sa racine), 0 = la base.
				nk_uint64 LayerOf(nk_uint64 id) const;
				/// Un ARBRE DE MELANGE (ANNULABLE), le premier parametre reel en X.
				nk_uint64 AddBlendTree(const NkString &name, nk_uint64 parent, float32 x, float32 y, uint8 dims = 1);
				bool AddBlendSample(nk_uint64 node, const NkString &clip, float32 x, float32 y = 0.f); ///< ANNULABLE
				bool RemoveBlendSample(nk_uint64 node, uint32 index);								   ///< ANNULABLE
				/// « Lineaire », « Douce », « Entree », « Sortie ».
				static const char *FadeCurveName(uint8 curve);
		};

		enum class NkStateGraphGesture : uint8 {
			None = 0,
			MoveNode,	 ///< un etat (ou un pseudo-noeud) tenu
			Link,		 ///< une transition en cours de trace
			Pan,		 ///< le fond tenu : la vue suit
			ScrubParam,	 ///< la valeur d'un parametre reel frottee
			ScrubField	 ///< un nombre de l'inspecteur frotte (fondu, seuil, etiquette)
		};

		// ── LE STYLE ────────────────────────────────────────────────────────────
		struct NkStateGraphStyle {
				uint16 canvasBg = 0;
				uint16 gridLine = 0;
				uint16 panelBg = 0;
				uint16 headerBg = 0;
				uint16 border = 0;
				uint16 text = 0;
				uint16 textMuted = 0;
				uint16 textOnAccent = 0;
				uint16 accent = 0;		 ///< selection, etat ACTIF de l'apercu
				uint16 nodeBody = 0;
				uint16 nodeHeader = 0;	 ///< un etat
				uint16 nodeEntry = 0;	 ///< l'etat d'entree (ambre)
				uint16 subMachine = 0;	 ///< une sous-machine
				uint16 wire = 0;		 ///< une transition
				uint16 pseudoEntry = 0;	 ///< « Entree » (vert)
				uint16 pseudoAny = 0;	 ///< « N'importe quel etat » (bleu-vert)
				uint16 paramBool = 0, paramFloat = 0, paramTrigger = 0;
				uint16 buttonBg = 0;
				uint16 inputBg = 0;
				uint16 blendTree = 0; ///< (01/10 soir) bandeau d'un arbre de melange (0 = pseudoAny)
				const NkComponentInstance *values = nullptr;
		};

		enum class NkStateGraphButton : uint8 {
			AddState = 0,
			AddSubMachine,
			Link,
			Delete,
			Undo,
			Redo,
			Frame,
			AddBool,	///< panneau des parametres
			AddFloat,
			AddTrigger,
			AddBlendTree, ///< (01/10 soir) « + Arbre »
			Count
		};

		/// Ce qu'une ligne de l'inspecteur represente (le banc y vise).
		enum class NkGraphField : uint8 {
			None = 0,
			NodeName,
			NodeClip,	   ///< clic : animation suivante
			NodeTag,	   ///< frotter
			NodeSetEntry,  ///< « Definir comme entree »
			NodeEnter,	   ///< « Ouvrir » (sous-machine)
			NodeDelete,
			TransFade,	   ///< frotter
			TransPriority, ///< frotter
			CondParam,	   ///< clic : parametre suivant
			CondOp,		   ///< clic : operateur suivant
			CondValue,	   ///< frotter
			CondRemove,
			CondAdd,
			TransDelete,
			ParamValue,	   ///< un parametre : bascule, frotter ou tirer
			ParamName,	   ///< double-clic : renommer
			ParamRemove,
			ClipItem,	   ///< une animation proposee : clic = un etat qui la joue
			Breadcrumb,	   ///< un niveau du fil d'Ariane
			// (01/10 soir) le melange
			TransCurve,	  ///< clic : courbe suivante
			BlendDims,	  ///< clic : 1D <-> 2D
			BlendParamX,  ///< clic : parametre reel suivant
			BlendParamY,
			SampleClip,	  ///< clic : animation suivante (index = echantillon)
			SampleX,	  ///< frotter
			SampleY,
			SampleRemove,
			SampleAdd,
			LayerItem,	  ///< « Base » ou une couche : l'ouvrir (id = sa racine, 0 = la base)
			LayerAdd,
			LayerWeight,  ///< frotter
			LayerMode,	  ///< clic : remplace <-> additif
			LayerMask,	  ///< clic : masque suivant
			LayerParam,	  ///< clic : parametre de poids suivant
			LayerRemove
		};

		struct NkGraphHit {
				NkGraphField field = NkGraphField::None;
				int32 index = -1;	  ///< condition, parametre, animation, niveau
				nk_uint64 id = 0;	  ///< noeud ou transition vise
				NkPaintRect rect;
		};

		struct NkGraphNodeRect {
				nk_uint64 id = 0;
				NkPaintRect rect;
				NkPaintRect port; ///< le rond d'ou part une transition
		};

		// ── LE RESULTAT ─────────────────────────────────────────────────────────
		struct NkStateGraphResult {
				bool changed = false;			 ///< la DEFINITION a change (modele deja a jour)
				bool paramValueChanged = false;	 ///< apercu : une valeur vivante a change
				int32 paramIndex = -1;
				bool triggerFired = false;		 ///< apercu : un declencheur tire
				bool levelChanged = false;
				bool selectionChanged = false;
				/// Renommer : l'hote pose son champ sur `renameRect`.
				bool renameRequested = false;
				nk_uint64 renameNode = 0;		 ///< un etat...
				int32 renameParam = -1;			 ///< ...ou un parametre
				NkPaintRect renameRect;
				nk_uint64 hoveredNode = 0;
				nk_uint64 hoveredTransition = 0;
				NkPaintRect buttons[(uint8)NkStateGraphButton::Count];
				NkPaintRect canvas, side, inspector, breadcrumb;
				NkVector<NkGraphNodeRect> nodeRects; ///< pseudo-noeuds compris (k...Id)
				NkVector<NkGraphHit> hits;			 ///< inspecteur, parametres, animations, fil d'Ariane
		};

		/// Graphe -> ecran et inverse (le dessin, les gestes et le banc la partagent).
		inline void NkStateGraphToScreen(const NkStateGraphModel &m, const NkPaintRect &canvas, float32 scale,
										 float32 gx, float32 gy, float32 &sx, float32 &sy) {
			sx = canvas.x + canvas.w * 0.5f + (gx - m.panX) * m.zoom * scale;
			sy = canvas.y + canvas.h * 0.5f + (gy - m.panY) * m.zoom * scale;
		}
		inline void NkStateGraphToGraph(const NkStateGraphModel &m, const NkPaintRect &canvas, float32 scale,
										float32 sx, float32 sy, float32 &gx, float32 &gy) {
			const float32 z = m.zoom * scale > 1e-4f ? m.zoom * scale : 1e-4f;
			gx = m.panX + (sx - canvas.x - canvas.w * 0.5f) / z;
			gy = m.panY + (sy - canvas.y - canvas.h * 0.5f) / z;
		}

		NkStateGraphResult NkDrawStateGraph(NkComponentPaint &p, const NkComponentInput &in, const NkPaintRect &rect,
											NkStateGraphModel &m, const NkStateGraphStyle &s);

		// ── LA DECLARATION ──────────────────────────────────────────────────────
		inline const NkComponentDecl &NkStateGraphDecl() {
			static const NkParamDecl kParams[] = {
				{"show_conditions", "Resume des conditions sur les fleches", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
				{"show_grid", "Grille du fond", NkParamKind::Bool, 1.f, 0.f, 0.f, nullptr, 0},
			};
			static const NkTokenDecl kTokens[] = {
				{"canvas_bg", "canvas_bg", "fond du graphe"},
				{"grid_line", "grid_line", "grille"},
				{"node_body", "node_body", "corps d'un etat"},
				{"node_header", "node_data_header", "bandeau d'un etat"},
				{"node_entry", "accent_sel", "l'etat d'entree"},
				{"sub_machine", "node_action_header", "une sous-machine"},
				{"wire", "node_wire", "une transition"},
				{"accent", "accent_ui", "selection et etat actif de l'apercu"},
				{"pseudo_entry", "status_ok", "« Entree »"},
				{"pseudo_any", "type_anim", "« N'importe quel etat »"},
				{"param_bool", "accent_ui", "pastille d'un booleen"},
				{"param_float", "status_ok", "pastille d'un reel"},
				{"param_trigger", "accent_sel", "pastille d'un declencheur"},
				{"blend_tree", "type_anim", "bandeau d'un arbre de melange"},
			};
			static const NkMetricDecl kMetrics[] = {
				{"bar_h", 28.f, "hauteur de la barre (fil d'Ariane, boutons)"},
				{"side_w", 220.f, "largeur du panneau des parametres"},
				{"inspector_w", 270.f, "largeur de l'inspecteur"},
				{"node_w", 160.f, "largeur d'un etat (zoom 1)"},
				{"node_h", 44.f, "hauteur d'un etat (zoom 1)"},
				{"node_header_h", 6.f, "bandeau de couleur d'un etat"},
				{"node_round", 6.f, "arrondi d'un etat"},
				{"pseudo_w", 130.f, "largeur d'un pseudo-noeud"},
				{"pseudo_h", 28.f, "hauteur d'un pseudo-noeud"},
				{"port_r", 6.f, "rayon du rond d'ou part une transition"},
				{"wire_w", 2.f, "epaisseur d'une transition"},
				{"wire_offset", 7.f, "ecart entre A->B et B->A"},
				{"arrow", 8.f, "taille de la pointe"},
				{"hit_wire", 6.f, "distance de prise d'une transition"},
				{"grid", 32.f, "pas de la grille (zoom 1)"},
				{"row_h", 24.f, "ligne d'un panneau"},
				{"pad", 8.f, "marge interieure"},
				{"button_gap", 3.f, "ecart entre deux boutons"},
				{"stroke_w", 1.f, "trait"},
				{"drag_px", 3.f, "seuil d'un glisser"},
				{"scrub_px", 0.01f, "valeur par pixel frotte"},
				{"wheel_zoom", 1.12f, "facteur de zoom par cran"},
			};
			static const NkArgDecl kArgsId[] = {
				{"id", NkArgKind::Int, nullptr, 0},
			};
			static const NkEventDecl kEvents[] = {
				{"onChange", "Definition modifiee", "etat, transition, condition, parametre -- le modele est deja a jour",
				 nullptr, 0, true},
				{"onParamValue", "Valeur vivante", "apercu : un parametre a change ou un declencheur a tire", kArgsId, 1,
				 true},
				{"onRename", "Renommer", "double-clic sur un nom ; l'hote pose son champ", kArgsId, 1, false},
				{"onLevel", "Niveau", "entree dans une sous-machine ou retour par le fil d'Ariane", kArgsId, 1, true},
			};
			static const NkComponentDecl kDecl = {
				/* name        */ "state_graph",
				/* title       */ "Graphe d'etats",
				/* summary     */ "etats, sous-machines et arbres de melange 1D / 2D, couches (poids, masque, additif), "
									  "transitions conditionnees et courbe de fondu, etat "
								  "d'entree, apercu de l'etat actif",
				/* params      */ kParams, (uint16)(sizeof(kParams) / sizeof(kParams[0])),
				/* variants    */ nullptr, 0,
				/* tokens      */ kTokens, (uint16)(sizeof(kTokens) / sizeof(kTokens[0])),
				/* metrics     */ kMetrics, (uint16)(sizeof(kMetrics) / sizeof(kMetrics[0])),
				/* hooks       */ nullptr, 0,
				/* events      */ kEvents, (uint16)(sizeof(kEvents) / sizeof(kEvents[0])),
				/* role        */ "graph",
				/* elements    */ nullptr, 0,
				/* provenance  */ NkProvenance{NkAuthorKind::AI, false, false},
			};
			return kDecl;
		}

		inline float32 NkStateGraphMetric(const NkStateGraphStyle &s, const char *name) {
			return s.values ? s.values->Metric(name) : NkStateGraphDecl().Metric(name);
		}
		inline float32 NkStateGraphParam(const NkStateGraphStyle &s, const char *name) {
			return s.values ? s.values->Param(name) : NkStateGraphDecl().Param(name);
		}

	} // namespace editorkit
} // namespace nkentseu
