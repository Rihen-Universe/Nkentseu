//
// NkUnkenyEffets.cpp
// =============================================================================
// Description :
//   La vie des particules visuelles : naissance a l'emetteur, mouvement,
//   vieillissement, mort. Voir NkUnkenyEffets.h.
//
// Algorithmes implementes :
//   - xorshift32 (Marsaglia 2003) par emetteur : sept operations, un etat de
//     32 bits, reproductible partout. Le meme que NkParticules2D::Alea, que ce
//     fichier ne peut pas appeler (prive).
//   - retrait par ECHANGE avec la derniere : O(1) par mort, l'ordre change mais
//     il est lui aussi deterministe.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Effets/NkUnkenyEffets.h"

#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		namespace {
			constexpr float32 PI = 3.14159265f;

			/// xorshift32. Un etat nul resterait nul pour toujours : on le refuse.
			uint32 Suivant(uint32 &x) noexcept {
				if (x == 0u) {
					x = 0x9E3779B9u;
				}
				x ^= x << 13;
				x ^= x >> 17;
				x ^= x << 5;
				return x;
			}

			/// [0, 1[ sur 24 bits : la mantisse d'un float32, rien de perdu.
			float32 Alea01(uint32 &x) noexcept {
				return static_cast<float32>(Suivant(x) >> 8) * (1.f / 16777216.f);
			}

			float32 Entre(uint32 &x, float32 a, float32 b) noexcept {
				return a + (b - a) * Alea01(x);
			}

			/// La graine d'un emetteur melangee une fois : deux graines voisines (1
			/// et 2) donneraient sinon des premiers tirages presque egaux.
			uint32 Semer(uint32 graine) noexcept {
				uint32 h = graine * 0x9E3779B1u + 0x7F4A7C15u;
				h ^= h >> 16;
				h *= 0x85EBCA6Bu;
				h ^= h >> 13;
				return h != 0u ? h : 0x9E3779B9u;
			}
		} // namespace

		// =====================================================================
		void NkEffets2D::Vider() {
			mParticules.Clear();
			mEtats.Clear();
			mAccumulateur = 0.f;
			mRefusees = 0u;
		}

		const NkEtatEmetteur2D *NkEffets2D::Etat(uint64 entite) const noexcept {
			for (uint32 i = 0; i < mEtats.Size(); ++i) {
				if (mEtats[i].entite == entite) {
					return &mEtats[i];
				}
			}
			return nullptr;
		}

		NkEtatEmetteur2D &NkEffets2D::EtatDe(uint64 entite, uint32 graine, bool demarre) {
			for (uint32 i = 0; i < mEtats.Size(); ++i) {
				if (mEtats[i].entite == entite) {
					return mEtats[i];
				}
			}
			NkEtatEmetteur2D e;
			e.entite = entite;
			e.alea = Semer(graine);
			e.lecture = demarre ? NkLectureEffet2D::NK_JOUE : NkLectureEffet2D::NK_ARRETE;
			mEtats.PushBack(e);
			return mEtats.Back();
		}

		// =====================================================================
		// PILOTER (R34)
		// =====================================================================
		bool NkEffets2D::Jouer(NkScene &scene, ecs::NkEntityId id) {
			const NkEmetteur2D *e = scene.Monde().IsAlive(id) ? scene.Monde().Get<NkEmetteur2D>(id) : nullptr;
			if (e == nullptr) {
				return false;
			}
			NkEtatEmetteur2D &s = EtatDe(id.Pack(), e->graine, true);
			if (s.lecture == NkLectureEffet2D::NK_ARRETE) {
				// Du DEBUT : la meme graine, la meme rafale -- le meme effet.
				s.alea = Semer(e->graine);
				s.accumulateur = 0.f;
				s.age = 0.f;
				s.vacillement = 0.f;
				s.rafaleFaite = false;
			}
			s.lecture = NkLectureEffet2D::NK_JOUE;
			return true;
		}

		bool NkEffets2D::Arreter(NkScene &scene, ecs::NkEntityId id, bool vider) {
			const NkEmetteur2D *e = scene.Monde().IsAlive(id) ? scene.Monde().Get<NkEmetteur2D>(id) : nullptr;
			if (e == nullptr) {
				return false;
			}
			const uint64 k = id.Pack();
			NkEtatEmetteur2D &s = EtatDe(k, e->graine, false);
			s.lecture = NkLectureEffet2D::NK_ARRETE;
			if (vider) {
				for (uint32 i = 0; i < mParticules.Size();) {
					if (mParticules[i].emetteur == k) {
						mParticules[i] = mParticules.Back();
						mParticules.PopBack();
					} else {
						++i;
					}
				}
			}
			return true;
		}

		bool NkEffets2D::Pause(NkScene &scene, ecs::NkEntityId id, bool pause) {
			const NkEmetteur2D *e = scene.Monde().IsAlive(id) ? scene.Monde().Get<NkEmetteur2D>(id) : nullptr;
			if (e == nullptr) {
				return false;
			}
			NkEtatEmetteur2D &s = EtatDe(id.Pack(), e->graine, e->jouerAuDemarrage);
			if (pause && s.lecture == NkLectureEffet2D::NK_JOUE) {
				s.lecture = NkLectureEffet2D::NK_PAUSE;
			} else if (!pause && s.lecture == NkLectureEffet2D::NK_PAUSE) {
				s.lecture = NkLectureEffet2D::NK_JOUE;
			}
			return true;
		}

		NkLectureEffet2D NkEffets2D::Lecture(NkScene &scene, ecs::NkEntityId id) const {
			const NkEtatEmetteur2D *s = Etat(id.Pack());
			if (s != nullptr) {
				return s->lecture;
			}
			const NkEmetteur2D *e = scene.Monde().IsAlive(id) ? scene.Monde().Get<NkEmetteur2D>(id) : nullptr;
			return e != nullptr && !e->jouerAuDemarrage ? NkLectureEffet2D::NK_ARRETE : NkLectureEffet2D::NK_JOUE;
		}

		void NkEffets2D::Rejouer(uint64 entite) {
			for (uint32 i = 0; i < mEtats.Size(); ++i) {
				if (mEtats[i].entite == entite) {
					// L'horloge est RETIREE : elle renaitra, semee, au prochain pas.
					mEtats[i] = mEtats.Back();
					mEtats.PopBack();
					return;
				}
			}
		}

		float32 NkEffets2D::FacteurLumiere(uint64 entite, const NkEmetteur2D &e) const noexcept {
			if (!e.actif) {
				return 0.f;
			}
			const NkEtatEmetteur2D *s = Etat(entite);
			if (s == nullptr) {
				return e.boucle ? 1.f : 0.f;
			}
			// (R34) En jeu, un effet ARRETE n'eclaire plus ; en pause, il garde sa lumiere.
			if (!edition && s->lecture == NkLectureEffet2D::NK_ARRETE) {
				return 0.f;
			}
			float32 activite = 1.f;
			if (!e.boucle && s->age > e.duree) {
				// Apres la derniere emission, la lumiere suit la vie des particules :
				// une explosion s'eteint avec ses flammeches, pas d'un coup.
				const float32 fondu = e.vieMax > 0.01f ? e.vieMax : 0.01f;
				activite = 1.f - (s->age - e.duree) / fondu;
				activite = activite < 0.f ? 0.f : activite;
			}
			const float32 v = 1.f - e.scintillement * s->vacillement;
			return activite * (v < 0.f ? 0.f : v);
		}

		// =====================================================================
		void NkEffets2D::Naitre(NkEtatEmetteur2D &etat, const NkEmetteur2D &e, const NkTransform2D &t) {
			// ⚠️ DEUX PLAFONDS, ET LE REFUS SE COMPTE. Une naissance refusee ne
			//    tire RIEN au hasard : sinon le plafond d'un emetteur decalerait
			//    toutes ses particules suivantes, et la graine ne dirait plus rien.
			if (etat.vivantes >= e.maxParticules || mParticules.Size() >= plafond) {
				++mRefusees;
				return;
			}
			uint32 &x = etat.alea;
			NkParticuleEffet2D p;
			NkVec2f local = e.decalage;
			switch (e.zone) {
				case NkZoneEmission2D::NK_DISQUE: {
					// sqrt du tirage : sans elle, les particules s'entassent au centre.
					const float32 r = e.rayonZone * math::NkSqrt(Alea01(x));
					const float32 a = Entre(x, -PI, PI);
					local = local + NkVec2f(r * math::NkCos(a), r * math::NkSin(a));
					break;
				}
				case NkZoneEmission2D::NK_LIGNE:
					local = local + NkVec2f(Entre(x, -0.5f, 0.5f) * e.largeurZone, 0.f);
					break;
				default:
					break;
			}
			p.pos = t.VersMonde(local);
			const float32 angle = e.direction + t.rotation + Entre(x, -e.dispersion, e.dispersion);
			const float32 v = Entre(x, e.vitesseMin, e.vitesseMax);
			p.vit = NkVec2f(math::NkCos(angle) * v, math::NkSin(angle) * v);
			p.vie = Entre(x, e.vieMin, e.vieMax);
			p.vie = p.vie < 0.02f ? 0.02f : p.vie;
			// Une variation de taille de +-20 % : cent flammeches de meme diametre
			// se lisent comme une grille, pas comme un feu.
			const float32 k = Entre(x, 0.8f, 1.2f);
			p.taille0 = e.tailleDebut * k;
			p.taille1 = e.tailleFin * k;
			p.gravite = e.gravite;
			p.frein = e.frein;
			p.c0 = e.couleurDebut;
			p.c1 = e.couleurMilieu;
			p.c2 = e.couleurFin;
			p.emetteur = etat.entite;
			p.forme = e.forme;
			p.additif = e.additif;
			mParticules.PushBack(p);
			++etat.vivantes;
		}

		void NkEffets2D::Pas(NkScene &scene) {
			const float32 dt = pasFixe;
			for (uint32 i = 0; i < mEtats.Size(); ++i) {
				mEtats[i].vu = false;
				mEtats[i].vivantes = 0u;
			}
			// Le compte par emetteur, refait : plus sur qu'un compteur tenu a jour
			// a chaque mort, qui deriverait au premier Rejouer. Les particules d'un
			// meme emetteur se suivent : on garde le dernier trouve.
			NkEtatEmetteur2D *dernier = nullptr;
			for (uint32 i = 0; i < mParticules.Size(); ++i) {
				const uint64 k = mParticules[i].emetteur;
				if (dernier == nullptr || dernier->entite != k) {
					dernier = nullptr;
					for (uint32 j = 0; j < mEtats.Size(); ++j) {
						if (mEtats[j].entite == k) {
							dernier = &mEtats[j];
							break;
						}
					}
				}
				if (dernier != nullptr) {
					++dernier->vivantes;
				}
			}

			// 1. Les naissances. On ne garde pas de pointeur sur les composants
			//    apres la requete (voir NkUnkenyRendu.cpp) : tout se fait dedans.
			scene.Monde().Query<NkTransform2D, NkEmetteur2D>().ForEach(
				[&](ecs::NkEntityId id, NkTransform2D &t, NkEmetteur2D &e) {
					NkEtatEmetteur2D &etat = EtatDe(id.Pack(), e.graine, e.jouerAuDemarrage);
					etat.vu = true;
					// Une entite ETEINTE (NkUnkenyActif.h) n'emet plus ; son etat reste.
					if (!e.actif || !scene.EstActive(id)) {
						return;
					}
					// (R34) En EDITION, seul l'apercu choisi tourne ; en JEU, la lecture.
					if (edition ? !e.apercuEdition : etat.lecture != NkLectureEffet2D::NK_JOUE) {
						return;
					}
					if (!etat.rafaleFaite) {
						etat.rafaleFaite = true;
						for (uint32 n = 0; n < e.rafale; ++n) {
							Naitre(etat, e, t);
						}
					}
					const bool emet = e.boucle || etat.age < e.duree;
					if (emet && e.debit > 0.f) {
						etat.accumulateur += e.debit * dt;
						// Borne : un debit absurde (1e9) ne doit pas bloquer la trame.
						int32 n = static_cast<int32>(etat.accumulateur);
						etat.accumulateur -= static_cast<float32>(n);
						n = n > 4096 ? 4096 : n;
						for (int32 k = 0; k < n; ++k) {
							Naitre(etat, e, t);
						}
					}
					etat.age += dt;
					// Le vacillement suit un tirage en DOUCEUR (filtre du premier
					// ordre) : un tirage brut par pas ferait clignoter, pas vaciller.
					if (e.scintillement > 0.f) {
						etat.vacillement += (Alea01(etat.alea) - etat.vacillement) * 0.25f;
					}
				});

			// 2. Les horloges des emetteurs disparus (entite detruite, composant
			//    retire) : oubliees. Leurs particules, elles, finissent leur vie.
			for (uint32 i = 0; i < mEtats.Size();) {
				if (!mEtats[i].vu) {
					mEtats[i] = mEtats.Back();
					mEtats.PopBack();
				} else {
					++i;
				}
			}

			// (R34) Les particules FIGEES (emetteur en pause, en jeu) et EFFACEES (en
			// edition, celles d'un emetteur sans apercu : decocher l'apercu le
			// vide). Ni l'un ni l'autre : la boucle d'avant, sans cout.
			NkVector<uint64> figes, effaces;
			for (uint32 i = 0; i < mEtats.Size(); ++i) {
				if (!edition && mEtats[i].lecture == NkLectureEffet2D::NK_PAUSE) {
					figes.PushBack(mEtats[i].entite);
				}
			}
			if (edition && !mParticules.Empty()) {
				scene.Monde().Query<NkEmetteur2D>().ForEach([&](ecs::NkEntityId id, NkEmetteur2D &e) {
					if (!e.apercuEdition) {
						effaces.PushBack(id.Pack());
					}
				});
			}
			auto Dans = [](const NkVector<uint64> &v, uint64 k) {
				for (uint32 j = 0; j < v.Size(); ++j) {
					if (v[j] == k) {
						return true;
					}
				}
				return false;
			};

			// 3. Le mouvement et la mort.
			for (uint32 i = 0; i < mParticules.Size();) {
				NkParticuleEffet2D &p = mParticules[i];
				if (!effaces.Empty() && Dans(effaces, p.emetteur)) {
					mParticules[i] = mParticules.Back();
					mParticules.PopBack();
					continue;
				}
				if (!figes.Empty() && Dans(figes, p.emetteur)) {
					++i;
					continue;
				}
				p.age += dt;
				if (p.age >= p.vie) {
					mParticules[i] = mParticules.Back();
					mParticules.PopBack();
					continue;
				}
				p.vit.x += p.gravite.x * dt;
				p.vit.y += p.gravite.y * dt;
				if (p.frein > 0.f) {
					const float32 f = 1.f - p.frein * dt;
					p.vit = p.vit * (f > 0.f ? f : 0.f);
				}
				p.pos.x += p.vit.x * dt;
				p.pos.y += p.vit.y * dt;
				++i;
			}
		}

		void NkEffets2D::Avancer(NkScene &scene, float32 dt) {
			if (dt <= 0.f || pasFixe <= 0.f) {
				return;
			}
			// ⚠️ LE CHEMIN SANS EFFET NE COUTE QU'UN TEST. Une scene sans emetteur
			//    ni particule ne lance pas de requete : un jeu de dames ne paie rien.
			if (mParticules.Empty() && mEtats.Empty()) {
				bool aucun = true;
				scene.Monde().Query<NkEmetteur2D>().ForEach([&aucun](ecs::NkEntityId, NkEmetteur2D &) { aucun = false; });
				if (aucun) {
					mAccumulateur = 0.f;
					return;
				}
			}
			mAccumulateur += dt;
			int32 n = 0;
			// La meme boucle que NkScene::Pas. (Une tolerance d'un millieme de pas
			// a ete essayee le 30/09 contre une derive supposee a 30 i/s : la
			// contre-epreuve l'a montree inutile -- 1/30 vaut exactement deux fois
			// 1/60 en float32 -- et elle a ete retiree. P1c le garde.)
			while (mAccumulateur >= pasFixe && n < 8) {
				Pas(scene);
				mAccumulateur -= pasFixe;
				++n;
			}
			if (mAccumulateur > pasFixe * 8.f) {
				mAccumulateur = 0.f;
			}
		}

		// =====================================================================
		// Les presets. Les valeurs de Noge (NkParticleEmitter.h, en unites de
		// son monde 3D) sont ramenees au metre d'Unkeny (une caisse fait 0,6 m).
		// =====================================================================
		NkEmetteur2D NkPresetEmetteur2D(NkPresetEffet2D preset) {
			NkEmetteur2D e;
			e.preset = preset;
			switch (preset) {
				case NkPresetEffet2D::NK_FEU:
					e.debit = 70.f;
					e.vieMin = 0.45f;
					e.vieMax = 0.9f;
					e.vitesseMin = 0.5f;
					e.vitesseMax = 1.2f;
					e.direction = PI * 0.5f;
					e.dispersion = 0.32f;
					e.gravite = NkVec2f(0.f, 1.6f); // l'air chaud monte
					e.frein = 1.2f;
					e.tailleDebut = 0.42f;
					e.tailleFin = 0.08f;
					e.couleurDebut = 0xFFF0A0FFu;
					e.couleurMilieu = 0xFF7A1EC8u;
					e.couleurFin = 0x6E140000u;
					e.additif = true;
					e.zone = NkZoneEmission2D::NK_DISQUE;
					e.rayonZone = 0.18f;
					e.maxParticules = 160u;
					e.eclaire = true;
					e.couleurLumiere = 0xFF9A48FFu;
					e.intensiteLumiere = 1.25f;
					e.porteeLumiere = 5.f;
					e.scintillement = 0.3f;
					break;
				case NkPresetEffet2D::NK_FUMEE:
					e.debit = 10.f;
					e.vieMin = 2.2f;
					e.vieMax = 3.6f;
					e.vitesseMin = 0.25f;
					e.vitesseMax = 0.55f;
					e.direction = PI * 0.5f;
					e.dispersion = 0.35f;
					e.gravite = NkVec2f(0.15f, 0.25f); // un peu de vent
					e.frein = 0.35f;
					e.tailleDebut = 0.35f;
					e.tailleFin = 1.6f;
					e.couleurDebut = 0x5C5C6490u;
					e.couleurMilieu = 0x6E6E7460u;
					e.couleurFin = 0x80808200u;
					e.additif = false;
					e.zone = NkZoneEmission2D::NK_DISQUE;
					e.rayonZone = 0.15f;
					e.maxParticules = 64u;
					break;
				case NkPresetEffet2D::NK_ETINCELLES:
					e.debit = 28.f;
					e.vieMin = 0.4f;
					e.vieMax = 1.0f;
					e.vitesseMin = 2.f;
					e.vitesseMax = 4.5f;
					e.direction = PI * 0.5f;
					e.dispersion = 0.75f;
					e.gravite = NkVec2f(0.f, -6.f);
					e.frein = 0.3f;
					e.tailleDebut = 0.06f;
					e.tailleFin = 0.02f;
					e.couleurDebut = 0xFFF4C0FFu;
					e.couleurMilieu = 0xFFA83CFFu;
					e.couleurFin = 0xFF400000u;
					e.additif = true;
					e.forme = NkFormeParticule2D::NK_TRAIT;
					e.maxParticules = 96u;
					break;
				case NkPresetEffet2D::NK_PLUIE:
					e.debit = 260.f;
					e.vieMin = 1.1f;
					e.vieMax = 1.3f;
					e.vitesseMin = 9.f;
					e.vitesseMax = 11.f;
					e.direction = -PI * 0.5f - 0.08f;
					e.dispersion = 0.03f;
					e.gravite = NkVec2f(0.f, -2.f);
					e.tailleDebut = 0.035f;
					e.tailleFin = 0.035f;
					e.couleurDebut = 0xAAC4FF90u;
					e.couleurMilieu = 0xAAC4FF90u;
					e.couleurFin = 0xAAC4FF40u;
					e.forme = NkFormeParticule2D::NK_TRAIT;
					e.zone = NkZoneEmission2D::NK_LIGNE;
					e.largeurZone = 24.f;
					e.maxParticules = 600u;
					break;
				case NkPresetEffet2D::NK_NEIGE:
					e.debit = 45.f;
					e.vieMin = 5.f;
					e.vieMax = 7.f;
					e.vitesseMin = 0.4f;
					e.vitesseMax = 0.9f;
					e.direction = -PI * 0.5f;
					e.dispersion = 0.45f;
					e.gravite = NkVec2f(0.1f, -0.15f);
					e.frein = 0.4f;
					e.tailleDebut = 0.1f;
					e.tailleFin = 0.08f;
					e.couleurDebut = 0xFFFFFFE0u;
					e.couleurMilieu = 0xF4F8FFD0u;
					e.couleurFin = 0xF4F8FF00u;
					e.zone = NkZoneEmission2D::NK_LIGNE;
					e.largeurZone = 24.f;
					e.maxParticules = 400u;
					break;
				case NkPresetEffet2D::NK_EXPLOSION:
					e.boucle = false;
					e.duree = 0.12f;
					e.rafale = 140u;
					e.debit = 0.f;
					e.vieMin = 0.35f;
					e.vieMax = 1.1f;
					e.vitesseMin = 1.5f;
					e.vitesseMax = 7.5f;
					e.direction = PI * 0.5f;
					e.dispersion = PI;
					e.gravite = NkVec2f(0.f, -2.f);
					e.frein = 2.4f;
					e.tailleDebut = 0.55f;
					e.tailleFin = 0.12f;
					e.couleurDebut = 0xFFF8D0FFu;
					e.couleurMilieu = 0xFF8024D0u;
					e.couleurFin = 0x50403800u;
					e.additif = true;
					e.maxParticules = 200u;
					e.eclaire = true;
					e.couleurLumiere = 0xFFB060FFu;
					e.intensiteLumiere = 2.5f;
					e.porteeLumiere = 8.f;
					break;
				default:
					break;
			}
			return e;
		}

		const char *NkNomPresetEffet2D(NkPresetEffet2D preset) noexcept {
			switch (preset) {
				case NkPresetEffet2D::NK_FEU:        return "Feu";
				case NkPresetEffet2D::NK_FUMEE:      return "Fumée";
				case NkPresetEffet2D::NK_ETINCELLES: return "Étincelles";
				case NkPresetEffet2D::NK_PLUIE:      return "Pluie";
				case NkPresetEffet2D::NK_NEIGE:      return "Neige";
				case NkPresetEffet2D::NK_EXPLOSION:  return "Explosion";
				default:                             return "Personnalisé";
			}
		}

	} // namespace unkeny
} // namespace nkentseu
