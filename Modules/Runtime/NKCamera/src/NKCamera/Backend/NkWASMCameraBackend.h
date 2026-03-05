#pragma once
// =============================================================================
// NkWASMCameraBackend.h
//
// Current WASM backend strategy:
// - Keep the API valid and compile-safe on Emscripten.
// - Return graceful "not available" results for camera-specific operations.
// - Let sandbox examples run in fallback/virtual mode on Web.
//
// This avoids brittle EM_ASM camera glue issues while preserving cross-platform
// build stability. Camera validation is done on native mobile/desktop backends.
// =============================================================================

#include "NKCamera/INkCameraBackend.h"

namespace nkentseu
{

class NkWASMCameraBackend : public INkCameraBackend
{
public:
    NkWASMCameraBackend() = default;
    ~NkWASMCameraBackend() override { Shutdown(); }

    bool Init() override
    {
        mLastError.clear();
        return true;
    }

    void Shutdown() override
    {
        StopStreaming();
    }

    std::vector<NkCameraDevice> EnumerateDevices() override
    {
        return {};
    }

    void SetHotPlugCallback(NkCameraHotPlugCallback cb) override
    {
        mHotPlugCb = std::move(cb);
    }

    bool StartStreaming(const NkCameraConfig&) override
    {
        mState = NkCameraState::NK_CAM_STATE_ERROR;
        mLastError = "WASM camera backend is currently disabled";
        return false;
    }

    void StopStreaming() override
    {
        StopVideoRecord();
        mState = NkCameraState::NK_CAM_STATE_CLOSED;
    }

    NkCameraState GetState() const override { return mState; }

    void SetFrameCallback(NkFrameCallback cb) override
    {
        mFrameCb = std::move(cb);
    }

    bool GetLastFrame(NkCameraFrame&) override
    {
        return false;
    }

    bool CapturePhoto(NkPhotoCaptureResult& out) override
    {
        out.success = false;
        out.errorMsg = "WASM camera backend is currently disabled";
        return false;
    }

    bool CapturePhotoToFile(const std::string&) override
    {
        mLastError = "WASM camera backend is currently disabled";
        return false;
    }

    bool StartVideoRecord(const NkVideoRecordConfig&) override
    {
        mLastError = "WASM camera backend is currently disabled";
        return false;
    }

    void StopVideoRecord() override
    {
        mRecording = false;
        if (mState == NkCameraState::NK_CAM_STATE_RECORDING)
            mState = NkCameraState::NK_CAM_STATE_CLOSED;
    }

    bool IsRecording() const override
    {
        return mRecording;
    }

    float GetRecordingDurationSeconds() const override
    {
        return 0.f;
    }

    bool GetOrientation(NkCameraOrientation&) override
    {
        return false;
    }

    NkU32 GetWidth() const override { return 0; }
    NkU32 GetHeight() const override { return 0; }
    NkU32 GetFPS() const override { return 0; }
    NkPixelFormat GetFormat() const override { return NkPixelFormat::NK_PIXEL_UNKNOWN; }

    std::string GetLastError() const override
    {
        return mLastError.empty()
             ? std::string("WASM camera backend is currently disabled")
             : mLastError;
    }

private:
    NkCameraState mState = NkCameraState::NK_CAM_STATE_CLOSED;
    bool mRecording = false;
    std::string mLastError;
    NkFrameCallback mFrameCb;
    NkCameraHotPlugCallback mHotPlugCb;
};

} // namespace nkentseu
