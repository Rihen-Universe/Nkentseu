// =============================================================================
// NkUnkenyBancScripts.cpp — le banc des SCRIPTS du moteur (2026-10-01)
//
// Document 01 d'UnkenyEditor, § 9 : l'hote (x), le module et la VM (b), le C++
// (c). Sans fenetre, sans compilateur : les classes C++ sont liees en STATIQUE
// au banc (le chemin du jeu construit), les modules Blueprint sont ecrits A LA
// MAIN (la contrainte du 24/08 : un module existe sans graphe).
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (x1)  deux scripts sur une entite, premier passage = un pas fixe : Debut de
//         A puis de B, AVANT tout autre evenement, puis chaque evenement a A
//         puis a B (l'ordre de la liste, decision Q1) ;
//         (x1n) une entite sans NkScript2D ne recoit rien
//   (x2)  a 30 i/s pour un pas fixe de 1/60 : « Pas fixe » deux fois (dt 1/60),
//         « Tick » une fois (dt 1/30)
//   (x3)  balle lachee sur le sol : « Contact debut » recu par la balle
//         (autre = Sol) ET par le sol (autre = Balle) ; (x3n) dans le vide : rien
//   (x4)  zone traversee : entree puis sortie, soiEstLaZone vrai pour la zone seule
//   (x5)  « Sauter » tenue trois trames : « Action pressee » UNE fois, puis
//         « Action relachee » une fois ; (x5n) une action jamais pressee : rien
//   (x6)  un script se DETRUIT dans Tick : plus aucun appel, son corps est parti,
//         les autres scripts tournent
//   (x7)  Photographier / Arreter / Restaurer : variables rendues, Debut rejoue
//   (x8)  .nkscene : references et variables vont et reviennent PAR NOM
//   (x9)  entite morte passee aux fonctions de la table : refus, rien ne bouge ;
//         force NaN refusee, vitesse finie
//   (x10) cout : 1000 entites, Tick Blueprint qui lit une position et ecrit une
//         variable -- temps par trame PUBLIE, sans seuil
//   (x11) effets (R34) : Arreter l'effet eteint l'emetteur ; Jouer le rallume et
//         le rejoue (des particules renaissent)
//   (c1)  module statique : classe trouvee par nom ; la meme trajectoire que b1
//         au millimetre (deux langages, une table)
//   (c2)  exception dans Tick : rattrapee, instance en faute, la scene continue
//   (c3)  module d'ABI majeure differente : refuse ; mineure differente : accepte
//   (r1)  RECHARGEMENT A CHAUD (deux modules statiques, meme classe) : l'etat
//         prive garde (Sauver/Relire), la variable exposee gardee, « Recharge »
//         recu une fois et pas de second Debut, le NOUVEAU comportement actif ;
//         (r1n) une classe disparue : instance detruite, le journal le dit
//   (b1)  module ecrit a la main : s'ecrit, se relit octet pour octet, et fait
//         sauter une balle (Debut -> Impulsion (0, 5)) a v²/2g a 5 % (Euler
//         semi-implicite a 60 Hz perd v·dt/2, 3 % ici)
//   (b2)  verificateur : saut hors fonction, registre hors cadre, import
//         inconnu, type discordant, constante hors table -> refus NOMME chacun ;
//         (b2n) le module de b1 passe
//   (b3)  boucle sans fin : coupee au budget, faute qui designe pc et noeud,
//         les autres entites avancent ; (b3n) 1000 tours : la boucle finit
//   (b4)  Si et Sequence : trace des « Afficher » = A, B1, B2
//   (b5)  imports permutes : meme execution
//   (b6)  force en « Pas fixe » : v = F/m·t a 1 % ; (b6n) la meme sous Tick :
//         refusee par le verificateur
//   (p1)  LA PORTE, en Blueprint : quand « Joueur » entre dans la zone, la porte
//         monte de 2 m ; (p1n) un « Caillou » qui entre : la porte reste
//   (p2)  la meme porte en C++ : meme resultat
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Unkeny/Banc/NkUnkenyBanc.h"
#include "Unkeny/Banc/NkUnkenyBancTas.h"

#include "Unkeny/Entree/NkUnkenyActions.h"
#include "Unkeny/Entree/NkUnkenyActionsStandard.h"
#include "Unkeny/Scene/NkUnkenySauvegarde.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Script/NkUnkenyBpMachine.h"
#include "Unkeny/Script/NkUnkenyScripts.h"

#include <chrono>
#include <limits>
#include <cstdio>
#include <cstring>
#include <stdexcept>

// =============================================================================
// Les classes C++ du banc (liees en statique, comme dans un jeu construit)
// =============================================================================
namespace {
	char gTrace[4096];
	void Tracer(const char *t) {
		const size_t n = std::strlen(gTrace);
		std::snprintf(gTrace + n, sizeof(gTrace) - n, "%s ", t);
	}
	int gTicks = 0;

	class BancTraceA : public nkunk::Script {
		public:
			void Debut() override {
				Tracer("A:D");
			}
			void PasFixe(float dt) override {
				Tracer(dt > 0.016f && dt < 0.0170f ? "A:P" : "A:P?");
			}
			void Tick(float dt) override {
				Tracer(dt > 0.033f && dt < 0.0334f ? "A:T" : "A:T?");
			}
	};
	class BancTraceB : public nkunk::Script {
		public:
			void Debut() override {
				Tracer("B:D");
			}
			void PasFixe(float) override {
				Tracer("B:P");
			}
			void Tick(float) override {
				Tracer("B:T");
			}
	};
	/// Ecrit les contacts et les zones : « C<nom de soi>><nom de l'autre> ».
	class BancContacts : public nkunk::Script {
		public:
			void Ecrire(const char *genre, NkUnkEntite autre, int zone) {
				char a[32] = {}, s[32] = {};
				hote_->Nom(hote_->ctx, autre, a, sizeof(a));
				hote_->Nom(hote_->ctx, Soi(), s, sizeof(s));
				char t[96];
				std::snprintf(t, sizeof(t), "%s:%s>%s%s", genre, s, a, zone ? "(z)" : "");
				Tracer(t);
			}
			void ContactDebut(NkUnkEntite autre) override {
				Ecrire("CD", autre, 0);
			}
			void ZoneEntree(NkUnkEntite autre, bool soiZone) override {
				Ecrire("ZE", autre, soiZone ? 1 : 0);
			}
			void ZoneSortie(NkUnkEntite autre, bool soiZone) override {
				Ecrire("ZS", autre, soiZone ? 1 : 0);
			}
	};
	class BancActions : public nkunk::Script {
		public:
			void ActionPressee(const char *a, float) override {
				char t[48];
				std::snprintf(t, sizeof(t), "P:%s", a);
				Tracer(t);
			}
			void ActionRelachee(const char *a) override {
				char t[48];
				std::snprintf(t, sizeof(t), "R:%s", a);
				Tracer(t);
			}
	};
	class BancSuicide : public nkunk::Script {
		public:
			void Tick(float) override {
				++gTicks;
				Detruire(Soi());
			}
	};
	class BancCompteur : public nkunk::Script {
		public:
			void Debut() override {
				Tracer("K:D");
			}
			void Tick(float) override {
				PoserVariable("n", Variable("n") + 1.f);
			}
	};
	class BancLanceur : public nkunk::Script {
		public:
			void Debut() override {
				Impulsion(Soi(), nkunk::Vec2(0.f, 5.f));
			}
	};
	class BancFautif : public nkunk::Script {
		public:
			void Tick(float) override {
				throw std::runtime_error("expres");
			}
	};
	uint64_t gMorte = 0u;
	int gHostiles = 0; ///< appels qui ont REUSSI sur une entite morte (attendu : 0)
	int gNaN = -1;
	class BancHostile : public nkunk::Script {
		public:
			void Debut() override {
				NkUnkEntite m{gMorte};
				const NkUnkHoteV1 *h = hote_;
				float x = 0.f, y = 0.f;
				char b[8];
				gHostiles += h->Vivante(h->ctx, m) + h->Detruire(h->ctx, m) + h->NomEst(h->ctx, m, "x") + h->Nom(h->ctx, m, b, 8) +
							 h->Activer(h->ctx, m, 0) + h->Position(h->ctx, m, &x, &y) + h->Teleporter(h->ctx, m, 1.f, 1.f) +
							 h->Rotation(h->ctx, m, &x) + h->PoserRotation(h->ctx, m, 1.f) + h->Vitesse(h->ctx, m, &x, &y) +
							 h->PoserVitesse(h->ctx, m, 1.f, 1.f) + h->AppliquerImpulsion(h->ctx, m, 1.f, 1.f) +
							 h->AppliquerForce(h->ctx, m, 1.f, 1.f) + h->PoserCouleur(h->ctx, m, 0u) + h->PoserVisible(h->ctx, m, 0) +
							 h->JouerClip(h->ctx, m, 0) + h->AnimParametre(h->ctx, m, "a", 1.f) + h->AnimDeclencher(h->ctx, m, "a") +
							 h->JouerEffet(h->ctx, m) + h->ArreterEffet(h->ctx, m) + h->LireVariable(h->ctx, m, 0, "n", &x, &y) +
							 h->EcrireVariable(h->ctx, m, 0, "n", 1.f, 1.f);
				const float nan = std::numeric_limits<float>::quiet_NaN();
				gNaN = h->AppliquerForce(h->ctx, Soi(), nan, 0.f) + h->AppliquerImpulsion(h->ctx, Soi(), 0.f, nan) +
					   h->PoserVitesse(h->ctx, Soi(), nan, nan) + h->Teleporter(h->ctx, Soi(), nan, 0.f);
			}
	};
	/// La porte, en C++ : le modele que l'editeur pose dans Contenu/Scripts.
	class BancPorte : public nkunk::Script {
		public:
			void ZoneEntree(NkUnkEntite autre, bool soiEstLaZone) override {
				if (soiEstLaZone && NomEst(autre, "Joueur")) {
					NkUnkEntite porte = ParNom("Porte");
					Teleporter(porte, Position(porte) + nkunk::Vec2(0.f, 2.f));
					Afficher("La porte s'ouvre");
				}
			}
	};
	class BancEffet : public nkunk::Script {
		public:
			void Tick(float) override {
				++gTicks;
				if (gTicks == 1) {
					ArreterEffet(Soi());
				} else if (gTicks == 3) {
					JouerEffet(Soi());
				}
			}
	};
	/// Le rechargement : v1 compte de 1, v2 de 10 ; l'etat prive est GARDE.
	int gRecharges = 0;
	class BancRechargeV1 : public nkunk::Script {
		public:
			struct Etat {
					int n = 0;
			} etat;
			NK_UNKENY_GARDER(etat)
			void Debut() override {
				Tracer("R:D");
			}
			void Tick(float) override {
				etat.n += 1;
				PoserVariable("vu", static_cast<float>(etat.n));
			}
	};
	class BancRechargeV2 : public nkunk::Script {
		public:
			struct Etat {
					int n = 0;
			} etat;
			NK_UNKENY_GARDER(etat)
			void Debut() override {
				Tracer("R:D");
			}
			void Recharge() override {
				++gRecharges;
			}
			void Tick(float) override {
				etat.n += 10;
				PoserVariable("vu", static_cast<float>(etat.n));
			}
	};
} // namespace

NK_UNKENY_CLASSE(BancTraceA)
NK_UNKENY_CLASSE(BancTraceB)
NK_UNKENY_CLASSE(BancContacts)
NK_UNKENY_CLASSE(BancActions)
NK_UNKENY_CLASSE(BancSuicide)
NK_UNKENY_CLASSE_VARIABLES(BancCompteur, NK_UNKENY_REEL("n", 0.f))
NK_UNKENY_CLASSE(BancLanceur)
NK_UNKENY_CLASSE(BancFautif)
NK_UNKENY_CLASSE(BancHostile)
NK_UNKENY_CLASSE(BancPorte)
NK_UNKENY_CLASSE(BancEffet)

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gEchecs = 0;
			int32 gReussis = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussis : gEchecs)++;
			}
			float32 Absf(float32 v) {
				return v < 0.f ? -v : v;
			}

			const NkUnkModuleV1 *ModuleBanc() {
				static const NkUnkClasseV1 *c[] = {NkUnkClasse_BancTraceA(),   NkUnkClasse_BancTraceB(),  NkUnkClasse_BancContacts(),
												   NkUnkClasse_BancActions(),  NkUnkClasse_BancSuicide(), NkUnkClasse_BancCompteur(),
												   NkUnkClasse_BancLanceur(),  NkUnkClasse_BancFautif(),  NkUnkClasse_BancHostile(),
												   NkUnkClasse_BancPorte(),	   NkUnkClasse_BancEffet()};
				static const NkUnkModuleV1 m = {static_cast<uint32_t>(sizeof(NkUnkModuleV1)), NK_UNK_ABI_MAJEURE, NK_UNK_ABI_MINEURE,
												static_cast<uint32_t>(sizeof(c) / sizeof(c[0])), c};
				return &m;
			}
			/// Le module « v1 » puis « v2 » du rechargement : la MEME classe (« BancRecharge »).
			const NkUnkModuleV1 *ModuleRecharge(int32 version) {
				static const NkUnkClasseV1 v1 = {"BancRecharge",
												 &nkunk::Creer<BancRechargeV1>,
												 &nkunk::Detruire<BancRechargeV1>,
												 &nkunk::Evenement<BancRechargeV1>,
												 &nkunk::Sauver<BancRechargeV1>,
												 &nkunk::Relire<BancRechargeV1>,
												 0u,
												 nullptr};
				static const NkUnkClasseV1 v2 = {"BancRecharge",
												 &nkunk::Creer<BancRechargeV2>,
												 &nkunk::Detruire<BancRechargeV2>,
												 &nkunk::Evenement<BancRechargeV2>,
												 &nkunk::Sauver<BancRechargeV2>,
												 &nkunk::Relire<BancRechargeV2>,
												 0u,
												 nullptr};
				static const NkUnkClasseV1 *c1[] = {&v1};
				static const NkUnkClasseV1 *c2[] = {&v2};
				static const NkUnkModuleV1 m1 = {static_cast<uint32_t>(sizeof(NkUnkModuleV1)), NK_UNK_ABI_MAJEURE, NK_UNK_ABI_MINEURE, 1u, c1};
				static const NkUnkModuleV1 m2 = {static_cast<uint32_t>(sizeof(NkUnkModuleV1)), NK_UNK_ABI_MAJEURE, NK_UNK_ABI_MINEURE, 1u, c2};
				static const NkUnkModuleV1 m3 = {static_cast<uint32_t>(sizeof(NkUnkModuleV1)), NK_UNK_ABI_MAJEURE, NK_UNK_ABI_MINEURE, 0u, nullptr};
				return version == 1 ? &m1 : version == 2 ? &m2 : &m3;
			}

			/// Une entite portant les scripts `refs` (separes par des virgules).
			ecs::NkEntityId AvecScripts(NkScene &s, const char *nom, const NkVec2f &p, const char *a, const char *b = nullptr) {
				const ecs::NkEntityId id = s.Creer(nom, p);
				NkScript2D sc;
				NkScriptAjouter(sc, a);
				if (b != nullptr) {
					NkScriptAjouter(sc, b);
				}
				s.Monde().Add<NkScript2D>(id, sc);
				return id;
			}
			void Ajouter(NkScene &s, ecs::NkEntityId id, const char *ref) {
				NkScript2D *sc = s.Monde().Get<NkScript2D>(id);
				if (sc == nullptr) {
					NkScript2D n;
					NkScriptAjouter(n, ref);
					s.Monde().Add<NkScript2D>(id, n);
				} else {
					NkScriptAjouter(*sc, ref);
				}
			}
			void Corps(NkScene &s, ecs::NkEntityId id, NkTypeCorps type, bool cercle, const NkVec2f &demi,
					   bool declencheur = false, float32 gravite = 1.f) {
				NkCollisionneur2D c;
				c.forme = cercle ? NkForme2D::NK_CERCLE : NkForme2D::NK_BOITE;
				c.rayon = demi.x;
				c.demiTaille = demi;
				c.declencheur = declencheur;
				s.Monde().Add<NkCollisionneur2D>(id, c);
				NkCorps2D k;
				k.type = type;
				k.echelleGravite = gravite;
				s.AjouterCorps(id, k);
			}
			void Physique(NkScene &s) {
				NkSceneConfig c;
				c.physique = true;
				s.Init(c);
			}
			bool Contient(const char *texte, const char *motif) {
				return std::strstr(texte, motif) != nullptr;
			}
			/// Les occurrences de `motif` dans `texte`.
			int32 Compter(const char *texte, const char *motif) {
				int32 n = 0;
				for (const char *p = texte; (p = std::strstr(p, motif)) != nullptr; ++p) {
					++n;
				}
				return n;
			}
			bool JournalContient(const NkHoteScripts2D &h, const char *motif) {
				for (uint32 i = 0; i < h.Journal().Size(); ++i) {
					if (Contient(h.Journal()[i].texte.CStr(), motif)) {
						return true;
					}
				}
				return false;
			}

			// --- Les modules Blueprint ecrits A LA MAIN ----------------------------
			/// b1 : Debut -> Impulsion(Soi, (0, 5)).
			NkModuleBp ModuleSaut() {
				NkAssembleurBp a;
				const uint32 imp = static_cast<uint32>(a.Import("unkeny.corps.impulsion"));
				const uint32 f = a.Fonction("Debut");
				const uint32 soi = a.Registre(NkTypeBp::NK_ENTITE);
				const uint32 v = a.Registre(NkTypeBp::NK_VEC2);
				a.Ligne(7u);
				a.Emettre(NkOpBp::NK_SOI, soi);
				a.Emettre(NkOpBp::NK_CONST, v, a.ConstVec2(0.f, 5.f));
				const uint32 regs[] = {soi, v};
				a.Natif(imp, regs, 2u);
				a.Emettre(NkOpBp::NK_FIN);
				a.Entree(NK_UNK_EV_DEBUT, f);
				return a.module;
			}
			/// Un module qui « Afficher » un texte sous un evenement.
			void Afficher(NkAssembleurBp &a, uint32 imp, const char *texte) {
				const uint32 r = a.Registre(NkTypeBp::NK_TEXTE);
				a.Emettre(NkOpBp::NK_CONST, r, a.ConstTexte(texte));
				a.Natif(imp, &r, 1u);
			}
			/// La PORTE (p1) : Zone entree -> Si (soiEstLaZone ET autre s'appelle
			/// « Joueur ») -> porte = Entite par nom « Porte » ; Teleporter(porte,
			/// Position(porte) + (0, 2)) ; Afficher.
			NkModuleBp ModulePorte(bool importsPermutes = false) {
				NkAssembleurBp a;
				if (importsPermutes) {
					a.Import("unkeny.journal.afficher");
					a.Import("unkeny.transform.teleporter");
				}
				const uint32 nomEst = static_cast<uint32>(a.Import("unkeny.entite.nom_est"));
				const uint32 parNom = static_cast<uint32>(a.Import("unkeny.entite.par_nom"));
				const uint32 pos = static_cast<uint32>(a.Import("unkeny.transform.position"));
				const uint32 tel = static_cast<uint32>(a.Import("unkeny.transform.teleporter"));
				const uint32 aff = static_cast<uint32>(a.Import("unkeny.journal.afficher"));
				const uint32 f = a.Fonction("Zone entree");
				const uint32 autre = a.Registre(NkTypeBp::NK_ENTITE);
				const uint32 zone = a.Registre(NkTypeBp::NK_BOOLEEN);
				const uint32 nomJ = a.Registre(NkTypeBp::NK_TEXTE);
				const uint32 estJ = a.Registre(NkTypeBp::NK_BOOLEEN);
				const uint32 cond = a.Registre(NkTypeBp::NK_BOOLEEN);
				const uint32 nomP = a.Registre(NkTypeBp::NK_TEXTE);
				const uint32 porte = a.Registre(NkTypeBp::NK_ENTITE);
				const uint32 p = a.Registre(NkTypeBp::NK_VEC2);
				const uint32 dp = a.Registre(NkTypeBp::NK_VEC2);
				a.Emettre(NkOpBp::NK_ARG, autre, static_cast<uint32>(NkArgBp::NK_AUTRE));
				a.Emettre(NkOpBp::NK_ARG, zone, static_cast<uint32>(NkArgBp::NK_SOI_EST_ZONE));
				a.Emettre(NkOpBp::NK_CONST, nomJ, a.ConstTexte("Joueur"));
				const uint32 r1[] = {autre, nomJ, estJ};
				a.Natif(nomEst, r1, 3u);
				a.Emettre(NkOpBp::NK_ET, cond, zone, estJ);
				a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, cond, 0u);
				const uint32 aPatcher = a.Pc() - 1u;
				a.Emettre(NkOpBp::NK_CONST, nomP, a.ConstTexte("Porte"));
				const uint32 r2[] = {nomP, porte};
				a.Natif(parNom, r2, 2u);
				const uint32 r3[] = {porte, p};
				a.Natif(pos, r3, 2u);
				a.Emettre(NkOpBp::NK_CONST, dp, a.ConstVec2(0.f, 2.f));
				a.Emettre(NkOpBp::NK_ADD_V, p, p, dp);
				const uint32 r4[] = {porte, p};
				a.Natif(tel, r4, 2u);
				Afficher(a, aff, "La porte s'ouvre");
				a.Patcher(aPatcher, a.Pc());
				a.Emettre(NkOpBp::NK_FIN);
				a.Entree(NK_UNK_EV_ZONE_ENTREE, f);
				return a.module;
			}

			/// Une scene : un Joueur (ou un Caillou) qui tombe dans une zone a
			/// script, une Porte a cote. Rend le deplacement vertical de la porte.
			float32 EssaiPorte(const char *script, const char *tombeur, NkScripts2D &reg, NkString *journal = nullptr) {
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId sol = s.Creer("Sol", NkVec2f(0.f, -0.5f));
				Corps(s, sol, NkTypeCorps::NK_STATIQUE, false, NkVec2f(10.f, 0.5f));
				const ecs::NkEntityId zone = AvecScripts(s, "Zone", NkVec2f(0.f, 2.f), script);
				Corps(s, zone, NkTypeCorps::NK_STATIQUE, false, NkVec2f(2.f, 0.5f), true);
				const ecs::NkEntityId porte = s.Creer("Porte", NkVec2f(4.f, 1.f));
				Corps(s, porte, NkTypeCorps::NK_STATIQUE, false, NkVec2f(0.25f, 1.f));
				const ecs::NkEntityId j = s.Creer(tombeur, NkVec2f(0.f, 5.f));
				Corps(s, j, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f));
				const float32 y0 = s.Monde().Get<NkTransform2D>(porte)->position.y;
				for (int32 k = 0; k < 120; ++k) {
					s.Pas(1.f / 60.f);
				}
				const float32 y1 = s.Monde().Get<NkTransform2D>(porte)->position.y;
				if (journal != nullptr) {
					for (uint32 i = 0; i < h.Journal().Size(); ++i) {
						*journal += h.Journal()[i].texte;
						*journal += "\n";
					}
				}
				return y1 - y0;
			}

			/// La hauteur max d'une balle lancee par le script `ref` (b1 / c1).
			float32 Trajectoire(const char *ref, NkScripts2D &reg, NkVector<float32> *ys) {
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId b = AvecScripts(s, "Balle", NkVec2f(0.f, 0.f), ref);
				Corps(s, b, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f));
				float32 yMax = 0.f;
				for (int32 k = 0; k < 90; ++k) {
					s.Pas(1.f / 60.f);
					const float32 y = s.Monde().Get<NkTransform2D>(b)->position.y;
					yMax = y > yMax ? y : yMax;
					if (ys != nullptr) {
						ys->PushBack(y);
					}
				}
				return yMax;
			}
		} // namespace

		int32 NkUnkenyLancerBancScripts() {
			gEchecs = 0;
			gReussis = 0;
			std::printf("Unkeny — banc des SCRIPTS (hote, Blueprint, C++)\n");

			NkScripts2D reg;
			NkString err;
			const bool moduleOk = reg.EnregistrerModuleCpp(ModuleBanc(), &err);
			Temoin(moduleOk && reg.Trouver("cpp:BancTraceA") != 0u, "(c1) module statique : classe trouvee par nom", static_cast<float32>(reg.Nombre()));

			// (x1) (x2) l'ordre, deux scripts sur une entite, a 30 i/s
			{
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				AvecScripts(s, "Deux", NkVec2f(0.f, 0.f), "cpp:BancTraceA", "cpp:BancTraceB");
				s.Creer("Sans script", NkVec2f(1.f, 0.f));
				s.Pas(1.f / 30.f);
				const char *attendu = "A:D B:D A:P B:P A:P B:P A:T B:T ";
				std::printf("    trace : %s\n", gTrace);
				Temoin(std::strcmp(gTrace, attendu) == 0, "(x1) Debut d'abord, puis A puis B a chaque evenement", 0.f);
				Temoin(!Contient(gTrace, "?"), "(x2) Pas fixe x2 a dt 1/60, Tick x1 a dt 1/30", 0.f);
				Temoin(h.NbInstances() == 2u, "(x1n) l'entite sans NkScript2D n'a aucune instance", static_cast<float32>(h.NbInstances()));
			}

			// (x3) contacts, (x3n) dans le vide
			{
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId sol = AvecScripts(s, "Sol", NkVec2f(0.f, -0.5f), "cpp:BancContacts");
				Corps(s, sol, NkTypeCorps::NK_STATIQUE, false, NkVec2f(10.f, 0.5f));
				const ecs::NkEntityId b = AvecScripts(s, "Balle", NkVec2f(0.f, 3.f), "cpp:BancContacts");
				Corps(s, b, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f));
				for (int32 k = 0; k < 120; ++k) {
					s.Pas(1.f / 60.f);
				}
				std::printf("    trace : %s\n", gTrace);
				Temoin(Contient(gTrace, "CD:Balle>Sol") && Contient(gTrace, "CD:Sol>Balle"), "(x3) contact debut recu par la balle ET par le sol", 0.f);
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, v);
				Physique(v);
				NkHoteScripts2D hv;
				hv.Brancher(v, reg);
				const ecs::NkEntityId bv = AvecScripts(v, "Balle", NkVec2f(40.f, 100.f), "cpp:BancContacts");
				Corps(v, bv, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f));
				for (int32 k = 0; k < 60; ++k) {
					v.Pas(1.f / 60.f);
				}
				Temoin(gTrace[0] == '\0', "(x3n) balle dans le vide : aucun evenement", 0.f);
			}

			// (x4) zone
			{
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId zone = AvecScripts(s, "Zone", NkVec2f(0.f, 2.f), "cpp:BancContacts");
				Corps(s, zone, NkTypeCorps::NK_STATIQUE, false, NkVec2f(2.f, 0.5f), true);
				const ecs::NkEntityId b = AvecScripts(s, "Balle", NkVec2f(0.f, 5.f), "cpp:BancContacts");
				Corps(s, b, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f));
				for (int32 k = 0; k < 150; ++k) {
					s.Pas(1.f / 60.f);
				}
				std::printf("    trace : %s\n", gTrace);
				const char *e1 = std::strstr(gTrace, "ZE:Zone>Balle(z)");
				const char *s1 = std::strstr(gTrace, "ZS:Zone>Balle(z)");
				Temoin(e1 != nullptr && s1 != nullptr && e1 < s1 && Contient(gTrace, "ZE:Balle>Zone ") &&
						   Contient(gTrace, "ZS:Balle>Zone "),
					   "(x4) zone : entree puis sortie, soiEstLaZone pour la zone seule", 0.f);
			}

			// (x5) actions
			{
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				NkActions a;
				h.PoserActions(&a);
				AvecScripts(s, "Heros", NkVec2f(0.f, 0.f), "cpp:BancActions");
				for (int32 k = 0; k < 6; ++k) {
					a.NouvelleTrame();
					a.Poser(NK_ACTION_SAUTER, (k >= 1 && k <= 3) ? 1.f : 0.f);
					s.Pas(1.f / 60.f);
				}
				std::printf("    trace : %s\n", gTrace);
				Temoin(std::strcmp(gTrace, "P:Sauter R:Sauter ") == 0, "(x5) Sauter tenue 3 trames : pressee une fois, relachee une fois", 0.f);
				Temoin(!Contient(gTrace, "Avancer"), "(x5n) une action jamais pressee : aucun evenement", 0.f);
			}

			// (x6) un script se detruit
			{
				gTrace[0] = '\0';
				gTicks = 0;
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId m = AvecScripts(s, "Suicide", NkVec2f(0.f, 0.f), "cpp:BancSuicide");
				Corps(s, m, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f));
				AvecScripts(s, "Temoin", NkVec2f(3.f, 0.f), "cpp:BancTraceB");
				for (int32 k = 0; k < 4; ++k) {
					s.Pas(1.f / 60.f);
				}
				int32 ticksB = 0;
				for (const char *p = gTrace; (p = std::strstr(p, "B:T")) != nullptr; ++p) {
					++ticksB;
				}
				Temoin(gTicks == 1 && !s.Monde().IsAlive(m) && ticksB == 4,
					   "(x6) detruit dans Tick : plus d'appel, les autres tournent", static_cast<float32>(gTicks));
			}

			// (x7) photo : variables rendues, Debut rejoue
			{
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				AvecScripts(s, "Compteur", NkVec2f(0.f, 0.f), "cpp:BancCompteur");
				NK_BANC_SUR_TAS(NkScene::NkPhoto, photo);
				s.Photographier(photo);
				for (int32 k = 0; k < 10; ++k) {
					s.Pas(1.f / 60.f);
				}
				float32 n10 = -1.f;
				s.Monde().Query<NkScript2D>().ForEach([&](ecs::NkEntityId, NkScript2D &sc) {
					const NkVarScript *v = NkScriptVariable(sc, 0, "n");
					n10 = v != nullptr ? v->valeur.x : -2.f;
				});
				h.Arreter();
				s.Restaurer(photo);
				float32 n0 = -1.f;
				s.Monde().Query<NkScript2D>().ForEach([&](ecs::NkEntityId, NkScript2D &sc) {
					const NkVarScript *v = NkScriptVariable(sc, 0, "n");
					n0 = v != nullptr ? v->valeur.x : 0.f;
				});
				s.Pas(1.f / 60.f);
				Temoin(n10 == 10.f && n0 == 0.f && std::strcmp(gTrace, "K:D K:D ") == 0,
					   "(x7) Arreter rend la variable (10 -> 0), Debut rejoue", n10);
			}

			// (x8) .nkscene : par nom
			{
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				const ecs::NkEntityId e = s.Creer("Porteur", NkVec2f(0.f, 0.f));
				NkScript2D sc;
				NkScriptAjouter(sc, "Contenu/Scripts/Porte.nkbp");
				NkScriptAjouter(sc, "cpp:Joueur avec espace");
				sc.actifs[1] = false;
				NkScriptPoserVariable(sc, 0, "hauteur", NkTypeVarScript::NK_REEL, NkVec2f(2.5f, 0.f));
				NkScriptPoserVariable(sc, 1, "cap", NkTypeVarScript::NK_VEC2, NkVec2f(1.f, -0.5f));
				s.Monde().Add<NkScript2D>(e, sc);
				NkString json;
				NkSauverSceneJSON(s, json);
				NK_BANC_SUR_TAS(NkScene, t);
				NkString erreur;
				const bool lu = NkChargerSceneJSON(t, json.View(), static_cast<NkTextures2D *>(nullptr), &erreur);
				bool egal = false;
				t.Monde().Query<NkScript2D>().ForEach([&](ecs::NkEntityId, NkScript2D &r) {
					const NkVarScript *h = NkScriptVariable(r, 0, "hauteur");
					const NkVarScript *c = NkScriptVariable(r, 1, "cap");
					egal = r.nombre == 2 && std::strcmp(r.refs[0], "Contenu/Scripts/Porte.nkbp") == 0 &&
						   std::strcmp(r.refs[1], "cpp:Joueur avec espace") == 0 && r.actifs[0] && !r.actifs[1] && h != nullptr &&
						   h->valeur.x == 2.5f && c != nullptr && c->valeur.x == 1.f && c->valeur.y == -0.5f &&
						   c->type == static_cast<uint8>(NkTypeVarScript::NK_VEC2);
				});
				Temoin(lu && egal && Contient(json.CStr(), "\"NkScript2D\"") && Contient(json.CStr(), "hauteur"),
					   "(x8) .nkscene : refs et variables vont et reviennent par nom", 0.f);
			}

			// (x9) entite morte, NaN
			{
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId morte = s.Creer("Morte", NkVec2f(0.f, 0.f));
				gMorte = morte.Pack();
				s.Detruire(morte);
				gHostiles = 0;
				gNaN = -1;
				const ecs::NkEntityId e = AvecScripts(s, "Hostile", NkVec2f(0.f, 0.f), "cpp:BancHostile");
				Corps(s, e, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f), false, 0.f);
				for (int32 k = 0; k < 10; ++k) {
					s.Pas(1.f / 60.f);
				}
				const NkVec2f v = s.Vitesse(e);
				const bool fini = v.x == v.x && v.y == v.y;
				Temoin(gHostiles == 0 && gNaN == 0 && fini && h.NbRefus() > 0u, "(x9) entite morte et NaN : refus partout, solveur fini",
					   static_cast<float32>(gHostiles + gNaN));
			}

			// (x11) effets (R34)
			{
				gTicks = 0;
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId f = AvecScripts(s, "Feu", NkVec2f(0.f, 0.f), "cpp:BancEffet");
				NkEmetteur2D em = NkPresetEmetteur2D(NkPresetEffet2D::NK_FEU);
				s.Monde().Add<NkEmetteur2D>(f, em);
				s.Pas(1.f / 60.f); // Tick 1 : Arreter
				const bool eteint = !s.Monde().Get<NkEmetteur2D>(f)->actif;
				s.Effets().Vider();
				s.Pas(1.f / 60.f);
				s.Effets().Pas(s);
				const uint32 pendant = s.Effets().NbParticules();
				s.Pas(1.f / 60.f); // Tick 3 : Jouer
				for (int32 k = 0; k < 10; ++k) {
					s.Effets().Pas(s);
				}
				const bool rallume = s.Monde().Get<NkEmetteur2D>(f)->actif && s.Effets().NbParticules() > 0u;
				Temoin(eteint && pendant == 0u && rallume, "(x11) effet : Arreter l'eteint, Jouer le rallume et le rejoue",
					   static_cast<float32>(s.Effets().NbParticules()));
			}

			// (c2) exception
			{
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, reg);
				const ecs::NkEntityId f = AvecScripts(s, "Fautif", NkVec2f(0.f, 0.f), "cpp:BancFautif");
				AvecScripts(s, "Sain", NkVec2f(1.f, 0.f), "cpp:BancTraceB");
				for (int32 k = 0; k < 3; ++k) {
					s.Pas(1.f / 60.f);
				}
				Temoin(h.EnFaute(f, 0) && h.NbFautes() == 1u && Compter(gTrace, "B:T") == 3 && JournalContient(h, "exception"),
					   "(c2) exception rattrapee : instance en faute, la scene continue", static_cast<float32>(h.NbFautes()));
			}

			// (c3) ABI
			{
				NkScripts2D r;
				NkUnkModuleV1 m = *ModuleBanc();
				m.abiMajeure = NK_UNK_ABI_MAJEURE + 1u;
				NkString e;
				const bool majeure = r.EnregistrerModuleCpp(&m, &e);
				m.abiMajeure = NK_UNK_ABI_MAJEURE;
				m.abiMineure = NK_UNK_ABI_MINEURE + 3u;
				const bool mineure = r.EnregistrerModuleCpp(&m, nullptr);
				Temoin(!majeure && Contient(e.CStr(), "refuse") && mineure, "(c3) ABI majeure differente refusee, mineure acceptee", 0.f);
			}

			// (r1) rechargement a chaud
			{
				gTrace[0] = '\0';
				gRecharges = 0;
				NkScripts2D r;
				r.EnregistrerModuleCpp(ModuleRecharge(1));
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, r);
				const ecs::NkEntityId e = AvecScripts(s, "Recharge", NkVec2f(0.f, 0.f), "cpp:BancRecharge");
				NkScriptPoserVariable(*s.Monde().Get<NkScript2D>(e), 0, "exposee", NkTypeVarScript::NK_REEL, NkVec2f(7.f, 0.f));
				for (int32 k = 0; k < 5; ++k) {
					s.Pas(1.f / 60.f);
				}
				const int32 avant = static_cast<BancRechargeV1 *>(h.InstanceCpp(e, 0))->etat.n;
				r.EnregistrerModuleCpp(ModuleRecharge(2), nullptr, ModuleRecharge(1));
				const uint32 migrees = h.MigrerInstances();
				s.Pas(1.f / 60.f);
				const int32 apres = static_cast<BancRechargeV2 *>(h.InstanceCpp(e, 0))->etat.n;
				const NkVarScript *x = NkScriptVariable(*s.Monde().Get<NkScript2D>(e), 0, "exposee");
				Temoin(avant == 5 && migrees == 1u && apres == 15 && gRecharges == 1 && std::strcmp(gTrace, "R:D ") == 0 &&
						   x != nullptr && x->valeur.x == 7.f,
					   "(r1) recharge : etat prive 5 -> 15, variable gardee, Recharge sans Debut", static_cast<float32>(apres));
				r.EnregistrerModuleCpp(ModuleRecharge(3), nullptr, ModuleRecharge(2));
				h.MigrerInstances();
				s.Pas(1.f / 60.f);
				Temoin(h.InstanceCpp(e, 0) == nullptr && JournalContient(h, "disparu") && s.Monde().Has<NkScript2D>(e),
					   "(r1n) classe disparue : instance detruite, le journal le dit, le composant reste", 0.f);
			}

			// (b1) module ecrit a la main ; (c1) meme trajectoire en C++
			{
				const NkModuleBp m = ModuleSaut();
				NkVector<uint8> o1, o2;
				NkEcrireModuleBp(m, o1);
				NkModuleBp relu;
				NkString e;
				const bool lu = NkLireModuleBp(o1.Data(), o1.Size(), relu, &e);
				NkEcrireModuleBp(relu, o2);
				bool identique = lu && o1.Size() == o2.Size();
				for (uint32 i = 0; identique && i < o1.Size(); ++i) {
					identique = o1[i] == o2[i];
				}
				Temoin(identique, "(b1) module ecrit a la main : relu octet pour octet", static_cast<float32>(o1.Size()));
				NkScripts2D r;
				r.EnregistrerModuleCpp(ModuleBanc());
				r.EnregistrerBlueprint("Saut.nkbp", relu, &e);
				NkVector<float32> ysBp, ysCpp;
				const float32 hBp = Trajectoire("Saut.nkbp", r, &ysBp);
				const float32 hCpp = Trajectoire("cpp:BancLanceur", r, &ysCpp);
				const float32 attendu = 25.f / (2.f * 9.81f);
				Temoin(Absf(hBp - attendu) <= 0.05f * attendu, "(b1) Debut -> Impulsion (0,5) : v²/2g a 5 %", hBp);
				float32 ecart = 0.f;
				for (uint32 i = 0; i < ysBp.Size() && i < ysCpp.Size(); ++i) {
					ecart = Absf(ysBp[i] - ysCpp[i]) > ecart ? Absf(ysBp[i] - ysCpp[i]) : ecart;
				}
				Temoin(ysBp.Size() == ysCpp.Size() && ecart <= 0.001f, "(c1) C++ et Blueprint : meme trajectoire au millimetre", ecart);
			}

			// (b2) le verificateur
			{
				uint32 n = 0;
				const NkSignatureNatifBp *t = NkSignaturesBp(n);
				NkVector<int32> nat;
				NkRefusBp refus;
				const NkModuleBp base = ModuleSaut();
				Temoin(NkVerifierModuleBp(base, t, n, nat, refus), "(b2n) le module de b1 passe le verificateur", 0.f);
				struct Cas {
						const char *nom;
						const char *motif;
						NkModuleBp m;
				};
				Cas cas[5];
				cas[0].nom = "(b2) saut hors fonction : refuse, nomme";
				cas[0].motif = "saut";
				cas[0].m = base;
				cas[0].m.fonctions[0].code[cas[0].m.fonctions[0].code.Size() - 1u] = static_cast<uint32>(NkOpBp::NK_SAUT);
				cas[0].m.fonctions[0].code.PushBack(9999u);
				cas[1].nom = "(b2) registre hors cadre : refuse, nomme";
				cas[1].motif = "registre";
				cas[1].m = base;
				cas[1].m.fonctions[0].code[1] = 99u; // SOI r99
				cas[2].nom = "(b2) import inconnu : refuse, nomme";
				cas[2].motif = "inconnu";
				cas[2].m = base;
				cas[2].m.imports[0].nom = "unkeny.inexistant";
				cas[3].nom = "(b2) type discordant : refuse, nomme";
				cas[3].motif = "type";
				cas[3].m = base;
				cas[3].m.fonctions[0].registres[1] = NkTypeBp::NK_REEL; // vec2 attendu
				cas[4].nom = "(b2) constante hors table : refuse, nommee";
				cas[4].motif = "constante";
				cas[4].m = base;
				cas[4].m.fonctions[0].code[4] = 99u; // CONST r, k99
				for (int32 i = 0; i < 5; ++i) {
					const bool ok = NkVerifierModuleBp(cas[i].m, t, n, nat, refus);
					std::printf("    %s -> %s\n", cas[i].nom, refus.raison.CStr());
					Temoin(!ok && Contient(refus.raison.CStr(), cas[i].motif), cas[i].nom, static_cast<float32>(refus.pc));
				}
			}

			// (b3) boucle sans fin ; (b3n) 1000 tours
			{
				NkAssembleurBp a;
				const uint32 f = a.Fonction("Tick");
				a.Ligne(42u);
				a.Emettre(NkOpBp::NK_SAUT, 0u);
				a.Entree(NK_UNK_EV_TICK, f);
				NkAssembleurBp b;
				const uint32 g = b.Fonction("Tick");
				const uint32 i = b.Registre(NkTypeBp::NK_ENTIER);
				const uint32 un = b.Registre(NkTypeBp::NK_ENTIER);
				const uint32 mille = b.Registre(NkTypeBp::NK_ENTIER);
				const uint32 c = b.Registre(NkTypeBp::NK_BOOLEEN);
				NkValeurBp zero;
				const uint32 var = b.Variable("tours", NkTypeBp::NK_ENTIER, zero);
				b.Emettre(NkOpBp::NK_CONST, i, b.ConstEntier(0));
				b.Emettre(NkOpBp::NK_CONST, un, b.ConstEntier(1));
				b.Emettre(NkOpBp::NK_CONST, mille, b.ConstEntier(1000));
				const uint32 boucle = b.Pc();
				b.Emettre(NkOpBp::NK_ADD_I, i, i, un);
				b.Emettre(NkOpBp::NK_LT_I, c, i, mille);
				b.Emettre(NkOpBp::NK_NON, c, c);
				b.Emettre(NkOpBp::NK_SAUT_SI_FAUX, c, boucle);
				b.Emettre(NkOpBp::NK_ECRIRE_VAR, var, i);
				b.Emettre(NkOpBp::NK_FIN);
				b.Entree(NK_UNK_EV_TICK, g);
				NkScripts2D r;
				r.EnregistrerModuleCpp(ModuleBanc());
				NkString e1, e2;
				r.EnregistrerBlueprint("Boucle.nkbp", a.module, &e1);
				r.EnregistrerBlueprint("Mille.nkbp", b.module, &e2);
				gTrace[0] = '\0';
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, r);
				const ecs::NkEntityId fb = AvecScripts(s, "Boucle", NkVec2f(0.f, 0.f), "Boucle.nkbp");
				const ecs::NkEntityId fm = AvecScripts(s, "Mille", NkVec2f(1.f, 0.f), "Mille.nkbp");
				AvecScripts(s, "Sain", NkVec2f(2.f, 0.f), "cpp:BancTraceB");
				const auto t0 = std::chrono::steady_clock::now();
				for (int32 k = 0; k < 3; ++k) {
					s.Pas(1.f / 60.f);
				}
				const float64 ms = std::chrono::duration<float64, std::milli>(std::chrono::steady_clock::now() - t0).count();
				Temoin(h.EnFaute(fb, 0) && h.NbFautes() == 1u && JournalContient(h, "budget") && JournalContient(h, "noeud 42") &&
						   Compter(gTrace, "B:T") == 3,
					   "(b3) boucle sans fin coupee au budget, noeud designe, la scene continue", static_cast<float32>(ms));
				const NkVarScript *tours = NkScriptVariable(*s.Monde().Get<NkScript2D>(fm), 0, "tours");
				Temoin(!h.EnFaute(fm, 0) && tours != nullptr && tours->valeur.x == 1000.f, "(b3n) 1000 tours : la boucle finit",
					   tours != nullptr ? tours->valeur.x : -1.f);
			}

			// (b4) Si et Sequence
			{
				NkAssembleurBp a;
				const uint32 aff = static_cast<uint32>(a.Import("unkeny.journal.afficher"));
				const uint32 f = a.Fonction("Debut");
				const uint32 vrai = a.Registre(NkTypeBp::NK_BOOLEEN);
				Afficher(a, aff, "A");
				a.Emettre(NkOpBp::NK_CONST, vrai, a.ConstBooleen(true));
				a.Emettre(NkOpBp::NK_SAUT_SI_FAUX, vrai, 0u);
				const uint32 p = a.Pc() - 1u;
				// Sequence : Alors 0, puis Alors 1.
				Afficher(a, aff, "B1");
				Afficher(a, aff, "B2");
				const uint32 finVrai = a.Pc();
				a.Emettre(NkOpBp::NK_SAUT, 0u);
				const uint32 sautFin = a.Pc() - 1u;
				a.Patcher(p, a.Pc());
				Afficher(a, aff, "FAUX");
				a.Patcher(sautFin, a.Pc());
				a.Emettre(NkOpBp::NK_FIN);
				(void)finVrai;
				a.Entree(NK_UNK_EV_DEBUT, f);
				NkScripts2D r;
				NkString e;
				r.EnregistrerBlueprint("Si.nkbp", a.module, &e);
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, r);
				AvecScripts(s, "Si", NkVec2f(0.f, 0.f), "Si.nkbp");
				s.Pas(1.f / 60.f);
				NkString trace;
				for (uint32 i = 0; i < h.Journal().Size(); ++i) {
					trace += h.Journal()[i].texte;
					trace += ";";
				}
				std::printf("    journal : %s\n", trace.CStr());
				Temoin(trace == "[Si] A;[Si] B1;[Si] B2;", "(b4) Si et Sequence : A, B1, B2 (et pas FAUX)", 0.f);
			}

			// (b5) imports permutes ; (p1) la porte en Blueprint ; (p2) en C++
			{
				NkScripts2D r;
				r.EnregistrerModuleCpp(ModuleBanc());
				NkString e1, e2;
				r.EnregistrerBlueprint("Contenu/Scripts/Porte.nkbp", ModulePorte(false), &e1);
				r.EnregistrerBlueprint("Contenu/Scripts/PortePermutee.nkbp", ModulePorte(true), &e2);
				NkString j1, j2, j3;
				const float32 dBp = EssaiPorte("Contenu/Scripts/Porte.nkbp", "Joueur", r, &j1);
				const float32 dCaillou = EssaiPorte("Contenu/Scripts/Porte.nkbp", "Caillou", r, &j3);
				const float32 dPerm = EssaiPorte("Contenu/Scripts/PortePermutee.nkbp", "Joueur", r, &j2);
				const float32 dCpp = EssaiPorte("cpp:BancPorte", "Joueur", r);
				std::printf("    journal porte : %s", j1.CStr());
				Temoin(e1.Empty() && Absf(dBp - 2.f) < 1e-4f && Contient(j1.CStr(), "La porte s'ouvre"),
					   "(p1) Blueprint : le Joueur entre, la porte monte de 2 m", dBp);
				Temoin(Absf(dCaillou) < 1e-4f, "(p1n) un Caillou entre : la porte reste", dCaillou);
				Temoin(e2.Empty() && Absf(dPerm - dBp) < 1e-6f && j1 == j2, "(b5) imports permutes : meme execution", dPerm);
				Temoin(Absf(dCpp - 2.f) < 1e-4f, "(p2) la meme porte en C++ : meme resultat", dCpp);
			}

			// (b6) force en Pas fixe ; (b6n) sous Tick : refusee
			{
				auto Module = [](uint32 genre) {
					NkAssembleurBp a;
					const uint32 force = static_cast<uint32>(a.Import("unkeny.corps.force"));
					const uint32 f = a.Fonction("Pousser");
					const uint32 soi = a.Registre(NkTypeBp::NK_ENTITE);
					const uint32 v = a.Registre(NkTypeBp::NK_VEC2);
					a.Emettre(NkOpBp::NK_SOI, soi);
					a.Emettre(NkOpBp::NK_CONST, v, a.ConstVec2(2.f, 0.f));
					const uint32 regs[] = {soi, v};
					a.Natif(force, regs, 2u);
					a.Emettre(NkOpBp::NK_FIN);
					a.Entree(genre, f);
					return a.module;
				};
				NkScripts2D r;
				NkString e1, e2;
				r.EnregistrerBlueprint("Force.nkbp", Module(NK_UNK_EV_PAS_FIXE), &e1);
				r.EnregistrerBlueprint("ForceTick.nkbp", Module(NK_UNK_EV_TICK), &e2);
				NK_BANC_SUR_TAS(NkScene, s);
				Physique(s);
				NkHoteScripts2D h;
				h.Brancher(s, r);
				const ecs::NkEntityId b = AvecScripts(s, "Pousse", NkVec2f(0.f, 0.f), "Force.nkbp");
				Corps(s, b, NkTypeCorps::NK_DYNAMIQUE, true, NkVec2f(0.25f, 0.25f), false, 0.f);
				for (int32 k = 0; k < 60; ++k) {
					s.Pas(1.f / 60.f);
				}
				const float32 vx = s.Vitesse(b).x;
				Temoin(e1.Empty() && Absf(vx - 2.f) <= 0.02f, "(b6) force en Pas fixe : v = F/m.t a 1 %", vx);
				std::printf("    (b6n) %s\n", e2.CStr());
				Temoin(Contient(e2.CStr(), "pas permis"), "(b6n) la meme force sous Tick : refusee par le verificateur", 0.f);
			}

			// (x10) cout : 1000 entites, Tick Blueprint (position lue, variable ecrite)
			{
				NkAssembleurBp a;
				const uint32 pos = static_cast<uint32>(a.Import("unkeny.transform.position"));
				const uint32 f = a.Fonction("Tick");
				const uint32 soi = a.Registre(NkTypeBp::NK_ENTITE);
				const uint32 p = a.Registre(NkTypeBp::NK_VEC2);
				const uint32 x = a.Registre(NkTypeBp::NK_REEL);
				NkValeurBp zero;
				const uint32 var = a.Variable("x", NkTypeBp::NK_REEL, zero);
				a.Emettre(NkOpBp::NK_SOI, soi);
				const uint32 regs[] = {soi, p};
				a.Natif(pos, regs, 2u);
				a.Emettre(NkOpBp::NK_VX, x, p);
				a.Emettre(NkOpBp::NK_ECRIRE_VAR, var, x);
				a.Emettre(NkOpBp::NK_FIN);
				a.Entree(NK_UNK_EV_TICK, f);
				NkScripts2D r;
				r.EnregistrerBlueprint("Cout.nkbp", a.module);
				NK_BANC_SUR_TAS(NkScene, s);
				s.Init(NkSceneConfig());
				NkHoteScripts2D h;
				h.Brancher(s, r);
				for (int32 i = 0; i < 1000; ++i) {
					AvecScripts(s, "C", NkVec2f(static_cast<float32>(i), 0.f), "Cout.nkbp");
				}
				s.Pas(1.f / 60.f); // Debut et liaison
				const auto t0 = std::chrono::steady_clock::now();
				for (int32 k = 0; k < 20; ++k) {
					s.Pas(1.f / 60.f);
				}
				const float64 ms = std::chrono::duration<float64, std::milli>(std::chrono::steady_clock::now() - t0).count() / 20.0;
				std::printf("    (x10) 1000 entites, Tick Blueprint : %.3f ms par trame (Debug, publie sans seuil)\n", ms);
				Temoin(h.NbFautes() == 0u, "(x10) cout mesure et publie (1000 entites, aucune faute)", static_cast<float32>(ms));
			}

			std::printf("\n%s : %d reussis, %d echec\n", gEchecs == 0 ? "BANC SCRIPTS REUSSI" : "BANC SCRIPTS EN ECHEC",
						static_cast<int>(gReussis), static_cast<int>(gEchecs));
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
