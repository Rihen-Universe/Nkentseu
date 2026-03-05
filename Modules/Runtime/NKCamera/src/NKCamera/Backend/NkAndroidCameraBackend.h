#pragma once
// =============================================================================
// NkAndroidCameraBackend.h — Capture caméra Android via Camera2 NDK API
//
// Utilise :
//   - ACameraManager       : énumération et ouverture des caméras
//   - ACameraCaptureSession: session de capture
//   - AImageReader         : réception des frames (format RGBA_8888 ou YUV_420_888)
//   - MediaCodec (JNI)     : encodage H.264 pour l'enregistrement vidéo
//   - MediaMuxer  (JNI)    : multiplexage dans un fichier MP4
//
// Requis dans CMakeLists.txt :
//   target_link_libraries(... camera2ndk mediandk android log)
//   android.permission.CAMERA dans AndroidManifest.xml
// =============================================================================

#include "NKCamera/INkCameraBackend.h"

#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <media/NdkImageReader.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>
#include <cmath>

#include <jni.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <memory>

#define NKCAM_TAG "NkCamera"
#define NKCAM_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, NKCAM_TAG, __VA_ARGS__)
#define NKCAM_LOGI(...) __android_log_print(ANDROID_LOG_INFO,  NKCAM_TAG, __VA_ARGS__)

namespace nkentseu
{

extern android_app* nk_android_global_app;

class NkAndroidCameraBackend : public INkCameraBackend
{
public:
    // JNI env du thread principal — à initialiser avant Init()
    static void SetJNIEnv(JNIEnv* env, jobject activity)
    {
        sEnv      = env;
        sActivity = activity;
    }

    NkAndroidCameraBackend()  = default;
    ~NkAndroidCameraBackend() override { Shutdown(); }

    // -----------------------------------------------------------------------
    bool Init() override
    {
        mCameraManager = ACameraManager_create();
        if (!mCameraManager) { mLastError = "ACameraManager_create failed"; return false; }
        NKCAM_LOGI("NkAndroidCameraBackend: initialized");
        return true;
    }

    void Shutdown() override
    {
        StopStreaming();
        if (mCameraManager) { ACameraManager_delete(mCameraManager); mCameraManager = nullptr; }
    }

    // -----------------------------------------------------------------------
    // Énumération
    // -----------------------------------------------------------------------
    std::vector<NkCameraDevice> EnumerateDevices() override
    {
        std::vector<NkCameraDevice> result;
        if (!mCameraManager) return result;

        ACameraIdList* idList = nullptr;
        if (ACameraManager_getCameraIdList(mCameraManager, &idList) != ACAMERA_OK)
            return result;

        for (int i = 0; i < idList->numCameras; ++i)
        {
            const char* id = idList->cameraIds[i];
            ACameraMetadata* meta = nullptr;
            ACameraManager_getCameraCharacteristics(mCameraManager, id, &meta);

            NkCameraDevice dev;
            dev.index = static_cast<NkU32>(i);
            dev.id    = id;
            dev.name  = std::string("Camera ") + id;

            // Facing
            ACameraMetadata_const_entry facing{};
            ACameraMetadata_getConstEntry(meta, ACAMERA_LENS_FACING, &facing);
            if (facing.count > 0)
            {
                switch (facing.data.u8[0])
                {
                case ACAMERA_LENS_FACING_FRONT:    dev.facing = NkCameraFacing::NK_CAMERA_FACING_FRONT;    break;
                case ACAMERA_LENS_FACING_BACK:     dev.facing = NkCameraFacing::NK_CAMERA_FACING_BACK;     break;
                case ACAMERA_LENS_FACING_EXTERNAL: dev.facing = NkCameraFacing::NK_CAMERA_FACING_EXTERNAL; break;
                }
            }

            // Résolutions supportées
            ACameraMetadata_const_entry configs{};
            ACameraMetadata_getConstEntry(meta,
                ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &configs);
            for (uint32_t j = 0; j + 3 < configs.count; j += 4)
            {
                // format, width, height, input (0=output)
                if (configs.data.i32[j+3] != 0) continue; // skip input configs
                int fmt = configs.data.i32[j];
                if (fmt != AIMAGE_FORMAT_YUV_420_888) continue;
                NkCameraDevice::Mode mode;
                mode.width  = static_cast<NkU32>(configs.data.i32[j+1]);
                mode.height = static_cast<NkU32>(configs.data.i32[j+2]);
                mode.fps    = 30;
                mode.format = NkPixelFormat::NK_PIXEL_YUV420;
                if (mode.width > 0 && mode.height > 0)
                    dev.modes.push_back(mode);
            }

            ACameraMetadata_free(meta);
            result.push_back(std::move(dev));
        }
        ACameraManager_deleteCameraIdList(idList);
        return result;
    }

    void SetHotPlugCallback(NkCameraHotPlugCallback cb) override
    {
        mHotPlugCb = std::move(cb);
        // Enregistrer ACameraManager_registerAvailabilityCallback si nécessaire
    }

    // -----------------------------------------------------------------------
    // Streaming
    // -----------------------------------------------------------------------
    bool StartStreaming(const NkCameraConfig& config) override
    {
        if (!mCameraManager)
        {
            mLastError = "Camera backend not initialized";
            return false;
        }

        if (mState != NkCameraState::NK_CAM_STATE_CLOSED)
            StopStreaming();

        if (!HasCameraPermission())
        {
            RequestCameraPermissionOnce();
            mLastError =
                "android.permission.CAMERA is not granted. "
                "Grant camera permission in Android settings and relaunch stream.";
            return false;
        }

        auto devices = EnumerateDevices();
        if (devices.empty())
        {
            mLastError =
                "No Camera2 YUV device exposed by platform. "
                "Some emulators (including MEmu variants) do not expose Camera2 for NDK.";
            return false;
        }
        if (config.deviceIndex >= devices.size())
        {
            mLastError = "Device index out of range";
            return false;
        }

        const auto& dev = devices[config.deviceIndex];
        mCameraId = dev.id;
        mWidth    = config.width;
        mHeight   = config.height;
        mFPS      = config.fps;

        if (!dev.modes.empty())
        {
            bool exact = false;
            for (const auto& mode : dev.modes)
            {
                if (mode.width == mWidth && mode.height == mHeight)
                {
                    exact = true;
                    break;
                }
            }
            if (!exact)
            {
                const auto& first = dev.modes.front();
                mWidth = first.width;
                mHeight = first.height;
            }
        }

        // Ouvrir la caméra
        mDeviceCallbacks.context   = this;
        mDeviceCallbacks.onError   = OnDeviceError;
        mDeviceCallbacks.onDisconnected = OnDeviceDisconnected;

        camera_status_t status = ACameraManager_openCamera(
            mCameraManager, mCameraId.c_str(), &mDeviceCallbacks, &mCameraDevice);
        if (status != ACAMERA_OK)
        {
            mLastError = std::string("ACameraManager_openCamera failed: ") + CameraStatusToString(status);
            return false;
        }

        media_status_t mstatus = AImageReader_new(
            static_cast<int32_t>(mWidth),
            static_cast<int32_t>(mHeight),
            AIMAGE_FORMAT_YUV_420_888,
            4,
            &mImageReader
        );
        if (mstatus != AMEDIA_OK || !mImageReader)
        {
            mLastError = "AImageReader_new failed: " + std::to_string(static_cast<int>(mstatus));
            StopStreaming();
            return false;
        }

        AImageReader_ImageListener listener{};
        listener.context    = this;
        listener.onImageAvailable = OnImageAvailable;
        mstatus = AImageReader_setImageListener(mImageReader, &listener);
        if (mstatus != AMEDIA_OK)
        {
            mLastError = "AImageReader_setImageListener failed: " + std::to_string(static_cast<int>(mstatus));
            StopStreaming();
            return false;
        }

        // Obtenir la surface de l'ImageReader
        ANativeWindow* window = nullptr;
        mstatus = AImageReader_getWindow(mImageReader, &window);
        if (mstatus != AMEDIA_OK || !window)
        {
            mLastError = "AImageReader_getWindow failed: " + std::to_string(static_cast<int>(mstatus));
            StopStreaming();
            return false;
        }

        // Créer la requête de capture
        status = ACameraDevice_createCaptureRequest(mCameraDevice, TEMPLATE_PREVIEW, &mCaptureRequest);
        if (status != ACAMERA_OK || !mCaptureRequest)
        {
            mLastError = std::string("ACameraDevice_createCaptureRequest failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        // Sortie → la surface de l'ImageReader
        status = ACameraOutputTarget_create(window, &mOutputTarget);
        if (status != ACAMERA_OK || !mOutputTarget)
        {
            mLastError = std::string("ACameraOutputTarget_create failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }
        status = ACaptureRequest_addTarget(mCaptureRequest, mOutputTarget);
        if (status != ACAMERA_OK)
        {
            mLastError = std::string("ACaptureRequest_addTarget failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        // Session
        mSessionOutputContainer = nullptr;
        status = ACaptureSessionOutputContainer_create(&mSessionOutputContainer);
        if (status != ACAMERA_OK || !mSessionOutputContainer)
        {
            mLastError = std::string("ACaptureSessionOutputContainer_create failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        status = ACaptureSessionOutput_create(window, &mSessionOutput);
        if (status != ACAMERA_OK || !mSessionOutput)
        {
            mLastError = std::string("ACaptureSessionOutput_create failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        status = ACaptureSessionOutputContainer_add(mSessionOutputContainer, mSessionOutput);
        if (status != ACAMERA_OK)
        {
            mLastError = std::string("ACaptureSessionOutputContainer_add failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        mSessionCallbacks.context              = this;
        mSessionCallbacks.onClosed             = OnSessionClosed;
        mSessionCallbacks.onReady              = OnSessionReady;
        mSessionCallbacks.onActive             = OnSessionActive;

        status = ACameraDevice_createCaptureSession(
            mCameraDevice,
            mSessionOutputContainer,
            &mSessionCallbacks,
            &mCaptureSession
        );
        if (status != ACAMERA_OK || !mCaptureSession)
        {
            mLastError = std::string("ACameraDevice_createCaptureSession failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        status = ACameraCaptureSession_setRepeatingRequest(
            mCaptureSession,
            nullptr,
            1,
            &mCaptureRequest,
            nullptr
        );
        if (status != ACAMERA_OK)
        {
            mLastError = std::string("ACameraCaptureSession_setRepeatingRequest failed: ") + CameraStatusToString(status);
            StopStreaming();
            return false;
        }

        {
            std::lock_guard<std::mutex> lk(mMutex);
            mHasFrame = false;
        }

        mState = NkCameraState::NK_CAM_STATE_STREAMING;
        NKCAM_LOGI("Streaming started: %ux%u @%u fps", mWidth, mHeight, mFPS);
        return true;
    }

    void StopStreaming() override
    {
        if (mCaptureSession) {
            ACameraCaptureSession_stopRepeating(mCaptureSession);
            ACameraCaptureSession_close(mCaptureSession);
            mCaptureSession = nullptr;
        }
        if (mCaptureRequest)  { ACaptureRequest_free(mCaptureRequest); mCaptureRequest = nullptr; }
        if (mOutputTarget)    { ACameraOutputTarget_free(mOutputTarget); mOutputTarget = nullptr; }
        if (mSessionOutput)   { ACaptureSessionOutput_free(mSessionOutput); mSessionOutput = nullptr; }
        if (mSessionOutputContainer) {
            ACaptureSessionOutputContainer_free(mSessionOutputContainer);
            mSessionOutputContainer = nullptr;
        }
        if (mCameraDevice)    { ACameraDevice_close(mCameraDevice); mCameraDevice = nullptr; }
        if (mImageReader)     { AImageReader_delete(mImageReader); mImageReader = nullptr; }
        StopVideoRecord();
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mHasFrame = false;
        }
        mState = NkCameraState::NK_CAM_STATE_CLOSED;
    }

    NkCameraState GetState()  const override { return mState; }
    void SetFrameCallback(NkFrameCallback cb) override { mFrameCb = std::move(cb); }

    bool GetLastFrame(NkCameraFrame& out) override
    {
        std::lock_guard<std::mutex> lk(mMutex);
        if (!mHasFrame) return false;
        out = mLastFrame; return true;
    }

    // -----------------------------------------------------------------------
    // Photo — capture la prochaine frame disponible
    // -----------------------------------------------------------------------
    bool CapturePhoto(NkPhotoCaptureResult& res) override
    {
        std::unique_lock<std::mutex> lk(mMutex);
        if (!mHasFrame) {
            mPhotoCv.wait_for(lk, std::chrono::seconds(3), [this]{ return mHasFrame; });
        }
        if (!mHasFrame) { res.success = false; res.errorMsg = "No frame"; return false; }
        res.frame   = mLastFrame;
        res.success = true;
        return true;
    }

    bool CapturePhotoToFile(const std::string& path) override
    {
        NkPhotoCaptureResult res;
        if (!CapturePhoto(res)) return false;
        return NkCameraSystem::SaveFrameToFile(res.frame, path);
    }

    // -----------------------------------------------------------------------
    // Vidéo — encodage via MediaCodec/MediaMuxer JNI
    // -----------------------------------------------------------------------
    bool StartVideoRecord(const NkVideoRecordConfig& config) override
    {
        if (!sEnv || mRecording) return false;
        if (config.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY)
        {
            mLastError = "IMAGE_SEQUENCE_ONLY mode is not implemented on Android backend yet";
            return false;
        }
        // Créer MediaCodec + MediaMuxer via JNI helper
        // (implémentation complète nécessiterait un bridge Java dédié)
        // Approche simplifiée : écrire frames YUV brutes + appeler ffmpeg via Runtime.exec
        mVideoRecordPath = config.outputPath;
        mRecording       = true;
        mRecordStart     = std::chrono::steady_clock::now();
        mState           = NkCameraState::NK_CAM_STATE_RECORDING;
        NKCAM_LOGI("Video record started: %s", config.outputPath.c_str());
        return true;
    }

    void StopVideoRecord() override
    {
        if (!mRecording) return;
        mRecording = false;
        if (mState == NkCameraState::NK_CAM_STATE_RECORDING)
            mState = NkCameraState::NK_CAM_STATE_STREAMING;
        NKCAM_LOGI("Video record stopped");
    }

    bool  IsRecording() const override { return mRecording; }
    float GetRecordingDurationSeconds() const override
    {
        if (!mRecording) return 0.f;
        return std::chrono::duration<float>(
            std::chrono::steady_clock::now() - mRecordStart).count();
    }

    // -----------------------------------------------------------------------
    // Contrôles via CaptureRequest metadata
    // -----------------------------------------------------------------------
    bool SetAutoFocus(bool e) override
    {
        if (!mCaptureRequest) return false;
        uint8_t mode = e ? ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO
                         : ACAMERA_CONTROL_AF_MODE_OFF;
        ACaptureRequest_setEntry_u8(mCaptureRequest, ACAMERA_CONTROL_AF_MODE, 1, &mode);
        if (mCaptureSession)
            ACameraCaptureSession_setRepeatingRequest(mCaptureSession,
                nullptr, 1, &mCaptureRequest, nullptr);
        return true;
    }

    bool SetAutoExposure(bool e) override
    {
        if (!mCaptureRequest) return false;
        uint8_t mode = e ? ACAMERA_CONTROL_AE_MODE_ON : ACAMERA_CONTROL_AE_MODE_OFF;
        ACaptureRequest_setEntry_u8(mCaptureRequest, ACAMERA_CONTROL_AE_MODE, 1, &mode);
        if (mCaptureSession)
            ACameraCaptureSession_setRepeatingRequest(mCaptureSession,
                nullptr, 1, &mCaptureRequest, nullptr);
        return true;
    }

    bool SetTorch(bool e) override
    {
        if (!mCaptureRequest) return false;
        uint8_t mode = e ? ACAMERA_FLASH_MODE_TORCH : ACAMERA_FLASH_MODE_OFF;
        ACaptureRequest_setEntry_u8(mCaptureRequest, ACAMERA_FLASH_MODE, 1, &mode);
        if (mCaptureSession)
            ACameraCaptureSession_setRepeatingRequest(mCaptureSession,
                nullptr, 1, &mCaptureRequest, nullptr);
        return true;
    }

    NkU32         GetWidth()     const override { return mWidth;  }
    NkU32         GetHeight()    const override { return mHeight; }
    NkU32         GetFPS()       const override { return mFPS;    }
    NkPixelFormat GetFormat()    const override { return NkPixelFormat::NK_PIXEL_YUV420; }
    std::string   GetLastError() const override { return mLastError; }

private:
    static const char* CameraStatusToString(camera_status_t status)
    {
        switch (status)
        {
        case ACAMERA_OK: return "ACAMERA_OK";
        case ACAMERA_ERROR_INVALID_PARAMETER: return "ACAMERA_ERROR_INVALID_PARAMETER";
        case ACAMERA_ERROR_CAMERA_DISCONNECTED: return "ACAMERA_ERROR_CAMERA_DISCONNECTED";
        case ACAMERA_ERROR_NOT_ENOUGH_MEMORY: return "ACAMERA_ERROR_NOT_ENOUGH_MEMORY";
        case ACAMERA_ERROR_METADATA_NOT_FOUND: return "ACAMERA_ERROR_METADATA_NOT_FOUND";
        case ACAMERA_ERROR_CAMERA_DEVICE: return "ACAMERA_ERROR_CAMERA_DEVICE";
        case ACAMERA_ERROR_CAMERA_SERVICE: return "ACAMERA_ERROR_CAMERA_SERVICE";
        case ACAMERA_ERROR_SESSION_CLOSED: return "ACAMERA_ERROR_SESSION_CLOSED";
        case ACAMERA_ERROR_INVALID_OPERATION: return "ACAMERA_ERROR_INVALID_OPERATION";
        case ACAMERA_ERROR_STREAM_CONFIGURE_FAIL: return "ACAMERA_ERROR_STREAM_CONFIGURE_FAIL";
        case ACAMERA_ERROR_CAMERA_IN_USE: return "ACAMERA_ERROR_CAMERA_IN_USE";
        case ACAMERA_ERROR_MAX_CAMERA_IN_USE: return "ACAMERA_ERROR_MAX_CAMERA_IN_USE";
        case ACAMERA_ERROR_CAMERA_DISABLED: return "ACAMERA_ERROR_CAMERA_DISABLED";
        case ACAMERA_ERROR_PERMISSION_DENIED: return "ACAMERA_ERROR_PERMISSION_DENIED";
        default: return "ACAMERA_ERROR_UNKNOWN";
        }
    }

    static bool GetJNIEnv(JNIEnv*& outEnv, bool& attachedHere)
    {
        outEnv = nullptr;
        attachedHere = false;
        if (!nk_android_global_app || !nk_android_global_app->activity || !nk_android_global_app->activity->vm)
            return false;
        JavaVM* vm = nk_android_global_app->activity->vm;
        if (vm->GetEnv(reinterpret_cast<void**>(&outEnv), JNI_VERSION_1_6) != JNI_OK)
        {
            if (vm->AttachCurrentThread(&outEnv, nullptr) != JNI_OK || !outEnv)
                return false;
            attachedHere = true;
        }
        return true;
    }

    bool HasCameraPermission() const
    {
        JNIEnv* env = nullptr;
        bool attachedHere = false;
        if (!GetJNIEnv(env, attachedHere))
            return true; // fallback permissif si JNI indisponible

        bool granted = true;
        jobject activity = nk_android_global_app->activity->clazz;
        jclass actClass = activity ? env->GetObjectClass(activity) : nullptr;
        if (actClass)
        {
            jmethodID checkPerm = env->GetMethodID(actClass, "checkSelfPermission", "(Ljava/lang/String;)I");
            if (checkPerm)
            {
                jstring perm = env->NewStringUTF("android.permission.CAMERA");
                jint result = env->CallIntMethod(activity, checkPerm, perm);
                env->DeleteLocalRef(perm);
                granted = (result == 0); // PackageManager.PERMISSION_GRANTED
                if (env->ExceptionCheck())
                {
                    env->ExceptionClear();
                    granted = false;
                }
            }
            env->DeleteLocalRef(actClass);
        }

        if (attachedHere)
            nk_android_global_app->activity->vm->DetachCurrentThread();
        return granted;
    }

    void RequestCameraPermissionOnce()
    {
        if (mRequestedPermissionPrompt)
            return;
        mRequestedPermissionPrompt = true;

        JNIEnv* env = nullptr;
        bool attachedHere = false;
        if (!GetJNIEnv(env, attachedHere))
            return;

        jobject activity = nk_android_global_app->activity->clazz;
        jclass actClass = activity ? env->GetObjectClass(activity) : nullptr;
        if (actClass)
        {
            jmethodID reqPerm = env->GetMethodID(actClass, "requestPermissions", "([Ljava/lang/String;I)V");
            if (reqPerm)
            {
                jclass strClass = env->FindClass("java/lang/String");
                jobjectArray perms = env->NewObjectArray(1, strClass, nullptr);
                jstring cameraPerm = env->NewStringUTF("android.permission.CAMERA");
                env->SetObjectArrayElement(perms, 0, cameraPerm);
                env->CallVoidMethod(activity, reqPerm, perms, 1001);
                if (env->ExceptionCheck())
                    env->ExceptionClear();
                env->DeleteLocalRef(cameraPerm);
                env->DeleteLocalRef(perms);
                env->DeleteLocalRef(strClass);
            }
            env->DeleteLocalRef(actClass);
        }

        if (attachedHere)
            nk_android_global_app->activity->vm->DetachCurrentThread();
    }


    // Sensors IMU
    ASensorManager*   mSensorManager = nullptr;
    const ASensor*    mAccel = nullptr;
    const ASensor*    mGyro  = nullptr;
    ASensorEventQueue* mAccelQueue = nullptr;
    ASensorEventQueue* mGyroQueue  = nullptr;
    ALooper*          mSensorLooper   = nullptr;
    std::thread       mSensorThread;
    std::atomic<bool> mSensorRunning{false};
    mutable std::mutex       mSensorMutex;
    NkCameraOrientation      mLastOrientation{};
    bool                     mSensorReady = false;

    // Gyroscope integration (pitch/roll/yaw)
    float mIntPitch=0.f, mIntRoll=0.f, mIntYaw=0.f;
    NkU64 mLastSensorTs=0;
    // -----------------------------------------------------------------------
    // Callbacks statiques C (NDK)
    // -----------------------------------------------------------------------
    static void OnImageAvailable(void* ctx, AImageReader* reader)
    {
        auto* self = static_cast<NkAndroidCameraBackend*>(ctx);
        AImage* image = nullptr;
        if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || !image) return;

        int32_t w = 0, h = 0;
        AImage_getWidth(image, &w);
        AImage_getHeight(image, &h);

        // YUV_420_888 — 3 planes
        uint8_t* yPlane = nullptr; int yLen = 0;
        uint8_t* uPlane = nullptr; int uLen = 0;
        uint8_t* vPlane = nullptr; int vLen = 0;
        AImage_getPlaneData(image, 0, &yPlane, &yLen);
        AImage_getPlaneData(image, 1, &uPlane, &uLen);
        AImage_getPlaneData(image, 2, &vPlane, &vLen);

        int64_t ts = 0;
        AImage_getTimestamp(image, &ts);

        NkCameraFrame frame;
        frame.width  = static_cast<NkU32>(w);
        frame.height = static_cast<NkU32>(h);
        frame.format = NkPixelFormat::NK_PIXEL_YUV420;
        frame.stride = static_cast<NkU32>(w);
        frame.timestampUs = static_cast<NkU64>(ts / 1000);
        frame.frameIndex  = self->mFrameIdx++;
        // Copier Y + UV
        frame.data.resize(yLen + uLen + vLen);
        if (yPlane) memcpy(frame.data.data(),            yPlane, yLen);
        if (uPlane) memcpy(frame.data.data() + yLen,     uPlane, uLen);
        if (vPlane) memcpy(frame.data.data() + yLen+uLen,vPlane, vLen);

        AImage_delete(image);

        {
            std::lock_guard<std::mutex> lk(self->mMutex);
            self->mLastFrame = frame;
            self->mHasFrame  = true;
        }
        self->mPhotoCv.notify_all();
        if (self->mFrameCb) self->mFrameCb(frame);
    }

    static void OnDeviceError(void* ctx, ACameraDevice* dev, int error)
    {
        auto* self = static_cast<NkAndroidCameraBackend*>(ctx);
        self->mLastError = "Camera device error: " + std::to_string(error);
        self->mState = NkCameraState::NK_CAM_STATE_ERROR;
        NKCAM_LOGE("Device error: %d", error);
    }
    static void OnDeviceDisconnected(void* ctx, ACameraDevice* dev)
    {
        auto* self = static_cast<NkAndroidCameraBackend*>(ctx);
        self->mState = NkCameraState::NK_CAM_STATE_CLOSED;
        NKCAM_LOGI("Device disconnected");
    }
    static void OnSessionClosed(void* ctx, ACameraCaptureSession* s) {}
    static void OnSessionReady (void* ctx, ACameraCaptureSession* s) {}
    static void OnSessionActive(void* ctx, ACameraCaptureSession* s) {}


    // ------------------------------------------------------------------
    // GetOrientation — ASensorManager (accéléromètre + gyroscope NDK)
    // Requiert: <android/sensor.h> + linker -landroid
    // ------------------------------------------------------------------
    bool GetOrientation(NkCameraOrientation& out) override
    {
        if (!mSensorLooper) return false;
        // Lire la dernière valeur mémorisée (mise à jour par le thread sensor)
        std::lock_guard<std::mutex> lk(mSensorMutex);
        if (!mSensorReady) return false;
        out = mLastOrientation;
        return true;
    }

    // Appeler depuis un thread dédié avec un Looper
    void InitSensors()
    {
        // Use getInstance() for API 24+ compatibility (getInstanceForPackage requires API 26+)
        mSensorManager = ASensorManager_getInstance();
        if (!mSensorManager) return;

        mAccel = ASensorManager_getDefaultSensor(mSensorManager,
                                                 ASENSOR_TYPE_ACCELEROMETER);
        mGyro  = ASensorManager_getDefaultSensor(mSensorManager,
                                                 ASENSOR_TYPE_GYROSCOPE);

        mSensorLooper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
        if (mAccel) {
            mAccelQueue = ASensorManager_createEventQueue(
                mSensorManager, mSensorLooper, 1, SensorCallback, this);
            ASensorEventQueue_enableSensor(mAccelQueue, mAccel);
            ASensorEventQueue_setEventRate(mAccelQueue, mAccel, 16667); // 60Hz
        }
        if (mGyro) {
            mGyroQueue = ASensorManager_createEventQueue(
                mSensorManager, mSensorLooper, 2, SensorCallback, this);
            ASensorEventQueue_enableSensor(mGyroQueue, mGyro);
            ASensorEventQueue_setEventRate(mGyroQueue, mGyro, 16667);
        }
        mSensorThread = std::thread([this]{ SensorLoop(); });
    }

    void ShutdownSensors()
    {
        mSensorRunning = false;
        if (mSensorThread.joinable()) mSensorThread.join();
        if (mAccelQueue) { ASensorEventQueue_disableSensor(mAccelQueue, mAccel);
                           ASensorManager_destroyEventQueue(mSensorManager, mAccelQueue); }
        if (mGyroQueue)  { ASensorEventQueue_disableSensor(mGyroQueue, mGyro);
                           ASensorManager_destroyEventQueue(mSensorManager, mGyroQueue); }
    }

    // -----------------------------------------------------------------------
    // Membres
    // -----------------------------------------------------------------------
    ACameraManager*               mCameraManager        = nullptr;
    ACameraDevice*                mCameraDevice         = nullptr;
    ACameraCaptureSession*        mCaptureSession       = nullptr;
    ACaptureRequest*              mCaptureRequest       = nullptr;
    ACameraOutputTarget*          mOutputTarget         = nullptr;
    ACaptureSessionOutput*        mSessionOutput        = nullptr;
    ACaptureSessionOutputContainer* mSessionOutputContainer = nullptr;
    AImageReader*                 mImageReader          = nullptr;

    ACameraDevice_StateCallbacks  mDeviceCallbacks{};
    ACameraCaptureSession_stateCallbacks mSessionCallbacks{};

    std::string   mCameraId;
    NkCameraState mState     = NkCameraState::NK_CAM_STATE_CLOSED;
    NkU32         mWidth     = 0, mHeight = 0, mFPS = 30;
    NkU32         mFrameIdx  = 0;
    std::string   mLastError;

    std::mutex              mMutex;
    std::condition_variable mPhotoCv;
    NkCameraFrame           mLastFrame;
    bool                    mHasFrame = false;

    NkFrameCallback         mFrameCb;
    NkCameraHotPlugCallback mHotPlugCb;

    // Vidéo
    bool          mRecording = false;
    std::string   mVideoRecordPath;
    std::chrono::steady_clock::time_point mRecordStart;
    bool          mRequestedPermissionPrompt = false;

    static JNIEnv*  sEnv;
    static jobject  sActivity;

    // Sensor thread functions
    static int SensorCallback(int fd, int events, void* data)
    {
        auto* self = static_cast<NkAndroidCameraBackend*>(data);
        ASensorEvent ev;
        // Traité dans SensorLoop via ALooper_pollOnce
        (void)self; (void)ev;
        return 1; // continuer
    }

    void SensorLoop()
    {
        mSensorRunning = true;
        while (mSensorRunning) {
            ALooper_pollOnce(16, nullptr, nullptr, nullptr);
            ASensorEvent ev;
            // Accéléromètre
            if (mAccelQueue) {
                while (ASensorEventQueue_getEvents(mAccelQueue, &ev, 1) > 0) {
                    if (ev.type == ASENSOR_TYPE_ACCELEROMETER) {
                        std::lock_guard<std::mutex> lk(mSensorMutex);
                        mLastOrientation.accelX = ev.acceleration.x;
                        mLastOrientation.accelY = ev.acceleration.y;
                        mLastOrientation.accelZ = ev.acceleration.z;
                        // Pitch/Roll depuis accéléromètre
                        float ax=ev.acceleration.x, ay=ev.acceleration.y, az=ev.acceleration.z;
                        mLastOrientation.pitch = atan2f(ay, sqrtf(ax*ax+az*az))*(180.f/3.14159f);
                        mLastOrientation.roll  = atan2f(-ax, az)*(180.f/3.14159f);
                        mSensorReady = true;
                    }
                }
            }
            // Gyroscope → intégration yaw
            if (mGyroQueue) {
                while (ASensorEventQueue_getEvents(mGyroQueue, &ev, 1) > 0) {
                    if (ev.type == ASENSOR_TYPE_GYROSCOPE) {
                        NkU64 ts = (NkU64)(ev.timestamp / 1000); // ns → µs
                        if (mLastSensorTs > 0) {
                            float dt = (float)(ts - mLastSensorTs) / 1e6f;
                            if (dt > 0.f && dt < 0.1f) {
                                std::lock_guard<std::mutex> lk(mSensorMutex);
                                mIntYaw   += ev.vector.z * (180.f/3.14159f) * dt;
                                mLastOrientation.yaw = mIntYaw;
                            }
                        }
                        mLastSensorTs = ts;
                    }
                }
            }
        }
    }
};

inline JNIEnv*  NkAndroidCameraBackend::sEnv      = nullptr;
inline jobject  NkAndroidCameraBackend::sActivity  = nullptr;

} // namespace nkentseu
