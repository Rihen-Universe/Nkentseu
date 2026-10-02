// =============================================================================
// NkAnimaControleur.cpp — voir NkAnimaControleur.h. NKAnima et le modele du
// graphe, sans NKRenderer : la conversion est celle d'UnkenyEditor, PARTAGEE
// (NKEditorKit/Components/NkStateGraphAnima.h).
// =============================================================================
#include "Frise/NkAnimaControleur.h"

#include "AnimBridge.h"
#include "NKEditorKit/Components/NkStateGraphAnima.h"

namespace nkanima {

	using namespace nkentseu;

	namespace {
		struct Etat {
				anim::NkAnimController ctl;
				anim::NkAnimControllerRuntime rt;
				NkVector<nk_uint64> noeudDeEtat;
				bool compile = false;
				int32 etatVu = -1;
				anim::NkAnimPose pose;
				NkVector<math::NkMat4f> skin;
		};

		Etat &E() {
			static Etat e;
			return e;
		}

		/// NkAnimaEditor n'a qu'UN clip (celui du modele cuit) : tout nom de clip
		/// qui est le sien le rend.
		anim::NkClipLookup Recherche() {
			return [](const NkString &nom) -> const anim::NkAnimationClip * {
				const anim::NkAnimationClip *c = static_cast<const anim::NkAnimationClip *>(AnimClipOpaque());
				return c != nullptr && nom == NkString(AnimClipNom()) ? c : nullptr;
			};
		}
	} // namespace

	bool NkAnimaControleurCompiler(const editorkit::NkStateGraphModel &g) {
		Etat &e = E();
		e.compile = editorkit::anima::NkControleurDepuisGraphe(g, e.ctl, e.noeudDeEtat);
		return e.compile;
	}

	bool NkAnimaControleurEnregistrer(const char *chemin) {
		Etat &e = E();
		return e.compile && chemin != nullptr && e.ctl.SaveBinary(NkString(chemin));
	}

	bool NkAnimaControleurCharger(const char *chemin, editorkit::NkStateGraphModel &g) {
		Etat &e = E();
		anim::NkAnimController lu;
		if (chemin == nullptr || !lu.LoadBinary(NkString(chemin))) {
			return false;
		}
		e.ctl = lu;
		editorkit::anima::NkGrapheDepuisControleur(e.ctl, g, e.noeudDeEtat);
		e.compile = true;
		e.rt = anim::NkAnimControllerRuntime();
		return true;
	}

	void NkAnimaControleurApercu(float32 dt, editorkit::NkStateGraphModel &g) {
		Etat &e = E();
		if (!e.compile) {
			return;
		}
		// Les parametres VIVANTS du graphe (frottes, bascules, declencheurs tires).
		anim::NkAnimStateMachine &base = e.ctl.base;
		for (uint32 i = 0; i < (uint32)g.params.Size(); ++i) {
			const int32 k = base.FindParam(g.params[i].name, (anim::NkAnimStateMachine::NkParamKind)g.params[i].kind);
			if (k >= 0) {
				base.SetParamValue((uint32)k, g.params[i].value);
			}
		}
		anim::NkAdvanceController(e.ctl, e.rt, dt, Recherche(), e.pose);
		// Un declencheur consomme revient a 0 dans le graphe.
		for (uint32 i = 0; i < (uint32)g.params.Size(); ++i) {
			const int32 k = base.FindParam(g.params[i].name, (anim::NkAnimStateMachine::NkParamKind)g.params[i].kind);
			if (k >= 0 && g.params[i].kind == (uint8)editorkit::NkGraphParamKind::Trigger) {
				g.params[i].value = base.GetParamValue((uint32)k);
			}
		}
		if (e.pose.ToSkinning(e.skin)) {
			AnimPoserApercu(e.skin);
		} else if (!e.pose.bones.Empty()) {
			// Un clip en matrices de SKINNING (pas de squelette local) : la pose
			// melangee EST deja celle des matrices.
			e.pose.ToLocalMatrices(e.skin);
			AnimPoserApercu(e.skin);
		}
		// Le graphe s'allume : l'etat actif, le fondu, la transition qui a tire.
		const anim::NkAnimStateMachine::NkRuntime &rt = e.rt.layers[0].rt;
		auto noeud = [&](int32 etat) -> nk_uint64 {
			return (etat >= 0 && etat < (int32)e.noeudDeEtat.Size()) ? e.noeudDeEtat[(uint32)etat] : 0;
		};
		g.live = true;
		g.firedAge += dt;
		g.liveState = noeud(rt.current);
		g.liveNext = noeud(rt.next);
		g.liveFade = (rt.next >= 0 && rt.fadeDur > 1e-6f) ? rt.fadeT / rt.fadeDur : 0.f;
		if (rt.current != e.etatVu && e.etatVu >= 0 && rt.current >= 0) {
			const nk_uint64 avant = noeud(e.etatVu);
			for (uint32 t = 0; t < (uint32)g.transitions.Size(); ++t) {
				const editorkit::NkGraphTransition &tr = g.transitions[t];
				const bool versIci = tr.to == g.liveState || g.IsUnder(g.liveState, tr.to);
				const bool depuis = tr.any || tr.from == avant || g.IsUnder(avant, tr.from);
				if (versIci && depuis) {
					g.firedTransition = tr.id;
					g.firedAge = 0.f;
					break;
				}
			}
		}
		e.etatVu = rt.current;
	}

	void NkAnimaControleurArreter(editorkit::NkStateGraphModel &g) {
		Etat &e = E();
		AnimFinApercu();
		e.rt = anim::NkAnimControllerRuntime();
		e.etatVu = -1;
		g.live = false;
		g.liveState = g.liveNext = 0;
		g.liveFade = 0.f;
		for (uint32 i = 0; i < (uint32)g.params.Size(); ++i) {
			g.params[i].value = g.params[i].defaultValue;
		}
	}

} // namespace nkanima
