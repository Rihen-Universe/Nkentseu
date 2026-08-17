//
// NkPdfStruct.h — ORDRE DE LECTURE LOGIQUE, via /StructTreeRoot.
//
// LE PROBLEME QU'IL RESOUT. `NkPdfRenderer::TextItems()` rend le texte dans
// l'ordre VISUEL : celui ou les glyphes ont ete peints, regroupes par ligne.
// Sur un document a deux colonnes, cet ordre ENTRELACE les colonnes. Le
// resultat reste grammaticalement plausible — et c'est precisement ce qui le
// rend dangereux : les phrases sont fausses sans que rien ne le signale. Pour
// un systeme qui CITE ses sources, une citation credible et inexacte est le
// pire mode d'echec possible.
//
// L'arbre /StructTreeRoot porte l'ordre que l'auteur a voulu. Il relie ses
// noeuds au texte par les MCID (cf. le champ `mcid` de TextItem, rempli par
// les operateurs BDC/EMC).
//
// PERIMETRE DECIDE PAR MESURE (sondage de 258 PDF, 13/08/2026) :
//   /StructTreeRoot present dans 140 documents (55 %).
// Quand il est absent, l'appelant garde l'ordre visuel : c'est un REPLI
// SILENCIEUX et voulu, pas une erreur — 118 documents du corpus en dependent.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfRender.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// Un bloc de texte tel que la STRUCTURE le decoupe, et non tel que la
			// page le dessine.
			struct NkPdfBloc {
					// Type de structure : « P », « H1 ».. « H6 », « TD », « LI »,
					// « Figure », « Caption »... tel qu'ecrit dans /S. Chaine vide si
					// le noeud n'en declare pas.
					NkString type;
					int32 niveau = 0; // profondeur dans l'arbre (0 = racine)
					int32 page = -1;  // index de page, ou -1 si indetermine
					NkString texte;	  // UTF-8, assemble dans l'ordre de l'arbre
			};

			// Table (page, MCID) -> rang de lecture, construite UNE fois par
			// document.
			//
			// Pourquoi une table plutot qu'un parcours par page : l'arbre est global
			// au document. Le reparcourir pour chacune des 500 pages d'un livre
			// couterait 500 fois le meme travail ; on l'aplatit donc une seule fois
			// en rangs croissants.
			class NkPdfStructIndex {
				public:
					// Construit l'index. Rend false si le document ne declare pas de
					// /StructTreeRoot exploitable — l'appelant doit alors garder
					// l'ordre visuel.
					bool Construire(const NkPdfDoc &doc);

					bool Valide() const { return mEntrees.Size() > 0; }

					// Rang de lecture du bloc marque (page, mcid), ou -1 s'il est
					// inconnu de l'arbre. Un rang inconnu n'est PAS une erreur : du
					// texte hors structure existe (en-tetes, numeros de page), et il
					// faut pouvoir le replacer sans le perdre.
					int32 Rang(int32 page, int32 mcid) const;

					// Type de structure du bloc, ou chaine vide.
					NkString Type(int32 page, int32 mcid) const;

					nk_size Taille() const { return mEntrees.Size(); }

				private:
					struct Entree {
							int32 page = -1;
							int32 mcid = -1;
							int32 rang = 0;
							int32 niveau = 0;
							NkString type;
					};
					NkVector<Entree> mEntrees;

					// Table « numero d'objet de page -> index de page », construite
					// une fois : l'arbre designe ses pages par reference (/Pg), pas
					// par numero d'ordre.
					NkVector<int32> mPageObjets;

					int32 IndexDePage(const NkPdfDoc &doc, const NkPdfVal &pg) const;
					void Parcourir(const NkPdfDoc &doc, const NkPdfVal &noeud, const NkString &typeHerite,
								   int32 pageHeritee, int32 niveau, NkVector<int32> &vus, int32 &rang);
			};

			// Part du texte d'un document qui n'est rattachee a AUCUN noeud de
			// l'arbre, entre 0 et 1. Rend -1 si le document n'a pas de texte.
			//
			// A QUOI CA SERT, ET POURQUOI C'EST OBLIGATOIRE. Declarer un
			// /StructTreeRoot ne veut pas dire que le balisage est exploitable :
			// mesure du 13/08/2026 sur les 140 documents balises du corpus —
			//   < 1 % hors structure : 91 documents      10-25 % : 5
			//   1-5 %                : 19                25-50 % : 5
			//   5-10 %               : 8                 >= 50 % : 12 (dont trois a 99,5 %, 100 %, 100 %)
			// Sur un document ou la moitie du texte n'a pas de MCID, appliquer
			// l'ordre logique DISLOQUE le document au lieu de le redresser : le
			// texte non rattache part en fin de page, et il perd au passage le tri
			// par ligne que l'assemblage visuel lui appliquait.
			//
			// ⚠️ Aucun invariant de conservation ne verrait ce degat : le
			// multiensemble des caracteres est identique, rien n'est perdu, tout
			// est DEPLACE. D'ou cette mesure explicite.
			// Le document n'est PAS const : rendre une page charge des objets a la
			// demande (l'arene se remplit paresseusement). Le forcer par un
			// const_cast masquerait ce fait a l'appelant.
			double NkPdfPartHorsStructure(NkPdfDoc &doc, const NkPdfStructIndex &index);

			// Seuil au-dela duquel on GARDE l'ordre visuel. 10 % : sous ce seuil,
			// le texte non rattache est fait d'en-tetes et de folios (quelques mots
			// par page, dont le deplacement en fin de page ne coute rien) ; au-dela,
			// c'est du corps de texte, et le deplacer serait une dislocation. Le
			// chiffre suit la distribution ci-dessus : il laisse 118 documents sur
			// 140 en ordre logique (91 + 19 + 8) et met les 22 mal balises a
			// l'abri (5 + 5 + 12).
			//
			// ⚠️ NE PAS CONFONDRE LES DEUX POPULATIONS DE 118 : celle-ci est celle
			// des documents BALISES EXPLOITABLES ; l'autre, de meme taille par
			// coincidence, est celle des documents SANS /StructTreeRoot.
			static const double kNkPdfSeuilHorsStructure = 0.10;

			// Assemble les items d'UNE page dans l'ordre de la structure.
			//
			// Les items dont le MCID est inconnu de l'arbre sont conserves et places
			// A LA FIN, jamais jetes : perdre du texte serait pire que le mal qu'on
			// soigne.
			void NkPdfAssemblerParStructure(const NkPdfStructIndex &index, int32 page,
											const NkVector<NkPdfRenderer::TextItem> &items,
											NkVector<NkPdfBloc> &out);

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
