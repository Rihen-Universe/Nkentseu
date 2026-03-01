#pragma once

// =============================================================================
// NkWaylandDropImpl.h — Drag & Drop Wayland (wl_data_device)
//
// Protocole Wayland pour le drag & drop :
//   wl_data_device_manager → wl_data_device
//   wl_data_offer          → offre de données (MIME types disponibles)
//   wl_data_source         → source de données (côté émetteur)
//
// Types MIME supportés :
//   text/uri-list                 → fichiers
//   text/plain;charset=utf-8      → texte UTF-8
//   text/plain                    → texte fallback
// =============================================================================

#include <wayland-client.h>
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include "../../Events/NkDropEvent.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

// ---------------------------------------------------------------------------
// NkWaylandDropImpl
// ---------------------------------------------------------------------------
class NkWaylandDropImpl {
public:
	using DropFilesCallback = std::function<void(const NkDropFileData &)>;
	using DropTextCallback = std::function<void(const NkDropTextData &)>;
	using DropEnterCallback = std::function<void(const NkDropEnterData &)>;
	using DropLeaveCallback = std::function<void()>;

	NkWaylandDropImpl(wl_display *display, wl_seat *seat, wl_surface *surface)
		: mDisplay(display), mSeat(seat), mSurface(surface) {
		// wl_data_device_manager est obtenu via le registre
		// Le binding doit être fait en dehors ou passé en paramètre
	}

	~NkWaylandDropImpl() {
		if (mDataDevice) {
			wl_data_device_destroy(mDataDevice);
			mDataDevice = nullptr;
		}
	}

	void SetDataDeviceManager(wl_data_device_manager *mgr) {
		if (!mgr || !mSeat)
			return;
		mDataDevice = wl_data_device_manager_get_data_device(mgr, mSeat);
		wl_data_device_add_listener(mDataDevice, &kDataDeviceListener, this);
	}

	void SetDropFilesCallback(DropFilesCallback cb) {
		mDropFiles = std::move(cb);
	}
	void SetDropTextCallback(DropTextCallback cb) {
		mDropText = std::move(cb);
	}
	void SetDropEnterCallback(DropEnterCallback cb) {
		mDropEnter = std::move(cb);
	}
	void SetDropLeaveCallback(DropLeaveCallback cb) {
		mDropLeave = std::move(cb);
	}

private:
	wl_display *mDisplay = nullptr;
	wl_seat *mSeat = nullptr;
	wl_surface *mSurface = nullptr;
	wl_data_device *mDataDevice = nullptr;
	wl_data_offer *mOffer = nullptr;

	// Types MIME annoncés par l'offre courante
	std::vector<std::string> mMimeTypes;

	// Position courante du drag
	NkF32 mDragX = 0.f;
	NkF32 mDragY = 0.f;

	DropFilesCallback mDropFiles;
	DropTextCallback mDropText;
	DropEnterCallback mDropEnter;
	DropLeaveCallback mDropLeave;

	// -----------------------------------------------------------------------
	// Lecture de la sélection via un pipe
	// -----------------------------------------------------------------------
	std::string ReadOfferData(wl_data_offer *offer, const std::string &mime) {
		int pipefd[2];
		if (pipe(pipefd) < 0)
			return {};

		wl_data_offer_receive(offer, mime.c_str(), pipefd[1]);
		close(pipefd[1]);
		wl_display_flush(mDisplay);

		std::string result;
		char buf[4096];
		ssize_t n;
		while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
			result.append(buf, static_cast<size_t>(n));
		close(pipefd[0]);
		return result;
	}

	static std::vector<std::string> ParseUriList(const std::string &raw) {
		std::vector<std::string> result;
		std::string line;
		for (char c : raw) {
			if (c == '\n') {
				if (!line.empty() && line.back() == '\r')
					line.pop_back();
				if (!line.empty() && line[0] != '#') {
					if (line.substr(0, 7) == "file://") {
						std::string path = line.substr(7);
						std::string decoded;
						for (size_t i = 0; i < path.size(); ++i) {
							if (path[i] == '%' && i + 2 < path.size()) {
								int v = 0;
								sscanf(path.c_str() + i + 1, "%2x", &v);
								decoded += static_cast<char>(v);
								i += 2;
							} else
								decoded += path[i];
						}
						result.push_back(decoded);
					}
				}
				line.clear();
			} else
				line += c;
		}
		return result;
	}

	// -----------------------------------------------------------------------
	// Listeners wl_data_offer (annonce des types MIME disponibles)
	// -----------------------------------------------------------------------
	static void OnOfferOffer(void *data, wl_data_offer *, const char *mime_type) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		self->mMimeTypes.emplace_back(mime_type);
	}
	static void OnOfferSourceActions(void *, wl_data_offer *, uint32_t) {
	}
	static void OnOfferAction(void *, wl_data_offer *, uint32_t) {
	}

	static constexpr wl_data_offer_listener kOfferListener = {
		.offer = OnOfferOffer,
		.source_actions = OnOfferSourceActions,
		.action = OnOfferAction,
	};

	// -----------------------------------------------------------------------
	// Listeners wl_data_device
	// -----------------------------------------------------------------------
	static void OnDataDeviceDataOffer(void *data, wl_data_device *, wl_data_offer *offer) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		self->mOffer = offer;
		self->mMimeTypes.clear();
		wl_data_offer_add_listener(offer, &kOfferListener, self);
	}

	static void OnDataDeviceEnter(void *data, wl_data_device *, uint32_t /*serial*/, wl_surface *, wl_fixed_t x,
								  wl_fixed_t y, wl_data_offer *offer) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		self->mDragX = static_cast<NkF32>(wl_fixed_to_double(x));
		self->mDragY = static_cast<NkF32>(wl_fixed_to_double(y));
		self->mOffer = offer;

		if (self->mDropEnter) {
			NkDropEnterData d;
			d.x = self->mDragX;
			d.y = self->mDragY;
			d.dropType = NkDropType::NK_DROP_TYPE_FILE;
			self->mDropEnter(d);
		}

		// Accepter l'action copie
		wl_data_offer_set_actions(offer, WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
								  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
		wl_data_offer_accept(offer, 0, "text/uri-list");
	}

	static void OnDataDeviceLeave(void *data, wl_data_device *) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		if (self->mDropLeave)
			self->mDropLeave();
		if (self->mOffer) {
			wl_data_offer_destroy(self->mOffer);
			self->mOffer = nullptr;
		}
	}

	static void OnDataDeviceMotion(void *data, wl_data_device *, uint32_t, wl_fixed_t x, wl_fixed_t y) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		self->mDragX = static_cast<NkF32>(wl_fixed_to_double(x));
		self->mDragY = static_cast<NkF32>(wl_fixed_to_double(y));
	}

	static void OnDataDeviceDrop(void *data, wl_data_device *) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		if (!self->mOffer)
			return;

		bool hasUri = false;
		bool hasText = false;
		for (const auto &m : self->mMimeTypes) {
			if (m == "text/uri-list")
				hasUri = true;
			if (m == "text/plain;charset=utf-8" || m == "text/plain")
				hasText = true;
		}

		if (hasUri) {
			std::string raw = self->ReadOfferData(self->mOffer, "text/uri-list");
			auto paths = ParseUriList(raw);
			if (!paths.empty() && self->mDropFiles) {
				NkDropFileData fd;
				fd.x = self->mDragX;
				fd.y = self->mDragY;
				fd.numFiles = static_cast<NkU32>(paths.size());
				for (const auto &p : paths) {
					NkDropFilePath fp;
					strncpy(fp.path, p.c_str(), sizeof(fp.path) - 1);
					fd.files.push_back(fp);
				}
				self->mDropFiles(fd);
			}
		} else if (hasText && self->mDropText) {
			const std::string mime = (std::find(self->mMimeTypes.begin(), self->mMimeTypes.end(),
												"text/plain;charset=utf-8") != self->mMimeTypes.end())
										 ? "text/plain;charset=utf-8"
										 : "text/plain";
			std::string text = self->ReadOfferData(self->mOffer, mime);
			NkDropTextData td;
			td.x = self->mDragX;
			td.y = self->mDragY;
			td.text = text;
			self->mDropText(td);
		}

		wl_data_offer_finish(self->mOffer);
		wl_data_offer_destroy(self->mOffer);
		self->mOffer = nullptr;
	}

	static void OnDataDeviceSelection(void *data, wl_data_device *, wl_data_offer *offer) {
		auto *self = static_cast<NkWaylandDropImpl *>(data);
		// Clipboard : on ignore pour l'instant
		if (offer && offer != self->mOffer)
			wl_data_offer_destroy(offer);
	}

	static constexpr wl_data_device_listener kDataDeviceListener = {
		.data_offer = OnDataDeviceDataOffer,
		.enter = OnDataDeviceEnter,
		.leave = OnDataDeviceLeave,
		.motion = OnDataDeviceMotion,
		.drop = OnDataDeviceDrop,
		.selection = OnDataDeviceSelection,
	};
};

} // namespace nkentseu
