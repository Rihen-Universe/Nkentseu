// =============================================================================
// NkEditeurBancUe5.cpp — `UnkenyEditor --selftest` : l'ETAPE 2 d'Unreal
//
// Les retours de Rihen du 01/10 apres essai (document 02), chacun dans la
// VRAIE trame (NkEditeurBancTrame.h), avec une contre-epreuve de mutation
// citee dans le commit qui l'introduit.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (u1)  le RENOMMAGE en place du Content Browser est un vrai champ : F2
//         choisit le nom entier (la frappe le remplace) ; un double-clic choisit
//         UN MOT et n'ouvre pas l'asset ; glisser dans le champ choisit des
//         lettres et ne tire pas la carte ; Origine, Maj+fleches, Ctrl+X / V,
//         Ctrl+A ; un « é » s'ecrit et s'efface d'un coup ; Echap annule (le
//         fichier garde son nom), Entree valide (l'extension reste)
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			int32 gE = 0, gR = 0;
			void Temoin(bool ok, const char *quoi, float32 v) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(v));
				(ok ? gR : gE)++;
			}
			bool Contient(const NkString &s, const char *mot) {
				return std::strstr(s.CStr(), mot) != nullptr;
			}
		} // namespace

		int32 NkEditeurLancerBancUe5() {
			gE = gR = 0;
			std::printf("\nUnkenyEditor — banc de l'etape 2 d'Unreal (retours du 01/10)\n\n");
			NkEditeurModele *pm = memory::NkGetDefaultAllocator().New<NkEditeurModele>(); // gros : sur le tas
			NkEditeurModele &m = *pm;
			NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
			NkEditeurNouvelleScene(m);
			const NkString cheminAvant = m.chemin;

			// (u1) LE RENOMMAGE EN PLACE EST UN VRAI CHAMP (retour 1 de Rihen).
			{
				NkDirectory::Delete("banc_u1", true);
				NkDirectory::CreateRecursive("banc_u1/projet/Contenu");
				NkFile::WriteAllText("banc_u1/projet/Contenu/vieille-caisse.png", "a");
				m.chemin = NkString("banc_u1/projet/scene.nkscene");
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.contenuCorbeille = false;
				t.Trame();
				t.Trame();
				auto Tampon = [&]() { return NkString(ui.contenu.renommeTampon); };
				// Le x d'une position du texte dans le champ (transparent : 2 px de marge).
				auto XDe = [&](const char *avant) { return ui.contenuRenommeRect.x + 2.f + t.police->MeasureWidth(avant); };
				const bool vu = t.CliquerCarte("Contenu/vieille-caisse.png");
				t.Touche(nkgui::NkGuiKey::F2);
				const bool ouvert = ui.renommeChemin == NkString("Contenu/vieille-caisse.png") && Tampon() == NkString("vieille-caisse") &&
									ui.contenuRenommeRect.w > 0.f;
				// (a) F2 choisit le nom ENTIER : la frappe le remplace
				t.Taper("z");
				const bool toutChoisi = Tampon() == NkString("z");
				// Ctrl+A puis la frappe : tout remplace, encore
				t.Touche(nkgui::NkGuiKey::A, true);
				t.Taper("vieille-caisse");
				const bool ctrlA = Tampon() == NkString("vieille-caisse");
				// (b) le DOUBLE-CLIC choisit UN MOT, et n'ouvre pas l'asset
				m.message = NkString();
				const float32 yc = ui.contenuRenommeRect.y + ui.contenuRenommeRect.h * 0.5f;
				t.DoubleClic(XDe("vieille-ca"), yc);
				t.Taper("boite");
				const bool mot = Tampon() == NkString("vieille-boite") && !Contient(m.message, "glissez") && !ui.renommeChemin.Empty();
				// (c) GLISSER dans le champ choisit des lettres, ne tire pas la carte
				// (mesure PENDANT le geste : au lacher, le composant oublie ce qui
				// etait arme)
				bool pasTire = true;
				{
					const float32 x0 = ui.contenuRenommeRect.x + 3.f, x1 = XDe("vieille");
					t.Ctx().input.mousePos = nkgui::NkVec2{x0, yc};
					t.souris.Appui(t.Ctx().input, 0);
					t.Trame();
					for (int32 k = 1; k <= 4; ++k) {
						t.Ctx().input.mousePos = nkgui::NkVec2{x0 + (x1 - x0) * static_cast<float32>(k) / 4.f, yc};
						t.Trame();
						pasTire = pasTire && ui.contenu.glisserChemin.Empty() && ui.contenu.armeChemin.Empty();
					}
					t.souris.Relache(t.Ctx().input, 0);
					t.Trame();
				}
				t.Taper("jeune");
				const bool lettres = pasTire && Tampon() == NkString("jeune-boite");
				// (d) Origine, Maj+Droite x5, Ctrl+X, Fin, Ctrl+V : « -boitejeune »
				t.Touche(nkgui::NkGuiKey::Home);
				for (int32 k = 0; k < 5; ++k) {
					t.Touche(nkgui::NkGuiKey::Right, false, true);
				}
				t.Touche(nkgui::NkGuiKey::X, true);
				const bool coupe = Tampon() == NkString("-boite") && t.pressePapiers == NkString("jeune");
				t.Touche(nkgui::NkGuiKey::End);
				t.Touche(nkgui::NkGuiKey::V, true);
				const bool colle = Tampon() == NkString("-boitejeune");
				// (e) un « é » s'ecrit (UTF-8, deux octets) et s'efface D'UN COUP
				t.Taper("\xC3\xA9");
				const bool accent = Tampon() == NkString("-boitejeune\xC3\xA9");
				t.Touche(nkgui::NkGuiKey::Backspace);
				const bool efface = Tampon() == NkString("-boitejeune");
				// (f) Echap ANNULE : le fichier garde son nom
				t.Touche(nkgui::NkGuiKey::Escape);
				const bool annule = ui.renommeChemin.Empty() && NkFile::Exists("banc_u1/projet/Contenu/vieille-caisse.png");
				// (g) F2, la frappe, Entree VALIDE (l'extension reste)
				t.Fermer();
				t.CliquerCarte("Contenu/vieille-caisse.png");
				t.Touche(nkgui::NkGuiKey::F2);
				t.Taper("caisse-rouge");
				t.Touche(nkgui::NkGuiKey::Enter);
				const bool valide = ui.renommeChemin.Empty() && NkFile::Exists("banc_u1/projet/Contenu/caisse-rouge.png") &&
									!NkFile::Exists("banc_u1/projet/Contenu/vieille-caisse.png");
				if (!(vu && ouvert && toutChoisi && ctrlA && mot && lettres && coupe && colle && accent && efface && annule && valide)) {
					std::printf("        vu %d ouvert %d tout %d ctrlA %d mot %d lettres %d (tire %d) coupe %d colle %d accent %d efface %d "
								"annule %d valide %d ; tampon « %s »\n",
								vu, ouvert, toutChoisi, ctrlA, mot, lettres, !pasTire, coupe, colle, accent, efface, annule, valide,
								ui.contenu.renommeTampon);
				}
				Temoin(vu && ouvert && toutChoisi && ctrlA && mot && lettres && coupe && colle && accent && efface && annule && valide,
					   "(u1) renommer : nom choisi, double-clic = mot, glisser = lettres, Ctrl+X/V, Echap, Entree",
					   static_cast<float32>(vu + ouvert + toutChoisi + ctrlA + mot + lettres + coupe + colle + accent + efface + annule +
											valide));
				NkDirectory::Delete("banc_u1", true);
			}

			m.chemin = cheminAvant;
			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC UE5 REUSSI" : "BANC UE5 EN ECHEC", gR, gE, gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
