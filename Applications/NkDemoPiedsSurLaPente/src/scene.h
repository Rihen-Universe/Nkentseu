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
			float solSousPiedD = 0.f; // ... et sous le droit
			bool solLisible = false;

			// ── LA PHASE DE CHAQUE PIED, telle qu'elle est FOURNIE ───────────
			// C'est le poids de plante lu dans le composant, pas une deduction
			// faite ici : ce que l'ecran montre est ce que l'IK a recu.
			float planteG = 1.f, planteD = 1.f;

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

	// ⚠️ Le squelette importe traverse la frontiere par POINTEUR OPAQUE :
	//    `scene.h` ne peut pas nommer `ecs::NkSkeleton` sans inclure l'en-tete
	//    qui le declare, et `import_cesium.cpp` ne peut pas cotoyer
	//    `NkLocomotion.h` (conflit `NkSpan`). Le pointeur appartient a
	//    l'importeur ; l'appelant ne le libere pas.
	void *SqueletteCesiumMan() noexcept;

	// ── Le montage CesiumMan SUR LA PENTE ────────────────────────────────
	// C'est LE critere du correctif : un squelette HIERARCHIQUE pose ses pieds.
	// L'eprouvette plate, elle, donne les memes chiffres avant et apres le
	// correctif (composition monde = identite quand tous les parents valent -1)
	// -- donc elle ne peut pas le distinguer d'un placebo.
	struct SurPente {
			bool monte = false;
			float solY = 0.f;      // sous le pied, lu par raycast
			float piedY = 0.f;     // position MONDE du pied apres l'IK
			float piedXMonde = 0.f;
			float penteLueDeg = 0.f;
	};

	// `branche` : l'IK interroge le monde physique (sinon : plan plat a y=0).
	bool PoserCesiumSurPente(float x, bool branche, SurPente &out) noexcept;

	// ── L'ALTERNANCE, IMPOSEE A LA MAIN ───────────────────────────────────
	// Fournit le poids de plante des deux pieds depuis l'exterieur, en creneau.
	// ⚠️ CE N'EST PAS UN CYCLE DE MARCHE, et la distinction est le sujet : un
	//    generateur de cycle serait du NEUF. Ici, deux valeurs en opposition
	//    de phase, juste assez pour que la question se voie.
	void ImposerAlternance(float temps, bool active) noexcept;

	// ── La compensation de hanche, mise a l'epreuve ────────────────────────
	// Deux soupcons a la LECTURE de NkFootIKSystem, qu'il faut MESURER :
	//   (1) `dL/dR` valent une ALTITUDE de sol, pas un ecart -- donc sur un sol
	//       haut la hanche recevrait une correction proportionnelle a l'altitude ;
	//   (2) `sk.Pose(hip).localPosition.y += hipOffset` s'AJOUTE a chaque image
	//       sans defaire la precedente.
	// ⚠️ Le defaut (1) DORMAIT parce que le repli met le sol a y = 0 : zero fois
	//    la compensation vaut zero. Il ne se reveille que sur du relief.
	struct Hanche {
			bool mesure = false;
			float avant = 0.f;    // hauteur de la hanche AVANT toute execution
			float apres1 = 0.f;   // hauteur de la hanche apres 1 image
			float apres30 = 0.f;  // ... apres 30 images, sans rien replacer
			float apres200 = 0.f; // ... apres 200 : la convergence est finie ici
			float offsetVu = 0.f; // hipOffset tel que le composant le porte
			bool groundeG = false, groundeD = false; // les DEUX pieds touchent-ils ?

			// ── De quoi former une EGALITE au lieu d'un seuil ─────────────────
			// La compensation doit valoir la correction du pied LE MOINS corrige,
			// multipliee par hipCompensation. C'est ce que le code pretend faire ;
			// une ALTITUDE de sol ne satisferait pas cette egalite.
			float corrPiedG = 0.f, corrPiedD = 0.f; // deplacement vertical de chaque pied
			float compensation = 0.5f;               // hipCompensation lu dans le composant
	};

	// Sur la pente, a une abscisse ou le sol est HAUT : la hanche derive-t-elle ?
	bool EprouverHanche(Hanche &out) noexcept;

	// ── UN PIED EN ENVOL EST-IL RAMENE AU SOL ? ──────────────────────────
	// Rodolf, 26/09 : « on ne peut pas distinguer les pieds qui montent
	// naturellement ». Mesure de structure : ni NkFootContact, ni NkFootIK, ni
	// NkLocomotion ne portent de notion de PHASE. Le champ le plus proche,
	// `contactWeight`, est un lissage de `isGrounded` -- vrai des que le sol
	// est A PORTEE DU RAYON, pas quand le pied TOUCHE.
	//
	// ⚠️ CETTE SONDE EST UN DIAGNOSTIC, PAS UN CRITERE : y remedier serait du
	//    NEUF (une phase d'appui/envol), donc une decision de Rodolf. Un banc
	//    durablement rouge ne protege plus rien.
	struct Envol {
			bool mesure = false;
			float leveA = 0.f;    // ou on a POSE le pied, au-dessus du sol
			float apres = 0.f;    // ou il se retrouve apres l'IK (60 images)
			// ⚠️ TROIS RELEVES DANS LE TEMPS, et c'est le seul moyen de distinguer
			//    un POIDS d'un TAUX : un poids se stabilise et y reste ; un taux
			//    continue de descendre, meme lentement.
			float a10 = 0.f, a60 = 0.f, a300 = 0.f;
			float solSous = 0.f;
			float poids = 0.f;    // contactWeight du pied leve
			bool groundeEnLair = false; // « touche le sol » alors qu'il est en l'air ?
	};

	// Leve le pied gauche de `hauteur` metres, fixe son POIDS DE PLANTE, puis
	// laisse l'IK travailler. `plante` : 0 = envol, 1 = appui.
	bool EprouverEnvol(float hauteur, float plante, Envol &out) noexcept;

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
