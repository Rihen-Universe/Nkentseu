//
// NkBpCatalogue.h
// =============================================================================
// Description :
//   La COUCHE 3 des Blueprints d'Unkeny sur NKGraph (document 01, § 2.2 et
//   § 7) : les types de prises, le CATALOGUE des noeuds (evenements, flot,
//   variables, calcul, et un noeud par NATIF de la machine), et le COMPILATEUR
//   graphe -> module de bytecode (NkUnkenyBpModule.h).
//
// Caracteristiques :
//   - Patron de NkMatGraph : types enregistres dans NKGraph par le
//     consommateur, conversions DIRIGEES (entier -> reel, jamais l'inverse),
//     prototypes de noeuds.
//   - Les noeuds de natifs sont FABRIQUES depuis la table de la machine
//     (NkNatifsBp) : un natif ajoute au moteur apparait au catalogue sans une
//     ligne ici, et le compilateur et le verificateur lisent la meme signature.
//   - Le compilateur MARCHE EN AVANT depuis chaque evenement le long des fils
//     d'execution (une sortie exec n'a qu'une suite), et evalue les noeuds PURS
//     a la demande, a chaque usage (comme UE5 : une Position lue apres un
//     Teleporter est la nouvelle). Il ecrit la table de lignes pc -> noeud : une
//     faute a l'execution designe le noeud.
//   - Une entree de donnees LIBRE prend sa valeur par defaut (saisie sur le
//     noeud) ; une entree « entite » libre vaut SOI.
//   - Toute erreur designe son NOEUD (la page du graphe l'entoure de rouge).
//   - Le compilateur vit dans l'EDITEUR : le jeu livre n'a ni NKGraph ni
//     compilateur (§ 4.7) ; il lit le module.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKBPCATALOGUE_H__
#define __NKENTSEU_UNKENYEDITOR_NKBPCATALOGUE_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGraph/NkNodeGraph.h"
#include "Script/NkBpDocument.h"
#include "Unkeny/Script/NkUnkenyBpMachine.h"
#include "Unkeny/Script/NkUnkenyScript.h"

namespace nkentseu {
	namespace editeur {

		/// Le GENRE d'un noeud : ce que le compilateur en fait. AJOUTES A LA FIN.
		/// Les genres « document » (variables declarees, fonctions, macros,
		/// repartiteurs, relais, commentaire) n'ont pas de prototype fixe : leurs
		/// prises viennent des declarations (NkBpDocument.h).
		enum class NkGenreNoeudBp : uint8 {
			NK_EVENEMENT = 0,
			NK_SI,
			NK_SEQUENCE,
			NK_LIRE_VAR, ///< l'ANCIEN noeud (nom saisi) : lu tel quel
			NK_ECRIRE_VAR,
			NK_NATIF,
			NK_MATH,
			NK_SOI,
			NK_POUR,	 ///< boucle « Pour » (premier, dernier -> corps, indice, fini)
			NK_TANT_QUE, ///< boucle « Tant que » (condition -> corps, fini)
			NK_VAR_GET,	 ///< variable DECLAREE (document)
			NK_VAR_SET,
			NK_FN_ENTREE,
			NK_FN_RETOUR,
			NK_APPEL_FN,
			NK_MACRO,
			NK_MACRO_ENTREE,
			NK_MACRO_SORTIE,
			NK_REP_APPELER,
			NK_REP_EVENEMENT,
			NK_RELAIS,
			NK_COMMENTAIRE,
			NK_INCONNU
		};
		/// Le genre de n'importe quel noeud (catalogue fixe ou document).
		NkGenreNoeudBp NkBpGenre(const graph::NkNode &n) noexcept;

		/// Une prise d'un prototype. `type` : « exec », « booleen », « entier »,
		/// « reel », « vec2 », « entite », « texte », « couleur », « asset ».
		struct NkPriseBp {
				const char *nom = nullptr;
				const char *type = nullptr;
		};

		struct NkProtoBp {
				NkString type;		///< la CLE du noeud dans le graphe (« bp.si »)
				NkString libelle;	///< « Si » (UTF-8)
				NkString categorie; ///< « Flot »
				NkGenreNoeudBp genre = NkGenreNoeudBp::NK_MATH;
				NkPriseBp entrees[6];
				uint8 nbEntrees = 0;
				NkPriseBp sorties[4];
				uint8 nbSorties = 0;
				uint32 evenement = 0u;					 ///< NK_UNK_EV_*
				int32 natif = -1;						 ///< indice dans NkNatifsBp
				unkeny::NkOpBp op = unkeny::NkOpBp::NK_FIN; ///< calcul
				unkeny::NkTypeBp typeVar = unkeny::NkTypeBp::NK_REEL;
				const char *aide = "";
		};

		/// Le catalogue, dans l'ordre de la palette.
		const NkVector<NkProtoBp> &NkBpProtos();
		const NkProtoBp *NkBpProto(const char *type);

		/// Les types des prises, et la conversion entier -> reel.
		void NkBpEnregistrerTypes(graph::NkNodeGraph &g);
		/// Un noeud du catalogue, ses prises posees, a (x, y). NK_NODE_INVALID :
		/// type inconnu.
		graph::NkNodeId NkBpCreerNoeud(graph::NkNodeGraph &g, const char *type, float32 x, float32 y);
		/// Pose la valeur par defaut d'une entree libre depuis un TEXTE (« 0 2 »
		/// pour un vec2, « Joueur » pour un texte, « vrai » / « 1 » pour un
		/// booleen). false : texte illisible pour ce type.
		bool NkBpPoserDefaut(graph::NkNodeGraph &g, graph::NkNodeId n, const char *prise, const char *texte);
		/// La valeur par defaut d'une entree, en texte (pour l'afficher).
		NkString NkBpTexteDefaut(const graph::NkNodeGraph &g, const graph::NkNode &n, const graph::NkSocket &s);

		unkeny::NkTypeBp NkBpTypeDeNom(const char *nomType) noexcept;

		/// Ce qui empeche de compiler, et OU : le noeud ET son graphe (l'indice
		/// dans NkDocumentBp::graphes ; 0 = le graphe d'evenements).
		struct NkErreurBp {
				NkString message;
				graph::NkNodeId noeud = graph::NK_NODE_INVALID;
				uint32 graphe = 0u;
		};

		/// Compile le graphe (un document d'un seul graphe). false : `erreur` dit
		/// pourquoi et designe le noeud. Le module rendu est VERIFIE (comme le jeu
		/// le verifiera).
		bool NkBpCompiler(const graph::NkNodeGraph &g, unkeny::NkModuleBp &sortie, NkErreurBp &erreur);
		/// Compile le DOCUMENT : variables declarees, fonctions (appelees par
		/// NK_APPEL), macros (inserees), repartiteurs, graphe d'evenements.
		bool NkBpCompilerDocument(const NkDocumentBp &d, unkeny::NkModuleBp &sortie, NkErreurBp &erreur);

		/// Compile puis ecrit le .nkbp (GRAF + MODL). La compilation echoue : le
		/// GRAPHE est ecrit quand meme (rien n'est perdu), sans module, et false.
		bool NkBpEnregistrer(const char *chemin, const graph::NkNodeGraph &g, NkErreurBp &erreur,
							 unkeny::NkModuleBp *module = nullptr);
		/// Le DOCUMENT : GRAF (graphe d'evenements) + MODL + DOCU.
		bool NkBpEnregistrerDocument(const char *chemin, const NkDocumentBp &d, NkErreurBp &erreur,
									 unkeny::NkModuleBp *module = nullptr);

		/// Une ENTREE du menu contextuel de la toile : ce qu'on peut poser dans le
		/// graphe `graphe` du document (catalogue fixe + variables, fonctions,
		/// macros, repartiteurs). `entrees` / `sorties` : les TYPES de ses prises
		/// (« exec » compris) -- le menu « sensible au contexte » ne garde que
		/// celles qui acceptent la prise tiree.
		struct NkEntreeMenuBp {
				NkString cle;		///< pour NkBpCreerParCle
				NkString libelle;	///< « Lire ouverte »
				NkString categorie; ///< « Variables »
				NkString aide;
				NkVector<NkString> entrees;
				NkVector<NkString> sorties;
		};
		void NkBpEntreesMenu(const NkDocumentBp &d, uint32 graphe, NkVector<NkEntreeMenuBp> &sortie);
		/// Relit le graphe d'un .nkbp (section GRAF).
		bool NkBpOuvrir(const char *chemin, graph::NkNodeGraph &g, NkString *erreur = nullptr);

		/// Le graphe de « la porte » (document 01) : Zone entree -> Si (soi est la
		/// zone ET l'autre s'appelle « Joueur » ET la variable « ouverte » est
		/// fausse) -> Teleporter l'entite `porte` de (0, 2), Jouer son effet,
		/// Afficher, ouverte = vrai. Construit PAR CODE (banc, exemple).
		void NkBpGraphePorte(graph::NkNodeGraph &g, const char *porte = "Porte");
		/// (2026-10-01) La MEME porte, reecrite avec de VRAIES declarations : les
		/// variables « ouverte » (booleen), « porte » (texte, modifiable par
		/// instance), « hauteur » (reel, modifiable par instance) et la fonction
		/// « OuvrirPorte(hauteur) » appelee par le graphe d'evenements.
		void NkBpDocumentPorte(NkDocumentBp &d, const char *porte = "Porte");
		/// « Debut -> Afficher "Bonjour" » : le Blueprint neuf.
		void NkBpGrapheBonjour(graph::NkNodeGraph &g);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKBPCATALOGUE_H__
