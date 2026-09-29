// AUTEUR : Rihen
// =============================================================================
// test_particules2d_jeu.cpp — un corps mou comme PERSONNAGE (2026-09-29).
//
// Ce que NkParticules2DJeu.cpp ajoute, eprouve sans fenetre. Chaque temoin qui
// peut etre vert par accident a sa contre-epreuve (n), qui doit rougir s'il ne
// mesure rien.
//
// PRE-ENREGISTREMENT — ecrit AVANT d'avoir lu un chiffre :
//   (m1)  marcher par AjouterVitesse (acceleration bornee vers 2 m/s) : apres
//         0,5 s, vx a 5 % pres ET la gravite agit (vy < -3 m/s) ;
//         (m1n) AppliquerVitesse(2, 0) a chaque pas : vy reste > -0,5 m/s — le
//         defaut qui justifie la brique
//   (m2)  pousser ne fige pas la forme : un blob pousse en tombant sur le sol
//         voit son rayon de giration varier de plus de 5 % (il s'ecrase) ;
//         (m2n) AppliquerVitesse a chaque pas : moins de 2 %
//   (m3)  au sol, TENU SUR LE PAS : un blob pose sur une caisse statique touche,
//         normale y > 0,9, nature STATIQUE, rigide = la caisse ; (m3b) en chute
//         libre : rien ; (m3c) contre un mur seul (sans gravite) : |nx| > 0,9 et,
//         filtre a 0,7 (pente), il ne « porte » pas ; (m3d) sur une caisse
//         DYNAMIQUE : nature dynamique ; sur un autre blob : nature MOU, autre =
//         son id ; sur la boite du monde : nature LIMITES ;
//         (m3n) le drapeau historique `contact`, lui, est faux apres Pas
//   (m4)  evenements : blob pose sur une caisse : UN debut ; souleve : UNE fin ;
//         une zone traversee : DEBUT puis FIN ; deux blobs qui se touchent : un
//         DEBUT corps mou ; (m4n) une zone a cote : aucun evenement de zone
//   (m5)  attache : blob attache sous une caisse qui tombe, ecart <= 1 cm ;
//         un bras mou attache au flanc d'un tronc rigide pose tient (ecart
//         <= 5 % de sa taille) ; (m5n) sans attache, le bras tombe (> 0,3 m) ;
//         (m5b) sans gravite, la caisse lancee entraine le blob, quantite de
//         mouvement a 10 % ; (m5c) une attache a rupture 5 cm casse sous un choc
//         violent, (m5cn) incassable elle tient ; (m5d) RemapperRigides suit un
//         nouvel id
//   (m6)  deux pointeurs, deux blobs : chacun tire le sien (> 0,5 m), aucune
//         particule tenue deux fois ; saisie restreinte a UN corps la ou deux se
//         touchent : aucune particule de l'autre ; (m6n) la saisie historique,
//         au meme point, prend les deux ; (m6b) la saisie historique marche
//         toujours
//   (m7)  parties nommees d'une gelee : pousser le HAUT l'etire (le haut monte
//         de plus de 5 cm par rapport au bas) ; le BAS touche le sol, pas le
//         haut ; la partie survit a la suppression d'un AUTRE corps (meme
//         centre a 1e-5) ; TrouverPartie par le nom ; un nom en double refuse
//   (m8)  RelierCorps : deux gelees reliees, lachees, restent ensemble (liens
//         croises a moins de 15 % d'allongement) ; la compaction (un corps
//         supprime AVANT elles) ne deplace aucun lien ; les liens de grille de
//         la gelee restent contigus ; (m8n) sans lien, poussees, elles se
//         separent (> 0,5 m)
//
// CE QUE LA PREMIERE MESURE A CHANGE (le detail est au temoin, dans le code) :
//   (m2)  le blob tombe mesurait la CHUTE, puis le blob etire (fluide, XSPH) et la
//         gelee (quasi critique) ne gardaient aucun mouvement interne : le temoin
//         juge l'energie cinetique INTERNE des 5 premiers pas d'un BALLON etire
//         (seuils inchanges : > 5 % en ajoutant, < 1 % en remplacant)
//   (m3c) le blob pousse contre le mur REBONDIT : on juge la derniere seconde
//   (m4)  le blob lache REBONDIT deux fois (3 debuts) : debuts = fins + 1, puis
//         aucun evenement une fois pose, et UNE fin au levage
//   (m5)  sous la caisse, sans couplage, le blob traversait le SOL : il pend au
//         flanc d'une caisse de 10 kg ; `ecart` = ce qui reste apres correction
//         (`tension` = avant, imprimee : 3,5 cm au choc)
//   (m7)  ni la gelee (raide en traction) ni le blob (visqueux) ne s'etirent par
//         parties : la poussee par partie se juge sur une CORDE (une poussee sur
//         la queue fait partir la queue, pas la tete)
// =============================================================================
#include "NKPhysics/NkParticules2D.h"
#include "NKPhysics/NkParticules2DFabrique.h"
#include "NKPhysics/NkPhysicsWorld.h"
#include "NKMath/NkFunctions.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::physics;

namespace {
	int *gPass = nullptr;
	int *gFail = nullptr;

	void Temoin(bool ok, const char *quoi, float32 valeur) {
		std::fprintf(stderr, "  [%s] %-70s %9.4f\n", ok ? "ok" : "FAIL", quoi, static_cast<double>(valeur));
		if (ok) {
			++*gPass;
		} else {
			++*gFail;
		}
	}

	float32 Absf(float32 v) {
		return v < 0.f ? -v : v;
	}

	float32 Len(const NkVec2f &v) {
		return math::NkSqrt(v.x * v.x + v.y * v.y);
	}

	NkBodyId Boite(NkPhysicsWorld &w, const NkVec2f &c, const NkVec2f &demi, NkBodyType type = NkBodyType::STATIC,
				   uint32 drapeaux = 0u) {
		NkBodyDef d;
		d.type = type;
		d.flags = drapeaux;
		d.position = NkVec3f(c.x, c.y, 0.f);
		return w.CreateBody(d, collision::NkShape::Box2D(c, demi));
	}

	void Pas(NkParticules2D &p, NkPhysicsWorld *w) {
		p.Pas(1.f / 60.f, w);
		if (w != nullptr) {
			w->Step(1.f / 60.f);
		}
	}

	/// Un monde de jeu : pas de boite implicite, le sol est un corps.
	void MondeDeJeu(NkParticules2D &p) {
		p.reglages.limites.actif = false;
	}

	float32 RayonGiration(const NkParticules2D &p, uint32 ci) {
		const NkCorpsP2D &c = p.corps[ci];
		const NkVec2f g = p.CentreCorps(ci);
		float32 s = 0.f;
		for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
			const NkVec2f d = p.particules[i].pos - g;
			s += d.x * d.x + d.y * d.y;
		}
		return math::NkSqrt(s / static_cast<float32>(c.nombre > 0u ? c.nombre : 1u));
	}

	// (m1) (m2)
	void TemoinPousser() {
		for (int32 k = 0; k < 2; ++k) {
			const bool ajout = k == 0;
			NkParticules2D p;
			MondeDeJeu(p);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.f, 20.f), 0.4f, NkPresetP2D::NK_BLOB));
			for (int32 i = 0; i < 30; ++i) {
				if (ajout) {
					const float32 vx = p.VitesseCorps(ci).x;
					const float32 dv = math::NkClamp(2.f - vx, -20.f / 60.f, 20.f / 60.f);
					p.AjouterVitesse(ci, NkVec2f(dv, 0.f));
				} else {
					p.AppliquerVitesse(ci, NkVec2f(2.f, 0.f));
				}
				p.Pas(1.f / 60.f, nullptr);
			}
			const NkVec2f v = p.VitesseCorps(ci);
			if (ajout) {
				Temoin(Absf(v.x - 2.f) < 0.1f && v.y < -3.f, "(m1) AjouterVitesse : vx vise atteint ET il tombe (vy m/s)", v.y);
			} else {
				Temoin(v.y > -0.5f, "(m1n) AppliquerVitesse a chaque pas : la gravite est annulee (vy m/s)", v.y);
			}
		}
		// ⚠️ (m2) A ETE REECRIT TROIS FOIS ; les versions fausses, pour la trace :
		//  1. blob lache sur un sol, variation du rayon de giration : 1,4 % en
		//     ajoutant contre 17 % par AppliquerVitesse(1, -2) -- le second ECRASAIT
		//     le blob dans le sol ; le temoin mesurait la chute, pas la forme ;
		//  2. blob etire sans gravite : 0,01 % dans les deux cas -- un blob est un
		//     FLUIDE, sa viscosite XSPH egalise les vitesses en quelques sous-pas ;
		//  3. gelee molle etiree : 0,34 % -- les liens de ce solveur sont RAIDES
		//     (compliance 2e-5 s^2/kg au plus, soit ~5e4 N/m par lien) : une gelee
		//     oscille, mais de quelques millimetres, et le rayon de giration n'en
		//     voit presque rien.
		//  4. energie cinetique INTERNE (par rapport au centre de masse) moyennee
		//     entre 0,5 et 1 s : 0 dans les deux cas -- l'appariement de forme et
		//     les liens XPBD amortissent une gelee en moins d'une demi-seconde.
		// Ce qui distingue vraiment les deux, c'est le PREMIER instant : ajouter
		// garde l'elan d'etirement (la reponse du corps a ce qui lui arrive),
		// remplacer l'efface d'un coup. On mesure l'energie interne moyenne des
		// cinq premiers pas, sur un BALLON (un anneau sous pression, qui rebondit) :
		// mesuree sur la gelee, elle n'etait que de 0,5 % en ajoutant (0 en
		// remplacant) -- une gelee de ce solveur est presque critique, elle
		// n'oscille pas ; c'est une propriete du materiau, dite ici.
		auto EnergieInterne = [](const NkParticules2D &p, uint32 ci) {
			const NkCorpsP2D &c = p.corps[ci];
			const NkVec2f vm = p.VitesseCorps(ci);
			float32 e = 0.f;
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				const NkVec2f d = p.particules[i].vit - vm;
				e += 0.5f * p.particules[i].masse * (d.x * d.x + d.y * d.y);
			}
			return e;
		};
		for (int32 k = 0; k < 2; ++k) {
			const bool ajout = k == 0;
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			p.reglages.amortAir = 0.f;
			const uint32 ci = static_cast<uint32>(NkCreerBallonP2D(p, NkVec2f(0.f, 0.f)));
			const NkCorpsP2D &c = p.corps[ci];
			for (uint32 i = c.debut; i < c.debut + c.nombre; ++i) {
				p.particules[i].vit.x = p.particules[i].pos.x > 0.f ? 1.5f : -1.5f; // on l'etire
			}
			const float32 e0 = EnergieInterne(p, ci);
			float32 somme = 0.f;
			for (int32 i = 0; i < 60; ++i) {
				if (ajout) {
					const float32 vx = p.VitesseCorps(ci).x;
					p.AjouterVitesse(ci, NkVec2f(math::NkClamp(0.5f - vx, -0.3f, 0.3f), 0.f));
				} else {
					p.AppliquerVitesse(ci, NkVec2f(0.5f, 0.f));
				}
				p.Pas(1.f / 60.f, nullptr);
				if (i < 5) {
					somme += EnergieInterne(p, ci);
				}
			}
			const float32 part = somme / 5.f / (e0 > 0.f ? e0 : 1.f);
			if (ajout) {
				Temoin(part > 0.05f, "(m2) pousser en ajoutant : l'elan d'etirement est garde (E interne / E0)", part);
			} else {
				Temoin(part < 0.01f, "(m2n) AppliquerVitesse a chaque pas : il est efface (E interne / E0)", part);
			}
		}
	}

	// (m3)
	void TemoinSol() {
		{
			NkPhysicsWorld w;
			const NkBodyId caisse = Boite(w, NkVec2f(0.f, 0.5f), NkVec2f(1.5f, 0.5f));
			NkParticules2D p;
			MondeDeJeu(p);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.f, 1.6f), 0.35f, NkPresetP2D::NK_BLOB));
			for (int32 i = 0; i < 90; ++i) {
				Pas(p, &w);
			}
			const NkContactCorpsP2D c = p.ContactCorps(ci, 0.7f);
			Temoin(c.Touche() && c.normale.y > 0.9f && (c.natures & NK_P2D_CONTACT_STATIQUE) != 0u && c.statique > 0u && c.rigide == caisse,
				   "(m3) pose sur une caisse statique : touche, normale, nature, rigide (ny)", c.normale.y);
			bool drapeau = false;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				drapeau = drapeau || p.particules[i].contact;
			}
			Temoin(!drapeau, "(m3n) le drapeau historique `contact` est faux apres Pas (le defaut)", drapeau ? 1.f : 0.f);
			// En chute libre, loin de tout.
			p.Translater(ci, NkVec2f(0.f, 30.f));
			Pas(p, &w);
			Temoin(!p.ContactCorps(ci).Touche(), "(m3b) en chute libre : aucun contact", static_cast<float32>(p.ContactCorps(ci).particules));
		}
		{
			NkPhysicsConfig cfg;
			cfg.gravity = NkVec3f(0.f, 0.f, 0.f);
			NkPhysicsWorld w(cfg);
			Boite(w, NkVec2f(2.f, 0.f), NkVec2f(0.25f, 3.f)); // un mur a droite
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.f, 0.f), 0.35f, NkPresetP2D::NK_BLOB));
			// ⚠️ Mesure de la premiere version : pousse a 8 m/s^2, le blob REBONDIT
			// et oscille contre le mur (vx -0,7 / +0,9 m/s) ; au dernier pas il
			// n'y touchait pas. On juge donc la derniere seconde entiere : il y a
			// touche, TOUJOURS de face, et jamais « porte ».
			int32 pasTouches = 0;
			int32 pasPortes = 0;
			float32 nxMax = -2.f;
			for (int32 i = 0; i < 180; ++i) {
				p.AppliquerAcceleration(ci, NkVec2f(8.f, 0.f), 1.f / 60.f);
				Pas(p, &w);
				if (i >= 120) {
					const NkContactCorpsP2D tout = p.ContactCorps(ci);
					if (tout.Touche()) {
						++pasTouches;
						nxMax = math::NkMax(nxMax, tout.normale.x);
					}
					pasPortes += p.ContactCorps(ci, 0.7f).Touche() ? 1 : 0;
				}
			}
			Temoin(pasTouches > 0 && nxMax < -0.9f && pasPortes == 0, "(m3c) contre un mur : normale (-1, 0), ne porte pas (nx le pire)",
				   nxMax);
		}
		{
			NkPhysicsWorld w;
			Boite(w, NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
			const NkBodyId caisse = Boite(w, NkVec2f(-3.f, 0.4f), NkVec2f(0.6f, 0.4f), NkBodyType::DYNAMIC);
			NkParticules2D p;
			MondeDeJeu(p);
			const uint32 surCaisse = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(-3.f, 1.3f), 0.3f, NkPresetP2D::NK_BLOB));
			const uint32 dessous = static_cast<uint32>(NkCreerGeleeP2D(p, NkVec2f(3.f, 0.4f), 7, 5, 0.13f));
			const uint32 dessus = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(3.f, 1.5f), 0.25f, NkPresetP2D::NK_BLOB));
			for (int32 i = 0; i < 90; ++i) {
				Pas(p, &w);
			}
			const NkContactCorpsP2D c1 = p.ContactCorps(surCaisse, 0.7f);
			const NkContactCorpsP2D c2 = p.ContactCorps(dessus, 0.7f);
			Temoin(c1.dynamique > 0u && c1.rigide == caisse, "(m3d) sur une caisse DYNAMIQUE : nature dynamique, rigide",
				   static_cast<float32>(c1.dynamique));
			Temoin(c2.mou > 0u && c2.autreCorps == p.corps[dessous].id, "(m3d) sur une gelee : nature MOU, autre = son id",
				   static_cast<float32>(c2.mou));
			NkParticules2D q; // la boite du monde (limites actives par defaut)
			const uint32 cq = static_cast<uint32>(NkCreerBlobP2D(q, NkVec2f(0.f, 1.f), 0.3f, NkPresetP2D::NK_BLOB));
			for (int32 i = 0; i < 60; ++i) {
				q.Pas(1.f / 60.f, nullptr);
			}
			const NkContactCorpsP2D c3 = q.ContactCorps(cq, 0.7f);
			Temoin(c3.limites > 0u && (c3.natures & NK_P2D_CONTACT_LIMITES) != 0u && c3.normale.y > 0.9f,
				   "(m3d) sur la boite du monde : nature LIMITES", static_cast<float32>(c3.limites));
		}
	}

	int32 Compter(const NkVector<NkEvenementP2D> &v, uint32 corps, uint32 autre, NkGenreEvenementP2D g) {
		int32 n = 0;
		for (uint32 i = 0; i < v.Size(); ++i) {
			n += (v[i].corps == corps && v[i].autre == autre && v[i].genre == g) ? 1 : 0;
		}
		return n;
	}

	// (m4)
	void TemoinEvenements() {
		{
			NkPhysicsWorld w;
			const NkBodyId caisse = Boite(w, NkVec2f(0.f, 0.5f), NkVec2f(1.5f, 0.5f));
			NkParticules2D p;
			MondeDeJeu(p);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.f, 1.8f), 0.35f, NkPresetP2D::NK_BLOB));
			const uint32 id = p.corps[ci].id;
			// ⚠️ Premiere version : « UN debut, aucune fin » sur 2 s -- mesure, 3
			// debuts : le blob lache de 45 cm REBONDIT deux fois, et chaque
			// rebond est un vrai nouveau contact. Le temoin dit desormais ce qui
			// compte : autant de fins que de debuts moins un pendant qu'il se pose,
			// AUCUN evenement une fois pose (pas de clignotement d'un pas a
			// l'autre), la paire « en cours », et UNE fin quand on le souleve.
			int32 debuts = 0;
			int32 fins = 0;
			for (int32 i = 0; i < 120; ++i) {
				Pas(p, &w);
				debuts += Compter(p.ContactsDebut(), id, caisse, NkGenreEvenementP2D::NK_RIGIDE);
				fins += Compter(p.ContactsFin(), id, caisse, NkGenreEvenementP2D::NK_RIGIDE);
			}
			int32 pose = 0;
			for (int32 i = 0; i < 60; ++i) {
				Pas(p, &w);
				pose += Compter(p.ContactsDebut(), id, caisse, NkGenreEvenementP2D::NK_RIGIDE);
				pose += Compter(p.ContactsFin(), id, caisse, NkGenreEvenementP2D::NK_RIGIDE);
			}
			const bool enCours = Compter(p.ContactsEnCours(), id, caisse, NkGenreEvenementP2D::NK_RIGIDE) == 1;
			Temoin(debuts >= 1 && debuts == fins + 1 && pose == 0 && enCours,
				   "(m4) blob sur une caisse : debuts = fins + 1, puis RIEN une fois pose (debuts)", static_cast<float32>(debuts));
			int32 finsLevee = 0;
			p.Translater(ci, NkVec2f(0.f, 5.f));
			for (int32 i = 0; i < 5; ++i) {
				Pas(p, &w);
				finsLevee += Compter(p.ContactsFin(), id, caisse, NkGenreEvenementP2D::NK_RIGIDE);
			}
			Temoin(finsLevee == 1, "(m4) souleve : UNE fin", static_cast<float32>(finsLevee));
		}
		for (int32 k = 0; k < 2; ++k) {
			const bool traverse = k == 0;
			NkPhysicsWorld w;
			Boite(w, NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
			const NkBodyId zone = Boite(w, NkVec2f(traverse ? 0.f : 3.f, 2.f), NkVec2f(1.f, 0.3f), NkBodyType::STATIC, NK_BODY_TRIGGER);
			NkParticules2D p;
			MondeDeJeu(p);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.f, 4.f), 0.3f, NkPresetP2D::NK_BLOB));
			const uint32 id = p.corps[ci].id;
			int32 entre = -1;
			int32 sort = -1;
			int32 autres = 0;
			for (int32 i = 0; i < 150; ++i) {
				Pas(p, &w);
				if (Compter(p.ContactsDebut(), id, zone, NkGenreEvenementP2D::NK_ZONE) > 0 && entre < 0) {
					entre = i;
				}
				if (Compter(p.ContactsFin(), id, zone, NkGenreEvenementP2D::NK_ZONE) > 0 && sort < 0) {
					sort = i;
				}
				for (uint32 e = 0; e < p.ContactsDebut().Size(); ++e) {
					autres += p.ContactsDebut()[e].genre == NkGenreEvenementP2D::NK_ZONE ? 1 : 0;
				}
			}
			if (traverse) {
				Temoin(entre >= 0 && sort > entre && autres == 1, "(m4) zone traversee : DEBUT puis FIN (pas de l'entree)", static_cast<float32>(entre));
			} else {
				Temoin(autres == 0, "(m4n) zone a cote : aucun evenement de zone", static_cast<float32>(autres));
			}
		}
		{
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			const uint32 a = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(-1.f, 0.f), 0.3f, NkPresetP2D::NK_BLOB));
			const uint32 b = static_cast<uint32>(NkCreerGeleeP2D(p, NkVec2f(1.f, 0.f), 5, 5, 0.13f));
			p.AjouterVitesse(a, NkVec2f(2.f, 0.f));
			int32 debuts = 0;
			for (int32 i = 0; i < 90; ++i) {
				p.Pas(1.f / 60.f, nullptr);
				debuts += Compter(p.ContactsDebut(), p.corps[a].id, p.corps[b].id, NkGenreEvenementP2D::NK_CORPS_MOU);
			}
			Temoin(debuts >= 1, "(m4) deux corps mous se touchent : DEBUT corps mou", static_cast<float32>(debuts));
		}
	}

	// (m5)
	void TemoinAttaches() {
		// (m5) au flanc d'une caisse qui tombe PUIS atterrit (le choc compris).
		// ⚠️ Premiere version : blob SOUS la caisse, couplage rigide coupe --
		// ecart 1,08 m, parce que le blob sans couplage traversait le SOL et
		// arrachait les particules attachees une fois la caisse posee. Le
		// temoin mesurait le sol, pas l'attache. Ici le blob pend a cote, couple,
		// et la caisse pese 10 kg (celle de densite 1 pesait 0,36 kg face a ~15 kg
		// de blob : un attelage qu'aucun jeu ne pose).
		{
			NkPhysicsWorld w;
			Boite(w, NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
			const NkBodyId caisse = Boite(w, NkVec2f(0.f, 2.f), NkVec2f(0.5f, 0.3f), NkBodyType::DYNAMIC);
			w.GetBody(caisse)->invMass = 1.f / 10.f;
			w.GetBody(caisse)->invInertiaDiag = w.GetBody(caisse)->invInertiaDiag * (0.36f / 10.f);
			NkParticules2D p;
			MondeDeJeu(p);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.5f + 0.26f, 2.f), 0.2f, NkPresetP2D::NK_BLOB));
			const uint32 n = p.AttacherZone(ci, NkVec2f(0.6f, 2.f), 0.1f, w, caisse);
			// ⚠️ Deuxieme version : `ecart` etait l'ecart AVANT correction -- 3,5 cm
			// au choc de l'atterrissage, ce que le blob de 13 kg TIRE en un
			// sous-pas. Ce qui se voit, c'est l'ecart qui RESTE apres correction :
			// `ecart` le dit desormais, et `tension` garde l'autre. Les deux sont
			// imprimes ; le seuil de 1 cm porte sur ce qui se voit.
			float32 ecart = 0.f;
			float32 tension = 0.f;
			for (int32 i = 0; i < 120; ++i) {
				Pas(p, &w);
				for (uint32 k = 0; k < p.attaches.Size(); ++k) {
					ecart = math::NkMax(ecart, p.attaches[k].ecart);
					tension = math::NkMax(tension, p.attaches[k].tension);
				}
			}
			std::fprintf(stderr, "        (tension max avant correction : %.4f m)\n", static_cast<double>(tension));
			Temoin(n > 0u && ecart <= 0.01f && w.GetBody(caisse)->position.y < 0.4f,
				   "(m5) blob attache au flanc d'une caisse qui tombe et atterrit : ecart max (m)", ecart);
		}
		// (m5) (m5n) un bras mou au flanc d'un tronc rigide pose.
		for (int32 k = 0; k < 2; ++k) {
			const bool attache = k == 0;
			NkPhysicsWorld w;
			Boite(w, NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
			NkBodyDef d;
			d.position = NkVec3f(0.f, 0.8f, 0.f);
			d.flags = NK_BODY_FIXED_ROT;
			d.material.density = 30.f;
			const NkBodyId tronc = w.CreateBody(d, collision::NkShape::Box2D(NkVec2f(0.f, 0.8f), NkVec2f(0.3f, 0.8f)));
			NkParticules2D p;
			MondeDeJeu(p);
			// Un bras de 7 x 2 particules (0,78 x 0,13 m) le long du flanc droit, a
			// hauteur d'epaule.
			const uint32 bras = static_cast<uint32>(NkCreerGeleeP2D(p, NkVec2f(0.3f + 0.39f + 0.07f, 1.3f), 7, 2, 0.13f));
			p.corps[bras].couplageRigide = false;
			if (attache) {
				p.AttacherZone(bras, NkVec2f(0.37f, 1.3f), 0.1f, w, tronc);
			}
			const float32 y0 = p.CentreCorps(bras).y;
			float32 ecart = 0.f;
			for (int32 i = 0; i < 120; ++i) {
				Pas(p, &w);
				for (uint32 a = 0; a < p.attaches.Size(); ++a) {
					ecart = math::NkMax(ecart, p.attaches[a].ecart);
				}
			}
			const float32 chute = y0 - p.CentreCorps(bras).y;
			if (attache) {
				Temoin(p.attaches.Size() > 0u && ecart <= 0.05f * 0.78f && chute < 0.3f,
					   "(m5) bras mou attache au tronc : tient (ecart max m)", ecart);
			} else {
				Temoin(chute > 0.3f, "(m5n) sans attache, le bras tombe (m)", chute);
			}
		}
		// (m5b) quantite de mouvement.
		{
			NkPhysicsConfig cfg;
			cfg.gravity = NkVec3f(0.f, 0.f, 0.f);
			NkPhysicsWorld w(cfg);
			NkBodyDef d;
			d.position = NkVec3f(0.f, 0.f, 0.f);
			d.linearVelocity = NkVec3f(2.f, 0.f, 0.f);
			d.linearDamping = 0.f;
			const NkBodyId caisse = w.CreateBody(d, collision::NkShape::Box2D(NkVec2f(0.f, 0.f), NkVec2f(0.3f, 0.3f)));
			NkRigidBody *b = w.GetBody(caisse);
			b->invMass = 1.f / 5.f; // 5 kg
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			p.reglages.amortAir = 0.f;
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(-0.6f, 0.f), 0.25f, NkPresetP2D::NK_BLOB));
			p.corps[ci].couplageRigide = false;
			p.AttacherZone(ci, NkVec2f(-0.35f, 0.f), 0.1f, w, caisse);
			float32 mBlob = 0.f;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				mBlob += p.particules[i].masse;
			}
			const float32 qAvant = 5.f * 2.f;
			for (int32 i = 0; i < 90; ++i) {
				Pas(p, &w);
			}
			b = w.GetBody(caisse);
			const float32 qApres = (1.f / b->invMass) * b->linearVelocity.x + mBlob * p.VitesseCorps(ci).x;
			const float32 ecart = Absf(qApres - qAvant) / qAvant;
			const bool suit = p.VitesseCorps(ci).x > 0.2f;
			Temoin(ecart < 0.10f && suit, "(m5b) caisse lancee : le blob suit, quantite de mouvement (ecart relatif)", ecart);
		}
		// (m5c) (m5cn) rupture.
		for (int32 k = 0; k < 2; ++k) {
			const bool cassable = k == 0;
			NkPhysicsConfig cfg;
			cfg.gravity = NkVec3f(0.f, 0.f, 0.f);
			NkPhysicsWorld w(cfg);
			const NkBodyId caisse = Boite(w, NkVec2f(0.f, 0.f), NkVec2f(0.3f, 0.3f), NkBodyType::KINEMATIC);
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(-0.6f, 0.f), 0.25f, NkPresetP2D::NK_BLOB));
			p.corps[ci].couplageRigide = false;
			p.AttacherZone(ci, NkVec2f(-0.35f, 0.f), 0.1f, w, caisse, 0.5f, cassable ? 0.05f : 0.f);
			// Un choc : la caisse (cinematique, donc de masse infinie) part a 12 m/s.
			w.SetLinearVelocity(caisse, NkVec3f(12.f, 0.f, 0.f));
			uint32 ruptures = 0;
			for (int32 i = 0; i < 30; ++i) {
				Pas(p, &w);
				ruptures += p.stats.rupturesAttaches;
			}
			if (cassable) {
				Temoin(ruptures > 0u && p.AttachesActives() == 0u, "(m5c) rupture 5 cm : l'attache casse au choc (rompues)",
					   static_cast<float32>(ruptures));
			} else {
				Temoin(ruptures == 0u && p.AttachesActives() > 0u, "(m5cn) incassable : elle tient", static_cast<float32>(p.AttachesActives()));
			}
		}
		// (m5d) RemapperRigides
		{
			NkPhysicsWorld w;
			const NkBodyId a = Boite(w, NkVec2f(0.f, 0.f), NkVec2f(0.3f, 0.3f));
			NkParticules2D p;
			const uint32 ci = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.f, 0.5f), 0.2f, NkPresetP2D::NK_BLOB));
			p.AttacherZone(ci, NkVec2f(0.f, 0.3f), 0.2f, w, a);
			const NkBodyId anciens[2] = {a, 7u};
			const NkBodyId nouveaux[2] = {7u, 9u}; // un ancien 7 deviendrait 9 : pas de remplacement en cascade
			p.RemapperRigides(anciens, nouveaux, 2u);
			bool juste = p.attaches.Size() > 0u;
			for (uint32 k = 0; k < p.attaches.Size(); ++k) {
				juste = juste && p.attaches[k].rigide == 7u;
			}
			Temoin(juste, "(m5d) RemapperRigides : a -> 7, sans cascade vers 9", static_cast<float32>(p.attaches.Size()));
		}
	}

	// (m6)
	void TemoinSaisies() {
		{
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			const uint32 a = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(-1.f, 0.f), 0.3f, NkPresetP2D::NK_BLOB));
			const uint32 b = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(1.f, 0.f), 0.3f, NkPresetP2D::NK_BLOB));
			const NkVec2f a0 = p.CentreCorps(a);
			const NkVec2f b0 = p.CentreCorps(b);
			const uint32 na = p.SaisirPointeurDebut(1u, a0, 0.2f);
			const uint32 nb = p.SaisirPointeurDebut(2u, b0, 0.2f);
			for (int32 i = 0; i < 60; ++i) {
				p.SaisirPointeurVers(1u, a0 + NkVec2f(0.f, 1.f * static_cast<float32>(i + 1) / 60.f));
				p.SaisirPointeurVers(2u, b0 + NkVec2f(1.f * static_cast<float32>(i + 1) / 60.f, 0.f));
				p.Pas(1.f / 60.f, nullptr);
			}
			const NkVec2f da = p.CentreCorps(a) - a0;
			const NkVec2f db = p.CentreCorps(b) - b0;
			Temoin(na > 0u && nb > 0u && p.PointeursEnSaisie() == 2u && da.y > 0.5f && Absf(da.x) < 0.2f && db.x > 0.5f && Absf(db.y) < 0.2f,
				   "(m6) deux pointeurs, deux blobs : chacun tire le sien (m)", math::NkMin(da.y, db.x));
			p.SaisirPointeursFin();
			Temoin(p.PointeursEnSaisie() == 0u, "(m6) tout lacher : plus aucun pointeur", 0.f);
		}
		{
			NkParticules2D p;
			MondeDeJeu(p);
			p.reglages.gravite = NkVec2f(0.f, 0.f);
			const uint32 a = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(-0.3f, 0.f), 0.3f, NkPresetP2D::NK_BLOB));
			const uint32 b = static_cast<uint32>(NkCreerBlobP2D(p, NkVec2f(0.3f, 0.f), 0.3f, NkPresetP2D::NK_BLOB));
			// Au point de jonction, les deux corps sont dans le disque.
			const uint32 nA = p.SaisirPointeurDebut(7u, NkVec2f(0.f, 0.f), 0.25f, static_cast<int32>(a));
			uint32 deB = 0;
			uint32 deA = 0;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				if (p.particules[i].saisie) {
					deA += p.particules[i].corps == a ? 1u : 0u;
					deB += p.particules[i].corps == b ? 1u : 0u;
				}
			}
			Temoin(nA > 0u && deA == nA && deB == 0u, "(m6) saisie restreinte au corps A : aucune particule de B", static_cast<float32>(deB));
			// Un second pointeur au meme endroit, tous corps : il ne vole rien a A.
			const uint32 n2 = p.SaisirPointeurDebut(8u, NkVec2f(0.f, 0.f), 0.25f);
			uint32 doubles = 0;
			uint32 tenues = 0;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				tenues += p.particules[i].saisie ? 1u : 0u;
			}
			doubles = (nA + n2) - tenues;
			Temoin(doubles == 0u && p.ParticulesSaisies(7u) == nA, "(m6) aucune particule tenue deux fois (doublons)", static_cast<float32>(doubles));
			p.SaisirPointeursFin();
			// (m6n) la saisie historique au meme point prend les deux corps.
			p.SaisirDebut(NkVec2f(0.f, 0.f), 0.25f);
			deA = 0;
			deB = 0;
			for (uint32 i = 0; i < p.particules.Size(); ++i) {
				if (p.particules[i].saisie) {
					deA += p.particules[i].corps == a ? 1u : 0u;
					deB += p.particules[i].corps == b ? 1u : 0u;
				}
			}
			Temoin(deA > 0u && deB > 0u, "(m6n) la saisie historique, au meme point, prend les deux", static_cast<float32>(deB));
			// (m6b) et elle tire toujours.
			const NkVec2f c0 = p.CentreCorps(a);
			for (int32 i = 0; i < 60; ++i) {
				p.SaisirVers(NkVec2f(0.f, 1.f * static_cast<float32>(i + 1) / 60.f));
				p.Pas(1.f / 60.f, nullptr);
			}
			const float32 monte = p.CentreCorps(a).y - c0.y;
			p.SaisirFin();
			Temoin(monte > 0.3f && !p.EnSaisie(), "(m6b) la saisie historique marche toujours (m)", monte);
		}
	}

	// (m7)
	void TemoinParties() {
		NkPhysicsWorld w;
		Boite(w, NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
		NkParticules2D p;
		MondeDeJeu(p);
		// Un corps AVANT la gelee : le supprimer decalera tous ses index.
		const int32 avant = NkCreerBlobP2D(p, NkVec2f(-5.f, 0.5f), 0.2f, NkPresetP2D::NK_BLOB);
		const uint32 g = static_cast<uint32>(NkCreerGeleeP2D(p, NkVec2f(0.f, 0.45f), 7, 5, 0.13f));
		// Rangees de la grille : la particule (x, y) est a l'index y * 7 + x.
		uint32 haut[7];
		uint32 bas[7];
		for (uint32 x = 0; x < 7u; ++x) {
			bas[x] = x;
			haut[x] = 4u * 7u + x;
		}
		const uint32 idHaut = p.CreerPartie(g, "haut", haut, 7u);
		const uint32 idBas = p.CreerPartie(g, "bas", bas, 7u);
		const uint32 doublon = p.CreerPartie(g, "haut", bas, 7u);
		Temoin(idHaut != 0u && idBas != 0u && doublon == 0u && p.TrouverPartie(p.corps[g].id, "bas") == idBas,
			   "(m7) parties nommees : creees, trouvees par le nom, doublon refuse", static_cast<float32>(p.parties.Size()));
		for (int32 i = 0; i < 60; ++i) {
			Pas(p, &w);
		}
		const NkContactCorpsP2D cBas = p.ContactPartie(idBas, 0.7f);
		const NkContactCorpsP2D cHaut = p.ContactPartie(idHaut, 0.7f);
		Temoin(cBas.Touche() && !cHaut.Touche(), "(m7) le BAS touche le sol, pas le haut (particules du bas)", static_cast<float32>(cBas.particules));
		// ⚠️ La poussee PAR PARTIE, trois versions dont deux fausses :
		//  1. tirer le haut d'une GELEE vers le haut : 0,4 mm d'allongement -- la
		//     grille est raide en traction, la gelee entiere decollait ;
		//  2. tirer la tete d'un BLOB en retenant ses pieds : -0,8 mm -- la
		//     viscosite XSPH etale la poussee sur tout le corps en un pas (un
		//     fluide ne se pilote pas par morceaux : propriete, pas defaut) ;
		//  3. la meme chose sur une gelee MOLLE : 0,4 mm -- les liens sont raides
		//     meme a raideur 0,2 (voir m2).
		// Ce qui se mesure sans dependre de la raideur : UNE poussee sur la QUEUE
		// d'une corde (sans gravite) met la queue en mouvement ce pas-la, pas la
		// tete ; contre-epreuve, la meme poussee sur le corps entier les met en
		// mouvement toutes les deux.
		for (int32 k = 0; k < 2; ++k) {
			const bool parPartie = k == 0;
			NkParticules2D q;
			MondeDeJeu(q);
			q.reglages.gravite = NkVec2f(0.f, 0.f);
			const uint32 b = static_cast<uint32>(NkCreerCordeP2D(q, NkVec2f(0.f, 1.f), 20, 0.09f));
			q.EpinglerCorps(b, false); // la corde de la fabrique est epinglee en haut
			const uint32 teteRel[4] = {0u, 1u, 2u, 3u};
			const uint32 queueRel[4] = {15u, 16u, 17u, 18u};
			const uint32 tete = q.CreerPartie(b, "tete", teteRel, 4u);
			const uint32 queue = q.CreerPartie(b, "queue", queueRel, 4u);
			if (parPartie) {
				q.AjouterVitessePartie(queue, NkVec2f(1.f, 0.f));
			} else {
				q.AjouterVitesse(b, NkVec2f(1.f, 0.f));
			}
			q.Pas(1.f / 60.f, nullptr);
			const float32 vq = q.VitessePartie(queue).x;
			const float32 vt = q.VitessePartie(tete).x;
			if (parPartie) {
				Temoin(tete != 0u && queue != 0u && vq > 0.5f && vt < 0.1f, "(m7) pousser la QUEUE d'une corde : elle part, pas la tete (vx queue)",
					   vq);
			} else {
				Temoin(vq > 0.5f && vt > 0.5f, "(m7n) la meme poussee sur le corps entier : les deux partent (vx tete)", vt);
			}
		}
		const NkVec2f cH = p.CentrePartie(idHaut);
		p.SupprimerCorps(static_cast<uint32>(avant));
		const NkVec2f cH2 = p.CentrePartie(idHaut);
		Temoin(Absf(cH.x - cH2.x) + Absf(cH.y - cH2.y) < 1.0e-5f && p.IndexPartie(idHaut) >= 0,
			   "(m7) la partie survit a la suppression d'un AUTRE corps (ecart m)", Absf(cH.x - cH2.x) + Absf(cH.y - cH2.y));
	}

	// (m8)
	void TemoinRelier() {
		for (int32 k = 0; k < 2; ++k) {
			const bool relie = k == 0;
			NkPhysicsWorld w;
			Boite(w, NkVec2f(0.f, -0.5f), NkVec2f(20.f, 0.5f));
			NkParticules2D p;
			MondeDeJeu(p);
			const int32 avant = NkCreerBlobP2D(p, NkVec2f(-6.f, 0.5f), 0.2f, NkPresetP2D::NK_BLOB);
			const uint32 a = static_cast<uint32>(NkCreerGeleeP2D(p, NkVec2f(-0.39f - 0.065f, 1.5f), 7, 5, 0.13f));
			const uint32 b = static_cast<uint32>(NkCreerGeleeP2D(p, NkVec2f(0.39f + 0.065f, 1.5f), 7, 5, 0.13f));
			const uint32 lienDebutA = p.corps[a].lienDebut;
			const uint32 lienNombreA = p.corps[a].lienNombre;
			const uint32 liensAvant = static_cast<uint32>(p.liens.Size());
			const uint32 n = relie ? p.RelierCorps(a, b, 0.15f) : 0u;
			// Les liens croises : ceux poses par RelierCorps (a la fin du tableau).
			auto etirementMax = [&]() {
				float32 e = 0.f;
				for (uint32 i = liensAvant; i < p.liens.Size(); ++i) {
					const NkLien2D &l = p.liens[i];
					if (l.casse) {
						continue;
					}
					const float32 d = Len(p.particules[l.b].pos - p.particules[l.a].pos);
					e = math::NkMax(e, Absf(d / l.reposInitial - 1.f));
				}
				return e;
			};
			float32 eMax = 0.f;
			for (int32 i = 0; i < 120; ++i) {
				Pas(p, &w);
				if (relie) {
					eMax = math::NkMax(eMax, etirementMax());
				}
			}
			if (relie) {
				const bool contigus = p.corps[a].lienDebut == lienDebutA && p.corps[a].lienNombre == lienNombreA;
				Temoin(n > 0u && eMax < 0.15f && contigus, "(m8) RelierCorps : deux gelees tiennent ensemble (allongement max)", eMax);
				// Compaction : on supprime le corps d'AVANT ; chaque lien croise
				// doit relier une particule de a a une particule de b.
				p.SupprimerCorps(static_cast<uint32>(avant));
				bool justes = true;
				// Le corps supprime etait l'index 0 : a et b reculent d'une case.
				const int32 ia = static_cast<int32>(a) - 1;
				const int32 ib = static_cast<int32>(b) - 1;
				uint32 croises = 0;
				for (uint32 i = 0; i < p.liens.Size(); ++i) {
					const NkLien2D &l = p.liens[i];
					if (l.genre != NkGenreLien2D::NK_LIAISON) {
						continue;
					}
					const int32 ca = static_cast<int32>(p.particules[l.a].corps);
					const int32 cb = static_cast<int32>(p.particules[l.b].corps);
					if (ca != cb) {
						++croises;
						justes = justes && ((ca == ia && cb == ib) || (ca == ib && cb == ia)) &&
								 Absf(Len(p.particules[l.b].pos - p.particules[l.a].pos) / l.reposInitial - 1.f) < 0.15f;
					}
				}
				Temoin(justes && croises == n, "(m8) apres compaction : chaque lien croise relie encore a et b (liens)", static_cast<float32>(croises));
			} else {
				// Sans lien : on pousse b vers la droite, elles se separent.
				for (int32 i = 0; i < 30; ++i) {
					p.AjouterVitesse(b, NkVec2f(0.2f, 0.f));
					Pas(p, &w);
				}
				const float32 ecart = Len(p.CentreCorps(b) - p.CentreCorps(a));
				Temoin(ecart > 0.91f + 0.5f, "(m8n) sans lien, poussees, elles se separent (distance des centres m)", ecart);
			}
		}
	}
} // namespace

int RunParticules2DJeuTests(int &pass, int &fail) {
	gPass = &pass;
	gFail = &fail;
	const int avant = fail;
	std::fprintf(stderr, "=== PARTICULES 2D, LE JEU : pousser, sol, evenements, attaches, saisies, parties ===\n");
	TemoinPousser();
	TemoinSol();
	TemoinEvenements();
	TemoinAttaches();
	TemoinSaisies();
	TemoinParties();
	TemoinRelier();
	std::fprintf(stderr, "=== particules 2D (jeu) : %d rouge(s) sur ce bloc ===\n", fail - avant);
	return fail - avant;
}
