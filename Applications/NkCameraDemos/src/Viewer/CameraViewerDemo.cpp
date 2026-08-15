// =============================================================================
// CameraViewerDemo.cpp -- Visualiseur plein ecran d'une camera physique.
//
// Refonte OpenGL pur (glad) : aucune dependance sur NkTexture / NkSprite /
// NkIRenderer2D (cf bug NkTextureSetBackend non appele par les backends 2D
// dans NKCanvas, qui logge "No backend registered" a chaque LoadFromImage).
//
// Pipeline : GLContext + CameraGLTexture + FullscreenBlit, modele Pong.
//
// Raccourcis :
//   P     : prise de photo PNG (NkCameraSystem::SaveFrameToFile)
//   R     : demarre / arrete un enregistrement IMAGE_SEQUENCE_ONLY
//   Echap : quitte la demo
// =============================================================================

#include "Viewer/CameraViewerDemo.h"

#include "NKWindow/NKWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"
#include "NKLogger/NkLogHeartbeat.h"

#if defined(_WIN32)
// Déclaré à la main plutôt que d'inclure <windows.h> : même approche que
// NkRendererImpl.cpp, qui appelle déjà `timeBeginPeriod(1)` à son init.
extern "C" unsigned int __stdcall timeBeginPeriod(unsigned int uPeriod);
#endif

#include "NKCamera/NkCameraSystem.h"

#include "Render/GLContext.h"
#include "Render/CameraGLTexture.h"
#include "Render/FullscreenBlit.h"

namespace nkentseu {
	namespace cameradem {

		// ---------------------------------------------------------------------
		// Calcule un rect aspect-fit (en NDC) : place srcW x srcH dans la zone
		// [-1, +1] x [-1, +1] tout en preservant l'aspect-ratio source / fenetre.
		// Retourne (x, y, w, h) en NDC.
		// ---------------------------------------------------------------------
		struct NdcRect {
				float x, y, w, h;
		};

		static NdcRect AspectFitNdc(uint32 srcW, uint32 srcH, uint32 dstW, uint32 dstH) {
			if (srcW == 0 || srcH == 0 || dstW == 0 || dstH == 0) {
				return {-1.f, -1.f, 2.f, 2.f};
			}
			const float srcAR = (float)srcW / (float)srcH;
			const float dstAR = (float)dstW / (float)dstH;
			// Dimensions normalisees [0, 2] avant decalage.
			float ndcW = 2.f;
			float ndcH = 2.f;
			if (srcAR > dstAR) {
				// Source plus large : on remplit largeur, hauteur reduite.
				ndcH = 2.f * (dstAR / srcAR);
			} else {
				ndcW = 2.f * (srcAR / dstAR);
			}
			return {-ndcW * 0.5f, -ndcH * 0.5f, ndcW, ndcH};
		}

		// =====================================================================
		// Point d'entree de la demo viewer.
		// =====================================================================
		int RunCameraViewerDemo(const NkEntryState &state) {
			// -- Fenetre ------------------------------------------------------
			NkWindowConfig cfg;
			cfg.title = "NkCameraDemos -- Viewer";
			cfg.width = 1280;
			cfg.height = 720;
			cfg.centered = true;
			cfg.resizable = true;

			NkWindow window(cfg);
			if (!window.IsOpen()) {
				logger.Error("[Viewer] Echec creation fenetre");
				return -1;
			}

			// -- Contexte OpenGL (glad) --------------------------------------
			GLContext gl;
			if (!gl.Init(window)) {
				logger.Error("[Viewer] GLContext::Init a echoue");
				window.Close();
				return -2;
			}

			// -- Pipeline blit ------------------------------------------------
			FullscreenBlit blit;
			if (!blit.Initialize()) {
				logger.Error("[Viewer] FullscreenBlit::Initialize a echoue");
				gl.Shutdown();
				window.Close();
				return -3;
			}

			// -- Camera physique ---------------------------------------------
			auto &cam = NkCamera();
			if (!cam.Init()) {
				logger.Errorf("[Viewer] NkCamera::Init() echoue : %s", cam.GetLastError().CStr());
			}

			NkString deviceName = "(aucune camera)";
			{
				auto devices = cam.EnumerateDevices();
				logger.Infof("[Viewer] %u camera(s) detectee(s)", (uint32)devices.Size());
				for (usize i = 0; i < devices.Size(); ++i) {
					logger.Infof("  device[%u] = %s", devices[i].index, devices[i].name.CStr());
				}
				if (!devices.Empty())
					deviceName = devices[0].name;
			}

			NkCameraConfig camCfg;
			camCfg.deviceIndex = 0;
			camCfg.preset = NkCameraResolution::NK_CAM_RES_HD;
			camCfg.outputFormat = NkPixelFormat::NK_PIXEL_RGBA8;
			// MIROIR PAR DÉFAUT dans CE viewer, et c'est un choix d'application,
			// pas du moteur. Cette démo montre l'utilisateur à lui-même : la
			// convention qu'il connaît — visio, appareil photo — est le miroir,
			// et une image non miroitée lui paraît « inversée ». Rihen l'a
			// signalé deux fois ; la deuxième fois sur des fenêtres de mesure
			// lancées sans le drapeau, ce qui a montré que l'option n'était pas
			// le bon défaut ICI.
			//
			// Le moteur, lui, garde `flipHorizontal = false` : l'image brute est
			// géométriquement vraie, et c'est la seule utilisable pour l'AR, la
			// calibration ou la mesure. Un défaut d'application ne remonte pas
			// au module.
			//
			// `--brut` rend l'image telle que le capteur la livre.
			camCfg.flipHorizontal = true;
			for (usize i = 1; i < state.args.Size(); ++i) {
				if (state.args[i] == "--brut" || state.args[i] == "--raw")
					camCfg.flipHorizontal = false;
			}
			logger.Infof("[Viewer] Image %s.",
						 camCfg.flipHorizontal ? "MIROITEE (defaut ; --brut pour l'image du capteur)"
											   : "BRUTE, telle que livree par le capteur");
			if (!cam.StartStreaming(camCfg)) {
				logger.Warnf("[Viewer] StartStreaming a echoue : %s", cam.GetLastError().CStr());
			}

			// -- Ressources GPU pour le flux ---------------------------------
			CameraGLTexture streamTex;

			// -- Etat enregistrement -----------------------------------------
			bool recordActive = false;

			// -- Dimensions fenetre courantes -------------------------------
			uint32 winW = cfg.width;
			uint32 winH = cfg.height;

			// -- Battement de journal (ETEINT par defaut) ---------------------
			// `--beat=<ms>` fait dire au journal ce qui change PENDANT que
			// l'application vit. Sans lui, la salve de demarrage est tout ce
			// qu'on obtient, et un fichier qui ne bouge plus se confond avec un
			// fichier retenu — c'est exactement le faux diagnostic du 15/08.
			NkHeartbeat beat;
			uint32 beatFrames = 0;
			bool timerFin = false;
			for (usize i = 1; i < state.args.Size(); ++i) {
				const char *a = state.args[i].CStr();
				// ESSAI D'INTERVENTION (2026-08-15) : `--timer` force la
				// resolution de minuterie a 1 ms. Sert a tester si la variance
				// de debit (35-55 img/s) vient de la quantification du sommeil.
				if (a[0] == '-' && a[1] == '-' && a[2] == 't' && a[3] == 'i' && a[4] == 'm' &&
					a[5] == 'e' && a[6] == 'r' && a[7] == '\0') {
					timerFin = true;
					continue;
				}
				const char *prefix = "--beat=";
				usize k = 0;
				while (prefix[k] != '\0' && a[k] == prefix[k])
					++k;
				if (prefix[k] != '\0')
					continue;
				// Lecture des chiffres a la main : pas de conversion toute faite
				// dans NkStringUtils, et une valeur absente ou non numerique doit
				// laisser le battement ETEINT plutot que de choisir a notre place.
				uint32 ms = 0;
				bool anyDigit = false;
				for (const char *d = a + k; *d >= '0' && *d <= '9'; ++d) {
					ms = ms * 10u + (uint32)(*d - '0');
					anyDigit = true;
				}
				if (anyDigit) {
					beat.SetInterval(ms);
					logger.Infof("[Viewer] Battement du journal : une ligne toutes les %u ms.", ms);
				} else {
					logger.Warnf("[Viewer] --beat sans valeur numerique : battement laisse ETEINT.");
				}
			}

#if defined(_WIN32)
			if (timerFin) {
				timeBeginPeriod(1);
				logger.Infof("[Viewer] ESSAI : resolution de minuterie forcee a 1 ms.");
			}
#endif

			// -- Boucle principale -------------------------------------------
			auto &events = NkEvents();
			bool running = true;
			NkChrono chrono;

			while (running && window.IsOpen()) {
				// -- Evenements ------------------------------------------------
				while (NkEvent *ev = events.PollEvent()) {
					if (ev->Is<NkWindowCloseEvent>()) {
						running = false;
						break;
					}
					if (auto *re = ev->As<NkWindowResizeEvent>()) {
						const uint32 nw = re->GetWidth();
						const uint32 nh = re->GetHeight();
						if (nw > 0 && nh > 0 && (nw != winW || nh != winH)) {
							winW = nw;
							winH = nh;
							gl.OnResize(winW, winH);
						}
						continue;
					}
					if (auto *ke = ev->As<NkKeyPressEvent>()) {
						const NkKey key = ke->GetKey();
						if (key == NkKey::NK_ESCAPE) {
							running = false;
							break;
						}
						if (key == NkKey::NK_P) {
							// Photo : snapshot du dernier frame.
							NkCameraFrame snap;
							if (cam.GetLastFrame(snap)) {
								NkCameraSystem::ConvertToRGBA8(snap);
								NkString outPath = NkCameraSystem::GenerateAutoPath("photo", "png");
								if (NkCameraSystem::SaveFrameToFile(snap, outPath, 90)) {
									logger.Infof("[Viewer] Photo enregistree : %s", outPath.CStr());
								} else {
									logger.Warn("[Viewer] Echec sauvegarde photo");
								}
							} else {
								logger.Warn("[Viewer] Pas de frame pour la photo");
							}
						} else if (key == NkKey::NK_R) {
							// Toggle record IMAGE_SEQUENCE_ONLY (sequence PNG).
							if (!recordActive) {
								NkVideoRecordConfig vcfg;
								vcfg.mode = NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY;
								vcfg.outputPath = "capture/frame";
								if (cam.StartVideoRecord(vcfg)) {
									recordActive = true;
									logger.Info("[Viewer] Enregistrement IMAGE_SEQUENCE_ONLY demarre (prefixe = "
												"capture/frame)");
								} else {
									logger.Warnf("[Viewer] StartVideoRecord a echoue : %s", cam.GetLastError().CStr());
								}
							} else {
								cam.StopVideoRecord();
								recordActive = false;
								logger.Info("[Viewer] Enregistrement arrete");
							}
						}
					}
				}
				if (!running)
					break;

				// -- Recuperation et upload de la derniere frame --------------
				NkCameraFrame frame;
				bool hasFrame = cam.GetLastFrame(frame);
				if (hasFrame) {
					if (frame.format != NkPixelFormat::NK_PIXEL_RGBA8) {
						if (!NkCameraSystem::ConvertToRGBA8(frame)) {
							hasFrame = false;
						}
					}
				}
				if (hasFrame && frame.IsValid()) {
					if (!streamTex.IsValid() || streamTex.Width() != frame.width ||
						streamTex.Height() != frame.height) {
						streamTex.Shutdown();
						if (!streamTex.Create(frame.width, frame.height)) {
							logger.Errorf("[Viewer] Create texture %ux%u a echoue", frame.width, frame.height);
						}
					}
					if (streamTex.IsValid()) {
						streamTex.Update(frame.data.Data());
					}
				}

				// -- Rendu ----------------------------------------------------
				if (!gl.BeginFrame()) {
					NkChrono::Sleep(16.f);
					continue;
				}

				blit.Clear(0.07f, 0.07f, 0.12f);

				if (streamTex.IsValid()) {
					// Aspect-fit pour respecter le ratio de la camera dans la
					// fenetre courante.
					const NdcRect r = AspectFitNdc(streamTex.Width(), streamTex.Height(), winW, winH);
					blit.Draw(streamTex.Id(), r.x, r.y, r.w, r.h);
				}

				gl.EndFrame();
				gl.Present();

				// -- Battement -------------------------------------------------
				// Éteint par défaut : `--beat=<ms>` l'allume. Sans lui, le
				// journal est muet une fois le démarrage passé — ce qui est la
				// bonne valeur par défaut, mais rend l'enquête en direct
				// impossible : le fichier paraît figé alors qu'il n'a
				// simplement plus rien à dire.
				++beatFrames;
				if (beat.ShouldBeat()) {
					// Le débit se calcule sur l'intervalle MESURÉ, jamais sur
					// l'intervalle demandé : un battement ne peut se déclencher
					// qu'à une frontière d'image, donc il arrive toujours en
					// retard sur la demande. Diviser par la demande gonfle le
					// chiffre — 32 % d'erreur mesurés à 50 ms le 2026-08-15.
					const float64 ms = beat.GetLastIntervalMs();
					const float64 img = (ms > 0.0) ? (float64)beatFrames * 1000.0 / ms : 0.0;
					logger.Infof("[Viewer] %.1f img/s (%u images en %.0f ms mesurees), "
								 "flux %ux%u, texture %s",
								 img, beatFrames, ms, frame.width, frame.height,
								 streamTex.IsValid() ? "valide" : "ABSENTE");
					beatFrames = 0;
				}

				// Cap 60 fps.
				const auto elapsed = chrono.Elapsed();
				if (elapsed.milliseconds < 16) {
					NkChrono::Sleep(16 - elapsed.milliseconds);
				} else {
					NkChrono::YieldThread();
				}
				(void)chrono.Reset();
			}

			// -- Cleanup ordonne (ordre strict : textures -> blit -> gl -> window)
			if (recordActive)
				cam.StopVideoRecord();
			cam.StopStreaming();
			cam.Shutdown();

			streamTex.Shutdown();
			blit.Shutdown();
			gl.Shutdown();
			window.Close();

			(void)deviceName;
			return 0;
		}

	} // namespace cameradem
} // namespace nkentseu
