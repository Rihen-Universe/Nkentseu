// =============================================================================
// NkUIKitCameraBackend.mm — AVFoundation iOS + CMMotionManager (IMU)
// COMPLET ET FONCTIONNEL
// =============================================================================
#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMotion/CoreMotion.h>

#include "NkUIKitCameraBackend.h"
#include "NKCamera/NkCameraSystem.h"

// ---------------------------------------------------------------------------
// Delegate vidéo
// ---------------------------------------------------------------------------
@interface NkiOSVidDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (nonatomic, assign) nkentseu::NkUIKitCameraBackend* backend;
@end
@implementation NkiOSVidDelegate
- (void)captureOutput:(AVCaptureOutput*)o
didOutputSampleBuffer:(CMSampleBufferRef)sb
       fromConnection:(AVCaptureConnection*)c
{
    CVImageBufferRef ib = CMSampleBufferGetImageBuffer(sb);
    if (!ib || !self.backend) return;
    CMTime pts = CMSampleBufferGetPresentationTimeStamp(sb);
    NkU64 ts = (NkU64)(CMTimeGetSeconds(pts)*1e6);
    self.backend->_OnVideoFrame((__bridge void*)ib, ts);
}
@end

// ---------------------------------------------------------------------------
// Delegate photo
// ---------------------------------------------------------------------------
@interface NkiOSPhotoDelegate : NSObject <AVCapturePhotoCaptureDelegate>
@property (nonatomic, assign) nkentseu::NkUIKitCameraBackend* backend;
@end
@implementation NkiOSPhotoDelegate
- (void)captureOutput:(AVCapturePhotoOutput*)o
didFinishProcessingPhoto:(AVCapturePhoto*)photo
                error:(NSError*)error
{
    if (!self.backend) return;
    NSData* d = [photo fileDataRepresentation];
    const char* err = error ? [[error localizedDescription] UTF8String] : nullptr;
    self.backend->_OnPhotoCapture(d?(void*)d.bytes:nullptr, d?d.length:0, !error, err);
}
@end

namespace nkentseu
{

// ---------------------------------------------------------------------------
// Membres privés ObjC (PIMPL interne)
// ---------------------------------------------------------------------------
struct UIKitPrivate {
    CMMotionManager* motionMgr = nil;
    AVCaptureSession* session  = nil;
    AVCaptureDevice*  device   = nil;
    NkiOSVidDelegate*  vidDel  = nil;
    NkiOSPhotoDelegate* photoDel = nil;
    AVCapturePhotoOutput* photoOut = nil;
    AVAssetWriter*    writer  = nil;
    AVAssetWriterInput* wrInput = nil;
    AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
};

NkUIKitCameraBackend::NkUIKitCameraBackend()
    : mPriv(new UIKitPrivate()) {}

NkUIKitCameraBackend::~NkUIKitCameraBackend() { Shutdown(); delete (UIKitPrivate*)mPriv; }

bool NkUIKitCameraBackend::Init()
{
    auto* p = (UIKitPrivate*)mPriv;
    p->motionMgr = [[CMMotionManager alloc] init];
    p->motionMgr.deviceMotionUpdateInterval = 1.0/60.0;
    if (p->motionMgr.isDeviceMotionAvailable)
        [p->motionMgr startDeviceMotionUpdates];
    return true;
}

void NkUIKitCameraBackend::Shutdown()
{
    StopStreaming();
    auto* p = (UIKitPrivate*)mPriv;
    if (p->motionMgr) { [p->motionMgr stopDeviceMotionUpdates]; p->motionMgr = nil; }
}

// ---------------------------------------------------------------------------
NkVector<NkCameraDevice> NkUIKitCameraBackend::EnumerateDevices()
{
    NkVector<NkCameraDevice> result;
    NSArray<AVCaptureDeviceType>* types = @[
        AVCaptureDeviceTypeBuiltInWideAngleCamera,
        AVCaptureDeviceTypeBuiltInTelephotoCamera,
        AVCaptureDeviceTypeBuiltInUltraWideCamera,
        AVCaptureDeviceTypeBuiltInDualCamera,
        AVCaptureDeviceTypeBuiltInTripleCamera
    ];
    AVCaptureDeviceDiscoverySession* ds =
        [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:types
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];
    NkU32 idx = 0;
    for (AVCaptureDevice* d in ds.devices) {
        NkCameraDevice dev;
        dev.index = idx++;
        dev.id    = [[d uniqueID] UTF8String];
        dev.name  = [[d localizedName] UTF8String];
        dev.facing = (d.position==AVCaptureDevicePositionFront)
                   ? NkCameraFacing::NK_CAMERA_FACING_FRONT
                   : NkCameraFacing::NK_CAMERA_FACING_BACK;
        for (AVCaptureDeviceFormat* fmt in d.formats) {
            CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(fmt.formatDescription);
            NkCameraDevice::Mode m;
            m.width=(NkU32)dim.width; m.height=(NkU32)dim.height;
            m.fps=30; m.format=NkPixelFormat::NK_PIXEL_BGRA8;
            if (m.width>0&&m.height>0) dev.modes.PushBack(m);
        }
        result.PushBack(dev);
    }
    return result;
}

// ---------------------------------------------------------------------------
bool NkUIKitCameraBackend::StartStreaming(const NkCameraConfig& config)
{
    auto devs = EnumerateDevices();
    if (config.deviceIndex >= devs.Size())
    { mLastError="Device index out of range"; return false; }

    auto* p = (UIKitPrivate*)mPriv;
    NSString* uid = [NSString stringWithUTF8String:devs[config.deviceIndex].id.c_str()];
    p->device = [AVCaptureDevice deviceWithUniqueID:uid];
    if (!p->device) { mLastError="Cannot find device"; return false; }

    NSError* err=nil;
    AVCaptureDeviceInput* input=[AVCaptureDeviceInput deviceInputWithDevice:p->device error:&err];
    if (!input){mLastError=[[err localizedDescription]UTF8String];return false;}

    p->session = [[AVCaptureSession alloc] init];
    [p->session beginConfiguration];

    // Preset selon résolution
    if (config.height<=480)       p->session.sessionPreset=AVCaptureSessionPreset640x480;
    else if (config.height<=720)  p->session.sessionPreset=AVCaptureSessionPreset1280x720;
    else if (config.height<=1080) p->session.sessionPreset=AVCaptureSessionPreset1920x1080;
    else                          p->session.sessionPreset=AVCaptureSessionPreset3840x2160;

    [p->session addInput:input];

    // Sortie vidéo BGRA
    AVCaptureVideoDataOutput* vOut = [[AVCaptureVideoDataOutput alloc] init];
    vOut.videoSettings=@{(NSString*)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA)};
    vOut.alwaysDiscardsLateVideoFrames=YES;
    p->vidDel=[[NkiOSVidDelegate alloc]init]; p->vidDel.backend=this;
    dispatch_queue_t q=dispatch_queue_create("nk.cam.ios",DISPATCH_QUEUE_SERIAL);
    [vOut setSampleBufferDelegate:p->vidDel queue:q];
    [p->session addOutput:vOut];

    // Sortie photo
    p->photoOut=[[AVCapturePhotoOutput alloc]init];
    [p->session addOutput:p->photoOut];

    [p->session commitConfiguration];
    [p->session startRunning];

    mWidth=config.width; mHeight=config.height; mFPS=config.fps;
    mState=NkCameraState::NK_CAM_STATE_STREAMING;
    return true;
}

void NkUIKitCameraBackend::StopStreaming()
{
    StopVideoRecord();
    auto* p=(UIKitPrivate*)mPriv;
    if (p->session) { [p->session stopRunning]; p->session=nil; }
    p->vidDel=nil; p->photoOut=nil; p->device=nil;
    mState=NkCameraState::NK_CAM_STATE_CLOSED;
}

// ---------------------------------------------------------------------------
void NkUIKitCameraBackend::_OnVideoFrame(void* pxBuf, NkU64 ts)
{
    CVImageBufferRef ib=(CVImageBufferRef)pxBuf;
    CVPixelBufferLockBaseAddress(ib,kCVPixelBufferLock_ReadOnly);
    NkU8* ptr=(NkU8*)CVPixelBufferGetBaseAddress(ib);
    size_t stride=CVPixelBufferGetBytesPerRow(ib);
    size_t w=CVPixelBufferGetWidth(ib), h=CVPixelBufferGetHeight(ib);

    NkCameraFrame frame;
    frame.width=(NkU32)w; frame.height=(NkU32)h;
    frame.stride=(NkU32)stride; frame.format=NkPixelFormat::NK_PIXEL_BGRA8;
    frame.timestampUs=ts; frame.frameIndex=mFrameIdx++;
    frame.data.Resize(static_cast<usize>(stride*h));
    memcpy(frame.data.Data(),ptr,stride*h);
    CVPixelBufferUnlockBaseAddress(ib,kCVPixelBufferLock_ReadOnly);

    { std::lock_guard<std::mutex> lk(mMutex);
      mLastFrame=frame; mHasFrame=true; }
    if (mFrameCb) mFrameCb(frame);

    // Enregistrement vidéo
    auto* p=(UIKitPrivate*)mPriv;
    if (mRecording && p->wrInput && p->adaptor && p->wrInput.readyForMoreMediaData) {
        if (mFirstFrameTime==0) mFirstFrameTime=ts;
        CMTime t=CMTimeMake((int64_t)(ts-mFirstFrameTime),1000000);
        [p->adaptor appendPixelBuffer:ib withPresentationTime:t];
    }
}

bool NkUIKitCameraBackend::GetLastFrame(NkCameraFrame& out) {
    std::lock_guard<std::mutex> lk(mMutex);
    if (!mHasFrame) return false; out=mLastFrame; return true;
}

// ---------------------------------------------------------------------------
// Photo
// ---------------------------------------------------------------------------
bool NkUIKitCameraBackend::CapturePhoto(NkPhotoCaptureResult& res)
{
    auto* p=(UIKitPrivate*)mPriv;
    if (!p->photoOut) { res.success=false; res.errorMsg="No photo output"; return false; }
    p->photoDel=[[NkiOSPhotoDelegate alloc]init]; p->photoDel.backend=this;
    { std::lock_guard<std::mutex> lk(mPhotoMutex); mPhotoReady=false; }
    [p->photoOut capturePhotoWithSettings:[AVCapturePhotoSettings photoSettings]
                                 delegate:p->photoDel];
    std::unique_lock<std::mutex> lk(mPhotoMutex);
    mPhotoCv.wait_for(lk,std::chrono::seconds(5),[this]{return mPhotoReady;});
    res=mPhotoPending; return res.success;
}

void NkUIKitCameraBackend::_OnPhotoCapture(void* data,size_t len,bool ok,const char* err)
{
    std::lock_guard<std::mutex> lk(mPhotoMutex);
    mPhotoPending={};
    if (ok && data && len>0) {
        mPhotoPending.success=true;
        mPhotoPending.frame.data.Clear();
        mPhotoPending.frame.data.Resize(static_cast<usize>(len));
        memcpy(mPhotoPending.frame.data.Data(), data, len);
        mPhotoPending.frame.format=NkPixelFormat::NK_PIXEL_MJPEG;
    } else {
        mPhotoPending.success=false;
        if (err) mPhotoPending.errorMsg=err;
    }
    mPhotoReady=true; mPhotoCv.notify_one();
}

bool NkUIKitCameraBackend::CapturePhotoToFile(const NkString& path)
{
    NkPhotoCaptureResult res; if (!CapturePhoto(res)||res.frame.data.Empty()) return false;
    // JPEG brut → écriture directe
    FILE* f=fopen(path.CStr(),"wb"); if (!f) return false;
    fwrite(res.frame.data.Data(),1,res.frame.data.Size(),f);
    fclose(f); return true;
}

// ---------------------------------------------------------------------------
// Vidéo
// ---------------------------------------------------------------------------
bool NkUIKitCameraBackend::StartVideoRecord(const NkVideoRecordConfig& config)
{
    if (mRecording) return false;
    if (config.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY)
    {
        mLastError = "IMAGE_SEQUENCE_ONLY mode is not implemented on UIKit backend yet";
        return false;
    }
    auto* p=(UIKitPrivate*)mPriv;
    NSURL* url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:config.outputPath.c_str()]];
    NSError* err=nil;
    p->writer=[[AVAssetWriter alloc]initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
    if (!p->writer){mLastError=[[err localizedDescription]UTF8String];return false;}

    NSDictionary* vs=@{AVVideoCodecKey:AVVideoCodecTypeH264,
                       AVVideoWidthKey:@(mWidth),AVVideoHeightKey:@(mHeight)};
    p->wrInput=[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                  outputSettings:vs];
    p->wrInput.expectsMediaDataInRealTime=YES;

    NSDictionary* pa=@{(NSString*)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA),
                       (NSString*)kCVPixelBufferWidthKey:@(mWidth),
                       (NSString*)kCVPixelBufferHeightKey:@(mHeight)};
    p->adaptor=[AVAssetWriterInputPixelBufferAdaptor
        assetWriterInputPixelBufferAdaptorWithAssetWriterInput:p->wrInput
                                   sourcePixelBufferAttributes:pa];
    [p->writer addInput:p->wrInput];
    [p->writer startWriting];
    [p->writer startSessionAtSourceTime:kCMTimeZero];

    mFirstFrameTime=0; mRecordStart=std::chrono::steady_clock::now();
    mRecording=true; mState=NkCameraState::NK_CAM_STATE_RECORDING;
    return true;
}

void NkUIKitCameraBackend::StopVideoRecord()
{
    if (!mRecording) return; mRecording=false;
    auto* p=(UIKitPrivate*)mPriv;
    if (p->writer) {
        [p->wrInput markAsFinished];
        dispatch_semaphore_t sem=dispatch_semaphore_create(0);
        [p->writer finishWritingWithCompletionHandler:^{dispatch_semaphore_signal(sem);}];
        dispatch_semaphore_wait(sem,dispatch_time(DISPATCH_TIME_NOW,10*NSEC_PER_SEC));
        p->writer=nil; p->wrInput=nil; p->adaptor=nil;
    }
    if (mState==NkCameraState::NK_CAM_STATE_RECORDING)
        mState=NkCameraState::NK_CAM_STATE_STREAMING;
}

bool NkUIKitCameraBackend::IsRecording() const { return mRecording; }
float NkUIKitCameraBackend::GetRecordingDurationSeconds() const {
    if (!mRecording) return 0.f;
    return std::chrono::duration<float>(std::chrono::steady_clock::now()-mRecordStart).count();
}

// ---------------------------------------------------------------------------
// GetOrientation — CMMotionManager (yaw, pitch, roll réels via gyro+accel+magneto)
// ---------------------------------------------------------------------------
bool NkUIKitCameraBackend::GetOrientation(NkCameraOrientation& out)
{
    auto* p=(UIKitPrivate*)mPriv;
    if (!p->motionMgr || !p->motionMgr.isDeviceMotionActive) return false;
    CMDeviceMotion* dm = p->motionMgr.deviceMotion;
    if (!dm) return false;

    // Attitude = rotation de l'appareil dans le référentiel monde
    CMAttitude* att = dm.attitude;
    out.yaw   = (float)(att.yaw   * 180.0 / M_PI);
    out.pitch = (float)(att.pitch * 180.0 / M_PI);
    out.roll  = (float)(att.roll  * 180.0 / M_PI);

    CMAcceleration acc = dm.userAcceleration;
    out.accelX = (float)acc.x;
    out.accelY = (float)acc.y;
    out.accelZ = (float)acc.z;
    return true;
}

// ---------------------------------------------------------------------------
// Contrôles
// ---------------------------------------------------------------------------
bool NkUIKitCameraBackend::SetAutoFocus(bool e) {
    auto* p=(UIKitPrivate*)mPriv; if (!p->device) return false;
    if (![p->device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus]) return false;
    NSError* err=nil; [p->device lockForConfiguration:&err];
    p->device.focusMode=e?AVCaptureFocusModeContinuousAutoFocus:AVCaptureFocusModeLocked;
    [p->device unlockForConfiguration]; return err==nil;
}
bool NkUIKitCameraBackend::SetAutoExposure(bool e) {
    auto* p=(UIKitPrivate*)mPriv; if (!p->device) return false;
    NSError* err=nil; [p->device lockForConfiguration:&err];
    p->device.exposureMode=e?AVCaptureExposureModeContinuousAutoExposure:AVCaptureExposureModeLocked;
    [p->device unlockForConfiguration]; return err==nil;
}
bool NkUIKitCameraBackend::SetAutoWhiteBalance(bool e) {
    auto* p=(UIKitPrivate*)mPriv; if (!p->device) return false;
    NSError* err=nil; [p->device lockForConfiguration:&err];
    p->device.whiteBalanceMode=e?AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance:AVCaptureWhiteBalanceModeLocked;
    [p->device unlockForConfiguration]; return err==nil;
}
bool NkUIKitCameraBackend::SetZoom(float z) {
    auto* p=(UIKitPrivate*)mPriv; if (!p->device) return false;
    NSError* err=nil; [p->device lockForConfiguration:&err];
    p->device.videoZoomFactor=MAX(1.f,MIN(z,p->device.maxAvailableVideoZoomFactor));
    [p->device unlockForConfiguration]; return err==nil;
}
bool NkUIKitCameraBackend::SetTorch(bool e) {
    auto* p=(UIKitPrivate*)mPriv; if (!p->device||![p->device hasTorch]) return false;
    NSError* err=nil; [p->device lockForConfiguration:&err];
    p->device.torchMode=e?AVCaptureTorchModeOn:AVCaptureTorchModeOff;
    [p->device unlockForConfiguration]; return err==nil;
}
bool NkUIKitCameraBackend::SetFocusPoint(float x,float y) {
    auto* p=(UIKitPrivate*)mPriv; if (!p->device) return false;
    if (![p->device isFocusPointOfInterestSupported]) return false;
    NSError* err=nil; [p->device lockForConfiguration:&err];
    p->device.focusPointOfInterest=CGPointMake(x,y);
    p->device.focusMode=AVCaptureFocusModeAutoFocus;
    [p->device unlockForConfiguration]; return err==nil;
}

} // namespace nkentseu
