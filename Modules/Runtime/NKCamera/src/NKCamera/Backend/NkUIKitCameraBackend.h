#pragma once
// NkUIKitCameraBackend.h — iOS AVFoundation + CMMotionManager IMU
#include "NKCamera/INkCameraBackend.h"
#include "NKTime/NkChrono.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <string>
#include <vector>

namespace nkentseu {

class NkUIKitCameraBackend : public INkCameraBackend {
public:
    NkUIKitCameraBackend();
    ~NkUIKitCameraBackend() override;

    bool Init() override;
    void Shutdown() override;

    NkVector<NkCameraDevice> EnumerateDevices() override;
    void SetHotPlugCallback(NkCameraHotPlugCallback cb) override { mHotPlugCb=std::move(cb); }

    bool StartStreaming(const NkCameraConfig& c) override;
    void StopStreaming() override;
    NkCameraState GetState() const override { return mState; }
    void SetFrameCallback(NkFrameCallback cb) override { mFrameCb=std::move(cb); }
    bool GetLastFrame(NkCameraFrame& out) override;

    bool CapturePhoto(NkPhotoCaptureResult& res) override;
    bool CapturePhotoToFile(const NkString& path) override;
    bool StartVideoRecord(const NkVideoRecordConfig& cfg) override;
    void StopVideoRecord() override;
    bool  IsRecording() const override;
    float GetRecordingDurationSeconds() const override;

    bool SetAutoFocus(bool e) override;
    bool SetAutoExposure(bool e) override;
    bool SetAutoWhiteBalance(bool e) override;
    bool SetZoom(float z) override;
    bool SetFlash(bool e) override { (void)e; return false; }
    bool SetTorch(bool e) override;
    bool SetFocusPoint(float x,float y) override;
    bool GetOrientation(NkCameraOrientation& out) override;

    uint32 GetWidth() const override { return mWidth; }
    uint32 GetHeight() const override { return mHeight; }
    uint32 GetFPS() const override { return mFPS; }
    NkPixelFormat GetFormat() const override { return NkPixelFormat::NK_PIXEL_BGRA8; }
    NkString GetLastError() const override { return mLastError; }

    // Appelé depuis le delegate ObjC
    void _OnVideoFrame(void* pixelBuffer, uint64 ts);
    void _OnPhotoCapture(void* data, size_t len, bool ok, const char* err);

private:
    void* mPriv; // UIKitPrivate* (Pimpl pour éviter headers ObjC)

    NkCameraState mState    = NkCameraState::NK_CAM_STATE_CLOSED;
    uint32         mWidth=0, mHeight=0, mFPS=30, mFrameIdx=0;
    NkString   mLastError;
    bool          mRecording=false;
    uint64         mFirstFrameTime=0;

    std::mutex    mMutex;
    NkCameraFrame mLastFrame;
    bool          mHasFrame=false;

    std::mutex              mPhotoMutex;
    std::condition_variable mPhotoCv;
    NkPhotoCaptureResult    mPhotoPending;
    bool                    mPhotoReady=false;

    NkFrameCallback         mFrameCb;
    NkCameraHotPlugCallback mHotPlugCb;

    NkElapsedTime mRecordStart;
};

} // namespace nkentseu
