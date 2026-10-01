//
// NogeDemoBanc.cpp
// =============================================================================
// Description :
//   `NogeDemo --selftest` : le banc SANS fenetre ni GPU. Il attache la VRAIE
//   couche du moteur (NkEngineLayer, sans NkApplication depuis le 26/09), la
//   nourrit d'evenements construits a la main (jamais la vraie souris ni le
//   vrai clavier) et fait tourner les MEMES fonctions que la fenetre
//   (NogeDemoJeu). Code de sortie 0 = tout tient.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (d1)  le texte d'entrees par defaut se lit sans refus et declare les 9
//         actions ; (d1n) une ligne fautive est REFUSEE, avec son numero
//   (d2)  la scene de depart : sol statique, joueur dynamique a rotation
//         figee, balle, camera, soleil -- 5 entites nommees
//   (d3)  la camera regarde son pivot ; fleche droite tenue 1 s : le lacet
//         tourne de 90 deg (vitesse 90 deg/s) ; (d3n) la souris SANS le bouton
//         droit ne tourne rien, AVEC elle tourne (l'accord de la liaison)
//   (d4)  le joueur tombe de 3 m et se POSE sur le sol (y = 0,5)
//   (d5)  la balle lui tombe dessus et reste posee SUR lui (y = 1 + 0,4) ;
//         (d5n) contre-epreuve : son masque de collision exclut le joueur, elle
//         le TRAVERSE et finit au sol (y = 0,4)
//   (d6)  « Deplacer » (D tenu 1 s) pousse le joueur de ~4 m vers la DROITE
//         de la camera, sans le faire basculer ; (d6n) le contexte « Jeu »
//         eteint, D ne pousse plus rien
//   (d7)  « Sauter » (Espace) : il monte au-dessus de 1,5 m puis se repose
//   (d8)  « Lacher » (E) : une caisse nait au-dessus de lui et tombe
//   (d9)  Sauver, deranger le monde, Charger : memes noms, meme nombre, memes
//         positions, couleur et formes relues ; les corps physiques sont
//         RECREES et le joueur reste pose ; (d9n) deux fichiers differents
//         donnent deux positions differentes -- la valeur vient du fichier
//   (d10) Charger SANS rendre les corps (physique = nul) laisse un corps
//         FANTOME sous le joueur recharge : il ne se repose plus au meme
//         endroit -- la contre-epreuve de ReleaseBodies
//   (d11) le fichier d'entrees : Sauter relie a J par TEXTE -- J saute,
//         Espace ne saute plus
//   (d12) un fichier de scene absent : Charger rend faux et le monde RESTE
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NogeDemoJeu.h"

#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/ECS/Systems/NkPhysicsSystem.h"
#include "Noge/Layers/NkEngineLayer.h"

#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogedemo {

		using namespace ecs;
		using namespace math;

		namespace {

			int32 gR = 0;
			int32 gE = 0;
			constexpr float32 kDt = 1.f / 60.f;

			void Temoin(bool ok, const char *quoi, float32 v) {
				(ok ? gR : gE)++;
				std::printf("  [%s] %s  (%.4f)\n", ok ? "OK" : "ECHEC", quoi, static_cast<double>(v));
			}

			/// Une image, comme NkApplication::Run : le pas fixe, la couche
			/// (entrees puis systemes), puis le jeu.
			void Image(NkEngineLayer &c, NkDemoEtat &e, int32 n = 1) {
				for (int32 i = 0; i < n; ++i) {
					c.OnFixedUpdate(kDt);
					c.OnUpdate(kDt);
					NogeDemoPas(e, c.GetWorld(), c.GetInput(), kDt);
				}
			}

			void Touche(NkEngineLayer &c, NkKey k, bool bas) {
				if (bas) {
					NkKeyPressEvent ev(k);
					(void)c.OnEvent(&ev);
				} else {
					NkKeyReleaseEvent ev(k);
					(void)c.OnEvent(&ev);
				}
			}

			NkVec3f Position(NkWorld &w, const char *nom) {
				const NkEntityId id = NogeDemoTrouver(w, nom);
				const NkTransform *tf = id.IsValid() ? w.Get<NkTransform>(id) : nullptr;
				return tf != nullptr ? tf->localPosition : NkVec3f{-999.f, -999.f, -999.f};
			}

			/// Une scene neuve, la physique rendue d'abord.
			void Neuve(NkEngineLayer &c, NkDemoEtat &e) {
				NogeDemoViderMonde(c.GetWorld(), c.GetPhysicsSystem());
				NogeDemoConstruireScene(c.GetWorld());
				e.camera = NkDemoCameraOrbite{};
				e.caissesLachees = 0;
				c.GetInput().ReleaseAll();
			}

			/// Lance la scene et laisse tout se poser (3 s).
			void Poser(NkEngineLayer &c, NkDemoEtat &e) {
				Image(c, e, 180);
			}

		} // namespace

		int32 NogeDemoLancerBanc() {
			gR = 0;
			gE = 0;
			std::printf("\nNogeDemo -- banc du premier resultat de Noge (sans fenetre)\n");

			NkEngineLayer *couche = new NkEngineLayer();
			couche->OnAttach();
			NkEngineLayer &c = *couche;
			NkWorld &w = c.GetWorld();
			NkInputMap &in = c.GetInput();
			NkDemoEtat e;

			// --- (d1) les entrees ---------------------------------------------
			{
				NkString err;
				const bool ok = NogeDemoDeclarerEntrees(in, e, NogeDemoTexteEntrees(), &err);
				if (!ok) {
					std::printf("      refus : %s\n", err.CStr());
				}
				Temoin(ok && in.ActionCount() == 9, "(d1) le texte d'entrees par defaut se lit et declare 9 actions",
					   static_cast<float32>(in.ActionCount()));
				NkInputMap autre;
				NkDemoEtat e2;
				NkString err2;
				const bool refuse = !NogeDemoDeclarerEntrees(autre, e2,
															 "action Sauter bouton\ncontexte Jeu\nlier Sauter Clavier:ESPACE\n", &err2);
				Temoin(refuse && err2.Find("3") != NkString::npos,
					   "(d1n) une ligne fautive est refusee, avec son numero (3)", refuse ? 1.f : 0.f);
				if (refuse) {
					std::printf("      refus lu : %s\n", err2.CStr());
				}
			}

			// --- (d2) la scene ---------------------------------------------------
			Neuve(c, e);
			{
				const NkEntityId sol = NogeDemoTrouver(w, kNomSol);
				const NkEntityId j = NogeDemoTrouver(w, kNomJoueur);
				const NkRigidbody3D *rs = sol.IsValid() ? w.Get<NkRigidbody3D>(sol) : nullptr;
				const NkRigidbody3D *rj = j.IsValid() ? w.Get<NkRigidbody3D>(j) : nullptr;
				const bool ok = NogeDemoCompterNommees(w) == 5 && rs && rs->bodyType == NkBodyType::Static && rj &&
								rj->bodyType == NkBodyType::Dynamic && rj->freezeRotX && rj->freezeRotY && rj->freezeRotZ &&
								NogeDemoTrouver(w, kNomBalle).IsValid() &&
								w.Has<NkCameraComponent>(NogeDemoTrouver(w, kNomCamera)) &&
								w.Has<NkLightComponent>(NogeDemoTrouver(w, kNomSoleil));
				Temoin(ok, "(d2) sol statique, joueur dynamique fige en rotation, balle, camera, soleil",
					   static_cast<float32>(NogeDemoCompterNommees(w)));
			}

			// --- (d3) la camera ----------------------------------------------------
			{
				Image(c, e, 1);
				const NkEntityId cam = NogeDemoTrouver(w, kNomCamera);
				const NkTransform *tf = w.Get<NkTransform>(cam);
				const NkVec3f avant = tf->GetWorldForward();
				NkVec3f versPivot = e.camera.pivot - tf->localPosition;
				versPivot = versPivot * (1.f / NkSqrt(versPivot.Dot(versPivot)));
				const float32 alignement = avant.Dot(versPivot);
				Temoin(alignement > 0.999f, "(d3) la camera regarde son pivot (cos de l'ecart)", alignement);

				const float32 lacet0 = e.camera.lacetDeg;
				Touche(c, NkKey::NK_RIGHT, true);
				Image(c, e, 60);
				Touche(c, NkKey::NK_RIGHT, false);
				Image(c, e, 1);
				const float32 tour = e.camera.lacetDeg - lacet0;
				Temoin(NkFabs(tour - 90.f) < 3.f, "(d3) fleche droite tenue 1 s : le lacet tourne de 90 deg", tour);

				// (d3n) l'accord : la souris seule ne tourne rien.
				const float32 l1 = e.camera.lacetDeg;
				{
					NkMouseMoveEvent mv(400, 300, 400, 300, 60, 0);
					(void)c.OnEvent(&mv);
				}
				Image(c, e, 1);
				const float32 sansBouton = e.camera.lacetDeg - l1;
				{
					NkMouseButtonPressEvent bas(NkMouseButton::NK_MB_RIGHT, 400, 300);
					(void)c.OnEvent(&bas);
					NkMouseMoveEvent mv(460, 300, 460, 300, 60, 0);
					(void)c.OnEvent(&mv);
				}
				Image(c, e, 1);
				const float32 avecBouton = e.camera.lacetDeg - l1;
				{
					NkMouseButtonReleaseEvent haut(NkMouseButton::NK_MB_RIGHT, 460, 300);
					(void)c.OnEvent(&haut);
				}
				Image(c, e, 1);
				// 60 px * 0,005 * 220 deg = -66 deg (glisser a droite tourne la vue a gauche).
				Temoin(NkFabs(sansBouton) < 1e-4f && NkFabs(avecBouton + 66.f) < 1.f,
					   "(d3n) souris sans bouton droit : rien ; avec : -66 deg", avecBouton);
			}

			// --- (d4) (d5) la chute et la collision ---------------------------------
			Neuve(c, e);
			{
				float32 minBalle = 1e9f;
				float32 maxJoueur = -1e9f;
				for (int32 i = 0; i < 180; ++i) {
					Image(c, e, 1);
					const float32 yb = Position(w, kNomBalle).y;
					minBalle = yb < minBalle ? yb : minBalle;
					const float32 yj = Position(w, kNomJoueur).y;
					maxJoueur = yj > maxJoueur ? yj : maxJoueur;
				}
				const float32 yj = Position(w, kNomJoueur).y;
				Temoin(NkFabs(yj - 0.5f) < 0.05f && maxJoueur > 2.9f, "(d4) le joueur tombe de 3 m et se pose sur le sol (y)",
					   yj);
				const float32 yb = Position(w, kNomBalle).y;
				Temoin(NkFabs(yb - (kJoueurCote + kBalleRayon)) < 0.1f && minBalle > 1.2f,
					   "(d5) la balle tombe SUR le joueur et y reste (y)", yb);
				// La rotation figee : pousse par la balle, il n'a pas bascule.
				const NkEntityId j = NogeDemoTrouver(w, kNomJoueur);
				const float32 qw = NkFabs(w.Get<NkTransform>(j)->localRotation.w);
				Temoin(qw > 0.9999f, "(d4) le joueur n'a pas tourne (|q.w|)", qw);
			}
			Neuve(c, e);
			{
				// (d5n) la balle passe dans une couche a elle (2), et les DEUX masques
				// excluent l'autre -- que le filtre du socle soit a sens unique ou
				// double, la paire joueur / balle est coupee. Le sol (couche 0, masque
				// plein) reste vu des deux.
				constexpr uint32 kCoucheBalleSeule = 2;
				const NkEntityId b = NogeDemoTrouver(w, kNomBalle);
				w.Get<NkCollider3D>(b)->layer = kCoucheBalleSeule;
				w.Get<NkCollider3D>(b)->layerMask = ~(1u << kCoucheJoueur);
				const NkEntityId j = NogeDemoTrouver(w, kNomJoueur);
				w.Get<NkCollider3D>(j)->layerMask = ~(1u << kCoucheBalleSeule);
				Poser(c, e);
				const float32 yb = Position(w, kNomBalle).y;
				Temoin(NkFabs(yb - kBalleRayon) < 0.1f, "(d5n) contre-epreuve : sans collision joueur/balle, elle finit au sol",
					   yb);
			}

			// --- (d6) pousser le joueur ------------------------------------------------
			Neuve(c, e);
			Poser(c, e);
			{
				const NkVec3f p0 = Position(w, kNomJoueur);
				Touche(c, NkKey::NK_D, true);
				Image(c, e, 60);
				Touche(c, NkKey::NK_D, false);
				Image(c, e, 30);
				const NkVec3f p1 = Position(w, kNomJoueur);
				const float32 lacet = e.camera.lacetDeg * (3.14159265f / 180.f);
				const NkVec3f droite{NkCos(lacet), 0.f, -NkSin(lacet)};
				const NkVec3f d = p1 - p0;
				const float32 leLong = d.Dot(droite);
				const NkVec3f travers = d - droite * leLong;
				const NkEntityId j = NogeDemoTrouver(w, kNomJoueur);
				const float32 qw = NkFabs(w.Get<NkTransform>(j)->localRotation.w);
				Temoin(NkFabs(leLong - 4.f) < 0.5f && NkSqrt(travers.x * travers.x + travers.z * travers.z) < 0.3f &&
						   qw > 0.9999f,
					   "(d6) D tenu 1 s : ~4 m vers la droite de la camera, sans basculer", leLong);

				// (d6n) le contexte eteint : D ne pousse plus.
				in.SetContextEnabled("Jeu", false);
				const NkVec3f p2 = Position(w, kNomJoueur);
				Touche(c, NkKey::NK_D, true);
				Image(c, e, 60);
				Touche(c, NkKey::NK_D, false);
				Image(c, e, 10);
				const NkVec3f p3 = Position(w, kNomJoueur);
				in.SetContextEnabled("Jeu", true);
				const NkVec3f d2 = p3 - p2;
				const float32 bouge = NkSqrt(d2.x * d2.x + d2.z * d2.z);
				Temoin(bouge < 0.05f, "(d6n) contre-epreuve : contexte « Jeu » eteint, D ne pousse rien (m)", bouge);
			}

			// --- (d7) sauter ----------------------------------------------------------
			{
				Touche(c, NkKey::NK_SPACE, true);
				Image(c, e, 1);
				Touche(c, NkKey::NK_SPACE, false);
				float32 yMax = 0.f;
				for (int32 i = 0; i < 120; ++i) {
					Image(c, e, 1);
					const float32 y = Position(w, kNomJoueur).y;
					yMax = y > yMax ? y : yMax;
				}
				const float32 y = Position(w, kNomJoueur).y;
				Temoin(yMax > 1.5f && NkFabs(y - 0.5f) < 0.05f, "(d7) Espace : le joueur monte (max) puis se repose", yMax);
			}

			// --- (d8) lacher une caisse ------------------------------------------------
			{
				const int32 avant = NogeDemoCompterNommees(w);
				Touche(c, NkKey::NK_E, true);
				Image(c, e, 1);
				Touche(c, NkKey::NK_E, false);
				const float32 y0 = Position(w, "Caisse_00").y;
				Image(c, e, 180);
				const float32 y1 = Position(w, "Caisse_00").y;
				Temoin(NogeDemoCompterNommees(w) == avant + 1 && y0 > 5.f && y1 < 3.f,
					   "(d8) E : une caisse nait en hauteur et tombe (y final)", y1);
			}

			// --- (d9) sauver / recharger ---------------------------------------------
			const char *fichierA = "Build/NogeDemo/banc_A.nkscene";
			const char *fichierB = "Build/NogeDemo/banc_B.nkscene";
			if (!NkDirectory::Exists("Build/NogeDemo")) {
				(void)NkDirectory::CreateRecursive("Build/NogeDemo");
			}
			{
				const int32 n = NogeDemoCompterNommees(w);
				const NkVec3f pj = Position(w, kNomJoueur);
				const NkVec3f pc = Position(w, "Caisse_00");
				const bool sauve = NogeDemoSauver(w, fichierA);
				// Deranger : pousser le joueur loin, retirer la balle.
				Touche(c, NkKey::NK_W, true);
				Image(c, e, 40);
				Touche(c, NkKey::NK_W, false);
				Image(c, e, 20);
				const NkEntityId b = NogeDemoTrouver(w, kNomBalle);
				if (b.IsValid()) {
					c.GetPhysicsSystem()->ReleaseBodies(w); // les corps seront recrees au pas suivant
					w.Destroy(b);
				}
				Image(c, e, 2);
				const NkVec3f derange = Position(w, kNomJoueur);
				const uint32 corps0 = c.GetPhysicsSystem()->BodiesCreated();
				const bool charge = NogeDemoCharger(w, c.GetPhysicsSystem(), fichierA, e);
				const NkVec3f pj2 = Position(w, kNomJoueur);
				const NkVec3f pc2 = Position(w, "Caisse_00");
				const NkVec3f dj = pj2 - pj;
				const NkVec3f dc = pc2 - pc;
				Temoin(sauve && charge && NogeDemoCompterNommees(w) == n && NogeDemoTrouver(w, kNomBalle).IsValid() &&
						   NkSqrt(dj.Dot(dj)) < 1e-4f && NkSqrt(dc.Dot(dc)) < 1e-4f &&
						   NkFabs(derange.z - pj.z) + NkFabs(derange.x - pj.x) > 1.f,
					   "(d9) sauver, deranger, charger : memes entites, memes positions", NkSqrt(dj.Dot(dj)));

				// Les composants relus : couleur, maillage, forme, lumiere.
				const NkEntityId j = NogeDemoTrouver(w, kNomJoueur);
				const NkMaterialComponent *mat = w.Get<NkMaterialComponent>(j);
				const NkMeshComponent *mesh = w.Get<NkMeshComponent>(j);
				const NkCollider3D *col = w.Get<NkCollider3D>(j);
				const NkRigidbody3D *rb = w.Get<NkRigidbody3D>(j);
				const NkLightComponent *l = w.Get<NkLightComponent>(NogeDemoTrouver(w, kNomSoleil));
				const NkCameraComponent *cam = w.Get<NkCameraComponent>(NogeDemoTrouver(w, kNomCamera));
				const bool composants = mat && mat->slotCount == 1 && NkFabs(mat->slots[0].albedo.r - 0.86f) < 1e-4f &&
										mesh && mesh->meshPath == NkString("primitive:cube") && mesh->meshHandle == 0 && col &&
										col->shape == NkCollider3DShape::Box && col->layer == kCoucheJoueur &&
										col->physicsBodyId == 0 && rb && rb->bodyType == NkBodyType::Dynamic &&
										rb->freezeRotY && l && l->type == NkLightType::Directional &&
										NkFabs(l->intensity - 3.f) < 1e-4f && cam && cam->priority == 10;
				Temoin(composants, "(d9) couleur, maillage, forme, corps, lumiere et camera relus", composants ? 1.f : 0.f);

				// Les corps recrees, et le joueur reste pose.
				Image(c, e, 90);
				const uint32 recrees = c.GetPhysicsSystem()->BodiesCreated() - corps0;
				const NkVec3f pj3 = Position(w, kNomJoueur);
				const NkVec3f d3 = pj3 - pj;
				Temoin(recrees == static_cast<uint32>(n) - 2u && NkSqrt(d3.Dot(d3)) < 0.05f,
					   "(d9) apres rechargement : un corps par collisionneur recree, le joueur reste pose (corps)",
					   static_cast<float32>(recrees));

				// (d9n) un second fichier, une autre position.
				Touche(c, NkKey::NK_A, true);
				Image(c, e, 30);
				Touche(c, NkKey::NK_A, false);
				Image(c, e, 30);
				const NkVec3f pB = Position(w, kNomJoueur);
				const bool sauveB = NogeDemoSauver(w, fichierB);
				const bool chA = NogeDemoCharger(w, c.GetPhysicsSystem(), fichierA, e);
				const NkVec3f lueA = Position(w, kNomJoueur);
				const bool chB = NogeDemoCharger(w, c.GetPhysicsSystem(), fichierB, e);
				const NkVec3f lueB = Position(w, kNomJoueur);
				const NkVec3f ea = lueA - pj;
				const NkVec3f eb = lueB - pB;
				const NkVec3f ab = pB - pj;
				Temoin(sauveB && chA && chB && NkSqrt(ea.Dot(ea)) < 1e-4f && NkSqrt(eb.Dot(eb)) < 1e-4f &&
						   NkSqrt(ab.Dot(ab)) > 1.f,
					   "(d9n) deux fichiers, deux positions : la valeur vient du FICHIER (ecart A-B, m)",
					   NkSqrt(ab.Dot(ab)));
			}

			// --- (d10) le fantome : charger sans rendre les corps ---------------------
			{
				(void)NogeDemoCharger(w, c.GetPhysicsSystem(), fichierA, e);
				Image(c, e, 60);
				const NkVec3f ref = Position(w, kNomJoueur);
				(void)NogeDemoCharger(w, nullptr, fichierA, e); // SANS ReleaseBodies
				Image(c, e, 90);
				const NkVec3f fantome = Position(w, kNomJoueur);
				const NkVec3f d = fantome - ref;
				const float32 ecart = NkSqrt(d.Dot(d));
				Temoin(ecart > 0.2f, "(d10) contre-epreuve : sans ReleaseBodies, un corps fantome deplace le joueur (m)",
					   ecart);
				// Remise au propre : TOUS les corps (fantomes compris) sont rendus
				// en reprenant une couche neuve plus bas.
			}

			// --- (d11) le fichier d'entrees ---------------------------------------------
			couche->OnDetach();
			delete couche;
			couche = new NkEngineLayer();
			couche->OnAttach();
			{
				NkEngineLayer &c2 = *couche;
				NkDemoEtat e2;
				NkString texte(NogeDemoTexteEntrees());
				const char *ancien = "lier Sauter Key:SPACE";
				const NkString::SizeType k = texte.Find(ancien);
				if (k != NkString::npos) {
					texte.Replace(k, static_cast<NkString::SizeType>(std::strlen(ancien)), "lier Sauter Key:J");
				}
				const char *fichierEntrees = "Build/NogeDemo/banc_entrees.nkinput";
				const bool ecrit = NkFile::WriteAllText(fichierEntrees, texte.CStr());
				const NkInputMapReport r = c2.LoadInputFile(fichierEntrees);
				NkString err;
				const bool ok = ecrit && r.Ok() &&
								NogeDemoDeclarerEntrees(c2.GetInput(), e2, NkFile::ReadAllText(fichierEntrees).CStr(), &err);
				NogeDemoConstruireScene(c2.GetWorld());
				Image(c2, e2, 150);
				Touche(c2, NkKey::NK_SPACE, true);
				Image(c2, e2, 1);
				Touche(c2, NkKey::NK_SPACE, false);
				float32 maxEspace = 0.f;
				for (int32 i = 0; i < 30; ++i) {
					Image(c2, e2, 1);
					const float32 y = Position(c2.GetWorld(), kNomJoueur).y;
					maxEspace = y > maxEspace ? y : maxEspace;
				}
				Image(c2, e2, 60);
				Touche(c2, NkKey::NK_J, true);
				Image(c2, e2, 1);
				Touche(c2, NkKey::NK_J, false);
				float32 maxJ = 0.f;
				for (int32 i = 0; i < 30; ++i) {
					Image(c2, e2, 1);
					const float32 y = Position(c2.GetWorld(), kNomJoueur).y;
					maxJ = y > maxJ ? y : maxJ;
				}
				Temoin(ok && maxEspace < 0.6f && maxJ > 1.2f,
					   "(d11) fichier d'entrees : Sauter relie a J -- J saute (max), Espace non", maxJ);

				// --- (d12) un fichier absent : le monde reste --------------------
				const int32 n = NogeDemoCompterNommees(c2.GetWorld());
				const bool charge = NogeDemoCharger(c2.GetWorld(), c2.GetPhysicsSystem(),
													"Build/NogeDemo/ce_fichier_n_existe_pas.nkscene", e2);
				Temoin(!charge && NogeDemoCompterNommees(c2.GetWorld()) == n,
					   "(d12) fichier absent : Charger rend faux et le monde reste (entites)",
					   static_cast<float32>(NogeDemoCompterNommees(c2.GetWorld())));
			}
			couche->OnDetach();
			delete couche;

			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC NOGE REUSSI" : "BANC NOGE EN ECHEC", gR, gE,
						gE > 1 ? "s" : "");
			std::fflush(stdout);
			return gE == 0 ? 0 : 1;
		}

	} // namespace nogedemo
} // namespace nkentseu
