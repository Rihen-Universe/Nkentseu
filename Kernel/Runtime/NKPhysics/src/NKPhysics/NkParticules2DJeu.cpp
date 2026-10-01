//
// NkParticules2DJeu.cpp
// =============================================================================
// Description :
//   Ce qu'il faut a NkParticules2D pour qu'un corps mou soit un PERSONNAGE de
//   jeu (demande R15 de Rihen, Applications/UnkenyEditor/design/00, § 4) :
//   pousser sans ecraser, savoir ce qu'on touche sur tout le pas, des evenements
//   DEBUT / FIN (rigides, zones, autres corps mous), des parties nommees, des
//   attaches particule <-> rigide, des saisies par pointeur, relier deux corps.
//
// Caracteristiques :
//   - Tout AJOUTE : aucune fonction existante ne change de sens. AppliquerVitesse
//     remplace toujours la vitesse ; SaisirDebut/Vers/Fin tiennent toujours UNE
//     saisie pour tous. Les crochets poses dans NkParticules2D.cpp (NoterContact,
//     NoterPaire, ResoudreAttaches, RemapperJeu) sont des APPELS, pas des
//     changements de calcul -- sauf les attaches, qui n'agissent que si l'on en
//     pose.
//   - Aucune allocation par pas en regime etabli : les tampons grandissent puis
//     restent, comme ceux du solveur.
//   - Tout ce qui tient une PARTICULE la tient par index, remappe a chaque
//     compaction (RemapperJeu) ; tout ce qui tient un CORPS le tient par id stable.
//
// Algorithmes implementes :
//   - Attache : contrainte XPBD d'ecart nul (Macklin 2016) cote particule ; cote
//     rigide, impulse sur la vitesse RELATIVE (comme les contacts de ce solveur,
//     voir ResoudreRigides : convertir l'erreur de position en impulse lambda / h
//     faisait EXPLOSER le couplage) plus un biais de position etale sur le pas.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace physics {

		namespace {
			NK_FORCE_INLINE float32 Cross2(const NkVec2f &a, const NkVec2f &b) noexcept {
				return a.x * b.y - a.y * b.x;
			}
			NK_FORCE_INLINE float32 Len2d(const NkVec2f &v) noexcept {
				return math::NkSqrt(v.x * v.x + v.y * v.y);
			}
			NK_FORCE_INLINE NkVec2f Perp2(const NkVec2f &v) noexcept {
				return NkVec2f(-v.y, v.x);
			}
			NK_FORCE_INLINE NkVec2f Rot2(const NkVec2f &v, float32 c, float32 s) noexcept {
				return NkVec2f(v.x * c - v.y * s, v.x * s + v.y * c);
			}
			NK_FORCE_INLINE float32 Clamp01(float32 v) noexcept {
				return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
			}

			bool MemeNom(const char *a, const char *b) noexcept {
				if (a == nullptr || b == nullptr) {
					return false;
				}
				uint32 i = 0;
				for (; a[i] != '\0' && b[i] != '\0'; ++i) {
					if (a[i] != b[i]) {
						return false;
					}
				}
				return a[i] == b[i];
			}

			bool MemePaire(const NkEvenementP2D &x, const NkEvenementP2D &y) noexcept {
				return x.corps == y.corps && x.autre == y.autre && x.genre == y.genre;
			}

			bool Contient(const NkVector<NkEvenementP2D> &v, const NkEvenementP2D &e) noexcept {
				for (uint32 i = 0; i < v.Size(); ++i) {
					if (MemePaire(v[i], e)) {
						return true;
					}
				}
				return false;
			}
		} // namespace

		// =====================================================================
		// Contacts tenus sur le pas
		// =====================================================================
		void NkParticules2D::OuvrirContactsDuPas() noexcept {
			const uint32 n = static_cast<uint32>(particules.Size());
			mContactNature.Resize(n);
			mContactNormale.Resize(n);
			mContactRigide.Resize(n);
			mContactMou.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				mContactNature[i] = 0u;
				mContactNormale[i] = NkVec2f(0.f, 0.f);
				mContactRigide[i] = 0u;
				mContactMou[i] = 0u;
			}
			mPairesPas.Clear();
		}

		void NkParticules2D::NoterContact(uint32 i, uint8 nature, const NkVec2f &n, uint32 autre) noexcept {
			// Hors d'un Pas (une edition appelle Contact ? non -- mais un tampon
			// plus court que le monde est possible juste apres un chargement).
			if (i >= mContactNature.Size()) {
				return;
			}
			mContactNature[i] = static_cast<uint8>(mContactNature[i] | nature);
			mContactNormale[i] += n;
			if ((nature & (NK_P2D_CONTACT_STATIQUE | NK_P2D_CONTACT_CINEMATIQUE | NK_P2D_CONTACT_DYNAMIQUE)) != 0u) {
				mContactRigide[i] = autre;
			}
			if ((nature & NK_P2D_CONTACT_MOU) != 0u) {
				mContactMou[i] = autre;
			}
		}

		void NkParticules2D::NoterPaire(uint32 corpsId, uint32 autre, NkGenreEvenementP2D genre) noexcept {
			NkEvenementP2D e;
			e.corps = corpsId;
			e.autre = autre;
			e.genre = genre;
			if (genre == NkGenreEvenementP2D::NK_CORPS_MOU && autre < corpsId) {
				e.corps = autre;
				e.autre = corpsId;
			}
			// Le cas courant : la meme paire que la derniere notee (un corps pose
			// sur un sol donne des centaines de contacts par pas, tous identiques).
			if (mPairesPas.Size() > 0u && MemePaire(mPairesPas[mPairesPas.Size() - 1u], e)) {
				return;
			}
			if (!Contient(mPairesPas, e)) {
				mPairesPas.PushBack(e);
			}
		}

		void NkParticules2D::FermerEvenementsDuPas() noexcept {
			mDebuts.Clear();
			mFins.Clear();
			for (uint32 i = 0; i < mPairesPas.Size(); ++i) {
				if (!Contient(mPairesPrec, mPairesPas[i])) {
					mDebuts.PushBack(mPairesPas[i]);
				}
			}
			for (uint32 i = 0; i < mPairesPrec.Size(); ++i) {
				if (!Contient(mPairesPas, mPairesPrec[i])) {
					mFins.PushBack(mPairesPrec[i]);
				}
			}
			mPairesPrec = mPairesPas;
		}

		void NkParticules2D::DetecterZones() noexcept {
			if (mZones.Size() == 0u) {
				return;
			}
			for (uint32 ci = 0; ci < corps.Size(); ++ci) {
				const NkCorpsP2D &c = corps[ci];
				if (c.nombre == 0u || !c.actif) {
					continue; // un corps eteint n'entre dans aucune zone
				}
				NkVec2f mn;
				NkVec2f mx;
				BoiteCorps(ci, mn, mx);
				for (uint32 zi = 0; zi < mZones.Size(); ++zi) {
					const Rigide &z = mZones[zi];
					if (mx.x < z.mnx || mn.x > z.mxx || mx.y < z.mny || mn.y > z.mxy) {
						continue;
					}
					const float32 cB = math::NkCos(z.angle);
					const float32 sB = math::NkSin(z.angle);
					bool dedans = false;
					for (uint32 i = c.debut; i < c.debut + c.nombre && !dedans; ++i) {
						const NkParticule2D &p = particules[i];
						// La particule TOUCHE la zone : distance de son centre a la
						// forme sous son rayon.
						if (z.forme == 0) {
							const NkVec2f d = p.pos - z.centre;
							const float32 rr = z.rayon + p.rayon;
							dedans = d.x * d.x + d.y * d.y < rr * rr;
						} else if (z.forme == 1) {
							const NkVec2f d = p.pos - z.centre;
							const NkVec2f loc = Rot2(d, cB, -sB);
							const float32 qx = (loc.x < 0.f ? -loc.x : loc.x) - z.demi.x;
							const float32 qy = (loc.y < 0.f ? -loc.y : loc.y) - z.demi.y;
							const float32 ex = qx > 0.f ? qx : 0.f;
							const float32 ey = qy > 0.f ? qy : 0.f;
							dedans = ex * ex + ey * ey < p.rayon * p.rayon;
						} else {
							const NkVec2f ab = z.b - z.a;
							const float32 l2 = ab.x * ab.x + ab.y * ab.y;
							const float32 u = l2 > 1.0e-12f ? Clamp01(((p.pos - z.a).x * ab.x + (p.pos - z.a).y * ab.y) / l2) : 0.f;
							const NkVec2f d = p.pos - (z.a + ab * u);
							const float32 rr = z.rayon + p.rayon;
							dedans = d.x * d.x + d.y * d.y < rr * rr;
						}
					}
					if (dedans) {
						NoterPaire(c.id, z.id, NkGenreEvenementP2D::NK_ZONE);
					}
				}
			}
		}

		NkContactCorpsP2D NkParticules2D::ContactCorps(uint32 ci, float32 normaleYMin) const noexcept {
			NkContactCorpsP2D r;
			if (ci >= corps.Size()) {
				return r;
			}
			const NkCorpsP2D &c = corps[ci];
			// Meme calcul que ContactPartie, sur la plage du corps.
			NkVec2f somme(0.f, 0.f);
			uint32 rig[8] = {};
			uint32 nRig[8] = {};
			uint32 mou[8] = {};
			uint32 nMou[8] = {};
			const uint32 n = static_cast<uint32>(mContactNature.Size());
			for (uint32 i = c.debut; i < c.debut + c.nombre && i < n; ++i) {
				const uint8 nat = mContactNature[i];
				if (nat == 0u) {
					continue;
				}
				const NkVec2f s = mContactNormale[i];
				const float32 l = Len2d(s);
				const NkVec2f u = l > 1.0e-9f ? s * (1.f / l) : NkVec2f(0.f, 0.f);
				if (u.y < normaleYMin) {
					continue;
				}
				r.particules++;
				somme += u;
				r.natures = static_cast<uint8>(r.natures | nat);
				r.statique += (nat & NK_P2D_CONTACT_STATIQUE) ? 1u : 0u;
				r.cinematique += (nat & NK_P2D_CONTACT_CINEMATIQUE) ? 1u : 0u;
				r.dynamique += (nat & NK_P2D_CONTACT_DYNAMIQUE) ? 1u : 0u;
				r.limites += (nat & NK_P2D_CONTACT_LIMITES) ? 1u : 0u;
				r.mou += (nat & NK_P2D_CONTACT_MOU) ? 1u : 0u;
				// Les plus touches : huit candidats suffisent a un personnage.
				const uint32 idR = mContactRigide[i];
				for (int32 k = 0; idR != 0u && k < 8; ++k) {
					if (rig[k] == idR || rig[k] == 0u) {
						rig[k] = idR;
						nRig[k]++;
						break;
					}
				}
				const uint32 idM = mContactMou[i];
				for (int32 k = 0; idM != 0u && k < 8; ++k) {
					if (mou[k] == idM || mou[k] == 0u) {
						mou[k] = idM;
						nMou[k]++;
						break;
					}
				}
			}
			const float32 l = Len2d(somme);
			r.normale = l > 1.0e-9f ? somme * (1.f / l) : NkVec2f(0.f, 0.f);
			uint32 best = 0;
			for (int32 k = 0; k < 8; ++k) {
				if (nRig[k] > best) {
					best = nRig[k];
					r.rigide = rig[k];
				}
			}
			best = 0;
			for (int32 k = 0; k < 8; ++k) {
				if (nMou[k] > best) {
					best = nMou[k];
					r.autreCorps = mou[k];
				}
			}
			return r;
		}

		NkContactCorpsP2D NkParticules2D::ContactPartie(uint32 partie, float32 normaleYMin) const noexcept {
			NkContactCorpsP2D r;
			const int32 pi = IndexPartie(partie);
			if (pi < 0) {
				return r;
			}
			const NkPartieP2D &pa = parties[static_cast<uint32>(pi)];
			NkVec2f somme(0.f, 0.f);
			const uint32 n = static_cast<uint32>(mContactNature.Size());
			for (uint32 k = pa.debut; k < pa.debut + pa.nombre; ++k) {
				const uint32 i = partiesParticules[k];
				if (i >= n || mContactNature[i] == 0u) {
					continue;
				}
				const uint8 nat = mContactNature[i];
				const NkVec2f s = mContactNormale[i];
				const float32 l = Len2d(s);
				const NkVec2f u = l > 1.0e-9f ? s * (1.f / l) : NkVec2f(0.f, 0.f);
				if (u.y < normaleYMin) {
					continue;
				}
				r.particules++;
				somme += u;
				r.natures = static_cast<uint8>(r.natures | nat);
				r.statique += (nat & NK_P2D_CONTACT_STATIQUE) ? 1u : 0u;
				r.cinematique += (nat & NK_P2D_CONTACT_CINEMATIQUE) ? 1u : 0u;
				r.dynamique += (nat & NK_P2D_CONTACT_DYNAMIQUE) ? 1u : 0u;
				r.limites += (nat & NK_P2D_CONTACT_LIMITES) ? 1u : 0u;
				r.mou += (nat & NK_P2D_CONTACT_MOU) ? 1u : 0u;
				if (mContactRigide[i] != 0u) {
					r.rigide = mContactRigide[i];
				}
				if (mContactMou[i] != 0u) {
					r.autreCorps = mContactMou[i];
				}
			}
			const float32 l = Len2d(somme);
			r.normale = l > 1.0e-9f ? somme * (1.f / l) : NkVec2f(0.f, 0.f);
			return r;
		}

		// =====================================================================
		// Pousser sans ecraser
		// =====================================================================
		void NkParticules2D::AjouterVitesse(uint32 ci, const NkVec2f &dv) noexcept {
			if (ci >= corps.Size()) {
				return;
			}
			const NkCorpsP2D &c = corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					particules[i].vit += dv;
				}
			}
		}

		void NkParticules2D::AppliquerAcceleration(uint32 ci, const NkVec2f &a, float32 dt) noexcept {
			AjouterVitesse(ci, a * dt);
		}

		void NkParticules2D::AppliquerForce(uint32 ci, const NkVec2f &f, float32 dt) noexcept {
			if (ci >= corps.Size()) {
				return;
			}
			const NkCorpsP2D &c = corps[ci];
			float32 m = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					m += particules[i].masse;
				}
			}
			if (m <= 0.f) {
				return; // tout epingle : rien a pousser
			}
			AjouterVitesse(ci, f * (dt / m));
		}

		void NkParticules2D::AppliquerCouple(uint32 ci, float32 couple, float32 dt) noexcept {
			if (ci >= corps.Size()) {
				return;
			}
			const NkCorpsP2D &c = corps[ci];
			NkVec2f centre(0.f, 0.f);
			float32 m = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					centre += particules[i].pos * particules[i].masse;
					m += particules[i].masse;
				}
			}
			if (m <= 0.f) {
				return;
			}
			centre = centre * (1.f / m);
			float32 inertie = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					const NkVec2f r = particules[i].pos - centre;
					inertie += particules[i].masse * (r.x * r.x + r.y * r.y);
				}
			}
			if (inertie <= 1.0e-12f) {
				return;
			}
			// Acceleration angulaire alpha = couple / I ; chaque particule recoit la
			// vitesse tangentielle alpha dt x r (sens trigonometrique).
			const float32 dw = couple / inertie * dt;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					particules[i].vit += Perp2(particules[i].pos - centre) * dw;
				}
			}
		}

		float32 NkParticules2D::VitesseAngulaireCorps(uint32 ci) const noexcept {
			if (ci >= corps.Size()) {
				return 0.f;
			}
			const NkCorpsP2D &c = corps[ci];
			NkVec2f centre(0.f, 0.f);
			NkVec2f vm(0.f, 0.f);
			float32 m = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				centre += particules[i].pos * particules[i].masse;
				vm += particules[i].vit * particules[i].masse;
				m += particules[i].masse;
			}
			if (m <= 0.f) {
				return 0.f;
			}
			centre = centre * (1.f / m);
			vm = vm * (1.f / m);
			float32 L = 0.f;
			float32 I = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				const NkVec2f r = particules[i].pos - centre;
				L += particules[i].masse * Cross2(r, particules[i].vit - vm);
				I += particules[i].masse * (r.x * r.x + r.y * r.y);
			}
			return I > 1.0e-12f ? L / I : 0.f;
		}

		void NkParticules2D::FreinerRotation(uint32 ci, float32 fraction) noexcept {
			if (ci >= corps.Size() || fraction <= 0.f) {
				return;
			}
			const float32 w = VitesseAngulaireCorps(ci) * Clamp01(fraction);
			if (w == 0.f) {
				return;
			}
			const NkCorpsP2D &c = corps[ci];
			NkVec2f centre(0.f, 0.f);
			float32 m = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				centre += particules[i].pos * particules[i].masse;
				m += particules[i].masse;
			}
			centre = centre * (1.f / m);
			// La rotation rigide d'ensemble est v = w x r : on en retire la part voulue.
			// La somme des m (w x r) est nulle autour du centre de masse : la
			// translation n'est pas touchee.
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				if (particules[i].invMasse > 0.f) {
					particules[i].vit -= Perp2(particules[i].pos - centre) * w;
				}
			}
		}

		// =====================================================================
		// Parties nommees
		// =====================================================================
		int32 NkParticules2D::IndexPartie(uint32 id) const noexcept {
			if (id == 0u) {
				return -1;
			}
			for (uint32 i = 0; i < parties.Size(); ++i) {
				if (parties[i].id == id) {
					return static_cast<int32>(i);
				}
			}
			return -1;
		}

		uint32 NkParticules2D::TrouverPartie(uint32 corpsId, const char *nom) const noexcept {
			for (uint32 i = 0; i < parties.Size(); ++i) {
				if (parties[i].corps == corpsId && MemeNom(parties[i].nom, nom)) {
					return parties[i].id;
				}
			}
			return 0u;
		}

		uint32 NkParticules2D::CreerPartie(uint32 ci, const char *nom, const uint32 *relatives, uint32 nombre) noexcept {
			if (ci >= corps.Size() || nom == nullptr || nom[0] == '\0' || relatives == nullptr || nombre == 0u) {
				return 0u;
			}
			const NkCorpsP2D &c = corps[ci];
			if (TrouverPartie(c.id, nom) != 0u) {
				return 0u; // un nom est une CLE : deux « bras » dans un corps seraient ambigus
			}
			NkPartieP2D p;
			p.id = prochainIdPartie++;
			p.corps = c.id;
			uint32 k = 0;
			for (; k + 1u < sizeof(p.nom) && nom[k] != '\0'; ++k) {
				p.nom[k] = nom[k];
			}
			p.nom[k] = '\0';
			p.debut = static_cast<uint32>(partiesParticules.Size());
			for (uint32 i = 0; i < nombre; ++i) {
				if (relatives[i] < c.nombre) {
					partiesParticules.PushBack(c.debut + relatives[i]);
				}
			}
			p.nombre = static_cast<uint32>(partiesParticules.Size()) - p.debut;
			if (p.nombre == 0u) {
				return 0u;
			}
			parties.PushBack(p);
			return p.id;
		}

		uint32 NkParticules2D::CreerPartieZone(uint32 ci, const char *nom, const NkVec2f &centre, float32 rayon) noexcept {
			if (ci >= corps.Size()) {
				return 0u;
			}
			const NkCorpsP2D &c = corps[ci];
			NkVector<uint32> rel;
			for (uint32 i = 0; i < c.nombre; ++i) {
				const NkVec2f d = particules[c.debut + i].pos - centre;
				if (d.x * d.x + d.y * d.y <= rayon * rayon) {
					rel.PushBack(i);
				}
			}
			return rel.Size() > 0u ? CreerPartie(ci, nom, rel.Data(), static_cast<uint32>(rel.Size())) : 0u;
		}

		uint32 NkParticules2D::CreerPartieBoite(uint32 ci, const char *nom, const NkVec2f &mn, const NkVec2f &mx) noexcept {
			if (ci >= corps.Size()) {
				return 0u;
			}
			const NkCorpsP2D &c = corps[ci];
			NkVector<uint32> rel;
			for (uint32 i = 0; i < c.nombre; ++i) {
				const NkVec2f &q = particules[c.debut + i].pos;
				if (q.x >= mn.x && q.x <= mx.x && q.y >= mn.y && q.y <= mx.y) {
					rel.PushBack(i);
				}
			}
			return rel.Size() > 0u ? CreerPartie(ci, nom, rel.Data(), static_cast<uint32>(rel.Size())) : 0u;
		}

		void NkParticules2D::SupprimerPartie(uint32 id) noexcept {
			const int32 pi = IndexPartie(id);
			if (pi < 0) {
				return;
			}
			const NkPartieP2D p = parties[static_cast<uint32>(pi)];
			// Les indices de la partie sortent du tableau commun, les suivantes
			// reculent d'autant.
			partiesParticules.RemoveAt(p.debut, p.nombre);
			for (uint32 i = 0; i < parties.Size(); ++i) {
				if (parties[i].debut > p.debut) {
					parties[i].debut -= p.nombre;
				}
			}
			parties.RemoveAt(static_cast<uint32>(pi));
		}

		void NkParticules2D::AjouterVitessePartie(uint32 id, const NkVec2f &dv) noexcept {
			const int32 pi = IndexPartie(id);
			if (pi < 0) {
				return;
			}
			const NkPartieP2D &p = parties[static_cast<uint32>(pi)];
			for (uint32 k = p.debut; k < p.debut + p.nombre; ++k) {
				NkParticule2D &q = particules[partiesParticules[k]];
				if (q.invMasse > 0.f) {
					q.vit += dv;
				}
			}
		}

		void NkParticules2D::AppliquerForcePartie(uint32 id, const NkVec2f &f, float32 dt) noexcept {
			const int32 pi = IndexPartie(id);
			if (pi < 0) {
				return;
			}
			const NkPartieP2D &p = parties[static_cast<uint32>(pi)];
			float32 m = 0.f;
			for (uint32 k = p.debut; k < p.debut + p.nombre; ++k) {
				const NkParticule2D &q = particules[partiesParticules[k]];
				if (q.invMasse > 0.f) {
					m += q.masse;
				}
			}
			if (m > 0.f) {
				AjouterVitessePartie(id, f * (dt / m));
			}
		}

		NkVec2f NkParticules2D::CentrePartie(uint32 id) const noexcept {
			const int32 pi = IndexPartie(id);
			NkVec2f s(0.f, 0.f);
			if (pi < 0) {
				return s;
			}
			const NkPartieP2D &p = parties[static_cast<uint32>(pi)];
			for (uint32 k = p.debut; k < p.debut + p.nombre; ++k) {
				s += particules[partiesParticules[k]].pos;
			}
			return p.nombre > 0u ? s * (1.f / static_cast<float32>(p.nombre)) : s;
		}

		NkVec2f NkParticules2D::VitessePartie(uint32 id) const noexcept {
			const int32 pi = IndexPartie(id);
			NkVec2f s(0.f, 0.f);
			if (pi < 0) {
				return s;
			}
			const NkPartieP2D &p = parties[static_cast<uint32>(pi)];
			for (uint32 k = p.debut; k < p.debut + p.nombre; ++k) {
				s += particules[partiesParticules[k]].vit;
			}
			return p.nombre > 0u ? s * (1.f / static_cast<float32>(p.nombre)) : s;
		}

		// =====================================================================
		// Attaches particule <-> rigide
		// =====================================================================
		int32 NkParticules2D::Attacher(uint32 i, const NkPhysicsWorld &monde, NkBodyId rigide, float32 raideur,
									   float32 rupture) noexcept {
			const NkRigidBody *b = monde.GetBody(rigide);
			if (b == nullptr || i >= particules.Size()) {
				return -1;
			}
			NkAttacheP2D a;
			a.corps = corps[particules[i].corps].id;
			a.particule = i;
			a.rigide = rigide;
			// L'ancre : la particule, ramenee dans le repere du corps rigide.
			const float32 ang = 2.f * math::NkAtan2(b->orientation.z, b->orientation.w);
			const NkVec2f d = particules[i].pos - NkVec2f(b->position.x, b->position.y);
			a.ancre = Rot2(d, math::NkCos(ang), -math::NkSin(ang));
			a.raideur = Clamp01(raideur);
			a.rupture = rupture > 0.f ? rupture : 0.f;
			attaches.PushBack(a);
			return static_cast<int32>(attaches.Size() - 1u);
		}

		uint32 NkParticules2D::AttacherZone(uint32 ci, const NkVec2f &point, float32 rayon, const NkPhysicsWorld &monde,
											NkBodyId rigide, float32 raideur, float32 rupture) noexcept {
			if (ci >= corps.Size()) {
				return 0u;
			}
			const NkCorpsP2D &c = corps[ci];
			uint32 n = 0;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				const NkVec2f d = particules[i].pos - point;
				if (d.x * d.x + d.y * d.y <= rayon * rayon && Attacher(i, monde, rigide, raideur, rupture) >= 0) {
					++n;
				}
			}
			return n;
		}

		uint32 NkParticules2D::Detacher(NkBodyId rigide) noexcept {
			uint32 w = 0;
			uint32 retirees = 0;
			for (uint32 i = 0; i < attaches.Size(); ++i) {
				if (rigide == 0u || attaches[i].rigide == rigide) {
					++retirees;
					continue;
				}
				attaches[w++] = attaches[i];
			}
			attaches.Resize(w);
			return retirees;
		}

		uint32 NkParticules2D::AttachesActives() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < attaches.Size(); ++i) {
				n += attaches[i].casse ? 0u : 1u;
			}
			return n;
		}

		void NkParticules2D::RemapperRigides(const NkBodyId *anciens, const NkBodyId *nouveaux, uint32 n) noexcept {
			if (anciens == nullptr || nouveaux == nullptr) {
				return;
			}
			auto remap = [&](uint32 id) {
				for (uint32 k = 0; k < n; ++k) {
					if (anciens[k] == id) {
						return nouveaux[k];
					}
				}
				return id;
			};
			for (uint32 i = 0; i < attaches.Size(); ++i) {
				attaches[i].rigide = remap(attaches[i].rigide);
			}
			// Les paires en cours aussi : sans cela, une photo restauree donnerait
			// une FIN (ancien id) et un DEBUT (nouveau) pour un contact qui dure.
			for (uint32 i = 0; i < mPairesPrec.Size(); ++i) {
				if (mPairesPrec[i].genre != NkGenreEvenementP2D::NK_CORPS_MOU) {
					mPairesPrec[i].autre = remap(mPairesPrec[i].autre);
				}
			}
			for (uint32 i = 0; i < mPairesPas.Size(); ++i) {
				if (mPairesPas[i].genre != NkGenreEvenementP2D::NK_CORPS_MOU) {
					mPairesPas[i].autre = remap(mPairesPas[i].autre);
				}
			}
			for (uint32 i = 0; i < mContactRigide.Size(); ++i) {
				mContactRigide[i] = remap(mContactRigide[i]);
			}
		}

		void NkParticules2D::PreparerAttaches(NkPhysicsWorld *monde) noexcept {
			mAttacheRigide.Resize(attaches.Size());
			for (uint32 k = 0; k < attaches.Size(); ++k) {
				mAttacheRigide[k] = -1;
				const NkAttacheP2D &a = attaches[k];
				if (a.casse || monde == nullptr || a.particule >= particules.Size() ||
					corps[particules[a.particule].corps].id != a.corps) {
					continue;
				}
				for (uint32 ri = 0; ri < mRigides.Size(); ++ri) {
					if (mRigides[ri].id == a.rigide) {
						mAttacheRigide[k] = static_cast<int32>(ri);
						break;
					}
				}
				if (mAttacheRigide[k] >= 0) {
					continue;
				}
				// Un rigide dont la forme n'est pas couplee (polygone, 3D, zone) peut
				// quand meme porter des attaches : il entre dans mRigides en forme 3,
				// que ResoudreRigides saute.
				NkRigidBody *b = monde->GetBody(a.rigide);
				if (b == nullptr) {
					continue;
				}
				Rigide r;
				r.corps = b;
				r.id = b->id;
				r.type = static_cast<uint8>(b->type);
				r.forme = 3;
				r.angleCorps = 2.f * math::NkAtan2(b->orientation.z, b->orientation.w);
				r.posCorps = NkVec2f(b->position.x, b->position.y);
				r.centre = r.posCorps;
				const bool dyn = b->type == NkBodyType::DYNAMIC;
				const bool bouge = b->type != NkBodyType::STATIC;
				r.vitesse = bouge ? NkVec2f(b->linearVelocity.x, b->linearVelocity.y) : NkVec2f(0.f, 0.f);
				r.omega = bouge ? b->angularVelocity.z : 0.f;
				r.vitessePose = r.vitesse;
				r.omegaPose = r.omega;
				r.invMasse = dyn ? b->invMass : 0.f;
				r.invInertie = (dyn && (b->flags & NK_BODY_FIXED_ROT) == 0u) ? b->invInertiaDiag.z : 0.f;
				mRigides.PushBack(r);
				mAttacheRigide[k] = static_cast<int32>(mRigides.Size() - 1u);
			}
		}

		void NkParticules2D::PoseRigide(const Rigide &r, float32 t, NkVec2f &pivot, float32 &dA) const noexcept {
			// La MEME extrapolation que ResoudreRigides (voir son commentaire : moitie
			// vitesse du debut, moitie vitesse apres impulses) -- une attache et un
			// contact sur le meme corps doivent voir la meme pose.
			dA = r.omegaPose * t;
			pivot = r.posCorps + (r.vitessePose + (r.vitesse - r.vitessePose) * 0.5f) * t;
		}

		void NkParticules2D::ResoudreAttaches(float32 h, float32 t, int32 sousPas) noexcept {
			if (attaches.Size() == 0u || mAttacheRigide.Size() != attaches.Size()) {
				return;
			}
			const float32 h2 = h * h;
			const float32 dtPas = h * static_cast<float32>(sousPas > 0 ? sousPas : 1);
			for (uint32 k = 0; k < attaches.Size(); ++k) {
				NkAttacheP2D &a = attaches[k];
				const int32 ri = mAttacheRigide[k];
				if (a.casse || ri < 0 || a.particule >= particules.Size()) {
					continue;
				}
				Rigide &r = mRigides[static_cast<uint32>(ri)];
				NkParticule2D &p = particules[a.particule];
				NkVec2f pivot;
				float32 dA = 0.f;
				PoseRigide(r, t, pivot, dA);
				const float32 ang = r.angleCorps + dA;
				const NkVec2f bras = Rot2(a.ancre, math::NkCos(ang), math::NkSin(ang));
				const NkVec2f A = pivot + bras;
				const NkVec2f d = p.pos - A;
				const float32 len = Len2d(d);
				// `ecart` = ce qui RESTE apres correction (plus bas) : c'est l'ecart
				// qui se voit. La rupture, elle, se juge sur la tension AVANT.
				a.ecart = len;
				a.tension = len;
				if (a.rupture > 0.f && len > a.rupture) {
					a.casse = true;
					stats.rupturesAttaches++;
					continue;
				}
				if (p.invMasse == 0.f || len < 1.0e-7f || !corps[p.corps].actif) {
					continue;
				}
				const NkVec2f n = d * (1.f / len);
				const float32 rn = Cross2(bras, n);
				const float32 wP = p.invMasse;
				const float32 wB = r.invMasse + rn * rn * r.invInertie;
				const float32 alpha = NkComplianceP2D(a.raideur) / h2;
				const float32 denom = wP + wB + alpha;
				// Vitesse de la particule AVANT correction : c'est elle que le rigide
				// doit rattraper (ou freiner).
				const NkVec2f vP = (p.pos - p.prec) * (1.f / h);
				// POSITION : la particule prend SA part de l'ecart (toute, si le
				// rigide est statique ou cinematique).
				p.pos -= n * (len * wP / denom);
				a.ecart = len * (1.f - wP / denom);

				NkRigidBody *corpsR = static_cast<NkRigidBody *>(r.corps);
				if (corpsR == nullptr || r.invMasse <= 0.f) {
					continue;
				}
				// IMPULSION sur le rigide, en deux termes :
				//  1. la vitesse RELATIVE particule / point d'ancre, annulee comme par
				//     un contact bilateral : c'est l'echange de quantite de mouvement ;
				//  2. la part d'ecart du rigide, etalee sur le PAS et divisee par le
				//     nombre de sous-pas -- chaque sous-pas voit presque le meme ecart
				//     (le rigide n'avance qu'a son Step), et sans la division il le
				//     rattrapait N fois (le lambda / h que ResoudreRigides a mesure
				//     explosif).
				const NkVec2f vPoint = r.vitesse + Perp2(bras) * r.omega;
				const NkVec2f vRel = vP - vPoint;
				const float32 lv = Len2d(vRel);
				NkVec2f J(0.f, 0.f);
				if (lv > 1.0e-9f) {
					const NkVec2f u = vRel * (1.f / lv);
					const float32 ru = Cross2(bras, u);
					const float32 wBu = r.invMasse + ru * ru * r.invInertie;
					J = vRel * (1.f / (wP + wBu));
				}
				J += n * (len / (denom * dtPas * static_cast<float32>(sousPas > 0 ? sousPas : 1)));
				const NkVec2f point = A;
				NkApplyImpulseAtPoint(*corpsR, NkVec3f(J.x, J.y, 0.f), NkVec3f(point.x, point.y, 0.f));
				r.vitesse += J * r.invMasse;
				r.omega += Cross2(point - pivot, J) * r.invInertie;
				corpsR->flags &= ~static_cast<uint32>(NK_BODY_SLEEPING);
				corpsR->sleepTimer = 0.f;
			}
		}

		// =====================================================================
		// Relier deux corps
		// =====================================================================
		uint32 NkParticules2D::RelierCorps(uint32 a, uint32 b, float32 portee) noexcept {
			if (a >= corps.Size() || b >= corps.Size() || a == b || portee <= 0.f) {
				return 0u;
			}
			const NkCorpsP2D ca = corps[a];
			const NkCorpsP2D cb = corps[b];
			uint32 n = 0;
			for (uint32 i = ca.debut; i < ca.debut + ca.nombre; ++i) {
				int32 meilleur = -1;
				float32 d2Min = portee * portee;
				for (uint32 j = cb.debut; j < cb.debut + cb.nombre; ++j) {
					const NkVec2f d = particules[j].pos - particules[i].pos;
					const float32 d2 = d.x * d.x + d.y * d.y;
					if (d2 <= d2Min) {
						d2Min = d2;
						meilleur = static_cast<int32>(j);
					}
				}
				if (meilleur >= 0 && Relier(i, static_cast<uint32>(meilleur), math::NkSqrt(d2Min))) {
					++n;
				}
			}
			return n;
		}

		// =====================================================================
		// Saisies par pointeur
		// =====================================================================
		uint32 NkParticules2D::SaisirPointeurDebut(uint32 pointeur, const NkVec2f &p, float32 rayon, int32 ci) noexcept {
			SaisirPointeurFin(pointeur); // un pointeur ne tient qu'une poignee a la fois
			uint32 prises = 0;
			auto libre = [&](uint32 i) {
				const NkParticule2D &q = particules[i];
				if (q.saisie || !corps[q.corps].actif) {
					return false; // tenue (jamais volee), ou d'un corps eteint (jamais prise)
				}
				return ci < 0 || q.corps == static_cast<uint32>(ci);
			};
			for (uint32 i = 0; i < particules.Size(); ++i) {
				if (libre(i) && Len2d(particules[i].pos - p) <= rayon) {
					mPtrId.PushBack(pointeur);
					mPtrParticule.PushBack(i);
					// Decalage RESSERRE, comme SaisirDebut : une poignee de matiere.
					mPtrDecalage.PushBack((particules[i].pos - p) * 0.6f);
					particules[i].saisie = true;
					++prises;
				}
			}
			if (prises == 0u) {
				// Rien dans le disque : la plus proche LIBRE a 2,5 rayons, comme
				// SaisirDebut -- un doigt est moins precis qu'une souris.
				int32 k = -1;
				float32 best = (rayon * 2.5f) * (rayon * 2.5f);
				for (uint32 i = 0; i < particules.Size(); ++i) {
					if (!libre(i)) {
						continue;
					}
					const float32 r = particules[i].rayon;
					const NkVec2f d = particules[i].pos - p;
					const float32 d2 = d.x * d.x + d.y * d.y - r * r;
					if (d2 < best) {
						best = d2;
						k = static_cast<int32>(i);
					}
				}
				if (k < 0) {
					return 0u;
				}
				mPtrId.PushBack(pointeur);
				mPtrParticule.PushBack(static_cast<uint32>(k));
				mPtrDecalage.PushBack((particules[static_cast<uint32>(k)].pos - p) * 0.6f);
				particules[static_cast<uint32>(k)].saisie = true;
				prises = 1u;
			}
			Pointeur ptr;
			ptr.id = pointeur;
			ptr.cible = p;
			mPointeurs.PushBack(ptr);
			return prises;
		}

		void NkParticules2D::SaisirPointeurVers(uint32 pointeur, const NkVec2f &p) noexcept {
			for (uint32 i = 0; i < mPointeurs.Size(); ++i) {
				if (mPointeurs[i].id == pointeur) {
					mPointeurs[i].cible = p;
					return;
				}
			}
		}

		void NkParticules2D::SaisirPointeurFin(uint32 pointeur) noexcept {
			uint32 w = 0;
			for (uint32 k = 0; k < mPtrId.Size(); ++k) {
				if (mPtrId[k] == pointeur) {
					if (mPtrParticule[k] < particules.Size()) {
						particules[mPtrParticule[k]].saisie = false;
					}
					continue;
				}
				mPtrId[w] = mPtrId[k];
				mPtrParticule[w] = mPtrParticule[k];
				mPtrDecalage[w] = mPtrDecalage[k];
				++w;
			}
			mPtrId.Resize(w);
			mPtrParticule.Resize(w);
			mPtrDecalage.Resize(w);
			for (uint32 i = 0; i < mPointeurs.Size(); ++i) {
				if (mPointeurs[i].id == pointeur) {
					mPointeurs.RemoveAt(i);
					break;
				}
			}
			// La saisie historique garde ses drapeaux : une particule qu'elle tient
			// aussi ne doit pas etre relachee par ce pointeur.
			for (uint32 k = 0; k < mSaisis.Size(); ++k) {
				if (mSaisis[k] < particules.Size()) {
					particules[mSaisis[k]].saisie = true;
				}
			}
		}

		void NkParticules2D::SaisirPointeursFin() noexcept {
			while (mPointeurs.Size() > 0u) {
				SaisirPointeurFin(mPointeurs[0].id);
			}
		}

		bool NkParticules2D::EnSaisiePointeur(uint32 pointeur) const noexcept {
			for (uint32 i = 0; i < mPointeurs.Size(); ++i) {
				if (mPointeurs[i].id == pointeur) {
					return true;
				}
			}
			return false;
		}

		uint32 NkParticules2D::ParticulesSaisies(uint32 pointeur) const noexcept {
			uint32 n = 0;
			for (uint32 k = 0; k < mPtrId.Size(); ++k) {
				n += mPtrId[k] == pointeur ? 1u : 0u;
			}
			return n;
		}

		void NkParticules2D::ResoudreSaisiesPointeurs() noexcept {
			for (uint32 k = 0; k < mPtrParticule.Size(); ++k) {
				const uint32 i = mPtrParticule[k];
				if (i >= particules.Size()) {
					continue;
				}
				NkVec2f cible(0.f, 0.f);
				bool trouve = false;
				for (uint32 q = 0; q < mPointeurs.Size(); ++q) {
					if (mPointeurs[q].id == mPtrId[k]) {
						cible = mPointeurs[q].cible + mPtrDecalage[k];
						trouve = true;
						break;
					}
				}
				if (!trouve) {
					continue;
				}
				NkParticule2D &p = particules[i];
				p.saisie = true; // la saisie historique a pu remettre le drapeau a faux
				if (p.epingle) {
					p.pos = cible;
					p.prec = cible;
				} else {
					p.pos += (cible - p.pos) * 0.35f;
				}
			}
		}

		// =====================================================================
		// Compaction
		// =====================================================================
		void NkParticules2D::RemapperJeu(const NkVector<uint32> &remap) noexcept {
			const uint32 n = static_cast<uint32>(remap.Size());
			auto nouveau = [&](uint32 i) { return i < n ? remap[i] : NK_P2D_AUCUN; };

			// Attaches : une particule disparue emporte son attache.
			uint32 w = 0;
			for (uint32 k = 0; k < attaches.Size(); ++k) {
				NkAttacheP2D a = attaches[k];
				a.particule = nouveau(a.particule);
				if (a.particule == NK_P2D_AUCUN) {
					continue;
				}
				attaches[w++] = a;
			}
			attaches.Resize(w);
			mAttacheRigide.Clear(); // refait au prochain Pas

			// Parties : le tableau commun est reconstruit, sans trou.
			NkVector<uint32> refait;
			refait.Reserve(partiesParticules.Size());
			uint32 wp = 0;
			for (uint32 k = 0; k < parties.Size(); ++k) {
				NkPartieP2D p = parties[k];
				const uint32 debut = static_cast<uint32>(refait.Size());
				for (uint32 q = p.debut; q < p.debut + p.nombre; ++q) {
					const uint32 j = nouveau(partiesParticules[q]);
					if (j != NK_P2D_AUCUN) {
						refait.PushBack(j);
					}
				}
				p.debut = debut;
				p.nombre = static_cast<uint32>(refait.Size()) - debut;
				if (p.nombre == 0u) {
					continue; // plus une particule : la partie n'a plus d'objet
				}
				parties[wp++] = p;
			}
			parties.Resize(wp);
			partiesParticules = refait;

			// Saisies par pointeur.
			uint32 ws = 0;
			for (uint32 k = 0; k < mPtrParticule.Size(); ++k) {
				const uint32 j = nouveau(mPtrParticule[k]);
				if (j == NK_P2D_AUCUN) {
					continue;
				}
				mPtrId[ws] = mPtrId[k];
				mPtrParticule[ws] = j;
				mPtrDecalage[ws] = mPtrDecalage[k];
				++ws;
			}
			mPtrId.Resize(ws);
			mPtrParticule.Resize(ws);
			mPtrDecalage.Resize(ws);

			// Contacts du pas : ils suivent leurs particules, pour qu'une requete
			// faite APRES un Pas qui a compacte (chute hors du monde) reste juste.
			if (mContactNature.Size() == n) {
				uint32 wc = 0;
				for (uint32 i = 0; i < n; ++i) {
					if (remap[i] == NK_P2D_AUCUN) {
						continue;
					}
					mContactNature[wc] = mContactNature[i];
					mContactNormale[wc] = mContactNormale[i];
					mContactRigide[wc] = mContactRigide[i];
					mContactMou[wc] = mContactMou[i];
					++wc;
				}
				mContactNature.Resize(wc);
				mContactNormale.Resize(wc);
				mContactRigide.Resize(wc);
				mContactMou.Resize(wc);
			} else {
				mContactNature.Clear();
				mContactNormale.Clear();
				mContactRigide.Clear();
				mContactMou.Clear();
			}
		}

	} // namespace physics
} // namespace nkentseu
