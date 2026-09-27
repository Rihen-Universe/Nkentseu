#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiEdition.h
// @Brief   Les six ecritures LOCALISEES sur une archive `.nkgui`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  POURQUOI CES OPERATIONS EXISTENT, ET POURQUOI ELLES NE PASSENT PAS PAR LE MODELE
// =============================================================================
//  Document 5 §2.2, item 2 : « Operations d'ecriture LOCALISEES sur l'archive :
//  `PoserPropriete`, `RetirerPropriete`, `InsererNoeud`, `RetirerNoeud`,
//  `DeplacerNoeud`, `RenommerIdentifiant`. »
//
//  L'invariant 2 du §1.1 dit ce qu'elles servent a tenir : **une modification ne
//  touche que ses lignes**. Et le temoin de R1 le chiffre : ouvrir un document,
//  changer UNE propriete a la souris, enregistrer, `git diff` montre UNE LIGNE.
//
//  ⚠️ REGENERER LE FICHIER DEPUIS LE MODELE NE PEUT PAS TENIR CET INVARIANT, et
//     ce n'est pas une question de soin. Le modele (`NkUIDocument`) ne porte que
//     le SENS : il ignore les commentaires, les lignes vides, l'alignement, la
//     forme `{ }` contre `{`/`}`. Mesure du 27/09 : sur les documents de la
//     famille, le modele ne sait pas tenir **175 attributs** et **20 sections**.
//     Un enregistrement par `NkDocumentVersTexte` reecrirait donc TOUT, effacerait
//     les commentaires -- et « les documents de la famille sont tres commentes ».
//
//     L'archive, elle, possede l'octet : chaque noeud porte son litteral ET sa
//     trivia. Toucher un seul litteral et reemettre rend le fichier a l'identique
//     sauf cette ligne. C'est la couche qui doit recevoir les modifications ; le
//     modele est la VUE qu'on manipule a l'ecran.
//
// =============================================================================
//  L'ADRESSE EST UN CHEMIN D'INDICES, PAS UN POINTEUR
// =============================================================================
//  Les six fonctions prennent le meme `NkVector<uint32>` que `NkUINode::refArchive`.
//  Un pointeur vers le bloc serait plus court a ecrire et faux : `AddBlock` reloge
//  le vecteur des blocs, donc le prochain ajout dans le meme `$body` rendrait
//  pendouillant tout pointeur garde. Le chemin, lui, survit -- il se resout a
//  chaque appel.
//
//  ⚠️ ET UNE INSERTION OU UN RETRAIT PERIME LES CHEMINS DES FRERES SUIVANTS.
//     `InsererNoeud(parent, 2, ...)` fait que l'ancien indice 2 devient 3. Aucune
//     de ces fonctions ne met a jour les chemins deja distribues : c'est
//     l'appelant qui relit le document apres une operation de STRUCTURE. Les trois
//     operations qui ne bougent aucun indice (`PoserPropriete`, `RetirerPropriete`,
//     `RenommerIdentifiant`) n'ont pas ce probleme -- ce sont d'ailleurs les seules
//     dont le temoin « une ligne » a un sens.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_NKGUIEDITION_H__
#define __NKENTSEU_NKUIDESIGN_NKGUIEDITION_H__

#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkuidesign {
	namespace guifmt {

		using nkentseu::NkArchive;
		using nkentseu::NkArchiveNode;
		using nkentseu::NkGuiArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::NkVector;
		using nkentseu::uint32;

		/// Le bloc designe par un chemin d'indices, MUTABLE. `nullptr` si le chemin
		/// ne mene nulle part -- ce qui arrive des qu'une insertion ou un retrait
		/// l'a perime (voir l'en-tete).
		inline NkArchive *NkBlocAuChemin(NkArchive &racine,
										 const NkVector<uint32> &chemin) noexcept {
			NkArchive *cur = &racine;
			for (uint32 i = 0; i < (uint32)chemin.Size(); ++i) {
				NkArchiveNode *corps = cur->FindNode(NkStringView(NkGuiArchive::KeyBody()));
				if (!corps || chemin[i] >= (uint32)corps->array.Size())
					return nullptr;
				NkArchiveNode &e = corps->array[chemin[i]];
				if (!e.IsObject() || !e.object)
					return nullptr;
				cur = e.object;
			}
			return cur;
		}

		// =====================================================================
		//  1. POSER UNE PROPRIETE
		// =====================================================================
		/// Pose `cle = valeur` sur le bloc, en CHAINE (entre guillemets).
		///
		/// ⚠️ SUR UNE CLE QUI EXISTE DEJA, C'EST UN REMPLACEMENT DE LITTERAL, et
		///    c'est tout l'interet : la trivia du noeud (le commentaire qui suit sur
		///    la meme ligne, les lignes qui le precedent) n'est pas touchee. Sur une
		///    cle neuve, le noeud s'ajoute a la fin des scalaires du bloc.
		inline bool NkPoserPropriete(NkArchive &racine, const NkVector<uint32> &chemin,
									 const char *cle, const char *valeur) noexcept {
			NkArchive *b = NkBlocAuChemin(racine, chemin);
			if (!b || !cle || !*cle)
				return false;
			return b->SetString(NkStringView(cle), NkStringView(valeur ? valeur : ""))
				   != 0;
		}

		/// Pose `cle = jeton` sans guillemets (`Center`, `(220, 32)`, `true`, `12`).
		/// Le format distingue les deux : `align = Center` n'est pas `align = "Center"`.
		inline bool NkPoserProprieteJeton(NkArchive &racine, const NkVector<uint32> &chemin,
										  const char *cle, const char *jeton) noexcept {
			NkArchive *b = NkBlocAuChemin(racine, chemin);
			if (!b || !cle || !*cle)
				return false;
			return NkGuiArchive::SetToken(*b, NkStringView(cle),
										  NkStringView(jeton ? jeton : ""));
		}

		// =====================================================================
		//  2. RETIRER UNE PROPRIETE
		// =====================================================================
		/// ⚠️ ELLE REFUSE LES CLES RESERVEES. `$type`, `$id`, `$body` portent la
		///    STRUCTURE du bloc : les retirer ne ferait pas un bloc sans propriete,
		///    ca ferait un bloc sans type -- c'est-a-dire un document que le lecteur
		///    refuserait, pour une operation qui avait l'air anodine.
		inline bool NkRetirerPropriete(NkArchive &racine, const NkVector<uint32> &chemin,
									   const char *cle) noexcept {
			if (!cle || !*cle || NkGuiArchive::IsReservedKey(NkStringView(cle)))
				return false;
			NkArchive *b = NkBlocAuChemin(racine, chemin);
			if (!b)
				return false;
			return b->Remove(NkStringView(cle)) != 0;
		}

		// =====================================================================
		//  3. RENOMMER L'IDENTIFIANT
		// =====================================================================
		/// ⚠️ UN IDENTIFIANT N'EST PAS UNE ETIQUETTE. C'est la cle d'etat du monteur
		///    ET le nom de l'action que l'hote branche. Le changer debranche le
		///    widget de son action, en silence, jusqu'a ce que quelqu'un clique.
		///    La fonction fait donc ce qu'on lui demande, mais l'appelant qui la
		///    propose a l'utilisateur doit l'avertir -- c'est un renommage, pas une
		///    retouche.
		inline bool NkRenommerIdentifiant(NkArchive &racine, const NkVector<uint32> &chemin,
										  const char *nouvel) noexcept {
			NkArchive *b = NkBlocAuChemin(racine, chemin);
			if (!b || !nouvel)
				return false;
			return b->SetString(NkStringView(NkGuiArchive::KeyId()), NkStringView(nouvel))
				   != 0;
		}

		// =====================================================================
		//  4-6. LES OPERATIONS DE STRUCTURE
		// =====================================================================
		//  Elles bougent des indices, donc elles PERIMENT les chemins des freres
		//  suivants (en-tete). Chacune rend `false` plutot que d'agir a cote quand
		//  l'indice demande sort des bornes : une insertion « a peu pres la » est
		//  pire qu'une insertion refusee, parce qu'elle ne se plaint pas.

		/// Insere `bloc` comme enfant de `cheminParent`, a `indice`. `indice` egal au
		/// nombre d'enfants = ajouter a la fin.
		inline bool NkInsererNoeud(NkArchive &racine, const NkVector<uint32> &cheminParent,
								   uint32 indice, const NkArchive &bloc) noexcept {
			NkArchive *p = NkBlocAuChemin(racine, cheminParent);
			if (!p)
				return false;
			NkArchiveNode &corps = NkGuiArchive::EnsureBody(*p);
			if (indice > (uint32)corps.array.Size())
				return false;
			NkArchiveNode n;
			n.object = new NkArchive(bloc);
			corps.array.Insert(corps.array.Begin() + (nkentseu::usize)indice, n);
			return true;
		}

		/// Retire le noeud designe. Le chemin doit avoir au moins un element : la
		/// racine du document n'est l'enfant de personne.
		inline bool NkRetirerNoeud(NkArchive &racine, const NkVector<uint32> &chemin) noexcept {
			if (chemin.Empty())
				return false;
			NkVector<uint32> parent = chemin;
			const uint32 dernier = parent[(uint32)parent.Size() - 1u];
			parent.PopBack();
			NkArchive *p = NkBlocAuChemin(racine, parent);
			if (!p)
				return false;
			NkArchiveNode *corps = p->FindNode(NkStringView(NkGuiArchive::KeyBody()));
			if (!corps || dernier >= (uint32)corps->array.Size())
				return false;
			corps->array.Erase(corps->array.Begin() + (nkentseu::usize)dernier);
			return true;
		}

		/// Deplace un noeud a l'interieur de SA fratrie.
		///
		/// ⚠️ `nouvelIndice` S'ENTEND APRES LE RETRAIT, et c'est la faute classique :
		///    deplacer l'element 1 vers 3 dans une fratrie de 5 ne le met pas ou on
		///    l'attend si on compte avant retrait. Ici la sequence est explicite --
		///    on retire, puis on insere dans la liste devenue plus courte.
		inline bool NkDeplacerNoeud(NkArchive &racine, const NkVector<uint32> &chemin,
									uint32 nouvelIndice) noexcept {
			if (chemin.Empty())
				return false;
			NkVector<uint32> parent = chemin;
			const uint32 dernier = parent[(uint32)parent.Size() - 1u];
			parent.PopBack();
			NkArchive *p = NkBlocAuChemin(racine, parent);
			if (!p)
				return false;
			NkArchiveNode *corps = p->FindNode(NkStringView(NkGuiArchive::KeyBody()));
			if (!corps || dernier >= (uint32)corps->array.Size())
				return false;
			const uint32 n = (uint32)corps->array.Size();
			if (nouvelIndice >= n)
				return false;
			NkArchiveNode garde = corps->array[dernier];
			corps->array.Erase(corps->array.Begin() + (nkentseu::usize)dernier);
			corps->array.Insert(corps->array.Begin() + (nkentseu::usize)nouvelIndice, garde);
			return true;
		}

	} // namespace guifmt
} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_NKGUIEDITION_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
