#pragma once
// =============================================================================
// NkCocoaCameraBackend.h — Capture caméra macOS via AVFoundation
//
// AVCaptureSession + AVCaptureVideoDataOutput pour le streaming.
// AVAssetWriter pour l'enregistrement vidéo H.264 en temps réel.
// =============================================================================

#include "NKCamera/INkCameraBackend.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#ifdef __OBJC__
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <AppKit/AppKit.h>

// ---------------------------------------------------------------------------
// Delegate Objective-C : reçoit les frames de AVCaptureVideoDataOutput
// ---------------------------------------------------------------------------
@class NkAVDelegate;

#endif // __OBJC__

namespace nkentseu
{

class NkCocoaCameraBackend : public INkCameraBackend
{
public:
    NkCocoaCameraBackend();
    ~NkCocoaCameraBackend() override;

    bool Init() override;
    void Shutdown() override;

    std::vector<NkCameraDevice> EnumerateDevices() override;
    void SetHotPlugCallback(NkCameraHotPlugCallback cb) override { mHotPlugCb = std::move(cb); }

    bool StartStreaming(const NkCameraConfig& config) override;
    void StopStreaming() override;
    NkCameraState GetState() const override { return mState; }
    void SetFrameCallback(NkFrameCallback cb) override { mFrameCb = std::move(cb); }
    bool GetLastFrame(NkCameraFrame& out) override;

    bool CapturePhoto(NkPhotoCaptureResult& res) override;
    bool CapturePhotoToFile(const std::string& path) override;

    bool StartVideoRecord(const NkVideoRecordConfig& config) override;
    void StopVideoRecord() override;
    bool IsRecording() const override;
    float GetRecordingDurationSeconds() const override;

    bool SetAutoFocus(bool e) override;
    bool SetTorch(bool e) override;
    bool SetZoom(float z) override;

    bool GetOrientation(NkCameraOrientation& out) override;

    NkU32         GetWidth()     const override { return mWidth;  }
    NkU32         GetHeight()    const override { return mHeight; }
    NkU32         GetFPS()       const override { return mFPS;    }
    NkPixelFormat GetFormat()    const override { return NkPixelFormat::NK_PIXEL_BGRA8; }
    std::string   GetLastError() const override { return mLastError; }

    // Appelé depuis le delegate ObjC
    void OnSampleBuffer(void* cmSampleBuffer);

private:
    NkCameraState   mState    = NkCameraState::NK_CAM_STATE_CLOSED;
    NkU32           mWidth    = 0, mHeight = 0, mFPS = 30;
    NkU32           mFrameIdx = 0;
    std::string     mLastError;

    std::mutex     mMutex;
    NkCameraFrame  mLastFrame;
    bool           mHasFrame = false;

    NkFrameCallback         mFrameCb;
    NkCameraHotPlugCallback mHotPlugCb;

    // ObjC objects (void* pour éviter d'inclure les headers ObjC ici)
    void* mSession       = nullptr;  // AVCaptureSession*
    void* mInput         = nullptr;  // AVCaptureDeviceInput*
    void* mOutput        = nullptr;  // AVCaptureVideoDataOutput*
    void* mDelegate      = nullptr;  // NkAVDelegate*
    void* mAssetWriter   = nullptr;  // AVAssetWriter*
    void* mAssetInput    = nullptr;  // AVAssetWriterInput*

    std::chrono::steady_clock::time_point mRecordStart;
    bool mRecording = false;
};

} // namespace nkentseu
