#pragma once

#include "NkEvent.h"
#include "NkEventState.h"

#include <queue>
#include <functional>
#include <memory>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace nkentseu {

    class IWindowImpl;

    using NkEventCallback = std::function<void(NkEvent*)>;

    class IEventImpl {
		public:
			virtual ~IEventImpl() = default;

			// Cycle de vie des fenêtres
			virtual void Initialize(IWindowImpl* owner, void* nativeHandle) = 0;
			virtual void Shutdown(void* nativeHandle) = 0;

			// Pompe d'événements
			virtual void PollEvents() = 0;

			// Queue FIFO d'événements
			virtual NkEvent* Front() const = 0;
			virtual void Pop() = 0;
			virtual bool IsEmpty() const = 0;
			virtual void PushEvent(std::unique_ptr<NkEvent> event) = 0;
			virtual std::size_t Size() const = 0;

			// Callbacks — global et par fenêtre (obligatoires)
			virtual void SetWindowCallback(void* nativeHandle, NkEventCallback cb) = 0;
			virtual void DispatchEvent(NkEvent* event, void* nativeHandle) = 0;

			// Callbacks optionnels — implémentation par défaut no-op
			virtual void SetGlobalCallback(NkEventCallback /*cb*/) {}
			virtual void AddTypedCallback(NkEventType::Value /*type*/, NkEventCallback /*cb*/) {}
			virtual void RemoveTypedCallback(NkEventType::Value /*type*/, NkEventCallback /*cb*/) {}
			virtual void ClearTypedCallbacks(NkEventType::Value /*type*/) {}
			virtual void ClearAllCallbacks() {}

			// État des entrées
			virtual const NkEventState& GetInputState() const { return mInputState; }
			virtual NkEventState& GetInputState() { return mInputState; }

			// Informations sur le backend
			virtual const char* GetPlatformName() const noexcept { return "Unknown"; }
			virtual bool IsReady() const noexcept { return true; }

		protected:
			std::queue<std::unique_ptr<NkEvent>> mQueue;
			NkEventState mInputState;
    };

} // namespace nkentseu
