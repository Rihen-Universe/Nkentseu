// =============================================================================
// NkCocoaCameraBackend.mm — Implementation AVFoundation macOS
// =============================================================================

#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>

#include "NkCocoaCameraBackend.h"
#include "NKCamera/NkCameraSystem.h"

// ---------------------------------------------------------------------------
// Delegate ObjC qui reçoit les frames
// ---------------------------------------------------------------------------

@interface NkAVDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (nonatomic, assign) nkentseu::NkCocoaCameraBackend* backend;
@end

@implementation NkAVDelegate
- (void)captureOutput:(AVCaptureOutput*)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection*)connection
{
    if (self.backend) self.backend->OnSampleBuffer((__bridge void*)sampleBuffer);
}
@end

namespace nkentseu
{

NkCocoaCameraBackend::NkCocoaCameraBackend() = default;
NkCocoaCameraBackend::~NkCocoaCameraBackend() { Shutdown(); }

bool NkCocoaCameraBackend::Init() { return true; }

void NkCocoaCameraBackend::Shutdown() { StopStreaming(); }

// ---------------------------------------------------------------------------

std::vector<NkCameraDevice> NkCocoaCameraBackend::EnumerateDevices()
{
    std::vector<NkCameraDevice> result;
    NSArray<AVCaptureDevice*>* devs = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    NkU32 idx = 0;
    for (AVCaptureDevice* d in devs)
    {
        NkCameraDevice dev;
        dev.index = idx++;
        dev.id    = [[d uniqueID] UTF8String];
        dev.name  = [[d localizedName] UTF8String];
        if (d.position == AVCaptureDevicePositionFront)
            dev.facing = NkCameraFacing::NK_CAMERA_FACING_FRONT;
        else if (d.position == AVCaptureDevicePositionBack)
            dev.facing = NkCameraFacing::NK_CAMERA_FACING_BACK;
        else
            dev.facing = NkCameraFacing::NK_CAMERA_FACING_EXTERNAL;

        // Modes
        for (AVCaptureDeviceFormat* fmt in d.formats)
        {
            CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(fmt.formatDescription);
            NkCameraDevice::Mode mode;
            mode.width  = static_cast<NkU32>(dim.width);
            mode.height = static_cast<NkU32>(dim.height);
            mode.fps    = 30;
            mode.format = NkPixelFormat::NK_PIXEL_BGRA8;
            if (mode.width > 0 && mode.height > 0) dev.modes.push_back(mode);
        }
        result.push_back(dev);
    }
    return result;
}

// ---------------------------------------------------------------------------

bool NkCocoaCameraBackend::StartStreaming(const NkCameraConfig& config)
{
    // Sélectionner le device
    NSArray<AVCaptureDevice*>* devs = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    if (config.deviceIndex >= (NkU32)devs.count) { mLastError = "Device index out of range"; return false; }
    AVCaptureDevice* device = devs[config.deviceIndex];

    NSError* err = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&err];
    if (!input) { mLastError = [[err localizedDescription] UTF8String]; return false; }

    // Session
    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    [session beginConfiguration];
    [session addInput:input];

    // Output BGRA
    AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
    output.videoSettings = @{
        (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
    };
    output.alwaysDiscardsLateVideoFrames = YES;

    NkAVDelegate* delegate = [[NkAVDelegate alloc] init];
    delegate.backend = this;
    dispatch_queue_t q = dispatch_queue_create("nk.camera.capture", DISPATCH_QUEUE_SERIAL);
    [output setSampleBufferDelegate:delegate queue:q];

    [session addOutput:output];
    [session commitConfiguration];
    [session startRunning];

    mSession  = const_cast<void*>(CFBridgingRetain(session));
    mInput    = const_cast<void*>(CFBridgingRetain(input));
    mOutput   = const_cast<void*>(CFBridgingRetain(output));
    mDelegate = const_cast<void*>(CFBridgingRetain(delegate));

    mWidth  = config.width;
    mHeight = config.height;
    mFPS    = config.fps;
    mState  = NkCameraState::NK_CAM_STATE_STREAMING;
    return true;
}

void NkCocoaCameraBackend::StopStreaming()
{
    StopVideoRecord();
    if (mSession)
    {
        AVCaptureSession* session = (__bridge AVCaptureSession*)mSession;
        [session stopRunning];
        CFBridgingRelease(mSession);
        mSession = nullptr;
    }
    mState = NkCameraState::NK_CAM_STATE_CLOSED;
}

// ---------------------------------------------------------------------------

void NkCocoaCameraBackend::OnSampleBuffer(void* sampleBuf)
{
    CMSampleBufferRef sb = (__bridge CMSampleBufferRef)sampleBuf;
    CVImageBufferRef  ib = CMSampleBufferGetImageBuffer(sb);
    if (!ib) return;

    CVPixelBufferLockBaseAddress(ib, kCVPixelBufferLock_ReadOnly);
    NkU8*  ptr    = static_cast<NkU8*>(CVPixelBufferGetBaseAddress(ib));
    size_t stride = CVPixelBufferGetBytesPerRow(ib);
    size_t w      = CVPixelBufferGetWidth(ib);
    size_t h      = CVPixelBufferGetHeight(ib);

    NkCameraFrame frame;
    frame.width      = static_cast<NkU32>(w);
    frame.height     = static_cast<NkU32>(h);
    frame.stride     = static_cast<NkU32>(stride);
    frame.format     = NkPixelFormat::NK_PIXEL_BGRA8;
    frame.frameIndex = mFrameIdx++;
    frame.data.resize(stride * h);
    memcpy(frame.data.data(), ptr, stride * h);

    CVPixelBufferUnlockBaseAddress(ib, kCVPixelBufferLock_ReadOnly);

    {
        std::lock_guard<std::mutex> lk(mMutex);
        mLastFrame = frame;
        mHasFrame  = true;
    }
    if (mFrameCb) mFrameCb(frame);
}

bool NkCocoaCameraBackend::GetLastFrame(NkCameraFrame& out)
{
    std::lock_guard<std::mutex> lk(mMutex);
    if (!mHasFrame) return false;
    out = mLastFrame;
    return true;
}

bool NkCocoaCameraBackend::CapturePhoto(NkPhotoCaptureResult& res)
{
    std::lock_guard<std::mutex> lk(mMutex);
    if (!mHasFrame) { res.success = false; return false; }
    res.frame = mLastFrame; res.success = true;
    return true;
}

bool NkCocoaCameraBackend::CapturePhotoToFile(const std::string& path)
{
    NkPhotoCaptureResult res;
    if (!CapturePhoto(res)) return false;
    return NkCameraSystem::SaveFrameToFile(res.frame, path);
}

bool NkCocoaCameraBackend::StartVideoRecord(const NkVideoRecordConfig& config)
{
    if (config.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY)
    {
        mLastError = "IMAGE_SEQUENCE_ONLY mode is not implemented on Cocoa backend yet";
        return false;
    }

    // Utiliser AVAssetWriter pour H.264
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:config.outputPath.c_str()]];
    NSError* err = nil;
    AVAssetWriter* writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
    if (!writer) { mLastError = [[err localizedDescription] UTF8String]; return false; }

    NSDictionary* settings = @{
        AVVideoCodecKey:              AVVideoCodecTypeH264,
        AVVideoWidthKey:              @(mWidth),
        AVVideoHeightKey:             @(mHeight),
        AVVideoCompressionPropertiesKey: @{
            AVVideoAverageBitRateKey: @(config.bitrateBps)
        }
    };
    AVAssetWriterInput* input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                                   outputSettings:settings];
    input.expectsMediaDataInRealTime = YES;
    [writer addInput:input];
    [writer startWriting];
    [writer startSessionAtSourceTime:kCMTimeZero];

    mAssetWriter = const_cast<void*>(CFBridgingRetain(writer));
    mAssetInput  = const_cast<void*>(CFBridgingRetain(input));
    mRecordStart = std::chrono::steady_clock::now();
    mRecording   = true;
    mState       = NkCameraState::NK_CAM_STATE_RECORDING;
    return true;
}

void NkCocoaCameraBackend::StopVideoRecord()
{
    if (!mRecording) return;
    mRecording = false;
    if (mAssetWriter)
    {
        AVAssetWriter* writer = (__bridge AVAssetWriter*)mAssetWriter; CFBridgingRelease(mAssetWriter); mAssetWriter=nullptr;
        AVAssetWriterInput* input = (__bridge AVAssetWriterInput*)mAssetInput; CFBridgingRelease(mAssetInput); mAssetInput=nullptr;
        [input markAsFinished];
        [writer finishWritingWithCompletionHandler:^{}];
        mAssetWriter = nullptr;
        mAssetInput  = nullptr;
    }
    if (mState == NkCameraState::NK_CAM_STATE_RECORDING)
        mState = NkCameraState::NK_CAM_STATE_STREAMING;
}

bool  NkCocoaCameraBackend::IsRecording() const { return mRecording; }
float NkCocoaCameraBackend::GetRecordingDurationSeconds() const
{
    if (!mRecording) return 0.f;
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - mRecordStart).count();
}

bool NkCocoaCameraBackend::SetAutoFocus(bool e)
{
    // TODO: via AVCaptureDevice lock/configure
    (void)e; return false;
}
bool NkCocoaCameraBackend::SetTorch(bool e) { (void)e; return false; }
bool NkCocoaCameraBackend::SetZoom(float z) { (void)z; return false; }

bool NkCocoaCameraBackend::GetOrientation(NkCameraOrientation& out)
{
#if defined(MAC_OS_X_VERSION_10_15) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15
    // CMMotionManager disponible sur macOS 10.15+ avec capteur de mouvement
    // (uniquement sur MacBook avec capteur)
    // Fallback: utiliser NSEvent pour la rotation trackpad
#endif
    (void)out;
    return false; // macOS desktop n'a pas d'IMU standard
}
} // namespace nkentseu
