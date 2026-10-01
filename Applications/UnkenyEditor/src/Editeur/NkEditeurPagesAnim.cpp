//
// NkEditeurPagesAnim.cpp
// =============================================================================
// Description :
//   Ce que les deux pages partagent : ouvrir, fermer, enregistrer un document ;
//   ses onglets a cote de celui de la scene ; la page au premier plan dans le
//   corps ; le clavier ; les actions ; et les CONVERSIONS frise <-> clip,
//   graphe <-> machine (NkEditeurPagesAnim.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurPagesAnim.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"
#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKEditorKit/Components/NkContentBrowserDisque.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;
		using editorkit::NkRole;

		// Les codes d'interpolation et de condition du kit SONT ceux de NKAnima
		// (NkTimelineModel.h, NkStateGraphModel.h) : on le verifie ici, a la
		// compilation, la ou les deux se rencontrent.
		static_assert((uint8)editorkit::NkTimelineInterp::Palier == (uint8)anim::NkInterpMode::NK_STEP &&
						  (uint8)editorkit::NkTimelineInterp::Lineaire == (uint8)anim::NkInterpMode::NK_LINEAR &&
						  (uint8)editorkit::NkTimelineInterp::Recul == (uint8)anim::NkInterpMode::NK_BACK,
					  "NkTimelineInterp doit suivre anim::NkInterpMode");
		static_assert((uint8)editorkit::NkTimelineTangent::Cassee == (uint8)anim::NkTangentMode::NK_BREAK &&
						  (uint8)editorkit::NkTimelineTangent::Plate == (uint8)anim::NkTangentMode::NK_FLAT,
					  "NkTimelineTangent doit suivre anim::NkTangentMode");
		static_assert((uint8)editorkit::NkGraphCondOp::Greater == (uint8)anim::NkAnimStateMachine::NkCondKind::FLOAT_GREATER &&
						  (uint8)editorkit::NkGraphCondOp::TimeInState ==
							  (uint8)anim::NkAnimStateMachine::NkCondKind::TIME_IN_STATE &&
						  (uint8)editorkit::NkGraphParamKind::Trigger == (uint8)anim::NkAnimStateMachine::NkParamKind::TRIGGER,
					  "NkGraphCondOp / NkGraphParamKind doivent suivre NKAnima");

		namespace {
			/// Le nom d'un fichier sans dossier ni extension.
			NkString Tronc(const char *chemin) {
				if (chemin == nullptr) {
					return NkString();
				}
				const char *debut = chemin;
				for (const char *p = chemin; *p != '\0'; ++p) {
					if (*p == '/' || *p == '\\') {
						debut = p + 1;
					}
				}
				const char *point = std::strrchr(debut, '.');
				return point != nullptr ? NkString(debut, static_cast<usize>(point - debut)) : NkString(debut);
			}

			NkDocAnim *DocParId(NkEditeurInterface &ui, nk_uint64 id) {
				for (uint32 i = 0; i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
					if (ui.pagesAnim.docs[i].id == id) {
						return &ui.pagesAnim.docs[i];
					}
				}
				return nullptr;
			}

			NkDocAnim *DocParChemin(NkEditeurInterface &ui, const NkString &chemin) {
				for (uint32 i = 0; !chemin.Empty() && i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
					if (ui.pagesAnim.docs[i].chemin == chemin) {
						return &ui.pagesAnim.docs[i];
					}
				}
				return nullptr;
			}

			NkDocAnim &NouveauDoc(NkEditeurModele &m, NkEditeurInterface &ui, NkGenreDocAnim genre) {
				NkDocAnim d;
				d.id = ui.pagesAnim.prochainId++;
				d.genre = genre;
				if (m.aSelection && m.scene.Monde().IsAlive(m.selection)) {
					d.cibleUid = m.scene.AssurerUid(m.selection);
				}
				ui.pagesAnim.docs.PushBack(d);
				// Son onglet entre dans la barre unique, au premier plan
				// (NkEditeurDocuments.h) : le document d'avant est QUITTE.
				NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, d.id));
				return *DocParId(ui, d.id);
			}

			/// Le nom de l'entite (« » si elle n'en a pas).
			NkString NomEntite(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkEtiquette *e = m.scene.Monde().IsAlive(id) ? m.scene.Monde().Get<NkEtiquette>(id) : nullptr;
				return e != nullptr ? NkString(e->nom) : NkString();
			}
		} // namespace

		// =====================================================================
		// LES CONVERSIONS
		// =====================================================================
		void NkFriseDepuisClip(const anim::NkAnimationClip &clip, editorkit::NkTimelineModel &frise) {
			using PK = anim::NkAnimationClip::NkPropertyKind;
			frise.tracks.Clear();
			frise.undoStack.Clear();
			frise.redoStack.Clear();
			frise.fps = clip.fps > 0.f ? clip.fps : 30.f;
			frise.duration = clip.duration > 1e-3f ? clip.duration : 1.f;
			frise.loop = clip.loop;
			for (uint32 i = 0; i < (uint32)clip.propertyTracks.Size(); ++i) {
				const anim::NkAnimationClip::NkPropertyTrack &p = clip.propertyTracks[i];
				editorkit::NkTimelineTrack t;
				t.id = (nk_uint64)(i + 1);
				t.object = p.target;
				t.property = p.property;
				switch (p.kind) {
					case PK::NK_COLOR:
						t.kind = editorkit::NkTimelineValueKind::Couleur;
						t.channels = 4;
						break;
					case PK::NK_STEP:
						t.kind = editorkit::NkTimelineValueKind::Palier;
						t.channels = 1;
						break;
					case PK::NK_VEC2:
						t.channels = 2;
						break;
					case PK::NK_VEC3:
						t.channels = 3;
						break;
					case PK::NK_VEC4:
						t.channels = 4;
						break;
					default:
						t.channels = 1;
						break;
				}
				for (uint32 k = 0; k < p.curve.KeyCount(); ++k) {
					const anim::NkKeyframe<math::NkVec4f> &kf = p.curve.GetKey(k);
					editorkit::NkTimelineKey key;
					key.time = kf.time;
					key.v[0] = kf.value.x;
					key.v[1] = kf.value.y;
					key.v[2] = kf.value.z;
					key.v[3] = kf.value.w;
					key.interp = (uint8)kf.interp;
					// (01/10 soir) la tangente posee a la main, s'il y en a une.
					if (k < (uint32)p.tangents.Size()) {
						const anim::NkAnimationClip::NkPropertyTangent &tg = p.tangents[k];
						key.tangent = (uint8)tg.mode;
						for (uint32 c = 0; c < 4; ++c) {
							key.tanIn[c] = tg.in[c];
							key.tanOut[c] = tg.out[c];
						}
					}
					t.keys.PushBack(key);
				}
				t.muted = !p.curve.enabled;
				frise.tracks.PushBack(t);
			}
			// (01/10 soir) Les pistes de CLIPS (NLA) : apres les proprietes.
			frise.nextClipId = 1;
			for (uint32 i = 0; i < (uint32)clip.clipTracks.Size(); ++i) {
				const anim::NkClipStripTrack &st = clip.clipTracks[i];
				editorkit::NkTimelineTrack t;
				t.id = (nk_uint64)(clip.propertyTracks.Size() + i + 1);
				t.object = NkString();
				t.property = st.name.Empty() ? NkString("Clips") : st.name;
				t.label = t.property;
				t.kind = editorkit::NkTimelineValueKind::Clips;
				t.weight = st.weight;
				t.additive = st.additive;
				t.muted = st.muted;
				t.mask = st.mask;
				for (uint32 k = 0; k < (uint32)st.strips.Size(); ++k) {
					const anim::NkClipStrip &s = st.strips[k];
					editorkit::NkTimelineClip c;
					c.id = frise.nextClipId++;
					c.name = s.clip;
					c.start = s.start;
					c.length = s.length;
					c.offset = s.offset;
					c.rate = s.rate;
					c.blendIn = s.blendIn;
					c.blendOut = s.blendOut;
					c.weight = s.weight;
					c.loop = s.loop;
					const anim::NkAnimationClip *src = unkeny::NkClipProprietesEnregistre(s.clip.CStr());
					c.sourceLength = src != nullptr && src->duration > 1e-3f ? src->duration : s.length;
					t.clips.PushBack(c);
				}
				frise.tracks.PushBack(t);
			}
			frise.revision++;
			frise.FrameAll();
		}

		void NkClipDepuisFrise(const editorkit::NkTimelineModel &frise, anim::NkAnimationClip &clip) {
			using PK = anim::NkAnimationClip::NkPropertyKind;
			clip.propertyTracks.Clear();
			clip.clipTracks.Clear();
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				const editorkit::NkTimelineTrack &t = frise.tracks[i];
				if (t.kind == editorkit::NkTimelineValueKind::Clips) {
					// (01/10 soir) une piste de CLIPS -> une NkClipStripTrack de NKAnima.
					anim::NkClipStripTrack st;
					st.name = t.property;
					st.weight = t.weight;
					st.additive = t.additive;
					st.muted = t.muted;
					st.mask = t.mask;
					for (uint32 k = 0; k < (uint32)t.clips.Size(); ++k) {
						const editorkit::NkTimelineClip &c = t.clips[k];
						anim::NkClipStrip s;
						s.clip = c.name;
						s.start = c.start;
						s.length = c.length;
						s.offset = c.offset;
						s.rate = c.rate;
						s.blendIn = c.blendIn;
						s.blendOut = c.blendOut;
						s.weight = c.weight;
						s.loop = c.loop;
						st.strips.PushBack(s);
					}
					clip.clipTracks.PushBack(st);
					continue;
				}
				PK genre = PK::NK_NUMBER;
				if (t.kind == editorkit::NkTimelineValueKind::Couleur) {
					genre = PK::NK_COLOR;
				} else if (t.kind == editorkit::NkTimelineValueKind::Palier) {
					genre = PK::NK_STEP;
				} else if (t.channels == 2) {
					genre = PK::NK_VEC2;
				} else if (t.channels == 3) {
					genre = PK::NK_VEC3;
				} else if (t.channels >= 4) {
					genre = PK::NK_VEC4;
				}
				anim::NkAnimationClip::NkPropertyTrack &p = clip.AddPropertyTrack(t.object, t.property, genre);
				for (uint32 k = 0; k < (uint32)t.keys.Size(); ++k) {
					const editorkit::NkTimelineKey &key = t.keys[k];
					p.curve.AddKey(key.time, math::NkVec4f(key.v[0], key.v[1], key.v[2], key.v[3]),
								   (anim::NkInterpMode)(key.interp < (uint8)editorkit::NkTimelineInterp::Count ? key.interp : 1u));
				}
				// (01/10 soir) muet = piste desactivee ; les tangentes posees a la main.
				p.curve.enabled = !t.muted;
				bool tangentes = false;
				for (uint32 k = 0; k < (uint32)t.keys.Size(); ++k) {
					tangentes = tangentes || t.keys[k].tangent != (uint8)editorkit::NkTimelineTangent::Auto;
				}
				p.tangents.Clear();
				for (uint32 k = 0; tangentes && k < (uint32)t.keys.Size(); ++k) {
					anim::NkAnimationClip::NkPropertyTangent tg;
					tg.mode = (anim::NkTangentMode)(t.keys[k].tangent < (uint8)editorkit::NkTimelineTangent::Count ? t.keys[k].tangent : 0u);
					for (uint32 c = 0; c < 4; ++c) {
						tg.in[c] = t.keys[k].tanIn[c];
						tg.out[c] = t.keys[k].tanOut[c];
					}
					p.tangents.PushBack(tg);
				}
			}
			clip.fps = frise.fps;
			clip.duration = frise.duration;
			clip.loop = frise.loop;
		}

		// (01/10 soir) LE CONTROLEUR : la machine de base, les COUCHES (rangees dans
		// le graphe comme des sous-machines cachees de la racine, `layerRoot`), les
		// ARBRES DE MELANGE (un etat de genre 2 ou 3 de NKAnima, son NkBlendSpaceDef
		// nomme du chemin du noeud), les courbes de fondu, et les masques (un masque
		// par objet nomme : « Torse » couvre « Torse » et ses descendants).
		namespace {
			using SM = anim::NkAnimStateMachine;

			/// Ajoute au graphe la machine `m` sous `racine` (0 = la base). Les identites
			/// suivent `g.nextId` ; `noeudDeEtat` (facultatif) recoit etat -> noeud.
			void AjouterMachine(const SM &m, const anim::NkAnimController *ctl, editorkit::NkStateGraphModel &g, nk_uint64 racine,
								NkVector<nk_uint64> *noeudDeEtat) {
				const int32 n = m.GetStateCount();
				const nk_uint64 premier = g.nextId;
				auto idDe = [&](int32 etat) -> nk_uint64 { return etat >= 0 ? premier + (nk_uint64)etat : racine; };
				if (noeudDeEtat != nullptr) {
					noeudDeEtat->Clear();
					for (int32 i = 0; i < n; ++i) {
						noeudDeEtat->PushBack(idDe(i));
					}
				}
				// Une disposition par niveau quand le fichier n'en porte pas : les etats
				// d'un meme niveau en grille, trois par rangee.
				NkVector<int32> rangDansNiveau;
				rangDansNiveau.Resize((uint32)(n + 1));
				NkVector<int32> parNiveau;
				parNiveau.Resize((uint32)(n + 1));
				for (int32 i = 0; i < n; ++i) {
					const int32 p = m.GetStateParent(i);
					const uint32 slot = (uint32)(p + 1);
					rangDansNiveau[(uint32)i] = parNiveau[slot]++;
				}
				for (int32 i = 0; i < n; ++i) {
					editorkit::NkGraphNode nd;
					nd.id = idDe(i);
					nd.name = m.GetStateName(i);
					nd.parent = idDe(m.GetStateParent(i));
					nd.subMachine = m.IsSubMachine(i);
					const uint8 genre = m.GetStateRefKind(i);
					nd.clip = genre == 1 ? m.GetStateRef(i) : NkString();
					if (genre == 2 || genre == 3) {
						nd.blendTree = true;
						nd.blendDims = genre == 3 ? (uint8)2 : (uint8)1;
						if (const anim::NkBlendSpaceDef *bs = ctl != nullptr ? ctl->FindBlendSpace(m.GetStateRef(i)) : nullptr) {
							nd.blendDims = bs->dims >= 2 ? (uint8)2 : (uint8)1;
							nd.paramX = bs->paramX;
							nd.paramY = bs->paramY;
							for (uint32 k = 0; k < (uint32)bs->samples.Size(); ++k) {
								editorkit::NkGraphBlendSample s;
								s.clip = bs->samples[k].clip;
								s.x = bs->samples[k].x;
								s.y = bs->samples[k].y;
								nd.samples.PushBack(s);
							}
						}
					}
					nd.tag = m.GetStateTag(i);
					float32 x = 0.f, y = 0.f;
					if (m.GetStatePosition(i, x, y)) {
						nd.x = x;
						nd.y = y;
					} else {
						const int32 r = rangDansNiveau[(uint32)i];
						// Assez d'air entre deux etats pour lire le resume des conditions.
						nd.x = (float32)(r % 3) * 340.f;
						nd.y = (float32)(r / 3) * 170.f;
					}
					if (nd.subMachine) {
						const int32 e = m.GetEntryState(i);
						nd.entry = e >= 0 ? idDe(e) : 0;
					}
					g.nodes.PushBack(nd);
				}
				const int32 e = m.GetEntryState(SM::NK_ROOT);
				if (racine == 0) {
					g.rootEntry = e >= 0 ? idDe(e) : 0;
				} else if (editorkit::NkGraphNode *r = g.Node(racine)) {
					r->entry = e >= 0 ? idDe(e) : 0;
				}
				// Les parametres : une fois (ceux d'une couche sont ceux de la base).
				for (uint32 i = 0; i < m.GetParamCount(); ++i) {
					if (g.ParamIndex(m.GetParamName(i)) >= 0) {
						continue;
					}
					editorkit::NkGraphParam p;
					p.name = m.GetParamName(i);
					p.kind = (uint8)m.GetParamKind(i);
					p.defaultValue = m.GetParamDefault(i);
					p.value = p.defaultValue;
					g.params.PushBack(p);
				}
				for (uint32 t = 0; t < m.GetTransitionCount(); ++t) {
					editorkit::NkGraphTransition tr;
					tr.id = premier + (nk_uint64)n + t;
					tr.any = m.IsAnyStateTransition(t);
					const int32 from = m.GetTransitionFrom(t), to = m.GetTransitionTo(t), sc = m.GetTransitionScope(t);
					tr.from = (!tr.any && from >= 0) ? idDe(from) : 0;
					tr.to = to >= 0 ? idDe(to) : 0;
					tr.scope = tr.any ? idDe(sc) : 0;
					tr.fade = m.GetTransitionFade(t);
					tr.priority = m.GetTransitionPriority(t);
					tr.curve = (uint8)m.GetTransitionCurve(t);
					for (uint32 k = 0; k < m.GetConditionCount(t); ++k) {
						editorkit::NkGraphCondition c;
						SM::NkCondKind kind = SM::NkCondKind::BOOL_TRUE;
						m.GetCondition(t, k, c.param, kind, c.threshold);
						c.op = (uint8)kind;
						tr.conditions.PushBack(c);
					}
					// Une transition qui ne mene nulle part (definition abimee) ne se dessine pas.
					if (tr.to != 0 && (tr.any || tr.from != 0)) {
						g.transitions.PushBack(tr);
					}
				}
				g.nextId = premier + (nk_uint64)n + m.GetTransitionCount();
				// Les pseudo-noeuds du fichier ; les autres se placeront seuls.
				for (int32 lvl = -1; lvl < n; ++lvl) {
					if (lvl >= 0 && !m.IsSubMachine(lvl)) {
						continue;
					}
					float32 ex = 0.f, ey = 0.f, ax = 0.f, ay = 0.f;
					const bool a = m.GetPseudoPosition(lvl, 0, ex, ey);
					const bool b = m.GetPseudoPosition(lvl, 1, ax, ay);
					if (a || b) {
						const nk_uint64 machine = idDe(lvl);
						g.PlacePseudos(machine); // les places par defaut, puis celles du fichier
						editorkit::NkGraphPseudo &p = g.PseudoOf(machine);
						if (a) {
							p.entryX = ex;
							p.entryY = ey;
						}
						if (b) {
							p.anyX = ax;
							p.anyY = ay;
						}
					}
				}
			}

			/// Le nom de l'arbre de melange d'un noeud : son chemin (unique).
			NkString NomArbre(const editorkit::NkStateGraphModel &g, const editorkit::NkGraphNode &nd) {
				return g.PathOf(nd.id);
			}
		} // namespace

		void NkGrapheDepuisMachine(const anim::NkAnimStateMachine &m, editorkit::NkStateGraphModel &g,
								   NkVector<nk_uint64> &noeudDeEtat) {
			g = editorkit::NkStateGraphModel();
			AjouterMachine(m, nullptr, g, 0, &noeudDeEtat);
		}

		void NkGrapheDepuisControleur(const anim::NkAnimController &ctl, editorkit::NkStateGraphModel &g,
									  NkVector<nk_uint64> &noeudDeEtat) {
			g = editorkit::NkStateGraphModel();
			AjouterMachine(ctl.base, &ctl, g, 0, &noeudDeEtat);
			for (uint32 i = 0; i < (uint32)ctl.layers.Size(); ++i) {
				const anim::NkAnimLayer &L = ctl.layers[i];
				const nk_uint64 racine = g.AddLayer(L.name);
				if (editorkit::NkGraphNode *r = g.Node(racine)) {
					r->layerWeight = L.weight;
					r->layerAdditive = L.mode == anim::NkLayerMode::NK_ADDITIVE;
					r->layerWeightParam = L.weightParam;
					// Le masque d'Unkeny est nomme de l'objet qu'il couvre.
					const anim::NkAnimMask *mk = ctl.FindMask(L.mask);
					r->layerMask = mk != nullptr && !mk->entries.Empty() ? mk->entries[0].path : L.mask;
				}
				AjouterMachine(L.machine, &ctl, g, racine, nullptr);
			}
			g.undoStack.Clear();
			g.redoStack.Clear();
		}

		bool NkMachineDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimStateMachine &m,
								   NkVector<nk_uint64> &noeudDeEtat, nk_uint64 racine) {
			m = SM();
			noeudDeEtat.Clear();
			// Les noeuds de CETTE machine : sous `racine` (0 = la base, hors couches).
			auto dedans = [&](const editorkit::NkGraphNode &nd) { return !nd.layerRoot && g.LayerOf(nd.id) == racine; };
			bool un = false;
			for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
				un = un || dedans(g.nodes[i]);
			}
			if (!un) {
				return false;
			}
			// Les parents AVANT leurs enfants (c'est ce que la machine exige) : par
			// profondeur croissante, dans l'ordre du graphe a profondeur egale.
			NkVector<int32> indexDe; // noeud (indice dans g.nodes) -> etat
			indexDe.Resize((uint32)g.nodes.Size());
			for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
				indexDe[i] = -1;
			}
			auto etatDe = [&](nk_uint64 id) -> int32 {
				const int32 k = g.NodeIndex(id);
				return k >= 0 ? indexDe[(uint32)k] : -1;
			};
			for (int32 profondeur = 0; profondeur < SM::NK_MAX_DEPTH; ++profondeur) {
				for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
					const editorkit::NkGraphNode &nd = g.nodes[i];
					if (!dedans(nd)) {
						continue;
					}
					int32 p = 0;
					for (const editorkit::NkGraphNode *x = g.Node(nd.parent); x != nullptr && x->id != racine && p < 64;
						 x = g.Node(x->parent)) {
						++p;
					}
					if (p != profondeur) {
						continue;
					}
					const int32 parent = nd.parent != racine ? etatDe(nd.parent) : SM::NK_ROOT;
					if (nd.parent != racine && parent < 0) {
						continue; // un parent perdu : l'enfant ne s'ecrit pas
					}
					const int32 idx = nd.subMachine ? m.AddSubMachine(nd.name, parent) : m.AddEmptyState(nd.name, parent);
					if (idx < 0) {
						continue;
					}
					indexDe[i] = idx;
					if (!nd.subMachine) {
						if (nd.blendTree) {
							m.SetStateBlendRef(idx, NomArbre(g, nd), nd.blendDims);
						} else if (!nd.clip.Empty()) {
							m.SetStateClipRef(idx, nd.clip);
						}
						m.SetStateTag(idx, nd.tag);
					}
					m.SetStatePosition(idx, nd.x, nd.y);
				}
			}
			noeudDeEtat.Resize((uint32)m.GetStateCount());
			for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
				if (indexDe[i] >= 0) {
					noeudDeEtat[(uint32)indexDe[i]] = g.nodes[i].id;
				}
			}
			// Les entrees : celle de la racine (de la base ou de la couche), puis
			// celle de chaque sous-machine.
			const nk_uint64 entreeRacine = racine == 0 ? g.rootEntry : (g.Node(racine) != nullptr ? g.Node(racine)->entry : 0);
			if (entreeRacine != 0 && etatDe(entreeRacine) >= 0) {
				m.SetEntryState(SM::NK_ROOT, etatDe(entreeRacine));
			}
			for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
				const editorkit::NkGraphNode &nd = g.nodes[i];
				if (nd.subMachine && nd.entry != 0 && indexDe[i] >= 0 && etatDe(nd.entry) >= 0) {
					m.SetEntryState(indexDe[i], etatDe(nd.entry));
				}
			}
			// Les parametres, declares d'abord : c'est l'ordre de l'inspecteur.
			for (uint32 i = 0; i < (uint32)g.params.Size(); ++i) {
				m.DeclareParam(g.params[i].name, (SM::NkParamKind)g.params[i].kind, g.params[i].defaultValue);
			}
			for (uint32 i = 0; i < (uint32)g.transitions.Size(); ++i) {
				const editorkit::NkGraphTransition &tr = g.transitions[i];
				const int32 to = etatDe(tr.to);
				if (to < 0) {
					continue; // une cible d'une autre machine (ou perdue)
				}
				int32 t = -1;
				if (tr.any) {
					t = m.AddAnyStateTransition(tr.scope != racine ? etatDe(tr.scope) : SM::NK_ROOT, to, tr.fade, tr.priority);
				} else if (etatDe(tr.from) >= 0) {
					t = m.AddTransitionEx(etatDe(tr.from), to, tr.fade, tr.priority);
				}
				if (t >= 0) {
					m.SetTransitionCurve(t, (anim::NkFadeCurve)(tr.curve < (uint8)anim::NkFadeCurve::NK_COUNT ? tr.curve : 0u));
				}
				for (uint32 k = 0; t >= 0 && k < (uint32)tr.conditions.Size(); ++k) {
					const editorkit::NkGraphCondition &c = tr.conditions[k];
					m.AddCondition(t, c.param, (SM::NkCondKind)c.op, c.threshold);
				}
			}
			// La disposition des pseudo-noeuds.
			for (uint32 i = 0; i < (uint32)g.pseudos.Size(); ++i) {
				const editorkit::NkGraphPseudo &p = g.pseudos[i];
				const bool racineIci = p.machine == racine;
				const int32 machine = racineIci ? SM::NK_ROOT : etatDe(p.machine);
				if (!racineIci && machine < 0) {
					continue;
				}
				m.SetPseudoPosition(machine, 0, p.entryX, p.entryY);
				m.SetPseudoPosition(machine, 1, p.anyX, p.anyY);
			}
			return true;
		}

		bool NkControleurDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimController &ctl,
									  NkVector<nk_uint64> &noeudDeEtat) {
			ctl = anim::NkAnimController();
			if (!NkMachineDepuisGraphe(g, ctl.base, noeudDeEtat, 0)) {
				return false;
			}
			NkVector<nk_uint64> ignore;
			for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
				const editorkit::NkGraphNode &nd = g.nodes[i];
				if (nd.blendTree) {
					anim::NkBlendSpaceDef &bs = ctl.AddBlendSpace(NomArbre(g, nd), nd.blendDims, nd.paramX, nd.paramY);
					for (uint32 k = 0; k < (uint32)nd.samples.Size(); ++k) {
						bs.AddSample(nd.samples[k].clip, nd.samples[k].x, nd.samples[k].y);
					}
				}
				if (!nd.layerRoot) {
					continue;
				}
				anim::NkAnimLayer *L = ctl.AddLayer(nd.name,
													nd.layerAdditive ? anim::NkLayerMode::NK_ADDITIVE : anim::NkLayerMode::NK_OVERRIDE,
													nd.layerWeight, nd.layerMask);
				if (L == nullptr) {
					continue; // au-dela de NK_ANIM_MAX_LAYERS
				}
				L->weightParam = nd.layerWeightParam;
				if (!NkMachineDepuisGraphe(g, L->machine, ignore, nd.id)) {
					// Une couche sans etat : ses parametres seulement (elle ne joue rien).
					for (uint32 p = 0; p < (uint32)g.params.Size(); ++p) {
						L->machine.DeclareParam(g.params[p].name, (SM::NkParamKind)g.params[p].kind, g.params[p].defaultValue);
					}
				}
				if (!nd.layerMask.Empty()) {
					anim::NkAnimMask &mk = ctl.AddMask(nd.layerMask);
					if (mk.entries.Empty()) {
						anim::NkAnimMask::Entry e;
						e.path = nd.layerMask;
						mk.entries.PushBack(e);
					}
				}
			}
			return true;
		}

		void NkEditeurAnimationsDuContenu(NkEditeurModele &m, NkVector<NkString> &noms) {
			noms.Clear();
			NkVector<NkString> dossiers;
			NkEditeurDossiersContenu(m, dossiers);
			dossiers.Insert(dossiers.Begin(), NkString());
			NkVector<NkElementContenu> elems;
			for (uint32 d = 0; d < (uint32)dossiers.Size(); ++d) {
				NkEditeurListerContenu(m, dossiers[d].CStr(), elems);
				for (uint32 i = 0; i < (uint32)elems.Size(); ++i) {
					if (!elems[i].dossier && elems[i].nature.type == NkAssetType::Animation) {
						noms.PushBack(Tronc(elems[i].nom.CStr()));
					}
				}
			}
		}

		// =====================================================================
		// OUVRIR, FERMER, ENREGISTRER
		// =====================================================================
		bool NkEditeurOuvrirAnimateur(NkEditeurModele &m, NkEditeurInterface &ui, const char *chemin) {
			const NkString c(chemin != nullptr ? chemin : "");
			if (NkDocAnim *deja = DocParChemin(ui, c)) {
				// Ouvert deux fois : son onglet REVIENT au premier plan.
				NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, deja->id));
				return true;
			}
			anim::NkAnimController machine; // (01/10 soir) le controleur : base, couches, arbres
			if (!c.Empty() && !machine.LoadBinary(c)) {
				NkEditeurAnnoncer(m, NkString::Format("Animateur : %s ne se lit pas", c.CStr()).CStr());
				return false;
			}
			NkDocAnim &d = NouveauDoc(m, ui, NkGenreDocAnim::NK_ANIMATEUR);
			d.chemin = c;
			d.nom = c.Empty() ? NkString("NouveauControleur") : Tronc(c.CStr());
			if (!c.Empty()) {
				NkGrapheDepuisControleur(machine, d.graphe, d.noeudDeEtat);
				// La machine relue sert AUSSI l'apercu : elle est enregistree sous son
				// nom, celui que l'entite observee doit porter.
				unkeny::NkEnregistrerControleurAnimateur(d.nom.CStr(), machine);
			}
			NkEditeurAnimationsDuContenu(m, d.graphe.clips);
			d.revisionEnregistree = d.graphe.revision;
			NkEditeurAnnoncer(m, NkString::Format("Animateur : %s", d.nom.CStr()).CStr());
			return true;
		}

		bool NkEditeurOuvrirAnimation(NkEditeurModele &m, NkEditeurInterface &ui, const char *chemin) {
			const NkString c(chemin != nullptr ? chemin : "");
			if (NkDocAnim *deja = DocParChemin(ui, c)) {
				// Ouvert deux fois : son onglet REVIENT au premier plan.
				NkEditeurActiverDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, deja->id));
				return true;
			}
			anim::NkAnimationClip clip;
			if (!c.Empty() && !clip.LoadBinary(c)) {
				NkEditeurAnnoncer(m, NkString::Format("Animation : %s ne se lit pas", c.CStr()).CStr());
				return false;
			}
			NkDocAnim &d = NouveauDoc(m, ui, NkGenreDocAnim::NK_ANIMATION);
			d.chemin = c;
			if (c.Empty()) {
				const NkString n = NomEntite(m, m.aSelection ? m.selection : ecs::NkEntityId());
				d.nom = n.Empty() ? NkString("NouvelleAnimation") : n;
				clip.fps = 30.f;
				clip.duration = 1.f;
			} else {
				d.nom = Tronc(c.CStr());
			}
			clip.name = d.nom;
			d.clip = clip;
			NkFriseDepuisClip(clip, d.frise);
			d.revisionClip = d.frise.revision;
			d.revisionEnregistree = d.frise.revision;
			if (!c.Empty()) {
				unkeny::NkEnregistrerClipProprietes(d.nom.CStr(), clip);
			}
			NkEditeurAnnoncer(m, NkString::Format("Animation : %s", d.nom.CStr()).CStr());
			return true;
		}

		bool NkEditeurOuvrirAnimateur(NkEditeurCadre &c, const char *chemin) {
			return NkEditeurOuvrirAnimateur(c.m, c.ui, chemin);
		}

		bool NkEditeurOuvrirAnimation(NkEditeurCadre &c, const char *chemin) {
			return NkEditeurOuvrirAnimation(c.m, c.ui, chemin);
		}

		bool NkEditeurFermerDocAnim(NkEditeurModele &m, NkEditeurInterface &ui, nk_uint64 id) {
			return NkEditeurFermerDocument(m, ui, NkDoc(NkGenreDocument::NK_ANIM, id));
		}

		bool NkEditeurDetruireDocAnim(NkEditeurModele &m, NkEditeurInterface &ui, nk_uint64 id) {
			for (uint32 i = 0; i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
				if (ui.pagesAnim.docs[i].id != id) {
					continue;
				}
				NkEditeurRendreApercu(m, ui.pagesAnim.docs[i]);
				ui.pagesAnim.docs.Erase(ui.pagesAnim.docs.Begin() + i);
				return true;
			}
			return false;
		}

		NkDocAnim *NkEditeurDocAnimActif(NkEditeurInterface &ui) {
			const NkDocumentOuvert d = NkEditeurDocumentActif(ui);
			return d.genre == NkGenreDocument::NK_ANIM ? DocParId(ui, d.cle) : nullptr;
		}

		bool NkEditeurPageAnimOuverte(const NkEditeurInterface &ui) noexcept {
			const NkDocumentOuvert d = NkEditeurDocumentActif(ui);
			for (uint32 i = 0; d.genre == NkGenreDocument::NK_ANIM && i < (uint32)ui.pagesAnim.docs.Size(); ++i) {
				if (ui.pagesAnim.docs[i].id == d.cle) {
					return true;
				}
			}
			return false;
		}

		bool NkEditeurEnregistrerDocAnim(NkEditeurModele &m, NkDocAnim &d) {
			if (d.chemin.Empty()) {
				// Un document neuf va dans Contenu/Animations, sous son nom.
				NkString dossier = NkEditeurDossierContenu(m);
				dossier.Append("/Animations");
				NkDirectory::CreateRecursive(dossier.CStr());
				NkString nom = d.nom;
				nom.Append(".");
				nom.Append(NkAssetExtensionFor(d.genre == NkGenreDocAnim::NK_ANIMATION ? NkAssetType::Animation
																						: NkAssetType::AnimationController));
				d.chemin = editorkit::NkDisqueCheminLibre(dossier.CStr(), nom.CStr());
				d.nom = Tronc(d.chemin.CStr());
			}
			bool ok = false;
			if (d.genre == NkGenreDocAnim::NK_ANIMATION) {
				NkClipDepuisFrise(d.frise, d.clip);
				d.clip.name = d.nom;
				ok = !d.chemin.Empty() && d.clip.SaveBinary(d.chemin);
				if (ok) {
					unkeny::NkEnregistrerClipProprietes(d.nom.CStr(), d.clip);
					d.revisionEnregistree = d.frise.revision;
				}
			} else {
				anim::NkAnimController machine;
				if (!NkControleurDepuisGraphe(d.graphe, machine, d.noeudDeEtat)) {
					NkEditeurAnnoncer(m, "Animateur : un controleur sans etat ne s'enregistre pas");
					return false;
				}
				ok = !d.chemin.Empty() && machine.SaveBinary(d.chemin);
				if (ok) {
					unkeny::NkEnregistrerControleurAnimateur(d.nom.CStr(), machine);
					d.revisionEnregistree = d.graphe.revision;
				}
			}
			d.modifie = !ok;
			NkEditeurAnnoncer(m, ok ? NkString::Format("Enregistré : %s", d.chemin.CStr()).CStr()
									: NkString::Format("Écriture impossible : %s", d.chemin.CStr()).CStr());
			return ok;
		}

		ecs::NkEntityId NkEditeurCibleDoc(NkEditeurModele &m, const NkDocAnim &d) {
			return d.cibleUid != 0 ? m.scene.EntiteParUid(d.cibleUid) : ecs::NkEntityId();
		}

		// =====================================================================
		// LES ONGLETS : (2026-10-02) dans la barre unique, NkEditeurDocuments.cpp
		// =====================================================================

		// =====================================================================
		// LA PAGE AU PREMIER PLAN
		// =====================================================================
		bool NkEditeurDessinerPageAnim(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			NkPagesAnim &pa = ui.pagesAnim;
			NkDocAnim *d = NkEditeurDocAnimActif(ui);
			// Les documents qui ne sont PAS au premier plan s'entretiennent : un
			// apercu quitte rend l'entite, un controleur suit le jeu.
			for (uint32 i = 0; i < (uint32)pa.docs.Size(); ++i) {
				if (&pa.docs[i] != d) {
					NkEditeurEntretenirDocAnim(c, pa.docs[i], false);
				}
			}
			if (d == nullptr) {
				return false;
			}
			// Le corps ENTIER sous les onglets (la barre d'outils comprise : la page
			// a la sienne, comme les editeurs d'assets d'Unreal), jusqu'a la barre d'etat.
			const NkRect zone{ui.ecran.x, ui.barreOutils.y, ui.ecran.w, ui.statut.y - ui.barreOutils.y};
			pa.page = zone;
			c.ctx.dl.AddRectFilled(zone, c.pal.fond);
			if (d->genre == NkGenreDocAnim::NK_ANIMATION) {
				NkEditeurDessinerPageAnimation(c, *d, zone);
			} else {
				NkEditeurDessinerPageAnimateur(c, *d, zone);
			}
			return true;
		}

		// =====================================================================
		// LE CLAVIER
		// =====================================================================
		bool NkEditeurPageAnimAuClavier(NkEditeurCadre &c) {
			NkDocAnim *d = NkEditeurDocAnimActif(c.ui);
			if (d == nullptr) {
				return false;
			}
			const nkgui::NkGuiInput &in = c.ctx.input;
			using nkgui::NkGuiKey;
			// Un nom en cours de saisie garde TOUTES ses touches (la page le valide).
			if (d->renomme) {
				return true;
			}
			if (in.ctrlDown) {
				if (in.KeyPressed(NkGuiKey::S)) {
					NkEditeurEnregistrerDocAnim(c.m, *d);
				} else if (in.KeyPressed(NkGuiKey::Z) || in.KeyPressed(NkGuiKey::Y)) {
					const bool refaire = in.KeyPressed(NkGuiKey::Y) || in.shiftDown;
					if (d->genre == NkGenreDocAnim::NK_ANIMATION) {
						refaire ? d->frise.Redo() : d->frise.Undo();
					} else {
						refaire ? d->graphe.Redo() : d->graphe.Undo();
					}
				} else if (in.KeyPressed(NkGuiKey::A) && d->genre == NkGenreDocAnim::NK_ANIMATION) {
					d->frise.SelectAll();
				} else if (d->genre == NkGenreDocAnim::NK_ANIMATION && in.KeyPressed(NkGuiKey::C)) {
					d->frise.CopySelection(); // (01/10 soir) copier / couper / coller des cles
				} else if (d->genre == NkGenreDocAnim::NK_ANIMATION && in.KeyPressed(NkGuiKey::X)) {
					d->frise.CutSelection();
				} else if (d->genre == NkGenreDocAnim::NK_ANIMATION && in.KeyPressed(NkGuiKey::V)) {
					d->frise.PasteAt(d->frise.cursor);
				} else if (in.KeyPressed(NkGuiKey::W)) {
					NkEditeurFermerDocAnim(c.m, c.ui, d->id);
				}
				return true;
			}
			if (in.KeyPressed(NkGuiKey::Escape)) {
				if (d->choix) {
					d->choix = false;
				} else if (d->genre == NkGenreDocAnim::NK_ANIMATEUR && c.m.etat != NkEtatJeu::NK_EDITION) {
					NkEditeurExecuter(c, NK_A_ARRETER);
				} else {
					NkEditeurActiverDocument(c.m, c.ui, NkDocScene()); // Echap rend la scene
				}
				return true;
			}
			if (d->genre == NkGenreDocAnim::NK_ANIMATION) {
				editorkit::NkTimelineModel &f = d->frise;
				if (in.KeyPressed(NkGuiKey::Space)) {
					f.TogglePlay();
				} else if (in.KeyPressed(NkGuiKey::Delete) || in.KeyPressed(NkGuiKey::Backspace)) {
					f.DeleteSelection();
				} else if (in.KeyPressed(NkGuiKey::Home)) {
					f.SetCursor(0.f);
				} else if (in.KeyPressed(NkGuiKey::End)) {
					f.SetCursor(f.duration);
				} else if (in.KeyPressed(NkGuiKey::Left)) {
					f.SetCursor(f.cursor - f.FrameDuration());
				} else if (in.KeyPressed(NkGuiKey::Right)) {
					f.SetCursor(f.cursor + f.FrameDuration());
				} else if (in.KeyPressed(NkGuiKey::F)) {
					f.FrameAll();
				}
				return true;
			}
			editorkit::NkStateGraphModel &g = d->graphe;
			if (in.KeyPressed(NkGuiKey::Space)) {
				NkEditeurExecuter(c, c.m.etat == NkEtatJeu::NK_EDITION ? NK_A_JOUER : NK_A_ARRETER);
			} else if (in.KeyPressed(NkGuiKey::Delete)) {
				if (g.selectedTransition != 0) {
					g.RemoveTransition(g.selectedTransition);
				} else if (g.Node(g.selectedNode) != nullptr) {
					g.RemoveNode(g.selectedNode);
					g.selectedNode = 0;
				}
			} else if (in.KeyPressed(NkGuiKey::F)) {
				g.FrameLevel(c.ui.pagesAnim.graphe.canvas.w, c.ui.pagesAnim.graphe.canvas.h);
			} else if (in.KeyPressed(NkGuiKey::F2) && g.Node(g.selectedNode) != nullptr) {
				d->renomme = true;
				d->renommeNoeud = g.selectedNode;
				d->renommeParam = -1;
				std::snprintf(d->tampon, sizeof(d->tampon), "%s", g.Node(g.selectedNode)->name.CStr());
				d->renommeChoisir = true;
				for (uint32 i = 0; i < (uint32)c.ui.pagesAnim.graphe.nodeRects.Size(); ++i) {
					if (c.ui.pagesAnim.graphe.nodeRects[i].id == g.selectedNode) {
						const editorkit::NkPaintRect &r = c.ui.pagesAnim.graphe.nodeRects[i].rect;
						d->renommeRect = NkRect{r.x, r.y, r.w, r.h};
					}
				}
			}
			return true;
		}

		// =====================================================================
		// LES ACTIONS (Fenetre > Animation / Animateur)
		// =====================================================================
		bool NkEditeurActionAnim(NkEditeurCadre &c, int32 action) {
			if (action != NK_A_ANIM_ANIMATION && action != NK_A_ANIM_ANIMATEUR) {
				return false;
			}
			NkEditeurModele &m = c.m;
			const bool sel = m.aSelection && m.scene.Monde().IsAlive(m.selection);
			if (action == NK_A_ANIM_ANIMATION) {
				// Le clip que l'entite joue deja, s'il est dans le Contenu ; sinon un neuf.
				if (sel) {
					if (const unkeny::NkClipProprietes2D *cp = m.scene.Monde().Get<unkeny::NkClipProprietes2D>(m.selection)) {
						NkString chemin = NkEditeurDossierContenu(m);
						chemin.Append(NkString::Format("/Animations/%s.%s", cp->clip, NkAssetExtensionFor(NkAssetType::Animation)));
						if (NkFile::Exists(chemin.CStr())) {
							return NkEditeurOuvrirAnimation(c, chemin.CStr()) || true;
						}
					}
				}
				NkEditeurOuvrirAnimation(c, nullptr);
				return true;
			}
			// L'Animateur de l'entite : son modele, lu du Contenu s'il y est.
			if (sel) {
				if (const unkeny::NkAnimateur2D *a = m.scene.Monde().Get<unkeny::NkAnimateur2D>(m.selection)) {
					NkString chemin = NkEditeurDossierContenu(m);
					chemin.Append(NkString::Format("/Animations/%s.%s", a->modele, NkAssetExtensionFor(NkAssetType::AnimationController)));
					if (NkFile::Exists(chemin.CStr())) {
						return NkEditeurOuvrirAnimateur(c, chemin.CStr()) || true;
					}
					// Un modele enregistre par le jeu (« plateforme ») : son graphe, a enregistrer.
					if (const anim::NkAnimController *mod = unkeny::NkControleurAnimateur(a->modele)) {
						NkEditeurOuvrirAnimateur(c, nullptr);
						if (NkDocAnim *d = NkEditeurDocAnimActif(c.ui)) {
							d->nom = a->modele;
							NkGrapheDepuisControleur(*mod, d->graphe, d->noeudDeEtat);
							NkEditeurAnimationsDuContenu(m, d->graphe.clips);
							d->revisionEnregistree = d->graphe.revision;
							NkEditeurAnnoncer(m, NkString::Format("Animateur : %s (modele du jeu, a enregistrer)", d->nom.CStr()).CStr());
						}
						return true;
					}
				}
			}
			NkEditeurOuvrirAnimateur(c, nullptr);
			return true;
		}

	} // namespace editeur
} // namespace nkentseu
