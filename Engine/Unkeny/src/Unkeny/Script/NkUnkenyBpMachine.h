//
// NkUnkenyBpMachine.h
// =============================================================================
// Description :
//   La MACHINE VIRTUELLE des Blueprints (document 01, § 4.6) et ses NATIFS : les
//   fonctions de NkUnkHoteV1, decrites UNE fois (nom qualifie, types, evenements
//   permis, libelle) et lues A LA FOIS par le verificateur du moteur et par le
//   catalogue de noeuds de l'editeur.
//
// Caracteristiques :
//   - Interpreteur a registres types, sans fil, sans JIT, sans allocation par
//     evenement une fois le cadre a sa taille : le Web et iOS l'acceptent tel quel.
//   - BUDGET d'instructions par evenement (NK_BP_BUDGET) : une boucle sans fin
//     est COUPEE, l'instance passe en faute et la scene continue ; la faute dit
//     la fonction, le pc et le NOEUD du graphe (table de lignes).
//   - La VM n'a aucun acces privilegie au moteur : ses natifs passent par la
//     table de l'hote, comme un script C++. Le meme geste, le meme code.
//   - Elle ne depend d'aucun type d'Unkeny hors de cette table : elle pourra
//     descendre dans le Kernel quand Noge sera son second client (§ 4.8).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYBPMACHINE_H__
#define __NKENTSEU_UNKENY_NKUNKENYBPMACHINE_H__

#include "Unkeny/Script/NkUnkenyBpModule.h"
#include "Unkeny/Script/NkUnkenyScriptABI.h"

namespace nkentseu {
	namespace unkeny {

		/// Les evenements permis a un natif (bits NK_UNK_EV_*).
		constexpr uint32 NK_BP_PARTOUT = 0xFFFFFFFFu;
		constexpr uint32 NK_BP_PAS_FIXE_SEUL = 1u << NK_UNK_EV_PAS_FIXE;

		/// Ce qu'un natif recoit.
		struct NkAppelNatifBp {
				const NkUnkHoteV1 *hote = nullptr;
				NkUnkEntite soi{0u};
				const NkValeurBp *args = nullptr;
				NkValeurBp *res = nullptr;
				const NkModuleBp *module = nullptr;
				/// Les textes FABRIQUES pendant l'appel (concatenation, variable
				/// texte) ; nul : le cadre n'en fabrique pas (les resultats texte
				/// valent alors « »).
				NkVector<NkString> *textes = nullptr;
				/// Le texte de l'argument `i` (constante ou texte fabrique), jamais nul.
				const char *Texte(uint32 i) const noexcept;
				/// Fabrique un texte : rend la valeur `i` d'un registre texte qui le designe.
				int32 NouveauTexte(const char *texte) const;
				NkUnkEntite Entite(uint32 i) const noexcept {
					NkUnkEntite e;
					e.pack = args[i].e;
					return e;
				}
		};

		/// Un natif : sa signature, ce que l'editeur en montre, et son appel.
		struct NkNatifBp {
				NkSignatureNatifBp signature;
				const char *libelle = nullptr;	 ///< « Appliquer une impulsion » (UTF-8)
				const char *categorie = nullptr; ///< « Corps rigide »
				/// Pur : sans effet, sans prise d'execution (Position, Vitesse...).
				bool pur = false;
				const char *nomsParams[4] = {};
				const char *nomsResultats[2] = {};
				/// false = l'hote a REFUSE (entite morte, composant absent) : ce n'est
				/// pas une faute, le geste n'a simplement pas eu lieu.
				bool (*appel)(NkAppelNatifBp &a) = nullptr;
		};

		/// La table des natifs, dans un ordre STABLE (le module les designe par
		/// nom ; l'ordre ne sert qu'a l'editeur).
		const NkNatifBp *NkNatifsBp(uint32 &nombre) noexcept;
		/// Les signatures seules, contigues (pour NkVerifierModuleBp).
		const NkSignatureNatifBp *NkSignaturesBp(uint32 &nombre) noexcept;
		/// L'indice du natif `nom`, ou -1.
		int32 NkTrouverNatifBp(const char *nom) noexcept;

		/// Un programme : un module VERIFIE, et ses imports resolus.
		struct NkProgrammeBp {
				NkModuleBp module;
				NkVector<int32> natifs; ///< import -> indice dans NkNatifsBp
				bool pret = false;
		};
		/// Verifie le module et resout ses imports. false : `refus` dit pourquoi.
		bool NkPreparerProgrammeBp(NkProgrammeBp &p, NkRefusBp &refus);

		/// Le cadre d'un appel.
		struct NkCadreBp {
				const NkUnkHoteV1 *hote = nullptr;
				NkUnkEntite soi{0u};
				const NkUnkEvenementV1 *ev = nullptr;
				/// Les variables du module (module.variables.Size()), lues et ecrites.
				NkValeurBp *variables = nullptr;
				/// Le tampon des registres, REUTILISE d'un appel a l'autre. Les
				/// appels de fonctions y EMPILENT leurs cadres (au-dessus de celui de
				/// l'appelant).
				NkVector<NkValeurBp> *registres = nullptr;
				/// Les textes fabriques pendant l'appel (facultatif ; l'hote le vide
				/// entre deux evenements). Un registre texte < 0 les designe.
				NkVector<NkString> *textes = nullptr;
				/// Les parametres d'un evenement PERSONNALISE (repartiteur) : copies
				/// dans les premiers registres de la fonction appelee.
				const NkValeurBp *arguments = nullptr;
				uint32 nbArguments = 0u;
				uint32 budget = NK_BP_BUDGET;
				/// Rempli : instructions executees, natifs refuses par l'hote.
				uint32 instructions = 0u;
				uint32 refus = 0u;
				/// LA TRACE (l'editeur : fils qui ont servi, compteurs « x3 ») :
				/// appelee quand l'execution passe a un autre NOEUD du graphe (table
				/// de lignes). Nulle : aucun cout.
				void (*trace)(void *donnees, uint32 fonction, uint32 noeud) = nullptr;
				void *traceDonnees = nullptr;
				/// Un REPARTITEUR appele sur une AUTRE entite (NK_DIFFUSER) : l'hote
				/// livre l'evenement personnalise `nom` a ses scripts. Nul : refuse.
				bool (*diffuser)(void *donnees, NkUnkEntite cible, const char *nom, const NkValeurBp *args, uint32 n,
								 const NkTypeBp *types) = nullptr;
				void *diffuserDonnees = nullptr;
		};

		struct NkFauteBp {
				NkString raison;
				uint32 fonction = 0u;
				uint32 pc = 0u;
				uint32 noeud = 0u;
		};

		/// Execute la fonction `f`. false = FAUTE (budget epuise, division entiere
		/// par zero) : `faute` dit ou. Le programme doit etre pret.
		bool NkExecuterBp(const NkProgrammeBp &p, uint32 f, NkCadreBp &c, NkFauteBp &faute);

		/// Un ASSEMBLEUR minimal : de quoi ecrire un module a la main (les bancs,
		/// la contrainte du 24/08) ou depuis un graphe (le compilateur de
		/// l'editeur). Il n'invente rien : il range ce qu'on lui donne, et le
		/// verificateur juge.
		class NkAssembleurBp {
			public:
				NkModuleBp module;

				/// L'import du natif `nom` (signature copiee de la table), deja la
				/// s'il a servi. -1 : natif inconnu.
				int32 Import(const char *nom);
				uint32 ConstReel(float32 v);
				uint32 ConstEntier(int32 v);
				uint32 ConstBooleen(bool v);
				uint32 ConstVec2(float32 x, float32 y);
				uint32 ConstTexte(const char *texte);
				uint32 ConstCouleur(uint32 rvba);
				/// La variable `nom` (creee au premier appel).
				uint32 Variable(const char *nom, NkTypeBp type, const NkValeurBp &defaut, bool exposee = true);
				/// Une fonction neuve, qui devient la COURANTE. Rend son indice.
				uint32 Fonction(const char *nom);
				/// (format 2) La SIGNATURE de la fonction COURANTE : ses parametres
				/// puis ses resultats deviennent ses premiers registres. A appeler
				/// juste apres Fonction(), avant tout autre registre.
				void Signature(const NkTypeBp *params, uint32 np, const NkTypeBp *resultats, uint32 nr);
				/// Fait de `f` la fonction COURANTE (les fonctions sont declarees
				/// d'abord, pour que leurs appels se resolvent, puis remplies).
				void Courante(uint32 f) noexcept {
					mCourante = f;
				}
				uint32 Courante() const noexcept {
					return mCourante;
				}
				/// NK_APPEL : la fonction `f`, puis ses parametres et ses resultats.
				void Appel(uint32 f, const uint32 *registres, uint32 nombre);
				uint32 Registre(NkTypeBp type);
				uint32 Pc() const noexcept;
				void Emettre(NkOpBp op);
				void Emettre(NkOpBp op, uint32 a);
				void Emettre(NkOpBp op, uint32 a, uint32 b);
				void Emettre(NkOpBp op, uint32 a, uint32 b, uint32 c);
				/// NK_NATIF : l'import, puis ses parametres et ses resultats.
				void Natif(uint32 import, const uint32 *registres, uint32 nombre);
				/// Recrit le mot `pc` (la cible d'un saut connue apres coup).
				void Patcher(uint32 pc, uint32 valeur);
				/// pc courant -> noeud du graphe (le journal designera le noeud).
				void Ligne(uint32 noeud);
				void Entree(uint32 genre, uint32 fonction, const char *parametre = "");

			private:
				uint32 Constante(const NkConstanteBp &c);
				uint32 mCourante = 0u;
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYBPMACHINE_H__
