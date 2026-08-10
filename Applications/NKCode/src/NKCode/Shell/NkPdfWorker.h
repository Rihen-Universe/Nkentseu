//
// NkPdfWorker.h — rendu de pages PDF en TACHE DE FOND.
//
// POURQUOI : rendre une page coute des dizaines a des centaines de
// millisecondes. Le faire sur le fil de l'interface fige l'application a chaque
// zoom, changement de page ou bascule de mode — ce que Rihen a constate.
//
// PRINCIPE : l'interface DEMANDE une page et continue son travail. Elle affiche
// entretemps ce qu'elle a — une page deja rendue a un autre zoom, mise a
// l'echelle — plutot qu'un ecran fige. Quand le rendu arrive, elle le prend.
//
// Le fil possede sa PROPRE instance de NkPdfDoc : le chargement d'objets d'un
// document est paresseux, donc mutant, et partager l'instance de l'interface
// serait une course aux donnees. Ouvrir le fichier deux fois coute peu au
// regard de ce que ca evite.
//
#pragma once

#include "NKMedia/Pdf/NkPdf.h"
#include "NKMedia/Pdf/NkPdfRender.h"

#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThread.h"

namespace nkentseu {
	namespace nkcode {

		// Voir NkPdfViewer.h : le lecteur PDF vit desormais dans NKMedia, et cet
		// alias garde valides les usages « pdf::X » deja ecrits.
		namespace pdf = ::nkentseu::media::pdf;

		class NkPdfWorker {
			public:
				NkPdfWorker() = default;
				~NkPdfWorker() { Stop(); }

				NkPdfWorker(const NkPdfWorker &) = delete;
				NkPdfWorker &operator=(const NkPdfWorker &) = delete;

				// Ouvre le document dans le fil. A appeler une fois.
				bool Open(const NkString &path);
				void Stop();

				// Demande le rendu de `page` a `dpi`. Sans effet si la meme demande est
				// deja en cours ou en attente. Renvoie false si le fil est occupe par
				// une autre demande (l'appelant reessaiera a la frame suivante).
				bool Request(int32 page, double zoom, double dpi);

				// Un resultat est-il pret ? Si oui, le TRANSFERE a l'appelant (le
				// canevas est deplace, pas copie : une page rendue pese plusieurs
				// mega-octets).
				bool TakeResult(int32 *page, double *zoom, pdf::NkPdfCanvas &canvas,
								NkVector<pdf::NkPdfRenderer::TextItem> &items, NkString &unsupported);

				bool Busy() const;
				// Page actuellement en cours de rendu (-1 si aucune) : sert a ne pas
				// redemander la meme chose en boucle.
				int32 Pending() const;

			private:
				void Main();

				mutable threading::NkMutex mMutex;
				threading::NkThread mThread;
				NkString mPath;
				bool mQuit = false;
				bool mStarted = false;

				// Demande en attente (une seule : la plus recente prime, car
				// l'utilisateur qui zoome vite n'a pas besoin des etapes intermediaires).
				bool mHasReq = false;
				int32 mReqPage = -1;
				double mReqZoom = 1.0, mReqDpi = 72.0;

				bool mRunning = false;
				int32 mRunPage = -1;

				// Resultat pret a recuperer.
				bool mHasResult = false;
				int32 mResPage = -1;
				double mResZoom = 1.0;
				pdf::NkPdfCanvas mResCanvas;
				NkVector<pdf::NkPdfRenderer::TextItem> mResItems;
				NkString mResUnsupported;
		};

	} // namespace nkcode
} // namespace nkentseu
