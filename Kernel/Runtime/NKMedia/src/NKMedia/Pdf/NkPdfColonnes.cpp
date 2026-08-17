//
// NkPdfColonnes.cpp — detection de gouttiere par projection sur l'axe x.
//
#include "NKMedia/Pdf/NkPdfColonnes.h"
#include "NKMedia/Pdf/NkPdfRaster.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			namespace {
				// Mediane par tri partiel. Sur quelques milliers de valeurs, un tri
				// par insertion suffit et evite d'ecrire un tri rapide de plus.
				float32 Mediane(NkVector<float32> &v) {
					if (v.Size() == 0)
						return 0.f;
					for (nk_size i = 1; i < v.Size(); ++i) {
						const float32 cle = v[i];
						nk_size j = i;
						while (j > 0 && v[j - 1] > cle) {
							v[j] = v[j - 1];
							--j;
						}
						v[j] = cle;
					}
					return v[v.Size() / 2];
				}
			} // namespace

			NkPdfPageColonnes NkPdfAnalyserColonnesPage(const NkVector<NkPdfRenderer::TextItem> &items) {
				NkPdfPageColonnes out;

				// Une page trop pauvre ne se juge pas : deux mots isoles ont
				// toujours un grand vide entre eux, et ce vide n'est pas une
				// gouttiere. Sans ce garde-fou, les pages de garde et les
				// couvertures seraient massivement declarees a deux colonnes.
				if (items.Size() < 100u)
					return out;

				// ── Boite du texte et hauteur de reference ──
				float32 x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
				NkVector<float32> hauteurs;
				hauteurs.Reserve(items.Size());
				for (nk_size i = 0; i < items.Size(); ++i) {
					const NkPdfRenderer::TextItem &it = items[i];
					if (it.text.Size() == 0 || it.w <= 0.f || it.h <= 0.f)
						continue;
					if (it.x < x0) x0 = it.x;
					if (it.x + it.w > x1) x1 = it.x + it.w;
					if (it.y < y0) y0 = it.y;
					if (it.y + it.h > y1) y1 = it.y + it.h;
					hauteurs.PushBack(it.h);
				}
				if (hauteurs.Size() < 100u || x1 <= x0 || y1 <= y0)
					return out;

				const float32 hMed = Mediane(hauteurs);
				if (hMed <= 0.f)
					return out;

				// ── Grille de couverture ──
				// 240 colonnes de bacs sur la largeur du TEXTE (pas de la page :
				// les marges ne doivent pas compter comme des gouttieres), et 40
				// bandes horizontales pour juger « vide sur la majorite de la
				// hauteur ».
				static const int32 kNx = 240, kNy = 40;
				NkVector<uint8> grille;
				grille.Resize((nk_size)(kNx * kNy));
				for (nk_size i = 0; i < grille.Size(); ++i)
					grille[i] = 0;

				const float32 largeur = x1 - x0, hauteur = y1 - y0;
				for (nk_size i = 0; i < items.Size(); ++i) {
					const NkPdfRenderer::TextItem &it = items[i];
					if (it.text.Size() == 0 || it.w <= 0.f)
						continue;
					const int32 by = (int32)(((it.y - y0) / hauteur) * (float32)(kNy - 1));
					int32 bx0 = (int32)(((it.x - x0) / largeur) * (float32)(kNx - 1));
					int32 bx1 = (int32)(((it.x + it.w - x0) / largeur) * (float32)(kNx - 1));
					if (bx0 < 0) bx0 = 0;
					if (bx1 > kNx - 1) bx1 = kNx - 1;
					if (by < 0 || by > kNy - 1)
						continue;
					for (int32 bx = bx0; bx <= bx1; ++bx)
						grille[(nk_size)(by * kNx + bx)] = 1;
				}

				// ── Couverture verticale de chaque bac ──
				// Un bac est « vide » si le texte ne l'occupe que sur une faible
				// part de la hauteur. Le seuil n'est pas zero : un titre ou une
				// figure pleine largeur TRAVERSE legitimement la gouttiere, et
				// exiger le vide absolu ferait manquer presque toutes les vraies
				// pages a deux colonnes.
				NkVector<int32> couverture;
				couverture.Resize((nk_size)kNx);
				for (int32 bx = 0; bx < kNx; ++bx) {
					int32 n = 0;
					for (int32 by = 0; by < kNy; ++by)
						n += grille[(nk_size)(by * kNx + bx)];
					couverture[(nk_size)bx] = n;
				}
				const int32 seuilVide = (int32)(kNy * 0.15); // <= 15 % de la hauteur

				// ── Plus large bande vide CONTIGUE, hors bords ──
				// Les bords sont exclus : la marge droite d'un paragraphe court
				// est un vide legitime, pas une gouttiere.
				int32 meilleurDeb = -1, meilleurLen = 0, deb = -1;
				for (int32 bx = kNx / 10; bx < kNx - kNx / 10; ++bx) {
					if (couverture[(nk_size)bx] <= seuilVide) {
						if (deb < 0)
							deb = bx;
						const int32 len = bx - deb + 1;
						if (len > meilleurLen) {
							meilleurLen = len;
							meilleurDeb = deb;
						}
					} else {
						deb = -1;
					}
				}
				if (meilleurLen <= 0)
					return out;

				// ── Verdict ──
				// La gouttiere doit mesurer au moins 1,5 hauteur de caractere :
				// en dessous, c'est un simple espace entre mots, present partout.
				const float32 largeurGouttiere = ((float32)meilleurLen / (float32)kNx) * largeur;
				if (largeurGouttiere < hMed * 1.5f)
					return out;

				out.multiColonnes = true;
				out.gouttiereX = x0 + (((float32)meilleurDeb + (float32)meilleurLen * 0.5f) /
									   (float32)kNx) * largeur;
				out.gouttiereLargeur = largeurGouttiere;
				return out;
			}

			NkPdfDocColonnes NkPdfDetecterColonnes(NkPdfDoc &doc) {
				NkPdfDocColonnes out;
				NkVector<float32> hauteurs;

				for (int32 p = 0; p < doc.PageCount(); ++p) {
					NkPdfRenderer rendu;
					NkPdfCanvas canevas;
					if (!rendu.RenderPage(doc, p, 72.0, canevas))
						continue;
					const NkVector<NkPdfRenderer::TextItem> &items = rendu.TextItems();
					const NkPdfPageColonnes r = NkPdfAnalyserColonnesPage(items);
					if (items.Size() < 100u)
						continue; // page non jugeable, elle ne compte pas au denominateur
					++out.pagesAnalysees;
					if (r.multiColonnes)
						++out.pagesMultiColonnes;
					for (nk_size i = 0; i < items.Size() && hauteurs.Size() < 5000u; ++i)
						if (items[i].h > 0.f)
							hauteurs.PushBack(items[i].h);
				}

				out.hauteurMediane = Mediane(hauteurs);
				if (out.pagesAnalysees > 0) {
					const double part = (double)out.pagesMultiColonnes / (double)out.pagesAnalysees;
					out.multiColonnes = (part >= kNkPdfSeuilPagesColonnes);
				}
				return out;
			}

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
