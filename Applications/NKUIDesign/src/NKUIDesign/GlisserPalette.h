#pragma once
// ═══════════════════════════════════════════════════════════════════════════
//  LE GLISSER-DEPOSER DEPUIS LA PALETTE — la DECISION, pas le geste
// ═══════════════════════════════════════════════════════════════════════════
// Rodolf : « la palette a huit composants, il faut pouvoir les poser. »
//
// ⚠️ CE FICHIER NE PORTE PAS LE GESTE, ET C'EST UNE CORRECTION. J'avais ecrit
//    un `NkGlisser` maison : etat actif/inactif, position de depart, seuil de
//    franchissement a 4 px. **NKGui portait deja tout cela** —
//    `BeginDragSource` / `SetDragPayload` / `BeginDropTarget` /
//    `AcceptDragPayload`, avec le fantome dessine par la bibliotheque sous la
//    souris, le surlignage de la cible, et le meme seuil de ~4 px. Il existe
//    meme une `BeginDropTarget(ctx, id, rect)` a ZONE EXPLICITE, ecrite le
//    2026-08-17 pour NK3DModeler, faite exactement pour une zone qui contient
//    deja des widgets — c'est-a-dire pour la toile.
//    *Le seuil de 4 px que j'avais « choisi » etait le seuil de la
//    bibliotheque, redecouvert et reecrit.* C'est la deuxieme fois ce
//    week-end que la couche du dessous avait deja la reponse.
//
// 🔑 CE QUI RESTE ICI EST CE QUE NKGui NE PEUT PAS SAVOIR : dans quel noeud du
//    DOCUMENT tombe le composant, ou exactement apres aimantation, et ce qui
//    se dit quand rien ne peut recevoir. Deux questions, deux fonctions :
//      1. « ou tombe-t-il, et dans quel parent ? »   -> NkViserDepot
//      2. « pose-le »                                -> NkDeposerVise
//    Le depot NE REDECIDE RIEN : il ecrit ce que la visee a vu. Sinon
//    l'apercu montrerait un endroit et le depot en choisirait un autre.
//
// ⚠️ LE PARENT SE TROUVE PAR `NkPickFreeContainer`, LE MEME QUE LES OUTILS QUI
//    TRACENT (F, R/O, L, M, T). J'avais d'abord ecrit une remontee « par
//    famille » qui acceptait aussi les conteneurs agences — c'etait un SECOND
//    repondeur a une question que la maison tranche deja, et il aurait fait
//    tomber un composant lache la ou un cadre dessine au meme endroit
//    n'atterrit pas. *Deux reponses a une question ne se departagent pas a
//    l'usage : elles se contredisent devant l'utilisateur.* Le refus emploie
//    donc MOT POUR MOT la phrase des outils de trace, depuis une seule source.
//
// ⚠️ AUCUNE TAILLE N'EST INVENTEE. `NkUIDocument::InitNode` refuse deja de
//    poser « 200x150 parce que ca rend bien », avec sa raison ecrite : une
//    `NkComponentDecl` ne declare aucune taille souhaitee. Le noeud pose garde
//    donc `NkExpand()`, comme par le bouton « Poser ».
//
// ⚠️ ET L'AIMANTATION EST CELLE DE `Snap.h`, PAS UNE SECONDE. Le rectangle
//    vise est de taille NULLE : ses trois sondes se confondent au point, donc
//    le point s'aimante aux bords, aux centres et aux voisins comme n'importe
//    quel noeud deplace. Elle passe par `NkCalculerSnapDans`, la porte qui
//    accepte un noeud INEXISTANT — ce qu'est un composant qu'on glisse.

#include "Document.h"
#include "Selection.h"
#include "Snap.h"

#include <cstdio>

namespace nkuidesign {
	namespace glisser {

		using nkentseu::float32;
		using nkentseu::int32;

		/// LE TYPE DE LA CHARGE NKGui. Une constante, pas un litteral repete :
		/// la source et la cible doivent lire le MEME mot, sinon la livraison
		/// echoue en silence — le pire des echecs, puisqu'il ressemble a « on
		/// n'a pas assez glisse ».
		inline const char *NkTypeCharge() { return "nkuidesign.composant"; }

		/// LA PHRASE DU REFUS — la MEME que les outils de trace. Elle est une
		/// fonction pour qu'un banc puisse verifier que les deux portes la
		/// disent a l'identique.
		inline const char *NkRefusHorsConteneur() {
			return "Aucun conteneur libre sous le curseur — rien n'est créé.";
		}

		/// CE QUE LE GESTE VISE a cette image. Ecrit UNE fois, lu par le peintre
		/// (guides d'aimantation, cadre du parent) ET par le depot : l'apercu et
		/// le resultat ne peuvent pas diverger.
		struct NkVise {
				int32 parent = -1;
				float32 docX = 0.f, docY = 0.f; ///< le point APRES aimantation
				NkSnapResultat snap;
				bool possible = false;
				char raison[192] = {0};
		};

		/// LA DECISION, image par image.
		/// `screenLay` sert au survol (espace ECRAN) ; `layDoc` a l'aimantation
		/// (espace DOCUMENT) ; `docX/docY` est la souris deja convertie.
		inline NkVise NkViserDepot(const NkUIDocument &doc, const NkLayoutResult &layDoc,
								   const NkLayoutResult &screenLay, float32 sx, float32 sy,
								   float32 docX, float32 docY, bool aimant, float32 tolDoc) {
			NkVise v;
			v.docX = docX;
			v.docY = docY;
			v.parent = NkConteneurPourCreation(doc, screenLay, sx, sy); // hors page : la racine
			if (!doc.IsValidIndex(v.parent)) {
				// sous le point, une feuille ? alors c'est ELLE qui refuse, et on le dit
				const int32 sous = NkPickSelectable(doc, screenLay, sx, sy);
				snprintf(v.raison, sizeof(v.raison), "%s",
						 (doc.IsValidIndex(sous) && sous != 0) ? NkRefusFeuille() : NkRefusHorsConteneur());
				return v;
			}
			v.possible = true;
			if (aimant) {
				const NkPaintRect point = {docX, docY, 0.f, 0.f};
				v.snap = NkCalculerSnapDans(doc, layDoc, v.parent, -1, point, tolDoc);
				v.docX = docX + v.snap.dx;
				v.docY = docY + v.snap.dy;
			}
			const NkUINode &p = doc.nodes[(nkentseu::uint32)v.parent];
			snprintf(v.raison, sizeof(v.raison), "Dans « %s ».",
					 p.label.Empty() ? "(sans nom)" : p.label.Data());
			return v;
		}

		/// LE DEPOT. Rend l'indice du noeud cree, ou -1 ; `dire` recoit TOUJOURS
		/// le verdict, succes comme refus — jamais un geste sans effet muet.
		inline int32 NkDeposerVise(NkUIDocument &doc, const NkVise &v, const char *composant,
								   NkString &dire) {
			if (!v.possible || !doc.IsValidIndex(v.parent)) {
				dire = NkString(v.raison[0] ? v.raison : NkRefusHorsConteneur());
				return -1;
			}
			const int32 neuf =
				doc.AddChild(v.parent, composant ? composant : "", NkAuthor::Humain);
			if (neuf < 0) {
				// `AddChild` refuse un composant absent du registre plutot que
				// d'en inventer un. On le DIT, avec son nom.
				char b[192];
				snprintf(b, sizeof(b), "Dépôt refusé : « %s » n'est pas au registre.",
						 (composant && *composant) ? composant : "(sans nom)");
				dire = NkString(b);
				return -1;
			}
			NkUINode &n = doc.nodes[(nkentseu::uint32)neuf];
			n.posX = v.docX;
			n.posY = v.docY;
			doc.MarkHumanEdit(neuf);
			char b[256];
			const NkUINode &p = doc.nodes[(nkentseu::uint32)v.parent];
			snprintf(b, sizeof(b), "« %s » posé dans « %s ».", n.label.Data(),
					 p.label.Empty() ? "(sans nom)" : p.label.Data());
			dire = NkString(b);
			return neuf;
		}

	} // namespace glisser
} // namespace nkuidesign
