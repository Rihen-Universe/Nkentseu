// =============================================================================
// NkEditeurApp.h — l'assemblage de l'editeur d'Unkeny
//
// ⚠️ REECRIT LE 2026-09-01 : DE `NkCanvasApp` VERS `NkEditorShell`
//   La premiere version batissait l'editeur sur `NkCanvasApp` -- la coquille
//   d'APPLICATION de NKCanvas -- et redessinait a la main une barre d'outils,
//   des colonnes, un inspecteur et une barre d'etat. NKEditorKit porte tout
//   cela, et la regle du depot est explicite : on cherche dans le kit AVANT
//   d'ecrire un element d'interface. Je ne l'avais pas fait.
//
// ⚠️ POURQUOI IL FALLAIT CHOISIR, ET NON COMBINER
//   `NkCanvasApp` et `NkEditorShell` possedent TOUTES DEUX la fenetre et la
//   boucle. Deux coquilles ne coexistent pas dans une fenetre -- c'est la meme
//   exclusivite que NKCanvas/NKRenderer. Mesure du 2026-09-01, sur sept axes :
//
//     NkEditorShell : ancrage, palette de commandes, barres d'activite ;
//                     ZERO cycle de vie mobile, zone sure, pointeur tactile
//     NkCanvasApp   : exactement l'inverse
//
//   Pour un EDITEUR DE BUREAU, c'est le shell qui gagne : l'ancrage et la
//   palette servent tous les jours, le cycle de vie mobile ne sert jamais. Ce
//   qu'on garde de l'autre monde -- la simulation de ZONE SURE -- est un DESSIN
//   dans le viseur, pas un service de la coquille : il traverse intact.
//
// CE QUE L'EDITEUR EST, ET POURQUOI IL EXISTE
//   Le premier consommateur d'Unkeny. Un moteur sans consommateur ne se prouve
//   pas : chaque panneau ici EXERCE quelque chose du moteur -- la scene, l'ECS,
//   la physique, le rendu, le hors-champ.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - un panneau        -> NkEditeurPanneaux.h, puis `AddPanel` dans Init()
//   - une commande      -> `RegisterCommand` dans Init() (palette Ctrl+Maj+P)
//   - un etat partage   -> NkEditeurModele.h
// =============================================================================
#pragma once

#include "Editeur/NkEditeurModele.h"
#include "Editeur/NkEditeurPanneaux.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkOptional.h"
#include "NKEditorKit/NkEditorShell.h"
#include "NKMemory/NKMemory.h"

namespace nkentseu {
	namespace editeur {

		class NkEditeurApp {
			public:
				NkEditeurApp() noexcept;

				/// Lit les arguments de la ligne de commande.
				///
				/// Rend un code de sortie quand l'application ne doit PAS
				/// demarrer (`--selftest`), et rien sinon.
				///
				/// ⚠️ `--profil=` et `--paysage` existent pour qu'une capture
				/// d'ecran soit REPRODUCTIBLE, donc comparable d'une version a
				/// l'autre : un reglage qu'on ne peut atteindre qu'a la souris ne
				/// se verifie jamais en automatique.
				NkOptional<int> LireArguments(const NkVector<NkString> &args);

				/// Cree la fenetre, la scene et les panneaux.
				bool Init();

				/// La boucle du shell. Bloquante ; rend le code de sortie.
				int Run();

			private:
				void ConstruireSceneExemple();

				// ⚠️ L ORDRE DE DECLARATION EST L ORDRE DE CONSTRUCTION, ET SON
				// INVERSE EST L ORDRE DE DESTRUCTION. Il est donc porteur de
				// sens ici, et pas seulement de style :
				//
				//   construction : modele -> panneaux -> shell
				//   destruction  : shell  -> panneaux -> modele
				//
				// Les panneaux tiennent une REFERENCE au modele, et le shell
				// tient des POINTEURS vers les panneaux. Chacun meurt donc avant
				// ce dont il depend. Declarer le shell en premier inverserait la
				// destruction et le laisserait pointer vers des panneaux morts --
				// erreur que j ai faite en ecrivant ce fichier, et que le
				// compilateur n aurait jamais signalee.
				NkEditeurModele mModele;

				NkPanneauViseur mViseur;
				NkPanneauHierarchie mHierarchie;
				NkPanneauInspecteur mInspecteur;
				NkPanneauOutils mOutils;

				memory::NkUniquePtr<editorkit::NkEditorShell> mShell;
		};

	} // namespace editeur
} // namespace nkentseu
