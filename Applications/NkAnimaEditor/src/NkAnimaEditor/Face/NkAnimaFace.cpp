// -----------------------------------------------------------------------------
// @File    NkAnimaFace.cpp
// @Brief   La face d'Unreal 5 de NkAnimaEditor (voir NkAnimaFace.h).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "Face/NkAnimaFace.h"

#include "AnimBridge.h"
#include "NkAnimaActions.h"
#include "NkEditorRHIRenderer.h"
#include "Panels.h"
#include "Frise/NkAnimaFrise.h"	 // (02/10) ACCROCHE FRISE : la frise partagee
#include "Frise/NkAnimaGraphe.h" // (02/10) ACCROCHE GRAPHE : le graphe d'etats partage

#include "NKEditorKit/Components/NkSilhouettes.h"
#include "NKEditorKit/Famille/NkFamille.h"
#include "NKEditorKit/Terminal/NkTerminalPanneau.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkClock.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKWindow/Core/NkWindow.h"

#include <cmath>
#include <cstdio>
#include <cstdlib> // (02/10) NKANIMA_TIROIR
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace nkanima {

	using namespace nkentseu;
	using namespace nkentseu::editorkit;
	using nkgui::NkColor;
	using nkgui::NkRect;
	using nkgui::NkVec2;

	namespace {

		enum : int32 {
			MENU_FICHIER = 0,
			MENU_EDITION,
			MENU_FENETRE,
			MENU_AIDE,
			MENU_OUTIL = 10,
			MENU_AJOUTER,
			MENU_VUE,
			MENU_REGLAGES,
		};

		/// Les actions : 1.. = une action NOMMEE de NkAnimaActions (le MEME chemin
		/// que les documents d'interface et la palette de l'ancienne coquille).
		enum : int32 {
			A_RIEN = 0,
			A_NOMMEE = 1,
			A_OUTIL = 300,
			A_QUITTER = 400,
			A_ANCIENNE,
			A_THEME,
			A_VOIR_PLACER,
			A_VOIR_OUTLINER,
			A_VOIR_DETAILS,
			A_VOIR_TIROIR,
			A_DISPOSITION,
			A_STOP,
			A_RAGDOLL,
			A_COM,
			A_POSE,
			A_TIROIR = 450, ///< + onglet
			A_MODELE = 1000, ///< + modele du catalogue
		};

		const char *const kNommees[] = {
			"anim.annuler",		   "anim.refaire",		  "anim.inserer",	   "anim.supprimer",		"anim.jouer",
			"anim.debut",		   "anim.fin",			  "anim.image_prec",   "anim.image_suiv",		"anim.selection_rien",
			"anim.pose_entrer",	   "anim.pose_enregistrer", "anim.pose_quitter", "anim.vue_solide",		"anim.vue_rendu",
			"anim.vue_filaire",	   "anim.compteurs",	  "anim.boucle_activee", "anim.boucle_coupee",	"anim.physique_on",
			"anim.physique_off",   "anim.com_on",		  "anim.com_off",
		};
		constexpr int32 kNbNommees = static_cast<int32>(sizeof(kNommees) / sizeof(kNommees[0]));

		int32 Nommee(const char *nom) {
			for (int32 i = 0; i < kNbNommees; ++i) {
				if (std::strcmp(kNommees[i], nom) == 0) {
					return A_NOMMEE + i;
				}
			}
			return A_RIEN;
		}

		void Appeler(const char *nom) {
			if (nkgui::NkActionFn fn = FonctionDe(nom)) {
				fn(nullptr);
			}
		}

		/// Les personnages du depot que l'editeur sait ouvrir (chemin de modele en
		/// ligne de commande, comme avant).
		struct NkModele {
				const char *nom;
				const char *chemin;
				const char *aide;
		};
		const NkModele kModeles[] = {
			{"CesiumMan", "Resources/Models/CesiumMan/CesiumMan.glb", "Le personnage de démonstration (glTF, marche)."},
			{"Renard (Fox)", "Resources/Models/Fox/Fox.glb", "Un renard quadrupède (glTF, trois clips)."},
			{"BrainStem", "Resources/Models/BrainStem/BrainStem.glb", "Un robot à nombreux os (glTF)."},
			{"SimpleSkin", "Resources/Models/SimpleSkin/SimpleSkin.gltf", "La peau la plus simple : deux os (glTF)."},
		};
		constexpr int32 kNbModeles = static_cast<int32>(sizeof(kModeles) / sizeof(kModeles[0]));

		const char *Fichier(const char *chemin) {
			const char *r = chemin;
			for (const char *p = chemin; p != nullptr && *p != '\0'; ++p) {
				if (*p == '/' || *p == '\\') {
					r = p + 1;
				}
			}
			return r;
		}

		/// LA MARQUE, PROVISOIRE (Rihen choisira) : une chaine de trois os et une
		/// image-cle en losange, dans le cyan d'Anima.
		void Logo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 s, bool sombre) {
			const float32 u = s / 100.f;
			auto P = [&](float32 vx, float32 vy) { return NkVec2{x + vx * u, y + vy * u}; };
			const NkColor cyan = sombre ? NkColor(0x00D4FFFFu) : NkColor(0x0090B8FFu);
			const NkColor os = sombre ? NkColor(0xE6EAF0FFu) : NkColor(0x2A2D33FFu);
			const NkColor cle = sombre ? NkColor(0xF2980EFFu) : NkColor(0xC97A08FFu);
			const NkVec2 a = P(22.f, 78.f), b = P(44.f, 46.f), c = P(74.f, 30.f);
			dl.AddLine(a, b, os, 7.f * u);
			dl.AddLine(b, c, os, 7.f * u);
			dl.AddCircleFilled(a, 9.f * u, cyan);
			dl.AddCircleFilled(b, 9.f * u, cyan);
			dl.AddCircleFilled(c, 9.f * u, cyan);
			const NkVec2 k[4] = {P(72.f, 62.f), P(84.f, 74.f), P(72.f, 86.f), P(60.f, 74.f)};
			dl.AddTriangleFilled(k[0], k[1], k[2], cle);
			dl.AddTriangleFilled(k[0], k[2], k[3], cle);
		}

		/// Les icones des personnages du Placer.
		void IconeModele(void *, nkgui::NkGuiDrawList &dl, int32 k, const NkRect &r, const NkColor &texte) {
			const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
			const NkColor cyan{0, 212, 255, 255};
			if (k == 1) { // le renard : un corps et une queue
				dl.AddEllipseFilled(NkVec2{cx - 1.f, cy + 1.f}, 6.f, 3.5f, NkColor{230, 130, 50, 255});
				dl.AddTriangleFilled(NkVec2{cx + 4.f, cy - 1.f}, NkVec2{cx + 8.f, cy - 5.f}, NkVec2{cx + 7.f, cy + 1.f},
									 NkColor{230, 130, 50, 255});
				return;
			}
			// Un bonhomme en os : tete, colonne, bras, jambes.
			dl.AddCircleFilled(NkVec2{cx, cy - 7.f}, 2.6f, texte);
			dl.AddLine(NkVec2{cx, cy - 4.f}, NkVec2{cx, cy + 3.f}, texte, 1.6f);
			dl.AddLine(NkVec2{cx - 5.f, cy - 2.f}, NkVec2{cx + 5.f, cy - 2.f}, texte, 1.4f);
			dl.AddLine(NkVec2{cx, cy + 3.f}, NkVec2{cx - 4.f, cy + 9.f}, texte, 1.4f);
			dl.AddLine(NkVec2{cx, cy + 3.f}, NkVec2{cx + 4.f, cy + 9.f}, texte, 1.4f);
			dl.AddCircleFilled(NkVec2{cx, cy + 3.f}, 1.6f, cyan);
		}

		const NkFamilleNatureFichier kNatures[] = {
			{".glb", "Personnage", NkRole::TypeAnim, static_cast<uint8>(NkAssetIcone::Inconnu)},
			{".gltf", "Personnage", NkRole::TypeAnim, static_cast<uint8>(NkAssetIcone::Inconnu)},
			{".fbx", "Personnage", NkRole::TypeAnim, static_cast<uint8>(NkAssetIcone::Inconnu)},
			{".nkanimctl", "Contrôleur", NkRole::AccentSel, static_cast<uint8>(NkAssetIcone::Texte)},
			{".png", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
			{".jpg", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
			{".obj", "Maillage", NkRole::TypeMesh, static_cast<uint8>(NkAssetIcone::Inconnu)},
		};
	} // namespace

	// =========================================================================
	// LA FACE
	// =========================================================================
	class NkAnimaFace final : public NkFamilleEditeur {
		public:
			explicit NkAnimaFace(const char *modele) : mModele(modele != nullptr ? modele : "") {
				contenu.racine = NkString("Resources/Models");
				contenu.nomProjet = NkString("NkAnimaEditor");
				terminal.dossierDepart = NkString(".");
				journal.Ajouter("NkAnimaEditor — l'apparence d'UnkenyEditor (R32), pièces NKEditorKit/Famille");
				journal.Ajouter(NkString::Format("Modèle : %s", mModele.CStr()).CStr());
			}

			PreviewPanel apercu;  ///< la vue 3D, ses os et ses outils de pose (Panels.h)
			TimelinePanel frise;  ///< la frise historique (ACCROCHE FRISE)
			NkTerminalPanneau terminal;
			NkFamilleJournal journal;
			NkFamilleContenu contenu;
			NkFamilleOutliner outliner;
			NkFamilleDetailsEtat details;
			NkFamillePlacer placer;
			int32 ongletTiroir = 0; ///< 0 Frise, 1 Graphe d'etats, 2 Contenu, 3 Journal, 4 Terminal
			int32 ongletDroite = 0;
			float32 dt = 1.f / 60.f;

		protected:
			NkString Titre() const override {
				return NkString::Format("NkAnimaEditor  —  %s%s", Fichier(mModele.CStr()), AnimInPoseEdit() ? "  (mode pose)" : "");
			}

			void PeindreLogo(nkgui::NkGuiDrawList &dl, float32 x, float32 y, float32 s, bool sombre) override {
				Logo(dl, x, y, s, sombre);
			}

			void AvantCorps(NkFamilleCtx &) override {
				mFriseVue = false;
			}

			void PeindreOnglets(NkFamilleCtx &c) override {
				NkFamilleOngletScene o;
				o.nom = Fichier(mModele.CStr());
				o.modifie = AnimKeyCount() > 0u && AnimInPoseEdit();
				(void)NkFamilleOngletsScene(c, plan, &o, 1, 0);
			}

			void PeindreBarreOutils(NkFamilleCtx &c) override {
				const NkRect &b = plan.barreOutils;
				NkFamilleFondBarreOutils(c, b);
				static const char *const kOutils[3] = {"Drag IK", "Rotation FK", "Translation FK"};
				float32 largeurOutil = 0.f;
				for (int32 k = 0; k < 3; ++k) {
					const NkString s = NkString::Format("Outil : %s", kOutils[k]);
					const float32 w = NkFamilleLargeurBoutonOutil(c, s.CStr(), true);
					largeurOutil = w > largeurOutil ? w : largeurOutil;
				}
				bool clic = false;
				NkRect r;
				float32 x = b.x + 8.f;
				x = NkFamilleBoutonOutil(c, b, x, "Enregistrer la pose", false, false, clic, r, 0.f, AnimInPoseEdit());
				if (clic) {
					Executer(Nommee("anim.pose_enregistrer"));
				}
				x = NkFamilleTrait(c, b, x);
				const NkString outil = NkString::Format("Outil : %s", kOutils[apercu.Outil()]);
				x = NkFamilleBoutonOutil(c, b, x, outil.CStr(), true, menus.menu == MENU_OUTIL, clic, r, largeurOutil);
				if (clic) {
					menus.Ouvrir(MENU_OUTIL, r);
				}
				x = NkFamilleTrait(c, b, x);
				x = NkFamilleBoutonOutil(c, b, x, "+ Ajouter", true, menus.menu == MENU_AJOUTER, clic, r);
				if (clic) {
					menus.Ouvrir(MENU_AJOUTER, r);
				}
				x = NkFamilleTrait(c, b, x);
				const NkFamilleEtatJeu etat = AnimIsPlaying() ? NkFamilleEtatJeu::Jeu
											  : (AnimCursor() > 0.f ? NkFamilleEtatJeu::Pause : NkFamilleEtatJeu::Edition);
				const int32 k = NkFamilleBoutonsLecture(c, b, x, etat);
				if (k == 0) {
					if (!AnimIsPlaying()) {
						Executer(Nommee("anim.jouer"));
					}
				} else if (k == 1) {
					if (AnimIsPlaying()) {
						Executer(Nommee("anim.jouer"));
					}
				} else if (k == 2) {
					Executer(A_STOP);
				} else if (k == 3) {
					Executer(Nommee("anim.image_suiv"));
				}
				x = NkFamilleTrait(c, b, x);
				static const char *const kVues[3] = {"Solide", "Rendu", "Filaire"};
				const NkString vue = NkString::Format("Vue : %s", kVues[static_cast<int32>(Anim3DViewMode())]);
				x = NkFamilleBoutonOutil(c, b, x, vue.CStr(), true, menus.menu == MENU_VUE, clic, r,
										 NkFamilleLargeurBoutonOutil(c, "Vue : Filaire", true) + 40.f);
				if (clic) {
					menus.Ouvrir(MENU_VUE, r);
				}
				x = NkFamilleTrait(c, b, x);
				(void)NkFamilleBoutonOutil(c, b, x, "Réglages", true, menus.menu == MENU_REGLAGES, clic, r);
				if (clic) {
					menus.Ouvrir(MENU_REGLAGES, r);
				}
			}

			void PeindreStatut(NkFamilleCtx &c) override {
				const NkFamilleEtatJeu etat = AnimIsPlaying() ? NkFamilleEtatJeu::Jeu : NkFamilleEtatJeu::Edition;
				uint32 d = 0, t = 0, v = 0;
				const bool cpt = Anim3DCompteurs(&d, &t, &v);
				const NkString compteurs = NkString::Format(
					"%u os   ·   t = %.2f / %.2f s   ·   %u clés   ·   %s   ·   %.0f ips", AnimJointCount(), static_cast<double>(AnimCursor()),
					static_cast<double>(AnimDuration()), AnimKeyCount(),
					cpt ? NkString::Format("%u draws", d).CStr() : "draws --", static_cast<double>(ips));
				NkFamilleBarreEtat(c, plan.statut, etat, mMessageAge < 4.f ? mMessage.CStr() : "", compteurs.CStr());
				mMessageAge += dt;
			}

			void PeindreCorps(NkFamilleCtx &c) override {
				Vue(c);
				Placer(c);
				Outliner(c);
				Details(c);
				Tiroir(c);
				// La lecture AVANCE meme quand la frise est cachee (elle seule
				// appelait AnimUpdate dans l'ancienne coquille).
				if (!mFriseVue) {
					AnimUpdate(dt);
				}
			}

			void Remplir(int32 m, NkVector<NkFamilleEntreeMenu> &out) override {
				auto L = [&](const char *libelle, const char *nommee, const char *raccourci = "", bool coche = false, bool actif = true) {
					out.PushBack(NkFamilleLigneMenu(libelle, Nommee(nommee), raccourci, coche, actif));
				};
				switch (m) {
					case MENU_FICHIER:
						for (int32 k = 0; k < kNbModeles; ++k) {
							out.PushBack(NkFamilleLigneMenu(NkString::Format("Ouvrir %s (nouvelle fenêtre)", kModeles[k].nom).CStr(),
															A_MODELE + k));
						}
						out.PushBack(NkFamilleSeparateur());
						out.PushBack(NkFamilleLigneMenu("Quitter", A_QUITTER, "Ctrl+Q"));
						break;
					case MENU_EDITION:
						L("Annuler", "anim.annuler", "Ctrl+Z");
						L("Refaire", "anim.refaire", "Ctrl+Y");
						out.PushBack(NkFamilleSeparateur());
						L("Insérer une clé", "anim.inserer", "I");
						L("Supprimer la clé", "anim.supprimer", "Suppr");
						L("Tout désélectionner", "anim.selection_rien");
						out.PushBack(NkFamilleSeparateur());
						L("Éditer la pose", "anim.pose_entrer", "", AnimInPoseEdit(), !AnimInPoseEdit());
						L("Enregistrer la pose en clé", "anim.pose_enregistrer", "", false, AnimInPoseEdit());
						L("Quitter la pose sans enregistrer", "anim.pose_quitter", "", false, AnimInPoseEdit());
						break;
					case MENU_FENETRE:
						out.PushBack(NkFamilleLigneMenu("Placer des acteurs", A_VOIR_PLACER, "", plan.voirPlacer));
						out.PushBack(NkFamilleLigneMenu("Outliner", A_VOIR_OUTLINER, "", plan.voirOutliner));
						out.PushBack(NkFamilleLigneMenu("Détails", A_VOIR_DETAILS, "", plan.voirDetails));
						out.PushBack(NkFamilleLigneMenu("Tiroir du bas", A_VOIR_TIROIR, "", plan.voirTiroir));
						out.PushBack(NkFamilleSeparateur());
						{
							static const char *const kTiroir[5] = {"Frise", "Graphe d'états", "Contenu", "Journal", "Terminal"};
							for (int32 k = 0; k < 5; ++k) {
								out.PushBack(NkFamilleLigneMenu(kTiroir[k], A_TIROIR + k, "", ongletTiroir == k));
							}
						}
						out.PushBack(NkFamilleSeparateur());
						out.PushBack(NkFamilleLigneMenu("Thème clair", A_THEME, "", Clair()));
						out.PushBack(NkFamilleLigneMenu("Réinitialiser la disposition", A_DISPOSITION));
						out.PushBack(NkFamilleSeparateur());
						out.PushBack(NkFamilleLigneMenu("Ancienne coquille (NkEditorShell, documents)…", A_ANCIENNE));
						break;
					case MENU_AIDE:
						out.PushBack(NkFamilleIntitule("NkAnimaEditor — animation et VFX (Aetherion Animate & FX)"));
						out.PushBack(NkFamilleIntitule("Apparence d'UnkenyEditor (R32), pièces NKEditorKit/Famille"));
						out.PushBack(NkFamilleSeparateur());
						out.PushBack(NkFamilleIntitule("Espace : jouer / pause · I : insérer une clé"));
						out.PushBack(NkFamilleIntitule("Clic droit + souris : tourner la vue · molette : zoom"));
						out.PushBack(NkFamilleIntitule("Mode pose : clic = os, glisser = manipuler"));
						break;
					case MENU_OUTIL: {
						static const char *const kOutils[3] = {"Drag IK", "Rotation FK", "Translation FK"};
						for (int32 k = 0; k < 3; ++k) {
							out.PushBack(NkFamilleLigneMenu(kOutils[k], A_OUTIL + k, "", apercu.Outil() == k));
						}
						out.PushBack(NkFamilleSeparateur());
						out.PushBack(NkFamilleLigneMenu("Mode pose", A_POSE, "", AnimInPoseEdit()));
						break;
					}
					case MENU_AJOUTER:
						out.PushBack(NkFamilleIntitule("Animation"));
						L("Clé de pose au curseur", "anim.inserer", "I");
						L("Pose : éditer, puis enregistrer en clé", "anim.pose_entrer", "", false, !AnimInPoseEdit());
						out.PushBack(NkFamilleIntitule("Personnages (nouvelle fenêtre)"));
						for (int32 k = 0; k < kNbModeles; ++k) {
							out.PushBack(NkFamilleLigneMenu(kModeles[k].nom, A_MODELE + k));
						}
						break;
					case MENU_VUE:
						L("Solide", "anim.vue_solide", "", Anim3DViewMode() == NkAnimViewMode::SOLIDE);
						L("Rendu", "anim.vue_rendu", "", Anim3DViewMode() == NkAnimViewMode::RENDU);
						L("Filaire", "anim.vue_filaire", "", Anim3DViewMode() == NkAnimViewMode::FILAIRE);
						out.PushBack(NkFamilleSeparateur());
						L("Compteurs de rendu", "anim.compteurs", "", Anim3DCompteursVisibles());
						out.PushBack(NkFamilleLigneMenu("Centre de masse", A_COM, "", AnimShowCOM()));
						break;
					case MENU_REGLAGES:
						L("Lecture en boucle", LectureEnBoucle() ? "anim.boucle_coupee" : "anim.boucle_activee", "", LectureEnBoucle());
						out.PushBack(NkFamilleLigneMenu("Ragdoll (NKPhysics)", A_RAGDOLL, "", AnimPhysicsEnabled()));
						out.PushBack(NkFamilleLigneMenu("Centre de masse", A_COM, "", AnimShowCOM()));
						break;
					default:
						break;
				}
			}

			void Executer(int32 a) override {
				if (a >= A_NOMMEE && a < A_NOMMEE + kNbNommees) {
					Appeler(kNommees[a - A_NOMMEE]);
					Annoncer(kNommees[a - A_NOMMEE]);
					return;
				}
				if (a >= A_OUTIL && a < A_OUTIL + 3) {
					apercu.PoserOutil(a - A_OUTIL);
					return;
				}
				if (a >= A_TIROIR && a < A_TIROIR + 5) {
					ongletTiroir = a - A_TIROIR;
					plan.voirTiroir = true;
					return;
				}
				if (a >= A_MODELE && a < A_MODELE + kNbModeles) {
					Relancer(kModeles[a - A_MODELE].chemin, false);
					return;
				}
				switch (a) {
					case A_QUITTER: DemanderQuitter(); break;
					case A_ANCIENNE: Relancer(mModele.CStr(), true); break;
					case A_THEME: PoserTheme(!Clair()); break;
					case A_VOIR_PLACER: plan.voirPlacer = !plan.voirPlacer; break;
					case A_VOIR_OUTLINER: plan.voirOutliner = !plan.voirOutliner; break;
					case A_VOIR_DETAILS: plan.voirDetails = !plan.voirDetails; break;
					case A_VOIR_TIROIR: plan.voirTiroir = !plan.voirTiroir; break;
					case A_DISPOSITION: {
						const NkFamillePlan neuf;
						plan.largeurPlacer = neuf.largeurPlacer;
						plan.largeurOutliner = neuf.largeurOutliner;
						plan.largeurDetails = neuf.largeurDetails;
						plan.hauteurTiroir = neuf.hauteurTiroir;
						plan.voirPlacer = plan.voirOutliner = plan.voirDetails = plan.voirTiroir = true;
						break;
					}
					case A_STOP:
						// Arreter = la lecture s'arrete et revient au debut.
						if (AnimIsPlaying()) {
							Appeler("anim.jouer");
						}
						Appeler("anim.debut");
						Annoncer("Lecture arrêtée, curseur au début");
						break;
					case A_RAGDOLL: Appeler(AnimPhysicsEnabled() ? "anim.physique_off" : "anim.physique_on"); break;
					case A_COM: Appeler(AnimShowCOM() ? "anim.com_off" : "anim.com_on"); break;
					case A_POSE: Appeler(AnimInPoseEdit() ? "anim.pose_quitter" : "anim.pose_entrer"); break;
					default: break;
				}
			}

			void Raccourcis(NkFamilleCtx &c) override {
				const nkgui::NkGuiInput &in = c.ctx.input;
				if (in.KeyPressed(nkgui::NkGuiKey::Escape) && menus.Ouvert()) {
					menus.Fermer();
					return;
				}
				if (c.ctx.inputId != nkgui::NKGUI_ID_NONE || terminal.AFocus() || !outliner.Libre() || !details.Libre() ||
					placer.filtreFocus || journal.rechercheFocus || contenu.modele.searchFocused) {
					return;
				}
				if (in.ctrlDown) {
					if (in.KeyPressed(nkgui::NkGuiKey::Z)) {
						Executer(Nommee("anim.annuler"));
					} else if (in.KeyPressed(nkgui::NkGuiKey::Y)) {
						Executer(Nommee("anim.refaire"));
					} else if (in.KeyPressed(nkgui::NkGuiKey::Q)) {
						DemanderQuitter();
					}
					return;
				}
				if (in.KeyPressed(nkgui::NkGuiKey::Space)) {
					Executer(Nommee("anim.jouer"));
				} else if (in.KeyPressed(nkgui::NkGuiKey::I)) {
					Executer(Nommee("anim.inserer"));
				} else if (in.KeyPressed(nkgui::NkGuiKey::Delete)) {
					Executer(Nommee("anim.supprimer"));
				} else if (in.KeyPressed(nkgui::NkGuiKey::Left)) {
					Executer(Nommee("anim.image_prec"));
				} else if (in.KeyPressed(nkgui::NkGuiKey::Right)) {
					Executer(Nommee("anim.image_suiv"));
				}
			}

		private:
			void Annoncer(const char *texte) {
				mMessage = NkString(texte);
				mMessageAge = 0.f;
				const int32 s = static_cast<int32>(temps);
				journal.Ajouter(NkString::Format("[%02d:%02d]  %s", s / 60, s % 60, texte).CStr());
			}

			/// Une NOUVELLE fenetre de l'editeur (un autre modele, ou l'ancienne coquille).
			void Relancer(const char *modele, bool ancienne) {
#if defined(_WIN32)
				char exe[MAX_PATH] = {};
				if (GetModuleFileNameA(nullptr, exe, MAX_PATH) == 0) {
					return;
				}
				char ligne[MAX_PATH * 2];
				std::snprintf(ligne, sizeof(ligne), "\"%s\" %s%s", exe, ancienne ? "--ancienne-coquille " : "",
							  modele != nullptr ? modele : "");
				STARTUPINFOA si{};
				si.cb = sizeof(si);
				PROCESS_INFORMATION pi{};
				if (CreateProcessA(nullptr, ligne, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
					Annoncer(ancienne ? "Ancienne coquille ouverte dans une autre fenêtre" : "Personnage ouvert dans une autre fenêtre");
				}
#else
				(void)modele;
				(void)ancienne;
				Annoncer("Relancer l'éditeur avec le chemin du modèle (ou --ancienne-coquille)");
#endif
			}

			// ── Le viseur : la vue 3D d'AnimBridge, ses os, ses outils de pose ──
			void Vue(NkFamilleCtx &c) {
				auto &dl = c.ctx.dl;
				nkgui::NkGuiInput &in = c.ctx.input;
				const NkRect &v = plan.viseur;
				if (plan.vue.w < 8.f || plan.vue.h < 8.f) {
					return;
				}
				const NkFamilleBoutonVue boutons[5] = {{"Solide", Anim3DViewMode() == NkAnimViewMode::SOLIDE},
													   {"Rendu", Anim3DViewMode() == NkAnimViewMode::RENDU},
													   {"Filaire", Anim3DViewMode() == NkAnimViewMode::FILAIRE},
													   {"Centre de masse", AnimShowCOM()},
													   {"Compteurs", Anim3DCompteursVisibles()}};
				const NkString reperes = NkString::Format("Perspective   ·   %s%s", AnimInPoseEdit() ? "mode pose   ·   " : "",
														  AnimShowCOM() ? AnimCOMRegimeLabel() : "rendu 1280x720 recadré");
				const int32 k = NkFamilleBarreVue(c, plan.barreVue, boutons, 5, reperes.CStr());
				static const char *const kActions[3] = {"anim.vue_solide", "anim.vue_rendu", "anim.vue_filaire"};
				if (k >= 0 && k < 3) {
					Executer(Nommee(kActions[k]));
				} else if (k == 3) {
					Executer(A_COM);
				} else if (k == 4) {
					Executer(Nommee("anim.compteurs"));
				}
				dl.AddRectFilled(v, NkColor{18, 18, 21, 255});
				// La vue, PEINTE PAR LE PANNEAU D'APERCU (ses os, ses gizmos de pose,
				// ses compteurs) dans le viseur de la famille.
				apercu.zoneImposee = v;
				NkEditorFrameContext ec;
				ec.ui = &c.ctx;
				ec.dt = dt;
				dl.PushClipRect(v, true);
				apercu.OnUI(ec);
				dl.PopClipRect();
				// La barre flottante : les trois outils de pose et le mode pose.
				NkFamilleElementBarre el[4];
				el[0].icone = NkFamilleIconeBarre::Deplacer;
				el[0].enfonce = apercu.Outil() == 0;
				el[0].bulle = NkString("Drag IK : tirer un os, la chaîne suit");
				el[1].icone = NkFamilleIconeBarre::Tourner;
				el[1].enfonce = apercu.Outil() == 1;
				el[1].bulle = NkString("Rotation FK : tourner l'os choisi");
				el[2].icone = NkFamilleIconeBarre::Echelle;
				el[2].enfonce = apercu.Outil() == 2;
				el[2].bulle = NkString("Translation FK : déplacer l'os choisi");
				el[3].icone = NkFamilleIconeBarre::Selection;
				el[3].texte = NkString(AnimInPoseEdit() ? "Pose : oui" : "Pose : non");
				el[3].enfonce = AnimInPoseEdit();
				el[3].separateur = true;
				el[3].bulle = NkString("Le mode pose : clic = os, glisser = manipuler, Enregistrer = clé");
				const NkFamilleBarreFlottanteResultat r = NkFamilleBarreFlottante(c, v, el, 4, menus.Ouvert());
				if (r.clic >= 0 && r.clic < 3) {
					apercu.PoserOutil(r.clic);
				} else if (r.clic == 3) {
					Executer(A_POSE);
				}
				// La camera : clic droit tenu + souris, molette (Anim3DOrbit).
				const bool dedans = NkFamilleDans(v, in.mousePos) && !NkFamilleDans(r.barre, in.mousePos);
				if (dedans && in.wheel != 0.f) {
					Anim3DOrbit(0.f, 0.f, -in.wheel * 0.1f);
				}
				if (dedans && in.mouseClicked[1]) {
					mOrbite = true;
					mAvant = in.mousePos;
				}
				if (mOrbite) {
					if (in.mouseDown[1]) {
						Anim3DOrbit((in.mousePos.x - mAvant.x) * 0.01f, (in.mousePos.y - mAvant.y) * 0.01f, 0.f);
						mAvant = in.mousePos;
					} else {
						mOrbite = false;
					}
				}
			}

			// ── Placer des acteurs : les personnages ───────────────────────────
			void Placer(NkFamilleCtx &c) {
				if (!plan.voirPlacer) {
					return;
				}
				static NkFamilleElementPlacer elements[kNbModeles];
				for (int32 k = 0; k < kNbModeles; ++k) {
					elements[k].nom = kModeles[k].nom;
					elements[k].aide = kModeles[k].aide;
					elements[k].onglets = NkFamilleBitOnglet(NkFamilleOngletPlacer::Base);
				}
				const NkFamillePlacerResultat r =
					NkFamilleDessinerPlacer(c, plan.placer, placer, elements, kNbModeles, &IconeModele, nullptr, plan.viseur);
				const int32 k = r.clic >= 0 ? r.clic : r.lache;
				if (k >= 0) {
					Relancer(kModeles[k].chemin, false);
				}
			}

			// ── L'Outliner : le modele, son clip, son squelette ────────────────
			void Outliner(NkFamilleCtx &c) {
				if (!plan.voirOutliner) {
					return;
				}
				NkTreeViewModel &a = outliner.arbre;
				a.nodes.Clear();
				NkTreeNode racine;
				racine.id = kNkFamilleRacine;
				racine.parent = -1;
				racine.label = NkString("Scène");
				racine.kindLabel = "Monde";
				racine.kindRole = static_cast<uint16>(NkRole::TextMuted);
				a.nodes.PushBack(racine);
				NkTreeNode modele;
				modele.id = 2u;
				modele.parent = 0;
				modele.label = NkString(Fichier(mModele.CStr()));
				modele.kindLabel = "personnage";
				modele.icon = NkFamilleIconeNoeud(NkFamilleNature::Maillage, false, false, false, false);
				modele.kindRole = static_cast<uint16>(NkRole::TypeAnim);
				a.nodes.PushBack(modele);
				NkTreeNode clip;
				clip.id = 3u;
				clip.parent = 1;
				clip.label = NkString::Format("Clip (%.2f s, %u clés)", static_cast<double>(AnimDuration()), AnimKeyCount());
				clip.kindLabel = "clip";
				clip.icon = NkFamilleIconeNoeud(NkFamilleNature::Emetteur, false, false, false, false);
				clip.kindRole = static_cast<uint16>(NkRole::AccentSel);
				a.nodes.PushBack(clip);
				NkVector<NkVec3f> pos;
				NkVector<int32> par;
				AnimGetSkeleton(pos, par);
				// Le squelette en ordre PREFIXE (un parent avant ses enfants).
				const int32 base = static_cast<int32>(a.nodes.Size());
				NkVector<int32> indiceDeNoeud;
				indiceDeNoeud.Resize(pos.Size());
				for (uint32 j = 0; j < pos.Size(); ++j) {
					indiceDeNoeud[j] = -1;
				}
				auto Ranger = [&](auto &&soi, int32 j, int32 parentNoeud, uint32 prof) -> void {
					NkTreeNode n;
					n.id = 100u + static_cast<nk_uint64>(j);
					n.parent = parentNoeud;
					n.label = NkString::Format("os %d", static_cast<int>(j));
					n.kindLabel = "os";
					n.icon = NkFamilleIconeNoeud(NkFamilleNature::Entite, false, false, false, false);
					n.kindRole = static_cast<uint16>(NkRole::TextMuted);
					indiceDeNoeud[static_cast<uint32>(j)] = static_cast<int32>(a.nodes.Size());
					const int32 moi = static_cast<int32>(a.nodes.Size());
					a.nodes.PushBack(n);
					if (prof + 3u >= static_cast<uint32>(NkTreeViewModel::kMaxDepth)) {
						return;
					}
					for (uint32 e = 0; e < par.Size(); ++e) {
						if (par[e] == j) {
							soi(soi, static_cast<int32>(e), moi, prof + 1u);
						}
					}
				};
				(void)base;
				for (uint32 j = 0; j < par.Size(); ++j) {
					if (par[j] < 0 || par[j] >= static_cast<int32>(par.Size())) {
						Ranger(Ranger, static_cast<int32>(j), 1, 1u);
					}
				}
				a.chosen.Clear();
				a.active = apercu.OsChoisi() >= 0 ? 100u + static_cast<nk_uint64>(apercu.OsChoisi()) : 0u;
				if (a.active != 0u) {
					a.chosen.PushBack(a.active);
				}
				NkFamilleOutlinerRappels rappels;
				const NkString pied = NkString::Format("%u os (%u sél.)", AnimJointCount(), apercu.OsChoisi() >= 0 ? 1u : 0u);
				const NkFamilleOutlinerResultat res = NkFamilleDessinerOutliner(c, plan.outliner, outliner, rappels, pied.CStr(), dt,
																				"Outliner", "");
				if (res.selectionChangee) {
					const int32 i = res.actif;
					if (i > 0 && i < static_cast<int32>(a.nodes.Size()) && a.nodes[static_cast<uint32>(i)].id >= 100u) {
						apercu.ChoisirOs(static_cast<int32>(a.nodes[static_cast<uint32>(i)].id - 100u));
					} else {
						apercu.ChoisirOs(-1);
					}
				}
			}

			// ── Les Details : le clip, la lecture, la pose, la vue, la physique ──
			void Details(NkFamilleCtx &c) {
				if (!plan.voirDetails || plan.details.w < 8.f || plan.details.h < 60.f) {
					return;
				}
				const NkRect &zone = plan.details;
				c.ctx.dl.AddRectFilled(zone, pal.panneau);
				static const char *const kOnglets[2] = {"Détails", "Monde"};
				(void)NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, NkFamilleCotes::kOngletPanneau}, kOnglets, 2, ongletDroite);
				const NkRect contenuR{zone.x, zone.y + NkFamilleCotes::kOngletPanneau, zone.w, zone.h - NkFamilleCotes::kOngletPanneau};
				c.ctx.dl.PushClipRect(contenuR, true);
				NkFamilleInspecteur I(c, details, contenuR);
				if (!AnimLoaded()) {
					I.Vide("Aucun personnage chargé.", "Ouvrez-en un : Placer des acteurs, ou Fichier.");
					c.ctx.dl.PopClipRect();
					return;
				}
				bool actif = true;
				const NkString type = NkString::Format("type : personnage animé  —  %u os", AnimJointCount());
				if (I.Entete(1u, Fichier(mModele.CStr()), nullptr, type.CStr(), menus.menu == MENU_AJOUTER, nullptr)) {
					menus.Ouvrir(MENU_AJOUTER, details.ajouter);
				}
				(void)actif;
				const NkFamilleComposant comps[5] = {{0, "Clip", NkFamilleIconeCarte::Animation},
													 {1, "Lecture", NkFamilleIconeCarte::Animateur},
													 {2, "Pose", NkFamilleIconeCarte::Hierarchie},
													 {3, "Vue", NkFamilleIconeCarte::Camera},
													 {4, "Physique", NkFamilleIconeCarte::Corps}};
				if (I.Debut("anima", Fichier(mModele.CStr()), comps, 5)) {
					if (I.Carte(0, "Clip", NkFamilleIconeCarte::Animation, NkFamilleCategorie::Animation)) {
						I.Info("Durée", NkString::Format("%.3f s", static_cast<double>(AnimDuration())).CStr());
						I.Info("Images par seconde", NkString::Format("%.0f", static_cast<double>(AnimFps())).CStr());
						I.Info("Clés de pose", NkString::Format("%u", AnimKeyCount()).CStr());
						I.Info("Os", NkString::Format("%u", AnimJointCount()).CStr());
						if (I.Boutons("Insérer une clé (I)", "Supprimer la clé", true, true) == 0) {
							Executer(Nommee("anim.inserer"));
						}
						I.FinCarte();
					}
					if (I.Carte(1, "Lecture", NkFamilleIconeCarte::Animateur, NkFamilleCategorie::Animation)) {
						float32 t = AnimCursor();
						if (I.Glissiere("Curseur (s)", t, 0.f, AnimDuration() > 0.f ? AnimDuration() : 1.f)) {
							AnimSeek(t);
						}
						bool lecture = AnimIsPlaying();
						if (I.Case("En lecture", lecture)) {
							AnimSetPlaying(lecture);
						}
						bool boucle = LectureEnBoucle();
						if (I.Case("En boucle", boucle)) {
							Appeler(boucle ? "anim.boucle_activee" : "anim.boucle_coupee");
						}
						const int32 b = I.Boutons("Image précédente", "Image suivante");
						if (b == 0) {
							Executer(Nommee("anim.image_prec"));
						} else if (b == 1) {
							Executer(Nommee("anim.image_suiv"));
						}
						I.FinCarte();
					}
					if (I.Carte(2, "Pose", NkFamilleIconeCarte::Hierarchie, NkFamilleCategorie::Animation)) {
						static const char *const kOutils[3] = {"Drag IK", "Rotation FK", "Transl. FK"};
						int32 o = apercu.Outil();
						if (I.Choix("Outil", kOutils, 3, o)) {
							apercu.PoserOutil(o);
						}
						bool pose = AnimInPoseEdit();
						if (I.Case("Mode pose", pose)) {
							Executer(A_POSE);
						}
						const int32 b = I.Boutons("Enregistrer en clé", "Quitter sans enregistrer", AnimInPoseEdit(), AnimInPoseEdit());
						if (b == 0) {
							Executer(Nommee("anim.pose_enregistrer"));
						} else if (b == 1) {
							Executer(Nommee("anim.pose_quitter"));
						}
						if (apercu.OsChoisi() >= 0) {
							float32 x = 0.f, y = 0.f, z = 0.f;
							AnimJointWorldPos(apercu.OsChoisi(), x, y, z);
							float32 p[3] = {x, y, z};
							I.Info("Os choisi", NkString::Format("os %d", static_cast<int>(apercu.OsChoisi())).CStr());
							(void)I.Vecteur("Position (monde)", p, 3, 0.f);
						} else {
							I.Ligne("Choisissez un os dans la vue (mode pose) ou l'Outliner.");
						}
						I.FinCarte();
					}
					if (I.Carte(3, "Vue", NkFamilleIconeCarte::Camera, NkFamilleCategorie::Rendu)) {
						static const char *const kVues[3] = {"Solide", "Rendu", "Filaire"};
						int32 m = static_cast<int32>(Anim3DViewMode());
						if (I.Choix("Affichage", kVues, 3, m)) {
							static const char *const kActions[3] = {"anim.vue_solide", "anim.vue_rendu", "anim.vue_filaire"};
							Executer(Nommee(kActions[m]));
						}
						bool cpt = Anim3DCompteursVisibles();
						if (I.Case("Compteurs de rendu", cpt)) {
							Executer(Nommee("anim.compteurs"));
						}
						I.FinCarte();
					}
					if (I.Carte(4, "Physique", NkFamilleIconeCarte::Corps, NkFamilleCategorie::Physique)) {
						bool rag = AnimPhysicsEnabled();
						if (I.Case("Ragdoll (NKPhysics)", rag)) {
							Executer(A_RAGDOLL);
						}
						bool com = AnimShowCOM();
						if (I.Case("Centre de masse", com)) {
							Executer(A_COM);
						}
						if (AnimShowCOM()) {
							I.Ligne(AnimCOMRegimeLabel());
						}
						I.FinCarte();
					}
					(void)I.Fin(nullptr, false);
				}
				c.ctx.dl.PopClipRect();
			}

			// ── Le tiroir : FRISE au premier plan, graphe d'etats, contenu... ──
			void Tiroir(NkFamilleCtx &c) {
				const NkRect &zone = plan.tiroir;
				if (!plan.voirTiroir || zone.w < 8.f || zone.h < 40.f) {
					return;
				}
				static const char *const kOnglets[5] = {"Frise", "Graphe d'états", "Contenu", "Journal", "Terminal"};
				// (02/10) NKANIMA_TIROIR=N ouvre l'onglet N au depart (captures hors ecran).
				static bool tiroirLu = false;
				if (!tiroirLu) {
					tiroirLu = true;
					if (const char *t = std::getenv("NKANIMA_TIROIR")) {
						const int32 n = std::atoi(t);
						ongletTiroir = n >= 0 && n < 5 ? n : ongletTiroir;
					}
				}
				const float32 h = NkFamilleCotes::kOngletPanneau;
				(void)NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, h}, kOnglets, 5, ongletTiroir);
				const NkRect r{zone.x, zone.y + h, zone.w, zone.h - h};
				c.ctx.dl.AddRectFilled(r, pal.panneau);
				c.ctx.dl.PushClipRect(r, true);
				if (ongletTiroir == 0) {
					// ═══ ACCROCHE FRISE ═══════════════════════════════════════════
					// (02/10) LA FRISE PARTAGEE de NKEditorKit (Frise/NkAnimaFrise.cpp),
					// celle de la page Animation d'UnkenyEditor, facon Sequencer d'UE5 :
					// poses-cles, arbre des os et leurs courbes, transport, plage,
					// marqueurs. Elle prend tout le rectangle `r`, au theme de la face.
					NkEditorFrameContext ec;
					ec.ui = &c.ctx;
					ec.dt = dt;
					NkAnimaThemeFrise(theme);
					NkAnimaDessinerFriseZone(ec, r.x, r.y, r.w, r.h); // AnimUpdate(dt) : une fois par image
					mFriseVue = true;
					NkAnimaGrapheCache();
				} else if (ongletTiroir == 1) {
					// ═══ ACCROCHE GRAPHE ══════════════════════════════════════════
					// (02/10) LE GRAPHE D'ETATS PARTAGE (Frise/NkAnimaGraphe.cpp), celui
					// de la page Animateur d'UnkenyEditor : etats, arbres de melange,
					// couches masquees, courbes de fondu ; .nkanimctl a cote du modele ;
					// l'apercu joue la pose melangee dans la vue 3D.
					NkEditorFrameContext ec;
					ec.ui = &c.ctx;
					ec.dt = dt;
					NkAnimaDessinerGraphe(ec, r.x, r.y, r.w, r.h, theme, mModele.CStr());
				} else if (ongletTiroir == 2) {
					NkAnimaGrapheCache();
					(void)NkFamilleDessinerContenu(c, r, contenu, kNatures, static_cast<int32>(sizeof(kNatures) / sizeof(kNatures[0])), dt);
				} else if (ongletTiroir == 3) {
					NkAnimaGrapheCache();
					NkFamilleDessinerJournal(c, r, journal, dt);
				} else {
					NkAnimaGrapheCache();
					terminal.Dessiner(c.ctx, c.ctx.dl, r, theme);
				}
				c.ctx.dl.PopClipRect();
			}

			NkString mModele;
			NkString mMessage;
			float32 mMessageAge = 99.f;
			bool mOrbite = false;
			NkVec2 mAvant{0.f, 0.f};
			bool mFriseVue = false;
	};

	// =========================================================================
	// L'HOTE : la fenetre sans cadre, le rendu NKRHI, AnimBridge
	// =========================================================================
	namespace {
		void Avant3D(NkICommandBuffer *cmd, void *user) {
			auto *r = static_cast<NkEditorRHIRenderer *>(user);
			Anim3DRenderOffscreen(cmd);
			Anim3DRegisterInto(&r->GetBackend(), ANIM_VIEWPORT_TEXID);
		}
	} // namespace

	int NkAnimaLancerFace(const char *modele, int api, float secondes, bool horsEcran) {
		NkWindowConfig wc;
		wc.title = "NkAnimaEditor";
		wc.width = 1280;
		wc.height = 760;
		wc.minWidth = 900;
		wc.minHeight = 560;
		wc.centered = true;
		wc.resizable = true;
		// SANS CADRE, comme UnkenyEditor : la barre de titre est peinte.
		wc.frame = false;
		wc.bgColor = 0x141414FFu;
		if (horsEcran) {
			wc.title = "NkAnimaEditor -- CAPTURE (pas le produit, se ferme seule)";
			wc.centered = false;
			wc.x = -20000;
			wc.y = -20000;
			wc.noActivate = true;
			wc.clickThrough = true;
			wc.native.utilityWindow = true;
		}
		NkWindow fenetre;
		if (!fenetre.Create(wc)) {
			logger.Error("[NkAnimaEditor] fenetre refusee\n");
			return -1;
		}
		static NkEditorRHIRenderer rhi;
		if (!rhi.Init(fenetre, static_cast<NkEditorGfxApi>(api))) {
			logger.Error("[NkAnimaEditor] rendu NKRHI refuse\n");
			return -1;
		}
		static NkAnimaFace face(modele);
		const math::NkVec2u t0 = rhi.Size();
		if (!face.InitialiserGui(static_cast<int32>(t0.x), static_cast<int32>(t0.y))) {
			return -1;
		}
		nkgui::NkGuiFont *polices[3];
		const int32 n = face.Polices(polices, 3);
		for (int32 k = 0; k < n; ++k) {
			(void)rhi.UploadFontGray8(polices[k]->TexId(), polices[k]->pixels, polices[k]->atlasW, polices[k]->atlasH);
		}
		face.terminal.police = face.PoliceMono();
		(void)AnimInit(modele);
		Anim3DSetSharedDevice(rhi.GetDevice());
		rhi.SetPreUI(&Avant3D, &rhi);
		logger.Info("[NkAnimaEditor] face d'UE5 (R32) : pieces NKEditorKit/Famille, vue 3D AnimBridge\n");

		NkClock horloge;
		float32 ecoule = 0.f;
		bool enMarche = true;
		while (enMarche && fenetre.IsOpen()) {
			while (NkEvent *ev = NkEvents().PollEvent()) {
				if (ev->As<NkWindowCloseEvent>() != nullptr) {
					enMarche = false;
				} else if (const auto *rs = ev->As<NkWindowResizeEvent>()) {
					rhi.OnResize(static_cast<uint32>(rs->GetWidth()), static_cast<uint32>(rs->GetHeight()));
				}
				face.LireEvenement(*ev);
			}
			if (!enMarche) {
				break;
			}
			float32 dt = horloge.Tick().delta;
			dt = dt <= 0.f ? 1.f / 60.f : (dt > 0.1f ? 0.1f : dt);
			ecoule += dt;
			face.dt = dt;
			const math::NkVec2u taille = rhi.Size();
			rhi.BeginFrame(); // la vue 3D se rend ici (Avant3D)
			const nkgui::NkGuiDrawList &dl =
				face.Trame(dt, static_cast<int32>(taille.x), static_cast<int32>(taille.y), fenetre.IsMaximized());
			rhi.SubmitDrawList(dl, taille.x, taille.y);
			rhi.EndFrame();
			face.AppliquerFenetre(fenetre);
			face.terminal.Pomper();
			if (face.QuitterDemande() || (secondes > 0.f && ecoule >= secondes)) {
				enMarche = false;
			}
		}
		face.terminal.FermerTout();
		if (rhi.GetDevice() != nullptr) {
			rhi.GetDevice()->WaitIdle();
		}
		return 0;
	}

} // namespace nkanima
