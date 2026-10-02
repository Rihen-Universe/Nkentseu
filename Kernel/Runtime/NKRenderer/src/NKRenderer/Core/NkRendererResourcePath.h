#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRendererResourcePath.h — OU TROUVER `Resources/NKRenderer`, quel que soit
// le dossier depuis lequel l'application a ete lancee.
//
// ⚠️ LA PANNE QUI L'A FAIT NAITRE (2026-10-01). NKCraft lance depuis son
//    propre dossier (`Build/Bin/Debug-Windows/NKCraft`) affichait :
//      « [NkShaderLibrary] 'Skybox' INTROUVABLE -- FICHIER ABSENT ...
//        cherche (1) Resources/NKRenderer/Shaders/... [relatif au repertoire
//        courant] »
//    pour ShadowInstanced, Skin, Instanced, Skybox, InfiniteGrid, ShadowAlpha.
//    Ces six shaders n'ont pas de source embarquee : sans le fichier, rien ne
//    les remplace. La bibliotheque cherchait (1) dans le REPERTOIRE COURANT et
//    (2) dans le DOSSIER DE L'EXECUTABLE ; or Jenga ne copie pas `Resources/` a
//    cote du binaire au `build` (`dependfiles` n'est servi qu'au `package`). Le
//    seul dossier ou les fichiers existent, c'est la racine du depot -- quatre
//    niveaux AU-DESSUS de l'executable. Lance d'ailleurs que de la racine,
//    aucune des deux racines ne repondait.
//
// LE REMEDE : une TROISIEME racine, trouvee en REMONTANT depuis l'executable
// jusqu'au premier dossier qui contient `Resources/NKRenderer` -- la meme
// demarche que `NkTrouverDepot` d'UnkenyEditor pour le depot. Elle est
// cherchee UNE fois (statique locale) puis gardee en chemin ABSOLU : un
// selecteur de fichiers qui change le repertoire courant en cours de route ne
// la deplace pas.
//
// ⚠️ ADDITIF, JAMAIS SUBSTITUTIF : le chemin RELATIF est toujours essaye EN
//    PREMIER. Une application lancee depuis la racine du depot, un jeu livre
//    avec `Resources/` a cote de lui, ou Android (repli AAssetManager de
//    `NkFile`, qui ne connait que les chemins relatifs) voient exactement les
//    memes fichiers qu'avant.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace renderer {

		/// Le dossier ABSOLU (barre finale comprise) qui contient
		/// `Resources/NKRenderer`, trouve en remontant depuis le dossier de
		/// l'executable (lui compris, douze niveaux au plus). Vide si aucun.
		/// Cherche une seule fois par processus.
		const NkString &NkRendererResourceRoot() noexcept;

		/// Resout un chemin RELATIF de ressource (« Resources/NKRenderer/... ») :
		///   1. tel quel (repertoire courant, comportement historique) ;
		///   2. sous `NkRendererResourceRoot()`.
		/// Rend le premier qui existe (fichier OU dossier). Si aucun n'existe, rend
		/// le chemin d'origine INCHANGE : le message d'erreur de l'appelant reste
		/// celui qu'il etait, et il dit ce qu'on cherchait.
		NkString NkRendererResolvePath(const NkString &relatif) noexcept;

		/// Une ligne pour les diagnostics : ou la recherche a regarde.
		/// Ex. « racine trouvee en remontant depuis l'executable : C:/.../Nkentseu/ »
		/// ou « aucune racine en remontant depuis C:/.../Bin/Debug-Windows/NKCraft ».
		NkString NkRendererResourceSearchReport() noexcept;

	} // namespace renderer
} // namespace nkentseu
