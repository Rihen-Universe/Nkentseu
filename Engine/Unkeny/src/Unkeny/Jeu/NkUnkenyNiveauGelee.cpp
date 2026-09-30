//
// NkUnkenyNiveauGelee.cpp
// =============================================================================
// Le niveau du jalon « Gelee » (voir l'en-tete). Les valeurs sont celles du
// banc du jeu, qui les a mesurees (NkUnkenyBancJeu.cpp, G1 / G2).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Jeu/NkUnkenyNiveauGelee.h"

#include "NKPhysics/NkParticules2DFabrique.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			ecs::NkEntityId Boite(NkScene &s, const char *nom, const NkVec2f &c, const NkVec2f &demi, bool zone) {
				const ecs::NkEntityId e = s.Creer(nom, c);
				NkCollisionneur2D col;
				col.demiTaille = demi;
				col.declencheur = zone;
				s.Monde().Add<NkCollisionneur2D>(e, col);
				NkCorps2D k;
				k.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(e, k);
				return e;
			}
		} // namespace

		bool NkConstruireNiveauGelee(NkScene &s, NkNiveauGelee &n, bool mou) {
			if (s.MondePhysique() == nullptr || (mou && s.Particules() == nullptr)) {
				return false;
			}
			n.mou = mou;
			// Plates-formes : [-2 ; 4] haut 0, [5,8 ; 10] haut 0,5, [11,8 ; 16] haut 0,5 :
			// deux trous de 1,8 m, le premier a franchir en MONTANT de 0,5 m.
			// ⚠️ Premiere version (banc du jeu) : trous de 1,5 m et troisieme
			// plate-forme plus BASSE -- la gelee les franchissait SANS sauter (G1n
			// rouge) : poussee a 3 m/s, elle roulait et REBONDISSAIT sur ses coins
			// (vy +1,75 m/s sans saut), et une descente de 0,5 m sur 1,5 m se passe
			// de saut.
			const NkVec2f c[3] = {NkVec2f(1.f, -0.5f), NkVec2f(7.9f, 0.f), NkVec2f(13.9f, 0.f)};
			const NkVec2f d[3] = {NkVec2f(3.f, 0.5f), NkVec2f(2.1f, 0.5f), NkVec2f(2.1f, 0.5f)};
			for (int32 i = 0; i < 3; ++i) {
				n.plateformes[i] = Boite(s, "Plateforme", c[i], d[i], false);
				n.mn[i] = c[i] - d[i];
				n.mx[i] = c[i] + d[i];
			}
			n.fin = Boite(s, "Fin", NkVec2f(15.f, 1.5f), NkVec2f(0.5f, 1.f), true);
			if (mou) {
				const int32 ci = physics::NkCreerGeleeP2D(*s.Particules(), NkVec2f(-1.f, 0.4f), 5, 5, 0.13f);
				n.heros = s.CreerCorpsMou("Gelee", ci, 0x7CE64CFFu);
				NkControleMou2D ctl;
				ctl.reglages.vitesseMax = 3.f;
				ctl.reglages.vitesseSaut = 6.f;
				ctl.redressement = 0.5f; // qu'il ne roule pas (voir NkControleMou2D)
				s.Monde().Add<NkControleMou2D>(n.heros, ctl);
			} else {
				n.heros = s.Creer("Heros", NkVec2f(-1.f, 0.45f));
				NkCollisionneur2D col;
				col.forme = NkForme2D::NK_BOITE;
				col.demiTaille = NkVec2f(0.25f, 0.4f);
				s.Monde().Add<NkCollisionneur2D>(n.heros, col);
				NkCorps2D k;
				k.masse = 60.f;
				k.rotationBloquee = true;
				k.friction = 0.f; // le controleur freine ; un frottement au sol le ferait trebucher
				s.AjouterCorps(n.heros, k);
				NkControleRigide2D ctl;
				ctl.reglages.vitesseMax = 3.f;
				ctl.reglages.vitesseSaut = 6.f;
				s.Monde().Add<NkControleRigide2D>(n.heros, ctl);
			}
			return true;
		}

		NkVec2f NkPositionHerosGelee(NkScene &s, const NkNiveauGelee &n) {
			if (n.mou) {
				const NkCorpsMou2D *m = s.Monde().Get<NkCorpsMou2D>(n.heros);
				const int32 ci = (m != nullptr && s.Particules() != nullptr) ? s.Particules()->IndexCorps(m->corpsId) : -1;
				return ci >= 0 ? s.Particules()->CentreCorps(static_cast<uint32>(ci)) : NkVec2f(-99.f, -99.f);
			}
			const NkTransform2D *t = s.Monde().Get<NkTransform2D>(n.heros);
			return t != nullptr ? t->position : NkVec2f(-99.f, -99.f);
		}

		const NkEtatControle2D *NkEtatHerosGelee(NkScene &s, const NkNiveauGelee &n) {
			if (n.mou) {
				const NkControleMou2D *c = s.Monde().Get<NkControleMou2D>(n.heros);
				return c != nullptr ? &c->etat : nullptr;
			}
			const NkControleRigide2D *c = s.Monde().Get<NkControleRigide2D>(n.heros);
			return c != nullptr ? &c->etat : nullptr;
		}

	} // namespace unkeny
} // namespace nkentseu
