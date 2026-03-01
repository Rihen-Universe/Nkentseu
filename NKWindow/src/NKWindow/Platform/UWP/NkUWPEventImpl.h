#pragma once
// NkUWPEventImpl.h -- Backend evenements UWP/Xbox
#include "../../Events/IEventImpl.h"
#include <unordered_map>

namespace nkentseu {

class NkUWPEventImpl : public IEventImpl {
public:
	NkUWPEventImpl() = default;
	~NkUWPEventImpl() override = default;

	void Initialize(IWindowImpl*, void*) override { mReady = true; }
	void Shutdown(void*) override { mReady = false; }
	void PollEvents() override {}

	NkEvent* Front() const override {
		return mQueue.empty() ? nullptr : mQueue.front().get();
	}
	void Pop() override {
		if (!mQueue.empty()) mQueue.pop();
	}
	bool IsEmpty() const override { return mQueue.empty(); }
	void PushEvent(std::unique_ptr<NkEvent> event) override {
		if (event) mQueue.push(std::move(event));
	}
	std::size_t Size() const override { return mQueue.size(); }

	void SetEventCallback(NkEventCallback cb) override { mGlobalCallback = std::move(cb); }
	void SetWindowCallback(void* handle, NkEventCallback cb) override {
		mWindowCallbacks[handle] = std::move(cb);
	}
	void DispatchEvent(NkEvent* event, void* handle) override {
		if (!event) return;
		auto it = mWindowCallbacks.find(handle);
		if (it != mWindowCallbacks.end() && it->second) it->second(event);
		if (mGlobalCallback) mGlobalCallback(event);
	}

	const char* GetPlatformName() const noexcept override { return "UWP"; }
	bool IsReady() const noexcept override { return true; }

private:
	bool mReady = false;
	NkEventCallback mGlobalCallback;
	std::unordered_map<void*, NkEventCallback> mWindowCallbacks;
};

} // namespace nkentseu
