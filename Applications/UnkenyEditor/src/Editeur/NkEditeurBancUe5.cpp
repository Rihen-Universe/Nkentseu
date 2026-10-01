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
//   (u3)  DOUBLE-CLIC SUR UNE SCENE du Content Browser : elle s'ouvre, et le
//         navigateur ne bouge pas -- meme racine du Contenu, meme dossier
//         courant, memes cartes, memes dossiers au rail ; une scene rangee dans
//         un dossier lui-meme nomme « Contenu » garde le projet (retenu) ; sans
//         memoire, une scene rangee dans le Contenu donne son projet
//   (u4)  la TEXTURE D'UN SPRITE depuis les Details (reference d'asset
//         d'Unreal) : le champ est la ; sa liste deroulante ne propose que les
//         IMAGES du Contenu et se cherche (taper, Entree) ; Ctrl+Z / Ctrl+Y ;
//         « utiliser la selection du Content Browser » ; « parcourir » saute a
//         l'asset ; une image GLISSEE sur le champ, puis SUR le sprite dans la
//         vue, devient sa texture (aucune entite creee) ; lachee dans le vide,
//         elle pose toujours un sprite neuf
//   (u5)  le TRANSFORM d'Unreal : chaque composante d'axe porte un LISERE de
//         sa couleur (rouge X, vert Y, bleu Z) colle au bord gauche de son
//         champ, de toute sa hauteur, au lieu d'une lettre
//   (u6)  CONSTRUIRE : un projet sans reglages sort dans <projet>/Construit ;
//         « Parcourir… » ouvre LE selecteur du kit en mode dossier ; le dossier
//         choisi va au champ et le projet le RETIENT (relatif, `.nkprojet`) ;
//         un autre projet reprend le sien ; --sortie= garde la priorite ; une
//         cle inconnue du fichier survit a la reecriture
//   (u7)  le JOURNAL du tiroir comme l'Output Log d'Unreal : lignes sans codes
//         ANSI, classees et colorees ; puces Erreurs (la seule erreur) et
//         Avertissements (avertissements et erreurs) ; la recherche se tape
//         (la scene n'en recoit rien) ; Copier ; Effacer
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurProjet.h"
#include "Editeur/NkEditeurReferences.h"
#include "Livraison/NkEditeurDeroulement.h"

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
			uint32 NbEntites(NkScene &s) {
				NkVector<ecs::NkEntityId> ids;
				s.Entites(ids);
				return static_cast<uint32>(ids.Size());
			}
			/// La premiere entite dont le nom COMMENCE par `prefixe` (« Caisse 1 »...).
			ecs::NkEntityId Par(NkScene &s, const char *prefixe) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				const usize n = std::strlen(prefixe);
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (std::strncmp(e.nom, prefixe, n) == 0 && !t.IsValid()) {
						t = id;
					}
				});
				return t;
			}
			/// Une VRAIE image PNG (1 x 1, rouge) : la texture se charge pour de bon.
			void EcrirePng(const char *chemin) {
				static const uint8 kPng[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
											 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
											 0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
											 0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
											 0x44, 0xAE, 0x42, 0x60, 0x82};
				NkVector<nk_uint8> octets;
				for (uint32 i = 0; i < sizeof(kPng); ++i) {
					octets.PushBack(kPng[i]);
				}
				NkFile::WriteAllBytes(chemin, octets);
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

			// (u3) DOUBLE-CLIC SUR UNE SCENE : elle s'ouvre, le navigateur ne bouge
			// pas (retour 3 de Rihen : « tout le navigateur change, les fichiers
			// disparaissent, les dossiers aussi »).
			{
				NkDirectory::Delete("banc_u3", true);
				NkDirectory::CreateRecursive("banc_u3/projet/Contenu/Scenes");
				NkDirectory::CreateRecursive("banc_u3/projet/Contenu/Decor/Contenu");
				NkFile::WriteAllText("banc_u3/projet/Contenu/a.png", "a");
				NkFile::WriteAllText("banc_u3/projet/Contenu/Scenes/b.png", "b");
				NkEditeurNouvelleScene(m);
				m.projet = NkString();
				m.chemin = NkString("banc_u3/projet/Contenu/Scenes/niveau.nkscene");
				const bool e1 = NkEditeurSauver(m);
				m.chemin = NkString("banc_u3/projet/Contenu/Decor/Contenu/cachee.nkscene");
				const bool e2 = NkEditeurSauver(m);
				m.chemin = NkString("banc_u3/projet/scene.nkscene");
				const bool e3 = NkEditeurSauver(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.contenuCorbeille = false;
				t.Trame();
				t.Trame();
				NkEditeurRetenirEmpreinte(m, ui);
				const NkString racine = NkEditeurDossierContenu(m);
				auto AllerDans = [&](const char *dossier) {
					NkEditeurCadre c = t.Cadre();
					ui.contenuMenuChemin = NkString(dossier);
					ui.contenuMenuDossier = true;
					NkEditeurExecuter(c, NK_A_CONTENU_OUVRIR);
					ui.contenuMenuChemin = NkString();
					t.Trame();
					t.Trame();
				};
				auto DoubleCliquer = [&](const char *chemin) {
					const int32 k = t.Carte(chemin);
					if (k < 0) {
						return false;
					}
					const nkgui::NkRect r = ui.contenuCartes[static_cast<uint32>(k)];
					t.DoubleClic(r.x + r.w * 0.5f, r.y + r.h * 0.3f);
					t.Trame();
					t.Trame();
					return true;
				};
				auto Finit = [](const NkString &s, const char *fin) {
					const usize n = std::strlen(fin);
					return s.Length() >= n && std::strcmp(s.CStr() + s.Length() - n, fin) == 0;
				};
				// (a) dans « Scenes », double-clic sur niveau.nkscene
				AllerDans("Contenu/Scenes");
				const uint32 cartes = static_cast<uint32>(ui.contenu.entries.Size());
				const uint32 rail = static_cast<uint32>(ui.contenuSousDossiers.Size());
				const bool vise = DoubleCliquer("Contenu/Scenes/niveau.nkscene");
				const bool ouverte = Finit(m.chemin, "Contenu/Scenes/niveau.nkscene") && ui.confirmation == NK_A_AUCUNE;
				const bool memeRacine = NkEditeurDossierContenu(m) == racine;
				const bool memeDossier = ui.contenuDossier == NkString("Scenes");
				const bool memeVue = static_cast<uint32>(ui.contenu.entries.Size()) == cartes && t.Carte("Contenu/Scenes/b.png") >= 0 &&
									 static_cast<uint32>(ui.contenuSousDossiers.Size()) == rail && rail >= 3u;
				// (b) une scene rangee dans un dossier NOMME « Contenu » : le projet retenu
				AllerDans("Contenu/Decor/Contenu");
				const bool vise2 = DoubleCliquer("Contenu/Decor/Contenu/cachee.nkscene");
				const bool niche = Finit(m.chemin, "Decor/Contenu/cachee.nkscene") && NkEditeurDossierContenu(m) == racine &&
								   ui.contenuDossier == NkString("Decor/Contenu");
				// (c) sans memoire : une scene rangee dans le Contenu donne son projet
				m.projet = NkString();
				m.chemin = NkString("banc_u3/projet/Contenu/Scenes/niveau.nkscene");
				const bool regle = NkEditeurDossierProjet(m) == NkString("banc_u3/projet/");
				m.chemin = NkString("banc_u3/projet/scene.nkscene");
				const bool historique = NkEditeurDossierProjet(m) == NkString("banc_u3/projet/");
				if (!(e1 && e2 && e3 && vise && ouverte && memeRacine && memeDossier && memeVue && vise2 && niche && regle && historique)) {
					std::printf("        ecrits %d%d%d vise %d ouverte %d racine %d dossier %d (« %s ») vue %d (%u/%u cartes, %u/%u rail) ; "
								"niche %d %d ; regle %d historique %d ; racine « %s »\n",
								e1, e2, e3, vise, ouverte, memeRacine, memeDossier, ui.contenuDossier.CStr(), memeVue,
								static_cast<uint32>(ui.contenu.entries.Size()), cartes, static_cast<uint32>(ui.contenuSousDossiers.Size()),
								rail, vise2, niche, regle, historique, NkEditeurDossierContenu(m).CStr());
				}
				Temoin(e1 && e2 && e3 && vise && ouverte && memeRacine && memeDossier && memeVue && vise2 && niche && regle && historique,
					   "(u3) double-clic sur une scene : ouverte, le navigateur ne bouge pas (racine, dossier, cartes)",
					   static_cast<float32>(cartes));
				m.projet = NkString();
				NkDirectory::Delete("banc_u3", true);
			}

			// (u4) LA TEXTURE D'UN SPRITE (retour 4 de Rihen : « comment mettre une
			// texture sur un sprite ? aujourd'hui rien ne le permet »).
			{
				NkDirectory::Delete("banc_u4", true);
				NkDirectory::CreateRecursive("banc_u4/projet/Contenu/Textures");
				EcrirePng("banc_u4/projet/Contenu/Textures/bleu.png");
				EcrirePng("banc_u4/projet/Contenu/Textures/rouge.png");
				NkFile::WriteAllText("banc_u4/projet/Contenu/Textures/son.wav", "s");
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				m.projet = NkString();
				m.chemin = NkString("banc_u4/projet/scene.nkscene");
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				NkEditeurCadre c = t.Cadre();
				ui.contenuCorbeille = false;
				const ecs::NkEntityId caisse = Par(m.scene, "Caisse");
				m.selection = caisse;
				m.aSelection = caisse.IsValid();
				t.Trame();
				t.Trame();
				// ⚠️ Annuler / refaire RESTAURE la scene : les poignees changent, on
				//    retrouve la caisse par son nom a chaque mesure.
				auto Tex = [&]() {
					const ecs::NkEntityId e = Par(m.scene, "Caisse");
					const NkSprite2D *sp = e.IsValid() ? m.scene.Monde().Get<NkSprite2D>(e) : nullptr;
					return sp != nullptr ? NkEditeurNavDeTexture(m, sp->texId) : NkString("?");
				};
				auto Milieu = [](const nkgui::NkRect &r) { return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f}; };
				const NkString avant = Tex();
				// (a) le champ est la (vignette, liste, selection, parcourir)
				const bool champ = ui.detailsTexture.w > 0.f && ui.detailsTextureListe.w > 0.f && ui.detailsTextureSelection.w > 0.f &&
								   ui.detailsTextureParcourir.w > 0.f && avant.Empty();
				// (b) la LISTE : les seules images, cherchee (« rou », Entree)
				const nkgui::NkVec2 pl = Milieu(ui.detailsTextureListe);
				t.Clic(0, pl.x, pl.y);
				const bool liste = ui.menu == NkMenuEditeur::NK_TEXTURE_SPRITE && ui.texturesProposees.Size() == 2u &&
								   ui.texturesProposees[0] == NkString("Contenu/Textures/bleu.png") &&
								   ui.texturesProposees[1] == NkString("Contenu/Textures/rouge.png");
				t.Taper("rou");
				t.Touche(nkgui::NkGuiKey::Enter);
				const bool choisie = Tex() == NkString("Contenu/Textures/rouge.png") && ui.menu == NkMenuEditeur::NK_AUCUN;
				// (c) Ctrl+Z la retire, Ctrl+Y la remet
				t.Fermer();
				t.Touche(nkgui::NkGuiKey::Z, true);
				const bool annulee = Tex() == avant;
				t.Touche(nkgui::NkGuiKey::Y, true);
				const bool refaite = Tex() == NkString("Contenu/Textures/rouge.png");
				// (d) « utiliser la selection du Content Browser » : bleu.png choisie
				ui.contenuMenuChemin = NkString("Contenu/Textures");
				ui.contenuMenuDossier = true;
				NkEditeurExecuter(c, NK_A_CONTENU_OUVRIR);
				ui.contenuMenuChemin = NkString();
				t.Trame();
				t.Trame();
				const bool carteBleue = t.CliquerCarte("Contenu/Textures/bleu.png");
				const nkgui::NkVec2 ps = Milieu(ui.detailsTextureSelection);
				t.Clic(0, ps.x, ps.y);
				const bool selection = carteBleue && Tex() == NkString("Contenu/Textures/bleu.png");
				// (e) « parcourir » : le navigateur, a la racine, saute a l'asset
				ui.contenuMenuChemin = NkString();
				NkEditeurExecuter(c, NK_A_CONTENU_RACINE);
				t.Trame();
				const nkgui::NkVec2 pp = Milieu(ui.detailsTextureParcourir);
				t.Clic(0, pp.x, pp.y);
				t.Trame();
				const bool parcourir = ui.contenuDossier == NkString("Textures") && ui.contenuActif == NkString("Contenu/Textures/bleu.png") &&
									   ui.voirTiroir && ui.ongletTiroir == 0;
				// (f) GLISSER rouge.png du Content Browser sur le CHAMP des Details
				auto Glisser = [&](nkgui::NkVec2 de, nkgui::NkVec2 a) {
					t.Ctx().input.mousePos = de;
					t.souris.Appui(t.Ctx().input, 0);
					t.Trame();
					t.Ctx().input.mousePos = nkgui::NkVec2{(de.x + a.x) * 0.5f, (de.y + a.y) * 0.5f + 3.f};
					t.Trame();
					t.Ctx().input.mousePos = a;
					t.Trame();
					t.souris.Relache(t.Ctx().input, 0);
					t.Trame();
				};
				auto CentreCarte = [&](const char *chemin) {
					const int32 k = t.Carte(chemin);
					return k < 0 ? nkgui::NkVec2{-1.f, -1.f}
								 : nkgui::NkVec2{ui.contenuCartes[static_cast<uint32>(k)].x + 20.f, ui.contenuCartes[static_cast<uint32>(k)].y + 20.f};
				};
				const uint32 n0 = NbEntites(m.scene);
				Glisser(CentreCarte("Contenu/Textures/rouge.png"), Milieu(ui.detailsTexture));
				const bool surChamp = Tex() == NkString("Contenu/Textures/rouge.png") && NbEntites(m.scene) == n0;
				// (g) GLISSER bleu.png SUR la caisse dans la vue : sa texture, pas un sprite neuf
				const ecs::NkEntityId caisse2 = Par(m.scene, "Caisse");
				const NkTransform2D *tc = caisse2.IsValid() ? m.scene.Monde().Get<NkTransform2D>(caisse2) : nullptr;
				const NkVec2f ecran = tc != nullptr ? m.scene.Camera().MondeVersEcran(tc->position) : NkVec2f(-1.f, -1.f);
				Glisser(CentreCarte("Contenu/Textures/bleu.png"), nkgui::NkVec2{ecran.x, ecran.y});
				const bool surSprite = Tex() == NkString("Contenu/Textures/bleu.png") && NbEntites(m.scene) == n0;
				// (h) lachee dans le VIDE de la vue : un sprite neuf, comme avant
				Glisser(CentreCarte("Contenu/Textures/rouge.png"), nkgui::NkVec2{ui.viseur.x + 12.f, ui.viseur.y + 12.f});
				const bool neuf = NbEntites(m.scene) == n0 + 1u && Tex() == NkString("Contenu/Textures/bleu.png");
				const bool ok = champ && liste && choisie && annulee && refaite && selection && parcourir && surChamp && surSprite && neuf;
				if (!ok) {
					std::printf("        champ %d liste %d (%u) choisie %d annulee %d refaite %d selection %d parcourir %d (« %s ») "
								"surChamp %d surSprite %d (ecran %.0f,%.0f) neuf %d ; texture « %s »\n",
								champ, liste, static_cast<uint32>(ui.texturesProposees.Size()), choisie, annulee, refaite, selection, parcourir,
								ui.contenuDossier.CStr(), surChamp, surSprite, static_cast<double>(ecran.x), static_cast<double>(ecran.y), neuf,
								Tex().CStr());
				}
				Temoin(ok, "(u4) texture d'un sprite : liste filtree, Ctrl+Z/Y, selection, parcourir, glisser (champ, sprite)",
					   static_cast<float32>(champ + liste + choisie + annulee + refaite + selection + parcourir + surChamp + surSprite + neuf));
				m.projet = NkString();
				NkDirectory::Delete("banc_u4", true);
			}

			// (u5) LE TRANSFORM D'UNREAL : des LISERES de couleur, pas des lettres
			// (retour 5 de Rihen, `ue58_a.png` : Location / Rotation / Scale).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurOublierHistorique(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				m.selection = Par(m.scene, "Caisse");
				m.aSelection = m.selection.IsValid();
				t.Trame();
				t.Trame();
				uint32 rouges = 0, verts = 0, bleus = 0, colles = 0;
				for (uint32 i = 0; i < ui.detailsLiseres.Size(); ++i) {
					const NkEditeurInterface::NkLisereAxe &l = ui.detailsLiseres[i];
					const nkgui::NkColor c(l.couleur);
					rouges += c.r > 200u && c.g < 120u ? 1u : 0u;
					verts += c.g > 180u && c.r < 120u ? 1u : 0u;
					bleus += c.b > 200u && c.r < 120u ? 1u : 0u;
					// colle au bord GAUCHE du champ, de TOUTE sa hauteur, etroit
					colles += (l.lisere.x == l.champ.x && l.lisere.y == l.champ.y && l.lisere.h == l.champ.h && l.lisere.w > 0.f &&
							   l.lisere.w <= 6.f)
								  ? 1u
								  : 0u;
				}
				const uint32 n = static_cast<uint32>(ui.detailsLiseres.Size());
				// Transform : X, Y (position), Z (rotation), X, Y (echelle) ; le sprite
				// en ajoute (taille L / H, pivot X / Y).
				Temoin(n >= 5u && colles == n && rouges >= 2u && verts >= 2u && bleus >= 1u,
					   "(u5) Transform : liseres rouge X / vert Y / bleu Z colles au bord gauche du champ (nombre)", static_cast<float32>(n));
			}

			// (u6) CONSTRUIRE : « Parcourir… », le dossier de sortie RETENU PAR PROJET
			// (retour 6 de Rihen : « pas de bouton pour choisir le dossier de
			// generation du jeu »).
			{
				NkDirectory::Delete("banc_u6", true);
				NkDirectory::CreateRecursive("banc_u6/projet/Contenu");
				NkDirectory::CreateRecursive("banc_u6/projet/Jeux");
				NkDirectory::CreateRecursive("banc_u6/autre/Contenu");
				NkEditeurNouvelleScene(m);
				m.projet = NkString();
				m.chemin = NkString("banc_u6/autre/scene.nkscene");
				const bool e1 = NkEditeurSauver(m);
				m.chemin = NkString("banc_u6/projet/scene.nkscene");
				const bool e2 = NkEditeurSauver(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				NkEditeurConstruction &k = *t.pcons;
				NkEditeurSelecteurEtat &sel = *t.psel;
				auto Normal = [](const char *s) {
					NkString n;
					for (const char *p = s; *p != '\0'; ++p) {
						n.Append(*p == '\\' ? '/' : *p);
					}
					return n;
				};
				auto Finit = [&](const char *s, const char *fin) {
					const NkString n = Normal(s);
					const usize l = std::strlen(fin);
					return n.Length() >= l && std::strcmp(n.CStr() + n.Length() - l, fin) == 0;
				};
				auto Ouvrir = [&]() {
					k.ouverte = false;
					ui.construireDemande = true;
					t.Trame();
					t.Trame();
				};
				// (a) un projet SANS reglages (d'avant) : <projet>/Construit
				Ouvrir();
				const bool defaut = k.ouverte && Finit(k.sortie, "banc_u6/projet/Construit") && !NkFile::Exists("banc_u6/projet/.nkprojet");
				// (b) « Parcourir… » ouvre LE selecteur, en mode dossier
				t.Clic(0, k.boutonParcourir.x + k.boutonParcourir.w * 0.5f, k.boutonParcourir.y + k.boutonParcourir.h * 0.5f);
				const bool ouvre = k.boutonParcourir.w > 0.f && sel.pickerOpen && sel.usage == NkUsageSelecteur::NK_DOSSIER_SORTIE &&
								   sel.pickerFor == editorkit::NkSelecteurOuvrirDossier && k.ouverte;
				// (c) « Choisir ce dossier » : le champ le prend, le PROJET le retient
				// (relatif : il est dedans)
				sel.AllerA("banc_u6/projet/Jeux");
				t.Trame();
				{
					const nkgui::NkVec2 ok = t.Confirmer();
					t.Clic(0, ok.x, ok.y);
				}
				const bool choisi = !sel.pickerOpen && Finit(k.sortie, "banc_u6/projet/Jeux") && k.ouverte &&
									NkEditeurProjetLire(m, NK_PROJET_SORTIE) == NkString("Jeux");
				// (d) un AUTRE projet reprend le sien ; (e) le premier, le sien
				m.chemin = NkString("banc_u6/autre/scene.nkscene");
				Ouvrir();
				const bool autre = Finit(k.sortie, "banc_u6/autre/Construit");
				m.chemin = NkString("banc_u6/projet/scene.nkscene");
				Ouvrir();
				const bool retenu = Finit(k.sortie, "banc_u6/projet/Jeux");
				// (f) --sortie= garde la priorite
				k.sortieImposee = true;
				std::snprintf(k.sortie, sizeof(k.sortie), "%s", "banc_u6/impose");
				m.chemin = NkString("banc_u6/autre/scene.nkscene");
				Ouvrir();
				const bool impose = NkString(k.sortie) == NkString("banc_u6/impose");
				k.sortieImposee = false;
				// (g) retro-compatible : une cle INCONNUE (version future) survit a la
				// reecriture
				m.chemin = NkString("banc_u6/projet/scene.nkscene");
				NkFile::WriteAllText("banc_u6/projet/.nkprojet", "futur.cle=42\nconstruire.sortie=Jeux\n");
				const bool reecrit = NkEditeurRetenirSortie(m, "banc_u6/projet/Construit") &&
									 NkEditeurProjetLire(m, "futur.cle") == NkString("42") &&
									 NkEditeurProjetLire(m, NK_PROJET_SORTIE) == NkString("Construit");
				const bool ok = e1 && e2 && defaut && ouvre && choisi && autre && retenu && impose && reecrit;
				if (!ok) {
					std::printf("        ecrits %d%d defaut %d ouvre %d choisi %d autre %d retenu %d impose %d reecrit %d ; sortie « %s » ; retenue « %s »\n",
								e1, e2, defaut, ouvre, choisi, autre, retenu, impose, reecrit, k.sortie, NkEditeurProjetLire(m, NK_PROJET_SORTIE).CStr());
				}
				Temoin(ok, "(u6) Construire : Parcourir… (selecteur du kit), sortie retenue par projet, --sortie= prioritaire",
					   static_cast<float32>(defaut + ouvre + choisi + autre + retenu + impose + reecrit));
				k.ouverte = false;
				m.projet = NkString();
				NkDirectory::Delete("banc_u6", true);
			}

			// (u7) LE JOURNAL DU TIROIR, comme l'Output Log d'Unreal (retour 7 de
			// Rihen : « le tiroir Journal du bas n'a pas de filtres »).
			{
				NkEditeurNouvelleScene(m);
				NkEditeurBancTrame t(m);
				NkEditeurInterface &ui = t.Ui();
				ui.ongletTiroir = 1;
				// Les annonces de la scene neuve passent d'abord au journal ; puis on
				// le remplit de lignes connues.
				t.Trame();
				t.Trame();
				ui.journal.Clear();
				ui.journal.PushBack(NkString("[00:01]  Scene ouverte"));
				ui.journal.PushBack(NkString("\x1b[31mmain.cpp:3: error: boom\x1b[0m"));
				ui.journal.PushBack(NkString("x.cpp:7: warning: variable inutilisee"));
				ui.journal.PushBack(NkString("[00:02]  Texture posee sur la caisse"));
				ui.journal.PushBack(NkString("Build Successful"));
				t.Trame();
				t.Trame();
				auto Milieu = [](const nkgui::NkRect &r) { return nkgui::NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f}; };
				// (a) tout, sans un code ANSI, colore par niveau ; la plus recente en haut
				bool propre = ui.journalMontrees.Size() == 5u;
				for (uint32 i = 0; i < ui.journalMontrees.Size(); ++i) {
					propre = propre && std::strchr(ui.journalMontrees[i].CStr(), 0x1b) == nullptr;
				}
				const bool niveaux = propre && ui.journalMontrees[0] == NkString("Build Successful") &&
									 ui.journalNiveaux[0] == static_cast<uint8>(NkNiveauLigne::NK_SUCCES) &&
									 ui.journalNiveaux[3] == static_cast<uint8>(NkNiveauLigne::NK_ERREUR) &&
									 ui.journalNiveaux[2] == static_cast<uint8>(NkNiveauLigne::NK_AVERTISSEMENT);
				// (b) la puce « Erreurs » : la seule erreur ; (c) « Avertissements » :
				// avertissements ET erreurs (le filtre de « Construire »)
				t.Clic(0, Milieu(ui.journalPuces[2]).x, Milieu(ui.journalPuces[2]).y);
				const bool erreurs = ui.journalFiltre == 2 && ui.journalMontrees.Size() == 1u && Contient(ui.journalMontrees[0], "boom");
				t.Clic(0, Milieu(ui.journalPuces[1]).x, Milieu(ui.journalPuces[1]).y);
				const bool avertissements = ui.journalFiltre == 1 && ui.journalMontrees.Size() == 2u;
				t.Clic(0, Milieu(ui.journalPuces[0]).x, Milieu(ui.journalPuces[0]).y);
				// (d) la RECHERCHE : cliquer le champ, taper ; la scene n'en recoit rien
				t.Clic(0, Milieu(ui.journalRechercheRect).x, Milieu(ui.journalRechercheRect).y);
				const NkOutil outilAvant = m.outil;
				t.Taper("TEXTURE w");
				t.Touche(nkgui::NkGuiKey::Backspace);
				t.Touche(nkgui::NkGuiKey::Backspace);
				const bool recherche = ui.journalRechercheFocus && ui.journalMontrees.Size() == 1u &&
									   Contient(ui.journalMontrees[0], "Texture posee") && m.outil == outilAvant;
				// (e) Copier : les lignes montrees au presse-papiers
				t.pressePapiers = NkString();
				t.Clic(0, Milieu(ui.journalCopier).x, Milieu(ui.journalCopier).y);
				const bool copie = Contient(t.pressePapiers, "Texture posee") && !Contient(t.pressePapiers, "boom");
				// (f) Effacer
				t.Clic(0, Milieu(ui.journalEffacer).x, Milieu(ui.journalEffacer).y);
				const bool efface = ui.journal.Empty() && ui.journalMontrees.Empty();
				const bool ok = niveaux && erreurs && avertissements && recherche && copie && efface;
				if (!ok) {
					std::printf("        propre %d niveaux %d erreurs %d avertissements %d (%u) recherche %d (%u, « %s ») copie %d efface %d\n", propre,
								niveaux, erreurs, avertissements, static_cast<uint32>(ui.journalMontrees.Size()), recherche,
								static_cast<uint32>(ui.journalMontrees.Size()), ui.journalRecherche, copie, efface);
				}
				Temoin(ok, "(u7) Journal du tiroir : Tout / Avertissements / Erreurs, recherche, Copier, Effacer, sans ANSI",
					   static_cast<float32>(niveaux + erreurs + avertissements + recherche + copie + efface));
			}

			m.chemin = cheminAvant;
			memory::NkGetDefaultAllocator().Delete(pm);
			std::printf("\n%s : %d reussis, %d echec%s\n", gE == 0 ? "BANC UE5 REUSSI" : "BANC UE5 EN ECHEC", gR, gE, gE > 1 ? "s" : "");
			return gE == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
