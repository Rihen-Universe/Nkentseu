#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// PV3DELanceur.h — LE LANCEUR DE PV3DE (2026-10-01).
//
// Le composant est celui de toute la famille (NKEditorKit) ; ce fichier n'en
// porte que LA TOUCHE du patient virtuel : son nom, son coeur, son vert
// medical, ses scenarios de depart, les cas cliniques .nkcase.
//
// CE QU'IL OUVRE PASSE PAR LES PORTES DU PATIENT, deja la :
//   - les SCENARIOS de depart -> ClearSymptoms / SetVitalSigns / ForceEmotion,
//     les memes gestes que le menu « Patient » (F1 a F6) ;
//   - « Ouvrir un cas... » et les recents -> PatientLayer::LoadCase (.nkcase).
// Les deux entrees MORTES du menu « Cas clinique » (« Nouveau cas » et
// « Charger (.nkcase) », dont le corps etait vide) y menent desormais.
//
// ⚠️ PV3DE N'EST PAS SUR LA COQUILLE NkEditorShell : sa couche medicale
//    (MedicalUILayer) peint NKGui sur NKRHI elle-meme. Elle heberge donc le
//    composant comme NKCraft, par NkLanceurPeindre (NkProjectLauncherHost.h).
// =============================================================================

#include "NKEditorKit/NkProjectLauncherHost.h"
#include "NKFileSystem/NkPath.h"
#include "NKMath/NkColor.h"
#include "PV3DE/Core/NkClinicalState.h"
#include <cstdio>

namespace nkentseu {
	namespace pv3de {

		/// Un scenario de depart : l'etat du patient a l'ouverture.
		struct PV3DEScenario {
				const char *nom, *categorie, *description;
				editorkit::NkLanceurGlyphe glyphe;
				uint32 teinte;
				EmotionState emotion;
				float32 intensite;
				float32 pouls, temperature, spo2;
		};

		inline const PV3DEScenario *PV3DEScenarios(uint32 &n) {
			using G = editorkit::NkLanceurGlyphe;
			static const PV3DEScenario k[] = {
				{"Consultation standard", "ACCUEIL", "Un patient calme, constantes normales : l'interrogatoire commence.",
				 G::Coeur, 0u, EmotionState::Neutral, 0.f, 72.f, 37.f, 98.f},
				{"Douleur aigue", "URGENCE", "Le patient souffre fortement : visage crispe, pouls rapide.", G::Croix,
				 0xD94F4FFFu, EmotionState::PainSevere, 1.f, 112.f, 37.4f, 97.f},
				{"Patient anxieux", "PSYCHOLOGIE", "Inquiet et agite : rassurer avant d'examiner.", G::Communaute,
				 0xE0A32EFFu, EmotionState::Anxious, 0.8f, 96.f, 37.f, 98.f},
				{"Crise de panique", "URGENCE", "Respiration courte, peur intense : la crise est en cours.", G::Etoile,
				 0xB05CE0FFu, EmotionState::Panic, 1.f, 128.f, 37.2f, 95.f},
				{"Patient epuise", "CHRONIQUE", "Fatigue profonde, reponses lentes : chercher la cause.", G::Lune,
				 0x5C8EE0FFu, EmotionState::Exhausted, 0.9f, 64.f, 36.6f, 97.f},
			};
			n = (uint32)(sizeof(k) / sizeof(k[0]));
			return k;
		}

		inline editorkit::NkLanceurRecents &PV3DERecents() {
			static editorkit::NkLanceurRecents r;
			return r;
		}

		/// LA TOUCHE de PV3DE.
		inline void PV3DERemplirLanceur(editorkit::NkProjectLauncherModel &m) {
			using namespace editorkit;
			using G = NkLanceurGlyphe;
			m.identite.nom = NkString("PV3DE");
			m.identite.prefixe = NkString("PV");
			m.identite.sousTitre = NkString("Patient virtuel 3D emotif — diagnostic");
			m.identite.version = NkString("0.1.0");
			m.identite.extensions = NkString(".nkcase");
			m.identite.motProjet = NkString("cas");
			m.identite.motProjets = NkString("cas");
			m.identite.accent = math::NkColor(0x1F, 0xA8, 0x8A).ToUint32A();
			m.identite.glyphe = G::Coeur;
			m.themeBasculable = true;
			m.modeles.Clear();
			uint32 n = 0;
			const PV3DEScenario *sc = PV3DEScenarios(n);
			for (uint32 i = 0; i < n; ++i) {
				NkLanceurModele md;
				md.nom = NkString(sc[i].nom);
				md.categorie = NkString(sc[i].categorie);
				md.description = NkString(sc[i].description);
				md.glyphe = sc[i].glyphe;
				md.couleur = sc[i].teinte;
				m.modeles.PushBack(md);
			}
			m.pages.Clear();
			NkLanceurPage pp;
			pp.libelle = NkString("Consultations");
			pp.titre = NkString("Consultations");
			pp.glyphe = G::Projets;
			m.pages.PushBack(pp);
			NkLanceurPage pa;
			pa.libelle = NkString("Apprendre");
			pa.sousTitre = NkString("L'examen clinique avec un patient virtuel.");
			pa.glyphe = G::Apprendre;
			pa.type = NkLanceurPageType::Liens;
			auto lien = [&](const char *t, const char *d, G g) {
				NkLanceurLien l;
				l.titre = NkString(t);
				l.description = NkString(d);
				l.glyphe = g;
				l.disponible = false;
				l.badge = NkString("a venir");
				pa.liens.PushBack(l);
			};
			lien("Conduire l'interrogatoire", "Poser les questions, lire les reponses et les emotions.", G::Communaute);
			lien("Ecrire un cas clinique", "Le format .nkcase : etat initial, evenements, questions.", G::Document);
			lien("Le rapport de diagnostic", "Hypotheses, examens et conclusion exportes.", G::Coeur);
			m.pages.PushBack(pa);
			NkLanceurPage pi;
			pi.libelle = NkString("Installations");
			pi.sousTitre = NkString("Ce qui est installe sur cette machine.");
			pi.glyphe = G::Installations;
			pi.type = NkLanceurPageType::Liens;
			{
				NkLanceurLien l;
				l.titre = NkString("PV3DE 0.1.0");
				l.description = NkString("Installe dans ") + NkPath::GetExecutableDirectory().ToString();
				l.glyphe = G::Coeur;
				l.badge = NkString("installe");
				pi.liens.PushBack(l);
			}
			m.pages.PushBack(pi);
			m.astuce = NkString("Astuce : F1 a F6 changent l'etat emotionnel du patient.");
			m.piedDePage = NkString("Outil de formation : il ne remplace pas un avis medical.");
		}

		/// `--capture-lanceur=FICHIER.png [--theme-lanceur=clair]` : la photo sans
		/// fenetre ni GPU, par le MEME composant et le MEME modele.
		inline int PV3DECapturerLanceur(const NkString &chemin, bool clair) {
			using namespace editorkit;
			static NkProjectLauncherModel m;
			PV3DERemplirLanceur(m);
			PV3DERecents().Charger("PV3DE");
			PV3DERecents().Remplir(m);
			m.themeSombre = !clair;
			NkLanceurCaptureDesc d;
			d.chemin = chemin.CStr();
			char msg[512];
			const bool ok = NkLanceurCapturer(d, m, clair ? NkTheme::Light() : NkTheme::Dark(), NkProjectLauncherHooks(),
											  NkProjectLauncherStyle(), msg, (int32)sizeof(msg));
			std::printf("[capture-lanceur] PV3DE : %s\n", msg);
			std::fflush(stdout);
			return ok ? 0 : 1;
		}

		/// Le lanceur s'ouvre-t-il au demarrage ? (faux : --sans-lanceur ou
		/// PV3DE_SANS_LANCEUR). Pose par nkmain, lu par la couche medicale.
		inline bool &PV3DELanceurAuDemarrage() {
			static bool b = true;
			return b;
		}

	} // namespace pv3de
} // namespace nkentseu
