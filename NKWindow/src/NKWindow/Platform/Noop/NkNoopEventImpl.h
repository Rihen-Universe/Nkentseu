#pragma once
// NkNoopEventImpl.h -- Backend evenements headless/CI (stub fonctionnel)
// Ne genere aucun evenement OS mais implemente completement IEventImpl.
#include "../../Events/IEventImpl.h"
#include <unordered_map>

namespace nkentseu {

class NkNoopEventImpl : public IEventImpl {
public:
	NkNoopEventImpl() = default;
	~NkNoopEventImpl() override = default;

	void Initialize(IWindowImpl*, void*) override {}
	void Shutdown(void*) override {}
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
	void SetWindowCallback(void*, NkEventCallback) override {}
	void DispatchEvent(NkEvent* event, void*) override {
		if (event && mGlobalCallback) mGlobalCallback(event);
	}

	const char* GetPlatformName() const noexcept override { return "Noop"; }
	bool IsReady() const noexcept override { return true; }

private:
	NkEventCallback mGlobalCallback;
};

} // namespace nkentseu
