// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkAnimaZones.h — CE QUE PEIGNENT LES ZONES HÔTES DE NkAnimaEditor.
// =============================================================================
// UN FICHIER, UNE RESPONSABILITÉ (règle posée par Rodolf le 26/09).
//
//   Ce fichier-ci répond à UNE seule question : « quand le document réserve une
//   zone nommée `Host "xxx"`, qu'est-ce qui se dessine dedans ? »
//
//   Le document dit OÙ et QUELLE TAILLE ; l'application garde QUAND et COMMENT.
//   C'est le partage qui permet de déplacer une bande des clés dans l'interface
//   sans recompiler, tout en gardant son dessin en C++.
//
// ⚠️ UNE ZONE QUE PERSONNE NE SERT SE COUVRE DE HACHURES avec son nom écrit
//    dedans. C'est voulu : une zone non servie ne doit pas se faire prendre
//    pour un fond. Rendre `false` veut dire « je ne sais pas peindre ceci » ;
//    rendre `true` sans peindre ferait disparaître la zone en silence.
//
// ⚠️ ET « PAS DE DONNÉES » N'EST PAS « PAS DE SERVICE ». Une zone qui ÉCRIT
//    « aucun clip chargé » rend `true` : elle est servie, et elle dit qu'il n'y
//    a rien. Les deux cas ne doivent pas se ressembler à l'écran, sinon un
//    défaut de câblage passe pour un fichier vide.
// =============================================================================
#pragma once

#include "NKGui/Doc/NkGuiCoquille.h"

namespace nkanima {

	// ⚠️ `nkanima` N'EST PAS DANS `nkentseu` : sans ces deux lignes, `nkgui` et
	//    `uint32` ne sont pas visibles. Même importation que
	//    `NkCoquilleDocument.h`, pas une seconde convention.
	using namespace nkentseu;
	using namespace nkentseu::nkgui;

	/// La table des zones nommées, et son nombre. Les deux vont ensemble : un
	/// compte recopié ailleurs se périme.
	const nkgui::NkZoneNommee *ZonesAnimation(uint32 &outNombre) noexcept;

} // namespace nkanima
