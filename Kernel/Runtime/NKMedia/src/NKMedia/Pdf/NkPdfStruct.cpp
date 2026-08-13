//
// NkPdfStruct.cpp — parcours de /StructTreeRoot et ordre de lecture logique.
//
#include "NKMedia/Pdf/NkPdfStruct.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			int32 NkPdfStructIndex::IndexDePage(const NkPdfDoc &doc, const NkPdfVal &pg) const {
				if (!pg.IsDictLike())
					return -1;
				// L'identite d'un objet est (kind, a) : `a` indexe sa premiere entree
				// dans l'arene et ne change pas.
				for (nk_size i = 0; i < mPageObjets.Size(); ++i)
					if (mPageObjets[i] == pg.a)
						return static_cast<int32>(i);
				(void)doc;
				return -1;
			}

			void NkPdfStructIndex::Parcourir(const NkPdfDoc &doc, const NkPdfVal &noeud,
											 const NkString &typeHerite, int32 pageHeritee,
											 int32 niveau, NkVector<int32> &vus, int32 &rang) {
				// Deux gardes, comme partout dans ce module : profondeur bornee ET
				// liste d'objets visites. La profondeur seule laisserait boucler un
				// arbre dont deux noeuds se pointent l'un l'autre en largeur.
				if (niveau > 64 || vus.Size() > 200000u)
					return;

				// ── Feuille : un ENTIER NU est un MCID du flux de la page ──
				if (noeud.kind == NK_PDF_INT) {
					Entree e;
					e.page = pageHeritee;
					e.mcid = static_cast<int32>(noeud.num);
					e.rang = rang++;
					e.niveau = niveau;
					e.type = typeHerite;
					mEntrees.PushBack(e);
					return;
				}

				// ── Tableau : chaque element est un enfant, dans l'ordre ──
				if (noeud.kind == NK_PDF_ARRAY) {
					const int32 n = doc.ArraySize(noeud);
					for (int32 i = 0; i < n; ++i)
						Parcourir(doc, doc.ArrayAt(noeud, i), typeHerite, pageHeritee, niveau, vus, rang);
					return;
				}

				if (!noeud.IsDictLike())
					return;

				for (nk_size i = 0; i < vus.Size(); ++i)
					if (vus[i] == noeud.a)
						return; // deja visite : un arbre malforme peut boucler
				vus.PushBack(noeud.a);

				// ── Reference de contenu marque : /MCR { /Pg, /MCID } ──
				// (/OBJR designe un objet entier, pas du texte : on l'ignore.)
				if (doc.NameIs(doc.DictGet(noeud, "Type"), "MCR")) {
					const NkPdfVal pg = doc.DictGet(noeud, "Pg");
					const int32 p = pg.IsDictLike() ? IndexDePage(doc, pg) : pageHeritee;
					const NkPdfVal mc = doc.DictGet(noeud, "MCID");
					if (mc.IsNum()) {
						Entree e;
						e.page = p;
						e.mcid = static_cast<int32>(mc.num);
						e.rang = rang++;
						e.niveau = niveau;
						e.type = typeHerite;
						mEntrees.PushBack(e);
					}
					return;
				}
				if (doc.NameIs(doc.DictGet(noeud, "Type"), "OBJR"))
					return;

				// ── Element de structure ──
				// /S porte son type (P, H1, TD...), /Pg la page par defaut de ses
				// descendants, /K ses enfants.
				NkString type = typeHerite;
				const NkPdfVal s = doc.DictGet(noeud, "S");
				if (s.kind == NK_PDF_NAME) {
					int32 len = 0;
					const char *p = doc.Text(s, &len);
					type = NkString();
					for (int32 i = 0; i < len; ++i)
						type += p[i];
				}

				int32 page = pageHeritee;
				const NkPdfVal pg = doc.DictGet(noeud, "Pg");
				if (pg.IsDictLike()) {
					const int32 p = IndexDePage(doc, pg);
					if (p >= 0)
						page = p;
				}

				const NkPdfVal k = doc.DictGet(noeud, "K");
				if (!k.IsNull())
					Parcourir(doc, k, type, page, niveau + 1, vus, rang);
			}

			bool NkPdfStructIndex::Construire(const NkPdfDoc &doc) {
				mEntrees.Clear();
				mPageObjets.Clear();

				const NkPdfVal racine = doc.DictGet(doc.Catalog(), "StructTreeRoot");
				if (!racine.IsDictLike())
					return false;

				// Table inverse « objet de page -> index », construite UNE fois :
				// l'arbre designe ses pages par reference, jamais par numero.
				const int32 nb = doc.PageCount();
				mPageObjets.Reserve(static_cast<nk_size>(nb));
				for (int32 i = 0; i < nb; ++i)
					mPageObjets.PushBack(doc.Page(i).a);

				const NkPdfVal k = doc.DictGet(racine, "K");
				if (k.IsNull())
					return false;

				NkVector<int32> vus;
				int32 rang = 0;
				Parcourir(doc, k, NkString(), -1, 0, vus, rang);
				return mEntrees.Size() > 0;
			}

			int32 NkPdfStructIndex::Rang(int32 page, int32 mcid) const {
				if (mcid < 0)
					return -1;
				for (nk_size i = 0; i < mEntrees.Size(); ++i)
					if (mEntrees[i].mcid == mcid && (mEntrees[i].page == page || mEntrees[i].page < 0))
						return mEntrees[i].rang;
				return -1;
			}

			NkString NkPdfStructIndex::Type(int32 page, int32 mcid) const {
				if (mcid < 0)
					return NkString();
				for (nk_size i = 0; i < mEntrees.Size(); ++i)
					if (mEntrees[i].mcid == mcid && (mEntrees[i].page == page || mEntrees[i].page < 0))
						return mEntrees[i].type;
				return NkString();
			}

			void NkPdfAssemblerParStructure(const NkPdfStructIndex &index, int32 page,
											const NkVector<NkPdfRenderer::TextItem> &items,
											NkVector<NkPdfBloc> &out) {
				if (items.Size() == 0)
					return;

				// Les blocs de CETTE page sont construits dans un tableau LOCAL, puis
				// tries, puis verses dans `out`. Les batir directement dans `out` et
				// les y indexer par decalage serait fragile : le vecteur grandit
				// pendant la boucle, et le moindre changement d'ordre casserait le
				// calcul d'indice en silence.
				NkVector<NkPdfBloc> blocs;
				NkVector<int32> mcids;
				NkVector<int32> rangs;

				for (nk_size i = 0; i < items.Size(); ++i) {
					const int32 mcid = items[i].mcid;
					nk_size pos = mcids.Size();
					for (nk_size k = 0; k < mcids.Size(); ++k)
						if (mcids[k] == mcid) {
							pos = k;
							break;
						}
					if (pos == mcids.Size()) {
						// Un bloc par MCID, cree a sa PREMIERE apparition. Les items
						// d'un meme MCID gardent entre eux leur ordre de dessin, qui
						// est le bon a l'interieur d'un paragraphe.
						mcids.PushBack(mcid);
						// Le texte hors structure (en-tetes, folios) recoit un rang
						// tres grand : il est REPOUSSE a la fin, jamais jete. Perdre
						// du texte serait pire que le desordre qu'on corrige.
						const int32 r = index.Rang(page, mcid);
						rangs.PushBack(r >= 0 ? r : 1000000000);
						NkPdfBloc b;
						b.page = page;
						b.type = index.Type(page, mcid);
						blocs.PushBack(b);
					}
					blocs[pos].texte.Append(items[i].text);
				}

				// Tri par insertion : quelques dizaines de blocs par page, et il est
				// STABLE — deux blocs de meme rang gardent leur ordre d'apparition,
				// ce qui laisse le texte hors structure dans son ordre visuel.
				for (nk_size i = 1; i < blocs.Size(); ++i) {
					const int32 rCle = rangs[i];
					NkPdfBloc cle = blocs[i];
					nk_size j = i;
					while (j > 0 && rangs[j - 1] > rCle) {
						rangs[j] = rangs[j - 1];
						blocs[j] = blocs[j - 1];
						--j;
					}
					rangs[j] = rCle;
					blocs[j] = cle;
				}

				for (nk_size i = 0; i < blocs.Size(); ++i)
					if (!blocs[i].texte.Empty())
						out.PushBack(blocs[i]);
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
