#pragma once
// -----------------------------------------------------------------------------
// @File    NkStateGraphAnima.h
// @Brief   LE GRAPHE D'ETATS du kit <-> LE CONTROLEUR de NKAnima : base,
//          COUCHES, ARBRES DE MELANGE, courbes de fondu, masques d'objets.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
//  (2026-10-02) ECRIT D'ABORD DANS UnkenyEditor (NkEditeurPagesAnim.cpp), puis
//  PARTAGE ici quand NkAnimaEditor a branche le meme graphe : deux copies d'une
//  conversion divergent (« deux timelines divergeront », NKScena/ROADMAP.md).
//
//  ⚠️ EN-TETE SEUL, ET LE KIT NE LE COMPILE PAS : NKEditorKit ne depend pas de
//     NKAnima (NKEditorKit.jenga). Ce fichier est inclus par un HOTE qui lie les
//     deux (UnkenyEditor, NkAnimaEditor -- dans une unite qui n'inclut PAS
//     NKRenderer, voir AnimBridge.h).
//
//  Une COUCHE du graphe est une sous-machine cachee de la racine (`layerRoot`) ;
//  un ARBRE est un etat de genre 2 (1D) ou 3 (2D) de NKAnima dont le
//  NkBlendSpaceDef porte le CHEMIN du noeud ; le masque d'une couche est celui de
//  l'objet nomme (« Torse » couvre « Torse » et ses descendants).
// -----------------------------------------------------------------------------

#include "NKAnima/Blend/NkAnimMix.h"
#include "NKAnima/Clip/NkAnimation.h"
#include "NKEditorKit/Components/NkStateGraphModel.h"

namespace nkentseu {
	namespace editorkit {
		namespace anima {

		// (01/10 soir) LE CONTROLEUR : la machine de base, les COUCHES (rangees dans
		// le graphe comme des sous-machines cachees de la racine, `layerRoot`), les
		// ARBRES DE MELANGE (un etat de genre 2 ou 3 de NKAnima, son NkBlendSpaceDef
		// nomme du chemin du noeud), les courbes de fondu, et les masques (un masque
		// par objet nomme : « Torse » couvre « Torse » et ses descendants).
		namespace detail {
			using SM = anim::NkAnimStateMachine;

			/// Ajoute au graphe la machine `m` sous `racine` (0 = la base). Les identites
			/// suivent `g.nextId` ; `noeudDeEtat` (facultatif) recoit etat -> noeud.
			inline void AjouterMachine(const SM &m, const anim::NkAnimController *ctl, editorkit::NkStateGraphModel &g, nk_uint64 racine,
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
			inline NkString NomArbre(const editorkit::NkStateGraphModel &g, const editorkit::NkGraphNode &nd) {
				return g.PathOf(nd.id);
			}
		} // namespace detail

		inline void NkGrapheDepuisMachine(const anim::NkAnimStateMachine &m, editorkit::NkStateGraphModel &g,
								   NkVector<nk_uint64> &noeudDeEtat) {
			g = editorkit::NkStateGraphModel();
			detail::AjouterMachine(m, nullptr, g, 0, &noeudDeEtat);
		}

		inline void NkGrapheDepuisControleur(const anim::NkAnimController &ctl, editorkit::NkStateGraphModel &g,
									  NkVector<nk_uint64> &noeudDeEtat) {
			g = editorkit::NkStateGraphModel();
			detail::AjouterMachine(ctl.base, &ctl, g, 0, &noeudDeEtat);
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
				detail::AjouterMachine(L.machine, &ctl, g, racine, nullptr);
			}
			g.undoStack.Clear();
			g.redoStack.Clear();
		}

		inline bool NkMachineDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimStateMachine &m,
								   NkVector<nk_uint64> &noeudDeEtat, nk_uint64 racine = 0) {
			using SM = anim::NkAnimStateMachine;
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
							m.SetStateBlendRef(idx, detail::NomArbre(g, nd), nd.blendDims);
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

		inline bool NkControleurDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimController &ctl,
									  NkVector<nk_uint64> &noeudDeEtat) {
			ctl = anim::NkAnimController();
			if (!NkMachineDepuisGraphe(g, ctl.base, noeudDeEtat, 0)) {
				return false;
			}
			NkVector<nk_uint64> ignore;
			for (uint32 i = 0; i < (uint32)g.nodes.Size(); ++i) {
				const editorkit::NkGraphNode &nd = g.nodes[i];
				if (nd.blendTree) {
					anim::NkBlendSpaceDef &bs = ctl.AddBlendSpace(detail::NomArbre(g, nd), nd.blendDims, nd.paramX, nd.paramY);
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
						L->machine.DeclareParam(g.params[p].name, (anim::NkAnimStateMachine::NkParamKind)g.params[p].kind, g.params[p].defaultValue);
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


		} // namespace anima
	} // namespace editorkit
} // namespace nkentseu
