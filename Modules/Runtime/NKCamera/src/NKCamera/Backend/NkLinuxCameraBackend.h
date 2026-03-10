#pragma once
// =============================================================================
// NkLinuxCameraBackend.h — V4L2 (Video4Linux2) — COMPLET ET FONCTIONNEL
// GetOrientation: via /sys/bus/iio/devices/iio:device* (accéléromètre intégré)
// Vidéo: ffmpeg pipe (MP4/H.264) ou écriture RAW si ffmpeg absent
// =============================================================================
#include "NKCamera/INkCameraBackend.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKCore/NkAtomic.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKTime/NkChrono.h"

#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <dirent.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <csignal>
#include <filesystem>
#include <cctype>
#include <cstdlib>

namespace nkentseu
{

struct V4L2Buf { void* start=nullptr; size_t length=0; };

class NkLinuxCameraBackend : public INkCameraBackend
{
public:
    NkLinuxCameraBackend()  = default;
    ~NkLinuxCameraBackend() override { Shutdown(); }

    bool Init()     override { return true; }
    void Shutdown() override { StopStreaming(); }

    // ------------------------------------------------------------------
    // Énumération — scanne /dev/video*
    // ------------------------------------------------------------------
    NkVector<NkCameraDevice> EnumerateDevices() override
    {
        NkVector<NkCameraDevice> result;
        NkVector<NkString> paths;
        DIR* dir = opendir("/dev");
        if (!dir) return result;
        struct dirent* ent;
        while ((ent = readdir(dir)))
            if (strncmp(ent->d_name,"video",5)==0) {
                NkString p = "/dev/";  p += ent->d_name;
                paths.PushBack(p);
            }
        closedir(dir);
        std::sort(paths.begin(), paths.end());

        uint32 idx = 0;
        for (const auto& path : paths) {
            int fd = open(path.CStr(), O_RDWR|O_NONBLOCK);
            if (fd < 0) continue;
            struct v4l2_capability cap={};
            if (ioctl(fd,VIDIOC_QUERYCAP,&cap)<0
             || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            { close(fd); continue; }

            NkCameraDevice dev;
            dev.index  = idx++;
            dev.id     = path;
            dev.name   = (const char*)cap.card;
            dev.facing = NkCameraFacing::NK_CAMERA_FACING_EXTERNAL;

            // Modes supportés
            v4l2_fmtdesc fd2={};
            fd2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            while (ioctl(fd,VIDIOC_ENUM_FMT,&fd2)==0) {
                v4l2_frmsizeenum fs={};
                fs.pixel_format = fd2.pixelformat;
                fs.index = 0;
                while (ioctl(fd,VIDIOC_ENUM_FRAMESIZES,&fs)==0) {
                    NkCameraDevice::Mode m;
                    if (fs.type==V4L2_FRMSIZE_TYPE_DISCRETE)
                    { m.width=fs.discrete.width; m.height=fs.discrete.height; }
                    else { m.width=fs.stepwise.max_width; m.height=fs.stepwise.max_height; }
                    m.fps=30; m.format=NkPixelFormat::NK_PIXEL_YUYV;
                    if (m.width>0&&m.height>0) dev.modes.PushBack(m);
                    ++fs.index;
                }
                ++fd2.index;
            }
            close(fd);
            result.PushBack(std::move(dev));
        }
        return result;
    }

    void SetHotPlugCallback(NkCameraHotPlugCallback cb) override
    { mHotPlugCb = std::move(cb); }

    // ------------------------------------------------------------------
    // StartStreaming
    // ------------------------------------------------------------------
    bool StartStreaming(const NkCameraConfig& config) override
    {
        mLastError.Clear();
        if (mFd >= 0) StopStreaming();
        auto devs = EnumerateDevices();
        if (config.deviceIndex >= devs.Size())
        { mLastError = NkString::Fmtf("Device index %u out of range", config.deviceIndex); return false; }

        const NkString& path = devs[config.deviceIndex].id;
        mFd = open(path.CStr(), O_RDWR);
        if (mFd < 0) { mLastError="Cannot open "+path+": "+strerror(errno); return false; }

        auto failAndCleanup = [&](const NkString& err) -> bool
        {
            mLastError = err;
            for (auto& b : mBufs)
            {
                if (b.start && b.start != MAP_FAILED)
                    munmap(b.start, b.length);
            }
            mBufs.Clear();
            if (mFd >= 0) { close(mFd); mFd = -1; }
            return false;
        };

        // En WSL2/usbip, MJPEG est souvent plus fiable que YUYV.
        // On essaie MJPEG d'abord, puis YUYV en fallback.
        struct v4l2_format fmt={};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width       = config.width;
        fmt.fmt.pix.height      = config.height;
        fmt.fmt.pix.field       = V4L2_FIELD_ANY;

        bool formatSet = false;
        for (const uint32 pixfmt : { static_cast<uint32>(V4L2_PIX_FMT_MJPEG), static_cast<uint32>(V4L2_PIX_FMT_YUYV) })
        {
            fmt.fmt.pix.pixelformat = pixfmt;
            if (ioctl(mFd, VIDIOC_S_FMT, &fmt) == 0)
            {
                formatSet = true;
                break;
            }
        }
        if (!formatSet)
            return failAndCleanup("VIDIOC_S_FMT failed: " + NkString(strerror(errno)));

        mWidth  = fmt.fmt.pix.width;
        mHeight = fmt.fmt.pix.height;
        mFPS    = config.fps;
        mFormat = (fmt.fmt.pix.pixelformat==V4L2_PIX_FMT_YUYV)
                ? NkPixelFormat::NK_PIXEL_YUYV : NkPixelFormat::NK_PIXEL_MJPEG;

        // FPS
        struct v4l2_streamparm parm={};
        parm.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe={1,mFPS};
        if (ioctl(mFd,VIDIOC_S_PARM,&parm)==0 &&
            parm.parm.capture.timeperframe.numerator != 0)
        {
            mFPS = parm.parm.capture.timeperframe.denominator /
                   parm.parm.capture.timeperframe.numerator;
        }

        // Buffers mmap
        struct v4l2_requestbuffers req={};
        req.count=4; req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; req.memory=V4L2_MEMORY_MMAP;
        if (ioctl(mFd,VIDIOC_REQBUFS,&req)<0||req.count<2)
            return failAndCleanup("VIDIOC_REQBUFS failed: " + NkString(strerror(errno)));

        mBufs.Resize(req.count);
        for (uint32 i=0;i<req.count;++i) {
            struct v4l2_buffer buf={};
            buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; buf.memory=V4L2_MEMORY_MMAP; buf.index=i;
            if (ioctl(mFd,VIDIOC_QUERYBUF,&buf)<0)
                return failAndCleanup("VIDIOC_QUERYBUF failed: " + NkString(strerror(errno)));
            mBufs[i].length = buf.length;
            mBufs[i].start  = mmap(nullptr,buf.length,PROT_READ|PROT_WRITE, MAP_SHARED,mFd,buf.m.offset);
            if (mBufs[i].start == MAP_FAILED)
                return failAndCleanup("mmap failed: " + NkString(strerror(errno)));
            if (ioctl(mFd,VIDIOC_QBUF,&buf)<0)
                return failAndCleanup("VIDIOC_QBUF failed: " + NkString(strerror(errno)));
        }

        v4l2_buf_type t=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mFd,VIDIOC_STREAMON,&t)<0)
            return failAndCleanup("VIDIOC_STREAMON failed: " + NkString(strerror(errno)));

        mRunning = true;
        mState   = NkCameraState::NK_CAM_STATE_STREAMING;
        mCaptureThread = std::thread([this]{ CaptureLoop(); });
        return true;
    }

    void StopStreaming() override
    {
        mRunning = false;
        if (mCaptureThread.joinable()) mCaptureThread.join();
        StopVideoRecord();
        if (mFd >= 0) {
            v4l2_buf_type t=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(mFd,VIDIOC_STREAMOFF,&t);
            for (auto& b:mBufs) if (b.start) munmap(b.start,b.length);
            mBufs.Clear();
            close(mFd); mFd=-1;
        }
        mState = NkCameraState::NK_CAM_STATE_CLOSED;
    }

    NkCameraState GetState() const override { return mState; }
    void SetFrameCallback(NkFrameCallback cb) override { mFrameCb=std::move(cb); }
    bool GetLastFrame(NkCameraFrame& out) override {
        std::lock_guard<std::mutex> lk(mMutex);
        if (!mHasFrame) return false; out=mLastFrame; return true;
    }

    // ------------------------------------------------------------------
    // Photo
    // ------------------------------------------------------------------
    bool CapturePhoto(NkPhotoCaptureResult& res) override {
        std::lock_guard<std::mutex> lk(mMutex);
        if (!mHasFrame){res.success=false;return false;}
        res.frame=mLastFrame; res.success=true; return true;
    }
    bool CapturePhotoToFile(const NkString& path) override {
        NkPhotoCaptureResult r; if (!CapturePhoto(r)) return false;
        // YUYV → RGBA → PPM (portabie, sans dépendance externe)
        auto& f = r.frame;
        uint32 w=f.width,h=f.height;
        NkVector<uint8> rgb; rgb.Resize(w*h*3);
        if (f.format==NkPixelFormat::NK_PIXEL_YUYV) {
            for (uint32 i=0;i<w*h/2;++i) {
                float y0=(float)f.data[i*4]-16.f,cb=(float)f.data[i*4+1]-128.f;
                float y1=(float)f.data[i*4+2]-16.f,cr=(float)f.data[i*4+3]-128.f;
                auto cl=[](float v)->uint8{return(uint8)(v<0?0:v>255?255:v);};
                rgb[i*6]  =cl(y0*1.164f+cr*1.596f);
                rgb[i*6+1]=cl(y0*1.164f-cb*0.391f-cr*0.813f);
                rgb[i*6+2]=cl(y0*1.164f+cb*2.018f);
                rgb[i*6+3]=cl(y1*1.164f+cr*1.596f);
                rgb[i*6+4]=cl(y1*1.164f-cb*0.391f-cr*0.813f);
                rgb[i*6+5]=cl(y1*1.164f+cb*2.018f);
            }
        }
        // Écrire PPM (compatible universellement)
        NkString out = path.Empty() ? "photo.ppm" : path;
        // Find last '.' to replace extension
        {
            NkString::SizeType dot = NkString::SizeType(-1);
            for (NkString::SizeType i = 0; i < out.Size(); ++i)
                if (out[i] == '.') dot = i;
            if (dot != NkString::SizeType(-1)) out = out.SubStr(0, dot) + ".ppm";
            else out += ".ppm";
        }
        FILE* fp=fopen(out.CStr(),"wb");
        if (!fp) return false;
        fprintf(fp,"P6\n%d %d\n255\n",(int)w,(int)h);
        fwrite(rgb.Data(),1,rgb.Size(),fp);
        fclose(fp);
        return true;
    }

    // ------------------------------------------------------------------
    // Vidéo — ffmpeg pipe
    // ------------------------------------------------------------------
    bool StartVideoRecord(const NkVideoRecordConfig& config) override {
        std::lock_guard<std::mutex> recLock(mRecordMutex);
        if (mRecordMode != RecordMode::None || mFfmpegPipe) return false;
        if (mState != NkCameraState::NK_CAM_STATE_STREAMING) {
            mLastError = "Cannot start recording: camera is not streaming";
            return false;
        }

        const NkString requestedCodec = ToLowerCopy(config.videoCodec);
        const bool forceImageSequence = (config.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY) ||
                                        (requestedCodec == "images" ||
                                         requestedCodec == "image-sequence" ||
                                         requestedCodec == "sequence" ||
                                         requestedCodec == "frames");
        const bool allowImageFallback = (config.mode == NkVideoRecordConfig::Mode::AUTO);

        // Empêche l'arrêt brutal du process si ffmpeg ferme le pipe (SIGPIPE).
        std::signal(SIGPIPE, SIG_IGN);

        mLastError.Clear();
        mRecordImageDir.Clear();
        mRecordFrameCounter = 0;

        NkString fallbackReason;
        if (!forceImageSequence)
        {
            if (!FfmpegExists())
            {
                fallbackReason = "ffmpeg not found in PATH";
            }
            else
            {
                const NkString encoder = ResolveVideoEncoder(requestedCodec);
                if (encoder.Empty())
                {
                    fallbackReason = "requested video codec unavailable in ffmpeg";
                }
                else
                {
                    char cmd[640];
                    if (mFormat == NkPixelFormat::NK_PIXEL_YUYV)
                    {
                        std::snprintf(
                            cmd, sizeof(cmd),
                            "ffmpeg -y -f rawvideo -pix_fmt yuyv422 -s %ux%u -r %u -i - "
                            "-c:v %s \"%s\" 2>/dev/null",
                            mWidth, mHeight, mFPS, encoder.CStr(), config.outputPath.CStr()
                        );
                    }
                    else
                    {
                        // Les frames V4L2 arrivent déjà compressées en MJPEG.
                        std::snprintf(
                            cmd, sizeof(cmd),
                            "ffmpeg -y -f mjpeg -r %u -i - "
                            "-c:v %s \"%s\" 2>/dev/null",
                            mFPS, encoder.CStr(), config.outputPath.CStr()
                        );
                    }

                    mFfmpegPipe = popen(cmd, "w");
                    if (mFfmpegPipe)
                    {
                        mRecordMode  = RecordMode::VideoPipe;
                        mRecordStart = NkChrono::Now();
                        mState       = NkCameraState::NK_CAM_STATE_RECORDING;
                        return true;
                    }
                    fallbackReason = "cannot launch ffmpeg process";
                }
            }
        }

        if (!forceImageSequence && !allowImageFallback)
        {
            mLastError = fallbackReason.Empty()
                       ? "video-only recording requested, but no encoder/path is available"
                       : fallbackReason;
            return false;
        }

        if (!StartImageSequenceRecordLocked(config.outputPath, fallbackReason))
            return false;

        mRecordStart = NkChrono::Now();
        mState       = NkCameraState::NK_CAM_STATE_RECORDING;
        return true;
    }
    void StopVideoRecord() override {
        std::lock_guard<std::mutex> recLock(mRecordMutex);
        if (mRecordMode == RecordMode::VideoPipe && mFfmpegPipe)
        {
            pclose(mFfmpegPipe);
            mFfmpegPipe = nullptr;
        }
        mRecordMode = RecordMode::None;
        mRecordImageDir.Clear();
        mRecordFrameCounter = 0;
        if (mState==NkCameraState::NK_CAM_STATE_RECORDING)
            mState=NkCameraState::NK_CAM_STATE_STREAMING;
    }
    bool  IsRecording() const override { return mState==NkCameraState::NK_CAM_STATE_RECORDING; }
    float GetRecordingDurationSeconds() const override {
        if (!IsRecording()) return 0.f;
        return static_cast<float>((NkChrono::Now() - mRecordStart).seconds);
    }

    // ------------------------------------------------------------------
    // GetOrientation — lecture IIO sysfs (accéléromètre intégré laptop/tablette)
    // ------------------------------------------------------------------
    bool GetOrientation(NkCameraOrientation& out) override
    {
        // Chercher /sys/bus/iio/devices/iio:device*/in_accel_*
        static NkString iioPath;
        if (iioPath.Empty()) {
            DIR* d = opendir("/sys/bus/iio/devices");
            if (d) {
                struct dirent* e;
                while ((e=readdir(d))) {
                    if (strncmp(e->d_name,"iio:device",10)==0) {
                        NkString p = "/sys/bus/iio/devices/";
                        p += e->d_name;
                        // Vérifier si in_accel_x_raw existe
                        NkString xp = p+"/in_accel_x_raw";
                        if (access(xp.CStr(),R_OK)==0) { iioPath=p; break; }
                    }
                }
                closedir(d);
            }
        }
        if (iioPath.Empty()) return false;

        auto readSysfs = [](const NkString& path) -> float {
            std::ifstream f(path.CStr());
            float v=0; f>>v; return v;
        };
        auto readScale = [&]() -> float {
            NkString sp = iioPath+"/in_accel_scale";
            float s = readSysfs(sp);
            return (s==0.f) ? 1.f : s;
        };

        float scale = readScale();
        float ax = readSysfs(iioPath+"/in_accel_x_raw") * scale;
        float ay = readSysfs(iioPath+"/in_accel_y_raw") * scale;
        float az = readSysfs(iioPath+"/in_accel_z_raw") * scale;

        out.accelX = ax;
        out.accelY = ay;
        out.accelZ = az;
        // Calcul pitch/roll depuis accéléromètre
        out.pitch = math::NkToDegrees(math::NkAtan2(ay, math::NkSqrt(ax * ax + az * az)));
        out.roll  = math::NkToDegrees(math::NkAtan2(-ax, az));
        out.yaw   = 0.f; // yaw non disponible sans magnétomètre
        return true;
    }

    uint32         GetWidth()  const override { return mWidth;  }
    uint32         GetHeight() const override { return mHeight; }
    uint32         GetFPS()    const override { return mFPS;    }
    NkPixelFormat GetFormat() const override { return mFormat; }
    NkString   GetLastError() const override { return mLastError; }

private:
    enum class RecordMode : uint8
    {
        None = 0,
        VideoPipe,
        ImageSequence,
    };

    static NkString ToLowerCopy(NkString value)
    {
        for (NkString::SizeType i = 0; i < value.Size(); ++i)
            value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
        return value;
    }

    static bool FfmpegExists()
    {
        static int cached = -1;
        if (cached >= 0) return cached == 1;
        cached = (std::system("ffmpeg -version >/dev/null 2>&1") == 0) ? 1 : 0;
        return cached == 1;
    }

    static NkString GetFfmpegEncoders()
    {
        static bool loaded = false;
        static NkString encoders;
        if (loaded) return encoders;
        loaded = true;
        if (!FfmpegExists()) return encoders;

        FILE* fp = popen("ffmpeg -hide_banner -v error -encoders 2>/dev/null", "r");
        if (!fp) return encoders;

        char line[512];
        while (std::fgets(line, sizeof(line), fp))
            encoders += line;
        pclose(fp);
        return encoders;
    }

    static bool HasEncoder(const NkString& encoder)
    {
        if (encoder.Empty()) return false;
        const NkString all = ToLowerCopy(GetFfmpegEncoders());
        const NkString key = " " + ToLowerCopy(encoder);
        return !all.Empty() && std::strstr(all.CStr(), key.CStr()) != nullptr;
    }

    static NkString ResolveVideoEncoder(const NkString& requestedCodec)
    {
        const NkString c = ToLowerCopy(requestedCodec);
        auto pickFirst = [](const NkVector<NkString>& options) -> NkString
        {
            for (const auto& o : options)
            {
                if (HasEncoder(o)) return o;
            }
            return {};
        };

        if (c.Empty() || c == "h264" || c == "avc") {
            NkVector<NkString> opts;
            opts.PushBack("libx264"); opts.PushBack("h264"); opts.PushBack("mpeg4");
            return pickFirst(opts);
        }
        if (c == "h265" || c == "hevc") {
            NkVector<NkString> opts;
            opts.PushBack("libx265"); opts.PushBack("hevc"); opts.PushBack("mpeg4");
            return pickFirst(opts);
        }
        if (c == "vp9") {
            NkVector<NkString> opts;
            opts.PushBack("libvpx-vp9"); opts.PushBack("vp9"); opts.PushBack("mpeg4");
            return pickFirst(opts);
        }
        if (c == "vp8") {
            NkVector<NkString> opts;
            opts.PushBack("libvpx"); opts.PushBack("vp8"); opts.PushBack("mpeg4");
            return pickFirst(opts);
        }
        if (c == "mpeg4") {
            NkVector<NkString> opts;
            opts.PushBack("mpeg4");
            return pickFirst(opts);
        }
        if (c == "mjpeg" || c == "jpeg") {
            NkVector<NkString> opts;
            opts.PushBack("mjpeg");
            return pickFirst(opts);
        }
        if (HasEncoder(c))
            return c;
        return {};
    }

    bool StartImageSequenceRecordLocked(const NkString& outputPath, const NkString& reason)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path base = outputPath.Empty() ? fs::path("video.mp4") : fs::path(outputPath.CStr());
        std::string stemStd = base.stem().string();
        NkString stem(stemStd.c_str());
        if (stem.Empty()) stem = "video";

        fs::path dir = base.parent_path() / (stemStd + "_frames");
        fs::create_directories(dir, ec);
        if (ec)
        {
            mLastError = NkString("Cannot create image-sequence folder: ") + NkString(dir.string().c_str());
            return false;
        }

        mRecordMode = RecordMode::ImageSequence;
        mRecordImageDir = NkString(dir.string().c_str());
        mRecordFrameCounter = 0;
        if (!reason.Empty())
        {
            logger.Warn("[NkLinuxCameraBackend] {0}. Falling back to image sequence in: {1}",
                        reason.CStr(), mRecordImageDir.CStr());
        }
        return true;
    }

    bool WriteFrameAsPpm(const NkCameraFrame& frame, const NkString& outPath) const
    {
        const uint32 w = frame.width;
        const uint32 h = frame.height;
        if (w == 0 || h == 0) return false;

        NkVector<uint8> rgb; rgb.Resize(static_cast<uint32>(w) * static_cast<uint32>(h) * 3u);
        if (frame.format == NkPixelFormat::NK_PIXEL_YUYV)
        {
            const std::size_t expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 2u;
            if (frame.data.Size() < expected) return false;
            for (uint32 i = 0; i < (w * h / 2u); ++i)
            {
                const float y0 = static_cast<float>(frame.data[i * 4u])     - 16.f;
                const float cb = static_cast<float>(frame.data[i * 4u + 1u]) - 128.f;
                const float y1 = static_cast<float>(frame.data[i * 4u + 2u]) - 16.f;
                const float cr = static_cast<float>(frame.data[i * 4u + 3u]) - 128.f;
                auto cl = [](float v) -> uint8 { return static_cast<uint8>(v < 0.f ? 0.f : (v > 255.f ? 255.f : v)); };
                rgb[i * 6u]     = cl(y0 * 1.164f + cr * 1.596f);
                rgb[i * 6u + 1] = cl(y0 * 1.164f - cb * 0.391f - cr * 0.813f);
                rgb[i * 6u + 2] = cl(y0 * 1.164f + cb * 2.018f);
                rgb[i * 6u + 3] = cl(y1 * 1.164f + cr * 1.596f);
                rgb[i * 6u + 4] = cl(y1 * 1.164f - cb * 0.391f - cr * 0.813f);
                rgb[i * 6u + 5] = cl(y1 * 1.164f + cb * 2.018f);
            }
        }
        else if (frame.format == NkPixelFormat::NK_PIXEL_RGBA8 || frame.format == NkPixelFormat::NK_PIXEL_BGRA8)
        {
            const std::size_t expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
            if (frame.data.Size() < expected) return false;
            for (std::size_t i = 0, o = 0; i < expected; i += 4u, o += 3u)
            {
                if (frame.format == NkPixelFormat::NK_PIXEL_RGBA8)
                {
                    rgb[o]     = frame.data[i];
                    rgb[o + 1] = frame.data[i + 1u];
                    rgb[o + 2] = frame.data[i + 2u];
                }
                else
                {
                    rgb[o]     = frame.data[i + 2u];
                    rgb[o + 1] = frame.data[i + 1u];
                    rgb[o + 2] = frame.data[i];
                }
            }
        }
        else if (frame.format == NkPixelFormat::NK_PIXEL_RGB8)
        {
            if (frame.data.Size() < rgb.Size()) return false;
            std::copy(frame.data.begin(), frame.data.begin() + static_cast<std::ptrdiff_t>(rgb.Size()), rgb.begin());
        }
        else
        {
            return false;
        }

        FILE* fp = std::fopen(outPath.CStr(), "wb");
        if (!fp) return false;
        std::fprintf(fp, "P6\n%u %u\n255\n", w, h);
        const std::size_t wrote = std::fwrite(rgb.Data(), 1, rgb.Size(), fp);
        std::fclose(fp);
        return wrote == rgb.Size();
    }

    bool WriteFrameToImageSequenceLocked(const NkCameraFrame& frame)
    {
        if (mRecordImageDir.Empty() || !frame.IsValid())
            return false;

        const uint32 frameIdx = mRecordFrameCounter++;
        char name[64];
        std::snprintf(name, sizeof(name), "frame_%06u", frameIdx);
        const NkString basePath = mRecordImageDir + "/" + NkString(name);

        if (frame.format == NkPixelFormat::NK_PIXEL_MJPEG)
        {
            const NkString jpgPath = basePath + ".jpg";
            FILE* fp = std::fopen(jpgPath.CStr(), "wb");
            if (!fp) return false;
            const std::size_t wrote = std::fwrite(frame.data.Data(), 1, frame.data.Size(), fp);
            std::fclose(fp);
            return wrote == frame.data.Size();
        }

        return WriteFrameAsPpm(frame, basePath + ".ppm");
    }

    void CaptureLoop()
    {
        uint32 timeoutCount = 0;
        while (mRunning) {
            fd_set fds; FD_ZERO(&fds); FD_SET(mFd,&fds);
            struct timeval tv={1,0};
            int sel = select(mFd+1,&fds,nullptr,nullptr,&tv);
            if (sel == 0) {
                ++timeoutCount;
                if (timeoutCount >= 5 && mLastError.Empty())
                    mLastError = "No camera frame received (select timeout)";
                continue;
            }
            if (sel < 0) {
                if (errno == EINTR) continue;
                mLastError = "select failed: " + NkString(strerror(errno));
                continue;
            }
            timeoutCount = 0;

            struct v4l2_buffer buf={};
            buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory=V4L2_MEMORY_MMAP;
            if (ioctl(mFd,VIDIOC_DQBUF,&buf)<0) {
                if (errno == EAGAIN) continue;
                mLastError = "VIDIOC_DQBUF failed: " + NkString(strerror(errno));
                continue;
            }

            if (buf.index >= mBufs.Size() || !mBufs[buf.index].start || mBufs[buf.index].start == MAP_FAILED) {
                mLastError = "Dequeued invalid buffer index";
                continue;
            }

            const uint8* src=(const uint8*)mBufs[buf.index].start;
            uint32 len = buf.bytesused ? buf.bytesused : static_cast<uint32>(mBufs[buf.index].length);
            if (len == 0) {
                mLastError = "Dequeued empty camera buffer";
                ioctl(mFd,VIDIOC_QBUF,&buf);
                continue;
            }

            NkCameraFrame frame;
            frame.width     =mWidth; frame.height=mHeight;
            frame.format    =mFormat;
            frame.stride    =(mFormat == NkPixelFormat::NK_PIXEL_YUYV) ? (mWidth * 2u) : len;
            frame.frameIndex=mFrameIdx++;
            frame.data.Clear();
            frame.data.Resize(len);
            memcpy(frame.data.Data(), src, len);

            if (ioctl(mFd,VIDIOC_QBUF,&buf)<0)
                mLastError = "VIDIOC_QBUF failed: " + NkString(strerror(errno));

            { std::lock_guard<std::mutex> lk(mMutex);
              mLastFrame=frame; mHasFrame=true; }
            mLastError.Clear();
            if (mFrameCb) mFrameCb(frame);
            bool recordingFailed = false;
            {
                std::lock_guard<std::mutex> recLock(mRecordMutex);
                if (mRecordMode == RecordMode::VideoPipe && mFfmpegPipe)
                {
                    const std::size_t wanted = frame.data.Size();
                    const std::size_t wrote  = std::fwrite(frame.data.Data(), 1, wanted, mFfmpegPipe);
                    if (wrote != wanted || std::ferror(mFfmpegPipe))
                    {
                        mLastError = "Recording pipe write failed (ffmpeg closed or errored)";
                        recordingFailed = true;
                    }
                }
                else if (mRecordMode == RecordMode::ImageSequence)
                {
                    if (!WriteFrameToImageSequenceLocked(frame))
                    {
                        mLastError = "Image-sequence recording write failed";
                        recordingFailed = true;
                    }
                }
            }
            if (recordingFailed)
            {
                StopVideoRecord();
            }
        }
    }

    int           mFd=-1;
    NkCameraState mState=NkCameraState::NK_CAM_STATE_CLOSED;
    uint32         mWidth=0,mHeight=0,mFPS=30,mFrameIdx=0;
    NkPixelFormat mFormat=NkPixelFormat::NK_PIXEL_YUYV;
    NkString   mLastError;

    NkVector<V4L2Buf> mBufs;
    std::thread          mCaptureThread;
    NkAtomicBool              mRunning{false};
    std::mutex           mMutex;
    NkCameraFrame        mLastFrame;
    bool                 mHasFrame=false;

    NkFrameCallback         mFrameCb;
    NkCameraHotPlugCallback mHotPlugCb;

    std::mutex mRecordMutex;
    RecordMode mRecordMode = RecordMode::None;
    FILE* mFfmpegPipe=nullptr;
    NkString mRecordImageDir;
    uint32 mRecordFrameCounter=0;
    NkElapsedTime mRecordStart;
};

} // namespace nkentseu
