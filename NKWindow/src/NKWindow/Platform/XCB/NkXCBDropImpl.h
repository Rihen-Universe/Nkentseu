#pragma once

// =============================================================================
// NkXCBDropImpl.h
// Drag & Drop XDND protocol pour XCB (X11 Drag-and-Drop version 5).
//
// Le protocol XDND fonctionne par échange de ClientMessage X11 :
//   XdndEnter    → annonce du type d'objet
//   XdndPosition → position courante
//   XdndDrop     → depot final → lecture via XdndSelection
//   XdndLeave    → annulation
//
// Types MIME supportés :
//   text/uri-list   → fichiers
//   text/plain;charset=utf-8 → texte
//   text/plain       → texte fallback
// =============================================================================

#include <xcb/xcb.h>
#include <string>
#include <vector>
#include <functional>
#include "../../Events/NkDropEvent.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/**
 * @brief Class NkXCBDropImpl.
 */
class NkXCBDropImpl {
public:
	using DropFilesCallback = std::function<void(const NkDropFileData &)>;
	using DropTextCallback = std::function<void(const NkDropTextData &)>;
	using DropEnterCallback = std::function<void(const NkDropEnterData &)>;
	using DropLeaveCallback = std::function<void()>;

	explicit NkXCBDropImpl(xcb_connection_t *conn, xcb_window_t win) : mConn(conn), mWin(win) {
		InitAtoms();
		SetXdndAware();
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

	/// À appeler dans le gestionnaire d'événements XCB sur XCB_CLIENT_MESSAGE.
	void HandleClientMessage(const xcb_client_message_event_t *ev);

	/// À appeler dans le gestionnaire sur XCB_SELECTION_NOTIFY.
	void HandleSelectionNotify(const xcb_selection_notify_event_t *ev);

private:
	xcb_connection_t *mConn = nullptr;
	xcb_window_t mWin = 0;

	// Atoms XDND
	xcb_atom_t aXdndAware = 0, aXdndEnter = 0, aXdndPosition = 0;
	xcb_atom_t aXdndDrop = 0, aXdndLeave = 0, aXdndFinished = 0;
	xcb_atom_t aXdndStatus = 0, aXdndSelection = 0;
	xcb_atom_t aUriList = 0, aTextPlain = 0, aTextPlainUtf8 = 0;
	xcb_atom_t aActionCopy = 0;

	xcb_window_t mSourceWin = 0;
	int32_t mDragX = 0, mDragY = 0;
	bool mHasDrop = false;

	DropFilesCallback mDropFiles;
	DropTextCallback mDropText;
	DropEnterCallback mDropEnter;
	DropLeaveCallback mDropLeave;

	xcb_atom_t GetAtom(const char *name) {
		xcb_intern_atom_cookie_t c = xcb_intern_atom(mConn, 0, static_cast<uint16_t>(strlen(name)), name);
		xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(mConn, c, nullptr);
		xcb_atom_t a = r ? r->atom : 0;
		free(r);
		return a;
	}

	void InitAtoms() {
		aXdndAware = GetAtom("XdndAware");
		aXdndEnter = GetAtom("XdndEnter");
		aXdndPosition = GetAtom("XdndPosition");
		aXdndDrop = GetAtom("XdndDrop");
		aXdndLeave = GetAtom("XdndLeave");
		aXdndFinished = GetAtom("XdndFinished");
		aXdndStatus = GetAtom("XdndStatus");
		aXdndSelection = GetAtom("XdndSelection");
		aUriList = GetAtom("text/uri-list");
		aTextPlain = GetAtom("text/plain");
		aTextPlainUtf8 = GetAtom("text/plain;charset=utf-8");
		aActionCopy = GetAtom("XdndActionCopy");
	}

	void SetXdndAware() {
		// Annonce XDND version 5
		uint32_t version = 5;
		xcb_change_property(mConn, XCB_PROP_MODE_REPLACE, mWin, aXdndAware, XCB_ATOM_ATOM, 32, 1, &version);
	}

	void SendStatus(bool accept) {
		xcb_client_message_event_t reply{};
		reply.response_type = XCB_CLIENT_MESSAGE;
		reply.format = 32;
		reply.window = mSourceWin;
		reply.type = aXdndStatus;
		reply.data.data32[0] = mWin;
		reply.data.data32[1] = accept ? 1 : 0; // bit 0 = accepté
		reply.data.data32[4] = accept ? aActionCopy : 0;
		xcb_send_event(mConn, 0, mSourceWin, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&reply));
		xcb_flush(mConn);
	}

	void SendFinished() {
		xcb_client_message_event_t reply{};
		reply.response_type = XCB_CLIENT_MESSAGE;
		reply.format = 32;
		reply.window = mSourceWin;
		reply.type = aXdndFinished;
		reply.data.data32[0] = mWin;
		reply.data.data32[1] = 1; // succès
		reply.data.data32[2] = aActionCopy;
		xcb_send_event(mConn, 0, mSourceWin, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&reply));
		xcb_flush(mConn);
	}

	void RequestSelection(xcb_atom_t type) {
		xcb_convert_selection(mConn, mWin, aXdndSelection, type, aXdndSelection, XCB_CURRENT_TIME);
		xcb_flush(mConn);
	}

	static std::vector<std::string> ParseUriList(const std::string &raw) {
		std::vector<std::string> result;
		std::istringstream ss(raw);
		std::string line;
		while (std::getline(ss, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			// Retirer \r
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			// file:// → chemin local
			if (line.substr(0, 7) == "file://") {
				std::string path = line.substr(7);
				// URL-decode basique %xx
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
		return result;
	}
};

inline void NkXCBDropImpl::HandleClientMessage(const xcb_client_message_event_t *ev) {
	if (ev->type == aXdndEnter) {
		mSourceWin = static_cast<xcb_window_t>(ev->data.data32[0]);
		NkDropEnterData d;
		d.x = 0;
		d.y = 0;
		d.dropType = NkDropType::NK_DROP_TYPE_FILE; // présumé
		if (mDropEnter)
			mDropEnter(d);
		SendStatus(true);
	} else if (ev->type == aXdndPosition) {
		mSourceWin = static_cast<xcb_window_t>(ev->data.data32[0]);
		uint32_t coords = ev->data.data32[2];
		mDragX = static_cast<int32_t>((coords >> 16) & 0xFFFF);
		mDragY = static_cast<int32_t>(coords & 0xFFFF);
		SendStatus(true);
	} else if (ev->type == aXdndLeave) {
		if (mDropLeave)
			mDropLeave();
	} else if (ev->type == aXdndDrop) {
		mSourceWin = static_cast<xcb_window_t>(ev->data.data32[0]);
		mHasDrop = true;
		// Demander la sélection (préférence : uri-list > text/plain;utf-8 > text/plain)
		RequestSelection(aUriList);
	}
}

inline void NkXCBDropImpl::HandleSelectionNotify(const xcb_selection_notify_event_t *ev) {
	if (!mHasDrop || ev->property == XCB_NONE) {
		SendFinished();
		return;
	}

	// Lire la propriété
	xcb_get_property_cookie_t c =
		xcb_get_property(mConn, 1, mWin, ev->property, XCB_GET_PROPERTY_TYPE_ANY, 0, 0x7FFFFFFF);
	xcb_get_property_reply_t *r = xcb_get_property_reply(mConn, c, nullptr);
	if (!r) {
		SendFinished();
		return;
	}

	std::string data(static_cast<char *>(xcb_get_property_value(r)),
					 static_cast<size_t>(xcb_get_property_value_length(r)));
	free(r);

	// Fichiers (text/uri-list)
	if (ev->target == aUriList) {
		auto paths = ParseUriList(data);
		if (!paths.empty() && mDropFiles) {
			NkDropFileData fd;
			fd.x = mDragX;
			fd.y = mDragY;
			fd.numFiles = static_cast<NkU32>(paths.size());
			for (const auto &p : paths) {
				NkDropFilePath fp;
				strncpy(fp.path, p.c_str(), sizeof(fp.path) - 1);
				fd.files.push_back(fp);
			}
			mDropFiles(fd);
		}
	}
	// Texte
	else if (ev->target == aTextPlainUtf8 || ev->target == aTextPlain) {
		if (mDropText) {
			NkDropTextData td;
			td.x = mDragX;
			td.y = mDragY;
			td.text = data;
			mDropText(td);
		}
	} else {
		// Essayer texte
		RequestSelection(aTextPlainUtf8);
		return;
	}

	mHasDrop = false;
	SendFinished();
}

} // namespace nkentseu
