#pragma once
// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkModelerData.h — OU SONT LES DONNEES LIVREES DE NK3DMODELER, ecrit UNE FOIS.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ── POURQUOI CE FICHIER EXISTE ───────────────────────────────────────────────
// Le 20/09, en une soiree, DEUX chargeurs de donnees se sont reveles morts
// depuis toujours, et pour la MEME raison :
//
//   - le catalogue de BROSSES cherchait « data/brushes » et
//     « <exe>/data/brushes » -- aucun des deux n'existe. Consequence chez
//     Rodolf : « le mode sculpt ne fonctionne pas », avec dans son journal
//     « brosses chargees depuis le disque : 0 » puis « sculpture REFUSEE :
//     brosse inconnue ». Le geste etait bon, le rayon touchait, le trait
//     partait -- il n'y avait simplement aucune brosse.
//   - les THEMES cherchaient « data/themes » : le theme `bleu_nuit` de Rodolf
//     n'avait JAMAIS ete charge, et le produit l'annoncait a chaque lancement
//     (« 2 themes (0 depuis le disque) ») dans une ligne que personne ne lisait.
//
// La convention JUSTE, elle, etait deja ecrite -- trois fois, a trois endroits,
// et deux des cinq consommateurs en avaient recopie la mauvaise moitie. Le
// defaut n'etait donc pas dans un chargeur : il etait dans le fait que la
// convention se RECOPIE. Un sixieme consommateur en aurait recopie une moitie
// de plus.
//
// ⚠️ ON N'AJOUTE PAS UNE SIXIEME COPIE : on ecrit les trois racines ICI, et
//    tout le monde passe par la. C'est la seule forme de correctif qui protege
//    le PROCHAIN chargeur, celui qui n'est pas encore ecrit.
//
// ── LES TROIS RACINES, ET POURQUOI IL EN FAUT TROIS ──────────────────────────
// Pour un chemin relatif `r` (ex. « data/brushes ») :
//
//   1. « r »                                  -- le repertoire courant.
//   2. « Applications/NK3DModeler/r »         -- LA RACINE DE L'ARBRE, c'est-a-
//      dire la facon dont l'application SE LANCE en developpement (`Resources/`
//      y est relatif en six endroits ; lancee depuis le dossier de l'exe, elle
//      charge zero icone et s'arrete sur « manque le source HLSL »). C'est la
//      SEULE racine ou les donnees de ce depot existent reellement, et c'est
//      exactement celle que les deux chargeurs morts avaient oubliee.
//   3. « <dossier de l'executable>/r »        -- une livraison ou les donnees
//      sont posees a cote du binaire. Elle n'existe pas encore ; elle existera.
//
// ⚠️ LA RACINE DE L'ARBRE EST DERIVEE DU CHEMIN RECU, jamais recopiee a cote de
//    lui. Ecrire « Applications/NK3DModeler/data/brushes » en dur A COTE de
//    « data/brushes » fabrique deux verites pour un dossier : changer l'une
//    laisse l'autre derriere, et c'est le genre d'ecart qui ne se voit qu'au
//    moment ou il fait perdre une soiree.
//
// ── ET UN REFUS EST NOMME ────────────────────────────────────────────────────
// « Un chargeur qui echoue en silence est pire qu'un qui echoue en le disant »
// (Rodolf, 20/09). Les deux chargeurs morts ont survecu des mois PARCE QUE leur
// compte se lisait : « 0 depuis le disque » est un refus nomme, il n'attendait
// qu'un lecteur. Ceux qui ne disent rien -- l'image de version, par exemple --
// n'ont meme pas cette chance. `NkDataRefus` ecrit donc CE QU'ON CHERCHAIT et
// LES TROIS ENDROITS OU L'ON A REGARDE : un chemin mort se lit alors dans le
// journal, sans relire le code.
// =============================================================================

#include "NKContainers/String/NkString.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nk3d {

		/// Le prefixe de la racine de l'arbre. UN SEUL point d'ecriture : si
		/// l'application demenage, cette ligne bouge et les cinq consommateurs
		/// suivent. C'est tout l'objet de ce fichier.
		static const char *const kNkDataTreePrefix = "Applications/NK3DModeler/";

		/// Les trois candidats pour un chemin relatif, dans l'ordre de priorite.
		/// Rend le nombre reellement rempli (3, ou 2 si le dossier de l'executable
		/// est inconnu). `relatif` peut porter ou non une barre finale : on ne la
		/// touche pas, puisque l'appelant la reutilise pour composer ses fichiers.
		inline uint32 NkDataRoots(const char *relatif, NkString out[3]) {
			if (!relatif || !*relatif)
				return 0u;
			uint32 n = 0u;
			out[n++] = NkString(relatif);
			// DERIVE, pas recopie (cf. l'en-tete).
			NkString arbre = NkString(kNkDataTreePrefix);
			arbre.Append(relatif);
			out[n++] = arbre;
			const NkString exeDir = NkPath::GetExecutableDirectory().ToString();
			if (!exeDir.Empty()) {
				NkString pres = exeDir;
				pres.Append("/");
				pres.Append(relatif);
				out[n++] = pres;
			}
			return n;
		}

		/// Dit CE QU'ON CHERCHAIT et OU l'on a regarde. A appeler quand aucune des
		/// trois racines n'a repondu -- jamais en cas de succes, sinon le journal
		/// se remplit de lignes qui n'apprennent rien et plus personne ne le lit.
		/// 🔴 `attendu` (26/09) : CETTE ABSENCE EST-ELLE NORMALE ?
		///
		/// LE DEFAUT QUI L'A FAIT NAITRE. Depuis le 25/09, le journal arrive A
		/// L'ECRAN (puits d'ecran de NKEditorKit, niveau AVERTISSEMENT). Rodolf a
		/// donc vu, sur son ecran d'accueil, un bandeau ambre :
		///   « [nk3d-data] image de version (ecran d'accueil) INTROUVABLE ... »
		/// Or cette absence est NORMALE ET DOCUMENTEE : le `LISEZMOI.md` du
		/// dossier, versionne, ecrit « Sans `splash.png`, la bande ne s'affiche pas
		/// du tout ». Le fichier n'est pas un actif oublie par git -- verifie : il
		/// n'existe dans AUCUNE branche et dans AUCUN arbre -- c'est un contenu
		/// EDITORIAL que Rodolf depose quand il le veut.
		///
		/// ⚠️ LE CABLAGE N'A PAS CREE CE MESSAGE, IL L'A REVELE. Il criait depuis
		///    toujours dans une console que personne ne lit. C'est le bon
		///    comportement du puits -- et c'est aussi ce qui oblige a trier : *ce
		///    qui monte a l'ecran doit meriter d'interrompre*. Une absence prevue
		///    n'interrompt pas.
		///
		/// `attendu = true` -> INFO : la ligne reste dans le journal et dans
		/// `logs/app.log`, ou elle sert toujours a diagnostiquer un chemin mort,
		/// mais elle ne monte plus a l'ecran (le puits filtre a AVERTISSEMENT).
		/// `attendu = false` (le defaut) -> AVERTISSEMENT, comme avant.
		inline void NkDataRefus(const char *quoi, const char *relatif, bool attendu = false) {
			NkString c[3];
			const uint32 n = NkDataRoots(relatif, c);
			NkString ou;
			for (uint32 i = 0; i < n; ++i) {
				if (i)
					ou.Append(" | ");
				ou.Append(c[i].CStr());
			}
			if (attendu) {
				NkLog::Instance().Infof(
					"[nk3d-data] %s absent (prevu) : « %s » cherche dans %u racine(s) -> %s\n",
					quoi, relatif, (unsigned)n, ou.CStr());
				return;
			}
			NkLog::Instance().Warn(
				"[nk3d-data] {0} INTROUVABLE : « {1} » cherche dans {2} racine(s) -> {3}\n", quoi,
				relatif, n, ou.CStr());
		}

		/// Le premier DOSSIER qui existe parmi les trois racines, ou une chaine
		/// vide. L'appelant teste `Empty()` et nomme son refus -- on ne journalise
		/// pas ici, parce que l'appelant sait dire CE QUE le dossier contenait.
		inline NkString NkDataDir(const char *relatif) {
			NkString c[3];
			const uint32 n = NkDataRoots(relatif, c);
			for (uint32 i = 0; i < n; ++i)
				if (NkDirectory::Exists(c[i].CStr()))
					return c[i];
			return NkString();
		}

		/// Le premier FICHIER qui existe parmi les trois racines, ou une chaine
		/// vide. Pour une donnee unique (l'image de version, un manifeste) ou
		/// resoudre le dossier ne suffit pas : un dossier peut exister et etre
		/// vide, et c'est le cas de `data/splash/` aujourd'hui.
		inline NkString NkDataFile(const char *relatif) {
			NkString c[3];
			const uint32 n = NkDataRoots(relatif, c);
			for (uint32 i = 0; i < n; ++i)
				if (NkFile::Exists(c[i].CStr()))
					return c[i];
			return NkString();
		}

	} // namespace nk3d
} // namespace nkentseu
