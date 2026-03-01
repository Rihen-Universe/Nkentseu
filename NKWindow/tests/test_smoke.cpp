#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKWindow/Core/NkTypes.h"
#include "NKWindow/Core/NkEvents.h"
#include "NKWindow/Events/NkEventSystem.h"
#include "NKWindow/Events/IEventImpl.h"
#include "NKWindow/Events/NkKeyboardEvent.h"

#include <queue>

/**
 * @brief Internal anonymous namespace.
 */
// namespace {

/**
 * @brief Class TestEventImpl.
 */
// class TestEventImpl final : public nkentseu::IEventImpl {
// public:
// 	void Initialize(nkentseu::IWindowImpl * /*owner*/, void * /*nativeHandle*/) override {
// 	}
// 	void Shutdown(void * /*nativeHandle*/) override {
// 	}

// 	void PollEvents() override {
// 		++pollCalls;
// 		while (!mPending.empty()) {
// 			mQueue.push(mPending.front());
// 			mPending.pop();
// 		}
// 	}

// 	const nkentseu::NkEvent &Front() const override {
// 		return mQueue.empty() ? mDummyEvent : mQueue.front();
// 	}

// 	void Pop() override {
// 		if (!mQueue.empty())
// 			mQueue.pop();
// 	}

// 	bool IsEmpty() const override {
// 		return mQueue.empty();
// 	}
// 	void PushEvent(const nkentseu::NkEvent &event) override {
// 		mQueue.push(event);
// 	}
// 	std::size_t Size() const override {
// 		return mQueue.size();
// 	}

// 	void SetEventCallback(nkentseu::NkEventCallback cb) override {
// 		mGlobalCallback = std::move(cb);
// 	}
// 	void SetWindowCallback(void * /*nativeHandle*/, nkentseu::NkEventCallback cb) override {
// 		mWindowCallback = std::move(cb);
// 	}

// 	void DispatchEvent(nkentseu::NkEvent &event, void * /*nativeHandle*/) override {
// 		if (mGlobalCallback)
// 			mGlobalCallback(event);
// 		if (mWindowCallback)
// 			mWindowCallback(event);
// 	}

// 	void QueuePending(const nkentseu::NkEvent &event) {
// 		mPending.push(event);
// 	}

// 	int pollCalls = 0;

// private:
// 	std::queue<nkentseu::NkEvent> mPending;
// 	nkentseu::NkEventCallback mGlobalCallback;
// 	nkentseu::NkEventCallback mWindowCallback;
// };

// /**
//  * @brief Class ScopedEventImplAttach.
//  */
// class ScopedEventImplAttach {
// public:
// 	explicit ScopedEventImplAttach(TestEventImpl &impl) : mEs(nkentseu::EventSystem::Instance()), mImpl(impl) {
// 		mEs.AttachImpl(&mImpl);
// 	}

// 	~ScopedEventImplAttach() {
// 		mEs.DetachImpl(&mImpl);
// 	}

// private:
// 	nkentseu::EventSystem &mEs;
// 	TestEventImpl &mImpl;
// };

// } // namespace

// TEST_CASE(NKWindowSmoke, FixedWidthTypes) {
// 	ASSERT_EQUAL(1, static_cast<int>(sizeof(nkentseu::NkU8)));
// 	ASSERT_EQUAL(2, static_cast<int>(sizeof(nkentseu::NkU16)));
// 	ASSERT_EQUAL(4, static_cast<int>(sizeof(nkentseu::NkU32)));
// 	ASSERT_EQUAL(8, static_cast<int>(sizeof(nkentseu::NkU64)));
// }

// TEST_CASE(NKWindowSmoke, VecDefaults) {
// 	nkentseu::NkVec2u v;
// 	ASSERT_EQUAL(0, static_cast<int>(v.x));
// 	ASSERT_EQUAL(0, static_cast<int>(v.y));
// }

// TEST_CASE(NKWindowEvent, DropTextCopyIsDeep) {
// 	nkentseu::NkDropTextData d;
// 	d.text = "hello";
// 	d.mimeType = "text/plain";

// 	nkentseu::NkEvent src(d);
// 	nkentseu::NkEvent copy = src;

// 	ASSERT_NOT_NULL(src.dropText);
// 	ASSERT_NOT_NULL(copy.dropText);
// 	ASSERT_NOT_EQUAL(src.dropText, copy.dropText);
// 	ASSERT_EQUAL(std::string("hello"), copy.dropText->text);

// 	src.dropText->text = "changed";
// 	ASSERT_EQUAL(std::string("hello"), copy.dropText->text);
// }

// TEST_CASE(NKWindowEvent, DropTextPointerCtorClonesPayload) {
// 	nkentseu::NkDropTextData d;
// 	d.text = "payload";
// 	d.mimeType = "text/plain;charset=utf-8";

// 	nkentseu::NkEvent ev(&d);
// 	ASSERT_NOT_NULL(ev.dropText);
// 	ASSERT_NOT_EQUAL(&d, ev.dropText);
// 	ASSERT_EQUAL(std::string("payload"), ev.dropText->text);
// }

// TEST_CASE(NKWindowEventSystem, PollEventsIsCallbackOnly) {
// 	using namespace nkentseu;

// 	auto &es = EventSystem::Instance();
// 	TestEventImpl impl;
// 	ScopedEventImplAttach attached(impl);

// 	int keyPressCallbacks = 0;
// 	es.SetEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *) { ++keyPressCallbacks; });

// 	NkModifierState mods{};
// 	impl.QueuePending(NkEvent(NkKeyData(NkKey::NK_A, NkButtonState::NK_PRESSED, mods)));

// 	es.PollEvents();
// 	ASSERT_EQUAL(1, keyPressCallbacks);

// 	NkEvent out;
// 	ASSERT_FALSE(es.PollEvent(out));

// 	es.RemoveEventCallback<NkKeyPressEvent>();
// }

// TEST_CASE(NKWindowEventSystem, PollEventAutoPumpsAndBatches) {
// 	using namespace nkentseu;

// 	auto &es = EventSystem::Instance();
// 	TestEventImpl impl;
// 	ScopedEventImplAttach attached(impl);

// 	int keyPressCallbacks = 0;
// 	es.SetEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent *) { ++keyPressCallbacks; });

// 	NkModifierState mods{};
// 	impl.QueuePending(NkEvent(NkKeyData(NkKey::NK_A, NkButtonState::NK_PRESSED, mods)));
// 	impl.QueuePending(NkEvent(NkKeyData(NkKey::NK_B, NkButtonState::NK_PRESSED, mods)));

// 	NkEvent out;
// 	ASSERT_TRUE(es.PollEvent(out));
// 	ASSERT_EQUAL(NkEventType::NK_KEY_PRESS, out.type);
// 	ASSERT_EQUAL(NkKey::NK_A, out.data.key.key);

// 	ASSERT_TRUE(es.PollEvent(out));
// 	ASSERT_EQUAL(NkKey::NK_B, out.data.key.key);

// 	ASSERT_FALSE(es.PollEvent(out));
// 	ASSERT_EQUAL(1, impl.pollCalls);
// 	ASSERT_EQUAL(2, keyPressCallbacks);

// 	es.RemoveEventCallback<NkKeyPressEvent>();
// }

// TEST_CASE(NKWindowEventSystem, BaseTypedCallbacksReceiveDerivedEvents) {
// 	using namespace nkentseu;

// 	auto &es = EventSystem::Instance();
// 	TestEventImpl impl;
// 	ScopedEventImplAttach attached(impl);

// 	int keyBaseCallbacks = 0;
// 	int mouseBaseCallbacks = 0;
// 	es.SetEventCallback<NkKeyEvent>([&](NkKeyEvent *) { ++keyBaseCallbacks; });
// 	es.SetEventCallback<NkMouseButtonEvent>([&](NkMouseButtonEvent *) { ++mouseBaseCallbacks; });

// 	NkModifierState mods{};
// 	impl.QueuePending(NkEvent(NkKeyData(NkKey::NK_SPACE, NkButtonState::NK_PRESSED, mods)));

// 	NkMouseButtonData mb{};
// 	mb.button = NkMouseButton::NK_MB_LEFT;
// 	mb.state = NkButtonState::NK_PRESSED;
// 	mb.x = 12;
// 	mb.y = 34;
// 	impl.QueuePending(NkEvent(mb));

// 	NkEvent out;
// 	while (es.PollEvent(out)) {
// 	}

// 	ASSERT_EQUAL(1, keyBaseCallbacks);
// 	ASSERT_EQUAL(1, mouseBaseCallbacks);

// 	es.RemoveEventCallback<NkKeyEvent>();
// 	es.RemoveEventCallback<NkMouseButtonEvent>();
// }

// TEST_CASE(NKWindowKeycodeMap, X11KeycodeBasicMappings) {
// 	using namespace nkentseu;

// 	// Valeurs X11 courantes (xev): Esc=9, A=38, Space=65.
// 	ASSERT_EQUAL(NkKey::NK_ESCAPE, NkKeycodeMap::NkKeyFromX11Keycode(9));
// 	ASSERT_EQUAL(NkKey::NK_A, NkKeycodeMap::NkKeyFromX11Keycode(38));
// 	ASSERT_EQUAL(NkKey::NK_SPACE, NkKeycodeMap::NkKeyFromX11Keycode(65));
// }