// -----------------------------------------------------------------------------
// @File    NKScenaBanc.cpp
// @Brief   LE BANC DE NKSCENA (`NKScena --selftest`) : le modele, l'aller-retour
//          .nkseq, l'evaluation d'une cle a un instant donne, la camera du plan,
//          la lecture, et le rendu de N images SANS FENETRE -- chacun avec sa
//          CONTRE-EPREUVE (une mutation ou un negatif qui doit faire ECHOUER le
//          critere : un critere qui ne sait pas echouer ne prouve rien).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LES ATTENDUS SONT ECRITS AVANT LA MESURE (la regle de NkSequenceCheck) :
//   l'exemple (NkScenaModele::Exemple, 4 s a 24 ips) pose le joueur a x = -3 a
//   0 s et x = +3 a 4 s, en LINEAIRE : a 1 s, x = -1,5 exactement ; a 2 s, son
//   lacet vaut 45 degres (0 -> 90). Le plan « Caméra » couvre [0, 4] s.
// -----------------------------------------------------------------------------
// ⚠️ NKRHI et NKRenderer AVANT Noge (voir NKScenaRendu.cpp).
#include "NKScena/NKScenaRendu.h"
#include "NKScena/NKScenaModele.h"
#include "NKScena/NKScenaPont.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"
#include "Noge/Sequencer/NkSequencer.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nkscena {

		namespace {
			int32 gReussis = 0;
			int32 gEchecs = 0;
			int32 gIgnores = 0;

			void Temoin(bool ok, const char *quoi, const char *detail = "") {
				(ok ? gReussis : gEchecs)++;
				std::printf("  [%s] %s%s%s\n", ok ? "OK" : "ECHEC", quoi, detail[0] != '\0' ? "  — " : "", detail);
				std::fflush(stdout);
			}

			/// Une CONTRE-EPREUVE : `critere` applique a l'etat MUTE doit rendre faux.
			void ContreEpreuve(bool critereSurMutation, const char *quoi, const char *detail = "") {
				(!critereSurMutation ? gReussis : gEchecs)++;
				std::printf("  [%s] %s : %s%s%s\n", !critereSurMutation ? "OK" : "ECHEC", quoi,
							!critereSurMutation ? "ECHEC vu" : "la mutation PASSE (le critere est aveugle)",
							detail[0] != '\0' ? "  — " : "", detail);
				std::fflush(stdout);
			}

			bool Proche(float32 a, float32 b, float32 tol) {
				return std::fabs(a - b) <= tol;
			}

			ecs::NkTransform *Tf(NkScenaModele &m, const char *nom) {
				const ecs::NkEntityId e = m.Entite(nom);
				return e.IsValid() ? m.scene.Monde().Get<ecs::NkTransform>(e) : nullptr;
			}

			/// LE CRITERE d'evaluation de s2 : a 1 s, le joueur est a x = -1,5.
			bool JoueurA(NkScenaModele &m, float32 t, float32 attendu, float32 *lu = nullptr) {
				m.Evaluer(t);
				const ecs::NkTransform *tf = Tf(m, "Joueur");
				if (lu != nullptr) {
					*lu = tf != nullptr ? tf->localPosition.x : -999.f;
				}
				return tf != nullptr && Proche(tf->localPosition.x, attendu, 1e-4f);
			}

			bool MemesOctets(const char *a, const char *b, int64 *ecart = nullptr) {
				const NkVector<uint8> x = NkFile::ReadAllBytes(a);
				const NkVector<uint8> y = NkFile::ReadAllBytes(b);
				if (ecart != nullptr) {
					*ecart = -1;
				}
				if (x.Size() != y.Size() || x.Empty()) {
					if (ecart != nullptr) {
						*ecart = (int64)(x.Size() < y.Size() ? x.Size() : y.Size());
					}
					return false;
				}
				for (uint32 i = 0; i < (uint32)x.Size(); ++i) {
					if (x[i] != y[i]) {
						if (ecart != nullptr) {
							*ecart = (int64)i;
						}
						return false;
					}
				}
				return true;
			}

			uint32 CompterFichiers(const char *dossier, const char *prefixe, int32 maximum) {
				uint32 n = 0;
				for (int32 i = 1; i <= maximum; ++i) {
					char chemin[600];
					std::snprintf(chemin, sizeof(chemin), "%s/%s_%04d.png", dossier, prefixe, i);
					n += NkFile::Exists(chemin) ? 1u : 0u;
				}
				return n;
			}
		} // namespace

		int32 NkScenaLancerBanc() {
			gReussis = 0;
			gEchecs = 0;
			gIgnores = 0;
			std::printf("\nNKScena — banc (le modèle, l'aller-retour .nkseq, l'évaluation, le rendu sans fenêtre)\n");
			(void)NkDirectory::CreateRecursive("Build/NKScena/Banc");

			static ecs::NkWorld monde;
			NkScenaModele m;
			m.Brancher(&monde, nullptr);

			// ── s1 LE MODELE ──────────────────────────────────────────────────
			std::printf("\ns1 — le modèle : la scène de NogeDemo, les pistes, les plans\n");
			const bool demo = m.SceneDemo();
			Temoin(demo && NkFile::Exists(NkScenaModele::kSceneDemo), "(s1) la scène de NogeDemo est écrite (.nkscene3d)",
				   NkScenaModele::kSceneDemo);
			Temoin(m.Entite("Joueur").IsValid() && m.Entite("Caméra").IsValid(), "(s1b) elle a son joueur et sa caméra");
			m.NouvelleSequence(4.f, 24.f);
			Temoin(m.AjouterPisteTransform("Joueur") && m.frise.tracks.Size() == 3u,
				   "(s1c) piste de transformation : 3 pistes de frise (position, rotation, échelle)");
			Temoin(!m.AjouterPisteTransform("Joueur") && m.frise.tracks.Size() == 3u,
				   "(s1d) contre-épreuve : la même entité deux fois est refusée");
			Temoin(!m.AjouterPisteTransform("Fantôme"), "(s1e) contre-épreuve : une entité absente est refusée");
			const nk_uint64 plan = m.AjouterPlan("Caméra", 0.f, 4.f);
			const editorkit::NkTimelineTrack *pp = m.frise.Track(m.PistePlans());
			Temoin(plan != 0 && pp != nullptr && pp->clips.Size() == 1u && m.frise.tracks[0].id == m.PistePlans(),
				   "(s1f) un plan « Caméra » de 0 à 4 s, sur la piste des plans (en tête)");
			Temoin(m.AjouterPlan("Soleil", 0.f, 1.f) == 0, "(s1g) contre-épreuve : un plan sur une entité sans caméra est refusé");

			// ── s2 L'EVALUATION D'UNE CLE A UN INSTANT DONNE ──────────────────
			std::printf("\ns2 — l'évaluation : attendu ÉCRIT AVANT, x(1 s) = -1,5 ; lacet(2 s) = 45°\n");
			Temoin(m.Exemple(4.f) && m.NombreCles() == 12u, "(s2) l'exemple : 2 clés x 3 pistes x 2 entités = 12 clés");
			float32 lu = 0.f;
			Temoin(JoueurA(m, 1.f, -1.5f, &lu), "(s2b) à 1 s, le joueur est à x = -1,5",
				   NkString::Format("lu %.5f", static_cast<double>(lu)).CStr());
			{
				m.Evaluer(2.f);
				const ecs::NkTransform *tj = Tf(m, "Joueur");
				const math::NkQuatf q = NkSequenceRotationFromDegrees(0.f, 45.f, 0.f);
				const float32 dot = tj != nullptr ? std::fabs(tj->localRotation.x * q.x + tj->localRotation.y * q.y +
															   tj->localRotation.z * q.z + tj->localRotation.w * q.w)
												  : 0.f;
				Temoin(tj != nullptr && dot > 0.9999f && Proche(tj->localPosition.z, 0.75f, 1e-4f),
					   "(s2c) à 2 s : lacet 45° (canaux localRotation, format v2) et z = 0,75",
					   NkString::Format("|q.q'| = %.6f", static_cast<double>(dot)).CStr());
			}
			{
				// LA MUTATION : la cle de fin passe de x = +3 a x = +5. Le critere
				// (x(1 s) = -1,5) doit alors ECHOUER, sinon il ne regarde rien.
				editorkit::NkTimelineTrack *pos = nullptr;
				for (uint32 i = 0; i < (uint32)m.frise.tracks.Size(); ++i) {
					if (m.frise.tracks[i].object == NkString("Joueur") && m.frise.tracks[i].property == NkString(kNkScenaPosition)) {
						pos = &m.frise.tracks[i];
					}
				}
				const float32 v[4] = {5.f, 0.5f, 1.5f, 0.f};
				if (pos != nullptr) {
					(void)m.frise.SetKey(pos->id, 4.f, v);
				}
				ContreEpreuve(JoueurA(m, 1.f, -1.5f, &lu), "(s2d) mutation de la clé de fin (x = +5)",
							  NkString::Format("lu %.5f", static_cast<double>(lu)).CStr());
				(void)m.frise.Undo();
				Temoin(JoueurA(m, 1.f, -1.5f, &lu), "(s2e) Ctrl+Z de la frise : la séquence rejoue x = -1,5");
			}
			{
				const ecs::NkEntityId cam = m.Entite("Caméra");
				Temoin(m.CameraDuPlan(2.f) == cam && cam.IsValid(), "(s2f) à 2 s, la caméra du plan est « Caméra »");
				Temoin(!m.CameraDuPlan(4.5f).IsValid(), "(s2g) contre-épreuve : après le plan (4,5 s), aucune caméra");
				m.vueCamera = true;
				m.Evaluer(2.f);
				const ecs::NkCameraComponent *c = m.scene.Monde().Get<ecs::NkCameraComponent>(cam);
				Temoin(c != nullptr && c->priority > 1000, "(s2h) vue « caméra du plan » : elle passe devant la caméra d'édition",
					   NkString::Format("priorité %d", c != nullptr ? c->priority : 0).CStr());
				m.vueCamera = false;
				m.Evaluer(2.f);
				Temoin(c != nullptr && c->priority == 10, "(s2i) vue libre : la caméra retrouve SA priorité (10)",
					   NkString::Format("priorité %d", c != nullptr ? c->priority : 0).CStr());
			}
			{
				// La camera a-t-elle bien ete posee en DEGRES a 0 s (-20, 35, 0) ?
				m.Evaluer(0.f);
				float32 v[4];
				const editorkit::NkTimelineTrack *rot = nullptr;
				for (uint32 i = 0; i < (uint32)m.frise.tracks.Size(); ++i) {
					if (m.frise.tracks[i].object == NkString("Caméra") && m.frise.tracks[i].property == NkString(kNkScenaRotation)) {
						rot = &m.frise.tracks[i];
					}
				}
				const bool lue = rot != nullptr && m.LireVivant(*rot, v);
				Temoin(lue && Proche(v[0], -20.f, 0.05f) && Proche(v[1], 35.f, 0.05f) && Proche(v[2], 0.f, 0.05f),
					   "(s2j) la rotation relue en degrés (tangage, lacet, roulis) = celle posée",
					   NkString::Format("(%.3f, %.3f, %.3f)", static_cast<double>(v[0]), static_cast<double>(v[1]),
										static_cast<double>(v[2]))
						   .CStr());
			}

			{
				// LA CONVENTION DES CANAUX DE ROTATION : degres -> quaternion -> degres
				// rend les MEMES angles (tangage dans ]-90, 90[, cap quelconque), et le
				// quaternion est bien celui de Nogee : RotateY(lacet) * RotateX(tangage).
				const float32 kAngles[5][3] = {{-20.f, 35.f, 0.f}, {10.f, 120.f, 5.f}, {-45.f, -150.f, 30.f}, {0.f, 179.f, 0.f}, {80.f, -90.f, -60.f}};
				float32 pire = 0.f;
				for (const auto &a : kAngles) {
					float32 p, y, r;
					NkSequenceDegreesFromRotation(NkSequenceRotationFromDegrees(a[0], a[1], a[2]), p, y, r);
					pire = std::fmax(pire, std::fmax(std::fabs(p - a[0]), std::fmax(std::fabs(y - a[1]), std::fabs(r - a[2]))));
				}
				const math::NkQuatf nogee = (math::NkQuatf::RotateY(math::NkAngle(35.f)) * math::NkQuatf::RotateX(math::NkAngle(-22.f))).Normalized();
				const math::NkQuatf seq = NkSequenceRotationFromDegrees(-22.f, 35.f, 0.f);
				const float32 dot = std::fabs(nogee.x * seq.x + nogee.y * seq.y + nogee.z * seq.z + nogee.w * seq.w);
				Temoin(pire < 0.01f && dot > 0.99999f, "(s2k) degrés -> quaternion -> degrés : les mêmes angles, cap au-delà de 90°",
					   NkString::Format("écart max %.5f°, accord avec Nogee %.6f", static_cast<double>(pire), static_cast<double>(dot)).CStr());
			}

			// ── s3 L'ALLER-RETOUR .nkseq ──────────────────────────────────────
			std::printf("\ns3 — l'aller-retour .nkseq : écrire, rouvrir DANS UN AUTRE MONDE, réécrire\n");
			const char *fA = "Build/NKScena/Banc/a.nkseq";
			const char *fB = "Build/NKScena/Banc/b.nkseq";
			const char *fC = "Build/NKScena/Banc/c.nkseq";
			const char *fAbime = "Build/NKScena/Banc/abime.nkseq";
			const bool ecritA = m.Enregistrer(fA);
			Temoin(ecritA && NkFile::Exists(fA), "(s3) la séquence s'écrit", fA);
			{
				NkSequence lue;
				const bool ok = lue.LoadFromFile(fA);
				Temoin(ok && lue.scene == NkString(NkScenaModele::kSceneDemo) && lue.cameraTrack.shots.Size() == 1u &&
						   lue.cameraTrack.shots[0].cameraName == NkString("Caméra") && lue.tracks.Size() == 2u &&
						   lue.tracks[0].entityName == NkString("Joueur"),
					   "(s3b) le fichier POINTE vers sa scène, et nomme ses cibles (Joueur, Caméra)",
					   ok ? lue.scene.CStr() : NkSequenceDernierRefus());
			}
			static ecs::NkWorld monde2;
			NkScenaModele m2;
			m2.Brancher(&monde2, nullptr);
			const bool rouvert = m2.Ouvrir(fA);
			Temoin(rouvert && m2.CiblesPerdues() == 0u && m2.NombreCles() == m.NombreCles() &&
					   m2.frise.tracks.Size() == m.frise.tracks.Size(),
				   "(s3c) rouverte dans un monde NEUF : sa scène s'ouvre, les cibles se relient par leur nom",
				   NkString::Format("%u clés, %u pistes, %u perdue(s)", static_cast<unsigned>(m2.NombreCles()),
									static_cast<unsigned>(m2.frise.tracks.Size()), static_cast<unsigned>(m2.CiblesPerdues()))
					   .CStr());
			Temoin(JoueurA(m2, 1.f, -1.5f, &lu), "(s3d) la séquence relue joue la même clé : x(1 s) = -1,5",
				   NkString::Format("lu %.5f", static_cast<double>(lu)).CStr());
			const bool ecritB = m2.Enregistrer(fB);
			int64 ecart = 0;
			Temoin(ecritB && MemesOctets(fA, fB, &ecart), "(s3e) réécrite : IDENTIQUE OCTET À OCTET (ReadAllBytes)",
				   NkString::Format("premier écart %lld", static_cast<long long>(ecart)).CStr());
			{
				// MUTATION : une cle deplacee d'une image -- le comparateur DOIT la voir.
				editorkit::NkTimelineTrack &t = m2.frise.tracks[m2.frise.tracks.Size() - 1u];
				if (!t.keys.Empty()) {
					t.keys[0].v[0] += 0.25f;
					m2.frise.Touch();
				}
				(void)m2.Enregistrer(fC);
				ContreEpreuve(MemesOctets(fA, fC, &ecart), "(s3f) mutation d'une valeur (+0,25)",
							  NkString::Format("premier écart à l'octet %lld", static_cast<long long>(ecart)).CStr());
			}
			{
				// UN OCTET ABIME AU MILIEU : le refus doit etre NOMME, et rien ne change.
				NkVector<uint8> octets = NkFile::ReadAllBytes(fA);
				if (octets.Size() > 64u) {
					octets[(uint32)octets.Size() / 2u] ^= 0x5Au;
				}
				(void)NkFile::WriteAllBytes(fAbime, octets);
				const uint32 clesAvant = m2.NombreCles();
				const bool lu2 = m2.Ouvrir(fAbime);
				Temoin(!lu2 && NkSequenceDernierRefus()[0] != '\0' && m2.NombreCles() == clesAvant,
					   "(s3g) un octet abîmé : refus NOMMÉ, la séquence ouverte reste intacte", NkSequenceDernierRefus());
			}
			{
				// LA LIAISON PAR LE NOM : le joueur renomme dans la scene -> la piste
				// est PERDUE (comptee), et elle n'anime plus rien.
				const ecs::NkEntityId j3 = m2.Entite("Joueur");
				if (ecs::NkName *n = monde2.Get<ecs::NkName>(j3)) {
					*n = ecs::NkName("Joueur renommé");
				}
				m2.frise.Touch(); // la sequence se refait, et se RELIE de nouveau
				(void)m2.Synchroniser();
				ecs::NkTransform *tr = monde2.Get<ecs::NkTransform>(j3);
				if (tr != nullptr) {
					tr->localPosition.x = 42.f;
				}
				m2.Evaluer(1.f);
				Temoin(m2.CiblesPerdues() == 1u && tr != nullptr && tr->localPosition.x == 42.f,
					   "(s3h) contre-épreuve : l'entité renommée n'est plus visée (1 cible perdue, elle ne bouge pas)",
					   NkString::Format("perdues %u, x %.2f", static_cast<unsigned>(m2.CiblesPerdues()),
										static_cast<double>(tr != nullptr ? tr->localPosition.x : 0.f))
						   .CStr());
			}

			// ── s4 LA LECTURE ─────────────────────────────────────────────────
			std::printf("\ns4 — la lecture : le temps avance, la scène suit\n");
			{
				m.frise.cursor = 0.f;
				m.frise.playing = true;
				(void)m.Image(0.f);
				(void)m.Image(0.5f);
				const ecs::NkTransform *tj = Tf(m, "Joueur");
				Temoin(Proche(m.frise.cursor, 0.5f, 1e-5f) && tj != nullptr && Proche(tj->localPosition.x, -2.25f, 1e-4f),
					   "(s4) en lecture, 0,5 s plus tard : x = -2,25",
					   NkString::Format("curseur %.3f, x %.4f", static_cast<double>(m.frise.cursor),
										static_cast<double>(tj != nullptr ? tj->localPosition.x : 0.f))
						   .CStr());
				m.frise.playing = false;
				const float32 avant = tj != nullptr ? tj->localPosition.x : 0.f;
				(void)m.Image(0.5f);
				Temoin(Proche(m.frise.cursor, 0.5f, 1e-5f) && tj != nullptr && tj->localPosition.x == avant,
					   "(s4b) contre-épreuve : en pause, le temps et la pose ne bougent pas");
			}

			// ── s5 LE RENDU DE N IMAGES SANS FENETRE ──────────────────────────
			std::printf("\ns5 — le rendu sans fenêtre : 12 images PNG, la caméra du plan, le joueur qui traverse\n");
			{
				NkScenaRenduDesc rd;
				rd.largeur = 320;
				rd.hauteur = 180;
				rd.images = 12;
				rd.dossier = "Build/NKScena/Banc/rendu";
				rd.prefixe = "image";
				NkScenaRenduResultat rr;
				// Le pas : 4 s en 12 images -- l'exemple entier, pas 0,5 s de lui.
				m.frise.fps = 3.f;
				const int32 code = NkScenaRendreSansFenetre(m, rd, rr);
				m.frise.fps = 24.f;
				if (code < 0) {
					++gIgnores;
					std::printf("  [IGNORE] aucun périphérique graphique : le rendu n'est ni vert ni rouge (%s)\n", rr.raison.CStr());
				} else {
					Temoin(code == 1 && rr.rendues == 12 && rr.ecrites == 12, "(s5) 12 images rendues et écrites",
						   NkString::Format("%s, %d rendues, %d écrites%s%s", rr.api.CStr(), rr.rendues, rr.ecrites,
											rr.raison.Empty() ? "" : ", ", rr.raison.CStr())
							   .CStr());
					Temoin(CompterFichiers(rd.dossier.CStr(), "image", 20) == 12u, "(s5b) 12 fichiers image_0001..0012.png sur le disque");
					Temoin(rr.premiere != rr.derniere && rr.pixelsFond < rr.pixels,
						   "(s5c) la première et la dernière image DIFFÈRENT (le joueur et la caméra ont bougé)",
						   NkString::Format("0x%016llx / 0x%016llx, fond %u/%u px", static_cast<unsigned long long>(rr.premiere),
											static_cast<unsigned long long>(rr.derniere), rr.pixelsFond, rr.pixels)
							   .CStr());
					Temoin(rr.lumHaut < rr.lumBas, "(s5d) l'image est À L'ENDROIT : le ciel en haut, le sol (plus clair) en bas",
						   NkString::Format("luminance haut %.1f, bas %.1f", static_cast<double>(rr.lumHaut),
											static_cast<double>(rr.lumBas))
							   .CStr());
					ContreEpreuve(rr.lumBas < rr.lumHaut, "(s5e) la même image RETOURNÉE",
								  "le témoin d'orientation la voit à l'envers");
					NkScenaRenduDesc fige = rd;
					fige.figer = true;
					fige.ecrire = false;
					NkScenaRenduResultat rf;
					m.frise.fps = 3.f;
					(void)NkScenaRendreSansFenetre(m, fige, rf);
					m.frise.fps = 24.f;
					ContreEpreuve(rf.premiere != rf.derniere, "(s5f) le temps FIGÉ : la même image douze fois",
								  NkString::Format("0x%016llx / 0x%016llx", static_cast<unsigned long long>(rf.premiere),
												   static_cast<unsigned long long>(rf.derniere))
									  .CStr());
				}
			}

			std::printf("\n%s : %d réussis, %d échec%s%s\n", gEchecs == 0 ? "BANC NKSCENA REUSSI" : "BANC NKSCENA EN ECHEC", gReussis,
						gEchecs, gEchecs > 1 ? "s" : "", gIgnores > 0 ? " (le rendu ignoré : pas de GPU)" : "");
			std::fflush(stdout);
			m2.scene.Vider();
			m.scene.Vider();
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace nkscena
} // namespace nkentseu
