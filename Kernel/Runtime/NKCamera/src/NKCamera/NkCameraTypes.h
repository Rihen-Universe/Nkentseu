#pragma once
// =============================================================================
// NkCameraTypes.h — Types communs au système de capture caméra physique
// VERSION CORRIGÉE ET COMPLÈTE
// =============================================================================

#include "NKWindow/Core/NkTypes.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {

	// ---------------------------------------------------------------------------
	// NkPixelFormat
	// ---------------------------------------------------------------------------

	inline const char *NkCameraPixelFormatToString(NkPixelFormat f) {
		switch (f) {
			case NkPixelFormat::NK_PIXEL_RGBA8:
				return "RGBA8";
			case NkPixelFormat::NK_PIXEL_BGRA8:
				return "BGRA8";
			case NkPixelFormat::NK_PIXEL_RGB8:
				return "RGB8";
			case NkPixelFormat::NK_PIXEL_YUV420:
				return "YUV420";
			case NkPixelFormat::NK_PIXEL_NV12:
				return "NV12";
			case NkPixelFormat::NK_PIXEL_YUYV:
				return "YUYV";
			case NkPixelFormat::NK_PIXEL_MJPEG:
				return "MJPEG";
			default:
				return "UNKNOWN";
		}
	}

	// ---------------------------------------------------------------------------
	// NkColorRange — l'étendue réelle des composantes d'une image YUV
	// ---------------------------------------------------------------------------
	// Deux caméras peuvent livrer le MÊME format et deux signaux différents :
	// l'une étale ses composantes sur 0-255, l'autre les confine à 16-235. La
	// formule de déquantification n'est pas la même, et se tromper ne produit
	// aucune erreur — seulement des clairs brûlés et des sombres bouchés que
	// personne ne rattache à sa cause.
	//
	// TROIS états, et le troisième est le plus important. Un choix binaire
	// obligerait chaque producteur à mentir : celui qui ne déclare rien
	// recevrait silencieusement l'une des deux valeurs, et l'on remplacerait une
	// coïncidence par une autre. Avec INCONNUE, un producteur qui n'a pas
	// déclaré SE VOIT — il est journalisé une fois, jamais deviné.
	// Une valeur par défaut qui a l'air d'une mesure est pire que pas de valeur.
	enum class NkColorRange : uint32 {
		NK_COLOR_RANGE_UNKNOWN = 0, ///< Non déclarée. Signalée, jamais supposée.
		NK_COLOR_RANGE_FULL,        ///< 0-255 (dite « JPEG »). Y compris Android YUV_420_888.
		NK_COLOR_RANGE_VIDEO,       ///< 16-235 luma, 16-240 chroma (dite « TV »).
	};

	inline const char *NkColorRangeToString(NkColorRange r) {
		switch (r) {
			case NkColorRange::NK_COLOR_RANGE_FULL:
				return "PLEINE";
			case NkColorRange::NK_COLOR_RANGE_VIDEO:
				return "REDUITE";
			default:
				return "INCONNUE";
		}
	}

	// ---------------------------------------------------------------------------
	// NkCameraFacing
	// ---------------------------------------------------------------------------
	enum class NkCameraFacing : uint32 {
		NK_CAMERA_FACING_ANY = 0,
		NK_CAMERA_FACING_FRONT,
		NK_CAMERA_FACING_BACK,
		NK_CAMERA_FACING_EXTERNAL,
	};

	// ---------------------------------------------------------------------------
	// NkCameraResolution
	// ---------------------------------------------------------------------------
	enum class NkCameraResolution : uint32 {
		NK_CAM_RES_CUSTOM = 0,
		NK_CAM_RES_QVGA, ///<  320×240
		NK_CAM_RES_VGA,	 ///<  640×480
		NK_CAM_RES_HD,	 ///<  1280×720
		NK_CAM_RES_FHD,	 ///<  1920×1080
		NK_CAM_RES_4K,	 ///<  3840×2160
	};

	inline void NkResolutionToSize(NkCameraResolution r, uint32 &w, uint32 &h) {
		switch (r) {
			case NkCameraResolution::NK_CAM_RES_QVGA:
				w = 320;
				h = 240;
				break;
			case NkCameraResolution::NK_CAM_RES_VGA:
				w = 640;
				h = 480;
				break;
			case NkCameraResolution::NK_CAM_RES_HD:
				w = 1280;
				h = 720;
				break;
			case NkCameraResolution::NK_CAM_RES_FHD:
				w = 1920;
				h = 1080;
				break;
			case NkCameraResolution::NK_CAM_RES_4K:
				w = 3840;
				h = 2160;
				break;
			default:
				w = 640;
				h = 480;
				break;
		}
	}

	// ---------------------------------------------------------------------------
	// NkCameraDevice
	// ---------------------------------------------------------------------------
	struct NkCameraDevice {
			uint32 index = 0;
			NkString id;   ///< Identifiant OS unique (path Linux, GUID Win32, uniqueID iOS/macOS)
			NkString name; ///< Nom lisible
			NkCameraFacing facing = NkCameraFacing::NK_CAMERA_FACING_ANY;

			/// Inclinaison du CAPTEUR par rapport au haut naturel de l'appareil,
			/// en degrés et dans le sens horaire (0, 90, 180 ou 270).
			///
			/// Pourquoi ce champ existe : sur un téléphone, le capteur n'est
			/// presque jamais monté droit — un dos d'appareil Android rend
			/// habituellement 90. Sans cette information l'application reçoit une
			/// image COUCHÉE sans aucun moyen de le savoir : elle doit deviner,
			/// et une devinette juste sur un appareil est fausse sur le suivant.
			/// Sur poste de travail la valeur reste 0, une webcam étant montée
			/// droite. Constaté le 2026-08-12 sur Galaxy S22+.
			int32 sensorOrientation = 0;

			struct Mode {
					uint32 width = 0;
					uint32 height = 0;
					uint32 fps = 30;
					NkPixelFormat format = NkPixelFormat::NK_PIXEL_RGBA8;
			};

			NkVector<Mode> modes;

			bool IsValid() const {
				return !id.Empty();
			}

			NkString ToString() const {
				return NkString::Fmt("Camera[{0}] \"{1}\" facing={2} modes={3}", index, name, (uint32)facing,
									 (uint32)modes.Size());
			}
	};

	// ---------------------------------------------------------------------------
	// NkCameraConfig
	// ---------------------------------------------------------------------------
	struct NkCameraConfig {
			uint32 deviceIndex = 0;
			NkCameraResolution preset = NkCameraResolution::NK_CAM_RES_HD;
			uint32 width = 0;
			uint32 height = 0;
			uint32 fps = 30;
			// ⚠️ TROIS CHAMPS RETIRÉS LE 2026-08-15 — ne pas les réintroduire
			// sans les câbler d'abord :
			//
			//   `outputFormat` — 3 écrivains, 0 lecteur. Trois applications
			//     demandaient `NK_PIXEL_RGBA8` et recevaient du NV12 (Windows),
			//     YUV420 (Android), BGRA8 (Cocoa/UIKit) ou YUYV (Linux) : le
			//     format est IMPOSÉ par la plateforme, jamais choisi. Le format
			//     réellement livré se lit sur `NkCameraFrame::format`, et
			//     `NkCameraSystem::ConvertToRGBA8` fait la conversion — ce que
			//     les trois faisaient déjà, correctement.
			//   `autoExposure`, `autoWhiteBalance` — 0 lecteur, 0 écrivain.
			//     Utiliser `SetAutoExposure()` / `SetAutoWhiteBalance()`, qui
			//     fonctionnent.
			//
			// Un champ mort dans un en-tête public est une promesse que
			// quelqu'un finira par croire.
			NkCameraFacing facing = NkCameraFacing::NK_CAMERA_FACING_ANY;
			bool flipHorizontal = false;
			// ⚠️ Lu par le SEUL backend Android. Sur Windows, aucun code de mise
			// au point n'existe : le réglage y est ignoré en silence — invisible
			// sur une webcam à focale fixe, ce qui explique que personne ne l'ait
			// vu. Câbler `IAMCameraControl` avant de compter dessus.
			bool autoFocus = true;

			void Resolve() {
				if (preset != NkCameraResolution::NK_CAM_RES_CUSTOM)
					NkResolutionToSize(preset, width, height);
				if (width == 0)
					width = 640;
				if (height == 0)
					height = 480;
				if (fps == 0)
					fps = 30;
			}
	};

	// ---------------------------------------------------------------------------
	// NkCameraFrame
	// ---------------------------------------------------------------------------
	struct NkCameraFrame {
			uint32 width = 0;
			uint32 height = 0;
			NkPixelFormat format = NkPixelFormat::NK_PIXEL_RGBA8;
			uint64 timestampUs = 0;
			uint32 frameIndex = 0;
			uint32 stride = 0;
			// Étendue des composantes, DÉCLARÉE PAR LE PRODUCTEUR. Non renseignée
			// = INCONNUE, et c'est voulu : le défaut ne prétend rien. Cf.
			// NkColorRange, et NkCameraSystem::ConvertToRGBA8 qui choisit la
			// formule par la PLAGE, jamais par le format.
			NkColorRange range = NkColorRange::NK_COLOR_RANGE_UNKNOWN;
			// Miroir horizontal DEMANDÉ par la configuration, reporté ici par le
			// système pour que la conversion puisse l'appliquer. Faux par défaut :
			// l'image brute d'un capteur est géométriquement VRAIE, et c'est la
			// seule utilisable pour de l'AR, de la calibration ou de la mesure.
			// On ne miroite que pour un affichage de SOI, où l'habitude prime sur
			// la géométrie — et cela reste une demande explicite.
			bool flipHorizontal = false;
			NkVector<uint8> data;

			bool IsValid() const {
				return width > 0 && height > 0 && !data.Empty();
			}

			/// Accès pixel RGBA8 (uniquement si format == NK_PIXEL_RGBA8)
			uint32 GetPixelRGBA(uint32 x, uint32 y) const {
				if (x >= width || y >= height || format != NkPixelFormat::NK_PIXEL_RGBA8)
					return 0;
				const uint8 *p = data.Data() + y * stride + x * 4;
				return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3];
			}

			static uint32 DefaultStride(uint32 w, NkPixelFormat fmt) {
				switch (fmt) {
					case NkPixelFormat::NK_PIXEL_RGBA8:
					case NkPixelFormat::NK_PIXEL_BGRA8:
						return w * 4;
					case NkPixelFormat::NK_PIXEL_RGB8:
						return w * 3;
					default:
						return w * 4;
				}
			}
	};

	// ---------------------------------------------------------------------------
	// NkPhotoCaptureResult
	// ---------------------------------------------------------------------------
	struct NkPhotoCaptureResult {
			bool success = false;
			NkString errorMsg;
			NkCameraFrame frame;
			NkString savedPath;

			explicit operator bool() const {
				return success;
			}
	};

	// ---------------------------------------------------------------------------
	// NkVideoRecordConfig
	// ---------------------------------------------------------------------------
	struct NkVideoRecordConfig {
			enum class Mode : uint32 {
				AUTO = 0,			 // Choisit la meilleure voie (vidéo native, puis fallback)
				VIDEO_ONLY,			 // Force un enregistrement vidéo (échec si indisponible)
				IMAGE_SEQUENCE_ONLY, // Force un enregistrement image par image
			};

			NkString outputPath;
			uint32 bitrateBps = 4000000;
			uint32 audioSampleRate = 44100;
			bool captureAudio = false;
			NkString videoCodec = "h264";
			NkString audioCodec = "aac";
			NkString container = "mp4";
			Mode mode = Mode::AUTO;
	};

	// ---------------------------------------------------------------------------
	// NkCameraState
	// ---------------------------------------------------------------------------
	enum class NkCameraState : uint32 {
		NK_CAM_STATE_CLOSED = 0,
		NK_CAM_STATE_OPENING,
		NK_CAM_STATE_STREAMING,
		NK_CAM_STATE_RECORDING,
		NK_CAM_STATE_PAUSED,
		NK_CAM_STATE_ERROR,
	};

	// ---------------------------------------------------------------------------
	// Callbacks
	// ---------------------------------------------------------------------------
	using NkFrameCallback = NkFunction<void(const NkCameraFrame &)>;
	using NkCameraHotPlugCallback = NkFunction<void(const NkVector<NkCameraDevice> &)>;

	// ---------------------------------------------------------------------------
	// NkCameraOrientation — pour le mapping caméra virtuelle / caméra réelle
	// ---------------------------------------------------------------------------
	struct NkCameraOrientation {
			float yaw = 0.f;								///< Rotation autour de Y (gauche/droite), degrés
			float pitch = 0.f;								///< Rotation autour de X (haut/bas), degrés
			float roll = 0.f;								///< Rotation autour de Z (inclinaison), degrés
			float accelX = 0.f, accelY = 0.f, accelZ = 0.f; ///< Accéléromètre (m/s²)
	};

} // namespace nkentseu
