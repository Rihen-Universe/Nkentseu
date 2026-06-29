#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::math;

TEST_CASE(NKmathmoke, ScalarFunctions) {
    ASSERT_NEAR(3.0f, NkSqrt(9.0f), 0.0001f);
    ASSERT_NEAR(8.0f, NkPowInt(2.0f, 3), 0.0001f);
    ASSERT_NEAR(180.0f, NkToDegrees(PI_F), 0.001f);
    ASSERT_EQUAL(16u, static_cast<unsigned int>(NkNextPowerOf2(13u)));
}

TEST_CASE(NKmathmoke, VectorAndRectTypes) {
    NkVec2f v1(2.0f, 3.0f);
    NkVec2f v2(1.0f, 5.0f);

    NkVec2f v3 = v1 + v2;
    ASSERT_NEAR(3.0f, v3.x, 0.0001f);
    ASSERT_NEAR(8.0f, v3.y, 0.0001f);

    NkRect r(10, 20, 100, 50);
    ASSERT_TRUE(r.Contains(20, 40));
    ASSERT_FALSE(r.Contains(200, 40));
}

TEST_CASE(NKmathmoke, BitAndIntegerUtilities) {
    ASSERT_TRUE(NkIsPowerOf2(16u));
    ASSERT_FALSE(NkIsPowerOf2(18u));

    ASSERT_EQUAL(1u, static_cast<unsigned int>(NkNextPowerOf2(0u)));
    ASSERT_EQUAL(16u, static_cast<unsigned int>(NkNextPowerOf2(13u)));

    ASSERT_EQUAL(31u, static_cast<unsigned int>(NkClz(static_cast<nkentseu::nk_uint32>(1u))));
    ASSERT_EQUAL(0u, static_cast<unsigned int>(NkCtz(static_cast<nkentseu::nk_uint32>(1u))));
    ASSERT_EQUAL(4u, static_cast<unsigned int>(NkPopcount(static_cast<nkentseu::nk_uint32>(0b10110100u))));
}

TEST_CASE(NKmathmoke, DivisionAndInterpolationEdges) {
    const DivResult64 normal = NkDivI64(10, 3);
    ASSERT_EQUAL(3, static_cast<int>(normal.quot));
    ASSERT_EQUAL(1, static_cast<int>(normal.rem));

    const DivResult64 divideByZero = NkDivI64(10, 0);
    ASSERT_EQUAL(0, static_cast<int>(divideByZero.quot));
    ASSERT_EQUAL(10, static_cast<int>(divideByZero.rem));

    ASSERT_NEAR(0.0f, NkSmoothstep(-0.25f), 0.0001f);
    ASSERT_NEAR(1.0f, NkSmoothstep(2.0f), 0.0001f);
    ASSERT_NEAR(0.5f, NkSmoothstep(0.5f), 0.0001f);

    ASSERT_NEAR(0.0f, NkSmootherstep(-0.25f), 0.0001f);
    ASSERT_NEAR(1.0f, NkSmootherstep(2.0f), 0.0001f);
    ASSERT_NEAR(0.5f, NkSmootherstep(0.5f), 0.0001f);
}

// =============================================================================
// Conversion quaternion -> matrice (convention M*v de NkMat4f)
//
// Regression du bug : operator NkMat4T passait R colonne-par-colonne dans un
// constructeur row-major -> on obtenait R^T (rotation inversee). Ces tests
// verifient que static_cast<NkMat4f>(quat) == NkMat4f::Rotation*(angle) et que
// M*v tourne les vecteurs dans le bon sens.
// =============================================================================
TEST_CASE(NKmathmoke, QuaternionToMatrix) {
    const float32 eps = 0.0005f;

    // --- RotateZ(90deg) : (1,0,0) -> (0,1,0), (0,1,0) -> (-1,0,0) ----------
    {
        NkMat4f M = static_cast<NkMat4f>(NkQuatf::RotateZ(NkAngle(90.0f)));
        NkVec3f rx = M * NkVec3f(1.0f, 0.0f, 0.0f);
        NkVec3f ry = M * NkVec3f(0.0f, 1.0f, 0.0f);
        ASSERT_NEAR(0.0f, rx.x, eps); ASSERT_NEAR(1.0f, rx.y, eps); ASSERT_NEAR(0.0f, rx.z, eps);
        ASSERT_NEAR(-1.0f, ry.x, eps); ASSERT_NEAR(0.0f, ry.y, eps); ASSERT_NEAR(0.0f, ry.z, eps);
    }

    // --- RotateX(90deg) : (0,1,0) -> (0,0,1)  (coherent NkMat4f::RotationX) -
    {
        NkMat4f M = static_cast<NkMat4f>(NkQuatf::RotateX(NkAngle(90.0f)));
        NkVec3f ry = M * NkVec3f(0.0f, 1.0f, 0.0f);
        ASSERT_NEAR(0.0f, ry.x, eps); ASSERT_NEAR(0.0f, ry.y, eps); ASSERT_NEAR(1.0f, ry.z, eps);
    }

    // --- RotateY(90deg) : (0,0,1) -> (1,0,0)  (coherent NkMat4f::RotationY) -
    {
        NkMat4f M = static_cast<NkMat4f>(NkQuatf::RotateY(NkAngle(90.0f)));
        NkVec3f rz = M * NkVec3f(0.0f, 0.0f, 1.0f);
        ASSERT_NEAR(1.0f, rz.x, eps); ASSERT_NEAR(0.0f, rz.y, eps); ASSERT_NEAR(0.0f, rz.z, eps);
    }

    // --- Egalite quat->mat == NkMat4f::Rotation* pour plusieurs angles -----
    const float32 angles[] = { 17.0f, 45.0f, 90.0f, 123.0f, -60.0f };
    for (float32 deg : angles) {
        NkAngle a = NkAngle(deg);

        NkMat4f qz = static_cast<NkMat4f>(NkQuatf::RotateZ(a));
        NkMat4f mz = NkMat4f::RotationZ(a);
        for (int i = 0; i < 16; ++i) ASSERT_NEAR(mz.data[i], qz.data[i], eps);

        NkMat4f qx = static_cast<NkMat4f>(NkQuatf::RotateX(a));
        NkMat4f mx = NkMat4f::RotationX(a);
        for (int i = 0; i < 16; ++i) ASSERT_NEAR(mx.data[i], qx.data[i], eps);

        NkMat4f qy = static_cast<NkMat4f>(NkQuatf::RotateY(a));
        NkMat4f my = NkMat4f::RotationY(a);
        for (int i = 0; i < 16; ++i) ASSERT_NEAR(my.data[i], qy.data[i], eps);
    }

    // --- La matrice issue du quaternion est orthonormale (rotation pure) ---
    // (colonnes unitaires + orthogonales entre elles). Garantit qu'on a bien
    // une rotation et pas une matrice degeneree/transposee-mais-non-rotation.
    {
        NkMat4f M = static_cast<NkMat4f>(NkQuatf::RotateY(NkAngle(57.0f)));
        NkVec3f c0(M.data[0], M.data[1], M.data[2]);   // colonne 0
        NkVec3f c1(M.data[4], M.data[5], M.data[6]);   // colonne 1
        NkVec3f c2(M.data[8], M.data[9], M.data[10]);  // colonne 2
        ASSERT_NEAR(1.0f, c0.Dot(c0), eps);
        ASSERT_NEAR(1.0f, c1.Dot(c1), eps);
        ASSERT_NEAR(1.0f, c2.Dot(c2), eps);
        ASSERT_NEAR(0.0f, c0.Dot(c1), eps);
        ASSERT_NEAR(0.0f, c0.Dot(c2), eps);
        ASSERT_NEAR(0.0f, c1.Dot(c2), eps);
    }

    // --- Determinant +1 (rotation propre, pas de reflexion/scale) ----------
    // det du bloc 3x3 superieur-gauche, en lignes (M*v) :
    //   r0=(m00,m10,m20)=data[0,4,8] ; r1=data[1,5,9] ; r2=data[2,6,10]
    {
        NkMat4f M = static_cast<NkMat4f>(NkQuatf::RotateY(NkAngle(57.0f)));
        float32 a = M.data[0], b = M.data[4], c = M.data[8];
        float32 d = M.data[1], e = M.data[5], f = M.data[9];
        float32 g = M.data[2], h = M.data[6], i = M.data[10];
        float32 det = a*(e*i - f*h) - b*(d*i - f*g) + c*(d*h - e*g);
        ASSERT_NEAR(1.0f, det, eps);
    }
}

// =============================================================================
// Rotation de vecteur par quaternion : NkQuatT::operator*(NkVec3)
//
// Regression du bug : l'ancienne formule utilisait l'argument `vector` a la
// place de la partie vectorielle (x,y,z) du quaternion + un vector.Cross(vector)
// toujours nul -> resultat faux et non-unitaire (~1.77). Le resultat doit etre
// unitaire si v l'est et egal a static_cast<NkMat4f>(q) * v.
// =============================================================================
TEST_CASE(NKmathmoke, QuaternionRotateVector) {
    const float32 eps = 0.0005f;

    // --- RotateZ(90deg) applique a (1,0,0) -> (0,1,0) ----------------------
    {
        NkQuatf q = NkQuatf::RotateZ(NkAngle(90.0f));
        NkVec3f r = q * NkVec3f(1.0f, 0.0f, 0.0f);
        ASSERT_NEAR(0.0f, r.x, eps);
        ASSERT_NEAR(1.0f, r.y, eps);
        ASSERT_NEAR(0.0f, r.z, eps);
        // Norme preservee (vecteur unitaire en entree -> unitaire en sortie).
        ASSERT_NEAR(1.0f, NkSqrt(r.Dot(r)), eps);
    }

    // --- Egalite q*v == mat(q)*v pour plusieurs axes/angles/vecteurs -------
    {
        const float32 angles[] = { 17.0f, 45.0f, 90.0f, 123.0f, -60.0f };
        NkVec3f vs[] = {
            NkVec3f(1.0f, 0.0f, 0.0f),
            NkVec3f(0.0f, 1.0f, 0.0f),
            NkVec3f(0.0f, 0.0f, 1.0f),
            NkVec3f(0.3f, -0.7f, 0.65f),  // non axe-aligne (norme ~1)
        };
        for (float32 deg : angles) {
            NkAngle a = NkAngle(deg);
            NkQuatf qz = NkQuatf::RotateZ(a);
            NkQuatf qx = NkQuatf::RotateX(a);
            NkQuatf qy = NkQuatf::RotateY(a);
            NkMat4f mz = static_cast<NkMat4f>(qz);
            NkMat4f mx = static_cast<NkMat4f>(qx);
            NkMat4f my = static_cast<NkMat4f>(qy);
            for (const NkVec3f& v : vs) {
                NkVec3f rqz = qz * v, rmz = mz * v;
                NkVec3f rqx = qx * v, rmx = mx * v;
                NkVec3f rqy = qy * v, rmy = my * v;
                ASSERT_NEAR(rmz.x, rqz.x, eps); ASSERT_NEAR(rmz.y, rqz.y, eps); ASSERT_NEAR(rmz.z, rqz.z, eps);
                ASSERT_NEAR(rmx.x, rqx.x, eps); ASSERT_NEAR(rmx.y, rqx.y, eps); ASSERT_NEAR(rmx.z, rqx.z, eps);
                ASSERT_NEAR(rmy.x, rqy.x, eps); ASSERT_NEAR(rmy.y, rqy.y, eps); ASSERT_NEAR(rmy.z, rqy.z, eps);
            }
        }
    }
}

// =============================================================================
// Composition de quaternions : NkQuatT::operator*(NkQuatT)
//
// Regression du bug : le produit de Hamilton avait les termes croises inverses
// -> il calculait other⊗this, donc mat(qA*qB) valait mat(qB)*mat(qA). Apres fix
// la convention est M·v : mat(qA*qB) == mat(qA)*mat(qB) et (qA*qB)*v == qA*(qB*v).
// =============================================================================
TEST_CASE(NKmathmoke, QuaternionComposition) {
    const float32 eps = 0.0006f;

    NkQuatf qA = NkQuatf::RotateZ(NkAngle(90.0f));
    NkQuatf qB = NkQuatf::RotateX(NkAngle(90.0f));

    // --- mat(qA*qB) == mat(qA)*mat(qB) -------------------------------------
    {
        NkMat4f lhs = static_cast<NkMat4f>(qA * qB);
        NkMat4f rhs = static_cast<NkMat4f>(qA) * static_cast<NkMat4f>(qB);
        for (int i = 0; i < 16; ++i) ASSERT_NEAR(rhs.data[i], lhs.data[i], eps);
    }

    // --- (qA*qB)*v == qA*(qB*v) (qB applique d'abord, puis qA) --------------
    {
        NkVec3f vs[] = {
            NkVec3f(1.0f, 0.0f, 0.0f),
            NkVec3f(0.0f, 1.0f, 0.0f),
            NkVec3f(0.0f, 0.0f, 1.0f),
            NkVec3f(0.3f, -0.7f, 0.65f),
        };
        NkQuatf qAB = qA * qB;
        for (const NkVec3f& v : vs) {
            NkVec3f lhs = qAB * v;
            NkVec3f rhs = qA * (qB * v);
            ASSERT_NEAR(rhs.x, lhs.x, eps);
            ASSERT_NEAR(rhs.y, lhs.y, eps);
            ASSERT_NEAR(rhs.z, lhs.z, eps);
        }
    }
}
