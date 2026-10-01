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
//   (u2)  les CARTES : un nom long tient sur DEUX lignes au plus, et la
//         nature (le type gris) en est SEPAREE : un ecart net d'au moins 3 px
//         et un filet fin entre les deux
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"

#include "NKEditorKit/Components/NkRecordingPaint.h"
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

			// (u2) LES CARTES : le nom long (deux lignes au plus) et la nature SEPARES
			// (retour 2 de Rihen : « le nom se confond avec la nature »).
			{
				using namespace editorkit;
				NkComponentInstance inst(NkContentBrowserDecl());
				inst.SetVariantByName("unreal");
				NkContentBrowserStyle st;
				st.values = &inst;
				st.panelBg = 1;
				st.headerBg = 2;
				st.border = 3;
				st.text = 4;
				st.textMuted = 6;
				st.cardBg = 7;
				st.cardFooterBg = 8;
				st.activeMark = 9;
				st.chosenMark = 10;
				st.folderTint = 11;
				NkContentBrowserModel mod;
				mod.thumbSize = 96.f;
				NkAssetEntry f;
				f.name = NkString("Niveau1jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj");
				f.path = NkString("Contenu/Niveau1.nkscene");
				f.kindRole = 5;
				f.kindLabel = "Scène";
				mod.entries.PushBack(f);
				NkContentBrowserHooks h;
				NkComponentInput in;
				NkRecordingPaint rp;
				NkDrawContentBrowser(rp, in, NkPaintRect{0.f, 0.f, 1000.f, 400.f}, mod, st, h);
				int32 lignes = 0;
				float32 basNom = -1.f, hautType = -1.f, largeurMax = 0.f;
				for (uint32 i = 0; i < rp.cmds.Size(); ++i) {
					const NkPaintCmd &k = rp.cmds[i];
					if (k.op != NkPaintOp::Text) {
						continue;
					}
					if (k.text == NkString("Scène")) {
						hautType = k.y;
					} else if (k.text.StartsWith("Niveau1") || (k.text.Length() > 0 && k.text.CStr()[0] == 'j')) {
						++lignes;
						basNom = k.y + k.h > basNom ? k.y + k.h : basNom;
						largeurMax = k.w > largeurMax ? k.w : largeurMax;
					}
				}
				bool filet = false;
				for (uint32 i = 0; i < rp.cmds.Size(); ++i) {
					const NkPaintCmd &k = rp.cmds[i];
					filet = filet || (k.op == NkPaintOp::FillColor && k.h > 0.f && k.h <= 1.5f && k.y >= basNom - 0.01f &&
									  k.y + k.h <= hautType + 0.01f);
				}
				const float32 ecart = hautType - basNom;
				Temoin(lignes == 2 && largeurMax <= 96.f && ecart >= 3.f && filet,
					   "(u2) carte : nom long sur deux lignes, ecart et filet avant la nature (ecart px)", ecart);
			}

			m.chemin = cheminAvant;
			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC UE5 REUSSI" : "BANC UE5 EN ECHEC", gR, gE, gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
