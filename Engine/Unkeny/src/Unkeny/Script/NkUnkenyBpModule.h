//
// NkUnkenyBpModule.h
// =============================================================================
// Description :
//   La forme NON NODALE d'un Blueprint : un MODULE DE BYTECODE (document 01,
//   § 4.6 et § 6.2 ; decision du 24/08 dans NKGraph/ROADMAP.md : il doit
//   « s'ecrire, se lire et se serialiser SANS QU'AUCUN GRAPHE N'EXISTE »).
//   Ce fichier ne connait pas NKGraph : le jeu livre n'a ni graphe ni
//   compilateur, il lit des octets, les VERIFIE, et la machine (NkUnkenyBpMachine)
//   les execute.
//
// Caracteristiques :
//   - Registres TYPES (booleen, entier, reel, vec2, entite, texte) ; un cadre par
//     appel d'evenement ; les variables vivent dans le composant (NkScript2D).
//   - Les NATIFS sont les fonctions de NkUnkHoteV1, designees PAR NOM
//     (« unkeny.corps.impulsion ») et resolues au chargement : la lecon de
//     NKGraph sur l'indice contre le nom, appliquee d'emblee.
//   - Octets ecrits EXPLICITEMENT en petit-boutiste, champ par champ : un module
//     voyage entre plateformes.
//   - VERIFIE avant toute execution (NkVerifierModuleBp) : opcodes connus,
//     registres dans le cadre, constantes, variables et imports dans leurs
//     tables, sauts sur un debut d'instruction, types conformes, natifs
//     autorises pour l'evenement (une force hors « Pas fixe » est refusee). Un
//     module refuse DIT pourquoi, et ou (fonction, pc, noeud).
//   - Le fichier .nkbp (NkAssetIO, type Blueprint) porte DEUX sections : GRAF
//     (le texte .nkgraph de l'editeur, l'autorat, retirable a la livraison) et
//     MODL (ce module, seul lu par le jeu).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYBPMODULE_H__
#define __NKENTSEU_UNKENY_NKUNKENYBPMODULE_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace unkeny {

		/// Version du format du module (section MODL). 2 (2026-10-01, editeur de
		/// Blueprint a la UE5) : chaque fonction porte sa SIGNATURE (parametres,
		/// resultats) et le code peut APPELER une fonction du module (NK_APPEL).
		/// Le format 1 reste LU (fonctions sans signature : les evenements).
		constexpr uint16 NK_BP_FORMAT = 2u;
		constexpr uint16 NK_BP_FORMAT_MIN = 1u;
		/// Profondeur d'appels de fonctions du Blueprint (une recursion sans fin
		/// est coupee, comme une boucle sans fin l'est par le budget).
		constexpr uint32 NK_BP_PILE_MAX = 64u;
		/// Version de la charge utile d'un .nkbp (en-tete « NKBP »).
		constexpr uint32 NK_BP_FICHIER_VERSION = 1u;
		/// Instructions par evenement avant que l'instance soit mise en faute.
		constexpr uint32 NK_BP_BUDGET = 100000u;
		/// Registres par fonction.
		constexpr uint32 NK_BP_REGISTRES_MAX = 256u;

		/// Les types d'une valeur. AJOUTES A LA FIN.
		/// NK_COULEUR (2026-10-01) : RVBA empaquete dans `i` (0xRRVVBBAA, le format
		/// de NkSprite2D::couleur).
		enum class NkTypeBp : uint8 { NK_RIEN = 0, NK_BOOLEEN, NK_ENTIER, NK_REEL, NK_VEC2, NK_ENTITE, NK_TEXTE, NK_COULEUR };
		constexpr uint8 NK_BP_TYPE_DERNIER = static_cast<uint8>(NkTypeBp::NK_COULEUR);
		const char *NkNomTypeBp(NkTypeBp t) noexcept;

		/// Une valeur. Pas d'union : un registre lu sous un autre type que le sien
		/// est refuse par le verificateur, il n'y a rien a reinterpreter.
		struct NkValeurBp {
				float32 x = 0.f, y = 0.f; ///< reel (x), vec2 (x, y)
				/// entier, booleen (0/1), couleur (RVBA), texte : >= 0 indice de
				/// constante, < 0 texte FABRIQUE pendant l'appel (-1 - k dans
				/// NkCadreBp::textes : une concatenation, une variable texte).
				int32 i = 0;
				uint64 e = 0u;			  ///< entite (NkEntityId::Pack, 0 = aucune)
		};

		/// Les instructions. Un mot d'opcode, puis ses operandes (un mot chacun).
		/// AJOUTES A LA FIN : un module ancien garde ses numeros.
		enum class NkOpBp : uint32 {
			NK_FIN = 0,		 ///< ()                 fin de la fonction
			NK_CONST,		 ///< (r, k)             r = constante k
			NK_COPIER,		 ///< (r, s)             meme type
			NK_LIRE_VAR,	 ///< (r, v)
			NK_ECRIRE_VAR,	 ///< (v, r)
			NK_SOI,			 ///< (r:E)
			NK_ARG,			 ///< (r, n)             argument n de l'evenement (NkArgBp)
			NK_ADD_R,		 ///< (r:R, a:R, b:R)
			NK_SUB_R,
			NK_MUL_R,
			NK_DIV_R,		 ///< division par zero : FAUTE (pas d'infini silencieux)
			NK_ADD_I,		 ///< (r:I, a:I, b:I)
			NK_SUB_I,
			NK_MUL_I,
			NK_DIV_I,
			NK_ADD_V,		 ///< (r:V, a:V, b:V)
			NK_SUB_V,
			NK_MUL_VR,		 ///< (r:V, a:V, b:R)
			NK_LT_R,		 ///< (r:B, a:R, b:R)
			NK_LE_R,
			NK_EQ_R,
			NK_LT_I,		 ///< (r:B, a:I, b:I)
			NK_LE_I,
			NK_EQ_I,
			NK_EQ_E,		 ///< (r:B, a:E, b:E)
			NK_ET,			 ///< (r:B, a:B, b:B)
			NK_OU,
			NK_NON,			 ///< (r:B, a:B)
			NK_I2R,			 ///< (r:R, a:I)
			NK_VEC2,		 ///< (r:V, x:R, y:R)
			NK_VX,			 ///< (r:R, a:V)
			NK_VY,
			NK_LONGUEUR,	 ///< (r:R, a:V)
			NK_NORMALISER,	 ///< (r:V, a:V)
			NK_SAUT,		 ///< (pc)
			NK_SAUT_SI_FAUX, ///< (r:B, pc)
			NK_NATIF,		 ///< (k, params..., resultats...) — nombres pris dans la signature de l'import k
			/// (2026-10-01, format 2) (f, params..., resultats...) : appelle la
			/// fonction f du module ; les nombres sont ceux de SA signature. Les
			/// parametres arrivent dans ses premiers registres, ses resultats sont
			/// les registres qui suivent (rendus a FIN).
			NK_APPEL,
			/// (cible:E, nom:K texte, n, args...) : appelle le REPARTITEUR `nom` sur
			/// l'entite `cible` -- l'hote livre l'evenement personnalise a ses
			/// scripts APRES l'evenement en cours (une file, jamais une reentree).
			NK_DIFFUSER,
			NK_NOMBRE
		};
		const char *NkNomOpBp(NkOpBp op) noexcept;

		/// Les arguments d'evenement (NK_ARG).
		enum class NkArgBp : uint32 {
			NK_DT = 0,		   ///< reel
			NK_AUTRE,		   ///< entite
			NK_SOI_EST_ZONE,   ///< booleen
			NK_VALEUR,		   ///< reel (l'axe d'une action)
			NK_NOMBRE
		};
		NkTypeBp NkTypeArgBp(uint32 n) noexcept;

		struct NkImportBp {
				NkString nom; ///< nom qualifie du natif
				NkVector<NkTypeBp> params;
				NkVector<NkTypeBp> resultats;
		};
		struct NkVariableBp {
				NkString nom;
				NkTypeBp type = NkTypeBp::NK_REEL;
				NkValeurBp defaut;
				bool exposee = true;
		};
		struct NkConstanteBp {
				NkTypeBp type = NkTypeBp::NK_REEL;
				NkValeurBp valeur;
				NkString texte; ///< type texte
		};
		/// Une entree : quel evenement appelle quelle fonction.
		struct NkEntreeBp {
				uint32 genre = 0u;	///< NK_UNK_EV_*
				NkString parametre; ///< ACTION_* : le NOM de l'action ; vide sinon
				uint32 fonction = 0u;
		};
		/// pc -> noeud du graphe source (facultatif) : le journal designe le noeud.
		struct NkLigneBp {
				uint32 pc = 0u;
				uint32 noeud = 0u;
		};
		struct NkFonctionBp {
				NkString nom;
				/// La SIGNATURE (format 2) : les parametres sont les registres
				/// 0..P-1, les resultats les registres P..P+R-1. Vide pour un
				/// evenement (une entree n'en a pas).
				NkVector<NkTypeBp> params;
				NkVector<NkTypeBp> resultats;
				NkVector<NkTypeBp> registres;
				NkVector<uint32> code;
				NkVector<NkLigneBp> lignes;
		};

		struct NkModuleBp {
				uint16 format = NK_BP_FORMAT;
				uint16 abiMajeure = 1u;
				uint16 abiMineure = 0u;
				/// L'empreinte du texte GRAF qui l'a produit (0 = ecrit a la main) :
				/// un graphe modifie a la main se signale « module perime ».
				uint64 empreinte = 0u;
				NkVector<NkImportBp> imports;
				NkVector<NkVariableBp> variables;
				NkVector<NkConstanteBp> constantes;
				NkVector<NkEntreeBp> entrees;
				NkVector<NkFonctionBp> fonctions;

				/// Le noeud du pc `pc` de la fonction `f` (la ligne la plus proche
				/// en amont), 0 s'il n'y en a pas.
				uint32 NoeudDe(uint32 f, uint32 pc) const noexcept;
		};

		// --- Octets --------------------------------------------------------------
		void NkEcrireModuleBp(const NkModuleBp &m, NkVector<uint8> &sortie);
		/// false (et `erreur`) : octets tronques, magie ou format inconnus.
		/// ⚠️ LIRE n'est pas VERIFIER : un module lu doit passer NkVerifierModuleBp.
		bool NkLireModuleBp(const uint8 *octets, usize taille, NkModuleBp &sortie, NkString *erreur = nullptr);

		// --- Verification ----------------------------------------------------------
		/// Un natif tel que le verificateur le connait (NkUnkenyBpNatifs.h le
		/// remplit depuis la table de l'hote).
		struct NkSignatureNatifBp {
				const char *nom = nullptr;
				uint8 nbParams = 0u;
				NkTypeBp params[4] = {};
				uint8 nbResultats = 0u;
				NkTypeBp resultats[2] = {};
				uint32 evenements = 0xFFFFFFFFu; ///< bit = NK_UNK_EV_* autorise
		};

		/// Pourquoi un module est refuse.
		struct NkRefusBp {
				NkString raison;
				int32 fonction = -1;
				int32 pc = -1;
				uint32 noeud = 0u;
		};

		/// Verifie `m` contre les natifs connus. `natifs` (sortie) : pour chaque
		/// import, l'indice du natif dans `table`. false : `refus` dit pourquoi.
		bool NkVerifierModuleBp(const NkModuleBp &m, const NkSignatureNatifBp *table, uint32 nbTable,
								NkVector<int32> &natifs, NkRefusBp &refus);

		// --- Le fichier .nkbp (NkAssetIO, NkAssetType::Blueprint) ------------------
		/// La charge utile « NKBP » : GRAF (peut etre vide), MODL (facultatif) et
		/// DOCU (2026-10-01, facultatif) : le DOCUMENT de l'editeur -- variables
		/// declarees, fonctions, macros, repartiteurs et leurs graphes. Ecrite
		/// EN DERNIER : un lecteur ancien lit GRAF et MODL puis s'arrete.
		/// ⚠️ Toutes les sections portent leur longueur : un lecteur saute
		///    desormais celle qu'il ne connait pas au lieu de s'arreter.
		void NkEcrireChargeBp(const NkString &graphe, const NkModuleBp *module, NkVector<uint8> &sortie,
							  const NkString *document = nullptr);
		/// `aModule` : la section MODL etait presente (et lue). false : charge
		/// illisible. `document` : la section DOCU (vide si absente).
		bool NkLireChargeBp(const uint8 *octets, usize taille, NkString *graphe, NkModuleBp *module, bool *aModule,
							NkString *erreur = nullptr, NkString *document = nullptr);
		bool NkEcrireFichierBp(const char *chemin, const NkString &graphe, const NkModuleBp *module,
							   NkString *erreur = nullptr, const NkString *document = nullptr);
		bool NkLireFichierBp(const char *chemin, NkString *graphe, NkModuleBp *module, bool *aModule,
							 NkString *erreur = nullptr, NkString *document = nullptr);

		/// Empreinte FNV-1a 64 d'un texte (celle de la section GRAF).
		uint64 NkEmpreinteBp(const char *texte, usize longueur) noexcept;

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYBPMODULE_H__
