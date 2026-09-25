// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @File    Kernel/System/NKConverse/src/NKConverse/NkConverse.cpp
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// CE FICHIER NE CONTIENT AUCUNE LOGIQUE, ET CE N'EST PAS UN OUBLI.
//
// NKConverse est entierement en en-tetes. Ce .cpp existe pour UNE raison, et
// elle est mesurable : il inclut les trois en-têtes du module ET RIEN D'AUTRE.
// S'il compile, alors chaque en-tete tire lui-meme ce qu'il utilise -- c'est la
// REGLE 2 du module, verifiee par la construction plutot que par la relecture.
//
// ⚠️ C'EST EXACTEMENT LA FAUTE QUI A COUTE 20 ERREURS A NKPA la semaine
//    derniere : un en-tete utilisait un chronometre sans inclure NkChrono. Il
//    compilait chez ses deux consommateurs d'alors, parce que TOUS DEUX
//    incluaient NKTime avant lui -- par chance, et la chance a une date de
//    peremption. Le premier consommateur qui ne l'avait pas a recolte les 20
//    erreurs.
//
// Un module dont aucun .cpp n'inclut les en-tetes ne prouve jamais rien : ses
// en-tetes ne sont compiles que dans le contexte de ses appelants, donc avec
// tout ce que ces appelants ont deja inclus. Ici, le contexte est VIDE.

#include "NKConverse/NkConverseChatAsync.h" // tire NkConverseChat.h, qui tire NkConverse.h
// ⚠️ LE DORSAL CLAUDE PASSE PAR LA MEME EPREUVE, et c'est tout l'interet de ce
//    fichier. Il utilise NkDirectory et NkPath : il les INCLUT donc lui-meme.
//    S'il avait compte sur un consommateur pour les tirer, il aurait compile
//    dans NK3DModeler -- qui inclut NKFileSystem partout -- et casse chez le
//    premier qui ne l'a pas. Ici le contexte est VIDE : la regle 2 est verifiee
//    par la construction, pas par la relecture.
#include "NKConverse/NkConverseClaude.h"

namespace nkentseu::converse {

	// Ancre de traduction : donne au module une unite de compilation propre, et
	// evite une bibliotheque statique sans aucun symbole (certains editeurs de
	// liens s'en plaignent, et un avertissement qu'on apprend a ignorer finit
	// par en cacher un vrai).
	const char *NkConverseVersion() {
		return "NKConverse 1.0 (extrait de NKUIDesign/DesignAI.h le 2026-09-17)";
	}

} // namespace nkentseu::converse
