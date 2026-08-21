// =============================================================================
// NKAudio/NkAudioCapture.cpp — implémentation (voir NkAudioCapture.h)
// Ring buffer SPSC lock-free (NkAtomic) + backend WASAPI capture (Windows) + Null.
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKAudio/NkAudioCapture.h"
#include "NKCore/NkAtomic.h"
#include "NKMemory/NKMemory.h"
#include "NKLogger/NkLog.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#elif defined(NKENTSEU_PLATFORM_ANDROID)
// Capture AAudio (Android 26+) — dlopen libaaudio.so, setDirection(INPUT), data callback -> ring.
#include <dlfcn.h>
#include <android/api-level.h>
#include <cstring>
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
// Capture getUserMedia (Web) — EM_JS : getUserMedia + ScriptProcessorNode -> ring.
#include <emscripten/emscripten.h>
#include <emscripten/em_js.h>
#include <cstring>
#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
// Capture OHAudio (HarmonyOS) — OH_AudioCapturer (F32LE), read-data callback -> ring.
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiocapturer.h>
#include <cstring>
#elif defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_HARMONYOS)
// Capture ALSA (Linux) — miroir du backend de lecture (SND_PCM_STREAM_CAPTURE + snd_pcm_readi).
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>
#endif

namespace nkentseu {
	namespace audio {

		// --- Ring buffer SPSC lock-free (1 producteur = thread capture, 1 consommateur = Read) ---
		struct NkCaptureRing {
				float32 *buf = nullptr;			  // capacité = cap floats
				uint64 cap = 0;					  // taille en FLOATS (frames*channels)
				NkAtomic<uint64> wr{0};			  // total floats écrits (monotone)
				NkAtomic<uint64> rd{0};			  // total floats lus (monotone)

				void Alloc(uint64 floats) {
					Free();
					if (floats == 0)
						return;
					buf = (float32 *)memory::NkAlloc((size_t)(floats * sizeof(float32)));
					cap = buf ? floats : 0;
					wr.Store(0);
					rd.Store(0);
				}

				void Free() {
					if (buf)
						memory::NkFree(buf);
					buf = nullptr;
					cap = 0;
				}

				// Écrit `n` floats (producteur). Si plein, drop le trop-plein NOUVEAU.
				void Write(const float32 *src, uint64 n) {
					if (!buf || cap == 0)
						return;
					const uint64 w = wr.Load();
					const uint64 r = rd.Load();
					const uint64 space = cap - (w - r);
					if (n > space)
						n = space; // drop du surplus (consommateur trop lent)
					for (uint64 i = 0; i < n; ++i)
						buf[(w + i) % cap] = src[i];
					wr.Store(w + n);
				}

				// Lit jusqu'à `n` floats (consommateur). Retourne le nombre lu.
				uint64 Read(float32 *dst, uint64 n) {
					if (!buf || cap == 0)
						return 0;
					const uint64 w = wr.Load();
					const uint64 r = rd.Load();
					uint64 avail = w - r;
					if (n > avail)
						n = avail;
					for (uint64 i = 0; i < n; ++i)
						dst[i] = buf[(r + i) % cap];
					rd.Store(r + n);
					return n;
				}

				uint64 AvailableFloats() const {
					return wr.Load() - rd.Load();
				}
		};

		struct NkAudioCapture::Impl {
				NkCaptureConfig cfg;
				NkCaptureRing ring;
				NkAudioInCallback cb;
				bool hasCb = false;
				bool capturing = false;
				const char *backend = "Null";

#if defined(NKENTSEU_PLATFORM_WINDOWS)
				IMMDeviceEnumerator *enumerator = nullptr;
				IMMDevice *device = nullptr;
				IAudioClient *client = nullptr;
				IAudioCaptureClient *capClient = nullptr;
				bool coInit = false;
				int32 devChannels = 0; // canaux réels du périphérique
				bool devFloat = true;  // format périphérique : float32 (sinon int16)
				HANDLE thread = nullptr;
				volatile long running = 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
				void *stream = nullptr; // AAudioStream*
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
				bool webStarted = false;
#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
				void *capturer = nullptr; // OH_AudioCapturer*
				void *builder = nullptr;  // OH_AudioStreamBuilder*
#elif defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_HARMONYOS)
				snd_pcm_t *pcm = nullptr;
				pthread_t thread = 0;
				volatile bool running = false;
				bool threadValid = false;
#endif
		};

		NkAudioCapture::NkAudioCapture() {
			mImpl = memory::NkGetDefaultAllocator().New<Impl>();
		}

		NkAudioCapture::~NkAudioCapture() {
			Close();
			if (mImpl)
				memory::NkGetDefaultAllocator().Delete(mImpl);
			mImpl = nullptr;
		}

		void NkAudioCapture::SetCallback(NkAudioInCallback c) {
			mImpl->cb = c;
			mImpl->hasCb = true;
		}

		int32 NkAudioCapture::Available() const {
			const int32 ch = mImpl->cfg.channels > 0 ? mImpl->cfg.channels : 1;
			return (int32)(mImpl->ring.AvailableFloats() / (uint64)ch);
		}

		int32 NkAudioCapture::Read(float32 *out, int32 maxFrames) {
			if (!out || maxFrames <= 0)
				return 0;
			const int32 ch = mImpl->cfg.channels > 0 ? mImpl->cfg.channels : 1;
			const uint64 got = mImpl->ring.Read(out, (uint64)maxFrames * (uint64)ch);
			return (int32)(got / (uint64)ch);
		}

		int32 NkAudioCapture::SampleRate() const {
			return mImpl->cfg.sampleRate;
		}

		int32 NkAudioCapture::Channels() const {
			return mImpl->cfg.channels;
		}

		const char *NkAudioCapture::BackendName() const {
			return mImpl->backend;
		}

		bool NkAudioCapture::IsCapturing() const {
			return mImpl->capturing;
		}

		// =====================================================================
		//  Backend WASAPI (Windows)
		// =====================================================================
#if defined(NKENTSEU_PLATFORM_WINDOWS)

		// Pousse un bloc du périphérique (devChannels, float32 ou int16) converti vers cfg.channels.
		static void PushBlock(NkAudioCapture::Impl *im, const BYTE *data, UINT32 frames, DWORD flags) {
			const int32 dstCh = im->cfg.channels > 0 ? im->cfg.channels : 1;
			const int32 srcCh = im->devChannels > 0 ? im->devChannels : 1;
			// buffer temporaire sur la pile (petits paquets WASAPI ~10ms).
			static const int32 kMaxTmp = 8192;
			float32 tmp[kMaxTmp];
			int32 outCount = 0;
			const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
			for (UINT32 f = 0; f < frames; ++f) {
				// moyenne des canaux source -> mono, puis réplique sur dstCh (down/up-mix simple).
				float32 mono = 0.f;
				if (!silent) {
					for (int32 c = 0; c < srcCh; ++c) {
						float32 s;
						if (im->devFloat)
							s = ((const float32 *)data)[f * srcCh + c];
						else
							s = (float32)((const int16 *)data)[f * srcCh + c] / 32768.0f;
						mono += s;
					}
					mono /= (float32)srcCh;
				}
				for (int32 c = 0; c < dstCh; ++c) {
					if (outCount < kMaxTmp)
						tmp[outCount++] = mono;
				}
				if (outCount > kMaxTmp - dstCh) {
					im->ring.Write(tmp, (uint64)outCount);
					if (im->hasCb)
						im->cb(tmp, outCount / dstCh, dstCh);
					outCount = 0;
				}
			}
			if (outCount > 0) {
				im->ring.Write(tmp, (uint64)outCount);
				if (im->hasCb)
					im->cb(tmp, outCount / dstCh, dstCh);
			}
		}

		static DWORD WINAPI CaptureThreadProc(LPVOID param) {
			NkAudioCapture::Impl *im = (NkAudioCapture::Impl *)param;
			CoInitializeEx(nullptr, COINIT_MULTITHREADED); // COM par thread
			if (im->client)
				im->client->Start();
			while (InterlockedCompareExchange(&im->running, 1, 1) == 1) {
				UINT32 packet = 0;
				if (im->capClient->GetNextPacketSize(&packet) != S_OK) {
					Sleep(5);
					continue;
				}
				while (packet > 0) {
					BYTE *data = nullptr;
					UINT32 numFrames = 0;
					DWORD flags = 0;
					if (im->capClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr) != S_OK)
						break;
					PushBlock(im, data, numFrames, flags);
					im->capClient->ReleaseBuffer(numFrames);
					if (im->capClient->GetNextPacketSize(&packet) != S_OK)
						break;
				}
				Sleep(3);
			}
			if (im->client)
				im->client->Stop();
			CoUninitialize();
			return 0;
		}

		NkVector<NkCaptureDeviceInfo> NkAudioCapture::EnumerateDevices() {
			NkVector<NkCaptureDeviceInfo> out;
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			IMMDeviceEnumerator *en = nullptr;
			if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
								 (void **)&en) == S_OK &&
				en) {
				IMMDevice *def = nullptr;
				if (en->GetDefaultAudioEndpoint(eCapture, eConsole, &def) == S_OK && def) {
					NkCaptureDeviceInfo info;
					info.name = NkString("Microphone par défaut");
					info.isDefault = true;
					out.PushBack(info);
					def->Release();
				}
				en->Release();
			}
			return out; // (énumération détaillée par nom = raffinement ; le device par défaut suffit)
		}

		bool NkAudioCapture::Open(const NkCaptureConfig &config) {
			Close();
			mImpl->cfg = config;
			if (mImpl->cfg.channels <= 0)
				mImpl->cfg.channels = 1;
			if (mImpl->cfg.sampleRate <= 0)
				mImpl->cfg.sampleRate = 48000;

			HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (SUCCEEDED(hr))
				mImpl->coInit = true;

			if (CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
								 (void **)&mImpl->enumerator) != S_OK) {
				logger.Error("[NkAudioCapture] CoCreateInstance MMDeviceEnumerator FAILED");
				return false;
			}
			if (mImpl->enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &mImpl->device) != S_OK) {
				logger.Error("[NkAudioCapture] pas de microphone (GetDefaultAudioEndpoint eCapture)");
				return false;
			}
			if (mImpl->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&mImpl->client) != S_OK) {
				logger.Error("[NkAudioCapture] Activate IAudioClient FAILED");
				return false;
			}
			WAVEFORMATEX *mix = nullptr;
			if (mImpl->client->GetMixFormat(&mix) != S_OK || !mix) {
				logger.Error("[NkAudioCapture] GetMixFormat FAILED");
				return false;
			}
			mImpl->devChannels = mix->nChannels;
			// format : IEEE float (WAVE_FORMAT_EXTENSIBLE avec sous-format float, ou WAVE_FORMAT_IEEE_FLOAT).
			mImpl->devFloat = (mix->wBitsPerSample == 32);
			mImpl->cfg.sampleRate = (int32)mix->nSamplesPerSec; // on capture au taux natif du device

			REFERENCE_TIME dur = 10 * 10000; // 10 ms buffer
			hr = mImpl->client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, dur, 0, mix, nullptr);
			CoTaskMemFree(mix);
			if (hr != S_OK) {
				logger.Error("[NkAudioCapture] IAudioClient::Initialize FAILED hr=0x{0:X}", (unsigned)hr);
				return false;
			}
			if (mImpl->client->GetService(__uuidof(IAudioCaptureClient), (void **)&mImpl->capClient) != S_OK) {
				logger.Error("[NkAudioCapture] GetService IAudioCaptureClient FAILED");
				return false;
			}

			// ring buffer : ringSeconds secondes @ sampleRate * channels floats.
			const int32 secs = mImpl->cfg.ringSeconds > 0 ? mImpl->cfg.ringSeconds : 4;
			mImpl->ring.Alloc((uint64)mImpl->cfg.sampleRate * (uint64)mImpl->cfg.channels * (uint64)secs);
			mImpl->backend = "WASAPI";
			logger.Info("[NkAudioCapture] OK (WASAPI, {0} Hz, dev {1} canaux -> {2}, {3})", mImpl->cfg.sampleRate,
						mImpl->devChannels, mImpl->cfg.channels, mImpl->devFloat ? "f32" : "i16");
			return true;
		}

		bool NkAudioCapture::Start() {
			if (!mImpl->capClient || mImpl->capturing)
				return false;
			InterlockedExchange(&mImpl->running, 1);
			mImpl->thread = CreateThread(nullptr, 0, CaptureThreadProc, mImpl, 0, nullptr);
			if (!mImpl->thread) {
				InterlockedExchange(&mImpl->running, 0);
				return false;
			}
			mImpl->capturing = true;
			return true;
		}

		void NkAudioCapture::Stop() {
			if (!mImpl->capturing)
				return;
			InterlockedExchange(&mImpl->running, 0);
			if (mImpl->thread) {
				WaitForSingleObject(mImpl->thread, 2000);
				CloseHandle(mImpl->thread);
				mImpl->thread = nullptr;
			}
			mImpl->capturing = false;
		}

		void NkAudioCapture::Close() {
			if (!mImpl)
				return;
			Stop();
			if (mImpl->capClient) {
				mImpl->capClient->Release();
				mImpl->capClient = nullptr;
			}
			if (mImpl->client) {
				mImpl->client->Release();
				mImpl->client = nullptr;
			}
			if (mImpl->device) {
				mImpl->device->Release();
				mImpl->device = nullptr;
			}
			if (mImpl->enumerator) {
				mImpl->enumerator->Release();
				mImpl->enumerator = nullptr;
			}
			mImpl->ring.Free();
			if (mImpl->coInit) {
				CoUninitialize();
				mImpl->coInit = false;
			}
			mImpl->backend = "Null";
		}

#elif defined(NKENTSEU_PLATFORM_ANDROID)
			// =====================================================================
			//  Backend AAudio capture (Android 26+) — dlopen libaaudio.so (comme la
			//  lecture) + setDirection(INPUT). Data callback -> ring buffer + callback.
			// =====================================================================
			extern "C" {
			typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;
			typedef struct AAudioStreamStruct AAudioStream;
			typedef int32 aaudio_result_t;
			typedef int32 aaudio_data_callback_result_t;
			typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(AAudioStream *, void *, void *, int32);
			}
			static constexpr aaudio_result_t AAUDIO_OK_ = 0;
			static constexpr int32 AAUDIO_FORMAT_PCM_FLOAT_ = 2;
			static constexpr int32 AAUDIO_DIRECTION_INPUT_ = 1;
			static constexpr int32 AAUDIO_PERFORMANCE_LOW_LAT_ = 12;
			static constexpr int32 AAUDIO_SHARING_SHARED_ = 1;
			static constexpr aaudio_data_callback_result_t AAUDIO_CALLBACK_CONTINUE_ = 0;

			namespace {
				using PFN_createBuilder = aaudio_result_t (*)(AAudioStreamBuilder **);
				using PFN_setSampleRate = void (*)(AAudioStreamBuilder *, int32);
				using PFN_setChannelCount = void (*)(AAudioStreamBuilder *, int32);
				using PFN_setFormat = void (*)(AAudioStreamBuilder *, int32);
				using PFN_setDirection = void (*)(AAudioStreamBuilder *, int32);
				using PFN_setPerformance = void (*)(AAudioStreamBuilder *, int32);
				using PFN_setSharing = void (*)(AAudioStreamBuilder *, int32);
				using PFN_setDataCallback = void (*)(AAudioStreamBuilder *, AAudioStream_dataCallback, void *);
				using PFN_openStream = aaudio_result_t (*)(AAudioStreamBuilder *, AAudioStream **);
				using PFN_builderDelete = aaudio_result_t (*)(AAudioStreamBuilder *);
				using PFN_streamClose = aaudio_result_t (*)(AAudioStream *);
				using PFN_streamStart = aaudio_result_t (*)(AAudioStream *);
				using PFN_streamStop = aaudio_result_t (*)(AAudioStream *);
				using PFN_getSampleRate = int32 (*)(AAudioStream *);
				using PFN_getChannelCount = int32 (*)(AAudioStream *);

				struct CapAAudioFns {
						void *lib = nullptr;
						PFN_createBuilder createBuilder = nullptr;
						PFN_setSampleRate setSampleRate = nullptr;
						PFN_setChannelCount setChannelCount = nullptr;
						PFN_setFormat setFormat = nullptr;
						PFN_setDirection setDirection = nullptr;
						PFN_setPerformance setPerformance = nullptr;
						PFN_setSharing setSharing = nullptr;
						PFN_setDataCallback setDataCallback = nullptr;
						PFN_openStream openStream = nullptr;
						PFN_builderDelete builderDelete = nullptr;
						PFN_streamClose streamClose = nullptr;
						PFN_streamStart streamStart = nullptr;
						PFN_streamStop streamStop = nullptr;
						PFN_getSampleRate getSampleRate = nullptr;
						PFN_getChannelCount getChannelCount = nullptr;
						bool available = false;
				};
				static CapAAudioFns gCapAAudio;

				bool LoadCapAAudio() {
					if (gCapAAudio.lib)
						return gCapAAudio.available;
					if (android_get_device_api_level() < 26)
						return false;
					gCapAAudio.lib = dlopen("libaaudio.so", RTLD_NOW | RTLD_LOCAL);
					if (!gCapAAudio.lib)
						return false;
#define LOADC(name, sym) gCapAAudio.name = (PFN_##name)dlsym(gCapAAudio.lib, sym)
					LOADC(createBuilder, "AAudio_createStreamBuilder");
					LOADC(setSampleRate, "AAudioStreamBuilder_setSampleRate");
					LOADC(setChannelCount, "AAudioStreamBuilder_setChannelCount");
					LOADC(setFormat, "AAudioStreamBuilder_setFormat");
					LOADC(setDirection, "AAudioStreamBuilder_setDirection");
					LOADC(setPerformance, "AAudioStreamBuilder_setPerformanceMode");
					LOADC(setSharing, "AAudioStreamBuilder_setSharingMode");
					LOADC(setDataCallback, "AAudioStreamBuilder_setDataCallback");
					LOADC(openStream, "AAudioStreamBuilder_openStream");
					LOADC(builderDelete, "AAudioStreamBuilder_delete");
					LOADC(streamClose, "AAudioStream_close");
					LOADC(streamStart, "AAudioStream_requestStart");
					LOADC(streamStop, "AAudioStream_requestStop");
					LOADC(getSampleRate, "AAudioStream_getSampleRate");
					LOADC(getChannelCount, "AAudioStream_getChannelCount");
#undef LOADC
					gCapAAudio.available = (gCapAAudio.createBuilder && gCapAAudio.openStream &&
											gCapAAudio.streamStart && gCapAAudio.streamClose && gCapAAudio.setDirection);
					return gCapAAudio.available;
				}
			} // namespace

			// Data callback : audioData contient les frames CAPTUREES -> ring + callback.
			static aaudio_data_callback_result_t CaptureDataCbAAudio(AAudioStream *, void *userData, void *audioData,
																	 int32 numFrames) {
				NkAudioCapture::Impl *im = (NkAudioCapture::Impl *)userData;
				const int32 ch = im->cfg.channels > 0 ? im->cfg.channels : 1;
				im->ring.Write((const float32 *)audioData, (uint64)numFrames * (uint64)ch);
				if (im->hasCb)
					im->cb((const float32 *)audioData, numFrames, ch);
				return AAUDIO_CALLBACK_CONTINUE_;
			}

			NkVector<NkCaptureDeviceInfo> NkAudioCapture::EnumerateDevices() {
				NkVector<NkCaptureDeviceInfo> out;
				NkCaptureDeviceInfo info;
				info.name = NkString("default (AAudio)");
				info.isDefault = true;
				out.PushBack(info);
				return out;
			}

			bool NkAudioCapture::Open(const NkCaptureConfig &config) {
				Close();
				mImpl->cfg = config;
				if (mImpl->cfg.channels <= 0)
					mImpl->cfg.channels = 1;
				if (mImpl->cfg.sampleRate <= 0)
					mImpl->cfg.sampleRate = 48000;
				if (!LoadCapAAudio()) {
					logger.Warn("[NkAudioCapture] AAudio indisponible (API<26) -> Null");
					mImpl->backend = "Null";
					return false;
				}
				AAudioStreamBuilder *b = nullptr;
				if (gCapAAudio.createBuilder(&b) != AAUDIO_OK_)
					return false;
				gCapAAudio.setSampleRate(b, mImpl->cfg.sampleRate);
				gCapAAudio.setChannelCount(b, mImpl->cfg.channels);
				gCapAAudio.setFormat(b, AAUDIO_FORMAT_PCM_FLOAT_);
				gCapAAudio.setDirection(b, AAUDIO_DIRECTION_INPUT_); // CAPTURE
				gCapAAudio.setPerformance(b, AAUDIO_PERFORMANCE_LOW_LAT_);
				gCapAAudio.setSharing(b, AAUDIO_SHARING_SHARED_);
				gCapAAudio.setDataCallback(b, CaptureDataCbAAudio, mImpl);
				AAudioStream *s = nullptr;
				aaudio_result_t r = gCapAAudio.openStream(b, &s);
				gCapAAudio.builderDelete(b);
				if (r != AAUDIO_OK_ || !s) {
					logger.Error("[NkAudioCapture] AAudio openStream (INPUT) FAILED");
					return false;
				}
				mImpl->stream = s;
				mImpl->cfg.sampleRate = gCapAAudio.getSampleRate(s);
				mImpl->cfg.channels = gCapAAudio.getChannelCount(s);
				const int32 secs = mImpl->cfg.ringSeconds > 0 ? mImpl->cfg.ringSeconds : 4;
				mImpl->ring.Alloc((uint64)mImpl->cfg.sampleRate * (uint64)mImpl->cfg.channels * (uint64)secs);
				mImpl->backend = "AAudio";
				logger.Info("[NkAudioCapture] OK (AAudio capture, {0} Hz, {1} canal(aux))", mImpl->cfg.sampleRate,
							mImpl->cfg.channels);
				return true;
			}

			bool NkAudioCapture::Start() {
				if (!mImpl->stream || mImpl->capturing)
					return false;
				if (gCapAAudio.streamStart((AAudioStream *)mImpl->stream) != AAUDIO_OK_)
					return false;
				mImpl->capturing = true;
				return true;
			}

			void NkAudioCapture::Stop() {
				if (!mImpl->capturing)
					return;
				if (mImpl->stream)
					gCapAAudio.streamStop((AAudioStream *)mImpl->stream);
				mImpl->capturing = false;
			}

			void NkAudioCapture::Close() {
				if (!mImpl)
					return;
				Stop();
				if (mImpl->stream) {
					gCapAAudio.streamClose((AAudioStream *)mImpl->stream);
					mImpl->stream = nullptr;
				}
				mImpl->ring.Free();
				mImpl->backend = "Null";
			}

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
			// =====================================================================
			//  Backend getUserMedia capture (Web) — EM_JS : getUserMedia + Script-
			//  ProcessorNode.onaudioprocess -> C -> ring buffer. Un seul flux actif.
			// =====================================================================
			static NkAudioCapture::Impl *gWebCapActive = nullptr;

			// Appelé depuis JS avec un bloc mono capturé -> ring + callback.
			extern "C" EMSCRIPTEN_KEEPALIVE void NkWebMicPush(int frames, float *data) {
				if (!gWebCapActive || frames <= 0)
					return;
				const int32 ch = gWebCapActive->cfg.channels > 0 ? gWebCapActive->cfg.channels : 1;
				// Le JS fournit du mono ; on réplique sur ch canaux si besoin.
				if (ch == 1) {
					gWebCapActive->ring.Write(data, (uint64)frames);
					if (gWebCapActive->hasCb)
						gWebCapActive->cb(data, frames, 1);
				} else {
					for (int i = 0; i < frames; ++i) {
						float32 s = data[i];
						for (int32 c = 0; c < ch; ++c)
							gWebCapActive->ring.Write(&s, 1);
					}
				}
			}

			EM_JS(int, NkWebMic_JSInit, (int sampleRate, int bufferSize), {
				if (typeof navigator === 'undefined' || !navigator.mediaDevices)
					return 0;
				try {
					if (!Module._nkMicCtx)
						Module._nkMicCtx = new (window.AudioContext || window.webkitAudioContext)({sampleRate : sampleRate});
					var ctx = Module._nkMicCtx;
					navigator.mediaDevices.getUserMedia({audio : true, video : false}).then(function(stream) {
						var src = ctx.createMediaStreamSource(stream);
						var node = ctx.createScriptProcessor(bufferSize, 1, 1);
						node.onaudioprocess = function(e) {
							var input = e.inputBuffer.getChannelData(0);
							var n = input.length;
							var ptr = Module._malloc(n * 4);
							Module.HEAPF32.set(input, ptr >> 2);
							Module._NkWebMicPush(n, ptr);
							Module._free(ptr);
						};
						src.connect(node);
						node.connect(ctx.destination);
						Module._nkMicStream = stream;
						Module._nkMicNode = node;
					})["catch"](function(err) { console.error('getUserMedia refuse:', err); });
					return ctx.sampleRate | 0;
				} catch (e) {
					return 0;
				}
			});

			EM_JS(void, NkWebMic_JSStop, (), {
				if (Module._nkMicNode) {
					Module._nkMicNode.disconnect();
					Module._nkMicNode = null;
				}
				if (Module._nkMicStream) {
					Module._nkMicStream.getTracks().forEach(function(t) { t.stop(); });
					Module._nkMicStream = null;
				}
			});

			NkVector<NkCaptureDeviceInfo> NkAudioCapture::EnumerateDevices() {
				NkVector<NkCaptureDeviceInfo> out;
				NkCaptureDeviceInfo info;
				info.name = NkString("default (getUserMedia)");
				info.isDefault = true;
				out.PushBack(info);
				return out;
			}

			bool NkAudioCapture::Open(const NkCaptureConfig &config) {
				Close();
				mImpl->cfg = config;
				if (mImpl->cfg.channels <= 0)
					mImpl->cfg.channels = 1;
				if (mImpl->cfg.sampleRate <= 0)
					mImpl->cfg.sampleRate = 48000;
				const int actualSr = NkWebMic_JSInit(mImpl->cfg.sampleRate, 4096);
				if (actualSr <= 0) {
					logger.Warn("[NkAudioCapture] getUserMedia indisponible -> Null");
					mImpl->backend = "Null";
					return false;
				}
				mImpl->cfg.sampleRate = actualSr;
				const int32 secs = mImpl->cfg.ringSeconds > 0 ? mImpl->cfg.ringSeconds : 4;
				mImpl->ring.Alloc((uint64)mImpl->cfg.sampleRate * (uint64)mImpl->cfg.channels * (uint64)secs);
				gWebCapActive = mImpl;
				mImpl->backend = "getUserMedia";
				logger.Info("[NkAudioCapture] OK (getUserMedia, {0} Hz)", mImpl->cfg.sampleRate);
				return true;
			}

			bool NkAudioCapture::Start() {
				if (mImpl->capturing)
					return false;
				mImpl->capturing = true; // le flux JS pousse déjà dès l'autorisation
				return true;
			}

			void NkAudioCapture::Stop() {
				mImpl->capturing = false;
			}

			void NkAudioCapture::Close() {
				if (!mImpl)
					return;
				NkWebMic_JSStop();
				if (gWebCapActive == mImpl)
					gWebCapActive = nullptr;
				mImpl->capturing = false;
				mImpl->ring.Free();
				mImpl->backend = "Null";
			}

#elif defined(NKENTSEU_PLATFORM_HARMONYOS)
			// =====================================================================
			//  Backend OHAudio capture (HarmonyOS) — OH_AudioCapturer, format F32LE,
			//  read-data callback -> ring buffer + callback.
			// =====================================================================

			// Read-data callback : audioData = frames capturées, audioDataSize en OCTETS.
			// API STRUCT (OH_AudioCapturer_Callbacks, API 10, depreciee) et PAS
			// OH_AudioStreamBuilder_SetCapturerReadDataCallback (API 12) : la
			// libohaudio.so de l'emulateur NEXT (5.0.0.25) n'exporte pas les
			// setters modernes — le symbole manquant ferait echouer le dlopen
			// de TOUTE app qui tire NkAudioCapture.o (meme lecon que le backend
			// de rendu, NkAudioBackends.cpp).
			static int32_t CaptureReadDataOH(OH_AudioCapturer *, void *userData, void *audioData,
											 int32_t audioDataSize) {
				NkAudioCapture::Impl *im = (NkAudioCapture::Impl *)userData;
				const uint64 floats = (uint64)audioDataSize / sizeof(float32);
				im->ring.Write((const float32 *)audioData, floats);
				if (im->hasCb) {
					const int32 ch = im->cfg.channels > 0 ? im->cfg.channels : 1;
					im->cb((const float32 *)audioData, (int32)(floats / (uint64)ch), ch);
				}
				return 0;
			}

			NkVector<NkCaptureDeviceInfo> NkAudioCapture::EnumerateDevices() {
				NkVector<NkCaptureDeviceInfo> out;
				NkCaptureDeviceInfo info;
				info.name = NkString("default (OHAudio)");
				info.isDefault = true;
				out.PushBack(info);
				return out;
			}

			bool NkAudioCapture::Open(const NkCaptureConfig &config) {
				Close();
				mImpl->cfg = config;
				if (mImpl->cfg.channels <= 0)
					mImpl->cfg.channels = 1;
				if (mImpl->cfg.sampleRate <= 0)
					mImpl->cfg.sampleRate = 48000;

				OH_AudioStreamBuilder *b = nullptr;
				if (OH_AudioStreamBuilder_Create(&b, AUDIOSTREAM_TYPE_CAPTURER) != AUDIOSTREAM_SUCCESS || !b) {
					logger.Error("[NkAudioCapture] OH_AudioStreamBuilder_Create FAILED");
					return false;
				}
				OH_AudioStreamBuilder_SetSamplingRate(b, mImpl->cfg.sampleRate);
				OH_AudioStreamBuilder_SetChannelCount(b, mImpl->cfg.channels);
				OH_AudioStreamBuilder_SetSampleFormat(b, AUDIOSTREAM_SAMPLE_F32LE);
				OH_AudioStreamBuilder_SetCapturerInfo(b, AUDIOSTREAM_SOURCE_TYPE_MIC);
				OH_AudioCapturer_Callbacks ccbs;
				memset(&ccbs, 0, sizeof(ccbs));
				ccbs.OH_AudioCapturer_OnReadData = CaptureReadDataOH;
				OH_AudioStreamBuilder_SetCapturerCallback(b, ccbs, mImpl);

				OH_AudioCapturer *cap = nullptr;
				if (OH_AudioStreamBuilder_GenerateCapturer(b, &cap) != AUDIOSTREAM_SUCCESS || !cap) {
					logger.Error("[NkAudioCapture] OH_AudioStreamBuilder_GenerateCapturer FAILED");
					OH_AudioStreamBuilder_Destroy(b);
					return false;
				}
				mImpl->builder = b;
				mImpl->capturer = cap;

				const int32 secs = mImpl->cfg.ringSeconds > 0 ? mImpl->cfg.ringSeconds : 4;
				mImpl->ring.Alloc((uint64)mImpl->cfg.sampleRate * (uint64)mImpl->cfg.channels * (uint64)secs);
				mImpl->backend = "OHAudio";
				logger.Info("[NkAudioCapture] OK (OHAudio capture, {0} Hz, {1} canal(aux))", mImpl->cfg.sampleRate,
							mImpl->cfg.channels);
				return true;
			}

			bool NkAudioCapture::Start() {
				if (!mImpl->capturer || mImpl->capturing)
					return false;
				if (OH_AudioCapturer_Start((OH_AudioCapturer *)mImpl->capturer) != AUDIOSTREAM_SUCCESS)
					return false;
				mImpl->capturing = true;
				return true;
			}

			void NkAudioCapture::Stop() {
				if (!mImpl->capturing)
					return;
				if (mImpl->capturer)
					OH_AudioCapturer_Stop((OH_AudioCapturer *)mImpl->capturer);
				mImpl->capturing = false;
			}

			void NkAudioCapture::Close() {
				if (!mImpl)
					return;
				Stop();
				if (mImpl->capturer) {
					OH_AudioCapturer_Release((OH_AudioCapturer *)mImpl->capturer);
					mImpl->capturer = nullptr;
				}
				if (mImpl->builder) {
					OH_AudioStreamBuilder_Destroy((OH_AudioStreamBuilder *)mImpl->builder);
					mImpl->builder = nullptr;
				}
				mImpl->ring.Free();
				mImpl->backend = "Null";
			}

#elif defined(NKENTSEU_PLATFORM_LINUX) && !defined(NKENTSEU_PLATFORM_HARMONYOS)
			// =====================================================================
			//  Backend ALSA capture (Linux) — miroir du backend de lecture.
			// =====================================================================

			// Thread de capture : lit des frames FLOAT_LE interleaved (cfg.channels) via
			// snd_pcm_readi, les pousse dans le ring buffer + appelle le callback éventuel.
			static void *CaptureThreadProcAlsa(void *param) {
				NkAudioCapture::Impl *im = (NkAudioCapture::Impl *)param;
				snd_pcm_t *pcm = im->pcm;
				const int32 ch = im->cfg.channels > 0 ? im->cfg.channels : 1;
				const snd_pcm_uframes_t period = 1024;
				const uint64 tmpFloats = (uint64)period * (uint64)ch;
				float32 *tmp = (float32 *)memory::NkAlloc((size_t)(tmpFloats * sizeof(float32)));
				if (!tmp)
					return nullptr;

				while (im->running) {
					snd_pcm_sframes_t got = snd_pcm_readi(pcm, tmp, period);
					if (got < 0) {
						// XRUN (overrun -EPIPE) / suspendu : récupération silencieuse.
						snd_pcm_recover(pcm, (int)got, 1);
						continue;
					}
					if (got > 0) {
						const uint64 n = (uint64)got * (uint64)ch;
						im->ring.Write(tmp, n);
						if (im->hasCb)
							im->cb(tmp, (int32)got, ch);
					}
				}
				memory::NkFree(tmp);
				return nullptr;
			}

			NkVector<NkCaptureDeviceInfo> NkAudioCapture::EnumerateDevices() {
				NkVector<NkCaptureDeviceInfo> out;
				NkCaptureDeviceInfo info;
				info.name = NkString("default (ALSA)");
				info.isDefault = true;
				out.PushBack(info);
				return out;
			}

			bool NkAudioCapture::Open(const NkCaptureConfig &config) {
				Close();
				mImpl->cfg = config;
				if (mImpl->cfg.channels <= 0)
					mImpl->cfg.channels = 1;
				if (mImpl->cfg.sampleRate <= 0)
					mImpl->cfg.sampleRate = 48000;

				snd_pcm_t *pcm = nullptr;
				const char *devName = mImpl->cfg.deviceId.Empty() ? "default" : mImpl->cfg.deviceId.CStr();
				if (snd_pcm_open(&pcm, devName, SND_PCM_STREAM_CAPTURE, 0) < 0) {
					logger.Error("[NkAudioCapture] snd_pcm_open (capture) FAILED");
					return false;
				}

				snd_pcm_hw_params_t *hw = nullptr;
				snd_pcm_hw_params_alloca(&hw);
				snd_pcm_hw_params_any(pcm, hw);
				snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
				snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_FLOAT_LE);
				unsigned int rate = (unsigned int)mImpl->cfg.sampleRate;
				snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
				snd_pcm_hw_params_set_channels(pcm, hw, (unsigned int)mImpl->cfg.channels);
				snd_pcm_uframes_t period = 1024;
				snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);
				if (snd_pcm_hw_params(pcm, hw) < 0) {
					logger.Error("[NkAudioCapture] snd_pcm_hw_params (capture) FAILED");
					snd_pcm_close(pcm);
					return false;
				}
				snd_pcm_prepare(pcm);
				mImpl->pcm = pcm;
				mImpl->cfg.sampleRate = (int32)rate; // taux réellement négocié

				const int32 secs = mImpl->cfg.ringSeconds > 0 ? mImpl->cfg.ringSeconds : 4;
				mImpl->ring.Alloc((uint64)mImpl->cfg.sampleRate * (uint64)mImpl->cfg.channels * (uint64)secs);
				mImpl->backend = "ALSA";
				logger.Info("[NkAudioCapture] OK (ALSA capture, {0} Hz, {1} canal(aux))", mImpl->cfg.sampleRate,
							mImpl->cfg.channels);
				return true;
			}

			bool NkAudioCapture::Start() {
				if (!mImpl->pcm || mImpl->capturing)
					return false;
				mImpl->running = true;
				if (pthread_create(&mImpl->thread, nullptr, CaptureThreadProcAlsa, mImpl) != 0) {
					mImpl->running = false;
					return false;
				}
				mImpl->threadValid = true;
				mImpl->capturing = true;
				return true;
			}

			void NkAudioCapture::Stop() {
				if (!mImpl->capturing)
					return;
				mImpl->running = false;
				if (mImpl->threadValid) {
					pthread_join(mImpl->thread, nullptr);
					mImpl->threadValid = false;
				}
				mImpl->capturing = false;
			}

			void NkAudioCapture::Close() {
				if (!mImpl)
					return;
				Stop();
				if (mImpl->pcm) {
					snd_pcm_drop(mImpl->pcm);
					snd_pcm_close(mImpl->pcm);
					mImpl->pcm = nullptr;
				}
				mImpl->ring.Free();
				mImpl->backend = "Null";
			}

#else // ---- Backend Null (autres plateformes ; CoreAudio/AAudio à venir) ----

		NkVector<NkCaptureDeviceInfo> NkAudioCapture::EnumerateDevices() {
			return NkVector<NkCaptureDeviceInfo>();
		}

		bool NkAudioCapture::Open(const NkCaptureConfig &config) {
			mImpl->cfg = config;
			mImpl->backend = "Null";
			logger.Warn("[NkAudioCapture] backend de capture non implémenté sur cette plateforme (Null)");
			return false;
		}

		bool NkAudioCapture::Start() {
			return false;
		}

		void NkAudioCapture::Stop() {
		}

		void NkAudioCapture::Close() {
			if (mImpl)
				mImpl->ring.Free();
		}

#endif

		// =====================================================================
		//  Self-test HEADLESS du ring buffer (aucun périphérique requis)
		// =====================================================================
		bool NkAudioCapture::SelfTest() {
			NkCaptureRing r;
			r.Alloc(8); // 8 floats

			// 1) write 4, read 4 -> mêmes valeurs.
			{
				float32 in[4] = {1.f, 2.f, 3.f, 4.f};
				r.Write(in, 4);
				if (r.AvailableFloats() != 4)
					return false;
				float32 out[4] = {0, 0, 0, 0};
				if (r.Read(out, 4) != 4)
					return false;
				for (int i = 0; i < 4; ++i)
					if (out[i] != in[i])
						return false;
				if (r.AvailableFloats() != 0)
					return false;
			}

			// 2) wrap-around : écrire au-delà de la moitié puis relire dans l'ordre.
			{
				float32 a[6] = {10.f, 11.f, 12.f, 13.f, 14.f, 15.f};
				r.Write(a, 6); // wr avance, wrap dans le buffer de 8
				float32 out[6] = {0};
				if (r.Read(out, 6) != 6)
					return false;
				for (int i = 0; i < 6; ++i)
					if (out[i] != a[i])
						return false;
			}

			// 3) overflow : écrire 12 dans un buffer de 8 -> ne garde que 8 (drop du surplus).
			{
				float32 big[12];
				for (int i = 0; i < 12; ++i)
					big[i] = (float32)i;
				r.Write(big, 12);
				if (r.AvailableFloats() != 8)
					return false;
			}

			// 4) read plus que disponible -> ne lit que le disponible.
			{
				float32 out[100];
				const uint64 got = r.Read(out, 100);
				if (got != 8)
					return false;
			}

			r.Free();
			return true;
		}

	} // namespace audio
} // namespace nkentseu
