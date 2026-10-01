// =============================================================================
// NkAnimaFrise.cpp — la frise PARTAGEE de NKEditorKit dans NkAnimaEditor
// (voir NkAnimaFrise.h). Le kit dessine et joue les gestes ; ce fichier
// TRADUIT : le clip (par AnimBridge) -> le modele de la frise, et les gestes
// de la frise -> les operations annulables de NkAnimationEditor.
//
// LA REGLE : hostKeys. Les poses-cles appartiennent a NkAnimationEditor (sa
// pile d'annulation, ses poses de tout le squelette) : la frise RAPPORTE
// « + Cle », Suppr., Coller, Annuler, Refaire ; elle ne fait elle-meme que
// deplacer et interpoler, et ce fichier relit le resultat (chaque cle porte son
// rang d'origine dans `user`).
// =============================================================================
#include "Frise/NkAnimaFrise.h"

#include "AnimBridge.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkanima {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	namespace {
		constexpr nk_uint64 kPistePoses = 1;
		constexpr nk_uint64 kPremierOs = 1000;
		constexpr uint32 kOsMax = 160; ///< au-dela, les os ne sont pas listes (la piste des poses reste)

		struct Etat {
				NkTimelineModel m;
				NkTimelineResult r;
				NkTheme theme = NkTheme::Dark();
				bool themeChoisi = false;
				/// Les temps des poses-cles a la derniere construction (rang = user - 1).
				NkVector<float32> temps;
				uint32 signature = 0;
				bool construit = false;
		};

		Etat &E() {
			static Etat e;
			return e;
		}

		nk_uint64 PisteOs(uint32 j, uint32 k) {
			return kPremierOs + (nk_uint64)j * 2u + (nk_uint64)k;
		}

		/// « Hips/Spine/Chest » : le chemin d'un os par ses parents (noms, ou « os N »).
		NkString CheminOs(uint32 j) {
			NkVector<uint32> chaine;
			for (int32 k = (int32)j, garde = 0; k >= 0 && garde < 64; k = AnimJointParent((uint32)k), ++garde) {
				chaine.PushBack((uint32)k);
			}
			NkString r;
			for (int32 i = (int32)chaine.Size() - 1; i >= 0; --i) {
				const char *n = AnimJointName(chaine[(uint32)i]);
				if (n != nullptr && n[0] != '\0') {
					r.Append(n);
				} else {
					char t[24];
					std::snprintf(t, sizeof(t), "os %u", chaine[(uint32)i]);
					r.Append(t);
				}
				if (i > 0) {
					r.Append("/");
				}
			}
			return r;
		}

		/// Ce qui, s'il change, oblige a reconstruire : les temps et les interpolations.
		uint32 Signature(const NkVector<float32> &temps) {
			uint32 h = 2166136261u ^ (uint32)temps.Size();
			for (uint32 i = 0; i < (uint32)temps.Size(); ++i) {
				uint32 b = 0;
				std::memcpy(&b, &temps[i], 4);
				h = (h ^ b) * 16777619u;
				h = (h ^ (uint32)AnimKeyInterp(temps[i])) * 16777619u;
			}
			return h ^ (uint32)(AnimDuration() * 1000.f);
		}

		/// Le clip -> le modele de la frise. La VUE (zoom, replis, marqueurs, plage,
		/// boucle, piste active) est gardee : seules les pistes sont refaites.
		void Construire(Etat &e, const NkVector<float32> &temps) {
			NkTimelineModel &m = e.m;
			const bool premier = !e.construit;
			// La SELECTION survit a la reconstruction (par temps de pose-cle).
			NkVector<float32> choisies;
			if (const NkTimelineTrack *ancien = m.Track(kPistePoses)) {
				for (uint32 i = 0; i < (uint32)ancien->keys.Size(); ++i) {
					if (ancien->keys[i].selected) {
						choisies.PushBack(ancien->keys[i].time);
					}
				}
			}
			m.tracks.Clear();
			m.undoStack.Clear();
			m.redoStack.Clear();
			m.fps = AnimFps() > 0.f ? AnimFps() : 30.f;
			m.duration = AnimDuration() > 1e-3f ? AnimDuration() : 1.f;
			m.rootLabel = "Squelette";
			m.hostKeys = true;
			// La piste des POSES-CLES : une cle par pose (de tout le squelette), sa
			// valeur est son NUMERO (la colonne dit « pose 3 » au curseur).
			// ⚠️ Les references rendues par AddTrack ne survivent pas a l'ajout suivant
			//    (le vecteur des pistes grandit) : les interpolations sont gardees a part.
			NkVector<uint8> interps;
			NkTimelineTrack &poses = m.AddTrack(kPistePoses, NkString(), "Poses", NkTimelineValueKind::Palier, 1);
			poses.label = "Poses-cles";
			poses.showCurve = false;
			for (uint32 i = 0; i < (uint32)temps.Size(); ++i) {
				NkTimelineKey k;
				k.time = temps[i];
				k.v[0] = (float32)(i + 1);
				const uint8 ip = AnimKeyInterp(temps[i]);
				k.interp = ip < (uint8)NkTimelineInterp::Count ? ip : (uint8)NkTimelineInterp::Lineaire;
				interps.PushBack(k.interp);
				k.user = (nk_uint64)(i + 1);
				for (uint32 q = 0; q < (uint32)choisies.Size(); ++q) {
					k.selected = k.selected || std::fabs(choisies[q] - k.time) < 1e-4f;
				}
				poses.keys.PushBack(k);
			}
			// Les OS : un objet par os (l'arbre des parents), deux pistes VERROUILLEES
			// (une pose s'edite dans la vue 3D) dont les courbes se lisent.
			const uint32 nb = AnimBoneCount() < kOsMax ? AnimBoneCount() : kOsMax;
			static const char *const kNoms[2] = {"Position", "Rotation (deg)"};
			for (uint32 j = 0; j < nb; ++j) {
				const NkString chemin = CheminOs(j);
				for (uint32 q = 0; q < 2; ++q) {
					NkTimelineTrack &t = m.AddTrack(PisteOs(j, q), chemin, kNoms[q], NkTimelineValueKind::Nombre, 3);
					t.label = kNoms[q];
					t.locked = true;
					t.showCurve = false;
					for (uint32 i = 0; i < (uint32)temps.Size(); ++i) {
						float32 p[3], r[3], s[3];
						if (!AnimSampleJointLocal(j, temps[i], p, r, s)) {
							continue;
						}
						NkTimelineKey k;
						k.time = temps[i];
						for (uint32 c = 0; c < 3; ++c) {
							k.v[c] = q == 0 ? p[c] : r[c];
						}
						k.interp = interps[i];
						t.keys.PushBack(k);
					}
				}
				// La premiere fois : les os de premier niveau REPLIES (l'arbre s'ouvre
				// a la demande, comme l'Outliner d'UE5).
				if (premier && AnimJointParent(j) < 0 && !m.IsCollapsed(chemin)) {
					m.collapsed.PushBack(chemin);
				}
			}
			m.undoStack.Clear(); // AddTrack empile : la frise n'a pas d'annulation a elle ici
			if (premier) {
				m.FrameAll();
				m.activeTrack = kPistePoses;
			}
			e.temps = temps;
			e.construit = true;
			m.Touch();
		}

		/// La courbe d'un OS, telle que le lecteur la joue (pas une approximation).
		bool Evaluer(void *, const NkTimelineTrack &tr, float32 t, float32 out[4]) {
			if (tr.id < kPremierOs) {
				return false; // la piste des poses : le repli du kit (paliers)
			}
			const uint32 j = (uint32)((tr.id - kPremierOs) / 2u), q = (uint32)((tr.id - kPremierOs) % 2u);
			float32 p[3], r[3], s[3];
			if (!AnimSampleJointLocal(j, t, p, r, s)) {
				return false;
			}
			for (uint32 c = 0; c < 3; ++c) {
				out[c] = q == 0 ? p[c] : r[c];
			}
			out[3] = 0.f;
			return true;
		}

		NkTimelineStyle Style() {
			NkTimelineStyle s;
			s.panelBg = (uint16)NkRole::PanelBg;
			s.headerBg = (uint16)NkRole::PanelHeader;
			s.rowAltBg = (uint16)NkRole::InputBg;
			s.border = (uint16)NkRole::Border;
			s.grid = (uint16)NkRole::GridLine;
			s.text = (uint16)NkRole::Text;
			s.textMuted = (uint16)NkRole::TextMuted;
			s.accent = (uint16)NkRole::AccentUi;
			s.textOnAccent = (uint16)NkRole::TextOnAccent;
			s.key = (uint16)NkRole::Text;
			s.keySelected = (uint16)NkRole::AccentSel;
			s.playhead = (uint16)NkRole::StatusErr;
			s.channel[0] = (uint16)NkRole::AxisX;
			s.channel[1] = (uint16)NkRole::AxisY;
			s.channel[2] = (uint16)NkRole::AxisZ;
			s.channel[3] = (uint16)NkRole::TextMuted;
			s.buttonBg = (uint16)NkRole::ButtonBg;
			s.inputBg = (uint16)NkRole::InputBg;
			s.rangeIn = (uint16)NkRole::StatusOk;
			s.rangeOut = (uint16)NkRole::StatusErr;
			s.marker = (uint16)NkRole::AccentSel;
			s.clip = (uint16)NkRole::TypeAnim;
			s.clipAlt = (uint16)NkRole::NodeActionHeader;
			s.tangent = (uint16)NkRole::AccentSel;
			return s;
		}

		NkComponentInput Entree(const nkgui::NkGuiContext &ctx) {
			// La meme traduction qu'UnkenyEditor (NkEditeurEntreeComposant) et que
			// NkEditorShell::DrawTabStrip.
			const nkgui::NkGuiInput &in = ctx.input;
			NkComponentInput ci;
			ci.surfaceScale = ctx.scale;
			ci.mouseX = in.mousePos.x;
			ci.mouseY = in.mousePos.y;
			ci.wheel = in.wheel;
			ci.mouseDown = in.mouseDown[0];
			ci.mousePressed = in.mouseClicked[0];
			ci.mouseReleased = in.mouseReleased[0];
			ci.doubleClick = in.mouseDoubleClicked[0];
			ci.rightPressed = in.mouseClicked[1];
			ci.ctrl = in.ctrlDown;
			ci.shift = in.shiftDown;
			ci.alt = in.altDown;
			return ci;
		}

		/// Les gestes de la frise -> NkAnimationEditor.
		void Appliquer(Etat &e) {
			NkTimelineModel &m = e.m;
			const NkTimelineResult &r = e.r;
			if (r.undoRequested) {
				AnimUndo();
			}
			if (r.redoRequested) {
				AnimRedo();
			}
			if (r.keyRequested) {
				AnimSeek(r.requestTime);
				AnimInsertKeyAtCursor();
			}
			const NkTimelineTrack *poses = m.Track(kPistePoses);
			if (r.deleteRequested && poses != nullptr) {
				for (uint32 i = 0; i < (uint32)poses->keys.Size(); ++i) {
					const NkTimelineKey &k = poses->keys[i];
					if (k.selected && k.user >= 1 && k.user <= (nk_uint64)e.temps.Size()) {
						AnimDeleteKeyAt(e.temps[(uint32)k.user - 1u]);
					}
				}
			}
			if (r.pasteRequested) {
				for (uint32 i = 0; i < (uint32)m.clipboard.Size(); ++i) {
					const NkTimelineClipboardKey &c = m.clipboard[i];
					if (c.track == kPistePoses && c.key.user >= 1 && c.key.user <= (nk_uint64)e.temps.Size()) {
						AnimCopyPoseKey(e.temps[(uint32)c.key.user - 1u], r.requestTime + c.key.time);
					}
				}
			}
			// DEPLACER et INTERPOLER ont ete faits dans la frise : on les relit.
			if (r.changed && poses != nullptr) {
				struct Mouvement {
						float32 de, vers;
				};
				NkVector<Mouvement> mv;
				for (uint32 i = 0; i < (uint32)poses->keys.Size(); ++i) {
					const NkTimelineKey &k = poses->keys[i];
					if (k.user < 1 || k.user > (nk_uint64)e.temps.Size()) {
						continue;
					}
					const float32 orig = e.temps[(uint32)k.user - 1u];
					if (k.interp != AnimKeyInterp(orig) && k.interp < (uint8)NkTimelineInterp::Count) {
						AnimSetKeyInterp(orig, k.interp);
					}
					if (std::fabs(k.time - orig) > 1e-4f) {
						Mouvement x;
						x.de = orig;
						x.vers = k.time;
						mv.PushBack(x);
					}
				}
				// Vers la droite : les dernieres d'abord (et l'inverse), pour qu'une cle
				// deplacee ne tombe pas sur une autre qui n'a pas encore bouge.
				const bool droite = !mv.Empty() && mv[0].vers > mv[0].de;
				for (uint32 k = 0; k < (uint32)mv.Size(); ++k) {
					const Mouvement &x = mv[droite ? (uint32)mv.Size() - 1u - k : k];
					AnimMoveKey(x.de, x.vers);
				}
			}
			// La LECTURE : le bouton de la frise pilote le lecteur ; le curseur frotte.
			if (m.playing != AnimIsPlaying()) {
				AnimSetPlaying(m.playing);
			}
			if (r.cursorChanged) {
				AnimSeek(m.cursor);
			}
		}

		/// La boucle et la plage de lecture, sur le lecteur de NkAnimaEditor.
		void BoucleEtPlage(Etat &e) {
			NkTimelineModel &m = e.m;
			if (!AnimIsPlaying()) {
				return;
			}
			const float32 t = AnimCursor();
			const float32 debut = m.PlayStart(), fin = m.PlayEnd();
			const float32 eps = m.FrameDuration() * 0.5f;
			if (t >= fin - 1e-4f || t < debut - eps) {
				if (m.loop || t < debut - eps) {
					AnimSeek(debut);
					AnimSetPlaying(true);
				} else {
					AnimSeek(fin);
				}
			}
		}
	} // namespace

	void NkAnimaThemeFrise(const NkTheme &theme) {
		E().theme = theme;
		E().themeChoisi = true;
	}

	const NkTimelineResult &NkAnimaFriseResultat() {
		return E().r;
	}

	NkTimelineModel &NkAnimaFriseModele() {
		return E().m;
	}

	void NkAnimaDessinerFrise(NkEditorFrameContext &ec) {
		Etat &e = E();
		nkgui::NkGuiContext &ctx = ec.Ui();
		if (!AnimLoaded()) {
			ec.Text("(aucun clip charge)");
			return;
		}
		if (!e.themeChoisi) {
			const char *v = std::getenv("NKANIMA_THEME");
			e.theme = (v != nullptr && std::strcmp(v, "light") == 0) ? NkTheme::Light() : NkTheme::Dark();
			e.themeChoisi = true;
		}
		AnimUpdate(ec.dt); // avance la lecture si en cours (une fois par image, comme l'ancienne)
		BoucleEtPlage(e);
		NkVector<float32> temps;
		AnimGetKeyTimes(temps);
		const uint32 sig = Signature(temps);
		if (!e.construit || sig != e.signature) {
			Construire(e, temps);
			e.signature = sig;
		}
		NkTimelineModel &m = e.m;
		m.cursor = AnimCursor();
		m.playing = AnimIsPlaying();
		m.hostCanUndo = AnimCanUndo();
		m.hostCanRedo = AnimCanRedo();
		// La place RESTANTE du panneau : la frise la prend toute.
		const nkgui::NkGuiLayout &lay = ctx.layout;
		const float32 x = lay.cursor.x, y = lay.cursor.y;
		float32 w = lay.region.x + lay.region.w - x - lay.padding;
		float32 h = lay.region.y + lay.region.h - y - lay.padding;
		w = w > 320.f ? w : 320.f;
		h = h > 160.f ? h : 160.f;
		const nkgui::NkRect rect = ctx.NextItemRect(w, h);
		NkGuiComponentPaint peintre(ctx, e.theme);
		NkTimelineHooks hooks;
		hooks.evaluate = &Evaluer;
		e.r = NkDrawTimeline(peintre, Entree(ctx), NkPaintRect{rect.x, rect.y, rect.w, rect.h}, m, Style(), hooks);
		Appliquer(e);
		// Ce qui a change cote clip se relira a l'image suivante (la signature).
	}

} // namespace nkanima
