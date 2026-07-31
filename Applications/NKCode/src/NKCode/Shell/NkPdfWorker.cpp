//
// NkPdfWorker.cpp — voir NkPdfWorker.h.
//
#include "NKCode/Shell/NkPdfWorker.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace nkentseu {
	namespace nkcode {

		// Sommeil court, sans dependance a la bibliotheque standard de threads :
		// meme approche que NkEmbeddedJenga, qui definit deja son propre helper.
		static void SleepMs(unsigned ms) {
#if defined(_WIN32)
			::Sleep(ms);
#else
			::usleep(ms * 1000u);
#endif
		}

		// Le verrou a portee du moteur s'appelle NkScopedLockMutex (sans
		// parametre de type) : c'est ce qu'utilise le reste du code.
		using threading::NkScopedLockMutex;

		bool NkPdfWorker::Open(const NkString &path) {
			NkScopedLockMutex lk(mMutex);
			if (mStarted)
				return true;
			mPath = path;
			mQuit = false;
			mStarted = true;
			mThread = threading::NkThread([this](void *) { Main(); });
			return true;
		}

		void NkPdfWorker::Stop() {
			{
				NkScopedLockMutex lk(mMutex);
				if (!mStarted)
					return;
				mQuit = true;
			}
			if (mThread.Joinable())
				mThread.Join();
			NkScopedLockMutex lk(mMutex);
			mStarted = false;
		}

		bool NkPdfWorker::Request(int32 page, double zoom, double dpi) {
			NkScopedLockMutex lk(mMutex);
			if (!mStarted)
				return false;
			// Meme demande deja en cours : ne pas la refaire.
			if (mRunning && mRunPage == page && mReqZoom == zoom)
				return true;
			// La demande en attente est REMPLACEE, pas mise en file : quand
			// l'utilisateur zoome vite, les etapes intermediaires n'ont aucune valeur.
			mHasReq = true;
			mReqPage = page;
			mReqZoom = zoom;
			mReqDpi = dpi;
			return true;
		}

		bool NkPdfWorker::Busy() const {
			NkScopedLockMutex lk(mMutex);
			return mRunning;
		}

		int32 NkPdfWorker::Pending() const {
			NkScopedLockMutex lk(mMutex);
			return mRunning ? mRunPage : -1;
		}

		bool NkPdfWorker::TakeResult(int32 *page, double *zoom, pdf::NkPdfCanvas &canvas,
									 NkVector<pdf::NkPdfRenderer::TextItem> &items,
									 NkString &unsupported) {
			NkScopedLockMutex lk(mMutex);
			if (!mHasResult)
				return false;
			if (page)
				*page = mResPage;
			if (zoom)
				*zoom = mResZoom;
			// Echange plutot que copie : une page rendue pese plusieurs mega-octets,
			// et la copier sous le verrou bloquerait le fil de rendu.
			canvas.Swap(mResCanvas);
			items = mResItems;
			unsupported = mResUnsupported;
			mResItems.Clear();
			mHasResult = false;
			return true;
		}

		void NkPdfWorker::Main() {
			// Instance PROPRE au fil : le chargement d'objets est paresseux donc
			// mutant, partager celle de l'interface serait une course aux donnees.
			pdf::NkPdfDoc doc;
			NkString path;
			{
				NkScopedLockMutex lk(mMutex);
				path = mPath;
			}
			const bool ok = (doc.Open(path.CStr()) == pdf::NK_PDF_OK);

			for (;;) {
				int32 page = -1;
				double zoom = 1.0, dpi = 72.0;
				{
					NkScopedLockMutex lk(mMutex);
					if (mQuit)
						return;
					// Un resultat non recupere bloque : on attend que l'interface le
					// prenne plutot que de l'ecraser.
					if (!mHasReq || mHasResult) {
						mRunning = false;
						mRunPage = -1;
					} else {
						page = mReqPage;
						zoom = mReqZoom;
						dpi = mReqDpi;
						mHasReq = false;
						mRunning = true;
						mRunPage = page;
					}
				}
				if (page < 0) {
					// Sommeil court : assez pour ne pas occuper un cœur, assez court
					// pour que la reponse reste immediate a l'echelle humaine.
					SleepMs(4);
					continue;
				}
				if (!ok) {
					NkScopedLockMutex lk(mMutex);
					mRunning = false;
					continue;
				}

				pdf::NkPdfCanvas canvas;
				pdf::NkPdfRenderer rend;
				const bool done = rend.RenderPage(doc, page, dpi, canvas);

				NkScopedLockMutex lk(mMutex);
				mRunning = false;
				mRunPage = -1;
				if (!done || mQuit)
					continue;
				mResCanvas.Swap(canvas);
				mResItems = rend.TextItems();
				mResUnsupported = rend.Unsupported();
				mResPage = page;
				mResZoom = zoom;
				mHasResult = true;
			}
		}

	} // namespace nkcode
} // namespace nkentseu
