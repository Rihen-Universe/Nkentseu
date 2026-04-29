#pragma once
// =============================================================================
// Demo.h  — NKRenderer v4.0 — Démos d'utilisation
//
// Démos couvertes :
//   Demo01_Basic3D       — PBR basique, lumières, ombres
//   Demo02_Game2D        — jeu 2D, sprites, text, UI
//   Demo03_Film          — rendu cinématique, DOF, motion blur
//   Demo04_Archviz       — visualisation architecturale
//   Demo05_Animation     — skinning GPU, onion skin, squelette
//   Demo06_VFX           — particules, trails, decals
//   Demo07_Offscreen     — RTT, minimap, export image
//   Demo08_PV3DE         — patient virtuel 3D émotif (NkSimulationRenderer)
//   Demo09_MultiViewport — éditeur 3D multi-viewports
//   Demo10_NkSL          — shader NkSL custom
// =============================================================================
#include "../NkRenderer.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkContext.h"
#include "NKEvent/NkEventSystem.h"

namespace nkentseu {
namespace renderer {
namespace demo {

    // Classe de base commune à toutes les démos
    class DemoBase {
    public:
        virtual ~DemoBase() = default;
        virtual bool Init(NkIDevice* device, const NkSurfaceDesc& surface,
                           uint32 width, uint32 height) = 0;
        virtual void Update(float32 dt) = 0;
        virtual void Render() = 0;
        virtual void Shutdown() = 0;
        virtual const char* GetName() const = 0;

    protected:
        NkRenderer* mRenderer = nullptr;
        uint32      mWidth    = 0;
        uint32      mHeight   = 0;
        float32     mTime     = 0.f;
    };

    // Démos individuelles
    class Demo01_Basic3D    : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "01 — PBR 3D Basic"; }
    private:
        NkMeshHandle        mMeshSphere, mMeshCube, mMeshPlane;
        NkMaterialInstance* mMatGold  = nullptr;
        NkMaterialInstance* mMatRed   = nullptr;
        NkMaterialInstance* mMatFloor = nullptr;
        NkTexHandle         mEnvMap;
        float32             mAngle    = 0.f;
    };

    class Demo02_Game2D     : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "02 — 2D Game"; }
    private:
        NkTexHandle  mSprite, mTileset, mUI;
        NkFontHandle mFont;
        float32      mSpriteX = 100.f;
        int32        mScore   = 0;
    };

    class Demo03_Film       : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "03 — Film / Cinematic"; }
    private:
        NkMeshHandle        mMeshChar;
        NkMaterialInstance* mMatSkin   = nullptr;
        NkVec3f             mCamPos    = {0, 1.5f, 3.f};
        float32             mCamYaw    = 0.f;
    };

    class Demo04_Archviz    : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "04 — Archviz"; }
    private:
        NkMeshHandle        mMeshRoom;
        NkMaterialInstance* mMatWall   = nullptr;
        NkMaterialInstance* mMatFloor  = nullptr;
        NkMaterialInstance* mMatGlass  = nullptr;
        NkTexHandle         mHDRI;
    };

    class Demo05_Animation  : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "05 — Animation / Skinning"; }
    private:
        NkMeshHandle        mMeshChar;
        NkMaterialInstance* mMatChar  = nullptr;
        NkVector<NkMat4f>   mBones;
        int32               mParents[64] = {};
        uint32              mBoneCount   = 0;
        float32             mAnimTime    = 0.f;
        bool                mShowOnion   = true;
        bool                mShowSkeleton= true;
        int32               mCurrentFrame= 0;
    };

    class Demo06_VFX        : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "06 — VFX Particles"; }
    private:
        NkEmitterId mFireEmitter, mSmokeEmitter, mSparkEmitter;
        NkTrailId   mTrail;
        NkDecalId   mScorchDecal;
        NkTexHandle mFireTex, mSmokeTex, mSparkTex;
        NkVec3f     mTrailPos   = {0, 0.5f, 0};
        float32     mTrailAngle = 0.f;
    };

    class Demo07_Offscreen  : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "07 — Offscreen RTT"; }
    private:
        NkOffscreenTarget*  mMinimap   = nullptr;
        NkOffscreenTarget*  mPortal    = nullptr;
        NkMeshHandle        mMeshScene;
        NkMaterialInstance* mMatScene  = nullptr;
        float32             mAngle     = 0.f;
    };

    class Demo08_PV3DE      : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "08 — PV3DE Patient Virtuel"; }
    private:
        NkMeshHandle        mMeshHead, mMeshBody;
        NkMaterialInstance* mMatSkin   = nullptr;
        NkMaterialInstance* mMatEyes   = nullptr;
        NkVector<NkMat4f>   mBones;
        NkBlendShapeState   mBS;
        NkEmotionState      mEmotion;
        NkEmitterId         mTearEmitter;
        float32             mEmotionTime= 0.f;
    };

    class Demo09_MultiVP    : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "09 — Multi-Viewport Editor"; }
    private:
        NkOffscreenTarget* mViewports[4] = {};
        NkMeshHandle       mMeshScene;
        NkMaterialInstance*mMat         = nullptr;
        // Caméras pour les 4 vues (top, front, side, perspective)
        NkCamera3D         mCamPersp;
        NkCamera3D         mCamTop;
        NkCamera3D         mCamFront;
        NkCamera3D         mCamSide;
    };

    class Demo10_NkSL       : public DemoBase {
    public:
        bool  Init(NkIDevice*, const NkSurfaceDesc&, uint32, uint32) override;
        void  Update(float32 dt) override;
        void  Render() override;
        void  Shutdown() override;
        const char* GetName() const override { return "10 — Custom NkSL Shader"; }
    private:
        NkMeshHandle        mMesh;
        NkMatHandle         mCustomTemplate;
        NkMaterialInstance* mCustomMat  = nullptr;
        float32             mWaveTime   = 0.f;
    };

} // namespace demo
} // namespace renderer
} // namespace nkentseu
