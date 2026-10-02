// =============================================================================
// NkAnimaGraphe.cpp — le graphe d'etats partage dans NkAnimaEditor (voir le
// .h). L'UI seulement : le kit dessine et joue les gestes, NkAnimaControleur
// compile, enregistre et fait l'apercu.
// =============================================================================
#include "Frise/NkAnimaGraphe.h"

#include "AnimBridge.h"
#include "Frise/NkAnimaControleur.h"
#include "NKEditorKit/Components/NkGuiComponentPaint.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>

namespace nkanima {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;

	namespace {
		struct Etat {
				NkStateGraphModel g;
				NkStateGraphResult r;
				bool pret = false;
				bool apercu = false;
				bool cadre = false;
				uint32 revisionCompilee = 0xFFFFFFFFu;
				NkString chemin;	///< le .nkanimctl a cote du modele
				NkString message;	///< « Enregistre : ... »
				NkPaintRect boutons[2]; ///< Apercu, Enregistrer
		};

		Etat &E() {
			static Etat e;
			return e;
		}

		/// « Resources/Models/X/X.glb » -> « Resources/Models/X/X.nkanimctl ».
		NkString CheminControleur(const char *modele) {
			NkString c(modele != nullptr && modele[0] != '\0' ? modele : "NkAnimaEditor");
			const char *s = c.CStr();
			const char *point = std::strrchr(s, '.');
			const char *barre = std::strrchr(s, '/');
			const char *barre2 = std::strrchr(s, '\\');
			barre = barre2 > barre ? barre2 : barre;
			NkString r = point != nullptr && (barre == nullptr || point > barre) ? NkString(s, (usize)(point - s)) : c;
			r.Append(".nkanimctl");
			return r;
		}

		/// Le premier contact : le .nkanimctl du modele s'il existe, sinon un etat
		/// qui joue le clip et un parametre « vitesse » (de quoi ajouter un arbre).
		void Preparer(Etat &e, const char *modele) {
			e.chemin = CheminControleur(modele);
			if (!(NkFile::Exists(e.chemin.CStr()) && NkAnimaControleurCharger(e.chemin.CStr(), e.g))) {
				e.g = NkStateGraphModel();
				e.g.AddParam("vitesse", (uint8)NkGraphParamKind::Float);
				e.g.AddState(AnimClipNom(), 0, 0.f, 0.f, AnimClipNom());
				e.g.undoStack.Clear();
			}
			e.g.clips.Clear();
			e.g.clips.PushBack(AnimClipNom());
			// Les MASQUES proposes aux couches : les os nommes (une branche chacun).
			for (uint32 j = 0; j < AnimBoneCount(); ++j) {
				const char *n = AnimJointName(j);
				if (n != nullptr && n[0] != '\0') {
					e.g.masks.PushBack(n);
				}
			}
			e.pret = true;
		}

		NkStateGraphStyle Style() {
			NkStateGraphStyle s;
			s.canvasBg = (uint16)NkRole::CanvasBg;
			s.gridLine = (uint16)NkRole::GridLine;
			s.panelBg = (uint16)NkRole::PanelBg;
			s.headerBg = (uint16)NkRole::PanelHeader;
			s.border = (uint16)NkRole::Border;
			s.text = (uint16)NkRole::Text;
			s.textMuted = (uint16)NkRole::TextMuted;
			s.textOnAccent = (uint16)NkRole::TextOnAccent;
			s.accent = (uint16)NkRole::AccentUi;
			s.nodeBody = (uint16)NkRole::NodeBody;
			s.nodeHeader = (uint16)NkRole::NodeDataHeader;
			s.nodeEntry = (uint16)NkRole::AccentSel;
			s.subMachine = (uint16)NkRole::AccentAI;
			s.wire = (uint16)NkRole::NodeWire;
			s.pseudoEntry = (uint16)NkRole::StatusOk;
			s.pseudoAny = (uint16)NkRole::TypeAnim;
			s.paramBool = (uint16)NkRole::AccentUi;
			s.paramFloat = (uint16)NkRole::StatusOk;
			s.paramTrigger = (uint16)NkRole::AccentSel;
			s.buttonBg = (uint16)NkRole::ButtonBg;
			s.inputBg = (uint16)NkRole::InputBg;
			s.blendTree = (uint16)NkRole::TypeAnim;
			return s;
		}

		NkComponentInput Entree(const nkgui::NkGuiContext &ctx) {
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

		/// Un bouton de la barre, peint par le peintre du kit ; vrai si clique.
		bool Bouton(NkGuiComponentPaint &p, const NkComponentInput &in, const NkPaintRect &rc, const char *texte, bool enfonce) {
			const bool survol = rc.Contains(in.mouseX, in.mouseY);
			p.Fill(rc, enfonce ? (uint16)NkRole::AccentUi : (survol ? (uint16)NkRole::InputBg : (uint16)NkRole::ButtonBg), 3.f);
			p.Text(rc, texte, enfonce ? (uint16)NkRole::TextOnAccent : (uint16)NkRole::Text, NkTextAlign::Center);
			return survol && in.mousePressed;
		}
	} // namespace

	NkStateGraphModel &NkAnimaGrapheModele() {
		return E().g;
	}

	const NkStateGraphResult &NkAnimaGrapheResultat() {
		return E().r;
	}

	bool &NkAnimaGrapheApercu() {
		return E().apercu;
	}

	void NkAnimaGrapheCache() {
		Etat &e = E();
		if (e.apercu) {
			e.apercu = false;
			NkAnimaControleurArreter(e.g);
		}
	}

	void NkAnimaDessinerGraphe(NkEditorFrameContext &ec, float32 x, float32 y, float32 w, float32 h, const NkTheme &theme,
							   const char *modele) {
		Etat &e = E();
		nkgui::NkGuiContext &ctx = ec.Ui();
		if (!AnimLoaded()) {
			return;
		}
		if (!e.pret) {
			Preparer(e, modele);
		}
		if (e.g.revision != e.revisionCompilee) {
			NkAnimaControleurCompiler(e.g);
			e.revisionCompilee = e.g.revision;
		}
		NkGuiComponentPaint peintre(ctx, theme);
		const NkComponentInput in = Entree(ctx);
		// ── La barre : l'apercu (le controleur joue dans la vue 3D), l'enregistrement.
		const float32 bh = 28.f;
		peintre.Fill(NkPaintRect{x, y, w, bh}, (uint16)NkRole::PanelHeader, 0.f);
		const float32 bw1 = peintre.TextWidth("Apercu dans la vue 3D") + 24.f, bw2 = peintre.TextWidth("Enregistrer") + 24.f;
		e.boutons[0] = NkPaintRect{x + 6.f, y + 3.f, bw1, bh - 6.f};
		e.boutons[1] = NkPaintRect{x + 12.f + bw1, y + 3.f, bw2, bh - 6.f};
		if (Bouton(peintre, in, e.boutons[0], e.apercu ? "Apercu en cours" : "Apercu dans la vue 3D", e.apercu)) {
			e.apercu = !e.apercu;
			if (!e.apercu) {
				NkAnimaControleurArreter(e.g);
			}
		}
		if (Bouton(peintre, in, e.boutons[1], "Enregistrer", false)) {
			const bool ok = NkAnimaControleurEnregistrer(e.chemin.CStr());
			e.message = NkString::Format(ok ? "Enregistre : %s" : "Ecriture impossible : %s", e.chemin.CStr());
		}
		const NkString info = e.message.Empty() ? NkString::Format("Controleur : %s", e.chemin.CStr()) : e.message;
		peintre.Text(NkPaintRect{e.boutons[1].x + e.boutons[1].w + 12.f, y, w - (e.boutons[1].x + e.boutons[1].w + 18.f - x), bh},
					 info.CStr(), (uint16)NkRole::TextMuted, NkTextAlign::Left);
		// ── Le graphe.
		if (e.apercu) {
			NkAnimaControleurApercu(ec.dt, e.g);
		}
		const NkPaintRect zone{x, y + bh, w, h - bh > 40.f ? h - bh : 40.f};
		if (!e.cadre) {
			e.g.FrameLevel(zone.w * 0.5f, zone.h);
			e.cadre = true;
		}
		e.r = NkDrawStateGraph(peintre, in, zone, e.g, Style());
	}

} // namespace nkanima
