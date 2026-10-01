//
// NkBpDocument.h
// =============================================================================
// Description :
//   LE DOCUMENT d'un Blueprint, a la maniere d'Unreal 5 (demande de Rihen du
//   01/10 : « declaration de variables, creer les methodes et plus ») : ses
//   VARIABLES declarees (nom, type, valeur par defaut, modifiable par
//   instance, info-bulle, categorie), son GRAPHE D'EVENEMENTS, ses FONCTIONS
//   (entrees, sorties, pure), ses MACROS (inserees a la compilation) et ses
//   REPARTITEURS d'evenements -- chacun avec son graphe.
//
// Caracteristiques :
//   - Le fichier .nkbp garde sa forme (NkUnkenyBpModule.h) : GRAF = le graphe
//     d'evenements (un .nkbp d'avant le 01/10 se lit TEL QUEL : un document a
//     un seul graphe, sans declaration), MODL = le module compile, DOCU (en
//     dernier) = les declarations et les AUTRES graphes, en texte.
//   - Les NOEUDS qui designent une declaration la nomment par une PROPRIETE
//     (« var », « fonction », « macro », « rep ») : renommer une variable
//     renomme ses noeuds ; changer son type les REFAIT (prises rebranchees par
//     nom, ce qui ne colle plus est debranche -- comme « Rafraichir » d'UE5).
//   - Les noms (variables, fonctions, parametres) sont des CLES sans espace :
//     le .nkgraph lit les prises par jetons (document 01, § 14.2, choix 6).
//     Un espace saisi devient « _ ».
//   - Annuler / refaire : des INSTANTANES du document entier (le patron de
//     NkGraphHistory, etendu aux declarations).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKBPDOCUMENT_H__
#define __NKENTSEU_UNKENYEDITOR_NKBPDOCUMENT_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGraph/NkNodeGraph.h"
#include "Unkeny/Script/NkUnkenyBpModule.h"

namespace nkentseu {
	namespace editeur {

		/// Les cles des noeuds qui designent une declaration du document.
		constexpr const char *NK_BP_VAR_LIRE = "bp.var.get";
		constexpr const char *NK_BP_VAR_ECRIRE = "bp.var.set";
		constexpr const char *NK_BP_FN_ENTREE = "bp.fn.entree";
		constexpr const char *NK_BP_FN_RETOUR = "bp.fn.retour";
		constexpr const char *NK_BP_APPEL = "bp.appel";
		constexpr const char *NK_BP_MACRO = "bp.macro";
		constexpr const char *NK_BP_MACRO_ENTREE = "bp.macro.entree";
		constexpr const char *NK_BP_MACRO_SORTIE = "bp.macro.sortie";
		constexpr const char *NK_BP_REP_APPELER = "bp.rep.appeler";
		constexpr const char *NK_BP_REP_EVENEMENT = "bp.rep.evenement";
		constexpr const char *NK_BP_RELAIS = "bp.relais";
		constexpr const char *NK_BP_COMMENTAIRE = "bp.commentaire";

		/// Les types qu'on DECLARE (variables, parametres), dans l'ordre du menu.
		/// « asset » : une reference d'asset (son chemin, « Contenu/Sons/x.wav »),
		/// un texte pour la machine.
		const char *const *NkBpTypesDeclarables(uint32 &nombre) noexcept;
		/// « Booléen », « Entier »... (UTF-8).
		const char *NkBpLibelleType(const char *type) noexcept;

		/// Un parametre (d'une fonction, d'une macro, d'un repartiteur).
		/// `type` : « reel »... ; dans une MACRO, « exec » est permis (une
		/// entree ou une sortie d'execution de plus).
		struct NkParamBp {
				NkString nom;
				NkString type = "reel";
				bool tableau = false;
		};

		struct NkVariableDeclBp {
				NkString nom;
				NkString type = "booleen";
				bool tableau = false;
				/// La valeur par defaut, en TEXTE (le format de NkBpPoserDefaut :
				/// « vrai », « 3 », « 1.5 », « 0 2 », « #FF8800FF », « Porte »).
				NkString defaut;
				/// « Modifiable par instance » : montree (et modifiable) dans la
				/// carte « Scripts » des Details de chaque entite.
				bool instance = true;
				NkString infobulle;
				NkString categorie;
		};

		enum class NkGenreGrapheBp : uint8 { NK_EVENEMENTS = 0, NK_FONCTION, NK_MACRO };

		struct NkGrapheBp {
				NkGenreGrapheBp genre = NkGenreGrapheBp::NK_EVENEMENTS;
				NkString nom; ///< « Graphe d'événements », « OuvrirPorte »
				graph::NkNodeGraph graphe;
				NkVector<NkParamBp> entrees;
				NkVector<NkParamBp> sorties;
				bool pure = false;
				NkString categorie;
				NkString infobulle;
		};

		struct NkRepartiteurBp {
				NkString nom;
				NkVector<NkParamBp> params;
				NkString categorie;
				NkString infobulle;
		};

		struct NkDocumentBp {
				NkVector<NkVariableDeclBp> variables;
				/// [0] : le graphe d'evenements (toujours present).
				NkVector<NkGrapheBp> graphes;
				NkVector<NkRepartiteurBp> repartiteurs;
				/// « Reglages de classe ».
				NkString description;
				NkString categorie;

				NkDocumentBp();
				void Vider();

				NkGrapheBp &Evenements() {
					return graphes[0];
				}
				const NkGrapheBp &Evenements() const {
					return graphes[0];
				}
				int32 TrouverVariable(const char *nom) const noexcept;
				/// Un graphe de fonction ou de macro par son nom (-1 : aucun).
				int32 TrouverGraphe(const char *nom, NkGenreGrapheBp genre) const noexcept;
				int32 TrouverRepartiteur(const char *nom) const noexcept;
				/// Un nom LIBRE (« NouvelleVar », « NouvelleVar_2 »...) parmi
				/// variables, graphes et repartiteurs.
				NkString NomLibre(const char *base) const;
				bool NomPris(const char *nom) const noexcept;
		};

		/// Une cle de nom : sans espace (« _ »), sans caractere de controle,
		/// tronquee a `max` - 1 octets. Vide : refus.
		NkString NkBpCleDeNom(const char *texte, uint32 max = 64u);

		// --- Lire et ecrire ------------------------------------------------------
		/// Le texte DOCU (declarations + graphes autres que le [0]).
		void NkBpEcrireDocument(const NkDocumentBp &d, NkString &sortie);
		/// Relit DOCU par-dessus un document dont le graphe [0] est deja lu.
		bool NkBpLireDocument(const char *texte, NkDocumentBp &d, NkString *erreur = nullptr);
		/// Le document ENTIER en un texte (annuler / refaire, copier un Blueprint).
		void NkBpInstantane(const NkDocumentBp &d, NkString &sortie);
		bool NkBpDepuisInstantane(const char *texte, NkDocumentBp &d);

		/// Ouvre un .nkbp (d'avant ou d'apres le 01/10).
		bool NkBpOuvrirDocument(const char *chemin, NkDocumentBp &d, NkString *erreur = nullptr);

		// --- Les gestes sur le document -------------------------------------------
		/// Une variable neuve (nom libre), rend son indice.
		uint32 NkBpAjouterVariable(NkDocumentBp &d, const char *nom = nullptr, const char *type = "booleen");
		/// Une fonction neuve : son graphe porte « Entree » (et « Retour » s'il y
		/// a des sorties). Rend l'indice du graphe.
		uint32 NkBpAjouterFonction(NkDocumentBp &d, const char *nom = nullptr);
		uint32 NkBpAjouterMacro(NkDocumentBp &d, const char *nom = nullptr);
		uint32 NkBpAjouterRepartiteur(NkDocumentBp &d, const char *nom = nullptr);
		/// Renommer : les NOEUDS qui la designent suivent. false : nom pris ou vide.
		bool NkBpRenommerVariable(NkDocumentBp &d, uint32 v, const char *nom);
		bool NkBpRenommerGraphe(NkDocumentBp &d, uint32 g, const char *nom);
		bool NkBpRenommerRepartiteur(NkDocumentBp &d, uint32 r, const char *nom);
		/// Supprimer : ses noeuds partent aussi (des fils pendants ne resteraient
		/// pas « valides »).
		void NkBpSupprimerVariable(NkDocumentBp &d, uint32 v);
		void NkBpSupprimerGraphe(NkDocumentBp &d, uint32 g);
		void NkBpSupprimerRepartiteur(NkDocumentBp &d, uint32 r);
		/// Apres un changement de TYPE (variable) ou de SIGNATURE (fonction,
		/// macro, repartiteur) : refait les noeuds qui la designent (positions et
		/// fils gardes par nom de prise). Rend le nombre de noeuds refaits.
		uint32 NkBpRafraichirNoeuds(NkDocumentBp &d);

		// --- Les noeuds du document -------------------------------------------------
		/// Lire / Ecrire la variable `nom`, dans le graphe `g`, a (x, y).
		graph::NkNodeId NkBpCreerLireVariable(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y);
		graph::NkNodeId NkBpCreerEcrireVariable(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y);
		/// Appeler la fonction `nom` (pure : sans prises d'execution).
		graph::NkNodeId NkBpCreerAppel(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y);
		graph::NkNodeId NkBpCreerMacro(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y);
		graph::NkNodeId NkBpCreerAppelerRepartiteur(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x, float32 y);
		graph::NkNodeId NkBpCreerEvenementRepartiteur(NkDocumentBp &d, graph::NkNodeGraph &g, const char *nom, float32 x,
													  float32 y);
		/// Un noeud de REACHEMINEMENT (le point d'UE5) du type `type` (« exec »...).
		graph::NkNodeId NkBpCreerRelais(graph::NkNodeGraph &g, const char *type, float32 x, float32 y);
		/// Un COMMENTAIRE (cadre) de (x, y, w, h).
		graph::NkNodeId NkBpCreerCommentaire(graph::NkNodeGraph &g, const char *texte, float32 x, float32 y, float32 w, float32 h);
		/// N'importe quel noeud, par sa CLE : le catalogue fixe (NkBpProtos) ou
		/// une cle de document « bp.var.get:ouverte », « bp.appel:OuvrirPorte »...
		/// (celles que propose le menu contextuel).
		graph::NkNodeId NkBpCreerParCle(NkDocumentBp &d, graph::NkNodeGraph &g, const char *cle, float32 x, float32 y);

		// --- Les noeuds de CODE (NkBpExpression.h) ----------------------------------
		/// « Expression » (bp.expr), « Si (expression) » (bp.si.expr), « Code »
		/// (bp.code), avec leurs entrees de depart et leur code d'exemple.
		graph::NkNodeId NkBpCreerNoeudCode(graph::NkNodeGraph &g, const char *cle, float32 x, float32 y);
		/// Le code d'un noeud (sauts de ligne compris), et le poser.
		NkString NkBpCodeNoeud(const graph::NkNodeGraph &g, const graph::NkNode &n);
		void NkBpPoserCodeNoeud(graph::NkNodeGraph &g, graph::NkNodeId n, const char *code);
		/// Ajoute une ENTREE (`dir` Input) ou une SORTIE de donnee nommee a un
		/// noeud de code : le noeud est REFAIT (fils gardes) et `n` suit. Le nom
		/// devient une cle (sans espace) et un nom pris est refuse.
		bool NkBpCodeAjouterPrise(graph::NkNodeGraph &g, graph::NkNodeId &n, const char *nom, const char *type, graph::NkSocketDir dir);
		bool NkBpCodeRetirerPrise(graph::NkNodeGraph &g, graph::NkNodeId &n, const char *nom, graph::NkSocketDir dir);
		/// Change le type d'une prise de donnee d'un noeud de code (refait le noeud).
		bool NkBpCodeTypePrise(graph::NkNodeGraph &g, graph::NkNodeId &n, const char *nom, graph::NkSocketDir dir, const char *type);
		/// Un nom libre pour une entree (« a », « b »... ) ou une sortie.
		NkString NkBpCodeNomLibre(const graph::NkNode &n, graph::NkSocketDir dir);

		/// La propriete texte `nom` d'un noeud (« var », « fonction »...), ou vide.
		NkString NkBpPropTexte(const graph::NkNodeGraph &g, const graph::NkNode &n, const char *nom);
		void NkBpPoserPropTexte(graph::NkNodeGraph &g, graph::NkNodeId n, const char *nom, const char *valeur);
		float32 NkBpPropReel(const graph::NkNodeGraph &g, const graph::NkNode &n, const char *nom, float32 defaut);
		void NkBpPoserPropReel(graph::NkNodeGraph &g, graph::NkNodeId n, const char *nom, float32 valeur);

		/// La valeur par defaut d'une variable, LUE pour la machine. false :
		/// texte illisible pour ce type (l'erreur dit lequel).
		bool NkBpValeurDeTexte(const char *type, const char *texte, unkeny::NkValeurBp &sortie, NkString *texteSortie = nullptr);
		/// « #RRVVBBAA » <-> RVBA.
		bool NkBpLireCouleur(const char *texte, uint32 &rvba) noexcept;
		NkString NkBpTexteCouleur(uint32 rvba);

		// --- Annuler / refaire -----------------------------------------------------
		class NkBpHistorique {
			public:
				void Reset(const NkDocumentBp &d);
				void Commit(const NkDocumentBp &d);
				bool Undo(NkDocumentBp &d);
				bool Redo(NkDocumentBp &d);
				uint32 Profondeur() const noexcept {
					return mCurseur;
				}

			private:
				NkVector<NkString> mPile;
				uint32 mCurseur = 0u;
		};

		/// Le code d'un noeud dans la table de lignes du module : (graphe << 20) |
		/// identifiant. Le graphe d'evenements (0) garde l'identifiant nu : un
		/// module d'avant le 01/10 se lit pareil.
		constexpr uint32 NkBpCodeNoeud(uint32 graphe, graph::NkNodeId id) noexcept {
			return (graphe << 20) | (id & 0xFFFFFu);
		}
		constexpr uint32 NkBpGrapheDuCode(uint32 code) noexcept {
			return code >> 20;
		}
		constexpr graph::NkNodeId NkBpNoeudDuCode(uint32 code) noexcept {
			return code & 0xFFFFFu;
		}

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKBPDOCUMENT_H__
