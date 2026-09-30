// =============================================================================
// NkUnkenyBancAnimateur.cpp — le banc de NkAnimateur2D (la HFSM de NKAnima
// dans Unkeny). Lance par NkUnkenyLancerBanc, compte avec lui.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (n1)  un heros « plateforme » a l'arret : Sol/idle, clip 0 ; ses quatre
//         parametres sont la, avec leurs defauts (auSol vrai)
//   (n2)  vitesse 2 : Sol/marche, clip 1 ; un SECOND heros du meme modele,
//         immobile, reste en idle (un modele, deux etats)
//   (n3)  saut + plus au sol + montee : Air/saut, clip 2, et le declencheur
//         revient a 0 dans le composant ; le clip UNE_FOIS termine n'est PAS
//         relance tant qu'on reste en saut
//   (n4)  vitesse verticale negative : Air/chute, clip 3
//   (n5)  au sol en courant : Sol/marche DIRECTEMENT (priorite), clip 1 ; puis
//         a l'arret : idle, clip 0
//   (n6)  la photo (Jouer / Arreter) garde l'etat ET les parametres
//   (n7)  .nkscene : l'animateur fait l'aller-retour ; une scene SANS
//         animateur n'ecrit pas la cle (l'ajout ne change pas les fichiers
//         des autres) ; un fichier d'AVANT (sans la cle, avec un composant
//         « jeu » nomme) se relit, composant compris
//   (n8)  un modele inconnu : rien ne casse, le sprite n'est pas touche, et
//         l'animateur le sait (modeleAbsent)
//   (n9)  un modele relu d'un .nkanim v3 se comporte comme l'original
//   (n10) le modele est PARTAGE : un parametre qu'une entite n'a pas ne
//         fuit pas de la precedente (le « saut » non consomme de l'une ne
//         fait pas sauter l'autre)
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "NKFileSystem/NkFile.h"
#include "Unkeny/Anim/NkUnkenyAnimateur.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			using FnTemoin = void (*)(bool, const char *, float32);

			/// Quatre clips de deux images : idle, marche, saut (UNE_FOIS), chute.
			NkAnimSprite2D Clips() {
				NkAnimSprite2D a;
				a.colonnes = 4;
				a.lignes = 2;
				a.nbClips = 4;
				for (int32 i = 0; i < 4; ++i) {
					a.clips[i].premiere = static_cast<uint16>(i * 2);
					a.clips[i].nombre = 2;
					a.clips[i].imagesParSeconde = 8.f;
				}
				a.clips[2].mode = NkModeLecture::NK_UNE_FOIS;
				return a;
			}

			ecs::NkEntityId Heros(NkScene &s, const char *nom, const char *modele) {
				const ecs::NkEntityId e = s.Creer(nom, NkVec2f(0.f, 0.f));
				s.Monde().Add<NkSprite2D>(e, NkSprite2D());
				s.Monde().Add<NkAnimSprite2D>(e, Clips());
				s.Monde().Add<NkAnimateur2D>(e, NkCreerAnimateur2D(modele));
				return e;
			}

			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId trouve = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strcmp(e.nom, nom) == 0) {
						trouve = id;
					}
				});
				return trouve;
			}

			bool Est(NkScene &s, ecs::NkEntityId e, const char *chemin, int32 clip) {
				const NkAnimateur2D *a = s.Monde().Get<NkAnimateur2D>(e);
				const NkAnimSprite2D *sp = s.Monde().Get<NkAnimSprite2D>(e);
				return a != nullptr && sp != nullptr && NkEtatAnimateur2D(*a) == chemin && sp->clipCourant == clip;
			}

			float32 Clip(NkScene &s, ecs::NkEntityId e) {
				const NkAnimSprite2D *sp = s.Monde().Get<NkAnimSprite2D>(e);
				return sp != nullptr ? static_cast<float32>(sp->clipCourant) : -1.f;
			}
		} // namespace

		void NkUnkenyBancAnimateur(FnTemoin Temoin) {
			const float32 dt = 1.f / 60.f;
			NkScene s;
			NkSceneConfig cfg;
			s.Init(cfg);
			const ecs::NkEntityId h = Heros(s, "Heros", "plateforme");
			const ecs::NkEntityId g = Heros(s, "Garde", "plateforme");
			auto A = [&](ecs::NkEntityId e) {
				return s.Monde().Get<NkAnimateur2D>(e);
			};

			// (n1)
			s.Pas(dt);
			Temoin(Est(s, h, "Sol/idle", 0) && A(h)->nbParams == 4 && A(h)->Valeur("auSol") == 1.f,
				   "(n1) a l'arret : Sol/idle, clip 0, 4 parametres (auSol vrai)", Clip(s, h));

			// (n2)
			A(h)->Poser("vitesse", 2.f);
			s.Pas(dt);
			Temoin(Est(s, h, "Sol/marche", 1) && Est(s, g, "Sol/idle", 0),
				   "(n2) vitesse 2 : Sol/marche, clip 1 ; le garde reste en idle", Clip(s, h));

			// (n3)
			A(h)->Declencher("saut");
			A(h)->PoserBool("auSol", false);
			A(h)->Poser("vitesseVerticale", 5.f);
			s.Pas(dt);
			Temoin(Est(s, h, "Air/saut", 2) && A(h)->Valeur("saut") == 0.f,
				   "(n3) saut : Air/saut, clip 2, declencheur consomme (0)", Clip(s, h));
			for (int32 k = 0; k < 40; ++k) {
				s.Pas(dt);
			}
			const NkAnimSprite2D *sp = s.Monde().Get<NkAnimSprite2D>(h);
			Temoin(Est(s, h, "Air/saut", 2) && sp->termine && sp->ImageCourante() == 5,
				   "(n3) clip de saut UNE_FOIS termine, PAS relance (image 5)", static_cast<float32>(sp->ImageCourante()));

			// (n4)
			A(h)->Poser("vitesseVerticale", -1.f);
			s.Pas(dt);
			Temoin(Est(s, h, "Air/chute", 3), "(n4) vy < 0 : Air/chute, clip 3", Clip(s, h));

			// (n6) la photo, prise en l'air
			NkScene::NkPhoto photo;
			s.Photographier(photo);

			// (n5)
			A(h)->PoserBool("auSol", true);
			A(h)->Poser("vitesseVerticale", 0.f);
			s.Pas(dt);
			const bool atterri = Est(s, h, "Sol/marche", 1);
			A(h)->Poser("vitesse", 0.f);
			s.Pas(dt);
			Temoin(atterri && Est(s, h, "Sol/idle", 0),
				   "(n5) atterrir en courant : Sol/marche direct ; arret : idle", Clip(s, h));

			// (n6)
			s.Restaurer(photo);
			const ecs::NkEntityId hr = ParNom(s, "Heros");
			const NkAnimateur2D *ar = hr.IsValid() ? s.Monde().Get<NkAnimateur2D>(hr) : nullptr;
			Temoin(ar != nullptr && Est(s, hr, "Air/chute", 3) && ar->Valeur("vitesseVerticale") == -1.f &&
					   ar->Valeur("vitesse") == 2.f,
				   "(n6) la photo garde l'etat (Air/chute) et les parametres", hr.IsValid() ? Clip(s, hr) : -1.f);
			// Et la machine REPREND de la : au sol, en courant -> marche.
			if (ar != nullptr) {
				s.Monde().Get<NkAnimateur2D>(hr)->PoserBool("auSol", true);
				s.Monde().Get<NkAnimateur2D>(hr)->Poser("vitesseVerticale", 0.f);
			}
			s.Pas(dt);
			Temoin(hr.IsValid() && Est(s, hr, "Sol/marche", 1), "(n6) apres Restaurer, la machine reprend (marche)",
				   hr.IsValid() ? Clip(s, hr) : -1.f);

			// (n7) .nkscene
			{
				NkString json;
				const bool ecrit = NkSauverSceneJSON(s, json);
				// Pour VOIR l'animateur dans l'editeur sans souris : la scene de ce
				// temoin (Heros en Sol/marche, Garde en idle), ecrite A LA DEMANDE,
				// puis `UnkenyEditor --scene=<fichier> --selection=Heros`.
				if (const char *sortie = std::getenv("NK_UNKENY_BANC_SCENE")) {
					NkFile::WriteAllText(sortie, json.CStr());
				}
				NkScene b;
				NkString err;
				const bool lu = ecrit && NkChargerSceneJSON(b, json.View(), nullptr, &err);
				const ecs::NkEntityId hb = lu ? ParNom(b, "Heros") : ecs::NkEntityId::Invalid();
				const NkAnimateur2D *ab = hb.IsValid() ? b.Monde().Get<NkAnimateur2D>(hb) : nullptr;
				Temoin(ab != nullptr && Est(b, hb, "Sol/marche", 1) && ab->Valeur("vitesse") == 2.f &&
						   json.Contains("NkAnimateur2D"),
					   "(n7) .nkscene : l'animateur fait l'aller-retour (etat, parametres)",
					   hb.IsValid() ? Clip(b, hb) : -1.f);

				NkScene nu;
				nu.Init(cfg);
				nu.Creer("Caisse", NkVec2f(1.f, 2.f));
				NkString jsonNu;
				NkSauverSceneJSON(nu, jsonNu);
				Temoin(!jsonNu.Contains("NkAnimateur2D"), "(n7) une scene SANS animateur n'ecrit pas la cle", 0.f);

				// Un fichier d'AVANT le 2026-09-29, tel qu'il s'ecrivait : pas de
				// cle NkAnimateur2D, et un composant « jeu » (NkVitesse2D =
				// 1,5 / 0 / 0,25, en octets). L'animateur a pris place dans la
				// liste des copieurs AVANT lui : c'est le NOM qui doit le retrouver.
				const char *ancien =
					"{\"format\": \"unkeny.scene\", \"version\": 1,"
					" \"config\": {\"physique\": false, \"particules\": false, \"gravite\": \"0 -9.81\","
					" \"pasFixe\": 0.0166666675, \"pasMaxParTrame\": 5},"
					" \"camera\": \"0 0 32 0\","
					" \"entites\": [{\"nom\": \"Ancien\", \"transform\": \"1 2 0 1 1\","
					" \"sprite\": {\"taille\": \"1 1\", \"pivot\": \"0.5 0.5\", \"couleur\": 4294967295,"
					" \"uv\": \"0 0 1 1\", \"couche\": 0, \"visible\": true},"
					" \"jeu\": {\"NkVitesse2D\": \"0000c03f000000000000803e\"}}]}";
				NkScene c;
				const bool luAncien = NkChargerSceneJSON(c, ancien, nullptr, &err);
				const ecs::NkEntityId ec = luAncien ? ParNom(c, "Ancien") : ecs::NkEntityId::Invalid();
				const NkVitesse2D *v = ec.IsValid() ? c.Monde().Get<NkVitesse2D>(ec) : nullptr;
				NkVector<ecs::NkEntityId> ids;
				c.Entites(ids);
				c.Pas(dt);
				Temoin(luAncien && ids.Size() == 1u && v != nullptr && v->lineaire.x == 1.5f && v->angulaire == 0.25f &&
						   !c.Monde().Has<NkAnimateur2D>(ec) && c.Monde().Has<NkSprite2D>(ec),
					   "(n7) un .nkscene d'AVANT se relit : entite, sprite, composant nomme", v != nullptr ? v->lineaire.x : -1.f);
				if (!luAncien) {
					std::printf("    erreur : %s\n", err.CStr());
				}
			}

			// (n8) modele inconnu
			{
				NkScene u;
				u.Init(cfg);
				const ecs::NkEntityId e = Heros(u, "Perdu", "modele-qui-n-existe-pas");
				u.Monde().Get<NkAnimSprite2D>(e)->Jouer(3);
				u.Pas(dt);
				u.Pas(dt);
				const NkAnimateur2D *a = u.Monde().Get<NkAnimateur2D>(e);
				Temoin(a->modeleAbsent && a->execution.current == -1 && u.Monde().Get<NkAnimSprite2D>(e)->clipCourant == 3,
					   "(n8) modele inconnu : rien ne casse, le sprite garde son clip", Clip(u, e));
			}

			// (n9) le modele relu d'un .nkanim v3
			{
				const char *chemin = "unkeny_banc_plateforme.nkanim";
				const bool ecrit = NkModeleAnimateur("plateforme") != nullptr &&
								   NkModeleAnimateur("plateforme")->SaveBinary(NkString(chemin));
				const bool lu = ecrit && NkChargerModeleAnimateur("plateforme.fichier", chemin);
				std::remove(chemin);
				NkScene f;
				f.Init(cfg);
				const ecs::NkEntityId e = Heros(f, "Relu", "plateforme.fichier");
				f.Pas(dt);
				const bool idle = Est(f, e, "Sol/idle", 0);
				f.Monde().Get<NkAnimateur2D>(e)->Poser("vitesse", 1.f);
				f.Pas(dt);
				f.Monde().Get<NkAnimateur2D>(e)->Declencher("saut");
				f.Pas(dt);
				Temoin(lu && idle && Est(f, e, "Air/saut", 2) && f.Monde().Get<NkAnimateur2D>(e)->nbParams == 4,
					   "(n9) modele relu d'un .nkanim v3 : idle, puis saut", Clip(f, e));
			}

			// (n10) pas de fuite de parametres entre entites du meme modele
			{
				NkScene q;
				q.Init(cfg);
				const ecs::NkEntityId ea = Heros(q, "Sauteur", "plateforme");
				const ecs::NkEntityId eb = q.Creer("Sobre", NkVec2f(2.f, 0.f));
				q.Monde().Add<NkSprite2D>(eb, NkSprite2D());
				q.Monde().Add<NkAnimSprite2D>(eb, Clips());
				// Un animateur qui ne porte QUE « vitesse » : ni saut, ni auSol.
				NkAnimateur2D sobre = NkCreerAnimateur2D("plateforme");
				sobre.nbParams = 0;
				sobre.Poser("vitesse", 0.f);
				q.Monde().Add<NkAnimateur2D>(eb, sobre);
				NkAnimateur2D *pa = q.Monde().Get<NkAnimateur2D>(ea);
				pa->Declencher("saut");
				pa->PoserBool("auSol", false);
				pa->Poser("vitesseVerticale", 5.f);
				q.Pas(dt);
				// En l'air, un second « saut » n'est consomme par rien : il reste pose.
				q.Monde().Get<NkAnimateur2D>(ea)->Declencher("saut");
				q.Pas(dt);
				q.Pas(dt);
				Temoin(Est(q, ea, "Air/saut", 2) && q.Monde().Get<NkAnimateur2D>(ea)->Valeur("saut") == 1.f &&
						   Est(q, eb, "Sol/idle", 0),
					   "(n10) modele partage : le saut de l'un ne fait pas sauter l'autre", Clip(q, eb));
			}
		}

	} // namespace unkeny
} // namespace nkentseu
