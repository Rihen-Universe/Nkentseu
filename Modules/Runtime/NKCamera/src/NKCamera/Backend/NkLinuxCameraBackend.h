#pragma once
// =============================================================================
// NkLinuxCameraBackend.h — V4L2 (Video4Linux2) — COMPLET ET FONCTIONNEL
// GetOrientation: via /sys/bus/iio/devices/iio:device* (accéléromètre intégré)
// Vidéo: ffmpeg pipe (MP4/H.264) ou écriture RAW si ffmpeg absent
// =============================================================================
#include "NKCamera/INkCameraBackend.h"

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
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
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
    std::vector<NkCameraDevice> EnumerateDevices() override
    {
        std::vector<NkCameraDevice> result;
        std::vector<std::string> paths;
        DIR* dir = opendir("/dev");
        if (!dir) return result;
        struct dirent* ent;
        while ((ent = readdir(dir)))
            if (strncmp(ent->d_name,"video",5)==0) {
                std::string p = "/dev/";  p += ent->d_name;
                paths.push_back(p);
            }
        closedir(dir);
        std::sort(paths.begin(), paths.end());

        NkU32 idx = 0;
        for (const auto& path : paths) {
            int fd = open(path.c_str(), O_RDWR|O_NONBLOCK);
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
                    if (m.width>0&&m.height>0) dev.modes.push_back(m);
                    ++fs.index;
                }
                ++fd2.index;
            }
            close(fd);
            result.push_back(std::move(dev));
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
        mLastError.clear();
        if (mFd >= 0) StopStreaming();
        auto devs = EnumerateDevices();
        if (config.deviceIndex >= devs.size())
        { mLastError="Device index "+std::to_string(config.deviceIndex)+" out of range"; return false; }

        const std::string& path = devs[config.deviceIndex].id;
        mFd = open(path.c_str(), O_RDWR);
        if (mFd < 0) { mLastError="Cannot open "+path+": "+strerror(errno); return false; }

        auto failAndCleanup = [&](const std::string& err) -> bool
        {
            mLastError = err;
            for (auto& b : mBufs)
            {
                if (b.start && b.start != MAP_FAILED)
                    munmap(b.start, b.length);
            }
            mBufs.clear();
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
        for (const NkU32 pixfmt : { static_cast<NkU32>(V4L2_PIX_FMT_MJPEG), static_cast<NkU32>(V4L2_PIX_FMT_YUYV) })
        {
            fmt.fmt.pix.pixelformat = pixfmt;
            if (ioctl(mFd, VIDIOC_S_FMT, &fmt) == 0)
            {
                formatSet = true;
                break;
            }
        }
        if (!formatSet)
            return failAndCleanup("VIDIOC_S_FMT failed: " + std::string(strerror(errno)));

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
            return failAndCleanup("VIDIOC_REQBUFS failed: " + std::string(strerror(errno)));

        mBufs.resize(req.count);
        for (NkU32 i=0;i<req.count;++i) {
            struct v4l2_buffer buf={};
            buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE; buf.memory=V4L2_MEMORY_MMAP; buf.index=i;
            if (ioctl(mFd,VIDIOC_QUERYBUF,&buf)<0)
                return failAndCleanup("VIDIOC_QUERYBUF failed: " + std::string(strerror(errno)));
            mBufs[i].length = buf.length;
            mBufs[i].start  = mmap(nullptr,buf.length,PROT_READ|PROT_WRITE, MAP_SHARED,mFd,buf.m.offset);
            if (mBufs[i].start == MAP_FAILED)
                return failAndCleanup("mmap failed: " + std::string(strerror(errno)));
            if (ioctl(mFd,VIDIOC_QBUF,&buf)<0)
                return failAndCleanup("VIDIOC_QBUF failed: " + std::string(strerror(errno)));
        }

        v4l2_buf_type t=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mFd,VIDIOC_STREAMON,&t)<0)
            return failAndCleanup("VIDIOC_STREAMON failed: " + std::string(strerror(errno)));

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
            mBufs.clear();
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
    bool CapturePhotoToFile(const std::string& path) override {
        NkPhotoCaptureResult r; if (!CapturePhoto(r)) return false;
        // YUYV → RGBA → PPM (portabie, sans dépendance externe)
        auto& f = r.frame;
        NkU32 w=f.width,h=f.height;
        std::vector<NkU8> rgb(w*h*3);
        if (f.format==NkPixelFormat::NK_PIXEL_YUYV) {
            for (NkU32 i=0;i<w*h/2;++i) {
                float y0=(float)f.data[i*4]-16.f,cb=(float)f.data[i*4+1]-128.f;
                float y1=(float)f.data[i*4+2]-16.f,cr=(float)f.data[i*4+3]-128.f;
                auto cl=[](float v)->NkU8{return(NkU8)(v<0?0:v>255?255:v);};
                rgb[i*6]  =cl(y0*1.164f+cr*1.596f);
                rgb[i*6+1]=cl(y0*1.164f-cb*0.391f-cr*0.813f);
                rgb[i*6+2]=cl(y0*1.164f+cb*2.018f);
                rgb[i*6+3]=cl(y1*1.164f+cr*1.596f);
                rgb[i*6+4]=cl(y1*1.164f-cb*0.391f-cr*0.813f);
                rgb[i*6+5]=cl(y1*1.164f+cb*2.018f);
            }
        }
        // Écrire PPM (compatible universellement)
        std::string out = path.empty() ? "photo.ppm" : path;
        auto dot=out.rfind('.');
        if (dot!=std::string::npos) out=out.substr(0,dot)+".ppm";
        FILE* fp=fopen(out.c_str(),"wb");
        if (!fp) return false;
        fprintf(fp,"P6\n%d %d\n255\n",(int)w,(int)h);
        fwrite(rgb.data(),1,rgb.size(),fp);
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

        const std::string requestedCodec = ToLowerCopy(config.videoCodec);
        const bool forceImageSequence = (config.mode == NkVideoRecordConfig::Mode::IMAGE_SEQUENCE_ONLY) ||
                                        (requestedCodec == "images" ||
                                         requestedCodec == "image-sequence" ||
                                         requestedCodec == "sequence" ||
                                         requestedCodec == "frames");
        const bool allowImageFallback = (config.mode == NkVideoRecordConfig::Mode::AUTO);

        // Empêche l'arrêt brutal du process si ffmpeg ferme le pipe (SIGPIPE).
        std::signal(SIGPIPE, SIG_IGN);

        mLastError.clear();
        mRecordImageDir.clear();
        mRecordFrameCounter = 0;

        std::string fallbackReason;
        if (!forceImageSequence)
        {
            if (!FfmpegExists())
            {
                fallbackReason = "ffmpeg not found in PATH";
            }
            else
            {
                const std::string encoder = ResolveVideoEncoder(requestedCodec);
                if (encoder.empty())
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
                            mWidth, mHeight, mFPS, encoder.c_str(), config.outputPath.c_str()
                        );
                    }
                    else
                    {
                        // Les frames V4L2 arrivent déjà compressées en MJPEG.
                        std::snprintf(
                            cmd, sizeof(cmd),
                            "ffmpeg -y -f mjpeg -r %u -i - "
                            "-c:v %s \"%s\" 2>/dev/null",
                            mFPS, encoder.c_str(), config.outputPath.c_str()
                        );
                    }

                    mFfmpegPipe = popen(cmd, "w");
                    if (mFfmpegPipe)
                    {
                        mRecordMode  = RecordMode::VideoPipe;
                        mRecordStart = std::chrono::steady_clock::now();
                        mState       = NkCameraState::NK_CAM_STATE_RECORDING;
                        return true;
                    }
                    fallbackReason = "cannot launch ffmpeg process";
                }
            }
        }

        if (!forceImageSequence && !allowImageFallback)
        {
            mLastError = fallbackReason.empty()
                       ? "video-only recording requested, but no encoder/path is available"
                       : fallbackReason;
            return false;
        }

        if (!StartImageSequenceRecordLocked(config.outputPath, fallbackReason))
            return false;

        mRecordStart = std::chrono::steady_clock::now();
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
        mRecordImageDir.clear();
        mRecordFrameCounter = 0;
        if (mState==NkCameraState::NK_CAM_STATE_RECORDING)
            mState=NkCameraState::NK_CAM_STATE_STREAMING;
    }
    bool  IsRecording() const override { return mState==NkCameraState::NK_CAM_STATE_RECORDING; }
    float GetRecordingDurationSeconds() const override {
        if (!IsRecording()) return 0.f;
        return std::chrono::duration<float>(
            std::chrono::steady_clock::now()-mRecordStart).count();
    }

    // ------------------------------------------------------------------
    // GetOrientation — lecture IIO sysfs (accéléromètre intégré laptop/tablette)
    // ------------------------------------------------------------------
    bool GetOrientation(NkCameraOrientation& out) override
    {
        // Chercher /sys/bus/iio/devices/iio:device*/in_accel_*
        static std::string iioPath;
        if (iioPath.empty()) {
            DIR* d = opendir("/sys/bus/iio/devices");
            if (d) {
                struct dirent* e;
                while ((e=readdir(d))) {
                    if (strncmp(e->d_name,"iio:device",10)==0) {
                        std::string p = "/sys/bus/iio/devices/";
                        p += e->d_name;
                        // Vérifier si in_accel_x_raw existe
                        std::string xp = p+"/in_accel_x_raw";
                        if (access(xp.c_str(),R_OK)==0) { iioPath=p; break; }
                    }
                }
                closedir(d);
            }
        }
        if (iioPath.empty()) return false;

        auto readSysfs = [](const std::string& path) -> float {
            std::ifstream f(path);
            float v=0; f>>v; return v;
        };
        auto readScale = [&]() -> float {
            std::string sp = iioPath+"/in_accel_scale";
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
        out.pitch = std::atan2(ay, std::sqrt(ax*ax+az*az)) * (180.f/3.14159f);
        out.roll  = std::atan2(-ax, az) * (180.f/3.14159f);
        out.yaw   = 0.f; // yaw non disponible sans magnétomètre
        return true;
    }

    NkU32         GetWidth()  const override { return mWidth;  }
    NkU32         GetHeight() const override { return mHeight; }
    NkU32         GetFPS()    const override { return mFPS;    }
    NkPixelFormat GetFormat() const override { return mFormat; }
    std::string   GetLastError() const override { return mLastError; }

private:
    enum class RecordMode : NkU8
    {
        None = 0,
        VideoPipe,
        ImageSequence,
    };

    static std::string ToLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
        );
        return value;
    }

    static bool FfmpegExists()
    {
        static int cached = -1;
        if (cached >= 0) return cached == 1;
        cached = (std::system("ffmpeg -version >/dev/null 2>&1") == 0) ? 1 : 0;
        return cached == 1;
    }

    static std::string GetFfmpegEncoders()
    {
        static bool loaded = false;
        static std::string encoders;
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

    static bool HasEncoder(const std::string& encoder)
    {
        if (encoder.empty()) return false;
        const std::string all = ToLowerCopy(GetFfmpegEncoders());
        const std::string key = " " + ToLowerCopy(encoder);
        return !all.empty() && all.find(key) != std::string::npos;
    }

    static std::string ResolveVideoEncoder(const std::string& requestedCodec)
    {
        const std::string c = ToLowerCopy(requestedCodec);
        auto pickFirst = [](const std::vector<std::string>& options) -> std::string
        {
            for (const auto& o : options)
            {
                if (HasEncoder(o)) return o;
            }
            return {};
        };

        if (c.empty() || c == "h264" || c == "avc")
            return pickFirst({ "libx264", "h264", "mpeg4" });
        if (c == "h265" || c == "hevc")
            return pickFirst({ "libx265", "hevc", "mpeg4" });
        if (c == "vp9")
            return pickFirst({ "libvpx-vp9", "vp9", "mpeg4" });
        if (c == "vp8")
            return pickFirst({ "libvpx", "vp8", "mpeg4" });
        if (c == "mpeg4")
            return pickFirst({ "mpeg4" });
        if (c == "mjpeg" || c == "jpeg")
            return pickFirst({ "mjpeg" });
        if (HasEncoder(c))
            return c;
        return {};
    }

    bool StartImageSequenceRecordLocked(const std::string& outputPath, const std::string& reason)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path base = outputPath.empty() ? fs::path("video.mp4") : fs::path(outputPath);
        std::string stem = base.stem().string();
        if (stem.empty()) stem = "video";

        fs::path dir = base.parent_path() / (stem + "_frames");
        fs::create_directories(dir, ec);
        if (ec)
        {
            mLastError = "Cannot create image-sequence folder: " + dir.string();
            return false;
        }

        mRecordMode = RecordMode::ImageSequence;
        mRecordImageDir = dir.string();
        mRecordFrameCounter = 0;
        if (!reason.empty())
        {
            std::fprintf(
                stderr,
                "[NkLinuxCameraBackend] %s. Falling back to image sequence in: %s\n",
                reason.c_str(),
                mRecordImageDir.c_str()
            );
        }
        return true;
    }

    bool WriteFrameAsPpm(const NkCameraFrame& frame, const std::string& outPath) const
    {
        const NkU32 w = frame.width;
        const NkU32 h = frame.height;
        if (w == 0 || h == 0) return false;

        std::vector<NkU8> rgb(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
        if (frame.format == NkPixelFormat::NK_PIXEL_YUYV)
        {
            const std::size_t expected = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 2u;
            if (frame.data.size() < expected) return false;
            for (NkU32 i = 0; i < (w * h / 2u); ++i)
            {
                const float y0 = static_cast<float>(frame.data[i * 4u])     - 16.f;
                const float cb = static_cast<float>(frame.data[i * 4u + 1u]) - 128.f;
                const float y1 = static_cast<float>(frame.data[i * 4u + 2u]) - 16.f;
                const float cr = static_cast<float>(frame.data[i * 4u + 3u]) - 128.f;
                auto cl = [](float v) -> NkU8 { return static_cast<NkU8>(v < 0.f ? 0.f : (v > 255.f ? 255.f : v)); };
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
            if (frame.data.size() < expected) return false;
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
            if (frame.data.size() < rgb.size()) return false;
            std::copy(frame.data.begin(), frame.data.begin() + static_cast<std::ptrdiff_t>(rgb.size()), rgb.begin());
        }
        else
        {
            return false;
        }

        FILE* fp = std::fopen(outPath.c_str(), "wb");
        if (!fp) return false;
        std::fprintf(fp, "P6\n%u %u\n255\n", w, h);
        const std::size_t wrote = std::fwrite(rgb.data(), 1, rgb.size(), fp);
        std::fclose(fp);
        return wrote == rgb.size();
    }

    bool WriteFrameToImageSequenceLocked(const NkCameraFrame& frame)
    {
        if (mRecordImageDir.empty() || !frame.IsValid())
            return false;

        const NkU32 frameIdx = mRecordFrameCounter++;
        char name[64];
        std::snprintf(name, sizeof(name), "frame_%06u", frameIdx);
        const std::string basePath = mRecordImageDir + "/" + std::string(name);

        if (frame.format == NkPixelFormat::NK_PIXEL_MJPEG)
        {
            const std::string jpgPath = basePath + ".jpg";
            FILE* fp = std::fopen(jpgPath.c_str(), "wb");
            if (!fp) return false;
            const std::size_t wrote = std::fwrite(frame.data.data(), 1, frame.data.size(), fp);
            std::fclose(fp);
            return wrote == frame.data.size();
        }

        return WriteFrameAsPpm(frame, basePath + ".ppm");
    }

    void CaptureLoop()
    {
        NkU32 timeoutCount = 0;
        while (mRunning) {
            fd_set fds; FD_ZERO(&fds); FD_SET(mFd,&fds);
            struct timeval tv={1,0};
            int sel = select(mFd+1,&fds,nullptr,nullptr,&tv);
            if (sel == 0) {
                ++timeoutCount;
                if (timeoutCount >= 5 && mLastError.empty())
                    mLastError = "No camera frame received (select timeout)";
                continue;
            }
            if (sel < 0) {
                if (errno == EINTR) continue;
                mLastError = "select failed: " + std::string(strerror(errno));
                continue;
            }
            timeoutCount = 0;

            struct v4l2_buffer buf={};
            buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory=V4L2_MEMORY_MMAP;
            if (ioctl(mFd,VIDIOC_DQBUF,&buf)<0) {
                if (errno == EAGAIN) continue;
                mLastError = "VIDIOC_DQBUF failed: " + std::string(strerror(errno));
                continue;
            }

            if (buf.index >= mBufs.size() || !mBufs[buf.index].start || mBufs[buf.index].start == MAP_FAILED) {
                mLastError = "Dequeued invalid buffer index";
                continue;
            }

            const NkU8* src=(const NkU8*)mBufs[buf.index].start;
            NkU32 len = buf.bytesused ? buf.bytesused : static_cast<NkU32>(mBufs[buf.index].length);
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
            frame.data.assign(src,src+len);

            if (ioctl(mFd,VIDIOC_QBUF,&buf)<0)
                mLastError = "VIDIOC_QBUF failed: " + std::string(strerror(errno));

            { std::lock_guard<std::mutex> lk(mMutex);
              mLastFrame=frame; mHasFrame=true; }
            mLastError.clear();
            if (mFrameCb) mFrameCb(frame);
            bool recordingFailed = false;
            {
                std::lock_guard<std::mutex> recLock(mRecordMutex);
                if (mRecordMode == RecordMode::VideoPipe && mFfmpegPipe)
                {
                    const std::size_t wanted = frame.data.size();
                    const std::size_t wrote  = std::fwrite(frame.data.data(), 1, wanted, mFfmpegPipe);
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
    NkU32         mWidth=0,mHeight=0,mFPS=30,mFrameIdx=0;
    NkPixelFormat mFormat=NkPixelFormat::NK_PIXEL_YUYV;
    std::string   mLastError;

    std::vector<V4L2Buf> mBufs;
    std::thread          mCaptureThread;
    std::atomic<bool>    mRunning{false};
    std::mutex           mMutex;
    NkCameraFrame        mLastFrame;
    bool                 mHasFrame=false;

    NkFrameCallback         mFrameCb;
    NkCameraHotPlugCallback mHotPlugCb;

    std::mutex mRecordMutex;
    RecordMode mRecordMode = RecordMode::None;
    FILE* mFfmpegPipe=nullptr;
    std::string mRecordImageDir;
    NkU32 mRecordFrameCounter=0;
    std::chrono::steady_clock::time_point mRecordStart;
};

} // namespace nkentseu
