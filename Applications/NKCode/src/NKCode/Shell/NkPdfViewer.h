//
// NkPdfViewer.h — visionneuse PDF : affiche les pages rendues comme texture.
//
// DEUX MODES, au choix de l'utilisateur :
//   CONTINU (defaut)  toutes les pages empilees, un seul defilement traverse
//                     le document entier — le comportement d'un vrai lecteur.
//   PAGE PAR PAGE     une page a la fois, avec les fleches.
//
// Le rendu est PARESSEUX et CACHE : seules les pages visibles sont rendues, et
// un petit cache garde les dernieres — sans lui, changer de page coutait un
// rendu complet a chaque fois, ce qui rendait la navigation penible.
//
// La texture, elle, garde TOUJOURS la taille du panneau : le backend fige les
// dimensions d'une texture a sa creation et n'offre aucune liberation.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfRender.h"
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Shell/NkPdfWorker.h"
#include "NKCode/Shell/NkUi.h"

namespace nkentseu {
	namespace nkcode {

		// Le lecteur PDF a ete descendu dans NKMedia : un module du noyau (NKAI)
		// ne peut pas dependre d'une application sans renverser les couches. Cet
		// alias garde valides les usages « pdf::X » ecrits du temps ou pdf etait
		// un enfant de nkcode — les reecrire un par un n'aurait rien apporte de
		// plus qu'une occasion de se tromper.
		namespace pdf = ::nkentseu::media::pdf;

		// Une page rendue, gardee en cache.
		struct NkPdfPageCache {
				int32 page = -1;
				double zoom = -1.0;
				pdf::NkPdfCanvas canvas;
				NkVector<pdf::NkPdfRenderer::TextItem> items;
				NkString unsupported;
				uint32 lastUse = 0; // pour evincer la plus ancienne
		};

		struct NkPdfView {
				pdf::NkPdfDoc doc;
				// Canevas de la FENETRE visible : il alimente la texture, et sa taille
				// ne change jamais tant que le panneau garde la sienne.
				pdf::NkPdfCanvas window;

				NkString path;
				NkString unsupported;
				bool opened = false;
				bool failed = false;

				// MODE. Continu par defaut : c'est ce qu'on attend d'un lecteur.
				bool continuous = true;

				int32 pageIdx = 0; // page courante (mode page par page, et affichage)
				double zoom = 1.0; // 1 = ajuste a la largeur

				// Defilement, en pixels du DOCUMENT (mode continu) ou de la page.
				float32 scrollX = 0.f, scrollY = 0.f;

				// ── Disposition, recalculee au changement de zoom ──
				// Hauteur cumulee de chaque page + espacement : permet de savoir quelles
				// pages sont visibles SANS les rendre.
				NkVector<int32> pageW, pageH;
				NkVector<float32> pageTop; // ordonnee du haut de chaque page
				float32 docW = 0.f, docH = 0.f;
				double layoutZoom = -1.0;
				int32 layoutPanelW = -1;

				// ── Cache de pages rendues ──
				NkVector<NkPdfPageCache *> cache;
				uint32 useClock = 0;

				// Rendu en TACHE DE FOND : l'interface demande une page et continue,
				// au lieu de figer pendant le rendu. C'est ce qui rendait le zoom, le
				// changement de page et la bascule de mode bloquants.
				NkPdfWorker worker;
				// Vrai tant qu'une page visible manque : sert a afficher un reperage
				// discret plutot que de laisser croire a un blocage.
				bool waiting = false;

				// Etat du dernier assemblage de la fenetre : evite de recopier a
				// l'identique a chaque frame.
				float32 builtScrollX = -1.f, builtScrollY = -1.f;
				double builtZoom = -1.0;
				int32 builtPanelW = -1, builtPanelH = -1;
				bool builtContinuous = true;

				// Selection : elements visibles, en coordonnees du DOCUMENT.
				NkVector<pdf::NkPdfRenderer::TextItem> items;
				int32 selA = -1, selB = -1;
				bool selecting = false;

				uint32 texId = 0;
				int32 texW = 0, texH = 0;

				~NkPdfView() {
					for (usize i = 0; i < cache.Size(); ++i)
						delete cache[i];
				}
		};

		// Etat par onglet : un document reste charge tant que son onglet vit.
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
