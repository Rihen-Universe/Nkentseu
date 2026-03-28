#pragma once

#include "NkRenderTypes.h"

namespace nkentseu {
    namespace renderer {

        // Camera used by the 3D renderer and scene systems.
        struct NkCamera {
            NkVec3f  position   = {0, 0, 5};
            NkVec3f  target     = {0, 0, 0};
            NkVec3f  up         = {0, 1, 0};
            float    fovDeg     = 60.f;
            float    nearPlane  = 0.01f;
            float    farPlane   = 1000.f;
            float    orthoSize  = 5.f;
            bool     isOrtho    = false;

            NkMat4f GetView() const {
                return NkMat4f::LookAt(position, target, up);
            }

            NkMat4f GetProjection(float aspect) const {
                if (isOrtho) {
                    return NkMat4f::Orthogonal(
                        NkVec2f(-orthoSize * aspect, -orthoSize),
                        NkVec2f( orthoSize * aspect,  orthoSize),
                        nearPlane, farPlane, true);
                }
                return NkMat4f::Perspective(NkAngle(fovDeg), aspect, nearPlane, farPlane);
            }
        };

    } // namespace renderer
} // namespace nkentseu
