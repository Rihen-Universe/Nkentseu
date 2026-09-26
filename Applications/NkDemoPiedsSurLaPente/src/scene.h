// =============================================================================
// @File    Applications/NkDemoPiedsSurLaPente/src/scene.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   La frontière entre la SCÈNE (Noge, physique, IK) et le DESSIN
//          (NKCanvas). Elle ne parle qu'en `float` et en `bool`.
//
// ⚠️ POURQUOI CETTE FRONTIÈRE EXISTE, ET CE N'EST PAS UN GOÛT D'ARCHITECTURE :
//    NKCanvas et NKRenderer **ne peuvent pas cohabiter dans une même unité de
//    compilation**. `NkBlendMode`, `NkVertex2D`, `NkGraphicsApiName`,
//    `NkGpuVendor`... sont définis DEUX FOIS, dans
//      Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Core/NkRenderer2DTypes.h
//      Kernel/Runtime/NKRenderer/src/NKRenderer/Core/NkRendererTypes.h
//    et le compilateur rend une quinzaine de « redefinition of ... ».
//
//    Or `Noge/Anim/NkLocomotion.h` tire NKRenderer (par `NkIKSolver`), et la
//    démo dessine avec NKCanvas. Les deux mondes sont donc SÉPARÉS :
//      scene.cpp  -> Noge + NKPhysics + NKAnima, jamais NKCanvas
//      main.cpp   -> NKCanvas + NKWindow,        jamais Noge
//    et ce fichier est le seul point de contact. *Les deux chemins graphiques
//    du dépôt se rencontrent ici, et il faut les tenir à distance.*
//
// @License Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================
#pragma once

namespace eprouvette {

	// Indices des os, alignés sur les valeurs par défaut de `NkFootIK`.
	enum {
		kHip = 0,
		kLThigh,
		kLCalf,
		kLFoot,
		kLToe,
		kRThigh,
		kRCalf,
		kRFoot,
		kRToe,
		kOs
	};

	// Tout ce que le dessin a besoin de savoir. Que des nombres : aucun type de
	// Noge, de NKPhysics ou de NKCanvas ne traverse cette frontière.
	struct Etat {
			float osX[kOs] = {};
			float osY[kOs] = {};

			// ⚠️ La pente LUE par deux raycasts, jamais la consigne. Si elle
			//    vaut 0 alors que la consigne ne l'est pas, le sol est plat et
			//    toute la mesure porte sur autre chose (« la pente n'existait
			//    pas », 13/09).
			float penteLueDeg = 0.f;
			bool penteLisible = false;

			float solSousPiedG = 0.f; // hauteur du sol juste sous le pied gauche
			bool solLisible = false;

			// Deux points du sol, pour tracer la pente telle qu'elle EST.
			float solGaucheY = 0.f, solDroiteY = 0.f;
			bool solTracable = false;
	};

	// La consigne et la hauteur de semelle, exposées pour l'affichage.
	float PenteConsigneDeg() noexcept;
	float HauteurSemelle() noexcept;
	float DemiEcartementPieds() noexcept;

	// `brancheMondePhysique` faux = le NÉGATIF : l'IK retombe sur son plan plat.
	void Construire(bool brancheMondePhysique) noexcept;
	void Detruire() noexcept;

	// Place les jambes à l'abscisse `x` puis laisse l'IK converger `pas` fois.
	void Placer(float x, int pas) noexcept;

	// Remplit `out` pour l'abscisse `x` (déjà placée).
	void Lire(float x, Etat &out) noexcept;

	// ── Le montage CesiumMan : les os DESIGNES PAR LEUR NOM ──────────────
	// Renseigne par l'import de `Resources/Models/CesiumMan/CesiumMan.glb`,
	// dont les 19 joints sont nommes dans le fichier.
	struct OsNomme {
			char nom[64] = {};
			int indice = -1; // -1 = introuvable : un REFUS, jamais l'os 0
	};

	struct Import {
			bool charge = false;
			int osTotal = 0;
			int osNommes = 0; // combien portent un nom apres l'import
			OsNomme cuisse, mollet, pied;
			// ⚠️ La HIERARCHIE, et c'est la limite qui compte : `NkFootIKSystem`
			//    lit `Pose(i).localPosition` COMME une position monde. Vrai pour
			//    un squelette plat, faux pour CesiumMan dont les os ont un parent.
			int osAvecParent = 0;
			float piedLocalY = 0.f;  // ce que le pont lit
			float piedMondeY = 0.f;  // ce que c'est vraiment (LocalToWorld)
			char refus[160] = {};    // le refus nomme d'un nom absent
			int indiceAbsent = 0;    // ce que rend FindBone sur un nom inexistant
	};

	// Importe CesiumMan et remplit `out`. Ne touche pas la scene de la pente.
	bool ImporterCesiumMan(Import &out) noexcept;

	// Mode console : imprime les critères, rend le nombre d'échecs.
	int Mesurer() noexcept;

	// ⚠️ L'HORODATAGE DE **CETTE** UNITE DE COMPILATION.
	//    `__DATE__`/`__TIME__` dans main.cpp ne datent QUE main.cpp : apres
	//    une modification de scene.cpp seule, l'identite affichee restait
	//    celle d'avant, et une capture d'ecran aurait designe le mauvais
	//    binaire. Constate le 26/09 sur cette demo meme. On affiche donc les
	//    DEUX, et c'est la plus recente qui compte.
	const char *DateCompilationScene() noexcept;

} // namespace eprouvette
