#pragma once
// =============================================================================
// NkCamera2D.h — Caméra 2D minimale pour le mapping IMU de NkCameraSystem
//
// Cette classe est volontairement légère et sans backend graphique :
// elle stocke seulement position + rotation, utilisées par
// NkCameraSystem::UpdateVirtualCamera().
// =============================================================================

namespace nkentseu
{

    class NkCamera2D
    {
        public:
            void SetPosition(float x, float y)
            {
                mX = x;
                mY = y;
            }

            void SetRotation(float degrees)
            {
                mRotationDeg = degrees;
            }

            float GetX() const { return mX; }
            float GetY() const { return mY; }
            float GetRotation() const { return mRotationDeg; }

        private:
            float mX = 0.f;
            float mY = 0.f;
            float mRotationDeg = 0.f;
    };

} // namespace nkentseu
