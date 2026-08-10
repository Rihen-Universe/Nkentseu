//
// NkPdfRender.h — rendu d'une page PDF vers un bitmap.
//
// Interprete le flux de contenu (§8 et §9 de la specification) : etat
// graphique, traces, couleurs, decoupage, texte, images. Chaque operateur se
// traduit en appels au rastériseur, qui ignore tout du PDF.
//
// PERIMETRE v1, decide par mesure sur 95 PDF reels :
//   RENDU        traces et remplissages (regles non-nulle et pair-impair),
//                contours, transformations, decoupage, gris/RVB/CMJN, texte
//                (TrueType et CFF embarques), images JPEG et brutes, alpha.
//   NON RENDU    degrades et motifs (aplat moyen a la place), groupes de
//                transparence, modes de fusion, Type 3, annotations.
//                Chacun est SIGNALE plutot que silencieusement ignore.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfFont.h"
#include "NKMedia/Pdf/NkPdfRaster.h"
#include "NKMedia/Pdf/NkPdfShading.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// Matrice affine PDF : [a b 0 ; c d 0 ; e f 1].
			struct NkPdfMat {
					double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;

					void Apply(double x, double y, double *ox, double *oy) const {
						*ox = a * x + c * y + e;
						*oy = b * x + d * y + f;
					}
					// this = m * this  (m s'applique AVANT, convention PDF pour `cm`).
					static NkPdfMat Mul(const NkPdfMat &m, const NkPdfMat &n) {
						NkPdfMat r;
						r.a = m.a * n.a + m.b * n.c;
						r.b = m.a * n.b + m.b * n.d;
						r.c = m.c * n.a + m.d * n.c;
						r.d = m.c * n.b + m.d * n.d;
						r.e = m.e * n.a + m.f * n.c + n.e;
						r.f = m.e * n.b + m.f * n.d + n.f;
						return r;
					}
					// Facteur d'echelle moyen : sert a convertir une epaisseur de trait.
					double Scale() const;
			};

			class NkPdfRenderer {
				public:
					// Rend la page `pageIdx` a `dpi` points par pouce (72 = taille
					// nominale). Renvoie false si la page est introuvable ou les
					// dimensions aberrantes.
					bool RenderPage(NkPdfDoc &doc, int32 pageIdx, double dpi, NkPdfCanvas &out);

					// Rend une FENETRE de la page dans un canevas DEJA dimensionne.
					// `offX`/`offY` sont le coin haut-gauche de la fenetre, en pixels de
					// la page rendue a `dpi`.
					//
					// C'est ce que fait tout vrai lecteur, et c'est necessaire ici pour
					// une raison precise : le backend fige la taille d'une texture a sa
					// CREATION, et n'offre aucune liberation. Rendre la page entiere
					// obligeait a reallouer une texture a chaque cran de zoom — fuite de
					// dizaines de mega-octets par cran, puis plantage. En rendant
					// toujours dans un canevas de la taille du PANNEAU, la texture ne
					// change jamais de dimensions : plus de reallocation, memoire bornee,
					// et le zoom peut monter aussi haut qu'on veut.
					bool RenderPageWindow(NkPdfDoc &doc, int32 pageIdx, double dpi, double offX, double offY,
										  NkPdfCanvas &out);

					// Dimensions de la page entiere a `dpi`, sans rien rendre : sert a
					// borner le defilement et a dimensionner les barres.
					static bool PagePixelSize(NkPdfDoc &doc, int32 pageIdx, double dpi, int32 *w, int32 *h);

					// Fonctionnalites rencontrees mais NON rendues, pour le dire a
					// l'utilisateur au lieu de le laisser croire a un rendu fidele.
					const NkString &Unsupported() const { return mUnsupported; }

					// ── Texte SELECTIONNABLE ──
					// Chaque caractere dessine laisse sa boite, en pixels du canevas, et
					// son equivalent UTF-8 issu de la table /ToUnicode de la police.
					//
					// C'est la SEULE facon honnete de selectionner du texte dans un PDF :
					// le format ne stocke pas de texte, mais des identifiants de glyphes
					// places un a un. Sans /ToUnicode on ne peut PAS deviner ce qu'un
					// glyphe represente — on laisse alors le caractere vide plutot que
					// d'inventer, et la selection le saute.
					struct TextItem {
							float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
							NkString text; // UTF-8 ; vide si la police n'a pas de /ToUnicode
					};
					const NkVector<TextItem> &TextItems() const { return mText; }

				// Compteurs de DIAGNOSTIC : une page blanche sans avertissement ne
					// se diagnostique pas autrement. Ils disent OU la chaine se rompt —
					// contenu vide, operateurs non executes, glyphes sans contour, ou
					// peinture hors cadre.
					struct Stats {
							int32 contentBytes = 0;
							int32 ops = 0;
							int32 fills = 0, strokes = 0;
							int32 textOps = 0, glyphsAsked = 0, glyphsGot = 0;
							int32 images = 0, forms = 0;
							int32 strBytes = 0;   // octets de chaine passes au rendu de texte
							uint32 firstCodes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
							int32 nFirstCodes = 0;
					};
					const Stats &GetStats() const { return mStats; }
					// Derniere police vue par Tf : sert au diagnostic des pages blanches.
					NkPdfFont *LastFont() const { return mLastFont; }

				private:
					struct GState {
							NkPdfMat ctm;
							double fill[3] = {0, 0, 0};
							double stroke[3] = {0, 0, 0};
							double fillAlpha = 1.0, strokeAlpha = 1.0;
							double lineWidth = 1.0;
							// Etat texte : conserve par q/Q comme le reste.
							double charSpace = 0.0, wordSpace = 0.0, hscale = 1.0;
							double leading = 0.0, rise = 0.0, fontSize = 0.0;
							int32 render = 0; // Tr : 3 = invisible (texte de calque OCR)
						// Motif/degrade non rendu : on s'ABSTIENT de peindre plutot que
						// de poser un aplat. Un aplat gris sur une grande forme couvre
						// le texte pose au-dessus et detruit la page ; ne rien peindre
						// laisse le contenu lisible. Mesure a l'appui : le repli gris
						// portait certaines pages a 95 % d'encre.
						bool fillIsPattern = false;
						bool strokeIsPattern = false;
						// Nom du motif courant : sert a le RETROUVER dans les ressources
						// au moment de peindre, pas au moment de le declarer (les
						// ressources peuvent changer entre les deux via un formulaire).
						NkString fillPattern, strokePattern;
							NkPdfFont *font = nullptr;
							// Le decoupage n'est PLUS ici : il vit dans la pile du canevas,
							// pour eviter de copier un masque de la taille de la page a
							// chaque q/Q. Voir NkPdfCanvas::PushClipState.
					};

					void Note(const char *what);
					void Run(const NkVector<uint8> &content, const NkPdfVal &resources, int32 depth);
					void DoXObject(const char *name, int32 nameLen, const NkPdfVal &resources, int32 depth);
					void DrawImage(const NkPdfVal &img, int32 depth);

					// Peint `path` avec un degrade, en resolvant la couleur PIXEL PAR
					// PIXEL : chaque point de l'image est ramene dans l'espace du
					// degrade par la matrice inverse.
					void FillWithShading(const NkPdfPath &path, bool evenOdd, const NkPdfVal &shDict,
										 const NkPdfMat &mat, double alpha);

					// Peint `path` avec un motif (PatternType 1 pave, 2 degrade).
					void FillWithPattern(const NkPdfPath &path, bool evenOdd, const NkString &name,
										 const NkPdfVal &resources, int32 depth);

					NkPdfDoc *mDoc = nullptr;
					NkPdfCanvas *mCv = nullptr;
					NkPdfFontCache mFonts;
					NkString mUnsupported;
					Stats mStats;
					NkPdfFont *mLastFont = nullptr;
					NkVector<TextItem> mText;
					bool mWindowMode = false; // rendu d'une fenetre dans un canevas fourni
					double mWinOffX = 0.0, mWinOffY = 0.0;

					NkVector<GState> mStack;
					GState mGs;
					NkPdfPath mPath;	 // trace en construction, en pixels
					bool mPendingClip = false;
					bool mPendingClipEO = false;

					// Etat texte inter-operateurs (BT/ET).
					NkPdfMat mTm, mTlm;
					// Matrice de la PAGE : espace de reference des motifs, qui ne
					// suivent PAS la matrice courante.
					NkPdfMat mBaseCtm;
			};

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
