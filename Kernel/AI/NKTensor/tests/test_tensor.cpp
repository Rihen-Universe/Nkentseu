// =============================================================================
// test_tensor.cpp — tests NKTensor (Jalon 1/2 CPU).
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKTensor/NkTensor.h"
#include "NKTensor/NkTensorOps.h"

using namespace nkentseu;
using namespace nkentseu::ai;

static bool Near(double a, double b, double eps = 1e-5) {
	double d = a - b;
	if (d < 0)
		d = -d;
	return d <= eps;
}

// ---- Structure / forme ------------------------------------------------------
TEST_CASE(NKTensor, ShapeAndStrides) {
	NkTensor t = NkTensor::Zeros(NkShape{2, 3, 4});
	ASSERT_TRUE(t.IsValid());
	ASSERT_EQUAL(3, static_cast<int>(t.Rank()));
	ASSERT_EQUAL(24, static_cast<int>(t.Numel()));
	ASSERT_TRUE(t.IsContiguous());
	// strides row-major : [12, 4, 1]
	ASSERT_EQUAL(12, static_cast<int>(t.Strides()[0]));
	ASSERT_EQUAL(4, static_cast<int>(t.Strides()[1]));
	ASSERT_EQUAL(1, static_cast<int>(t.Strides()[2]));
}

TEST_CASE(NKTensor, FillAndItem) {
	NkTensor t = NkTensor::Full(NkShape{2, 2}, 3.0);
	ASSERT_TRUE(Near(t.GetItem(NkShape{0, 0}), 3.0));
	ASSERT_TRUE(Near(t.GetItem(NkShape{1, 1}), 3.0));
	t.SetItem(NkShape{0, 1}, 7.5);
	ASSERT_TRUE(Near(t.GetItem(NkShape{0, 1}), 7.5));
}

TEST_CASE(NKTensor, FromDataAndReshape) {
	float data[6] = {1, 2, 3, 4, 5, 6};
	NkTensor t = NkTensor::FromData(NkShape{2, 3}, data, NkDType::NK_F32);
	ASSERT_TRUE(Near(t.GetItem(NkShape{1, 2}), 6.0));

	NkTensor r = t.Reshape(NkShape{3, 2}); // vue contiguë
	ASSERT_EQUAL(2, static_cast<int>(r.Rank()));
	ASSERT_TRUE(Near(r.GetItem(NkShape{0, 0}), 1.0));
	ASSERT_TRUE(Near(r.GetItem(NkShape{2, 1}), 6.0));
}

// ---- Vue transposée (strided) ----------------------------------------------
TEST_CASE(NKTensor, TransposeIsStridedView) {
	float data[6] = {1, 2, 3, 4, 5, 6}; // [[1,2,3],[4,5,6]]
	NkTensor t = NkTensor::FromData(NkShape{2, 3}, data, NkDType::NK_F32);
	NkTensor tt = t.Transpose(0, 1); // 3x2, non contigu
	ASSERT_EQUAL(3, static_cast<int>(tt.Shape()[0]));
	ASSERT_EQUAL(2, static_cast<int>(tt.Shape()[1]));
	ASSERT_FALSE(tt.IsContiguous());
	ASSERT_TRUE(Near(tt.GetItem(NkShape{0, 1}), 4.0)); // = t[1,0]
	ASSERT_TRUE(Near(tt.GetItem(NkShape{2, 0}), 3.0)); // = t[0,2]

	NkTensor c = tt.Contiguous(); // matérialise
	ASSERT_TRUE(c.IsContiguous());
	ASSERT_TRUE(Near(c.GetItem(NkShape{0, 1}), 4.0));
}

// ---- Élémentaires + broadcasting -------------------------------------------
TEST_CASE(NKTensor, ElementwiseAdd) {
	float a[4] = {1, 2, 3, 4};
	float b[4] = {10, 20, 30, 40};
	NkTensor ta = NkTensor::FromData(NkShape{2, 2}, a, NkDType::NK_F32);
	NkTensor tb = NkTensor::FromData(NkShape{2, 2}, b, NkDType::NK_F32);
	NkTensor c = ops::Add(ta, tb);
	ASSERT_TRUE(Near(c.GetItem(NkShape{0, 0}), 11.0));
	ASSERT_TRUE(Near(c.GetItem(NkShape{1, 1}), 44.0));
}

TEST_CASE(NKTensor, BroadcastAdd) {
	float a[6] = {1, 2, 3, 4, 5, 6}; // {2,3}
	float b[3] = {10, 20, 30};		 // {3} -> diffusé sur les lignes
	NkTensor ta = NkTensor::FromData(NkShape{2, 3}, a, NkDType::NK_F32);
	NkTensor tb = NkTensor::FromData(NkShape{3}, b, NkDType::NK_F32);
	NkTensor c = ops::Add(ta, tb);
	ASSERT_EQUAL(2, static_cast<int>(c.Shape()[0]));
	ASSERT_EQUAL(3, static_cast<int>(c.Shape()[1]));
	ASSERT_TRUE(Near(c.GetItem(NkShape{0, 0}), 11.0));
	ASSERT_TRUE(Near(c.GetItem(NkShape{1, 2}), 36.0)); // 6 + 30
}

TEST_CASE(NKTensor, UnaryReluExp) {
	float a[4] = {-1, 0, 2, -3};
	NkTensor t = NkTensor::FromData(NkShape{4}, a, NkDType::NK_F32);
	NkTensor r = ops::Relu(t);
	ASSERT_TRUE(Near(r.GetItem(NkShape{0}), 0.0));
	ASSERT_TRUE(Near(r.GetItem(NkShape{2}), 2.0));
	NkTensor e = ops::Exp(NkTensor::Full(NkShape{1}, 0.0));
	ASSERT_TRUE(Near(e.GetItem(NkShape{0}), 1.0));
}

// ---- Produit de matrices (le jalon « multiplier deux matrices ») -----------
TEST_CASE(NKTensor, Matmul2D) {
	float a[6] = {1, 2, 3, 4, 5, 6};	// 2x3
	float b[6] = {7, 8, 9, 10, 11, 12}; // 3x2
	NkTensor ta = NkTensor::FromData(NkShape{2, 3}, a, NkDType::NK_F32);
	NkTensor tb = NkTensor::FromData(NkShape{3, 2}, b, NkDType::NK_F32);
	NkTensor c = ops::Matmul(ta, tb); // 2x2 = [[58,64],[139,154]]
	ASSERT_EQUAL(2, static_cast<int>(c.Shape()[0]));
	ASSERT_EQUAL(2, static_cast<int>(c.Shape()[1]));
	ASSERT_TRUE(Near(c.GetItem(NkShape{0, 0}), 58.0));
	ASSERT_TRUE(Near(c.GetItem(NkShape{0, 1}), 64.0));
	ASSERT_TRUE(Near(c.GetItem(NkShape{1, 0}), 139.0));
	ASSERT_TRUE(Near(c.GetItem(NkShape{1, 1}), 154.0));
}

// Matmul via une vue transposée (exerce le chemin Contiguous interne).
TEST_CASE(NKTensor, MatmulWithTransposedView) {
	float a[6] = {1, 2, 3, 4, 5, 6}; // 2x3
	NkTensor ta = NkTensor::FromData(NkShape{2, 3}, a, NkDType::NK_F32);
	NkTensor at = ta.Transpose(0, 1);								  // 3x2 (strided)
	NkTensor c = ops::Matmul(at, ta.Transpose(0, 1).Transpose(0, 1)); // 3x2 * 2x3
	ASSERT_EQUAL(3, static_cast<int>(c.Shape()[0]));
	ASSERT_EQUAL(3, static_cast<int>(c.Shape()[1]));
}

// ---- Réductions -------------------------------------------------------------
TEST_CASE(NKTensor, ReductionsGlobal) {
	float a[6] = {1, 2, 3, 4, 5, 6};
	NkTensor t = NkTensor::FromData(NkShape{2, 3}, a, NkDType::NK_F32);
	ASSERT_TRUE(Near(ops::Sum(t).GetItem(NkShape{0}), 21.0));
	ASSERT_TRUE(Near(ops::Mean(t).GetItem(NkShape{0}), 3.5));
	ASSERT_TRUE(Near(ops::Max(t).GetItem(NkShape{0}), 6.0));
}

TEST_CASE(NKTensor, ReductionsAxis) {
	float a[6] = {1, 2, 3, 4, 5, 6}; // {2,3}
	NkTensor t = NkTensor::FromData(NkShape{2, 3}, a, NkDType::NK_F32);
	NkTensor s0 = ops::Sum(t, 0); // -> {3} : [5,7,9]
	ASSERT_EQUAL(1, static_cast<int>(s0.Rank()));
	ASSERT_TRUE(Near(s0.GetItem(NkShape{0}), 5.0));
	ASSERT_TRUE(Near(s0.GetItem(NkShape{2}), 9.0));

	NkTensor s1 = ops::Sum(t, 1); // -> {2} : [6,15]
	ASSERT_TRUE(Near(s1.GetItem(NkShape{0}), 6.0));
	ASSERT_TRUE(Near(s1.GetItem(NkShape{1}), 15.0));

	NkTensor am = ops::Argmax(t, 1); // -> {2} : [2,2]
	ASSERT_TRUE(Near(am.GetItem(NkShape{0}), 2.0));
	ASSERT_TRUE(Near(am.GetItem(NkShape{1}), 2.0));
}
