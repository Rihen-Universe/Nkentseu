#pragma once
// =============================================================================
// NkWin32CameraBackend.h — Media Foundation (Windows 7+)
// CORRIGÉ: sélection device par index via MFEnumDeviceSources + ActivateObject
// Libs: mf.lib mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib
// =============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

#include "NKCamera/NKICameraBackend.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKCore/NkAtomic.h"
#include "NKCore/NkTraits.h"
#include "NKLogger/NkLog.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"
#include "NKThreading/NkThread.h"
#include "NKTime/NkChrono.h"

using Microsoft::WRL::ComPtr;

namespace nkentseu {

	class NkWin32CameraBackend : public NKICameraBackend {
		public:
			NkWin32CameraBackend() = default;

			~NkWin32CameraBackend() override {
				Shutdown();
			}

			// ------------------------------------------------------------------
			bool Init() override {
				HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				mCOMInited = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
				hr = MFStartup(MF_VERSION);
				if (FAILED(hr)) {
					mLastError = "MFStartup failed";
					return false;
				}
				mMFReady = true;
				return true;
			}

			void Shutdown() override {
				StopStreaming();
				if (mMFReady) {
					MFShutdown();
					mMFReady = false;
				}
				if (mCOMInited) {
					CoUninitialize();
					mCOMInited = false;
				}
			}

			// ------------------------------------------------------------------
			// Énumération — CORRECTE
			// ------------------------------------------------------------------
			NkVector<NkCameraDevice> EnumerateDevices() override {
				NkVector<NkCameraDevice> result;
				ComPtr<IMFAttributes> pAttr;
				MFCreateAttributes(&pAttr, 1);
				pAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

				IMFActivate **ppDev = nullptr;
				UINT32 count = 0;
				if (FAILED(MFEnumDeviceSources(pAttr.Get(), &ppDev, &count)))
					return result;

				for (UINT32 i = 0; i < count; ++i) {
					NkCameraDevice dev;
					dev.index = i;
					dev.facing = NkCameraFacing::NK_CAMERA_FACING_EXTERNAL;

					WCHAR buf[256] = {};
					UINT32 len = 0;
					ppDev[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, buf, 256, &len);
					dev.name = WideToUtf8(buf);

					WCHAR idBuf[512] = {};
					ppDev[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, idBuf, 512, &len);
					dev.id = WideToUtf8(idBuf);

					// Modes: activer temporairement pour lire les types
					ComPtr<IMFMediaSource> pSrc;
					if (SUCCEEDED(ppDev[i]->ActivateObject(IID_PPV_ARGS(&pSrc)))) {
						QueryModes(pSrc.Get(), dev.modes);
						pSrc->Shutdown();
						ppDev[i]->ShutdownObject(); // libérer proprement
					}
					result.PushBack(traits::NkMove(dev));
					ppDev[i]->Release();
				}
				CoTaskMemFree(ppDev);
				return result;
			}

			void SetHotPlugCallback(NkCameraHotPlugCallback cb) override {
				mHotPlugCb = traits::NkMove(cb);
			}

			// ------------------------------------------------------------------
			// StartStreaming — sélection par index CORRECTE
			// ------------------------------------------------------------------
			bool StartStreaming(const NkCameraConfig &config) override {
				if (mState != NkCameraState::NK_CAM_STATE_CLOSED)
					StopStreaming();

				// 1. Énumérer à nouveau pour obtenir IMFActivate frais
				ComPtr<IMFAttributes> pAttr;
				MFCreateAttributes(&pAttr, 1);
				pAttr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

				IMFActivate **ppDev = nullptr;
				UINT32 count = 0;
				if (FAILED(MFEnumDeviceSources(pAttr.Get(), &ppDev, &count)) || config.deviceIndex >= count) {
					mLastError = NkString::Fmtf("No camera found at index %u", config.deviceIndex);
					for (UINT32 i = 0; i < count; ++i)
						ppDev[i]->Release();
					CoTaskMemFree(ppDev);
					return false;
				}

				// 2. Activer le device sélectionné
				ComPtr<IMFMediaSource> pSource;
				HRESULT hr = ppDev[config.deviceIndex]->ActivateObject(IID_PPV_ARGS(&pSource));
				for (UINT32 i = 0; i < count; ++i)
					ppDev[i]->Release();
				CoTaskMemFree(ppDev);

				if (FAILED(hr)) {
					mLastError = "ActivateObject failed";
					return false;
				}
				mSource = pSource;

				// 3. Créer le SourceReader
				ComPtr<IMFAttributes> rAttr;
				MFCreateAttributes(&rAttr, 1);
				rAttr->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);

				if (FAILED(MFCreateSourceReaderFromMediaSource(mSource.Get(), rAttr.Get(), &mReader))) {
					mLastError = "MFCreateSourceReaderFromMediaSource failed";
					mSource.Reset();
					return false;
				}

				// 4. Configurer le type de sortie souhaité (NV12 → le plus compatible)
				ComPtr<IMFMediaType> pType;
				MFCreateMediaType(&pType);
				pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
				pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
				MFSetAttributeSize(pType.Get(), MF_MT_FRAME_SIZE, config.width, config.height);
				MFSetAttributeRatio(pType.Get(), MF_MT_FRAME_RATE, config.fps, 1);
				// Ignorer l'erreur si le format exact n'est pas dispo — MF choisira le plus proche
				mReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType.Get());

				// Lire le type effectivement sélectionné
				ComPtr<IMFMediaType> pActual;
				mReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActual);
				if (pActual) {
					MFGetAttributeSize(pActual.Get(), MF_MT_FRAME_SIZE, &mWidth, &mHeight);
					UINT32 num = config.fps, den = 1;
					MFGetAttributeRatio(pActual.Get(), MF_MT_FRAME_RATE, &num, &den);
					mFPS = (den > 0) ? num / den : config.fps;
				} else {
					mWidth = config.width;
					mHeight = config.height;
					mFPS = config.fps;
				}
				mFormat = NkPixelFormat::NK_PIXEL_NV12;
				mFrameIndex = 0;
				mNextVideoTS = 0;

				// ANNONCER CE QU'ON A OBTENU, ET PAS SEULEMENT CE QU'ON A DEMANDE.
				// Media Foundation ne refuse pas un type qu'il ne sait pas servir :
				// il en choisit un PROCHE, en silence. L'application croit donc
				// filmer dans la résolution demandée alors qu'elle reçoit autre
				// chose — et la seule trace visible est une image qui paraît floue,
				// parce qu'elle est étirée à la taille de la fenêtre.
				// Le même angle mort a coûté une calibration entière sur Android le
				// 2026-08-12 : les intrinsèques avaient été calculées sur la
				// résolution DEMANDÉE, jamais sur celle reçue.
				// On imprime donc les deux : l'écart se lit d'un coup d'œil.
				if (mWidth != config.width || mHeight != config.height) {
					logger.Warnf("[NkCamera] Flux demarre en %ux%u @%u fps (NV12) — "
								 "DIFFERENT du %ux%u @%u demande. Le pilote a impose le "
								 "format le plus proche ; une image affichee plus grande "
								 "sera etiree, donc floue.",
								 mWidth, mHeight, mFPS, config.width, config.height, config.fps);
				} else {
					logger.Infof("[NkCamera] Flux demarre : %ux%u @%u fps (NV12), conforme a la demande.",
								 mWidth, mHeight, mFPS);
				}

				// 5. Démarrer le thread de capture
				mRunning = true;
				mState = NkCameraState::NK_CAM_STATE_STREAMING;
				mCaptureThread = threading::NkThread([this](void *) { CaptureLoop(); });
				return true;
			}

			void StopStreaming() override {
				mRunning = false;
				if (mCaptureThread.Joinable())
					mCaptureThread.Join();
				StopVideoRecord();
				mReader.Reset();
				if (mSource) {
					mSource->Shutdown();
					mSource.Reset();
				}
				mState = NkCameraState::NK_CAM_STATE_CLOSED;
			}

			NkCameraState GetState() const override {
				return mState;
			}

			void SetFrameCallback(NkFrameCallback cb) override {
				mFrameCb = traits::NkMove(cb);
			}

			bool GetLastFrame(NkCameraFrame &out) override {
				threading::NkScopedLock<threading::NkMutex> lk(mMutex);
				if (!mHasFrame)
					return false;
				out = mLastFrame;
				return true;
			}

			// ------------------------------------------------------------------
			// Photo
			// ------------------------------------------------------------------
			bool CapturePhoto(NkPhotoCaptureResult &res) override {
				threading::NkScopedLock<threading::NkMutex> lk(mMutex);
				if (!mHasFrame) {
					res.success = false;
					res.errorMsg = "No frame";
					return false;
				}
				res.frame = mLastFrame;
				res.success = true;
				return true;
			}

			bool CapturePhotoToFile(const NkString &path) override {
				NkPhotoCaptureResult r;
				if (!CapturePhoto(r))
					return false;
				// Conversion NV12→RGBA8 puis sauvegarde via stb
				// La conversion est gérée par NkCameraSystem::SaveFrameToFile
				extern bool NkSaveFrame(const NkCameraFrame &, const NkString &, int);
				// Appel direct stbi depuis ici
				// NV12→RGBA8
				auto &f = r.frame;
				uint32 w = f.width, h = f.height;
				NkVector<uint8> rgba;
				rgba.Resize(w * h * 4);
				const uint8 *Y = f.data.Data();
				const uint8 *UV = f.data.Data() + w * h;
				for (uint32 row = 0; row < h; ++row)
					for (uint32 col = 0; col < w; ++col) {
						float y = (float)Y[row * w + col] - 16.f;
						float u = (float)UV[(row / 2) * w + (col & ~1u)] - 128.f;
						float v = (float)UV[(row / 2) * w + (col & ~1u) + 1] - 128.f;
						auto cl = [](float x) -> uint8 { return (uint8)(x < 0 ? 0 : x > 255 ? 255 : x); };
						uint32 i = (row * w + col) * 4;
						rgba[i] = cl(y * 1.164f + v * 1.596f);
						rgba[i + 1] = cl(y * 1.164f - u * 0.391f - v * 0.813f);
						rgba[i + 2] = cl(y * 1.164f + u * 2.018f);
						rgba[i + 3] = 255;
					}
				// Déléguer à SaveFrameToFile du système (inclus via NkCameraSystem.cpp)
				NkCameraFrame rgbaFrame = r.frame;
				rgbaFrame.data = traits::NkMove(rgba);
				rgbaFrame.format = NkPixelFormat::NK_PIXEL_RGBA8;
				rgbaFrame.stride = w * 4;
				// Pas d'encodeur image embarqué dans NKCamera:
				// on écrit ici un .ppm portable (pipeline raw/framebuffer).
				FILE *fp = fopen(path.CStr(), "wb");
				if (!fp)
					return false;
				fprintf(fp, "P6\n%d %d\n255\n", (int)w, (int)h);
				for (uint32 i = 0; i < w * h; ++i)
					fwrite(&rgbaFrame.data[i * 4], 1, 3, fp);
				fclose(fp);
				return true;
			}

			// ------------------------------------------------------------------
			// Vidéo — IMFSinkWriter → MP4/H.264
			// ------------------------------------------------------------------
			bool StartVideoRecord(const NkVideoRecordConfig &config) override {
				if (mSinkWriter || !mReady())
					return false;
				if (config.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY) {
					mLastError = "IMAGE_SEQUENCE_ONLY mode is not implemented on Win32 backend yet";
					return false;
				}
				NkVector<wchar_t> wPath = Utf8ToWide(config.outputPath);

				ComPtr<IMFAttributes> pAttr;
				MFCreateAttributes(&pAttr, 1);
				pAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

				if (FAILED(MFCreateSinkWriterFromURL(wPath.Data(), nullptr, pAttr.Get(), &mSinkWriter))) {
					mLastError = "MFCreateSinkWriterFromURL failed";
					return false;
				}

				// Stream de sortie H.264
				ComPtr<IMFMediaType> pOut;
				MFCreateMediaType(&pOut);
				pOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
				pOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
				MFSetAttributeSize(pOut.Get(), MF_MT_FRAME_SIZE, mWidth, mHeight);
				MFSetAttributeRatio(pOut.Get(), MF_MT_FRAME_RATE, mFPS, 1);
				pOut->SetUINT32(MF_MT_AVG_BITRATE, config.bitrateBps);
				pOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
				MFSetAttributeRatio(pOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
				mSinkWriter->AddStream(pOut.Get(), &mVideoStreamIdx);

				// Stream d'entrée NV12
				ComPtr<IMFMediaType> pIn;
				MFCreateMediaType(&pIn);
				pIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
				pIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
				MFSetAttributeSize(pIn.Get(), MF_MT_FRAME_SIZE, mWidth, mHeight);
				MFSetAttributeRatio(pIn.Get(), MF_MT_FRAME_RATE, mFPS, 1);
				mSinkWriter->SetInputMediaType(mVideoStreamIdx, pIn.Get(), nullptr);
				mSinkWriter->BeginWriting();

				mRecordStart = NkChrono::Now();
				mState = NkCameraState::NK_CAM_STATE_RECORDING;
				return true;
			}

			void StopVideoRecord() override {
				if (!mSinkWriter)
					return;
				mSinkWriter->Finalize();
				mSinkWriter.Reset();
				if (mState == NkCameraState::NK_CAM_STATE_RECORDING)
					mState = NkCameraState::NK_CAM_STATE_STREAMING;
			}

			bool IsRecording() const override {
				return mState == NkCameraState::NK_CAM_STATE_RECORDING;
			}

			float GetRecordingDurationSeconds() const override {
				if (!IsRecording())
					return 0.f;
				return static_cast<float>((NkChrono::Now() - mRecordStart).seconds);
			}

			// IMU: non disponible sur Win32 sans capteur externe
			bool GetOrientation(NkCameraOrientation &) override {
				return false;
			}

			uint32 GetWidth() const override {
				return mWidth;
			}

			uint32 GetHeight() const override {
				return mHeight;
			}

			uint32 GetFPS() const override {
				return mFPS;
			}

			NkPixelFormat GetFormat() const override {
				return mFormat;
			}

			NkString GetLastError() const override {
				return mLastError;
			}

		private:
			bool mReady() {
				return mMFReady && mReader;
			}

			void CaptureLoop() {
				while (mRunning) {
					DWORD flags = 0;
					LONGLONG ts = 0;
					ComPtr<IMFSample> pSample;
					HRESULT hr =
						mReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &ts, &pSample);

					if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM))
						break;
					if (!pSample)
						continue;

					ComPtr<IMFMediaBuffer> pBuf;
					pSample->ConvertToContiguousBuffer(&pBuf);
					if (!pBuf)
						continue;

					BYTE *pData = nullptr;
					DWORD maxLen = 0, curLen = 0;
					pBuf->Lock(&pData, &maxLen, &curLen);

					NkCameraFrame frame;
					frame.width = mWidth;
					frame.height = mHeight;
					frame.format = NkPixelFormat::NK_PIXEL_NV12;
					frame.stride = mWidth;
					frame.timestampUs = (uint64)(ts / 10); // 100ns → µs
					frame.frameIndex = mFrameIndex++;
					frame.data.Clear();
					frame.data.Resize(curLen);
					memcpy(frame.data.Data(), pData, curLen);

					pBuf->Unlock();

					{
						threading::NkScopedLock<threading::NkMutex> lk(mMutex);
						mLastFrame = frame;
						mHasFrame = true;
					}
					if (mFrameCb)
						mFrameCb(frame);

					// Écriture SinkWriter (enregistrement)
					if (mSinkWriter) {
						LONGLONG dur = 10000000LL / mFPS;
						pSample->SetSampleDuration(dur);
						pSample->SetSampleTime(mNextVideoTS);
						mNextVideoTS += dur;
						mSinkWriter->WriteSample(mVideoStreamIdx, pSample.Get());
					}
				}
				mRunning = false;
			}

			// Conversions UTF-16 ↔ UTF-8 sans `std::wstring` : `NkVector<wchar_t>`
			// possède le tampon, et l'API Win32 écrit dedans. Le terminateur est
			// compté par `MultiByteToWideChar` quand on lui passe -1, donc le
			// tampon le contient déjà — inutile de l'ajouter à la main, et
			// dangereux de l'oublier.
			static NkString WideToUtf8(const wchar_t *ws) {
				if (ws == nullptr || ws[0] == L'\0')
					return {};
				const int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
				if (n <= 0)
					return {};
				NkVector<char> buf;
				buf.Resize((usize)n);
				WideCharToMultiByte(CP_UTF8, 0, ws, -1, buf.Data(), n, nullptr, nullptr);
				return NkString(buf.Data()); // le tampon est terminé par zéro
			}

			static NkVector<wchar_t> Utf8ToWide(const NkString &s) {
				NkVector<wchar_t> ws;
				if (s.Empty()) {
					ws.Resize(1);
					ws[0] = L'\0';
					return ws;
				}
				const int n = MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, nullptr, 0);
				if (n <= 0) {
					ws.Resize(1);
					ws[0] = L'\0';
					return ws;
				}
				ws.Resize((usize)n);
				MultiByteToWideChar(CP_UTF8, 0, s.CStr(), -1, ws.Data(), n);
				return ws;
			}

			static void QueryModes(IMFMediaSource *pSrc, NkVector<NkCameraDevice::Mode> &modes) {
				ComPtr<IMFPresentationDescriptor> pPD;
				if (FAILED(pSrc->CreatePresentationDescriptor(&pPD)))
					return;
				BOOL sel = FALSE;
				ComPtr<IMFStreamDescriptor> pSD;
				pPD->GetStreamDescriptorByIndex(0, &sel, &pSD);
				if (!pSD)
					return;
				ComPtr<IMFMediaTypeHandler> pMTH;
				pSD->GetMediaTypeHandler(&pMTH);
				if (!pMTH)
					return;
				DWORD cnt = 0;
				pMTH->GetMediaTypeCount(&cnt);
				for (DWORD m = 0; m < cnt; ++m) {
					ComPtr<IMFMediaType> pMT;
					if (FAILED(pMTH->GetMediaTypeByIndex(m, &pMT)))
						continue;
					NkCameraDevice::Mode mode;
					MFGetAttributeSize(pMT.Get(), MF_MT_FRAME_SIZE, &mode.width, &mode.height);
					UINT32 num = 30, den = 1;
					MFGetAttributeRatio(pMT.Get(), MF_MT_FRAME_RATE, &num, &den);
					mode.fps = (den > 0) ? num / den : 30;
					mode.format = NkPixelFormat::NK_PIXEL_NV12;
					if (mode.width > 0 && mode.height > 0)
						modes.PushBack(mode);
				}
			}

			bool mMFReady = false;
			bool mCOMInited = false;
			NkCameraState mState = NkCameraState::NK_CAM_STATE_CLOSED;
			NkString mLastError;

			ComPtr<IMFMediaSource> mSource;
			ComPtr<IMFSourceReader> mReader;
			ComPtr<IMFSinkWriter> mSinkWriter;

			uint32 mWidth = 0, mHeight = 0, mFPS = 30;
			NkPixelFormat mFormat = NkPixelFormat::NK_PIXEL_NV12;
			uint32 mFrameIndex = 0;
			DWORD mVideoStreamIdx = 0;
			LONGLONG mNextVideoTS = 0;

			threading::NkThread mCaptureThread;
			NkAtomicBool mRunning{false};
			threading::NkMutex mMutex;
			NkCameraFrame mLastFrame;
			bool mHasFrame = false;

			NkFrameCallback mFrameCb;
			NkCameraHotPlugCallback mHotPlugCb;
			NkElapsedTime mRecordStart;
	};

} // namespace nkentseu
