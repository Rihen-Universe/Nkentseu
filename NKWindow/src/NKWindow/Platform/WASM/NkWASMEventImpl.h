#pragma once

// =============================================================================
// NkWASMEventImpl.h  —  Système d'événements WebAssembly / Emscripten
// =============================================================================

#include "../../Events/IEventImpl.h"
#include "../../Core/NkWindowConfig.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <unordered_map>

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Class NkWASMEventImpl.
 */
class NkWASMEventImpl : public IEventImpl {
public:
	NkWASMEventImpl();
	~NkWASMEventImpl() override;

	void Initialize(IWindowImpl *owner, void *nativeHandle) override;
	void Shutdown(void *nativeHandle) override;

	void PollEvents() override;
	NkEvent *Front() const override;
	void Pop() override;
	bool IsEmpty() const override;
	void PushEvent(std::unique_ptr<NkEvent> event) override;
	std::size_t Size() const override;

	void SetEventCallback(NkEventCallback cb) override;
	void SetWindowCallback(void *nativeHandle, NkEventCallback cb) override;
	void DispatchEvent(NkEvent *event, void *nativeHandle) override;

	const NkEventState& GetInputState() const override;
	NkEventState& GetInputState() override;
	const char* GetPlatformName() const noexcept override;
	bool IsReady() const noexcept override;

	static void SetInputOptions(const NkWebInputOptions &options);
	static NkWebInputOptions GetInputOptions();

private:
	static EM_BOOL OnKeyDown(int, const EmscriptenKeyboardEvent *, void *);
	static EM_BOOL OnKeyUp(int, const EmscriptenKeyboardEvent *, void *);
	static EM_BOOL OnMouseMove(int, const EmscriptenMouseEvent *, void *);
	static EM_BOOL OnMouseDown(int, const EmscriptenMouseEvent *, void *);
	static EM_BOOL OnMouseUp(int, const EmscriptenMouseEvent *, void *);
	static EM_BOOL OnWheel(int, const EmscriptenWheelEvent *, void *);
	static EM_BOOL OnTouchStart(int, const EmscriptenTouchEvent *, void *);
	static EM_BOOL OnTouchMove(int, const EmscriptenTouchEvent *, void *);
	static EM_BOOL OnTouchEnd(int, const EmscriptenTouchEvent *, void *);
	static EM_BOOL OnTouchCancel(int, const EmscriptenTouchEvent *, void *);

	void PushTouchEvent(const EmscriptenTouchEvent *te, NkTouchPhase phase);
	static NkKey DomVkToNkKey(unsigned long kc);

	/**
	 * @brief Struct WindowEntry.
	 */
	struct WindowEntry {
		IWindowImpl *window = nullptr;
		NkEventCallback callback;
	};
	std::unordered_map<void *, WindowEntry> mWindowMap;
	void *mPrimaryHandle = nullptr;
	NkEventCallback mGlobalCallback;
	static NkWASMEventImpl *sInstance;
};
} // namespace nkentseu
