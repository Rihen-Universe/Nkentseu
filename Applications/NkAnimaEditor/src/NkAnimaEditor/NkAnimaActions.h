// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkAnimaActions.h — CE QUE FONT LES COMMANDES NOMMÉES DE NkAnimaEditor.
// =============================================================================
// UN FICHIER, UNE RESPONSABILITÉ (règle posée par Rodolf le 26/09).
//
//   Ce fichier-ci répond à UNE seule question : « quand l'utilisateur déclenche
//   l'action nommée `anim.xxx`, quel appel part vers `AnimBridge` ? »
//
//   Il ne dit pas OÙ le bouton se trouve — c'est le document `.nkgui`.
//   Il ne dit pas COMMENT une zone se peint — c'est `NkAnimaZones`.
//   Il ne construit ni fenêtre ni coquille — c'est `main.cpp`.
//
// ⚠️ LA TABLE EST FERMÉE, ET UN NOM ABSENT NE SE TAIT PAS. Un bouton dont le
//    document nomme une action inconnue apparaît, se clique, et le compteur
//    `actionsInconnues` monte. C'est la même règle que pour les zones hôtes :
//    *ce qui n'est pas servi doit se voir, pas disparaître.*
//
// ⚠️ ON BRANCHE CE QUI EXISTE, ON N'ÉCRIT PAS DE MOTEUR. Chaque action appelle
//    une fonction que `AnimBridge.h` expose DÉJÀ. Ce qui n'existe pas —
//    l'éditeur de courbes, les calques, le reciblage, les VFX, l'IK — n'est pas
//    simulé ici : ces entrées sont GRISÉES dans les documents, avec leur raison
//    écrite à côté. *Une entrée qui s'affiche et ne fait rien est un mensonge ;
//    une entrée grisée est une promesse datée.*
// =============================================================================
#pragma once

#include "NKGui/Doc/NkGuiCoquille.h"

namespace nkanima {

	// ⚠️ `nkanima` N'EST PAS DANS `nkentseu`. Sans ces deux lignes, `nkgui` et
	//    `uint32` ne sont pas visibles ici — le compilateur l'a dit sans
	//    ambiguïté. On reprend la MÊME importation que `NkCoquilleDocument.h`,
	//    au lieu d'en inventer une seconde : deux conventions dans un même
	//    module finissent par diverger.
	using namespace nkentseu;
	using namespace nkentseu::nkgui;

	/// La table des actions nommées, et son nombre. Les deux vont ensemble :
	/// un compte recopié ailleurs se périme — ce dépôt a déjà vu « Quitter »
	/// disparaître d'un menu parce qu'un nombre écrit à la main n'avait pas
	/// suivi l'ajout d'une entrée.
	const nkgui::NkActionNommee *ActionsAnimation(uint32 &outNombre) noexcept;

	/// ⚠️ LA MÊME PORTE POUR LA PALETTE ET POUR LE DOCUMENT.
	///
	/// La palette de commandes (Ctrl+P) et les boutons du document doivent
	/// appeler LA MÊME fonction, jamais deux jumelles. Ce dépôt a déjà payé
	/// *deux compteurs sans code commun* : deux chemins vers deux fonctions qui
	/// se ressemblaient ont divergé au premier cas particulier, et plus
	/// personne ne savait laquelle était en panne.
	///
	/// `RegisterCommand` prend donc sa fonction ICI, par le nom de l'action.
	///
	/// @return nullptr si le nom n'est pas dans la table. L'appelant DOIT le
	///         traiter : enregistrer une commande nulle donnerait une entrée de
	///         palette qui se clique et ne fait rien — le défaut que toute
	///         cette mécanique sert à rendre impossible.
	nkgui::NkActionFn FonctionDe(const char *nom) noexcept;

	/// Vrai si la lecture en boucle est demandée, posée par les actions
	/// `anim.boucle_activee` / `anim.boucle_coupee`.
	///
	/// ⚠️ RIEN NE L'INTERROGE AUJOURD'HUI, et c'est dit plutôt que laissé
	///    croire : le rebouclage est obtenu autrement, par le `behavior` du
	///    document rejoué à chaque image, qui ramène le curseur à zéro quand la
	///    lecture atteint la fin. Cet accesseur existe pour que l'état soit
	///    LISIBLE le jour où la barre d'état ou un raccourci en aura besoin.
	bool LectureEnBoucle() noexcept;

} // namespace nkanima
