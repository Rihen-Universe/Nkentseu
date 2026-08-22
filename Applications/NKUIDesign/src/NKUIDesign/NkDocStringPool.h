// =============================================================================
// NKUIDesign/NkDocStringPool.h
// LE PROPRIETAIRE DES NOMS DE METRIQUE.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// `NkSizeDecl` et `NkLayoutDecl` (types du kit, NkComponentLayout.h) portent
// leurs noms de metrique en `const char *` :
//
//     const char *valueMetric   = "";
//     const char *spacingMetric = "";
//     const char *padMetric     = "";
//     const char *gridCellMetric = "";
//
// C'est juste POUR LE KIT : ce sont des types de COMPILATION, leurs chaines sont
// des litteraux, et le type n'a rien a posseder. Ca cesse d'etre juste des qu'un
// document est RELU D'UN FICHIER : la chaine vient alors du disque, et plus
// personne ne la possede.
//
// Mesure du 2026-08-22 (banc C5 de SandboxNKSerialization) : sans proprietaire,
// `NkReflectSerializer` classait ces champs en NK_POINTER, les omettait, et --
// avant correction -- rendait `true` quand meme. Une taille qui designait la
// metrique « largeur_palette » se relisait sans metrique et se resolvait au
// NOMBRE, sans le moindre signal.
//
// Ce pool est le proprietaire manquant.
//
// LES TROIS DECISIONS, ET ELLES SONT DES CONTRATS
// -----------------------------------------------
//
// 1. DUREE DE VIE — une chaine du pool vit EXACTEMENT aussi longtemps que le
//    document qui la porte. Ni plus, ni moins. Un `const char *` obtenu de
//    `Intern()` est valide tant que le document existe, et devient pendouillant
//    a sa destruction. Ne jamais faire survivre un tel pointeur a son document
//    -- ni dans un cache, ni dans une capture, ni dans un rapport.
//
// 2. IDENTITE — **PAS DE DEDUPLICATION. C'est un choix, pas un oubli.**
//    Deux noeuds qui emploient tous deux « largeur_palette » recoivent DEUX
//    adresses DIFFERENTES.
//
//    La deduplication etait tentante : elle economise de la memoire et elle
//    ferait marcher `a.valueMetric == b.valueMetric` comme test d'egalite de
//    nom. C'est precisement pour ca qu'elle est refusee. Cette egalite ne
//    serait vraie qu'A L'INTERIEUR d'un meme document : deux documents, une
//    copie, un import, et elle redevient fausse. Quelqu'un l'essaierait, ca
//    marcherait dans son test, et ca casserait en silence bien plus tard.
//
//    Sans deduplication, l'egalite d'adresse est SYSTEMATIQUEMENT fausse entre
//    deux champs distincts : le premier essai echoue, tout de suite, chez celui
//    qui l'a ecrit. **On prefere l'echec visible et immediat a l'echec
//    silencieux et differe.**
//
//    ⚠️ **Comparez les noms de metrique PAR CONTENU, jamais par adresse.** Tout
//    le depot le fait deja : verifie le 2026-08-22, les resolutions passent
//    toutes par `StrEq` (`NkComponentDecl::FindMetric`, `NkUIDocument::Metric`),
//    et les seuls tests sur le pointeur lui-meme sont `name && *name`. **Zero
//    comparaison d'adresse** sur ces champs dans tout le depot -- c'est ce qui
//    rend ce choix sans risque aujourd'hui, et la regle ci-dessus le maintient.
//
//    Si la memoire devenait un probleme MESURE, la deduplication redeviendrait
//    possible -- mais seulement le jour ou un controle interdirait les
//    comparaisons d'adresse. Pas avant.
//
// 3. STABILITE DES ADRESSES — le piege qui tue ce genre de pool.
//    ⚠️ Un `NkVector<NkString>` RELOGE ses elements quand il grandit. Les
//    `const char *` deja distribues deviendraient alors pendouillants, et le
//    document se corromprait a la Nieme metrique, pas a la premiere. C'est
//    exactement ce que `Document.h` documente pour les noeuds (« un NkVector
//    reloge ses elements : garder un NkUINode* a travers un AddChild serait un
//    pointeur pendouillant qui ne se manifesterait qu'a la 17e pose »).
//
//    D'ou `NkVector<NkString *>` : le VECTEUR de pointeurs peut se reloger tant
//    qu'il veut, les NkString qu'il designe, elles, ne bougent jamais. Une
//    NkString posee n'est plus jamais modifiee -- sinon son SSO pourrait
//    basculer vers le tas et deplacer le contenu.
//
// Zero-STL. Auteur : Rihen. License : Proprietary - All Rights Reserved.
// =============================================================================

#pragma once

#ifndef NKUIDESIGN_NKDOCSTRINGPOOL_H
#define NKUIDESIGN_NKDOCSTRINGPOOL_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkuidesign {

		class NkDocStringPool {
			public:
				NkDocStringPool() = default;

				// Non copiable : deux pools qui partageraient leurs NkString*
				// libereraient deux fois. Un document se copie par ses VALEURS,
				// et le pool du destinataire re-interne -- voir CopyFrom.
				NkDocStringPool(const NkDocStringPool &) = delete;
				NkDocStringPool &operator=(const NkDocStringPool &) = delete;

				NkDocStringPool(NkDocStringPool &&o) noexcept : mItems(traits::NkMove(o.mItems)) {
				}

				NkDocStringPool &operator=(NkDocStringPool &&o) noexcept {
					if (this != &o) {
						Clear();
						mItems = traits::NkMove(o.mItems);
					}
					return *this;
				}

				~NkDocStringPool() {
					Clear();
				}

				/// @brief Prend possession d'un nom et rend un pointeur stable
				/// @param s Nom a copier (peut venir du disque, d'une NkString temporaire...)
				/// @return Pointeur valide TANT QUE LE POOL VIT ; jamais nullptr
				/// @note Une chaine vide rend le litteral "" et n'alloue RIEN : c'est
				///       le defaut du kit, et il ne doit pas couter une entree.
				/// @note PAS de deduplication (voir l'en-tete) : deux appels avec le
				///       meme texte rendent deux adresses DIFFERENTES.
				const char *Intern(const char *s) {
					if (!s || !*s) {
						return ""; // litteral statique : toujours valide
					}
					NkString *held = new NkString(s);
					mItems.PushBack(held);
					// L'adresse du CONTENU, pas celle de la NkString : c'est elle
					// qui doit rester stable, et elle le reste parce que la
					// NkString ne sera plus jamais modifiee.
					return held->CStr();
				}

				const char *Intern(const NkString &s) {
					return Intern(s.CStr());
				}

				/// @brief Nombre d'entrees possedees (la chaine vide n'en cree aucune)
				nk_size Size() const {
					return mItems.Size();
				}

				/// @brief Libere tout. Tous les pointeurs distribues deviennent invalides.
				void Clear() {
					for (nk_size i = 0; i < mItems.Size(); ++i) {
						delete mItems[i];
					}
					mItems.Clear();
				}

			private:
				// Vecteur de POINTEURS : il peut se reloger, les NkString non.
				NkVector<NkString *> mItems;
		};

} // namespace nkuidesign

#endif // NKUIDESIGN_NKDOCSTRINGPOOL_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
