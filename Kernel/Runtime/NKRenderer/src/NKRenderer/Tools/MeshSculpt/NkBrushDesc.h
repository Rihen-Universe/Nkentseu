#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkBrushDesc.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Le DESCRIPTEUR d'une brosse de sculpture : une DONNEE, pas un cas
//          d'enumeration.
//
// LA DEMANDE QUI A PRODUIT CE FICHIER (Rodolf, 19/09/2026)
//   « Une brosse ne peut pas etre une enumeration, car on sera amenes a en
//     creer d'autres plus tard, ou a ajouter des brosses SANS AVOIR A
//     RECOMPILER. »
//
//   L'etat d'avant : `NkSculptBrushMode` declarait HUIT modes en dur, et cinq
//   profils d'attenuation en dur. Ajouter « Glaise » exigeait de toucher
//   l'enumeration, le kernel, et la liste de l'interface -- trois fichiers et
//   une recompilation pour ce qui est, en verite, un JEU DE COEFFICIENTS.
//
// LA FRONTIERE, ET ELLE EST HONNETE
//   Ce descripteur ne rend pas « tout » possible sans recompiler, et personne
//   ne doit lire ce fichier en croyant le contraire. Il y a trois zones :
//
//   1. EN DONNEES SEULES -- toute brosse qui est une COMBINAISON PARAMETREE
//      des primitives ci-dessous. C'est la grande majorite du catalogue :
//      Dessiner, Creuser, Gonfler, Aplatir, Glaise, Pincer, Ecarter sont la
//      MEME poignee d'operations avec des coefficients differents. Creuser,
//      c'est Dessiner avec `sens = -1` -- pas une ligne de code neuve.
//   2. AVEC UN EVALUATEUR D'EXPRESSIONS -- une brosse dont le deplacement est
//      une expression des memes entrees. Pas fait, pas promis.
//   3. IMPOSSIBLE EN DONNEES -- une brosse qui change la TOPOLOGIE (remaillage
//      dynamique, Serpent qui tire de la matiere neuve, Pli qui insere des
//      aretes) ou qui porte un ETAT entre les tampons. Ca demande du code neuf.
//
//   NkSL ne change pas cette frontiere ICI : il compile des nuanceurs, donc il
//   ouvre la brosse programmable du cote 2.5D (pixol). La sculpture VOLUMIQUE
//   deforme des sommets CPU sur une structure demi-arete ; NkSL n'y a pas de
//   prise.
//
// ⚠️ AUCUNE PRIMITIVE N'EST DECLAREE ICI SANS ETRE IMPLEMENTEE. Le depot a paye
//    « 108 widgets declares, 2 qui peignent » : une enumeration qui annonce dix
//    operations dont deux agissent est pire qu'une enumeration de deux, parce
//    qu'elle se lit comme un inventaire. `NK_SCULPT_OP_COUNT` compte ce qui
//    AGIT. Une donnee qui reclame une primitive absente est REFUSEE AVEC SON
//    MOTIF -- jamais repliee en silence sur une primitive voisine.
// -----------------------------------------------------------------------------

#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		using namespace math;

		// Version du format de donnees `.nkbrush`. Un fichier qui annonce une
		// version superieure est refuse : mieux vaut un refus lisible qu'une
		// lecture partielle qui perd des champs en silence.
		static constexpr uint32 kNkBrushFormatVersion = 1;

		// ─────────────────────────────────────────────────────────────────────
		// LES PRIMITIVES. C'est le vocabulaire ferme sur lequel les brosses se
		// composent. Il est VOLONTAIREMENT court : chaque entree ici est du code
		// a ecrire, a eprouver et a maintenir, alors qu'une brosse de plus n'est
		// qu'un fichier de donnees.
		//
		// ⚠️ Cette enumeration n'est PAS « la liste des brosses ». C'est la liste
		//    des GESTES ELEMENTAIRES. Dessiner et Creuser sont la meme primitive.
		// ─────────────────────────────────────────────────────────────────────
		enum class NkSculptOp : uint8 {
			NK_SCULPT_OP_NORMAL = 0, ///< Deplace le long de la normale du sommet.
			/// Rapproche chaque sommet de la MOYENNE DE SES VOISINS. D'une autre
			/// nature que la precedente : elle ne suit aucune direction imposee, elle
			/// REDUIT un ecart. C'est pour ca qu'elle valait d'etre la deuxieme --
			/// une seconde primitive qui n'aurait fait que changer de direction
			/// n'aurait rien prouve du mecanisme.
			/// `sens = -1` l'inverse en ACCENTUANT l'ecart (le relief se durcit) :
			/// ce n'est pas un effet de bord, c'est la meme formule prise a rebours.
			NK_SCULPT_OP_SMOOTH = 1,
			/// PEINT LE MASQUE au lieu de deplacer la matiere. C'est la TROISIEME
			/// primitive, et elle est d'une autre nature que les deux premieres :
			/// elle n'ecrit AUCUNE position -- elle ecrit un poids par sommet
			/// (NkEditMesh::vertMask), que les deux autres LISENT ensuite.
			/// `sens = -1` EFFACE le masque au lieu de le poser : meme geste, meme
			/// formule prise a rebours, exactement comme creuser/dessiner.
			/// ⚠️ Elle entre dans cette table parce que `NkSculptApplyStroke` la
			///    TRAITE, pas parce qu'on la prevoit -- la regle ecrite au-dessus.
			NK_SCULPT_OP_MASK = 2,
			NK_SCULPT_OP_COUNT		 ///< ⚠️ Compte ce qui AGIT, pas ce qu'on projette.
		};

		// Profil d'attenuation du centre (t=0) vers le bord (t=1) de la brosse.
		enum class NkSculptFalloffKind : uint8 {
			NK_FALLOFF_SMOOTH = 0, ///< smoothstep, module par `durete`.
			NK_FALLOFF_LINEAR = 1,
			NK_FALLOFF_CONSTANT = 2, ///< Plein jusqu'au bord (tampon dur).
			NK_FALLOFF_SHARP = 3,
			NK_FALLOFF_SPHERE = 4, ///< Profil hemispherique.
			NK_FALLOFF_COUNT
		};

		// ─────────────────────────────────────────────────────────────────────
		// LE DESCRIPTEUR.
		//
		// ⚠️ POD A CHAMPS FIXES, AUCUNE ALLOCATION, AUCUN POINTEUR. Ce choix n'est
		//    pas de l'economie : c'est ce qui rend le registre sur.
		//    `NkComponentRegistry` (NKEditorKit) garde des POINTEURS vers des
		//    declarations statiques -- correct pour lui, qui enumere du COMPILE,
		//    et c'est la faute que le depot a payee en defaut de segmentation
		//    mouvant quand une declaration arrivait par valeur. Ici les
		//    descripteurs viennent de FICHIERS : ils n'ont aucune duree de vie
		//    statique a offrir. Le registre doit donc les POSSEDER, et un POD
		//    copiable est ce qui rend cette possession sans piege.
		//
		// ⚠️ `char[]` et non `NkString` : pour que la structure reste trivialement
		//    copiable et sans allocation. Une copie de descripteur ne doit jamais
		//    pouvoir echouer.
		// ─────────────────────────────────────────────────────────────────────
		struct NkBrushDesc {
				static constexpr uint32 kNameCap = 48;
				static constexpr uint32 kLabelCap = 64;
				static constexpr uint32 kIconCap = 32;

				// IDENTITE STABLE, jamais traduite, jamais affichee. C'est par ce
				// nom qu'une donnee, un raccourci ou un fichier de projet designe
				// la brosse. Le depot a deja paye « un panneau porte un identifiant
				// stable ; renommer ou traduire ne doit pas perdre la disposition ».
				char name[kNameCap] = {};
				// CE QUE L'UTILISATEUR LIT. Traduisible sans rien casser.
				char label[kLabelCap] = {};
				char icon[kIconCap] = {};

				NkSculptOp op = NkSculptOp::NK_SCULPT_OP_NORMAL;
				NkSculptFalloffKind falloff = NkSculptFalloffKind::NK_FALLOFF_SMOOTH;

				float32 radius = 0.25f;	  ///< Rayon en UNITES MONDE (pas en pixels).
				float32 strength = 0.5f;  ///< Intensite [0..1].
				float32 hardness = 0.5f;  ///< Durete du profil [0..1].
				// ⚠️ LE CHAMP QUI PORTE LA DEMANDE DE RODOLF. `sens = -1` transforme
				//    Dessiner en Creuser SANS UNE LIGNE DE CODE. C'est l'exemple le
				//    plus court de « une brosse est une donnee ».
				float32 dir = 1.f;		  ///< +1 sort de la surface, -1 y entre.
				float32 spacing = 0.25f;  ///< Espacement des tampons (x rayon).

				// Bornes de l'interface. Elles vivent dans la DONNEE parce qu'une
				// brosse de detail et une brosse de blocage n'ont pas la meme
				// plage utile -- les coder en dur forcerait toutes les brosses a
				// partager les reglages de la premiere ecrite.
				float32 radiusMin = 0.001f, radiusMax = 10.f;
				float32 strengthMin = 0.f, strengthMax = 1.f;

				bool valid = false; ///< false tant qu'un parseur ne l'a pas remplie.
		};

		// ─────────────────────────────────────────────────────────────────────
		// LECTURE D'UN DESCRIPTEUR DEPUIS DU TEXTE.
		//
		// ⚠️ PREND UNE CHAINE, PAS UN CHEMIN. Ce module n'ouvre aucun fichier, et
		//    c'est deliberemment le meme arbitrage que `NkThemeLibrary`, qui
		//    « n'ouvre volontairement aucun fichier : dependre de NKFileSystem lui
		//    ferait perdre sa propriete d'etre testable sans rien lier ».
		//    L'application sait ou sont ses dossiers ; le noyau sait lire.
		// ─────────────────────────────────────────────────────────────────────
		enum class NkBrushParse : uint8 {
			NK_BRUSH_OK = 0,
			NK_BRUSH_ERR_MAGIC,		 ///< Premiere ligne utile != « nkbrush <version> ».
			NK_BRUSH_ERR_VERSION,	 ///< Version du format inconnue.
			NK_BRUSH_ERR_NO_NAME,	 ///< Pas de `nom` : la brosse n'a pas d'identite.
			NK_BRUSH_ERR_UNKNOWN_OP, ///< `operation` nomme une primitive qui n'existe pas.
			NK_BRUSH_ERR_UNKNOWN_FALLOFF,
			NK_BRUSH_ERR_BAD_NUMBER, ///< Un champ numerique est illisible.
			NK_BRUSH_ERR_OUT_OF_RANGE
		};

		// Rend le motif, et ecrit dans `errBuf` la ligne fautive quand il y en a
		// une. ⚠️ UN REFUS EST TOUJOURS NOMME : le depot a paye « deux politiques
		// opposees sur le repli », et la regle qui en sort est qu'un repli muet
		// est le pire des deux. Une brosse illisible ne doit pas apparaitre en
		// silence avec des valeurs par defaut -- elle ne doit pas apparaitre.
		NkBrushParse ParseBrushDesc(const char *text, uint32 len, NkBrushDesc &out, char *errBuf,
									uint32 errCap) noexcept;

		// Libelle court d'un motif de refus (pour le journal et l'interface).
		const char *NkBrushParseText(NkBrushParse r) noexcept;

		// Poids d'attenuation a la distance normalisee `t` dans [0..1].
		// ⚠️ UN SEUL SITE POUR CE CALCUL. Le meme profil doit valoir la meme chose
		//    dans l'apercu de l'interface et dans la deformation ; deux copies
		//    divergent, et l'ecart ne se voit que sur une forme, jamais dans un
		//    compteur. (« Le meme calcul a deux sites, garde a un seul ».)
		float32 NkBrushFalloff(float32 t, NkSculptFalloffKind kind, float32 hardness) noexcept;

	} // namespace renderer
} // namespace nkentseu
