#pragma once
// NkNoopGamepadBackend.h — stub headless
#include "../../Core/NkGamepadSystem.h"
#include <array>
/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {
/**
 * @brief Class NkNoopGamepadBackend.
 */
class NkNoopGamepadBackend : public INkGamepadBackend {
public:
	bool Init() override {
		return true;
	}
	void Shutdown() override {
	}
	void Poll() override {
	}
	NkU32 GetConnectedCount() const override {
		return 0;
	}
	const NkGamepadInfo &GetInfo(NkU32) const override {
		static NkGamepadInfo d;
		return d;
	}
	const NkGamepadStateData &GetState(NkU32) const override {
		static NkGamepadStateData d;
		return d;
	}
	void Rumble(NkU32, float, float, float, float, NkU32) override {
	}
};
} // namespace nkentseu
