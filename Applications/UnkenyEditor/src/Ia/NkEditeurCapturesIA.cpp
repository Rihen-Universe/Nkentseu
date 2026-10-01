// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Ia/NkEditeurCapturesIA.cpp
// DESCRIPTION: Les captures HORS ECRAN de l'IA (`--captures-ia=DOSSIER`) : aucune
//              fenetre, aucune souris reelle ; le faux serveur local repond, la
//              trame de banc peint l'editeur et le rasteriseur de NKGui l'ecrit
//              en PNG.
//
//   01  l'IA en ONGLET du groupe Details | Monde (le defaut) : une conversation
//   02  la meme, DETACHEE en panneau a part (Details et IA cote a cote)
//   03  le panneau a part REPLIE en bande (son chevron)
//   04  l'action faite (la caisse au centre, le bloc d'effet, son bouton)
//   05  la meme, ANNULEE (Ctrl+Z) : la caisse est partie
//   06  une suppression qui ATTEND la confirmation (Confirmer / Refuser)
//   07  la fenetre des reglages : la liste des modeles LUE SUR LE SERVEUR
//   08  le verdict de « Tester la connexion » quand le serveur est absent
//   09  la cle d'un fournisseur : jamais en clair
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Ia/NkEditeurHarnaisIA.h"

#include "NKGui/Core/NkGuiDrawListRaster.h"
#include "NKImage/Core/NkImage.h"

#include <cstdio>

namespace nkentseu {
	namespace editeur {

		namespace {
			bool Png(NkEditeurBancTrame &T, const NkString &chemin) {
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				nkgui::NkGuiDrawListRaster *ras = tas.New<nkgui::NkGuiDrawListRaster>();
				const int32 w = static_cast<int32>(T.W), h = static_cast<int32>(T.H);
				ras->Init(w, h);
				ras->Effacer(0x141414FFu);
				ras->PoserTexture(T.police->TexId(), T.police->pixels, T.police->atlasW, T.police->atlasH, 1);
				ras->Rasteriser(T.pctx->dl);
				ras->Rasteriser(T.pctx->dlOverlay);
				NkImage img = NkImage::Wrap(const_cast<uint8 *>(ras->Pixels()), w, h, NkImagePixelFormat::NK_RGBA32);
				const bool ok = img.SavePNG(chemin.CStr());
				std::printf("  capture %s : %s\n", chemin.CStr(), ok ? "ok" : "ECHEC");
				tas.Delete(ras);
				return ok;
			}
			void Trames(NkHarnaisIA &h, int32 n) {
				for (int32 k = 0; k < n; ++k)
					h.T().Trame();
			}
		} // namespace

		int32 NkEditeurCapturesIA(const char *dossier) {
			NkDirectory::CreateRecursive(dossier);
			NkHarnaisIA h;
			if (!h.Ouvrir("unkeny_captures_ia", 1600.f, 900.f)) {
				h.Fermer();
				return 1;
			}
			int32 erreurs = 0;
			NkEditeurIA &ia = h.IA();
			NkEditeurInterface &ui = h.T().Ui();
			ui.largeurIA = 460.f;
			ui.largeurDetails = 380.f;
			// La liste du fournisseur actif, lue par la premiere ouverture du panneau.
			for (int32 k = 0; k < 60 && (ia.sonde.EnCours() || ia.modeles[0].Empty()); ++k) {
				h.T().Trame();
				NkChrono::Sleep(static_cast<int64>(5));
			}
			ui.cadrageEnAttente = true;
			Trames(h, 4);

			// 01 + 04 : une conversation, puis « ajoute une caisse au centre ».
			{
				const char *bonjour[] = {"Bonjour ! Je vois votre scène : un sol, des caisses, un blob, de l'eau et un tissu. ",
										 "Je peux y poser des acteurs, régler leurs composants, écrire des scripts C++ ou des Blueprints, et tenir le GDD."};
				h.serveur.PousserTexte(bonjour, 2);
				h.Dire("Bonjour, que vois-tu ?");
				h.Attendre();
				h.serveur.PousserOutil("creer_acteur", "{\"acteur\":\"caisse\"}", "Je pose une caisse au centre de la vue.");
				const char *fin[] = {"C'est fait : une Caisse de 20 kg est au centre. Ctrl+Z la retire."};
				h.serveur.PousserTexte(fin, 1);
				h.Dire("ajoute une caisse au centre");
				h.Attendre();
				Trames(h, 3);
				erreurs += Png(h.T(), NkString::Format("%s/01_onglet_ia_a_cote_de_details_et_monde.png", dossier)) ? 0 : 1;
				// 02 : DETACHE en panneau a part (le bouton de la barre de l'onglet).
				NkEditeurInterface &u = h.T().Ui();
				h.T().Clic(0, u.iaBoutonPlace.x + u.iaBoutonPlace.w * 0.5f, u.iaBoutonPlace.y + u.iaBoutonPlace.h * 0.5f);
				u.ongletDroite = 0; // les Details de la caisse, a cote
				Trames(h, 3);
				erreurs += Png(h.T(), NkString::Format("%s/02_panneau_a_part_detache.png", dossier)) ? 0 : 1;
				// 03 : REPLIE (son chevron), puis deplie (un clic sur la bande).
				h.T().Clic(0, u.iaBoutonRepli.x + u.iaBoutonRepli.w * 0.5f, u.iaBoutonRepli.y + u.iaBoutonRepli.h * 0.5f);
				Trames(h, 3);
				erreurs += Png(h.T(), NkString::Format("%s/03_panneau_replie_chevron.png", dossier)) ? 0 : 1;
				h.T().Clic(0, u.ia.x + u.ia.w * 0.5f, u.ia.y + 200.f);
				Trames(h, 3);
				// 04 : la vue s'approche du centre pour qu'on VOIE la caisse posee.
				NkVue2D &cam = h.M().scene.Camera();
				const float32 zoom = cam.Zoom();
				cam.PoserZoom(zoom * 3.f);
				Trames(h, 2);
				erreurs += Png(h.T(), NkString::Format("%s/04_action_faite_caisse_au_centre.png", dossier)) ? 0 : 1;
			}
			// 05 : la meme, annulee par « Annuler cette action » (la meme porte que Ctrl+Z).
			{
				const uint32 effet = ia.blocEffet;
				h.CliquerAction(effet, 0u);
				Trames(h, 3);
				erreurs += Png(h.T(), NkString::Format("%s/05_action_annulee_ctrl_z.png", dossier)) ? 0 : 1;
				h.M().scene.Camera().PoserZoom(h.M().scene.Camera().Zoom() / 3.f);
			}
			// 06 : une suppression qui attend la confirmation.
			{
				NkFile::WriteAllText((h.projet + "Contenu/brouillon.txt").CStr(), "un brouillon de niveau");
				h.serveur.PousserOutil("supprimer_fichier", "{\"chemin\":\"Contenu/brouillon.txt\"}", "Je supprime le brouillon.");
				h.Dire("supprime Contenu/brouillon.txt");
				h.Attendre();
				Trames(h, 3);
				erreurs += Png(h.T(), NkString::Format("%s/06_suppression_attend_confirmation.png", dossier)) ? 0 : 1;
				h.CliquerAction(ia.blocConfirmation, 1u); // Refuser : le brouillon reste
				h.Attendre();
			}
			// 07 : les reglages, la liste des modeles lue sur le serveur.
			{
				NkEditeurIAOuvrirReglages(ia, 0);
				Trames(h, 2);
				h.T().Clic(0, ia.vue.btnActualiser.x + ia.vue.btnActualiser.w * 0.5f, ia.vue.btnActualiser.y + ia.vue.btnActualiser.h * 0.5f);
				for (int32 k = 0; k < 80 && ia.sonde.EnCours(); ++k) {
					h.T().Trame();
					NkChrono::Sleep(static_cast<int64>(5));
				}
				Trames(h, 2);
				ia.vue.OuvrirListeModeles();
				Trames(h, 2);
				erreurs += Png(h.T(), NkString::Format("%s/07_reglages_liste_des_modeles_du_serveur.png", dossier)) ? 0 : 1;
				ia.vue.liste = 0;
			}
			// 08 : « Tester la connexion », serveur absent.
			{
				ia.vue.PoserChamp(ia.vue.adresse, sizeof(ia.vue.adresse), "http://127.0.0.1:11999"); // personne n ecoute ici
				ia.vue.PoserChamp(ia.vue.nom, sizeof(ia.vue.nom), "Ollama (adresse fausse)");
				Trames(h, 1);
				h.T().Clic(0, ia.vue.btnTester.x + ia.vue.btnTester.w * 0.5f, ia.vue.btnTester.y + ia.vue.btnTester.h * 0.5f);
				for (int32 k = 0; k < 2000 && ia.sonde.EnCours(); ++k) {
					h.T().Trame();
					NkChrono::Sleep(static_cast<int64>(5));
				}
				Trames(h, 2);
				erreurs += Png(h.T(), NkString::Format("%s/08_tester_la_connexion_serveur_absent.png", dossier)) ? 0 : 1;
			}
			// 09 : Claude par l'API, une cle tapee : des points, jamais la cle.
			{
				NkEditeurIAOuvrirReglages(ia, 2);
				Trames(h, 2);
				h.T().Clic(0, ia.vue.champCle.x + 20.f, ia.vue.champCle.y + ia.vue.champCle.h * 0.5f);
				h.T().Taper("sk-ant-exemple-pas-une-vraie-cle");
				Trames(h, 2);
				erreurs += Png(h.T(), NkString::Format("%s/09_reglages_cle_masquee.png", dossier)) ? 0 : 1;
				ia.vue.cleSaisie = NkString(); // jamais enregistree (persister est faux, et on l'oublie)
				ia.reglagesOuverts = false;
			}
			h.Fermer();
			return erreurs == 0 ? 0 : 1;
		}

	} // namespace editeur
} // namespace nkentseu
