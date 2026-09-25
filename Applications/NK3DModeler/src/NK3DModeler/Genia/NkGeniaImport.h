#pragma once
// -----------------------------------------------------------------------------
// @File    NkGeniaImport.h
// @Brief   GENIA -- LE PONT : une image -> le generateur (derriere son
//          interface) -> un glTF -> L'IMPORT EXISTANT (NkImportFiles, donc
//          NkGLTFLoader, la decoupe par nom, la creation des noeuds, l'archive
//          et l'ecriture du `.nkmesh`). Le plus petit code possible : ce
//          fichier ne sait ni generer ni importer, il ENCHAINE.
//
//          La sortie est ecrite dans `<racine du projet>/Genia/<nom image>.glb`
//          : dans le projet, comme tout ce que l'import ecrit -- et hors du
//          depot du moteur, comme tout binaire.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NK3DModeler/Genia/NkGenerateur.h"
#include "NK3DModeler/Shell/NkModelerImport.h" // NkImportFiles, NkImportNote, NkImpStem
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"

#include <ctime>

namespace nkentseu {
	namespace nk3d {

		/// ── LE DOSSIER D'UN MODELE (25/09, Q18.4) ─────────────────────────────
		/// Rodolf : « un dossier par modele, nomme d'apres l'objet, dans le projet :
		/// le maillage, un sous-dossier vues/ avec les images 2D produites ou
		/// fournies, materiaux/, textures/, et une fiche. Rien dans un dossier
		/// temporaire : ces vues serviront a mesurer la fidelite, a rejouer et a
		/// entrainer. »
		///
		/// ⚠️ ET LE REPLI SUR LE DOSSIER TEMPORAIRE DISPARAIT, parce qu'il cachait un
		///    second defaut. Sans projet ouvert, la generation ecrivait dans TEMP un
		///    fichier parfaitement valide -- que l'import refusait ensuite (« Import
		///    impossible : aucun PROJET ouvert »). 72 secondes de carte graphique pour
		///    un fichier que personne ne pouvait ouvrir, et le fil ne disait « 0 carte »
		///    qu'a la fin. On refuse maintenant AVANT de generer, et on dit quoi faire.
		///
		/// Rend le dossier du modele, termine par '/', ou une chaine VIDE si aucun
		/// projet n'est ouvert. Cree `vues/`, `materiaux/` et `textures/`.
		inline NkString NkGeniaDossierModele(const NkModelerState &st, const char *nom) {
			if (st.projectRoot.Empty() || !nom || !nom[0])
				return NkString();
			NkString base = st.projectRoot;
			if (!base.EndsWith('/') && !base.EndsWith('\\'))
				base.Append('/');
			base.Append("Modeles/");
			base.Append(nom);
			base.Append('/');
			static const char *const kSous[] = {"vues", "materiaux", "textures"};
			for (const char *sd : kSous) {
				NkString d = base;
				d.Append(sd);
				NkDirectory::CreateRecursive(d.CStr());
			}
			return base;
		}

		/// LA FICHE DU MODELE : ce qu'on a demande, par quelle voie, avec quel
		/// modele, et quand. ⚠️ Elle est ecrite EN MEME TEMPS que le maillage, pas
		/// apres coup : une fiche qu'on remplirait plus tard est une fiche qui
		/// manquera pour les objets qui auront echoue -- c'est-a-dire exactement
		/// ceux qu'on voudra relire.
		inline void NkGeniaEcrireFiche(const NkString &dossier, const char *nom, const char *demande,
									   const char *voie, const char *modele, const char *source) {
			if (dossier.Empty())
				return;
			NkString f = dossier;
			f.Append("fiche.md");
			char t[64] = {0};
			{
				const std::time_t now = std::time(nullptr);
				std::tm tmv{};
#if defined(_WIN32)
				localtime_s(&tmv, &now);
#else
				localtime_r(&now, &tmv);
#endif
				std::strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S", &tmv);
			}
			char buf[1400];
			snprintf(buf, sizeof(buf),
					 "# %s\n\n"
					 "AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen\n\n"
					 "| | |\n|---|---|\n"
					 "| demande | %s |\n"
					 "| voie | %s |\n"
					 "| modele | %s |\n"
					 "| image d'entree | %s |\n"
					 "| date | %s |\n\n"
					 "Le maillage est a cote de cette fiche. `vues/` porte les images 2D -- celle\n"
					 "fournie par l'utilisateur comme celles rendues par l'application ; elles\n"
					 "servent a mesurer la fidelite, a rejouer et a entrainer.\n",
					 nom ? nom : "modele", demande && demande[0] ? demande : "(aucune)",
					 voie && voie[0] ? voie : "(inconnue)", modele && modele[0] ? modele : "(inconnu)",
					 source && source[0] ? source : "(aucune)", t);
			(void)NkFile::WriteAllText(f.CStr(), buf);
		}

		/// Le chemin du glTF que la generation va ecrire, pour `imagePath` : il vit
		/// DANS le dossier du modele. Chaine vide = aucun projet ouvert, et
		/// l'appelant doit refuser AVANT de lancer quoi que ce soit.
		inline NkString NkGeniaSortiePour(const NkModelerState &st, const char *imagePath) {
			char stem[32];
			NkImpStem(imagePath, stem, (uint32)sizeof(stem));
			NkString d = NkGeniaDossierModele(st, stem);
			if (d.Empty())
				return NkString();
			d.Append(stem);
			d.Append(".glb");
			return d;
		}

		/// LE GESTE COMPLET : genere puis importe. Tout refus est NOMME a
		/// l'ecran (NkImportNote) et au journal. Rend vrai si l'import a produit
		/// au moins un model.
		inline bool NkGeniaImporterImage(NkModelerState &st, const char *imagePath,
										 NkIGenerateur &gen = NkGeniaGenerateurParDefaut(),
										 NkVector<int32> *cardsOut = nullptr) {
			if (!imagePath || !imagePath[0]) {
				NkImportNote(st, NkToastKind::Refus, "Generation impossible : aucune image");
				return false;
			}
			const NkString out = NkGeniaSortiePour(st, imagePath);
			NkString why;
			NkLog::Instance().Infof("[genia] generation : '%s' -> '%s' par %s", imagePath, out.CStr(), gen.Nom());
			if (!gen.Generer(imagePath, out.CStr(), why)) {
				NkImportNote(st, NkToastKind::Refus, "Generation impossible depuis '%s' : %s", imagePath,
							 why.Empty() ? "raison inconnue" : why.CStr());
				return false;
			}
			NkLog::Instance().Infof("[genia] MESURE generation : '%s' ecrit par %s", out.CStr(), gen.Nom());
			// PAR LA PORTE DE LISTE, comme le bouton Importer : un seul appelant a
			// changer le jour ou l'import prend plusieurs images.
			const char *un[1] = {out.CStr()};
			const int32 ok = NkImportFiles(st, un, 1, cardsOut);
			if (ok > 0)
				NkImportNote(st, NkToastKind::Succes, "Genere et importe : '%s'", out.CStr());
			return ok > 0;
		}

	} // namespace nk3d
} // namespace nkentseu
