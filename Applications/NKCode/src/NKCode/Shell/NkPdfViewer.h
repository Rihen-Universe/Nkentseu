//
// NkPdfViewer.h — visionneuse PDF : affiche une page rendue comme une texture.
//
// Meme patron que la visionneuse video : le moteur produit un bitmap RGBA,
// `UploadRGBA` l'envoie au backend, `AddImage` l'affiche. La visionneuse ne
// sait rien du format PDF.
//
// Le rendu est PARESSEUX et MIS EN CACHE : une page ne se rend qu'au
// changement de page ou de zoom, jamais a chaque frame — rendre une page A4 a
// 150 ppp coute des dizaines de millisecondes.
//
#pragma once

#include "NKCode/Pdf/NkPdf.h"
#include "NKCode/Pdf/NkPdfRender.h"
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Shell/NkUi.h"

namespace nkentseu {
	namespace nkcode {

		struct NkPdfView {
				pdf::NkPdfDoc doc;
				pdf::NkPdfCanvas page;
				NkString path;		// document actuellement charge
				NkString unsupported; // fonctionnalites non rendues, a afficher
				bool opened = false;
				bool failed = false;

				int32 pageIdx = 0;
				int32 wantPage = 0;
				double zoom = 1.0;	   // 1 = ajuste a la largeur

				// Defilement, en pixels de la page rendue au zoom courant.
				float32 scrollX = 0.f, scrollY = 0.f;

				// Etat du dernier rendu : sert a ne PAS refaire le travail tant que
				// rien n'a bouge (une fenetre de page coute quelques millisecondes,
				// mais a 60 images par seconde ce serait ruineux).
				double renderedZoom = -1.0;
				int32 renderedPage = -1;
				float32 renderedScrollX = -1.f, renderedScrollY = -1.f;

				// Glisser des barres de defilement : `grab` retient l'ecart au point
				// de saisie, sinon le curseur saute sous la souris des le 1er pixel.
				int32 dragV = 0, dragH = 0;
				float32 grabV = 0.f, grabH = 0.f;

				uint32 texId = 0;
				int32 texW = 0, texH = 0;
		};

		// Etat par onglet. Un document reste charge tant que son onglet vit :
		// reouvrir le fichier a chaque frame serait ruineux.
		inline NkPdfView *NkPdfViewFor(const NkString &path) {
			static NkVector<NkPdfView *> views;
			for (usize i = 0; i < views.Size(); ++i)
				if (views[i]->path == path)
					return views[i];
			NkPdfView *v = new NkPdfView();
			v->path = path;
			views.PushBack(v);
			return v;
		}

		void DrawPdfViewer(NkGuiContext &ctx, editorkit::NkEditorShell *shell, const NkString &path,
						   const NkRect &r);

	} // namespace nkcode
} // namespace nkentseu
