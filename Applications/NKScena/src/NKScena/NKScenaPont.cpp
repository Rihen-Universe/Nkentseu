// -----------------------------------------------------------------------------
// @File    NKScenaPont.cpp
// @Brief   Le pont frise <-> sequence de Noge (voir NKScenaPont.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKScena/NKScenaPont.h"

#include "Noge/Sequencer/NkSequencer.h"

namespace nkentseu {
	namespace nkscena {

		using editorkit::NkTimelineClip;
		using editorkit::NkTimelineInterp;
		using editorkit::NkTimelineKey;
		using editorkit::NkTimelineMarker;
		using editorkit::NkTimelineModel;
		using editorkit::NkTimelineTangent;
		using editorkit::NkTimelineTrack;
		using editorkit::NkTimelineValueKind;

		namespace {
			const char *const kProprietes[3] = {kNkScenaPosition, kNkScenaRotation, kNkScenaEchelle};
			const char *const kPrefixes[3] = {"localPosition", "localRotation", "localScale"};
			const char *const kAxes[3] = {"x", "y", "z"};

			int32 IndicePropriete(const NkString &p) noexcept {
				for (int32 k = 0; k < 3; ++k) {
					if (p == NkString(kProprietes[k])) {
						return k;
					}
				}
				return -1;
			}

			bool EstPlans(const NkTimelineTrack &t) noexcept {
				return t.kind == NkTimelineValueKind::Clips && t.property == NkString(kNkScenaPlans);
			}

			NkString NomCanal(int32 propriete, int32 axe) {
				NkString n(kPrefixes[propriete]);
				n.Append(".");
				n.Append(kAxes[axe]);
				return n;
			}

			/// Le canal `c` d'une piste de la frise, en canal de Noge : les memes
			/// temps, la valeur du canal, l'interpolation traduite et, pour une
			/// courbe, les PENTES EFFECTIVES de la frise (automatiques, posees ou
			/// plates) -- c'est ce qui fait jouer a Noge la courbe que la frise peint.
			void RemplirCanal(const NkTimelineModel &f, const NkTimelineTrack &t, uint32 c, NkAnimChannel &ch) {
				ch.keyframes.Clear();
				for (uint32 k = 0; k < (uint32)t.keys.Size(); ++k) {
					const NkTimelineKey &cle = t.keys[k];
					nkentseu::NkKeyframe kf;
					kf.time = cle.time;
					kf.value = cle.v[c];
					kf.interpolation = (NkInterpolation)NkScenaInterpVersNoge(cle.interp, cle.tangent);
					if (kf.interpolation == NkInterpolation::Bezier || kf.interpolation == NkInterpolation::Auto) {
						float32 entree = 0.f;
						float32 sortie = 0.f;
						f.KeySlopes(t, k, c, entree, sortie);
						kf.inTangent = entree;
						kf.outTangent = sortie;
					}
					ch.keyframes.PushBack(kf);
				}
				ch.muted = t.muted;
				ch.locked = t.locked;
			}

			const NkAnimChannel *TrouverCanal(const NkTrack &tr, int32 propriete, int32 axe) {
				const NkString nom = NomCanal(propriete, axe);
				for (uint32 i = 0; i < (uint32)tr.channels.Size(); ++i) {
					if (NkString(tr.channels[i].propertyName) == nom) {
						return &tr.channels[i];
					}
				}
				return nullptr;
			}

			int32 CleAuTemps(const NkAnimChannel &ch, float32 t) {
				for (uint32 i = 0; i < (uint32)ch.keyframes.Size(); ++i) {
					const float32 d = ch.keyframes[i].time - t;
					if (d < 1e-6f && d > -1e-6f) {
						return (int32)i;
					}
				}
				return -1;
			}
		} // namespace

		// =====================================================================
		const char *NkScenaPrefixeCanal(const NkString &propriete) noexcept {
			const int32 k = IndicePropriete(propriete);
			return k >= 0 ? kPrefixes[k] : nullptr;
		}

		float32 NkScenaValeurNeutre(const NkString &propriete) noexcept {
			return IndicePropriete(propriete) == 2 ? 1.f : 0.f;
		}

		uint8 NkScenaInterpVersNoge(uint8 interp, uint8 tangente, bool *approchee) noexcept {
			if (approchee != nullptr) {
				*approchee = false;
			}
			switch ((NkTimelineInterp)interp) {
				case NkTimelineInterp::Palier:
					return (uint8)NkInterpolation::Constant;
				case NkTimelineInterp::Lineaire:
					return (uint8)NkInterpolation::Linear;
				case NkTimelineInterp::Courbe:
					// Auto : les pentes de Catmull-Rom (bornees) ; sinon des pentes
					// posees a la main ou plates. Les deux jouent par Hermite dans Noge.
					return tangente == (uint8)NkTimelineTangent::Auto ? (uint8)NkInterpolation::Auto
																	  : (uint8)NkInterpolation::Bezier;
				case NkTimelineInterp::Entree:
					return (uint8)NkInterpolation::EaseIn;
				case NkTimelineInterp::Sortie:
					return (uint8)NkInterpolation::EaseOut;
				case NkTimelineInterp::EntreeSortie:
					return (uint8)NkInterpolation::EaseInOut;
				default:
					// Rebond, Elastique, Recul : Noge ne les joue pas (encore).
					if (approchee != nullptr) {
						*approchee = true;
					}
					return (uint8)NkInterpolation::EaseInOut;
			}
		}

		uint8 NkScenaInterpDepuisNoge(uint8 interp) noexcept {
			switch ((NkInterpolation)interp) {
				case NkInterpolation::Constant:
					return (uint8)NkTimelineInterp::Palier;
				case NkInterpolation::Bezier:
				case NkInterpolation::Auto:
					return (uint8)NkTimelineInterp::Courbe;
				case NkInterpolation::EaseIn:
					return (uint8)NkTimelineInterp::Entree;
				case NkInterpolation::EaseOut:
					return (uint8)NkTimelineInterp::Sortie;
				case NkInterpolation::EaseInOut:
					return (uint8)NkTimelineInterp::EntreeSortie;
				default:
					return (uint8)NkTimelineInterp::Lineaire;
			}
		}

		// =====================================================================
		// LA FRISE -> LA SEQUENCE
		// =====================================================================
		void NkScenaSequenceDepuisFrise(const NkTimelineModel &f, const NkSequence *reste, NkSequence &s) {
			s.tracks.Clear();
			s.nlaTracks.Clear();
			s.cameraTrack = NkCameraTrack{};
			s.markers.Clear();
			s.fps = f.fps > 0.f ? f.fps : 24.f;
			s.duration = f.duration;
			s.renderOutput.startTime = f.rangeStart;
			s.renderOutput.endTime = f.rangeEnd < 0.f ? 0.f : f.rangeEnd;

			// ── Les entites, dans l'ordre ou la frise les montre la premiere fois ──
			// L'ordre des canaux dans une piste est FIXE (position, rotation,
			// echelle ; x, y, z), quel que soit l'ordre des pistes de la frise :
			// c'est ce qui rend l'aller-retour identique a l'octet.
			NkVector<NkString> objets;
			for (uint32 i = 0; i < (uint32)f.tracks.Size(); ++i) {
				const NkTimelineTrack &t = f.tracks[i];
				if (IndicePropriete(t.property) < 0 || t.kind == NkTimelineValueKind::Clips) {
					continue;
				}
				bool connu = false;
				for (uint32 j = 0; j < (uint32)objets.Size(); ++j) {
					connu = connu || objets[j] == t.object;
				}
				if (!connu) {
					objets.PushBack(t.object);
				}
			}
			for (uint32 o = 0; o < (uint32)objets.Size(); ++o) {
				NkTrack piste;
				piste.type = NkTrackType::Transform;
				piste.entityName = objets[o];
				piste.name = "Transform";
				bool tousVerrouilles = true;
				for (int32 k = 0; k < 3; ++k) {
					const NkTimelineTrack *t = nullptr;
					for (uint32 i = 0; i < (uint32)f.tracks.Size() && t == nullptr; ++i) {
						if (f.tracks[i].object == objets[o] && f.tracks[i].property == NkString(kProprietes[k]) &&
							f.tracks[i].kind != NkTimelineValueKind::Clips) {
							t = &f.tracks[i];
						}
					}
					if (t == nullptr) {
						continue;
					}
					tousVerrouilles = tousVerrouilles && t->locked;
					for (int32 c = 0; c < 3; ++c) {
						NkAnimChannel ch;
						const NkString nom = NomCanal(k, c);
						NkSeqStrNCpy(ch.propertyName, nom.CStr(), NkAnimChannel::kMaxName - 1u);
						RemplirCanal(f, *t, (uint32)c, ch);
						piste.channels.PushBack(ch);
					}
				}
				piste.locked = tousVerrouilles && !piste.channels.Empty();
				s.tracks.PushBack(piste);
			}

			// ── Les plans camera : la premiere piste de clips « Plans caméra » ──
			for (uint32 i = 0; i < (uint32)f.tracks.Size(); ++i) {
				const NkTimelineTrack &t = f.tracks[i];
				if (!EstPlans(t)) {
					continue;
				}
				s.cameraTrack.muted = t.muted;
				s.cameraTrack.locked = t.locked;
				for (uint32 k = 0; k < (uint32)t.clips.Size(); ++k) {
					const NkTimelineClip &c = t.clips[k];
					NkCameraShot plan;
					plan.cameraName = c.name;
					plan.startTime = c.start;
					plan.duration = c.length;
					plan.cutType = c.blendIn > 1e-6f ? NkCutType::Blend : NkCutType::Cut;
					plan.blendDuration = c.blendIn;
					s.cameraTrack.shots.PushBack(plan);
				}
				break;
			}

			// ── Les marqueurs ──
			for (uint32 i = 0; i < (uint32)f.markers.Size(); ++i) {
				NkMarker m;
				m.time = f.markers[i].time;
				m.type = NkMarker::Type::Note;
				NkSeqStrNCpy(m.label, f.markers[i].name.CStr(), NkMarker::kMaxLabel - 1u);
				s.markers.PushBack(m);
			}

			// ── Ce que la frise ne montre pas, rendu tel quel ──
			if (reste != nullptr) {
				for (uint32 i = 0; i < (uint32)reste->tracks.Size(); ++i) {
					s.tracks.PushBack(reste->tracks[i]);
				}
				for (uint32 i = 0; i < (uint32)reste->nlaTracks.Size(); ++i) {
					s.nlaTracks.PushBack(reste->nlaTracks[i]);
				}
			}
		}

		// =====================================================================
		// LA SEQUENCE -> LA FRISE
		// =====================================================================
		void NkScenaFriseDepuisSequence(const NkSequence &s, NkTimelineModel &f, NkSequence *reste) {
			f.tracks.Clear();
			f.undoStack.Clear();
			f.redoStack.Clear();
			f.markers.Clear();
			f.activeClip = 0;
			f.fps = s.fps > 0.f ? s.fps : 24.f;
			f.duration = s.duration > 1e-3f ? s.duration : 5.f;
			f.rangeStart = s.renderOutput.startTime;
			f.rangeEnd = s.renderOutput.endTime > 0.f ? s.renderOutput.endTime : -1.f;
			if (reste != nullptr) {
				reste->tracks.Clear();
				reste->nlaTracks = s.nlaTracks;
			}
			nk_uint64 id = 1;

			// ── Les plans camera, a la racine (le « Camera Cuts » d'UE5 est en tete) ──
			if (!s.cameraTrack.shots.Empty()) {
				NkTimelineTrack t;
				t.id = id++;
				t.property = kNkScenaPlans;
				t.label = kNkScenaPlans;
				t.kind = NkTimelineValueKind::Clips;
				t.channels = 1;
				t.showCurve = false;
				t.muted = s.cameraTrack.muted;
				t.locked = s.cameraTrack.locked;
				for (uint32 k = 0; k < (uint32)s.cameraTrack.shots.Size(); ++k) {
					const NkCameraShot &p = s.cameraTrack.shots[k];
					NkTimelineClip c;
					c.id = f.nextClipId++;
					c.name = p.cameraName.Empty() ? NkString("(caméra sans nom)") : p.cameraName;
					c.start = p.startTime;
					c.length = p.duration;
					c.sourceLength = p.duration;
					c.blendIn = p.cutType == NkCutType::Blend ? p.blendDuration : 0.f;
					c.loop = false;
					t.clips.PushBack(c);
				}
				f.tracks.PushBack(t);
			}

			// ── Les pistes Transform : une piste de la frise par propriete ──
			for (uint32 i = 0; i < (uint32)s.tracks.Size(); ++i) {
				const NkTrack &tr = s.tracks[i];
				if (tr.type != NkTrackType::Transform) {
					if (reste != nullptr) {
						reste->tracks.PushBack(tr);
					}
					continue;
				}
				const NkString objet =
					!tr.entityName.Empty() ? tr.entityName : (!tr.name.Empty() ? tr.name : NkString("Entité"));
				for (int32 k = 0; k < 3; ++k) {
					const NkAnimChannel *canaux[3] = {TrouverCanal(tr, k, 0), TrouverCanal(tr, k, 1), TrouverCanal(tr, k, 2)};
					if (canaux[0] == nullptr && canaux[1] == nullptr && canaux[2] == nullptr) {
						continue;
					}
					NkTimelineTrack t;
					t.id = id++;
					t.object = objet;
					t.property = kProprietes[k];
					t.label = kProprietes[k];
					t.kind = NkTimelineValueKind::Nombre;
					t.channels = 3;
					t.locked = tr.locked;
					// Les temps : l'union de ceux des trois canaux, tries.
					NkVector<float32> temps;
					bool tousMuets = true;
					for (int32 c = 0; c < 3; ++c) {
						if (canaux[c] == nullptr) {
							continue;
						}
						tousMuets = tousMuets && canaux[c]->muted;
						for (uint32 j = 0; j < (uint32)canaux[c]->keyframes.Size(); ++j) {
							const float32 tk = canaux[c]->keyframes[j].time;
							uint32 at = 0;
							bool deja = false;
							while (at < (uint32)temps.Size() && temps[at] < tk - 1e-6f) {
								++at;
							}
							deja = at < (uint32)temps.Size() && temps[at] - tk < 1e-6f && tk - temps[at] < 1e-6f;
							if (!deja) {
								temps.Insert(temps.Begin() + at, tk);
							}
						}
					}
					t.muted = tousMuets;
					for (uint32 j = 0; j < (uint32)temps.Size(); ++j) {
						NkTimelineKey cle;
						cle.time = temps[j];
						bool interpLue = false;
						bool bezier = false;
						bool unifiee = true;
						for (int32 c = 0; c < 3; ++c) {
							if (canaux[c] == nullptr) {
								cle.v[c] = NkScenaValeurNeutre(t.property);
								continue;
							}
							const int32 ik = CleAuTemps(*canaux[c], temps[j]);
							if (ik < 0) {
								cle.v[c] = canaux[c]->Evaluate(temps[j]);
								continue;
							}
							const nkentseu::NkKeyframe &kf = canaux[c]->keyframes[(uint32)ik];
							cle.v[c] = kf.value;
							if (!interpLue) {
								cle.interp = NkScenaInterpDepuisNoge((uint8)kf.interpolation);
								bezier = kf.interpolation == NkInterpolation::Bezier;
								interpLue = true;
							}
							cle.tanIn[c] = kf.inTangent;
							cle.tanOut[c] = kf.outTangent;
							unifiee = unifiee && kf.inTangent == kf.outTangent;
						}
						// Des pentes POSEES (Bezier) : la tangente a la main ; Auto se
						// recalcule dans la frise (memes valeurs, memes temps).
						if (bezier) {
							cle.tangent = unifiee ? (uint8)NkTimelineTangent::Unifiee : (uint8)NkTimelineTangent::Cassee;
						}
						t.keys.PushBack(cle);
					}
					f.tracks.PushBack(t);
				}
			}

			// ── Les marqueurs ──
			for (uint32 i = 0; i < (uint32)s.markers.Size(); ++i) {
				NkTimelineMarker m;
				m.id = f.nextMarkerId++;
				m.time = s.markers[i].time;
				m.name = NkString(s.markers[i].label);
				f.markers.PushBack(m);
			}
			if (f.TrackIndex(f.activeTrack) < 0) {
				f.activeTrack = f.tracks.Empty() ? 0u : f.tracks[0].id;
			}
			f.Touch();
		}

		// =====================================================================
		bool NkScenaEvaluerPiste(const NkTimelineModel &f, const NkTimelineTrack &piste, float32 t, float32 out[4]) noexcept {
			if (piste.kind == NkTimelineValueKind::Clips || IndicePropriete(piste.property) < 0 || piste.keys.Empty()) {
				return false;
			}
			out[0] = out[1] = out[2] = out[3] = 0.f;
			const uint32 n = piste.channels < 4u ? piste.channels : 4u;
			for (uint32 c = 0; c < n; ++c) {
				NkAnimChannel ch;
				RemplirCanal(f, piste, c, ch);
				out[c] = ch.Evaluate(t);
			}
			return true;
		}

	} // namespace nkscena
} // namespace nkentseu
