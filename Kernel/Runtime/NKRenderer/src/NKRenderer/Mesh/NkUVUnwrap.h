#pragma once
// -----------------------------------------------------------------------------
// @File    NkUVUnwrap.h
// @Brief   DEPLIAGE UV (LSCM) : decoupe un maillage en ilots le long de ses
//          coutures, leur donne des coordonnees de texture, et MESURE ce que le
//          depliage a deforme.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER, ET POURQUOI ICI
//   Sans UV, aucune peinture de texture n'est possible — ni dans NK3DModeler, ni
//   dans Noge, ni dans NKScena. Les trois attendent la meme operation, donc elle
//   vit dans le noyau, a cote du maillage qu'elle deplie, et PAS dans une
//   application.
//
//   Le plan de donnees existait deja et personne ne l'avait remarque :
//   `NkEditMesh::Vert` porte `uv` et `uv2`, et comme un Vert EST un coin dans ce
//   maillage (copies dupliquees par face, identite soudee reconstruite par
//   `canonOf`), ces UV sont deja DES UV PAR COIN — le modele de Blender, celui
//   qu'un depliage exige : un meme point de l'espace doit pouvoir porter deux UV
//   differentes de part et d'autre d'une couture.
//
//   Consequence sur laquelle ce fichier repose : la topologie du depliage ne se
//   lit PAS sur les indices de `Vert`, qui separent deja des coins confondus. Un
//   LSCM cable naivement sur les `Vert` deplierait un cube en 6 ilots QUELLES QUE
//   SOIENT les coutures — et le resultat aurait l'air correct. On passe donc par
//   `VertOwner`/`canonOf` pour la connexite, et les `Vert` ne servent qu'a PORTER
//   le resultat.
//
// FORME DU CODE : DES FONCTIONS LIBRES
//   Ce n'est pas un gout : `NkEditMesh.h` (l.11-18) demande explicitement que les
//   operations avancees soient ecrites « comme FONCTIONS LIBRES operant sur
//   renderer::NkEditMesh, et NON via une structure demi-arete concurrente ». Une
//   deuxieme structure parallele a deja ete supprimee pour cette raison.
//
// L'ALGORITHME
//   LSCM — Levy B., Petitjean S., Ray N., Maillot J., « Least Squares Conformal
//   Maps for Automatic Texture Atlas Generation », ACM SIGGRAPH 2002,
//   ACM Transactions on Graphics 21(3), pp. 362-371.
//
//   Choisi contre ABF++ (Sheffer et al. 2005) pour trois raisons, dont la
//   troisieme est la seule qui compte :
//     1. il est LINEAIRE (un seul systeme aux moindres carres, aucune iteration
//        non lineaire qui puisse ne pas converger, aucune initialisation valide a
//        fournir) ;
//     2. il tolere les triangles fins la ou une methode purement angulaire diverge ;
//     3. il rend la mesure de distorsion FALSIFIABLE : une surface developpable
//        appartient au NOYAU de l'energie conforme, donc sur un quad plan
//        l'energie minimale vaut exactement zero et la solution est l'isometrie.
//        Si la distorsion mesuree n'est pas nulle sur un quad plan, ce n'est pas
//        une tolerance d'algorithme — c'est un defaut. Un algorithme qui rendrait
//        « presque zero » par nature aurait laisse une excuse.
//
//   Formulation. Chaque triangle est projete isometriquement dans son propre plan
//   en p1,p2,p3. Avec W_j = u_j + i.v_j et d_j = p_{j+2} - p_{j+1} (complexe,
//   indices modulo 3), la condition de Cauchy-Riemann d'une application affine
//   s'ecrit
//
//       somme_j ( W_j . d_j ) = 0
//
//   soit deux equations reelles par triangle, ponderees par 1/racine(aire) pour
//   l'invariance d'echelle. Le systeme est invariant par similitude (noyau de
//   dimension 4) : on FIXE DEUX sommets eloignes, ce qui retire exactement ces 4
//   degres de liberte. Les equations normales sont resolues par gradient conjugue
//   sur une matrice creuse en triplets (zero-STL, aucune allocation par iteration).
//
// CE QUE CE FICHIER NE FAIT PAS
//   La peinture de texture, l'editeur UV a la souris, et l'empaquetage OPTIMAL des
//   ilots. `NkUVUnwrapParams::packIslands` range les ilots cote a cote en bandes,
//   sans aucune pretention d'optimalite : c'est ce qu'il faut pour que les ilots
//   ne se recouvrent pas, pas le PackIslands promis ailleurs.
// -----------------------------------------------------------------------------

#include "NkEditMesh.h"

namespace nkentseu {
	namespace renderer {

		// ── POURQUOI UN REFUS NOMME PLUTOT QU'UN RESULTAT APPROXIMATIF ──────────
		// Un cube ferme sans couture est topologiquement une SPHERE, et il n'existe
		// aucune application continue et injective d'une sphere vers le plan. Un
		// solveur qui rendrait quand meme un resultat produirait NECESSAIREMENT un
		// recouvrement : il ferait echouer le controle de recouvrement en cachant
		// la vraie cause. Le refus dit la cause a l'endroit ou elle nait.
		//
		// Le critere n'est pas une opinion, il se compte : un ilot depliable est
		// homeomorphe a un DISQUE, donc sa caracteristique d'Euler V - E + F vaut
		// 1. Une sphere fermee donne 2, un tore 0, un anneau (deux bords) 0.
		enum class NkUVRefus : uint32 {
			Aucun = 0,
			MaillageVide,	 // aucune face vivante
			IlotNonDisque,	 // Euler != 1 : voir `refusEuler` pour le chiffre trouve
			AreteNonManifold, // plus de deux faces sur une arete : « l'autre cote » n'existe pas
			SolveurNonConverge, // le gradient conjugue n'a pas atteint la tolerance
			// ── COINS SOUDES : LA COUTURE NE PEUT PAS ETRE REPRESENTEE ──────
			// Il n'existe qu'UN champ `uv` par `Vert`. Si un maillage arrive avec
			// ses sommets SOUDES (un sommet partage par plusieurs faces, comme un
			// import classique), deux coins separes par une couture retombent sur
			// le meme `Vert` et ne peuvent pas porter deux UV differentes : la
			// couture est alors sans effet.
			//
			// CE REFUS EXISTE PARCE QU'UN NEGATIF L'A EXIGE. Le cas etait prevu
			// dans le code, mais il ressortait sous le nom `IlotNonDisque` — la
			// soudure fait effondrer le compte des sommets, donc Euler s'ecarte de
			// 1, et le solveur accusait la TOPOLOGIE d'un defaut de
			// REPRESENTATION. Un diagnostic juste sur le symptome et faux sur la
			// cause envoie corriger au mauvais endroit.
			// `NkUVResult::weldedCorners` donne le nombre de coins concernes ; la
			// reponse est de dupliquer les sommets le long des coutures avant de
			// deplier (ce que le solveur ne fait pas : il ne touche pas a la
			// topologie).
			CoinsSoudes,
		};

		const char *NkUVRefusName(NkUVRefus r) noexcept;

		struct NkUVIslandInfo {
			uint32 firstFace = 0; // offset dans le tableau de faces rendu par NkUVUnwrap
			uint32 faceCount = 0;
			// Le compte qui decide du refus. Ils sont RAPPORTES meme quand l'ilot
			// passe : un Euler juste par accident et un Euler juste par construction
			// s'annoncent pareil si on ne publie que le verdict.
			uint32 vertCount = 0; // sommets APRES decoupe (coins fusionnes par ilot)
			uint32 edgeCount = 0;
			int32 euler = 0; // vertCount - edgeCount + faceCount
			NkVec2f uvMin = {0.f, 0.f};
			NkVec2f uvMax = {0.f, 0.f};
		};

		// ── ⚠ UNE DE-SOUDURE SANS ECART EST TOPOLOGIQUEMENT INVISIBLE ──────────
		// Mesure, pas theorie. Apres `NkEditMesh::SplitEdges(..., keepGeometry = true)`
		// sur une sphere soudee, les SOMMETS sont bien dedoubles (144 -> 258, soit 114
		// copies) et `weldedCorners` retombe a 0 — mais le nombre d'ARETES ne bouge pas
		// (272), et la caracteristique d'Euler reste 0 au lieu de passer a 1.
		//
		// La raison : l'identite soudee de `NkEditMesh` est SPATIALE. `RebuildEdges`
		// rappelle `BuildVertexMerge`, qui refusionne par POSITION — et comme la
		// de-soudure n'a justement rien deplace, les copies retombent exactement l'une
		// sur l'autre. Le maillage se recoud tout seul.
		//
		// C'est aussi l'explication du `gap` « obligatoire » de `SplitSelectedEdges` :
		// son ecart de 1 % n'est pas une coquetterie d'affichage, c'est ce qui rend sa
		// decoupe PERSISTANTE. Une couture UV ne peut pas payer ce prix — deplacer la
		// geometrie deformerait le modele qu'on veut seulement deplier.
		//
		// CONSEQUENCE SUR L'USAGE, ET ELLE N'EST PAS FACULTATIVE : la de-soudure et les
		// coutures sont COMPLEMENTAIRES, jamais alternatives.
		//   `SplitEdges` donne a chaque coin son propre `Vert`, donc le DROIT de porter
		//                une UV distincte — c'est la REPRESENTATION ;
		//   `seams`      dit au solveur ou ne pas propager la connexite — c'est la
		//                TOPOLOGIE, et ca reste indispensable APRES la decoupe.
		// Deplier apres une de-soudure sans repasser les coutures rend un ilot ferme, et
		// le solveur le refuse a juste titre.
		struct NkUVUnwrapParams {
			// Aretes-coutures : indices dans `NkEditMesh::edges`. Les coutures sont
			// une ENTREE et non un champ de `Edge` : poser un `uint8 seam` dans une
			// structure que six chantiers se partagent obligerait chacun a la
			// reconstruire. Un appelant qui veut des coutures persistantes les garde
			// de son cote — c'est deja ce que fait la selection d'aretes.
			const NkEmId *seams = nullptr;
			uint32 seamCount = 0;

			// 0 => automatique (4 fois le nombre d'inconnues, borne a 4096).
			uint32 cgIterations = 0;
			float32 cgTolerance = 1e-12f;

			// Rangement des ilots cote a cote. NON OPTIMAL — cf. l'en-tete.
			bool packIslands = true;
			float32 packMargin = 0.02f;
		};

		// ── MESURE DE DISTORSION ────────────────────────────────────────────────
		// `area*` : rapport aireUV / aire3D par triangle, NORMALISE par le rapport
		// global (somme aireUV / somme aire3D).
		//
		// LA NORMALISATION N'EST PAS UN ARRANGEMENT, C'EST UNE NECESSITE. Le
		// resultat d'un LSCM est defini A UNE SIMILITUDE PRES : il depend de
		// l'ecartement donne aux deux sommets fixes. Un rapport d'aire BRUT
		// vaudrait donc « 4 » partout simplement parce que les deux pins ont ete
		// poses deux fois trop loin — un chiffre qui crierait une distorsion alors
		// que la forme est parfaite. Normalise, il vaut 1 partout pour toute
		// similitude et ne s'ecarte de 1 que si le depliage deforme vraiment.
		// Sans cela, le critere aurait rougi sur un solveur correct.
		//
		// `angle*` : ecart, en DEGRES, entre l'angle UV et l'angle 3D, pris au pire
		// des 3 coins de chaque triangle. Aucune normalisation possible ni
		// souhaitable : un angle est deja invariant par similitude.
		struct NkUVDistortion {
			float32 areaMin = 0.f, areaMean = 0.f, areaMax = 0.f;
			float32 angleMin = 0.f, angleMean = 0.f, angleMax = 0.f;
			uint32 triCount = 0;
		};

		struct NkUVResult {
			NkUVRefus refus = NkUVRefus::Aucun;
			uint32 refusIsland = 0xFFFFFFFFu; // ilot fautif, si refus
			int32 refusEuler = 0;			  // le chiffre trouve, pas seulement « != 1 »
			uint32 islandCount = 0;
			// Coins qu'une couture separait mais que le partage d'un meme `Vert`
			// a forces a se rejoindre. 0 sur un maillage ou un Vert est un coin.
			uint32 weldedCorners = 0;
			uint32 cgIterationsUsed = 0;
			float32 cgResidual = 0.f;
			NkUVDistortion distortion;
		};

		// ── LE SOLVEUR ──────────────────────────────────────────────────────────
		// Ecrit `Vert::uv` sur TOUS les coins des faces vivantes, et rien d'autre :
		// aucune modification de topologie, de position, de normale.
		//
		// EN CAS DE REFUS, AUCUNE UV N'EST ECRITE. Un refus qui laisserait des UV a
		// moitie posees serait pire qu'une erreur : l'appelant verrait un maillage
		// partiellement deplie et croirait a un demi-succes.
		//
		// `outIslands` / `outIslandFaces` sont facultatifs ; quand ils sont fournis,
		// `outIslandFaces` contient les faces de tous les ilots a la suite, et
		// chaque `NkUVIslandInfo` y pointe par (firstFace, faceCount).
		// -- COUTURES AUTOMATIQUES : ARBRE COUVRANT DU DUAL ---------------------
		// DEMENAGEE DEPUIS LE BANC, le 25/09/2026, et c'est la raison pour laquelle
		// ce module n'avait AUCUN appelant dans le produit. `NkUVUnwrapParams::seams`
		// est une ENTREE OBLIGATOIRE pour toute surface fermee : sans coupe, une
		// sphere a un Euler de 2, le solveur refuse a juste titre, et le refus
		// ressemble a une panne. Or la seule fonction du depot capable de produire
		// ces coutures vivait `static` dans `NKUVUnwrapTest/src/main.cpp` : un
		// deplieur de 1 045 lignes rendu inappelable par l'absence de son entree.
		// Le fil manquant etait la, pas dans le solveur.
		//
		// Propriete utilisee : decouper une surface fermee de genre 0 le long du
		// complementaire d'un arbre couvrant de son dual donne EXACTEMENT un
		// disque. L'attendu se DERIVE des chiffres du maillage :
		//   coutures = E - (F - 1)   ·   ilots = 1   ·   Euler = 1
		//
		// ATTENTION -- C'EST UNE COUPE VALIDE, PAS UNE BONNE COUPE. Elle garantit
		// un disque ; elle ne dit rien de la distorsion ni du nombre d'ilots qu'un
		// artiste voudrait. Un placement de coutures intelligent est un autre
		// sujet ; celui-ci rend le module APPELABLE, et la mesure dira ce que la
		// coupe brute vaut vraiment.
		//
		// `dropOne` retire N coutures : sert au NEGATIF du banc (moins de coupes
		// donc moins d'ilots). Rend le nombre de coutures produites.
		// `outFacesVisitees` : LE CHIFFRE QUI DIT SI LE PARCOURS A TRAVERSE.
		// Un arbre couvrant du dual doit atteindre TOUTES les faces vivantes ;
		// s'il n'en atteint qu'une, aucune arete n'est dans l'arbre, donc TOUTES
		// deviennent des coutures -- et chaque triangle se retrouve seul dans son
		// ilot. Sans ce compte, ce cas se lit comme une mauvaise strategie de
		// coupe alors que c'est un parcours qui n'avance pas.
		uint32 NkUVSeamsFromDualSpanningTree(const NkEditMesh &mesh, NkVector<NkEmId> &outSeams,
											 uint32 dropOne = 0u,
											 uint32 *outFacesVisitees = nullptr) noexcept;

		bool NkUVUnwrap(NkEditMesh &mesh, const NkUVUnwrapParams &params, NkUVResult &outResult,
						NkVector<NkUVIslandInfo> *outIslands = nullptr,
						NkVector<NkEmId> *outIslandFaces = nullptr) noexcept;

		// ── LES DEUX MESURES, INDEPENDANTES DU SOLVEUR ──────────────────────────
		// Elles lisent `Vert::uv` et ne savent RIEN de la facon dont ces UV sont
		// arrivees la. C'est ce qui permet de separer un defaut de solveur d'un
		// defaut de mesure : on pose a la main des UV dont on connait la reponse,
		// on mesure, et le verdict designe l'un ou l'autre sans ambiguite.

		// Aire totale de recouvrement entre triangles UV, par decoupe de polygone
		// convexe (Sutherland-Hodgman) — une AIRE, pas un predicat booleen.
		//
		// Aucune paire n'est exclue, et c'est voulu : deux triangles voisins qui
		// partagent une arete rendent une aire d'intersection nulle par
		// construction. Une liste d'exclusions serait exactement l'endroit ou un
		// detecteur devient aveugle sans le dire.
		// Rend le NOMBRE de paires en recouvrement ; `outArea` recoit l'aire totale.
		// -- LA CHAINE COMPLETE : D'UN MAILLAGE IMPORTE A SES UV ----------------
		// Coutures automatiques, de-soudure, reconstruction, depliage, mesures.
		// Elle n'invente aucun calcul : elle ENCHAINE quatre briques deja
		// eprouvees. Elle existe parce qu'aucun appelant du produit ne pouvait
		// les enchainer -- la sequence n'etait ecrite que dans un cas de banc.
		//
		// `NkUVAutoBilan` publie les chiffres de chaque etape, y compris le
		// nombre de coutures RETROUVEES apres la reconstruction : un appariement
		// incomplet ferait rougir le depliage pour une raison etrangere au
		// depliage, et il faut pouvoir le distinguer.
		struct NkUVAutoBilan {
			uint32 facesVisitees = 0; // par le parcours du dual
			uint32 facesTotal = 0;
			uint32 coutures = 0;
			uint32 couturesRetrouvees = 0;
			uint32 sommetsAvant = 0;
			uint32 sommetsApres = 0;
			uint32 sommetsDupliques = 0;
			uint32 pairesRecouvrement = 0;
			float32 aireRecouvrement = 0.f;
		};
		bool NkUVUnwrapAuto(NkEditMesh &mesh, const NkUVUnwrapParams &base, NkUVResult &outResult,
							NkUVAutoBilan *outBilan = nullptr) noexcept;

		uint32 NkUVCountOverlaps(const NkEditMesh &mesh, float32 *outArea = nullptr) noexcept;

		bool NkUVMeasureDistortion(const NkEditMesh &mesh, NkUVDistortion &out) noexcept;

		// ── SURVIE A L'ENREGISTREMENT ───────────────────────────────────────────
		// Enregistrement BINAIRE des UV par coin : les float32 tels quels, bit pour
		// bit, jamais de texte.
		//
		// POURQUOI UN FORMAT PROPRE PLUTOT QUE L'EXPORT DU MAILLAGE : il n'existe
		// aujourd'hui AUCUN ecrivain de maillage dans le depot. `NkGLTFIO.h:50` dit
		// « aucun ecrivain glTF/GLB n'existe », `NkOBJIO.cpp:82` rend false avec
		// « NON IMPLEMENTE », et le `Serialize` de `NkEditMesh.h:1350` appartient a
		// `NkMeshEditRecorder` : il serialise les COMMANDES d'edition, pas le
		// maillage. Ecrire un exportateur complet serait un autre lot.
		//
		// La relecture se fait par NkFile::ReadAllBytes, JAMAIS par ReadAllText :
		// WriteAllText ecrit en CRLF et ReadAllText renormalise, ce qui masque
		// l'ecart des deux cotes a la fois.
		bool NkUVSaveLayout(const NkEditMesh &mesh, const char *path) noexcept;

		// Rend false si le fichier ne correspond pas au maillage (nombre de coins
		// different, signature absente) : relire des UV dans le mauvais maillage
		// produirait un resultat plausible et faux.
		bool NkUVLoadLayout(NkEditMesh &mesh, const char *path) noexcept;

	} // namespace renderer
} // namespace nkentseu
