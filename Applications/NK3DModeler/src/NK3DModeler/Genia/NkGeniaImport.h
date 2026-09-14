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

namespace nkentseu {
	namespace nk3d {

		/// Le chemin du glTF que la generation va ecrire, pour `imagePath`.
		/// SANS PROJET OUVERT (`projectRoot` vide), le chemin relatif « Genia/ »
		/// tomberait dans le dossier courant -- depuis la racine du worktree,
		/// DANS l'arbre du depot, et `Genia/` n'y est pas ignore (verifie le
		/// 2026-09-12 par git check-ignore). Un binaire genere n'entre jamais
		/// dans un depot : on se replie sur le dossier temporaire du systeme.
		inline NkString NkGeniaSortiePour(const NkModelerState &st, const char *imagePath) {
			char stem[32];
			NkImpStem(imagePath, stem, (uint32)sizeof(stem));
			NkString out = st.projectRoot.Empty() ? NkPath::GetTempDirectory().ToString() : st.projectRoot;
			if (!out.Empty() && !out.EndsWith('/') && !out.EndsWith('\\'))
				out.Append('/');
			out.Append("Genia/");
			out.Append(stem);
			out.Append(".glb");
			return out;
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
