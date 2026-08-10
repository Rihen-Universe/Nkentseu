//
// NkPdfShading.h — fonctions PDF et degrades (§8.7.4).
//
// Un degrade PDF = une GEOMETRIE (axiale ou radiale) + une FONCTION qui donne
// la couleur le long de l'axe. Les deux se traitent separement, et la fonction
// sert aussi ailleurs (transparence, transfert).
//
// PERIMETRE, par frequence reelle : degrades axiaux (type 2) et radiaux
// (type 3) — 8 % du corpus mesure. Les types 1, 4, 5, 6, 7 (maillages) sont
// rares et seraient une deuxieme mecanique entiere.
//
// FONCTIONS : types 2 (exponentielle), 3 (raccordement) et 0 (echantillonnee).
// Le type 4 est un petit langage PostScript : il est SIGNALE, pas devine.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"

namespace nkentseu {
	namespace media {
		namespace pdf {

			// Fonction PDF : [0,1] (ou [t0,t1]) -> jusqu'a 4 composantes.
			class NkPdfFunction {
				public:
					bool Load(const NkPdfDoc &doc, const NkPdfVal &fn);
					bool Valid() const { return mType >= 0; }

					// Evalue en `t` ; ecrit `nOut` composantes dans `out` (max 8).
					// Renvoie le nombre de composantes ecrites.
					int32 Eval(double t, double *out, int32 maxOut) const;

					// Nombre de composantes en sortie, si connu (0 sinon).
					int32 OutCount() const { return mOutN; }

				private:
					int32 mType = -1;
					double mD0 = 0.0, mD1 = 1.0; // domaine
					int32 mOutN = 0;

					// Type 2 : C0 + t^N * (C1 - C0)
					double mC0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
					double mC1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
					double mN = 1.0;

					// Type 3 : sous-fonctions raccordees
					NkVector<NkPdfFunction> mSubs;
					NkVector<double> mBounds;
					NkVector<double> mEncode;

					// Type 0 : echantillons
					NkVector<double> mSamples; // normalises dans [0,1]
					int32 mSize = 0;
					NkVector<double> mRange;
			};

			// Degrade : geometrie + fonction(s).
			class NkPdfShading {
				public:
					bool Load(const NkPdfDoc &doc, const NkPdfVal &sh);
					bool Valid() const { return mType == 2 || mType == 3; }
					int32 Type() const { return mType; }

					// Couleur RVB au point (x, y) exprime dans l'espace du degrade.
					// Renvoie false si le point est hors du degrade ET non etendu —
					// l'appelant ne doit alors rien peindre a cet endroit.
					bool ColorAt(double x, double y, double *r, double *g, double *b) const;

				private:
					void FnColor(double t, double *r, double *g, double *b) const;

					int32 mType = 0;
					double mCoords[6] = {0, 0, 0, 0, 0, 0};
					bool mExtend0 = false, mExtend1 = false;
					double mT0 = 0.0, mT1 = 1.0;
					int32 mComps = 3; // 1 = gris, 3 = RVB, 4 = CMJN
					NkVector<NkPdfFunction> mFns; // 1 fonction a n sorties, ou n fonctions
			};

		} // namespace pdf
	} // namespace media
} // namespace nkentseu
