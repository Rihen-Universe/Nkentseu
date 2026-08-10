//
// NkPdfFont.h — polices d'un document PDF : chargement et contours de glyphes.
//
// AUCUN interpreteur de police n'est ecrit ici, et c'est le resultat d'une
// MESURE : sur un corpus de 95 PDF reels, les programmes de police embarques
// sont a 58,9 % du TrueType (/FontFile2) et 3,2 % du CFF (/FontFile3) — deux
// formats que NKFont lit deja. Le Type 1 brut (/FontFile) ne pese que 2,1 % :
// il est SUBSTITUE, pas rendu fidelement, et c'est un compromis assume.
//
// Ce module fait donc trois choses : trouver le programme de police, le donner
// a NKFont (qui sait charger depuis la MEMOIRE), et traduire un code de
// caractere du flux de contenu en identifiant de glyphe + largeur d'avance.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfRaster.h"

#include "NKFont/Core/NkFontParser.h"

namespace nkentseu {
	namespace nkcode {
		namespace pdf {

			// Une police telle que le flux de contenu la designe (/F1, /F2...).
			class NkPdfFont {
				public:
					NkPdfFont() = default;
					~NkPdfFont() { Unload(); }

					NkPdfFont(const NkPdfFont &) = delete;
					NkPdfFont &operator=(const NkPdfFont &) = delete;

					// Charge depuis le dictionnaire de police du document.
					bool Load(const NkPdfDoc &doc, const NkPdfVal &fontDict);
					void Unload();

					bool HasProgram() const { return mHasFace; }

					// Composite (Type0/CID) : les codes du flux font DEUX octets.
					// C'est la premiere chose a savoir pour decouper une chaine.
					bool IsTwoByte() const { return mTwoByte; }

					// Nombre d'octets consommes et code obtenu, a la position `i` de la
					// chaine. Renvoie le nombre d'octets lus (1 ou 2).
					int32 NextCode(const uint8 *s, int32 len, int32 i, uint32 *codeOut) const;

					// Largeur d'avance du code, en unites de texte (1/1000 d'em, la
					// convention PDF). Utilise /Widths, /W ou la valeur par defaut.
					double Advance(uint32 code) const;

					// Contour du glyphe correspondant, en unites d'em normalisees
					// (1 = corps de la police). `out` est COMPLETE, pas efface : un
					// appelant peut accumuler plusieurs glyphes dans un meme trace.
					// `tx`, `ty` = translation appliquee, `scale` = taille finale.
					// Renvoie false si le glyphe est vide ou la police absente.
					bool AppendGlyph(uint32 code, double tx, double ty, double scale,
									 double shearX, double vScale, NkPdfPath &out) const;

					// Texte UNICODE (UTF-8) correspondant a un code, via la table
					// /ToUnicode du document. Chaine vide si la police n'en fournit pas.
					//
					// C'est la SEULE facon honnete de retrouver du texte lisible : un
					// code de glyphe n'a aucun sens hors de sa police (le meme code 3
					// peut etre un espace ici et un « e » ailleurs). Sans /ToUnicode, on
					// ne peut PAS deviner — et on le dira plutot que d'inventer.
					NkString ToUnicode(uint32 code) const;
					bool HasToUnicode() const { return !mUniCodes.Empty(); }

					// Nom de la police tel que declare (diagnostic).
					const NkString &BaseFont() const { return mBaseFont; }

					// DIAGNOSTIC : combien des `n` premiers identifiants de glyphe
					// produisent un contour ? Isole NKFont de ce module — si ce compte
					// est nul alors que la police s'est chargee, le probleme est dans
					// la lecture du programme de police, pas dans l'appelant.
					int32 DiagGlyphsWithShape(int32 n) const;
					usize ProgramSize() const { return mProgram.Size(); }

				private:
					NkGlyphId GlyphOf(uint32 code) const;

					bool mHasFace = false;
					nkfont::NkFontFaceInfo mFace{};
					NkVector<uint8> mProgram; // le programme de police doit SURVIVRE a
											  // la face : NKFont pointe dedans.
					NkString mBaseFont;
					bool mTwoByte = false;
					bool mIdentityCid = false; // Identity-H : le code EST le CID

					// Largeurs des polices simples : /FirstChar + /Widths.
					int32 mFirstChar = 0;
					NkVector<double> mWidths;
					double mDefaultWidth = 500.0;

					// Largeurs des polices composites : /W, en paires (code, largeur).
					NkVector<uint32> mCidCodes;
					NkVector<double> mCidWidths;

					// Table /ToUnicode : code -> texte UTF-8. Deux tableaux paralleles
					// plutot qu'une table de hachage : quelques centaines d'entrees au
					// plus, et la recherche n'a lieu qu'a la selection, pas au rendu.
					NkVector<uint32> mUniCodes;
					NkVector<NkString> mUniText;

					void ParseToUnicode(const NkPdfDoc &doc, const NkPdfVal &fontDict);

					// Echelle du programme : unites de police -> em.
					double mUnitsPerEmInv = 1.0 / 1000.0;
			};

			// Cache des polices d'une page : le dictionnaire /Font des ressources.
			// Une police est chargee UNE fois meme si elle sert des milliers de fois.
			class NkPdfFontCache {
				public:
					~NkPdfFontCache() { Clear(); }
					void Clear();

					// Renvoie la police nommee `name` dans le /Font de `resources`,
					// ou nullptr. La propriete reste au cache.
					NkPdfFont *Get(const NkPdfDoc &doc, const NkPdfVal &resources, const char *name,
								   int32 nameLen);

				private:
					struct Slot {
							NkString name;
							NkPdfFont *font = nullptr;
					};
					NkVector<Slot> mSlots;
			};

		} // namespace pdf
	} // namespace nkcode
} // namespace nkentseu
