#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkEditorTiroirMode.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   CE QU'UN TIROIR FAIT A CE QU'IL Y A DESSOUS : deux modes declares,
//          et deux fonctions pures qui en decoulent.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA DEMANDE (Rodolf, 20/09/2026)
//   Le tiroir de NKUIDesign ASSOMBRIT toute la toile pendant qu'il est ouvert.
//   Il ne voit plus son document pendant qu'il demande de le modifier.
//
// ⚠️ LE VOILE EST JUSTE POUR UNE MODALE, et pour elle seule : il dit « reponds
//    a ceci avant de continuer ». Un panneau de conversation n'est pas une
//    modale -- ON Y TAPE EN REGARDANT CE QU'ON MODIFIE. *Un panneau qui
//    assombrit sa propre cible se comporte comme ce qu'il n'est pas.*
//
//    Le voile venait de VSCode, repris avec son apparence et sans sa condition
//    (`NkEditorShell.h` le dit en propre : « ce qui a ete repris [...] et le
//    voile de `theme.scrim` »). On reprend une apparence, on herite d'un
//    comportement.
//
// ⚠️ LE VOILE N'ETAIT PAS SEUL, ET C'EST LE POINT QU'ON AURAIT PU MANQUER.
//    Le tiroir RECLAMAIT AUSSI tout le corps a l'entree. Le voile se voit ; la
//    reclamation, non -- et c'est elle qui empeche vraiment de travailler
//    dessous. Corriger le voile seul aurait rendu un panneau qui A L'AIR
//    utilisable et ne l'est pas : le pire des deux etats, parce qu'on cesse
//    alors de chercher.
//
// POURQUOI CE FICHIER EXISTE, PLUTOT QUE TROIS LIGNES DANS LE PEINTRE
//   Tant que la decision vivait dans `DrawRailDrawers`, elle n'etait
//   atteignable qu'avec une fenetre -- donc jamais mesuree. C'est pour ca qu'un
//   voile sans aucun usage modal a survecu si longtemps a cote d'un panneau
//   qu'il empechait d'utiliser. Sortie ici, elle est deux fonctions pures que
//   le banc console eprouve.
//
// ⚠️ ZERO DEPENDANCE, ET CE N'EST PAS DU CONFORT. `NkEditorShell.h` tire
//    `windows.h` par transitivite, et `windows.h` DEFINIT `pascal` (convention
//    d'appel Win16). Inclure la coquille depuis un banc a casse une variable
//    nommee `pascal` dans un test sans aucun rapport. Un en-tete qui porte une
//    DECISION doit pouvoir etre inclus partout ; celui qui porte une COQUILLE,
//    non.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// ⚠️ LE DEFAUT EST `Travail`, ET LA MESURE LE JUSTIFIE. Un seul fichier de
		//    la maison pose des rails (`NKUIDesign/main.cpp`, `SetRail` x3) et ses
		//    cinq tiroirs -- Bibliotheque, IA, Console, Apercu, et le troisieme du
		//    rail droit -- sont TOUS des panneaux de travail. Aucun n'est modal :
		//    les vraies modales passent par `NkEditorModal.h`, qui n'emprunte pas
		//    ce chemin. Le voile ne servait donc nulle part ce qu'il pretendait
		//    dire -- ce n'est pas « on change d'avis », c'est « il n'a jamais eu
		//    d'usage ».
		enum class NkEditorTiroirMode : uint8 {
			/// On travaille DESSOUS pendant qu'il est ouvert : aucun voile, et le
			/// tiroir ne reclame que son propre rectangle.
			Travail = 0,
			/// « Reponds a ceci avant de continuer » : voile sur le corps, et le
			/// corps entier attend.
			/// ⚠️ AUCUN CONSOMMATEUR AUJOURD'HUI. Il existe parce que le tiroir du
			///    kit PEUT servir aux deux, et qu'un mode non declare redeviendrait
			///    un defaut silencieux -- exactement l'etat d'avant.
			///    CONDITION DE RETRAIT : si dans six mois personne ne l'a pose, il
			///    part. *Un mode sans usage est un inventaire.*
			Modal,
		};

		/// Le tiroir peint-il un voile sur le corps ?
		inline bool NkEditorTiroirVoile(NkEditorTiroirMode m) {
			return m == NkEditorTiroirMode::Modal;
		}

		/// Le tiroir reclame-t-il TOUT le corps a l'entree ?
		/// `drag` : pendant un glisser, meme une modale ne reclame que soi-meme.
		/// ⚠️ C'est la regle R20, deja payee le 06/09 : le tiroir reclamait le corps
		///    glisser compris, la toile dessous recevait une souris hors ecran, et
		///    la zone de depot n'etait jamais atteinte -- mesure : souris MASQUEE,
		///    cible jamais ouverte, 0 composant pose. Elle est conservee telle
		///    quelle ; ce lot ne la rouvre pas.
		inline bool NkEditorTiroirReclameLeCorps(NkEditorTiroirMode m, bool drag) {
			return m == NkEditorTiroirMode::Modal && !drag;
		}

	} // namespace editorkit
} // namespace nkentseu
