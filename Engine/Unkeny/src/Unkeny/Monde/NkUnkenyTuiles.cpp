// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Monde/NkUnkenyTuiles.cpp
// DESCRIPTION: La carte de tuiles. Des donnees et des requetes, aucun dessin.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Monde/NkUnkenyTuiles.h"

#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace unkeny {

		bool NkCarteTuiles::Creer(int32 largeur, int32 hauteur, float32 tailleTuile) {
			Liberer();
			if (largeur <= 0 || hauteur <= 0 || tailleTuile <= 0.f) {
				// Un refus se DIT, et il dit POURQUOI. Une carte silencieusement
				// vide se cherche dans le rendu pendant une heure.
				logger.Error("[unkeny] carte refusee : {0}x{1}, tuile {2}", largeur, hauteur, tailleTuile);
				return false;
			}
			mLargeur = largeur;
			mHauteur = hauteur;
			mTaille = tailleTuile;
			// La tuile 0 est VIDE par convention : une carte fraiche est donc
			// vide, et non remplie de la premiere tuile de l'atlas.
			mNatures.Resize(256);
			for (uint32 i = 0; i < mNatures.Size(); ++i) {
				mNatures[i] = NkNatureTuile::NK_TRAVERSABLE;
			}
			return true;
		}

		void NkCarteTuiles::Liberer() {
			mCouches.Clear();
			mNatures.Clear();
			mLargeur = 0;
			mHauteur = 0;
		}

		int32 NkCarteTuiles::AjouterCouche(int32 ordre, float32 parallaxe) {
			if (!EstValide()) {
				logger.Warn("[unkeny] AjouterCouche refuse : la carte n'est pas creee");
				return -1;
			}
			NkCoucheTuiles c;
			c.couche = ordre;
			c.parallaxe = parallaxe;
			c.tuiles.Resize(static_cast<uint32>(mLargeur * mHauteur));
			for (uint32 i = 0; i < c.tuiles.Size(); ++i) {
				c.tuiles[i] = NK_TUILE_VIDE;
			}
			mCouches.PushBack(c);
			return static_cast<int32>(mCouches.Size()) - 1;
		}

		NkCoucheTuiles *NkCarteTuiles::Couche(int32 i) noexcept {
			if (i < 0 || i >= static_cast<int32>(mCouches.Size())) {
				return nullptr;
			}
			return &mCouches[static_cast<uint32>(i)];
		}

		const NkCoucheTuiles *NkCarteTuiles::Couche(int32 i) const noexcept {
			if (i < 0 || i >= static_cast<int32>(mCouches.Size())) {
				return nullptr;
			}
			return &mCouches[static_cast<uint32>(i)];
		}

		uint16 NkCarteTuiles::Tuile(int32 couche, int32 x, int32 y) const noexcept {
			// ⚠️ Hors carte rend VIDE, jamais une valeur au hasard. Un jeu de
			// plateforme interroge en permanence les cases voisines, y compris
			// au bord : rendre n'importe quoi la ferait « rebondir sur le vide ».
			if (x < 0 || x >= mLargeur || y < 0 || y >= mHauteur) {
				return NK_TUILE_VIDE;
			}
			const NkCoucheTuiles *c = Couche(couche);
			if (c == nullptr) {
				return NK_TUILE_VIDE;
			}
			return c->tuiles[static_cast<uint32>(y * mLargeur + x)];
		}

		void NkCarteTuiles::PoserTuile(int32 couche, int32 x, int32 y, uint16 tuile) noexcept {
			if (x < 0 || x >= mLargeur || y < 0 || y >= mHauteur) {
				return;
			}
			NkCoucheTuiles *c = Couche(couche);
			if (c == nullptr) {
				return;
			}
			c->tuiles[static_cast<uint32>(y * mLargeur + x)] = tuile;
		}

		void NkCarteTuiles::PoserNature(uint16 tuile, NkNatureTuile nature) {
			if (tuile >= mNatures.Size()) {
				mNatures.Resize(static_cast<uint32>(tuile) + 1u);
			}
			mNatures[tuile] = nature;
		}

		NkNatureTuile NkCarteTuiles::Nature(uint16 tuile) const noexcept {
			if (tuile == NK_TUILE_VIDE || tuile >= mNatures.Size()) {
				return NkNatureTuile::NK_TRAVERSABLE;
			}
			return mNatures[tuile];
		}

		NkNatureTuile NkCarteTuiles::NatureAu(const NkVec2f &monde, int32 coucheCollision) const noexcept {
			int32 x = 0, y = 0;
			if (!MondeVersCase(monde, x, y)) {
				// Hors carte = traversable. Un jeu qui veut des bords solides les
				// pose explicitement : ce n'est pas au moteur de decider si le
				// monde est une boite ou s'il continue.
				return NkNatureTuile::NK_TRAVERSABLE;
			}
			return Nature(Tuile(coucheCollision, x, y));
		}

	} // namespace unkeny
} // namespace nkentseu
