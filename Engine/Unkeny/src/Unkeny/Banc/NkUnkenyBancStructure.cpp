//
// NkUnkenyBancStructure.cpp
// =============================================================================
// Description :
//   Le banc de la STRUCTURE de scene : hierarchie, identites stables, sauvegarde
//   portable (v2) et relecture de la v1, son par nom, prefabs. Lance par
//   NkUnkenyLancerBanc, apres ses propres temoins (dont il ne change aucun).
//
// Caracteristiques :
//   - PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//     (h1) un enfant suit son parent en translation, rotation et echelle
//     (h2) detacher garde la position monde (exacte) ; rattacher aussi ;
//          rattacher « sans garder » fait du transform le local
//     (h3) une boucle (le parent dans sa propre descendance) est refusee
//     (h4) un geste sur l'enfant (teleport) est garde, et l'enfant suit ensuite
//     (h5) detruire un parent laisse ses enfants a leur place ; la cascade
//          emporte toute la descendance ET ses corps physiques
//     (h6) un enfant a corps STATIQUE est porte (son corps suit) ; un enfant
//          DYNAMIQUE ne l'est pas
//     (h7) parents et locaux traversent Photographier/Restaurer et le fichier
//     (h8) un fichier RETOUCHE A LA MAIN (le parent deplace, pas ses enfants) :
//          a la relecture, l'enfant revient a (parent o local)
//     (i1) identites : distinctes, les memes apres Restaurer (les poignees non) ;
//          (i1n) une identite detruite ne resert jamais
//     (i2) les memes identites apres un aller-retour par fichier
//     (i3) une reference d'entite (NK_ENTITE) d'un composant du jeu DECRIT
//          designe la bonne entite apres Restaurer et apres le fichier ;
//          (i3n) la meme reference en OCTETS (non decrite) pointe dans le vide
//     (v1) un fichier VERSION 1, ecrit par le code d'avant et fige ici, se relit
//          a l'identique (valeurs et composants en octets), puis se reecrit en v2
//          et se relit encore a l'identique
//     (r1) un composant decrit relu apres l'AJOUT d'un champ : anciens champs
//          justes, nouveau a sa valeur par defaut ; (r1n) en octets : perdu
//     (r2) un composant decrit est ecrit en objet, pas en octets
//     (a7) un son garde son IDENTITE apres reouverture (ordre de chargement
//          different) ; (a7n) ecrit par son numero de session : le mauvais son
//     (p1) un prefab pose 20 fois, modifie : les 20 suivent (couleur, masse au
//          solveur, un enfant de plus), UNE surcharge est gardee et listee
//     (p2) .nkprefab : ecrit, relu ailleurs, instancie a l'identique ; le fichier
//          modifie et relu : les instances suivent
//     (p3) une instance sauvee dans un .nkscene retrouve son prefab PAR NOM
//     (f1) FUSION (30/09) : un enfant a corps, une gelee ATTACHEE a lui, deux
//          controleurs : parent, attache rebranchee sur le corps refait et
//          reglages traversent Photographier/Restaurer ET le fichier v2
//     (f2) le meme fichier tel que l'ecrivait la branche physique (v1, "id"
//          sous "corps", sans identite) : attaches et controleurs relus
//     (x1) NkAssetType::Scene <-> .nkscene et SaveGame <-> .nksave, dans les deux
//          sens ; les types d'avant ne bougent pas
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBanc.h"

#include "NKECS/Hierarchy/NkHierarchy.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "NKPhysics/NkParticules2DFabrique.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/NkArchive.h"
#include "Unkeny/Scene/NkUnkenyControles.h"
#include "Unkeny/Anim/NkUnkenySpriteAnim.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyPrefab.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Son/NkUnkenySon.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gEchecsS = 0;
			int32 gReussisS = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussisS : gEchecsS)++;
			}

			bool Pres(float32 a, float32 b, float32 tol = 1.0e-5f) {
				return math::NkAbs(a - b) <= tol;
			}
			bool PresV(const NkVec2f &a, const NkVec2f &b, float32 tol = 1.0e-5f) {
				return Pres(a.x, b.x, tol) && Pres(a.y, b.y, tol);
			}

			/// L'entite qui porte ce nom (NkEtiquette), ou Invalid.
			ecs::NkEntityId ParNom(NkScene &s, const char *nom) {
				ecs::NkEntityId trouve = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strcmp(e.nom, nom) == 0) {
						trouve = id;
					}
				});
				return trouve;
			}
			/// Le sprite de `id`, ou un sprite VIDE (jamais nul) : un temoin qui
			/// echoue doit ROUGIR, pas faire tomber le banc (mesure par la
			/// contre-epreuve M4, ou le prefab ne se relisait plus).
			NkSprite2D *SpriteDe(NkScene &s, ecs::NkEntityId id) {
				static NkSprite2D vide;
				NkSprite2D *x = s.Monde().IsAlive(id) ? s.Monde().Get<NkSprite2D>(id) : nullptr;
				if (x == nullptr) {
					vide = NkSprite2D();
					vide.couleur = 0u;
					return &vide;
				}
				return x;
			}
			NkCorps2D *CorpsDe(NkScene &s, ecs::NkEntityId id) {
				static NkCorps2D vide;
				NkCorps2D *x = s.Monde().IsAlive(id) ? s.Monde().Get<NkCorps2D>(id) : nullptr;
				if (x == nullptr) {
					vide = NkCorps2D();
					vide.masse = -1.f;
					return &vide;
				}
				return x;
			}
			const char *NomDe(NkScene &s, ecs::NkEntityId id) {
				const NkEtiquette *e = s.Monde().IsAlive(id) ? s.Monde().Get<NkEtiquette>(id) : nullptr;
				return e != nullptr ? e->nom : "";
			}

			// ---- Composants du jeu, pour les temoins ------------------------------
			/// Le meme que NkBancMarque de NkUnkenyBanc.cpp : c'est lui que le fichier
			/// v1 fige porte en octets, sous « banc.NkBancMarque ».
			struct NkBancMarqueV1 {
					uint32 code = 0;
					float32 valeur = 0.f;
			};
			/// Une reference d'entite, DECRITE.
			struct NkBancCible {
					ecs::NkEntityId cible;
					uint32 n = 0;
			};
			const NkChampSauve kChampsCible[] = {
				NK_UNKENY_CHAMP(NkBancCible, cible, NkTypeChamp::NK_ENTITE),
				NK_UNKENY_CHAMP(NkBancCible, n, NkTypeChamp::NK_U32),
			};
			/// La meme, en OCTETS (non decrite) : le comportement d'avant.
			struct NkBancCibleBrute {
					ecs::NkEntityId cible;
			};
			/// Deux versions d'un meme composant : la seconde a gagne un champ AU
			/// MILIEU (le cas qui decale tout en octets).
			struct NkBancVersion1 {
					uint32 a = 0;
					float32 b = 0.f;
			};
			struct NkBancVersion2 {
					uint32 a = 0;
					float32 nouveau = 7.f;
					float32 b = 0.f;
			};
			/// La meme evolution, SANS description : les octets d'avant.
			struct NkBancBrut1 {
					uint32 a = 0;
					float32 b = 0.f;
			};
			struct NkBancBrut2 {
					uint32 a = 0;
					float32 nouveau = 7.f;
					float32 b = 0.f;
			};
			const NkChampSauve kChampsV1[] = {
				NK_UNKENY_CHAMP(NkBancVersion1, a, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkBancVersion1, b, NkTypeChamp::NK_F32),
			};
			const NkChampSauve kChampsV2[] = {
				NK_UNKENY_CHAMP(NkBancVersion2, a, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkBancVersion2, nouveau, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkBancVersion2, b, NkTypeChamp::NK_F32),
			};

			// ---- LE FICHIER VERSION 1, ecrit par le code d'avant (07a5a050a) --------
			// Produit le 2026-09-29 par NkSauverSceneFichier AVANT tout changement de
			// cette branche (banc temporairement etendu, puis remis) : sol statique,
			// caisse dynamique texturee et animee avec un composant du jeu, radio avec
			// NkVitesse2D et NkSource2D (le son « b », identifiant 2). Les octets hex
			// sont ceux de clang-mingw x64. NE PAS LE RETOUCHER : c'est la reference.
			const char *const kSceneV1 = R"V1({
 "format": "unkeny.scene",
 "version": 1,
 "config": {
  "physique": true,
  "particules": false,
  "gravite": "0 -9.5",
  "pasFixe": 0.01666666753590107,
  "pasMaxParTrame": 5
 },
 "camera": "1.25 -0.5 40 0",
 "entites": [
  {
   "nom": "Sol",
   "transform": "0 -0.5 0 1 1",
   "collisionneur": {
    "forme": 1,
    "demi": "20 0.5",
    "rayon": 0.5,
    "decalage": "0 0",
    "couche": 2,
    "masque": 4294967295,
    "declencheur": false
   },
   "corps": {
    "type": 0,
    "masse": 1,
    "amortLineaire": 0,
    "amortAngulaire": 0.05000000074505806,
    "echelleGravite": 1,
    "rotationBloquee": false,
    "friction": 0.89999997615814209,
    "rebond": 0,
    "etat": "0 -0.5 0 0 0 0 1 0 0 0 0 0 0"
   }
  },
  {
   "nom": "Caisse",
   "transform": "-1.75000024 3.18819451 0.249999985 1 1",
   "sprite": {
    "taille": "2 2",
    "pivot": "0.5 0.5",
    "couleur": 4294967295,
    "uv": "0.25 0.5 0.5 1",
    "couche": 3,
    "visible": true,
    "texture": "quatre"
   },
   "collisionneur": {
    "forme": 1,
    "demi": "0.300000012 0.300000012",
    "rayon": 0.5,
    "decalage": "0 0",
    "couche": 1,
    "masque": 4294967295,
    "declencheur": false
   },
   "corps": {
    "type": 2,
    "masse": 12,
    "amortLineaire": 0,
    "amortAngulaire": 0.05000000074505806,
    "echelleGravite": 1,
    "rotationBloquee": false,
    "friction": 0.69999998807907104,
    "rebond": 0.20000000298023224,
    "etat": "-1.75000024 3.18819451 0 0 0 0.12467473 0.992197692 1.5 0.416666746 0 0 0 0"
   },
   "jeu": {
    "NkAnimSprite2D": "00000400000020410000ffff04000300000040410200020000000100000040410000ffff00000100000040410000ffff00000100000040410000ffff00000100000040410000ffff00000100000040410000ffff00000100000040410000ffff02010000f4eeee3e0000000004000200",
    "banc.NkBancMarque": "2a00000000002040"
   }
  },
  {
   "nom": "Radio",
   "transform": "4.08333206 1 0.25000003 1 1",
   "sprite": {
    "taille": "0.5 0.25",
    "pivot": "0.5 0.5",
    "couleur": 862362111,
    "uv": "0 0 1 1",
    "couche": 0,
    "visible": true
   },
   "jeu": {
    "NkVitesse2D": "0000003f000000000000c03f",
    "NkSource2D": "020000000000003f0000803f00004041010101000000000000000000000000000000803f00000000"
   }
  }
 ]
})V1";

			/// Les valeurs que le fichier v1 porte, verifiees UNE a UNE.
			bool VerifierSceneV1(NkScene &s, const NkTextures2D &tex, NkString &quoi) {
				const ecs::NkEntityId sol = ParNom(s, "Sol");
				const ecs::NkEntityId caisse = ParNom(s, "Caisse");
				const ecs::NkEntityId radio = ParNom(s, "Radio");
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				if (ids.Size() != 3u || !sol.IsValid() || !caisse.IsValid() || !radio.IsValid()) {
					quoi = "entites";
					return false;
				}
				const ecs::NkWorld &w = s.Monde();
				const NkTransform2D &ts = *w.Get<NkTransform2D>(sol);
				const NkTransform2D &tc = *w.Get<NkTransform2D>(caisse);
				const NkTransform2D &tr = *w.Get<NkTransform2D>(radio);
				if (!(ts.position.x == 0.f && ts.position.y == -0.5f && tc.position.x == -1.75000024f &&
					  tc.position.y == 3.18819451f && tc.rotation == 0.249999985f && tr.position.x == 4.08333206f &&
					  tr.rotation == 0.25000003f)) {
					quoi = "transforms";
					return false;
				}
				const NkCollisionneur2D *cs = w.Get<NkCollisionneur2D>(sol);
				const NkCorps2D *ks = w.Get<NkCorps2D>(sol);
				if (cs == nullptr || cs->demiTaille.x != 20.f || cs->couche != 2u || ks == nullptr ||
					ks->type != NkTypeCorps::NK_STATIQUE || ks->friction != 0.899999976f) {
					quoi = "sol";
					return false;
				}
				const NkSprite2D *sc = w.Get<NkSprite2D>(caisse);
				const NkCorps2D *kc = w.Get<NkCorps2D>(caisse);
				if (sc == nullptr || sc->couche != 3 || sc->texId != tex.Trouver("quatre") || sc->uv0.x != 0.25f ||
					kc == nullptr || kc->masse != 12.f || kc->rebond != 0.200000003f) {
					quoi = "caisse";
					return false;
				}
				const physics::NkRigidBody *b = const_cast<NkScene &>(s).MondePhysique()->GetBody(kc->corpsId);
				if (b == nullptr || b->position.x != -1.75000024f || b->linearVelocity.y != 0.416666746f ||
					b->invMass < 0.0833f || b->invMass > 0.0834f) {
					quoi = "etat du corps";
					return false;
				}
				const NkAnimSprite2D *an = w.Get<NkAnimSprite2D>(caisse);
				if (an == nullptr || an->nbClips != 2 || an->clips[0].nombre != 4 || an->clips[0].imagesParSeconde != 10.f ||
					an->clips[1].premiere != 4 || an->clips[1].nombre != 3 ||
					an->clips[1].mode != NkModeLecture::NK_ALLER_RETOUR || an->clips[1].imageEvent != 2 ||
					an->clipCourant != 1 || an->colonnes != 4 || an->lignes != 2 || !Pres(an->temps, 0.4666668f, 1.0e-6f)) {
					quoi = "animation (octets)";
					return false;
				}
				const NkBancMarqueV1 *m = w.Get<NkBancMarqueV1>(caisse);
				if (m == nullptr || m->code != 42u || m->valeur != 2.5f) {
					quoi = "composant du jeu (octets)";
					return false;
				}
				const NkVitesse2D *v = w.Get<NkVitesse2D>(radio);
				const NkSource2D *so = w.Get<NkSource2D>(radio);
				const NkSprite2D *sr = w.Get<NkSprite2D>(radio);
				if (v == nullptr || v->lineaire.x != 0.5f || v->angulaire != 1.5f || so == nullptr || so->son != 2u ||
					so->volume != 0.5f || so->portee != 12.f || !so->boucle || !so->auDemarrage || !so->spatial ||
					sr == nullptr || sr->couleur != 862362111u) {
					quoi = "radio (vitesse, source)";
					return false;
				}
				return true;
			}

			/// Un composant du jeu tel qu'un jeu le declare.
			void DeclarerBanc(NkScene &s) {
				s.PhotographierAussi<NkBancMarqueV1>("banc.NkBancMarque");
				s.PhotographierAussi<NkBancCible>("banc.Cible", kChampsCible, 2u);
				s.PhotographierAussi<NkBancCibleBrute>("banc.CibleBrute");
			}
		} // namespace

		int32 NkUnkenyLancerBancStructure() {
			gEchecsS = 0;
			gReussisS = 0;
			std::printf("\nUnkeny — banc de la STRUCTURE (hierarchie, identites, sauvegarde, prefabs)\n\n");
			const float32 quart = 1.57079632679f;

			// (h1) (h2) (h3) (h4)
			{
				NkScene s;
				NkSceneConfig cfg;
				s.Init(cfg);
				const ecs::NkEntityId p = s.Creer("P", NkVec2f(1.f, 2.f));
				const ecs::NkEntityId e = s.Creer("E", NkVec2f(3.f, 2.f));
				const bool lie = s.Rattacher(e, p, true);
				NkTransform2D *tp = s.Monde().Get<NkTransform2D>(p);
				tp->position = NkVec2f(5.f, 5.f);
				tp->rotation = quart;
				tp->echelle = NkVec2f(2.f, 2.f);
				s.PropagerHierarchie();
				const NkTransform2D te = *s.Monde().Get<NkTransform2D>(e);
				// local (2, 0) : x2, puis un quart de tour -> (0, 4), puis + (5, 5)
				Temoin(lie && PresV(te.position, NkVec2f(5.f, 9.f)) && Pres(te.rotation, quart) &&
						   PresV(te.echelle, NkVec2f(2.f, 2.f)) && s.Parent(e) == p,
					   "(h1) l'enfant suit son parent : translation, quart de tour, x2 (y)", te.position.y);

				const NkTransform2D avant = *s.Monde().Get<NkTransform2D>(e);
				s.Detacher(e, true);
				s.Monde().Get<NkTransform2D>(p)->position = NkVec2f(0.f, 0.f);
				s.PropagerHierarchie();
				const NkTransform2D apres = *s.Monde().Get<NkTransform2D>(e);
				const bool detacheExact = NkMemeTransform2D(avant, apres) && !s.Parent(e).IsValid() &&
										  s.Local(e) == nullptr && !s.Monde().Has<ecs::NkParent>(e);
				const bool rattacheGarde = s.Rattacher(e, p, true);
				const NkTransform2D apres2 = *s.Monde().Get<NkTransform2D>(e);
				const ecs::NkEntityId f = s.Creer("F", NkVec2f(1.f, 0.f));
				s.Rattacher(f, p, false); // (1, 0) DEVIENT le local : x2, quart de tour -> (0, 2)
				const NkTransform2D tf = *s.Monde().Get<NkTransform2D>(f);
				Temoin(detacheExact && rattacheGarde && PresV(apres2.position, apres.position) &&
						   PresV(tf.position, NkVec2f(0.f, 2.f)) && s.Local(f) != nullptr && PresV(s.Local(f)->position, NkVec2f(1.f, 0.f)),
					   "(h2) detacher et rattacher gardent le monde ; sans garder : local", tf.position.y);

				Temoin(!s.Rattacher(p, e, true) && !s.Rattacher(p, p, true) && !s.Parent(p).IsValid(),
					   "(h3) boucle et soi-meme refuses, rien ne change", 0.f);

				// (h4) le parent a (0,0), quart de tour, x2 ; l'enfant est teleporte.
				s.TeleporterEntite(e, NkVec2f(10.f, 10.f));
				s.PropagerHierarchie();
				const NkVec2f garde = s.Monde().Get<NkTransform2D>(e)->position;
				s.Monde().Get<NkTransform2D>(p)->position = NkVec2f(1.f, 0.f);
				s.PropagerHierarchie();
				const NkVec2f suit = s.Monde().Get<NkTransform2D>(e)->position;
				Temoin(garde.x == 10.f && garde.y == 10.f && PresV(suit, NkVec2f(11.f, 10.f), 1.0e-4f),
					   "(h4) geste sur l'enfant garde, puis l'enfant suit le parent (x)", suit.x);

				// (h5) detruire le parent : l'enfant reste, a sa place, racine.
				const NkVec2f place = s.Monde().Get<NkTransform2D>(e)->position;
				s.Detruire(p);
				s.PropagerHierarchie();
				Temoin(s.Monde().IsAlive(e) && s.Monde().IsAlive(f) && !s.Parent(e).IsValid() &&
						   s.Monde().Get<NkTransform2D>(e)->position.x == place.x && s.Local(e) == nullptr,
					   "(h5) parent detruit : ses enfants restent, a leur place, racines", place.x);
			}

			// (h5 bis) (h6) avec la physique
			{
				NkScene s;
				NkSceneConfig cfg;
				cfg.physique = true;
				s.Init(cfg);
				auto AvecCorps = [&](const char *nom, const NkVec2f &pos, NkTypeCorps type) {
					const ecs::NkEntityId id = s.Creer(nom, pos);
					NkCollisionneur2D c;
					c.demiTaille = NkVec2f(0.2f, 0.2f);
					s.Monde().Add<NkCollisionneur2D>(id, c);
					NkCorps2D k;
					k.type = type;
					s.AjouterCorps(id, k);
					return id;
				};
				const ecs::NkEntityId r = AvecCorps("R", NkVec2f(0.f, 0.f), NkTypeCorps::NK_STATIQUE);
				const ecs::NkEntityId c = AvecCorps("C", NkVec2f(2.f, 0.f), NkTypeCorps::NK_STATIQUE);
				const ecs::NkEntityId d = AvecCorps("D", NkVec2f(-2.f, 0.f), NkTypeCorps::NK_DYNAMIQUE);
				const ecs::NkEntityId g = s.Creer("G", NkVec2f(4.f, 0.f));
				s.Rattacher(c, r, true);
				s.Rattacher(d, r, true);
				s.Rattacher(g, c, true);
				s.Monde().Get<NkTransform2D>(r)->position = NkVec2f(0.f, 3.f);
				s.PropagerHierarchie();
				const physics::NkRigidBody *bc = s.MondePhysique()->GetBody(s.Monde().Get<NkCorps2D>(c)->corpsId);
				const NkVec2f pd = s.Monde().Get<NkTransform2D>(d)->position;
				const NkVec2f pg = s.Monde().Get<NkTransform2D>(g)->position;
				Temoin(bc != nullptr && bc->position.x == 2.f && bc->position.y == 3.f && pd.x == -2.f && pd.y == 0.f &&
						   PresV(pg, NkVec2f(4.f, 3.f)),
					   "(h6) corps STATIQUE porte (son corps suit), DYNAMIQUE non (y)", bc != nullptr ? bc->position.y : -1.f);

				const uint32 corpsC = s.Monde().Get<NkCorps2D>(c)->corpsId;
				s.DetruireAvecEnfants(r);
				Temoin(!s.Monde().IsAlive(r) && !s.Monde().IsAlive(c) && !s.Monde().IsAlive(d) && !s.Monde().IsAlive(g) &&
						   s.MondePhysique()->GetBody(corpsC) == nullptr,
					   "(h5) cascade : toute la descendance ET ses corps", 0.f);
			}

			// (h7) (i1) (i1n) (i2) (i3) (i3n) : photo et fichier
			{
				NkScene s;
				NkSceneConfig cfg;
				s.Init(cfg);
				DeclarerBanc(s);
				const ecs::NkEntityId a = s.Creer("A", NkVec2f(1.f, 1.f));
				const ecs::NkEntityId b = s.Creer("B", NkVec2f(2.f, 1.f));
				const ecs::NkEntityId c = s.Creer("C", NkVec2f(3.f, 1.f));
				const uint64 ua = s.Uid(a);
				const uint64 ub = s.Uid(b);
				const uint64 uc = s.Uid(c);
				s.Rattacher(c, b, true); // local (1, 0)
				s.Monde().Get<NkTransform2D>(b)->rotation = quart;
				s.PropagerHierarchie();
				NkBancCible cible;
				cible.cible = a;
				cible.n = 5;
				s.Monde().Add<NkBancCible>(b, cible);
				s.Monde().Add<NkBancCibleBrute>(b, NkBancCibleBrute{a});
				NkScene::NkPhoto photo;
				s.Photographier(photo);
				const NkVec2f mondeC = s.Monde().Get<NkTransform2D>(c)->position;
				s.Restaurer(photo);
				const ecs::NkEntityId a2 = s.EntiteParUid(ua);
				const ecs::NkEntityId b2 = s.EntiteParUid(ub);
				const ecs::NkEntityId c2 = s.EntiteParUid(uc);
				const bool poigneesChangent = !(a2 == a) || !(b2 == b) || !(c2 == c);
				Temoin(ua != 0u && ub != 0u && uc != 0u && ua != ub && ub != uc && std::strcmp(NomDe(s, a2), "A") == 0 &&
						   std::strcmp(NomDe(s, b2), "B") == 0 && std::strcmp(NomDe(s, c2), "C") == 0 && poigneesChangent,
					   "(i1) identites distinctes, les MEMES apres Restaurer (poignees neuves)", static_cast<float32>(ub));
				const NkTransform2D *lc = s.Local(c2);
				Temoin(s.Parent(c2) == b2 && lc != nullptr && PresV(lc->position, NkVec2f(1.f, 0.f)) &&
						   PresV(s.Monde().Get<NkTransform2D>(c2)->position, mondeC),
					   "(h7) parent et local traversent Photographier / Restaurer", lc != nullptr ? lc->position.x : -1.f);
				const NkBancCible *ci = s.Monde().Get<NkBancCible>(b2);
				const NkBancCibleBrute *cb = s.Monde().Get<NkBancCibleBrute>(b2);
				Temoin(ci != nullptr && ci->cible == a2 && ci->n == 5u, "(i3) reference d'entite DECRITE juste apres Restaurer",
					   ci != nullptr ? static_cast<float32>(ci->n) : -1.f);
				Temoin(cb != nullptr && !(cb->cible == a2) && !s.Monde().IsAlive(cb->cible),
					   "(i3n) la meme en OCTETS : poignee perimee (le defaut d'avant)", 0.f);

				// (i1n) une identite detruite ne resert pas.
				const ecs::NkEntityId d = s.Creer("D", NkVec2f(0.f, 0.f));
				const uint64 ud = s.Uid(d);
				s.Detruire(d);
				const ecs::NkEntityId e = s.Creer("E", NkVec2f(0.f, 0.f));
				Temoin(ud > uc && s.Uid(e) > ud && !s.EntiteParUid(ud).IsValid(),
					   "(i1n) une identite detruite ne resert jamais", static_cast<float32>(s.Uid(e)));

				// (i2) (h7 fichier) (i3 fichier)
				NkString json;
				const bool ecrit = NkSauverSceneJSON(s, json, static_cast<const NkTextures2D *>(nullptr));
				NkScene t;
				DeclarerBanc(t);
				NkString err;
				const bool lu = ecrit && NkChargerSceneJSON(t, json.View(), static_cast<NkTextures2D *>(nullptr), &err);
				const ecs::NkEntityId a3 = t.EntiteParUid(ua);
				const ecs::NkEntityId b3 = t.EntiteParUid(ub);
				const ecs::NkEntityId c3 = t.EntiteParUid(uc);
				Temoin(lu && std::strcmp(NomDe(t, a3), "A") == 0 && std::strcmp(NomDe(t, c3), "C") == 0 &&
						   t.Uid(t.EntiteParUid(s.Uid(e))) == s.Uid(e) && t.ProchainUid() == s.ProchainUid(),
					   "(i2) memes identites et meme compteur apres le fichier", static_cast<float32>(t.ProchainUid()));
				const NkTransform2D *lc3 = t.Local(c3);
				const NkBancCible *ci3 = lu ? t.Monde().Get<NkBancCible>(b3) : nullptr;
				Temoin(t.Parent(c3) == b3 && lc3 != nullptr && PresV(lc3->position, NkVec2f(1.f, 0.f)) && ci3 != nullptr &&
						   ci3->cible == a3,
					   "(h7) (i3) parent, local et reference d'entite traversent le fichier", 0.f);
				if (!lu) {
					std::printf("    erreur : %s\n", err.CStr());
				}

				// (h8) le fichier RETOUCHE A LA MAIN : le parent deplace de 3 m, pas
				//      ses enfants. L'enfant doit revenir a (parent o local).
				const NkTransform2D tb = *s.Monde().Get<NkTransform2D>(s.EntiteParUid(ub));
				char avant[128];
				char apres[128];
				std::snprintf(avant, sizeof(avant), "\"transform\": \"%.9g %.9g %.9g %.9g %.9g\"",
							  static_cast<double>(tb.position.x), static_cast<double>(tb.position.y),
							  static_cast<double>(tb.rotation), static_cast<double>(tb.echelle.x),
							  static_cast<double>(tb.echelle.y));
				std::snprintf(apres, sizeof(apres), "\"transform\": \"%.9g %.9g %.9g %.9g %.9g\"",
							  static_cast<double>(tb.position.x + 3.f), static_cast<double>(tb.position.y),
							  static_cast<double>(tb.rotation), static_cast<double>(tb.echelle.x),
							  static_cast<double>(tb.echelle.y));
				const char *at = std::strstr(json.CStr(), avant);
				NkString retouche;
				if (at != nullptr) {
					retouche = NkString(json.CStr(), static_cast<usize>(at - json.CStr()));
					retouche.Append(apres);
					retouche.Append(at + std::strlen(avant));
				}
				NkScene u;
				DeclarerBanc(u);
				const bool lu4 = at != nullptr && NkChargerSceneJSON(u, retouche.View(), static_cast<NkTextures2D *>(nullptr), &err);
				const ecs::NkEntityId c4 = u.EntiteParUid(uc);
				const NkTransform2D *tc4 = lu4 ? u.Monde().Get<NkTransform2D>(c4) : nullptr;
				Temoin(tc4 != nullptr && PresV(tc4->position, NkVec2f(mondeC.x + 3.f, mondeC.y), 1.0e-4f) &&
						   PresV(u.Local(c4)->position, NkVec2f(1.f, 0.f)),
					   "(h8) fichier retouche (parent deplace) : l'enfant suit son local (x)",
					   tc4 != nullptr ? tc4->position.x : -1.f);
			}

			// (f1) (f2) LA FUSION du 30/09 avec la physique : hierarchie, attache
			//      particule-rigide et controleur, ENSEMBLE
			{
				NkScene s;
				NkSceneConfig cfg;
				cfg.physique = true;
				cfg.particules = true;
				s.Init(cfg);
				s.Particules()->reglages.limites.actif = false;
				const ecs::NkEntityId porteur = s.Creer("Porteur", NkVec2f(0.f, 0.f));
				const ecs::NkEntityId caisse = s.Creer("CaisseF", NkVec2f(0.f, 0.3f));
				NkCollisionneur2D cc;
				cc.demiTaille = NkVec2f(0.4f, 0.3f);
				s.Monde().Add<NkCollisionneur2D>(caisse, cc);
				NkCorps2D kc;
				kc.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(caisse, kc);
				// Le corps est REFAIT trois fois : son id ne vaut plus celui qu'un monde
				// neuf lui donnerait, et un rebranchement « par hasard » ne passe plus.
				s.ActualiserCorps(caisse);
				s.ActualiserCorps(caisse);
				s.ActualiserCorps(caisse);
				s.Rattacher(caisse, porteur, true);
				const int32 ci = physics::NkCreerBlobP2D(*s.Particules(), NkVec2f(0.72f, 0.4f), 0.3f, physics::NkPresetP2D::NK_BLOB);
				const ecs::NkEntityId gelee = s.CreerCorpsMou("GeleeF", ci, 0x7CE64CFFu);
				const uint32 bas[3] = {0u, 1u, 2u};
				s.Particules()->CreerPartie(static_cast<uint32>(ci), "pieds", bas, 3u);
				const uint32 nAtt = s.Particules()->AttacherZone(static_cast<uint32>(ci), NkVec2f(0.47f, 0.4f), 0.15f,
																 *s.MondePhysique(), s.Monde().Get<NkCorps2D>(caisse)->corpsId);
				const ecs::NkEntityId heros = s.Creer("HerosF", NkVec2f(-3.f, 0.4f));
				NkCollisionneur2D ch;
				ch.forme = NkForme2D::NK_CERCLE;
				ch.rayon = 0.25f;
				s.Monde().Add<NkCollisionneur2D>(heros, ch);
				NkCorps2D kh;
				s.AjouterCorps(heros, kh);
				NkControleRigide2D cr;
				cr.reglages.vitesseMax = 7.25f;
				cr.reglages.actionSauter = 7;
				s.Monde().Add<NkControleRigide2D>(heros, cr);
				NkControleMou2D cm;
				cm.reglages.vitesseSaut = 4.5f;
				std::snprintf(cm.partieSol, sizeof(cm.partieSol), "%s", "pieds");
				s.Monde().Add<NkControleMou2D>(gelee, cm);
				const uint64 uCaisse = s.Uid(caisse);
				const uint64 uPorteur = s.Uid(porteur);
				const uint64 uHeros = s.Uid(heros);
				const uint64 uGelee = s.Uid(gelee);
				const uint32 idAvant = s.Monde().Get<NkCorps2D>(caisse)->corpsId;

				// Tout ce que la fusion doit garder, sur une scene `t` (les
				// identites sont celles de `s` si `avecIdentites`).
				auto Verifier = [&](NkScene &t, bool avecIdentites, NkString &quoi) {
					const ecs::NkEntityId c2 = avecIdentites ? t.EntiteParUid(uCaisse) : ParNom(t, "CaisseF");
					const ecs::NkEntityId p2 = avecIdentites ? t.EntiteParUid(uPorteur) : ParNom(t, "Porteur");
					const ecs::NkEntityId h2 = avecIdentites ? t.EntiteParUid(uHeros) : ParNom(t, "HerosF");
					const ecs::NkEntityId g2 = avecIdentites ? t.EntiteParUid(uGelee) : ParNom(t, "GeleeF");
					if (!c2.IsValid() || !h2.IsValid() || !g2.IsValid()) {
						quoi = "entites";
						return false;
					}
					if (avecIdentites && !(t.Parent(c2) == p2)) {
						quoi = "parent";
						return false;
					}
					const NkCorps2D *k = t.Monde().Get<NkCorps2D>(c2);
					const physics::NkParticules2D *p = t.Particules();
					if (k == nullptr || p == nullptr || nAtt == 0u || p->attaches.Size() != nAtt || p->parties.Size() != 1u) {
						quoi = "attaches / parties";
						return false;
					}
					for (uint32 i = 0; i < p->attaches.Size(); ++i) {
						if (p->attaches[i].rigide != k->corpsId) {
							quoi = "attache non rebranchee";
							return false;
						}
					}
					const NkControleRigide2D *r2 = t.Monde().Get<NkControleRigide2D>(h2);
					const NkControleMou2D *m2 = t.Monde().Get<NkControleMou2D>(g2);
					if (r2 == nullptr || r2->reglages.vitesseMax != 7.25f || r2->reglages.actionSauter != 7 || m2 == nullptr ||
						m2->reglages.vitesseSaut != 4.5f || std::strcmp(m2->partieSol, "pieds") != 0) {
						quoi = "controleurs";
						return false;
					}
					return true;
				};

				NkString json;
				NkSauverSceneJSON(s, json, static_cast<const NkTextures2D *>(nullptr));
				NkScene::NkPhoto photo;
				s.Photographier(photo);
				s.Restaurer(photo);
				NkString quoi1;
				const bool okPhoto = Verifier(s, true, quoi1);
				NkScene t;
				NkString err;
				const bool lu = NkChargerSceneJSON(t, json.View(), static_cast<NkTextures2D *>(nullptr), &err);
				NkString quoi2;
				const bool okFichier = lu && Verifier(t, true, quoi2);
				Temoin(okPhoto && okFichier,
					   "(f1) fusion : parent, attache rebranchee, controleurs (photo et v2)",
					   static_cast<float32>(nAtt));
				if (!okPhoto || !okFichier) {
					std::printf("    ecart : photo[%s] fichier[%s] %s\n", quoi1.CStr(), quoi2.CStr(), err.CStr());
				}

				// (f2) le meme fichier tel que l'ECRIVAIT la branche physique : version
				//      1, pas d'identite, "id" sous "corps". Ramene ici depuis la v2
				//      (memes cles pour le reste : c'est le meme code d'ecriture).
				NkArchive a;
				NkJSONReader::ReadArchive(json.View(), a, nullptr);
				a.SetInt32(NkStringView("version"), 1);
				a.Remove(NkStringView("prochainUid"));
				NkVector<NkArchive> entites;
				a.GetObjectArray(NkStringView("entites"), entites);
				for (uint32 i = 0; i < entites.Size(); ++i) {
					entites[i].Remove(NkStringView("uid"));
					entites[i].Remove(NkStringView("parent"));
					entites[i].Remove(NkStringView("local"));
				}
				a.SetObjectArray(NkStringView("entites"), entites);
				NkString physiqueV1;
				NkJSONWriter::WriteArchive(a, physiqueV1, true, 1);
				NkScene u;
				const bool lu2 = NkChargerSceneJSON(u, physiqueV1.View(), static_cast<NkTextures2D *>(nullptr), &err);
				NkString quoi3;
				const bool okV1 = lu2 && Verifier(u, false, quoi3) &&
								  std::strstr(physiqueV1.CStr(), "\"version\": 1") != nullptr &&
								  std::strstr(physiqueV1.CStr(), "\"uid\"") == nullptr &&
								  u.Monde().Get<NkCorps2D>(ParNom(u, "CaisseF"))->corpsId != idAvant;
				Temoin(okV1, "(f2) fichier de la physique (v1, \"id\") : attaches et controleurs relus", 0.f);
				if (!okV1) {
					std::printf("    ecart : %s %s\n", quoi3.CStr(), err.CStr());
				}
			}

			// (v1) le fichier version 1 fige
			{
				const uint8 quatre[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
				NkTextures2D tex;
				tex.Creer(quatre, 2, 2, "quatre");
				NkSons2D sons;
				NkVector<float32> bip;
				bip.Resize(480);
				sons.Creer(bip.Data(), bip.Size(), 48000, "a");
				sons.Creer(bip.Data(), bip.Size(), 48000, "b");
				NkRessourcesScene r;
				r.textures = &tex;
				r.sons = &sons;
				NkScene s;
				s.PhotographierAussi<NkBancMarqueV1>("banc.NkBancMarque");
				NkString err;
				const bool lu = NkChargerSceneJSON(s, NkStringView(kSceneV1), r, &err);
				NkString quoi;
				const bool juste = lu && VerifierSceneV1(s, tex, quoi);
				const bool uids = lu && s.Uid(ParNom(s, "Sol")) == 1u && s.Uid(ParNom(s, "Caisse")) == 2u &&
								  s.Uid(ParNom(s, "Radio")) == 3u && s.Camera().Zoom() == 40.f;
				Temoin(juste && uids, "(v1) fichier v1 du code d'avant relu a l'identique (identites 1..3)", 3.f);
				if (!juste) {
					std::printf("    ecart : %s %s\n", quoi.CStr(), err.CStr());
				}
				NkString v2;
				const bool reecrit = lu && NkSauverSceneJSON(s, v2, r);
				NkScene t;
				t.PhotographierAussi<NkBancMarqueV1>("banc.NkBancMarque");
				const bool relu = reecrit && NkChargerSceneJSON(t, v2.View(), r, &err);
				NkString quoi2;
				const bool juste2 = relu && VerifierSceneV1(t, tex, quoi2) && t.Uid(ParNom(t, "Radio")) == 3u;
				const bool enV2 = std::strstr(v2.CStr(), "\"version\": 2") != nullptr;
				Temoin(juste2 && enV2, "(v1) puis reecrit en v2 et relu : les memes valeurs", enV2 ? 2.f : 1.f);
				if (!juste2) {
					std::printf("    ecart : %s %s\n", quoi2.CStr(), err.CStr());
				}
				// (r2) le composant decrit est un OBJET dans la v2.
				Temoin(std::strstr(v2.CStr(), "\"NkVitesse2D\": {") != nullptr &&
						   std::strstr(v2.CStr(), "0000003f000000000000c03f") == nullptr &&
						   std::strstr(v2.CStr(), "\"son\": \"b\"") != nullptr,
					   "(r2) v2 : NkVitesse2D en objet, NkSource2D.son par son NOM", 0.f);
			}

			// (r1) (r1n) l'ajout d'un champ
			{
				NkScene s1;
				NkSceneConfig cfg;
				s1.Init(cfg);
				s1.PhotographierAussi<NkBancVersion1>("banc.Version", kChampsV1, 2u);
				s1.PhotographierAussi<NkBancBrut1>("banc.Brut"); // SANS description : octets
				const ecs::NkEntityId e = s1.Creer("X", NkVec2f(0.f, 0.f));
				s1.Monde().Add<NkBancVersion1>(e, NkBancVersion1{3u, 4.5f});
				s1.Monde().Add<NkBancBrut1>(e, NkBancBrut1{3u, 4.5f});
				NkString json;
				NkSauverSceneJSON(s1, json, static_cast<const NkTextures2D *>(nullptr));
				NkScene s2;
				s2.PhotographierAussi<NkBancVersion2>("banc.Version", kChampsV2, 3u);
				s2.PhotographierAussi<NkBancBrut2>("banc.Brut");
				NkString err;
				const bool lu = NkChargerSceneJSON(s2, json.View(), static_cast<NkTextures2D *>(nullptr), &err);
				const ecs::NkEntityId x = ParNom(s2, "X");
				const NkBancVersion2 *v = lu ? s2.Monde().Get<NkBancVersion2>(x) : nullptr;
				Temoin(v != nullptr && v->a == 3u && v->b == 4.5f && v->nouveau == 7.f,
					   "(r1) champ AJOUTE au milieu : anciens justes, nouveau par defaut (b)", v != nullptr ? v->b : -1.f);

				// (r1n) la meme evolution en OCTETS : la taille a change, le composant
				// est perdu (a taille egale, ses champs seraient decales).
				Temoin(lu && s2.Monde().IsAlive(x) && !s2.Monde().Has<NkBancBrut2>(x),
					   "(r1n) la meme chose en octets : le composant est perdu", 0.f);
			}

			// (a7) (a7n) un son garde son identite
			{
				NkVector<float32> bip;
				bip.Resize(480);
				NkSons2D sonsA;
				sonsA.Creer(bip.Data(), bip.Size(), 48000, "pas.wav");
				const uint32 cloche = sonsA.Creer(bip.Data(), bip.Size(), 48000, "cloche.wav");
				NkScene a;
				NkSceneConfig cfg;
				a.Init(cfg);
				const ecs::NkEntityId e = a.Creer("Cloche", NkVec2f(0.f, 0.f));
				NkSource2D src;
				src.son = cloche;
				src.boucle = true;
				a.Monde().Add<NkSource2D>(e, src);
				NkRessourcesScene ra;
				ra.sons = &sonsA;
				NkString json;
				NkSauverSceneJSON(a, json, ra);
				// Une AUTRE session : les sons se chargent dans l'autre ordre.
				NkSons2D sonsB;
				sonsB.Creer(bip.Data(), bip.Size(), 48000, "cloche.wav");
				sonsB.Creer(bip.Data(), bip.Size(), 48000, "pas.wav");
				NkRessourcesScene rb;
				rb.sons = &sonsB;
				NkScene b;
				NkString err;
				const bool lu = NkChargerSceneJSON(b, json.View(), rb, &err);
				const NkSource2D *sb = lu ? b.Monde().Get<NkSource2D>(ParNom(b, "Cloche")) : nullptr;
				Temoin(sb != nullptr && sb->son == sonsB.Trouver("cloche.wav") && std::strcmp(sonsB.Nom(sb->son), "cloche.wav") == 0 &&
						   cloche == 2u && sb->son == 1u,
					   "(a7) le son garde son identite apres reouverture (id 2 -> 1)", sb != nullptr ? static_cast<float32>(sb->son) : -1.f);
				// (a7n) ecrit SANS registre de sons : le numero de session, comme avant.
				NkString brut;
				NkSauverSceneJSON(a, brut, static_cast<const NkTextures2D *>(nullptr));
				NkScene c;
				const bool lu2 = NkChargerSceneJSON(c, brut.View(), rb, &err);
				const NkSource2D *sc = lu2 ? c.Monde().Get<NkSource2D>(ParNom(c, "Cloche")) : nullptr;
				Temoin(sc != nullptr && sc->son == 2u && std::strcmp(sonsB.Nom(sc->son), "pas.wav") == 0,
					   "(a7n) par numero de session : c'est « pas.wav » qui sonne", 0.f);
			}

			// (p1) (p2) (p3) les prefabs
			{
				NkScene s;
				NkSceneConfig cfg;
				cfg.physique = true;
				s.Init(cfg);
				NkPrefabs2D pf;
				const ecs::NkEntityId racine = s.Creer("Piece", NkVec2f(0.f, 0.f));
				NkSprite2D sp;
				sp.couleur = 0xFF0000FFu;
				s.Monde().Add<NkSprite2D>(racine, sp);
				NkCollisionneur2D col;
				col.forme = NkForme2D::NK_CERCLE;
				col.rayon = 0.25f;
				s.Monde().Add<NkCollisionneur2D>(racine, col);
				NkCorps2D k;
				k.masse = 1.f;
				s.AjouterCorps(racine, k);
				const ecs::NkEntityId eclat = s.Creer("Eclat", NkVec2f(0.5f, 0.f));
				s.Monde().Add<NkSprite2D>(eclat, sp);
				s.Rattacher(eclat, racine, true);
				const uint32 id = pf.Creer(s, racine, "Prefabs/Piece.nkprefab");
				NkVector<ecs::NkEntityId> poses;
				for (int32 i = 0; i < 20; ++i) {
					poses.PushBack(pf.Instancier(s, id, NkVec2f(static_cast<float32>(i) * 2.f, 5.f)));
				}
				// Chaque instance : sa racine a sa place, son eclat 0,5 m a droite.
				bool places = true;
				for (uint32 i = 0; i < poses.Size() && places; ++i) {
					NkVector<ecs::NkEntityId> enfants;
					s.Enfants(poses[i], enfants);
					places = enfants.Size() == 1u &&
							 PresV(s.Monde().Get<NkTransform2D>(enfants[0])->position,
								   NkVec2f(static_cast<float32>(i) * 2.f + 0.5f, 5.f)) &&
							 SpriteDe(s, poses[i])->couleur == 0xFF0000FFu && s.Uid(poses[i]) != s.Uid(racine);
				}
				// UNE surcharge, a la main, sur l'instance 7.
				SpriteDe(s, poses[7])->couleur = 0x00FF00FFu;
				// Le modele change : couleur, masse, et un enfant de plus.
				SpriteDe(s, racine)->couleur = 0x0000FFFFu;
				CorpsDe(s, racine)->masse = 3.f;
				s.ActualiserCorps(racine);
				const ecs::NkEntityId nouveau = s.Creer("Nouveau", NkVec2f(0.f, 1.f));
				s.Rattacher(nouveau, racine, true);
				const uint32 suivies = pf.MettreAJour(s, id, racine);
				uint32 bleues = 0;
				uint32 masses = 0;
				uint32 nouveaux = 0;
				for (uint32 i = 0; i < poses.Size(); ++i) {
					if (SpriteDe(s, poses[i])->couleur == 0x0000FFFFu) {
						++bleues;
					}
					const NkCorps2D *kc = s.Monde().Get<NkCorps2D>(poses[i]);
					const physics::NkRigidBody *b = kc != nullptr ? s.MondePhysique()->GetBody(kc->corpsId) : nullptr;
					if (kc != nullptr && kc->masse == 3.f && b != nullptr && Pres(b->invMass, 1.f / 3.f, 1.0e-4f)) {
						++masses;
					}
					NkVector<ecs::NkEntityId> enfants;
					s.Enfants(poses[i], enfants);
					for (uint32 j = 0; j < enfants.Size(); ++j) {
						if (std::strcmp(NomDe(s, enfants[j]), "Nouveau") == 0 &&
							PresV(s.Monde().Get<NkTransform2D>(enfants[j])->position, NkVec2f(static_cast<float32>(i) * 2.f, 6.f))) {
							++nouveaux;
						}
					}
				}
				Temoin(places && suivies == 21u && bleues == 19u && masses == 20u && nouveaux == 20u,
					   "(p1) 20 instances suivent le modele (couleur, masse, enfant ajoute)", static_cast<float32>(bleues));
				NkVector<NkString> sur;
				pf.Surcharges(s, poses[7], sur);
				bool listee = false;
				for (uint32 i = 0; i < sur.Size(); ++i) {
					listee = listee || sur[i] == "NkSprite2D.couleur";
				}
				Temoin(SpriteDe(s, poses[7])->couleur == 0x00FF00FFu && listee && sur.Size() == 1u,
					   "(p1) la surcharge de l'instance 7 est GARDEE, et elle seule listee", static_cast<float32>(sur.Size()));

				// (p2) le fichier .nkprefab
				NkRessourcesScene r;
				r.prefabs = &pf;
				NkString texte;
				const bool ecrit = pf.EnregistrerJSON(s, id, texte, r);
				NkScene s2;
				s2.Init(cfg);
				NkPrefabs2D pf2;
				NkRessourcesScene r2;
				r2.prefabs = &pf2;
				uint32 id2 = 0;
				NkString err;
				const bool lu = ecrit && pf2.ChargerJSON(s2, "Prefabs/Piece.nkprefab", texte.View(), r2, id2, &err);
				const ecs::NkEntityId i1 = lu ? pf2.Instancier(s2, id2, NkVec2f(1.f, 1.f)) : ecs::NkEntityId::Invalid();
				NkVector<ecs::NkEntityId> enfants2;
				s2.Enfants(i1, enfants2);
				const ecs::NkEntityId i2 = lu ? pf2.Instancier(s2, id2, NkVec2f(9.f, 1.f)) : ecs::NkEntityId::Invalid();
				Temoin(lu && s2.Monde().IsAlive(i1) && SpriteDe(s2, i1)->couleur == 0x0000FFFFu &&
						   enfants2.Size() == 2u && CorpsDe(s2, i1)->masse == 3.f,
					   "(p2) .nkprefab ecrit, relu ailleurs, instancie a l'identique", static_cast<float32>(enfants2.Size()));
				if (!lu) {
					std::printf("    erreur : %s\n", err.CStr());
				}
				// Le fichier change sur le disque (couleur) : on le relit, les instances suivent.
				NkString modifie = texte;
				{
					char avant[32];
					char apres[32];
					std::snprintf(avant, sizeof(avant), "\"couleur\": %lu", static_cast<unsigned long>(0x0000FFFFu));
					std::snprintf(apres, sizeof(apres), "\"couleur\": %lu", static_cast<unsigned long>(0xFFFF00FFu));
					const char *at = std::strstr(modifie.CStr(), avant);
					if (at != nullptr) {
						const usize pos = static_cast<usize>(at - modifie.CStr());
						NkString nouv(modifie.CStr(), pos);
						nouv.Append(apres);
						nouv.Append(at + std::strlen(avant));
						modifie = nouv;
					}
				}
				SpriteDe(s2, i2)->couleur = 0x123456FFu; // surcharge sur i2
				uint32 id3 = 0;
				const bool relu = pf2.ChargerJSON(s2, "Prefabs/Piece.nkprefab", modifie.View(), r2, id3, &err);
				Temoin(relu && id3 == id2 && SpriteDe(s2, i1)->couleur == 0xFFFF00FFu &&
						   SpriteDe(s2, i2)->couleur == 0x123456FFu,
					   "(p2) fichier modifie et relu : les instances suivent, surcharge gardee", 0.f);

				// (p3) une instance dans un .nkscene retrouve son prefab par NOM
				NkString scene;
				NkSauverSceneJSON(s2, scene, r2);
				NkPrefabs2D pf3;
				NkScene s3;
				NkRessourcesScene r3;
				r3.prefabs = &pf3;
				uint32 id4 = 0;
				pf3.Creer(s3, s3.Creer("Leurre", NkVec2f(0.f, 0.f)), "Autre.nkprefab"); // decale les identifiants
				pf3.ChargerJSON(s3, "Prefabs/Piece.nkprefab", modifie.View(), r3, id4, &err);
				const bool lu3 = NkChargerSceneJSON(s3, scene.View(), r3, &err);
				NkVector<ecs::NkEntityId> inst3;
				pf3.Instances(s3, id4, inst3);
				Temoin(lu3 && id4 == 2u && inst3.Size() == 2u, "(p3) instances d'un .nkscene relie a leur prefab PAR NOM",
					   static_cast<float32>(inst3.Size()));
			}

			// (x1) les deux types d'asset ajoutes, dans les deux sens de la table
			//      unique, sans deplacer ceux d'avant
			{
				const bool aller = std::strcmp(NkAssetExtensionFor(NkAssetType::Scene), "nkscene") == 0 &&
								   std::strcmp(NkAssetExtensionFor(NkAssetType::SaveGame), "nksave") == 0;
				const bool retour = NkAssetTypeFromExtension(".nkscene") == NkAssetType::Scene &&
									NkAssetTypeFromExtension("NKSAVE") == NkAssetType::SaveGame;
				// La fusion avec l'animation (30/09) : AnimationController a pris 17,
				// Scene et SaveGame sont 18 et 19 ; aucun ne marche sur un autre.
				const bool avant = NkAssetTypeFromExtension("nkprefab") == NkAssetType::Prefab &&
								   static_cast<uint32>(NkAssetType::Script) == 16u &&
								   static_cast<uint32>(NkAssetType::AnimationController) == 17u &&
								   static_cast<uint32>(NkAssetType::Scene) == 18u &&
								   static_cast<uint32>(NkAssetType::SaveGame) == 19u &&
								   NkAssetTypeFromExtension("nkanimctl") == NkAssetType::AnimationController &&
								   static_cast<uint32>(NkAssetType::Custom) == 255u &&
								   std::strcmp(NkAssetTypeName(NkAssetType::SaveGame), "SaveGame") == 0;
				Temoin(aller && retour && avant, "(x1) types d'asset Scene (.nkscene) et SaveGame (.nksave)",
					   static_cast<float32>(NkAssetType::SaveGame));
			}

			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecsS == 0 ? "BANC STRUCTURE REUSSI" : "BANC STRUCTURE EN ECHEC",
						gReussisS, gEchecsS, gEchecsS > 1 ? "s" : "");
			return gEchecsS == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
