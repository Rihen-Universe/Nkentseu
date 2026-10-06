//
// NkPdfType1.h — les polices TYPE 1 embarquees (/FontFile) : leur programme lu,
// leurs glyphes DESSINES.
//
// Pourquoi : les PDF produits par LaTeX (pdflatex, dvips) embarquent leurs polices
// en Type 1 brut. NKFont lit le TrueType et le CFF, pas le Type 1 : NkPdfFont
// lisait leur texte (via /ToUnicode ou les noms de glyphes) mais n'en dessinait
// AUCUN contour -- les pages de texte d'un memoire LaTeX sortaient blanches.
//
// Ce que fait ce module (Adobe Type 1 Font Format, 1990) :
//   - la partie en CLAIR : /FontMatrix, l'encodage interne (« dup N /nom put » ou
//     StandardEncoding) ;
//   - la partie chiffree (eexec, r = 55665, en binaire ou en hexadecimal) ;
//   - le dictionnaire prive : /lenIV, /Subrs, /CharStrings ;
//   - l'interpreteur de charstrings (r = 4330) : tous les traces, closepath,
//     callsubr / return, hsbw / sbw, seac (les accents), div, callothersubr / pop
//     (flex : othersubrs 0, 1, 2 ; remplacement d'indices : 3), setcurrentpoint,
//     endchar. Les indices (hstem, vstem...) sont lus et ignores : on dessine a
//     la resolution de la page, sans hinting.
// Sortie : des contours (deplacement, ligne, courbe CUBIQUE, fermeture) en
// unites du programme ; NkPdfFont les met a l'echelle par la /FontMatrix.
//
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			/// Un element de contour, en unites du programme Type 1.
			struct NkPdfT1Op {
					uint8 type = 0; ///< 0 deplacement, 1 ligne, 2 courbe cubique, 3 fermeture
					double x0 = 0, y0 = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0; ///< courbe : 2 controles puis l'arrivee
			};

			class NkPdfType1 {
				public:
					/// Lit le programme (le flux /FontFile decode). `longueur1` : /Length1 du
					/// flux (la fin de la partie en clair), 0 si inconnue.
					bool Load(const uint8 *prog, usize n, usize longueur1);
					bool Valid() const { return mValide; }

					/// La /FontMatrix [a b c d e f] (defaut 0,001 0 0 0,001 0 0).
					const double *Matrix() const { return mMatrice; }

					/// Le nom du glyphe de `code` dans l'encodage INTERNE du programme
					/// (nullptr : aucun). StandardEncoding si le programme le declare.
					const char *BuiltinName(uint32 code) const;

					/// L'index du glyphe `nom` (-1 : absent).
					int32 GlyphIndex(const char *nom, int32 len) const;
					int32 GlyphCount() const { return static_cast<int32>(mGlyphes.Size()); }
					const NkString &GlyphName(int32 i) const { return mGlyphes[static_cast<usize>(i)].nom; }

					/// Le contour du glyphe `index` (unites du programme), ajoute a `ops`.
					/// `largeur` recoit son avance (hsbw / sbw). false : charstring illisible.
					bool Outline(int32 index, NkVector<NkPdfT1Op> &ops, double *largeur = nullptr) const;

					/// Le nom du glyphe d'un code de StandardEncoding (nullptr : indefini).
					static const char *StandardName(uint32 code);

				private:
					struct Glyphe {
							NkString nom;
							usize debut = 0, longueur = 0; ///< dans mPrive (charstring encore chiffre)
					};
					bool mValide = false;
					double mMatrice[6] = {0.001, 0.0, 0.0, 0.001, 0.0, 0.0};
					bool mEncodageStandard = false;
					NkString mEncodage[256];
					NkVector<uint8> mPrive; ///< la partie eexec, dechiffree
					NkVector<Glyphe> mGlyphes;
					NkVector<usize> mSubrDebut, mSubrLongueur;
					int32 mLenIV = 4;

					bool Executer(int32 index, NkVector<NkPdfT1Op> &ops, double dx, double dy, double *largeur, int32 profondeurSeac) const;
			};

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
