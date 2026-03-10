#pragma once
// NkNoopCameraBackend.h — Stub headless (serveurs, tests, plateformes non supportées)
#include "NKCamera/INkCameraBackend.h"
namespace nkentseu {
class NkNoopCameraBackend : public INkCameraBackend {
public:
    bool Init()     override { return true; }
    void Shutdown() override {}
    NkVector<NkCameraDevice> EnumerateDevices() override { return {}; }
    void SetHotPlugCallback(NkCameraHotPlugCallback) override {}
    bool StartStreaming(const NkCameraConfig&) override { mState = NkCameraState::NK_CAM_STATE_STREAMING; return true; }
    void StopStreaming() override { mState = NkCameraState::NK_CAM_STATE_CLOSED; }
    NkCameraState GetState() const override { return mState; }
    void SetFrameCallback(NkFrameCallback) override {}
    bool GetLastFrame(NkCameraFrame&) override { return false; }
    bool CapturePhoto(NkPhotoCaptureResult& r) override { r.success = false; r.errorMsg = "Noop"; return false; }
    bool CapturePhotoToFile(const NkString&) override { return false; }
    bool StartVideoRecord(const NkVideoRecordConfig& cfg) override {
        if (cfg.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY)
            mLastError = "IMAGE_SEQUENCE_ONLY mode is not implemented on NOOP backend";
        else
            mLastError = "Noop camera backend cannot record";
        return false;
    }
    void StopVideoRecord() override {}
    bool IsRecording() const override { return false; }
    float GetRecordingDurationSeconds() const override { return 0.f; }
    NkU32 GetWidth()  const override { return 0; }
    NkU32 GetHeight() const override { return 0; }
    NkU32 GetFPS()    const override { return 0; }
    NkPixelFormat GetFormat() const override { return NkPixelFormat::NK_PIXEL_UNKNOWN; }
    NkString GetLastError() const override {
        return mLastError.Empty() ? "Noop camera — no hardware" : mLastError;
    }
private:
    NkCameraState mState = NkCameraState::NK_CAM_STATE_CLOSED;
    NkString   mLastError;
};
}
