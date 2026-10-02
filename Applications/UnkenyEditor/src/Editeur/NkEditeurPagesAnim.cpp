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
#include "Unkeny/Squelette/NkUnkenySquelette.h"
#include "NKEditorKit/Components/NkStateGraphAnima.h" // (02/10) graphe <-> controleur, partage

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
		// (2026-10-02, R30) LES PISTES D'OS
		// =====================================================================
		NkString NkCheminOsDuClip(const anim::NkAnimationClip &clip, uint32 j) {
			NkString chemin;
			uint32 pile[64];
			uint32 n = 0;
			for (int32 o = static_cast<int32>(j); o >= 0 && n < 64u && static_cast<uint32>(o) < clip.boneCount;
				 o = static_cast<uint32>(o) < (uint32)clip.jointParent.Size() ? clip.jointParent[(uint32)o] : -1) {
				pile[n++] = static_cast<uint32>(o);
			}
			chemin = NK_FRISE_OBJET_SQUELETTE;
			for (int32 k = static_cast<int32>(n) - 1; k >= 0; --k) {
				chemin.Append("/");
				const uint32 o = pile[k];
				if (o < (uint32)clip.jointNames.Size() && !clip.jointNames[o].Empty()) {
					chemin.Append(clip.jointNames[o].CStr());
				} else if (o < (uint32)clip.boneTracks.Size() && !clip.boneTracks[o].name.Empty()) {
					chemin.Append(clip.boneTracks[o].name.CStr());
				} else {
					chemin.Append(NkString::Format("os %u", o).CStr());
				}
			}
			return chemin;
		}

		NkString NkNomOsDePiste(const NkString &objet) {
			const char *s = objet.CStr();
			const char *d = std::strrchr(s, '/');
			return NkString(d != nullptr ? d + 1 : s);
		}

		int32 NkOsDuClipParNom(const anim::NkAnimationClip &clip, const NkString &nom) {
			for (uint32 j = 0; j < clip.boneCount; ++j) {
				if ((j < (uint32)clip.jointNames.Size() && clip.jointNames[j] == nom) ||
					(j < (uint32)clip.boneTracks.Size() && clip.boneTracks[j].name == nom)) {
					return static_cast<int32>(j);
				}
			}
			return -1;
		}

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
			// (2026-10-02, R30) Les pistes d'OS : une par os CLE (angle en degres, x, y
			// de sa place locale), sous « Squelette/... » (l'arbre des os).
			if (clip.skeletalLocal) {
				for (uint32 j = 0; j < clip.boneCount && j < (uint32)clip.boneTracks.Size(); ++j) {
					const anim::NkAnimationTrack<math::NkMat4f> &bt = clip.boneTracks[j];
					if (bt.Empty()) {
						continue;
					}
					editorkit::NkTimelineTrack t;
					t.id = NK_FRISE_ID_OS + (nk_uint64)j;
					t.object = NkCheminOsDuClip(clip, j);
					t.property = NK_FRISE_PROPRIETE_OS;
					t.label = NkString::Format("Os %s", NkNomOsDePiste(t.object).CStr());
					t.channels = 3;
					t.channelNames[0] = "Angle";
					t.channelNames[1] = "X";
					t.channelNames[2] = "Y";
					for (uint32 k = 0; k < bt.KeyCount(); ++k) {
						const anim::NkKeyframe<math::NkMat4f> &kf = bt.GetKey(k);
						const anim::NkBone2D b = anim::NkBone2DFromMatrix(kf.value);
						editorkit::NkTimelineKey key;
						key.time = kf.time;
						key.v[0] = b.angle * 180.f / 3.14159265f;
						key.v[1] = b.x;
						key.v[2] = b.y;
						key.interp = (uint8)kf.interp;
						t.keys.PushBack(key);
					}
					t.muted = !bt.enabled;
					frise.tracks.PushBack(t);
				}
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
			// (2026-10-02, R30) Les pistes d'OS refont les pistes d'os du clip (l'echelle
			// de chaque cle est reprise de l'ancien clip : la frise n'en montre pas).
			if (clip.skeletalLocal && clip.boneCount > 0u) {
				const anim::NkAnimationClip ancien = clip;
				NkVector<uint8> vu;
				vu.Resize(clip.boneCount, 0u);
				for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
					const editorkit::NkTimelineTrack &t = frise.tracks[i];
					if (!(t.property == NkString(NK_FRISE_PROPRIETE_OS))) {
						continue;
					}
					const int32 j = NkOsDuClipParNom(clip, NkNomOsDePiste(t.object));
					if (j < 0 || (uint32)j >= (uint32)clip.boneTracks.Size()) {
						continue;
					}
					vu[(uint32)j] = 1u;
					anim::NkAnimationTrack<math::NkMat4f> neuf;
					neuf.name = clip.boneTracks[(uint32)j].name;
					neuf.enabled = !t.muted;
					for (uint32 k = 0; k < (uint32)t.keys.Size(); ++k) {
						const editorkit::NkTimelineKey &key = t.keys[k];
						anim::NkBone2D b = ancien.boneTracks[(uint32)j].Empty() ? anim::NkBone2D() : anim::NkSampleBone2D(ancien, (uint32)j, key.time);
						b.angle = key.v[0] * 3.14159265f / 180.f;
						b.x = key.v[1];
						b.y = key.v[2];
						neuf.AddKey(key.time, anim::NkBone2DToMatrix(b),
									(anim::NkInterpMode)(key.interp < (uint8)editorkit::NkTimelineInterp::Count ? key.interp : 1u));
					}
					clip.boneTracks[(uint32)j] = neuf;
				}
				// Une piste d'os RETIREE de la frise : l'os n'est plus cle.
				for (uint32 j = 0; j < clip.boneCount && j < (uint32)clip.boneTracks.Size(); ++j) {
					if (vu[j] == 0u && !clip.boneTracks[j].Empty()) {
						anim::NkAnimationTrack<math::NkMat4f> vide;
						vide.name = clip.boneTracks[j].name;
						clip.boneTracks[j] = vide;
					}
				}
			}
			for (uint32 i = 0; i < (uint32)frise.tracks.Size(); ++i) {
				const editorkit::NkTimelineTrack &t = frise.tracks[i];
				if (t.property == NkString(NK_FRISE_PROPRIETE_OS)) {
					continue; // (R30) une piste d'os : faite ci-dessus
				}
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

		// (2026-10-02) Les conversions graphe <-> controleur sont PARTAGEES avec
		// NkAnimaEditor : NKEditorKit/Components/NkStateGraphAnima.h. Ces noms
		// restent ceux d'UnkenyEditor (le banc et les pages les appellent).
		void NkGrapheDepuisMachine(const anim::NkAnimStateMachine &m, editorkit::NkStateGraphModel &g,
								   NkVector<nk_uint64> &noeudDeEtat) {
			editorkit::anima::NkGrapheDepuisMachine(m, g, noeudDeEtat);
		}

		void NkGrapheDepuisControleur(const anim::NkAnimController &ctl, editorkit::NkStateGraphModel &g,
									  NkVector<nk_uint64> &noeudDeEtat) {
			editorkit::anima::NkGrapheDepuisControleur(ctl, g, noeudDeEtat);
		}

		bool NkMachineDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimStateMachine &m,
								   NkVector<nk_uint64> &noeudDeEtat, nk_uint64 racine) {
			return editorkit::anima::NkMachineDepuisGraphe(g, m, noeudDeEtat, racine);
		}

		bool NkControleurDepuisGraphe(const editorkit::NkStateGraphModel &g, anim::NkAnimController &ctl,
									  NkVector<nk_uint64> &noeudDeEtat) {
			return editorkit::anima::NkControleurDepuisGraphe(g, ctl, noeudDeEtat);
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
