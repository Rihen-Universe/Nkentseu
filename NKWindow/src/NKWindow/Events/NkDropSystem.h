#pragma once

// =============================================================================
// NkDropSystem.h
// Système Drag & Drop cross-platform.
//
// Sur chaque plateforme, l'EventImpl enregistre la fenêtre comme cible de
// drop et convertit les notifications OS en NkDropFileData / NkDropTextData
// qu'il insère dans la queue d'événements de NkWin32EventImpl / XCBEventImpl…
//
// Win32   : OLE IDropTarget + DragAcceptFiles (WM_DROPFILES fallback)
// XCB/XLib: XDND protocol (Motif / Freedesktop)
// Cocoa   : NSView registerForDraggedTypes
// UIKit   : UIDragInteraction + UIDropInteraction
// Android : IntentFilter ACTION_SEND / ACTION_VIEW
// WASM    : HTMLElement ondrop / DataTransfer API
// UWP     : DragDrop.Drop
// =============================================================================

#include "NkDropEvent.h"

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

/// Active le support drag&drop sur la fenêtre donnée (handle natif).
/// Appelé automatiquement par IEventImpl::Initialize si DropEnabled=true dans NkWindowConfig.
void NkEnableDropTarget(void *nativeHandle);

/// Désactive le support drag&drop.
void NkDisableDropTarget(void *nativeHandle);

} // namespace nkentseu
