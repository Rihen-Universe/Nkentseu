// =============================================================================
// NkRenderer2DTypes.cpp — Matrix math for 2D renderer
// Orthographic projection (Y-down, maps to NDC [-1,+1])
// Column-major storage compatible with GLSL, HLSL (transposed at upload), MSL
// =============================================================================
#include "NkRenderer2DTypes.h"
#include <cmath>

namespace nkentseu {
    namespace renderer {

        // ── Helper: build a column-major 4x4 identity ────────────────────────────────
        static void MatIdentity(float32 m[16]) {
            m[ 0]=1; m[ 1]=0; m[ 2]=0; m[ 3]=0;
            m[ 4]=0; m[ 5]=1; m[ 6]=0; m[ 7]=0;
            m[ 8]=0; m[ 9]=0; m[10]=1; m[11]=0;
            m[12]=0; m[13]=0; m[14]=0; m[15]=1;
        }

        // =============================================================================
        // NkTransform2D::ToMatrix4
        // Produces a column-major TRS matrix:
        //   T(position) * R(rotation) * S(scale) * T(-origin)
        // =============================================================================
        void NkTransform2D::ToMatrix4(float32 m[16]) const {
            const float32 cos_r = cosf(rotation);
            const float32 sin_r = sinf(rotation);

            // Combined TRS (translate after rotate+scale to bake origin pivot)
            // Column-major layout: m[col*4 + row]
            m[ 0] =  cos_r * scale.x;
            m[ 1] =  sin_r * scale.x;
            m[ 2] =  0.f;
            m[ 3] =  0.f;

            m[ 4] = -sin_r * scale.y;
            m[ 5] =  cos_r * scale.y;
            m[ 6] =  0.f;
            m[ 7] =  0.f;

            m[ 8] =  0.f;
            m[ 9] =  0.f;
            m[10] =  1.f;
            m[11] =  0.f;

            // Translation: position - R*S*origin
            m[12] = position.x - m[0]*origin.x - m[4]*origin.y;
            m[13] = position.y - m[1]*origin.x - m[5]*origin.y;
            m[14] = 0.f;
            m[15] = 1.f;
        }

        // =============================================================================
        // NkView2D::ToProjectionMatrix
        // Orthographic projection for 2D:
        //   left   = center.x - size.x/2
        //   right  = center.x + size.x/2
        //   top    = center.y - size.y/2   (Y-down: top < bottom in world space)
        //   bottom = center.y + size.y/2
        //
        // Maps to NDC [-1,+1] x [-1,+1] x [0,1] (DX convention)
        // OpenGL backends should negate the Y scale or flip in shader.
        // We use DX-style here and handle OpenGL flip in the GL backend's shader
        // by simply passing the same matrix (GLSL's gl_Position.y = -pos.y is
        // handled by the vertex shader which can set y = -y, or we just produce
        // a Y-down NDC and accept it — OpenGL will still render correctly since
        // we flip the V coordinate in UV space).
        //
        // Column-major output (compatible with glUniformMatrix4fv with transpose=GL_FALSE,
        // and with HLSL cbuffer when uploaded as-is since HLSL is row-major — we
        // compensate by transposing at upload in the DX backends).
        // =============================================================================
        void NkView2D::ToProjectionMatrix(float32 m[16]) const {
            // Apply rotation to the view
            const float32 cos_r = cosf(rotation);
            const float32 sin_r = sinf(rotation);

            const float32 half_w = size.x * 0.5f;
            const float32 half_h = size.y * 0.5f;

            // Without rotation: straightforward ortho
            // With rotation: apply 2D rotation to the orthographic axes
            const float32 a =  2.f / size.x;
            const float32 b = -2.f / size.y;    // Y-down (positive Y goes down in screen space)
            const float32 c = -center.x * a;
            const float32 d = -center.y * b;

            // Rotation applied to the ortho basis:
            m[ 0] =  a * cos_r;
            m[ 1] =  a * sin_r;
            m[ 2] =  0.f;
            m[ 3] =  0.f;

            m[ 4] =  b * (-sin_r);
            m[ 5] =  b * cos_r;
            m[ 6] =  0.f;
            m[ 7] =  0.f;

            m[ 8] =  0.f;
            m[ 9] =  0.f;
            m[10] =  1.f;
            m[11] =  0.f;

            m[12] = c * cos_r + d * (-sin_r);
            m[13] = c * sin_r + d *   cos_r;
            m[14] = 0.f;
            m[15] = 1.f;
        }

    } // namespace renderer
} // namespace nkentseu