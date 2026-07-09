// =============================================================================
// NkVar.cpp — différentiation automatique en mode inverse (NKAI, Phase 2).
//
// Chaque opération différentiable fait le FORWARD via les ops:: de NKTensor et
// enregistre un nœud (tag NkAutoOp + parents). Backward() fait un tri
// topologique du graphe puis remonte de la perte (scalaire) vers les feuilles,
// en dispatchant la dérivée sur le tag — pas de std::function (contrainte
// « sans STL »). Les nœuds sont refcomptés, alloués via NKMemory, comme
// NkTensorStorage. Gradients calculés sur CPU en F32.
// =============================================================================
#include "NKAutograd/NkVar.h"
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkTensorGpu.h" // im2col/col2im GPU quand l'entrée est résidente
#include "NKMemory/NkAllocator.h"

#include <cmath>

namespace nkentseu {
	namespace ai {

		static memory::NkIAllocator &Alloc() {
			return memory::NkGetDefaultAllocator();
		}

		// =====================================================================
		// NkVarNode — cycle de vie refcompté
		// =====================================================================
		NkVarNode *NkVarNode::New() {
			NkVarNode *n = Alloc().New<NkVarNode>();
			n->refcount = 1;
			return n;
		}

		void NkVarNode::Retain(NkVarNode *n) {
			if (n)
				++n->refcount;
		}

		void NkVarNode::Release(NkVarNode *n) {
			if (!n)
				return;
			if (--n->refcount == 0) {
				Release(n->a);
				Release(n->b);
				Alloc().Delete(n);
			}
		}

		// =====================================================================
		// NkVar — handle refcompté
		// =====================================================================
		NkVar::~NkVar() {
			NkVarNode::Release(mNode);
		}

		NkVar::NkVar(const NkVar &o) : mNode(o.mNode) {
			NkVarNode::Retain(mNode);
		}

		NkVar &NkVar::operator=(const NkVar &o) {
			if (this != &o) {
				NkVarNode::Retain(o.mNode);
				NkVarNode::Release(mNode);
				mNode = o.mNode;
			}
			return *this;
		}

		NkVar::NkVar(NkVar &&o) noexcept : mNode(o.mNode) {
			o.mNode = nullptr;
		}

		NkVar &NkVar::operator=(NkVar &&o) noexcept {
			if (this != &o) {
				NkVarNode::Release(mNode);
				mNode = o.mNode;
				o.mNode = nullptr;
			}
			return *this;
		}

		NkVar NkVar::Leaf(const NkTensor &value, bool requiresGrad) {
			NkVarNode *n = NkVarNode::New();
			n->value = value;
			n->op = NkAutoOp::NK_LEAF;
			n->requiresGrad = requiresGrad;
			return NkVar(n);
		}

		static const NkTensor kEmptyTensor;

		const NkTensor &NkVar::Value() const {
			return mNode ? mNode->value : kEmptyTensor;
		}

		const NkTensor &NkVar::Grad() const {
			return mNode ? mNode->grad : kEmptyTensor;
		}

		bool NkVar::RequiresGrad() const {
			return mNode && mNode->requiresGrad;
		}

		void NkVar::SetValue(const NkTensor &v) {
			if (mNode)
				mNode->value = v;
		}

		// Construit un nœud d'opération : forward déjà calculé (`value`), parents
		// retenus, requiresGrad propagé si un parent l'exige.
		NkVar NkMakeOp(NkAutoOp op, const NkTensor &value, NkVarNode *a, NkVarNode *b) {
			NkVarNode *n = NkVarNode::New();
			n->value = value;
			n->op = op;
			n->a = a;
			NkVarNode::Retain(a);
			n->b = b;
			NkVarNode::Retain(b);
			n->requiresGrad = (a && a->requiresGrad) || (b && b->requiresGrad);
			return NkVar(n);
		}

		// =====================================================================
		// Helpers gradient
		// =====================================================================

		// Réduit un gradient `g` (forme de sortie, éventuellement broadcastée) à la
		// forme `target` d'un opérande, en sommant sur les axes broadcastés. C'est
		// l'« unbroadcast » standard de l'autodiff (ex. biais [1,H] additionné à [B,H]
		// -> le gradient [B,H] se somme sur l'axe 0 pour redevenir [1,H]).
		static NkTensor SumAxisKeepdim(const NkTensor &a, uint32 axis) {
			NkTensor s = ops::Sum(a, axis); // l'axe disparaît
			NkShape sh = a.Shape();
			sh[axis] = 1; // on le réinsère à 1 (keepdim)
			return s.Reshape(sh);
		}

		// --- Helpers résidence GPU ---------------------------------------------
		// Un noyau CPU-only reçoit une COPIE CPU ; son résultat est ramené sur le
		// device d'un tenseur de référence -> la chaîne reste résidente autour de
		// l'op (l'op elle-même fait un aller-retour, faute de noyau GPU dédié).
		static NkTensor ToCpuT(const NkTensor &t) {
			return (t.IsValid() && t.Device() == NkDevice::NK_GPU) ? t.ToCPU() : t;
		}

		static NkTensor ToDevOf(const NkTensor &r, const NkTensor &ref) {
			return (ref.Device() == NkDevice::NK_GPU && r.IsValid() && r.Device() != NkDevice::NK_GPU) ? r.ToGPU() : r;
		}

		static double ScalarOf(const NkTensor &t) {
			NkTensor c = ToCpuT(t);
			return c.IsValid() ? c.GetItem(NkShape{(int64)0}) : 0.0;
		}

		static NkTensor Unbroadcast(const NkTensor &g, const NkShape &target) {
			NkTensor r = g;
			// 1) réduire les dimensions de tête en trop (rang plus grand que la cible).
			while ((int64)r.Shape().Size() > (int64)target.Size())
				r = ops::Sum(r, 0);
			// 2) sommer (keepdim) les axes où la cible vaut 1 mais pas le gradient.
			for (uint32 i = 0; i < target.Size(); ++i)
				if (target[i] == 1 && r.Shape()[i] != 1)
					r = SumAxisKeepdim(r, i);
			return r.Reshape(target); // restaure exactement la forme cible
		}

		// Accumule `contrib` dans node->grad (avec unbroadcast). Ignoré si le nœud
		// (et donc tout son sous-graphe) ne participe pas au gradient.
		static void AccumGrad(NkVarNode *node, const NkTensor &contrib) {
			if (!node || !node->requiresGrad)
				return;
			NkTensor c = Unbroadcast(contrib, node->value.Shape());
			node->grad = node->grad.IsValid() ? ops::Add(node->grad, c) : c;
		}

		// Softmax sur le DERNIER axe (F32), stable (soustraction du max).
		static NkTensor SoftmaxRows(const NkTensor &logits) {
			if (logits.Device() == NkDevice::NK_GPU)
				return NkGpuSoftmaxRows(logits);	  // natif GPU
			NkTensor x = ToCpuT(logits).Contiguous(); // noyau CPU
			const int64 C = x.Shape()[x.Rank() - 1];  // dernier axe
			const int64 B = (C > 0) ? x.Numel() / C : 0;
			NkTensor out = NkTensor::Zeros(x.Shape());
			const float *xp = x.DataAs<float>();
			float *op = out.DataAs<float>();
			for (int64 b = 0; b < B; ++b) {
				const float *row = xp + b * C;
				float *orow = op + b * C;
				float mx = row[0];
				for (int64 c = 1; c < C; ++c)
					if (row[c] > mx)
						mx = row[c];
				double sum = 0.0;
				for (int64 c = 0; c < C; ++c) {
					double e = std::exp((double)(row[c] - mx));
					orow[c] = (float)e;
					sum += e;
				}
				const double inv = (sum > 0.0) ? 1.0 / sum : 0.0;
				for (int64 c = 0; c < C; ++c)
					orow[c] = (float)((double)orow[c] * inv);
			}
			return ToDevOf(out, logits); // ramène sur le device de l'entrée
		}

		// Softmax backward (CPU) : dx = y ⊙ (dy − Σ_lastaxis(dy⊙y)).
		static NkTensor SoftmaxBackwardCpu(const NkTensor &y, const NkTensor &g) {
			NkTensor yc = y.Contiguous();
			NkTensor gc = g.Contiguous();
			const int64 C = yc.Shape()[yc.Rank() - 1];
			const int64 R = (C > 0) ? yc.Numel() / C : 0;
			NkTensor dx = NkTensor::Zeros(yc.Shape());
			const float *yp = yc.DataAs<float>();
			const float *gp = gc.DataAs<float>();
			float *dp = dx.DataAs<float>();
			for (int64 r = 0; r < R; ++r) {
				const float *yr = yp + r * C;
				const float *gr = gp + r * C;
				float *dr = dp + r * C;
				double s = 0;
				for (int64 c = 0; c < C; ++c)
					s += (double)gr[c] * yr[c];
				for (int64 c = 0; c < C; ++c)
					dr[c] = (float)(yr[c] * (gr[c] - s));
			}
			return dx;
		}

		// Softmax CAUSAL (CPU) : dernier axe [.., T, T], requête = row % T ; masque j > pos.
		static NkTensor SoftmaxCausalCpu(const NkTensor &x) {
			NkTensor xc = x.Contiguous();
			const int64 T = xc.Shape()[xc.Rank() - 1];
			const int64 R = (T > 0) ? xc.Numel() / T : 0;
			NkTensor out = NkTensor::Zeros(xc.Shape());
			const float *xp = xc.DataAs<float>();
			float *op = out.DataAs<float>();
			for (int64 r = 0; r < R; ++r) {
				const float *xr = xp + r * T;
				float *orr = op + r * T;
				int64 pos = r % T;
				float mx = xr[0];
				for (int64 c = 1; c <= pos; ++c)
					if (xr[c] > mx)
						mx = xr[c];
				double sum = 0;
				for (int64 c = 0; c <= pos; ++c) {
					double e = std::exp((double)(xr[c] - mx));
					orr[c] = (float)e;
					sum += e;
				}
				double inv = sum > 0 ? 1.0 / sum : 0;
				for (int64 c = 0; c <= pos; ++c)
					orr[c] = (float)(orr[c] * inv);
				for (int64 c = pos + 1; c < T; ++c)
					orr[c] = 0.f;
			}
			return out;
		}

		// GELU (tanh-approx) CPU + backward.
		static NkTensor GeluCpu(const NkTensor &x) {
			NkTensor xc = x.Contiguous();
			NkTensor out = NkTensor::Zeros(xc.Shape());
			const float *xp = xc.DataAs<float>();
			float *op = out.DataAs<float>();
			const int64 n = xc.Numel();
			const double c = 0.7978845608;
			for (int64 i = 0; i < n; ++i) {
				double v = xp[i];
				double inner = c * (v + 0.044715 * v * v * v);
				op[i] = (float)(0.5 * v * (1.0 + std::tanh(inner)));
			}
			return out;
		}

		static NkTensor GeluBackwardCpu(const NkTensor &x, const NkTensor &g) {
			NkTensor xc = x.Contiguous();
			NkTensor gc = g.Contiguous();
			NkTensor dx = NkTensor::Zeros(xc.Shape());
			const float *xp = xc.DataAs<float>();
			const float *gp = gc.DataAs<float>();
			float *dp = dx.DataAs<float>();
			const int64 n = xc.Numel();
			const double c = 0.7978845608;
			for (int64 i = 0; i < n; ++i) {
				double v = xp[i];
				double v2 = v * v;
				double inner = c * (v + 0.044715 * v2 * v);
				double t = std::tanh(inner);
				double sech2 = 1.0 - t * t;
				double dg = 0.5 * (1.0 + t) + 0.5 * v * sech2 * c * (1.0 + 3.0 * 0.044715 * v2);
				dp[i] = (float)(gp[i] * dg);
			}
			return dx;
		}

		// Embedding CPU (lookup) + backward (scatter-add).
		static NkTensor EmbeddingCpu(const NkTensor &table, const NkTensor &idx) {
			NkTensor tc = table.Contiguous();
			NkTensor ic = idx.Contiguous();
			const int64 vocab = tc.Shape()[0], d = tc.Shape()[1];
			const int64 numPos = ic.Numel();
			NkShape outShape;
			outShape.Resize(ic.Rank() + 1);
			for (uint32 k = 0; k < ic.Rank(); ++k)
				outShape[k] = ic.Shape()[k];
			outShape[ic.Rank()] = d;
			NkTensor out = NkTensor::Zeros(outShape);
			const float *tp = tc.DataAs<float>();
			const float *ip = ic.DataAs<float>();
			float *op = out.DataAs<float>();
			for (int64 pos = 0; pos < numPos; ++pos) {
				int64 tid = (int64)(ip[pos] + 0.5f);
				if (tid < 0 || tid >= vocab)
					tid = 0;
				for (int64 c = 0; c < d; ++c)
					op[pos * d + c] = tp[tid * d + c];
			}
			return out;
		}

		static NkTensor EmbeddingBackwardCpu(const NkTensor &g, const NkTensor &idx, int64 vocab, int64 d) {
			NkTensor gc = g.Contiguous();
			NkTensor ic = idx.Contiguous();
			const int64 numPos = ic.Numel();
			NkTensor dt = NkTensor::Zeros(NkShape{vocab, d});
			const float *gp = gc.DataAs<float>();
			const float *ip = ic.DataAs<float>();
			float *dp = dt.DataAs<float>();
			for (int64 pos = 0; pos < numPos; ++pos) {
				int64 tid = (int64)(ip[pos] + 0.5f);
				if (tid < 0 || tid >= vocab)
					tid = 0;
				for (int64 c = 0; c < d; ++c)
					dp[tid * d + c] += gp[pos * d + c];
			}
			return dt;
		}

		// im2col : déplie les fenêtres réceptives en une matrice [B·outH·outW, Cin·kH·kW]
		// (ligne = position de sortie, colonne = fenêtre aplatie). La conv devient alors
		// un matmul (col · Wᵀ), donc GPU-accéléré pour les grosses convs. Pas de MAC ici,
		// juste du réarrangement mémoire.
		static NkTensor Im2Col(const NkTensor &x, int64 kH, int64 kW, int64 stride, int64 pad, int64 outH, int64 outW) {
			if (x.Device() == NkDevice::NK_GPU) // conv résidente : réarrangement sur GPU
				return NkGpuIm2Col(x, kH, kW, stride, pad, outH, outW);
			const NkShape &xs = x.Shape();
			const int64 B = xs[0], Cin = xs[1], H = xs[2], W = xs[3];
			const int64 K = Cin * kH * kW, M = B * outH * outW;
			NkTensor col = NkTensor::Zeros(NkShape{M, K});
			const float *xp = x.DataAs<float>();
			float *cp = col.DataAs<float>();
			for (int64 b = 0; b < B; ++b)
				for (int64 oh = 0; oh < outH; ++oh)
					for (int64 ow = 0; ow < outW; ++ow) {
						const int64 row = (b * outH + oh) * outW + ow;
						for (int64 ic = 0; ic < Cin; ++ic)
							for (int64 ky = 0; ky < kH; ++ky)
								for (int64 kx = 0; kx < kW; ++kx) {
									const int64 iy = oh * stride - pad + ky, ix = ow * stride - pad + kx;
									const int64 kcol = (ic * kH + ky) * kW + kx;
									cp[row * K + kcol] = (iy >= 0 && iy < H && ix >= 0 && ix < W)
															 ? xp[((b * Cin + ic) * H + iy) * W + ix]
															 : 0.f;
								}
					}
			return col;
		}

		// col2im : transposé de im2col — redistribue (accumule) les colonnes vers [B,Cin,H,W].
		static NkTensor Col2Im(const NkTensor &col, int64 B, int64 Cin, int64 H, int64 W, int64 kH, int64 kW,
							   int64 stride, int64 pad, int64 outH, int64 outW) {
			if (col.Device() == NkDevice::NK_GPU) // conv résidente : redistribution sur GPU
				return NkGpuCol2Im(col, B, Cin, H, W, kH, kW, stride, pad, outH, outW);
			const int64 K = Cin * kH * kW;
			NkTensor dx = NkTensor::Zeros(NkShape{B, Cin, H, W});
			float *dp = dx.DataAs<float>();
			const float *cp = col.DataAs<float>();
			for (int64 b = 0; b < B; ++b)
				for (int64 oh = 0; oh < outH; ++oh)
					for (int64 ow = 0; ow < outW; ++ow) {
						const int64 row = (b * outH + oh) * outW + ow;
						for (int64 ic = 0; ic < Cin; ++ic)
							for (int64 ky = 0; ky < kH; ++ky)
								for (int64 kx = 0; kx < kW; ++kx) {
									const int64 iy = oh * stride - pad + ky, ix = ow * stride - pad + kx;
									if (iy >= 0 && iy < H && ix >= 0 && ix < W) {
										const int64 kcol = (ic * kH + ky) * kW + kx;
										dp[((b * Cin + ic) * H + iy) * W + ix] += cp[row * K + kcol];
									}
								}
					}
			return dx;
		}

		// Masque de la dérivée de ReLU : 1 où x>0, sinon 0 (F32, forme de x).
		static NkTensor ReluMask(const NkTensor &x) {
			if (x.Device() == NkDevice::NK_GPU)
				return NkGpuStep(x); // masque ReLU' sur GPU
			NkTensor xc = x.Contiguous();
			NkTensor m = NkTensor::Zeros(xc.Shape());
			const float *src = xc.DataAs<float>();
			float *dst = m.DataAs<float>();
			int64 n = NkShapeNumel(xc.Shape());
			for (int64 i = 0; i < n; ++i)
				dst[i] = src[i] > 0.f ? 1.f : 0.f;
			return m;
		}

		// LayerNorm sur le dernier axe (CPU) : y=(x−μ)/√(var+ε), ε=1e-5.
		static NkTensor LayerNormStdCpu(const NkTensor &x) {
			NkTensor xc = x.Contiguous();
			const int64 D = xc.Shape()[xc.Rank() - 1];
			const int64 rows = (D > 0) ? xc.Numel() / D : 0;
			NkTensor out = NkTensor::Zeros(xc.Shape());
			const float *xp = xc.DataAs<float>();
			float *op = out.DataAs<float>();
			for (int64 r = 0; r < rows; ++r) {
				const float *xr = xp + r * D;
				float *orr = op + r * D;
				double mean = 0;
				for (int64 c = 0; c < D; ++c)
					mean += xr[c];
				mean /= (double)D;
				double var = 0;
				for (int64 c = 0; c < D; ++c) {
					double t = xr[c] - mean;
					var += t * t;
				}
				var /= (double)D;
				double invstd = 1.0 / std::sqrt(var + 1e-5);
				for (int64 c = 0; c < D; ++c)
					orr[c] = (float)((xr[c] - mean) * invstd);
			}
			return out;
		}

		static NkTensor LayerNormStdBackwardCpu(const NkTensor &x, const NkTensor &g) {
			NkTensor xc = x.Contiguous();
			NkTensor gc = g.Contiguous();
			const int64 D = xc.Shape()[xc.Rank() - 1];
			const int64 rows = (D > 0) ? xc.Numel() / D : 0;
			NkTensor dx = NkTensor::Zeros(xc.Shape());
			const float *xp = xc.DataAs<float>();
			const float *gp = gc.DataAs<float>();
			float *dp = dx.DataAs<float>();
			for (int64 r = 0; r < rows; ++r) {
				const float *xr = xp + r * D;
				const float *gr = gp + r * D;
				float *dr = dp + r * D;
				double mean = 0;
				for (int64 c = 0; c < D; ++c)
					mean += xr[c];
				mean /= (double)D;
				double var = 0;
				for (int64 c = 0; c < D; ++c) {
					double t = xr[c] - mean;
					var += t * t;
				}
				var /= (double)D;
				double invstd = 1.0 / std::sqrt(var + 1e-5);
				double m1 = 0, m2 = 0;
				for (int64 c = 0; c < D; ++c) {
					double xh = (xr[c] - mean) * invstd;
					m1 += gr[c];
					m2 += gr[c] * xh;
				}
				m1 /= (double)D;
				m2 /= (double)D;
				for (int64 c = 0; c < D; ++c) {
					double xh = (xr[c] - mean) * invstd;
					dr[c] = (float)(invstd * (gr[c] - m1 - xh * m2));
				}
			}
			return dx;
		}

		// =====================================================================
		// Tri topologique + backward
		// =====================================================================
		static bool Contains(const NkVector<NkVarNode *> &v, NkVarNode *n) {
			for (uint32 i = 0; i < v.Size(); ++i)
				if (v[i] == n)
					return true;
			return false;
		}

		// DFS post-ordre : les parents (entrées) apparaissent AVANT le nœud produit.
		// `order` finit donc par la racine ; le backward l'itère à l'envers.
		static void CollectTopo(NkVarNode *n, NkVector<NkVarNode *> &order, NkVector<NkVarNode *> &seen) {
			if (!n || Contains(seen, n))
				return;
			seen.PushBack(n);
			CollectTopo(n->a, order, seen);
			CollectTopo(n->b, order, seen);
			order.PushBack(n);
		}

		// Rétropropage le gradient d'UN nœud vers ses parents (règle de la chaîne).
		static void BackwardNode(NkVarNode *n) {
			const NkTensor &g = n->grad; // gradient de sortie (déjà accumulé)
			if (!g.IsValid())
				return;
			switch (n->op) {
				case NkAutoOp::NK_LEAF:
					break;

				case NkAutoOp::NK_ADD: // c = a + b
					AccumGrad(n->a, g);
					AccumGrad(n->b, g);
					break;

				case NkAutoOp::NK_SUB: // c = a - b
					AccumGrad(n->a, g);
					AccumGrad(n->b, ops::Neg(g));
					break;

				case NkAutoOp::NK_MUL: // c = a ⊙ b
					if (n->a)
						AccumGrad(n->a, ops::Mul(g, n->b->value));
					if (n->b)
						AccumGrad(n->b, ops::Mul(g, n->a->value));
					break;

				case NkAutoOp::NK_MATMUL: { // c = a[..,M,K] · b[..,K,N] (2D ou par lots)
					// dA = dC · Bᵀ ; dB = Aᵀ · dC  (transpose des 2 DERNIERS axes).
					if (n->a) {
						const uint32 rb = n->b->value.Rank();
						NkTensor bt = n->b->value.Transpose(rb - 2, rb - 1).Contiguous();
						AccumGrad(n->a, ops::Matmul(g, bt));
					}
					if (n->b) {
						const uint32 ra = n->a->value.Rank();
						NkTensor at = n->a->value.Transpose(ra - 2, ra - 1).Contiguous();
						AccumGrad(n->b, ops::Matmul(at, g));
					}
					break;
				}

				case NkAutoOp::NK_RELU: // relu'(x) = (x>0)
					AccumGrad(n->a, ops::Mul(g, ReluMask(n->a->value)));
					break;

				case NkAutoOp::NK_SIGMOID: { // σ'(x) = y(1-y) = y - y²  (y = sortie)
					const NkTensor &y = n->value;
					NkTensor d = ops::Sub(y, ops::Mul(y, y));
					AccumGrad(n->a, ops::Mul(g, d));
					break;
				}
				case NkAutoOp::NK_TANH: { // tanh'(x) = 1 - y²  (y = sortie)
					const NkTensor &y = n->value;
					NkTensor d = ops::AddScalar(ops::MulScalar(ops::Mul(y, y), -1.0), 1.0);
					AccumGrad(n->a, ops::Mul(g, d));
					break;
				}

				case NkAutoOp::NK_SUM: { // c = Σ a  -> chaque a_i reçoit dC
					// Lit le scalaire côté CPU (g peut résider sur GPU) et recrée le
					// gradient plein sur le MÊME device que l'entrée -> backward résident.
					NkTensor gc = (g.Device() == NkDevice::NK_GPU) ? g.ToCPU() : g;
					double s = gc.GetItem(NkShape{(int64)0});
					NkTensor full = NkTensor::Full(n->a->value.Shape(), s);
					if (n->a->value.Device() == NkDevice::NK_GPU)
						full = full.ToGPU();
					AccumGrad(n->a, full);
					break;
				}

				case NkAutoOp::NK_MSE: {	// c = mean((p-t)²) ; dP = 2(p-t)/N · dC
					double s = ScalarOf(g); // g peut résider sur GPU
					int64 N = NkShapeNumel(n->a->value.Shape());
					NkTensor diff = ops::Sub(n->a->value, n->b->value);
					double coef = (N > 0) ? (2.0 / (double)N) * s : 0.0;
					AccumGrad(n->a, ops::MulScalar(diff, coef));
					AccumGrad(n->b, ops::MulScalar(diff, -coef));
					break;
				}

				case NkAutoOp::NK_SOFTMAX_CE: { // dLogits = (softmax(logits) − onehot)/activeRows ; lignes masquées ->
												// 0
					double s = ScalarOf(g); // g peut résider sur GPU
					NkTensor probs = SoftmaxRows(n->a->value);
					const NkShape &sh = probs.Shape();
					const int64 B = sh.Size() >= 1 ? sh[0] : 1;
					const int64 activeRows = n->iparam[0] > 0 ? (int64)n->iparam[0] : B;
					const double coef = (activeRows > 0) ? s / (double)activeRows : 0.0;
					if (n->iparam[1] == 0) {
						// Aucune ligne masquée -> chemin rapide GPU-résident (inchangé).
						AccumGrad(n->a, ops::MulScalar(ops::Sub(probs, n->b->value), coef));
					} else {
						// Lignes masquées présentes -> annule leur gradient (passage CPU ciblé).
						const int64 C = sh.Size() >= 2 ? sh[1] : NkShapeNumel(sh);
						NkTensor tc = ToCpuT(n->b->value).Contiguous();
						NkTensor diff = ToCpuT(ops::Sub(probs, n->b->value)).Contiguous();
						const float *tp = tc.DataAs<float>();
						float *dp = diff.DataAs<float>();
						for (int64 b = 0; b < B; ++b) {
							double rowSum = 0.0;
							for (int64 c = 0; c < C; ++c)
								rowSum += (double)tp[b * C + c];
							if (rowSum == 0.0)
								for (int64 c = 0; c < C; ++c)
									dp[b * C + c] = 0.f;
						}
						AccumGrad(n->a, ops::MulScalar(ToDevOf(diff, n->a->value), coef));
					}
					break;
				}

				case NkAutoOp::NK_SOFTMAX_CE_IDX: { // cible par INDICES : dLogits = (probs − onehot(idx))/active
					double s = ScalarOf(g);
					NkTensor probs = SoftmaxRows(n->a->value);
					const NkShape &sh = probs.Shape();
					const int64 B = sh.Size() >= 1 ? sh[0] : 1;
					const int64 C = sh.Size() >= 2 ? sh[1] : NkShapeNumel(sh);
					const int64 activeRows = n->iparam[0] > 0 ? (int64)n->iparam[0] : B;
					const double coef = (activeRows > 0) ? s / (double)activeRows : 0.0;
					// probs -> CPU, soustrait 1 à la classe cible (scatter), annule les lignes masquées.
					NkTensor diff = ToCpuT(probs).Contiguous();
					NkTensor ti = ToCpuT(n->b->value).Contiguous();
					float *dp = diff.DataAs<float>();
					const float *tp = ti.DataAs<float>();
					for (int64 b = 0; b < B; ++b) {
						const int64 idx = (int64)((double)tp[b] + 0.5);
						if (tp[b] < 0.f || idx < 0 || idx >= C) {
							for (int64 c = 0; c < C; ++c)
								dp[b * C + c] = 0.f; // ligne masquée -> gradient nul
						} else {
							dp[b * C + idx] -= 1.0f; // probs - onehot(idx)
						}
					}
					AccumGrad(n->a, ops::MulScalar(ToDevOf(diff, n->a->value), coef));
					break;
				}

				case NkAutoOp::NK_CONV2D: { // backward via im2col + matmul (GPU-accéléré)
					const int32 stride = n->iparam[0], pad = n->iparam[1];
					NkTensor x = n->a->value.Contiguous();
					NkTensor w = n->b->value.Contiguous();
					NkTensor go = g.Contiguous(); // [B,Cout,oH,oW]
					const NkShape &xs = x.Shape();
					const NkShape &ws = w.Shape();
					const int64 B = xs[0], Cin = xs[1], H = xs[2], W = xs[3];
					const int64 Cout = ws[0], kH = ws[2], kW = ws[3];
					const int64 outH = go.Shape()[2], outW = go.Shape()[3];
					const int64 K = Cin * kH * kW, M = B * outH * outW;

					NkTensor col = Im2Col(x, kH, kW, stride, pad, outH, outW);								// [M,K]
					NkTensor go2d = go.Permute(NkShape{0, 2, 3, 1}).Contiguous().Reshape(NkShape{M, Cout}); // [M,Cout]
					NkTensor W2d = w.Reshape(NkShape{Cout, K});												// [Cout,K]

					// dInput : dCol = go2d · W2d, puis col2im.
					NkTensor dCol = ops::Matmul(go2d, W2d); // [M,K]
					NkTensor dX = Col2Im(dCol, B, Cin, H, W, kH, kW, stride, pad, outH, outW);
					// dWeight : dW2d = go2dᵀ · col.
					NkTensor dW2d = ops::Matmul(go2d.Transpose(0, 1).Contiguous(), col); // [Cout,K]
					NkTensor dW = dW2d.Reshape(NkShape{Cout, Cin, kH, kW});

					AccumGrad(n->a, dX);
					AccumGrad(n->b, dW);
					break;
				}

				case NkAutoOp::NK_MAXPOOL2D: { // grad routé vers l'argmax mémorisé
					if (g.Device() == NkDevice::NK_GPU || n->aux.Device() == NkDevice::NK_GPU) {
						const NkShape &xsg = n->a->value.Shape();
						NkTensor dXg = NkGpuMaxPool2DBackward(g, n->aux, xsg[0], xsg[1], xsg[2], xsg[3], g.Shape()[2],
															  g.Shape()[3], n->iparam[0], n->iparam[1]);
						AccumGrad(n->a, dXg);
						break;
					}
					NkTensor go = ToCpuT(g).Contiguous();
					NkTensor auxC = ToCpuT(n->aux);
					const NkShape &xs = n->a->value.Shape();
					const int64 B = xs[0], C = xs[1], H = xs[2], W = xs[3];
					const int64 outH = go.Shape()[2], outW = go.Shape()[3];
					const float *gp = go.DataAs<float>();
					const float *ap = auxC.DataAs<float>();
					NkTensor dX = NkTensor::Zeros(xs);
					float *dxp = dX.DataAs<float>();
					for (int64 b = 0; b < B; ++b)
						for (int64 c = 0; c < C; ++c)
							for (int64 oy = 0; oy < outH; ++oy)
								for (int64 ox = 0; ox < outW; ++ox) {
									const int64 oidx = ((b * C + c) * outH + oy) * outW + ox;
									const int64 hw = (int64)(ap[oidx] + 0.5f);
									dxp[(b * C + c) * H * W + hw] += gp[oidx];
								}
					AccumGrad(n->a, ToDevOf(dX, n->a->value));
					break;
				}

				case NkAutoOp::NK_RESHAPE: { // grad remodelé à la forme d'entrée
					NkTensor gi = g.Contiguous().Reshape(n->a->value.Shape());
					AccumGrad(n->a, gi);
					break;
				}

				case NkAutoOp::NK_CONVT2D: { // conv transposée : miroir du forward (scatter)
					const int32 stride = n->iparam[0], pad = n->iparam[1];
					if (g.Device() == NkDevice::NK_GPU) { // résident GPU
						const NkShape &xs4 = n->a->value.Shape();
						const NkShape &ws4 = n->b->value.Shape();
						const int64 B = xs4[0], Cin = xs4[1], H = xs4[2], W = xs4[3];
						const int64 Cout = ws4[1], kH = ws4[2], kW = ws4[3];
						const int64 oH = g.Shape()[2], oW = g.Shape()[3];
						AccumGrad(n->a, NkGpuConvTranspose2DBackwardX(g, n->b->value, B, Cin, H, W, Cout, kH, kW,
																	  stride, pad, oH, oW));
						AccumGrad(n->b, NkGpuConvTranspose2DBackwardW(n->a->value, g, B, Cin, H, W, Cout, kH, kW,
																	  stride, pad, oH, oW));
						break;
					}
					NkTensor x = ToCpuT(n->a->value).Contiguous(); // [B,Cin,H,W]
					NkTensor w = ToCpuT(n->b->value).Contiguous(); // [Cin,Cout,kH,kW]
					NkTensor go = ToCpuT(g).Contiguous();		   // [B,Cout,outH,outW]
					const NkShape &xs = x.Shape();
					const NkShape &ws = w.Shape();
					const int64 B = xs[0], Cin = xs[1], H = xs[2], W = xs[3];
					const int64 Cout = ws[1], kH = ws[2], kW = ws[3];
					const int64 outH = go.Shape()[2], outW = go.Shape()[3];
					const float *xp = x.DataAs<float>();
					const float *wp = w.DataAs<float>();
					const float *gp = go.DataAs<float>();
					NkTensor dX = NkTensor::Zeros(xs);
					NkTensor dW = NkTensor::Zeros(ws);
					float *dxp = dX.DataAs<float>();
					float *dwp = dW.DataAs<float>();
					for (int64 b = 0; b < B; ++b)
						for (int64 ic = 0; ic < Cin; ++ic)
							for (int64 iy = 0; iy < H; ++iy)
								for (int64 ix = 0; ix < W; ++ix) {
									const int64 xi = ((b * Cin + ic) * H + iy) * W + ix;
									for (int64 oc = 0; oc < Cout; ++oc)
										for (int64 ky = 0; ky < kH; ++ky)
											for (int64 kx = 0; kx < kW; ++kx) {
												const int64 oy = iy * stride - pad + ky, ox = ix * stride - pad + kx;
												if (oy < 0 || oy >= outH || ox < 0 || ox >= outW)
													continue;
												const int64 wi = ((ic * Cout + oc) * kH + ky) * kW + kx;
												const int64 gi = ((b * Cout + oc) * outH + oy) * outW + ox;
												dxp[xi] += gp[gi] * wp[wi];
												dwp[wi] += xp[xi] * gp[gi];
											}
								}
					AccumGrad(n->a, ToDevOf(dX, n->a->value));
					AccumGrad(n->b, ToDevOf(dW, n->b->value));
					break;
				}

				case NkAutoOp::NK_EXP: // d/dx exp(x) = exp(x) = value
					AccumGrad(n->a, ops::Mul(g, n->value));
					break;
				case NkAutoOp::NK_MULSCALAR: // d/dx (x·s) = s
					AccumGrad(n->a, ops::MulScalar(g, n->fparam));
					break;
				case NkAutoOp::NK_ADDSCALAR: // d/dx (x+s) = 1
					AccumGrad(n->a, g);
					break;

				case NkAutoOp::NK_CONV3D: { // conv 3D : dX/dW explicites
					const int32 stride = n->iparam[0], pad = n->iparam[1];
					if (g.Device() == NkDevice::NK_GPU) { // résident GPU
						AccumGrad(n->a, NkGpuConv3DBackwardX(g, n->b->value, n->a->value, stride, pad));
						AccumGrad(n->b, NkGpuConv3DBackwardW(g, n->a->value, n->b->value, stride, pad));
						break;
					}
					NkTensor x = ToCpuT(n->a->value).Contiguous();
					NkTensor w = ToCpuT(n->b->value).Contiguous();
					NkTensor go = ToCpuT(g).Contiguous();
					const NkShape &xs = x.Shape();
					const NkShape &ws = w.Shape();
					const int64 B = xs[0], Cin = xs[1], Dd = xs[2], H = xs[3], W = xs[4];
					const int64 Cout = ws[0], kD = ws[2], kH = ws[3], kW = ws[4];
					const int64 oD = go.Shape()[2], oH = go.Shape()[3], oW = go.Shape()[4];
					const float *xp = x.DataAs<float>();
					const float *wp = w.DataAs<float>();
					const float *gp = go.DataAs<float>();
					NkTensor dX = NkTensor::Zeros(xs);
					NkTensor dW = NkTensor::Zeros(ws);
					float *dxp = dX.DataAs<float>();
					float *dwp = dW.DataAs<float>();
					for (int64 b = 0; b < B; ++b)
						for (int64 oc = 0; oc < Cout; ++oc)
							for (int64 od = 0; od < oD; ++od)
								for (int64 oy = 0; oy < oH; ++oy)
									for (int64 ox = 0; ox < oW; ++ox) {
										const float gg = gp[((((b * Cout + oc) * oD + od) * oH + oy) * oW + ox)];
										for (int64 ic = 0; ic < Cin; ++ic)
											for (int64 kz = 0; kz < kD; ++kz)
												for (int64 ky = 0; ky < kH; ++ky)
													for (int64 kx = 0; kx < kW; ++kx) {
														const int64 iz = od * stride - pad + kz,
																	iy = oy * stride - pad + ky,
																	ix = ox * stride - pad + kx;
														if (iz < 0 || iz >= Dd || iy < 0 || iy >= H || ix < 0 ||
															ix >= W)
															continue;
														const int64 xi =
															((((b * Cin + ic) * Dd + iz) * H + iy) * W + ix);
														const int64 wi =
															((((oc * Cin + ic) * kD + kz) * kH + ky) * kW + kx);
														dxp[xi] += gg * wp[wi];
														dwp[wi] += gg * xp[xi];
													}
									}
					AccumGrad(n->a, ToDevOf(dX, n->a->value));
					AccumGrad(n->b, ToDevOf(dW, n->b->value));
					break;
				}

				case NkAutoOp::NK_CONVT3D: { // conv transposée 3D : miroir du forward
					const int32 stride = n->iparam[0], pad = n->iparam[1];
					if (g.Device() == NkDevice::NK_GPU) { // résident GPU
						AccumGrad(n->a, NkGpuConvTranspose3DBackwardX(g, n->b->value, n->a->value, stride, pad));
						AccumGrad(n->b, NkGpuConvTranspose3DBackwardW(g, n->a->value, n->b->value, stride, pad));
						break;
					}
					NkTensor x = ToCpuT(n->a->value).Contiguous();
					NkTensor w = ToCpuT(n->b->value).Contiguous();
					NkTensor go = ToCpuT(g).Contiguous();
					const NkShape &xs = x.Shape();
					const NkShape &ws = w.Shape();
					const int64 B = xs[0], Cin = xs[1], Dd = xs[2], H = xs[3], W = xs[4];
					const int64 Cout = ws[1], kD = ws[2], kH = ws[3], kW = ws[4];
					const int64 oD = go.Shape()[2], oH = go.Shape()[3], oW = go.Shape()[4];
					const float *xp = x.DataAs<float>();
					const float *wp = w.DataAs<float>();
					const float *gp = go.DataAs<float>();
					NkTensor dX = NkTensor::Zeros(xs);
					NkTensor dW = NkTensor::Zeros(ws);
					float *dxp = dX.DataAs<float>();
					float *dwp = dW.DataAs<float>();
					for (int64 b = 0; b < B; ++b)
						for (int64 ic = 0; ic < Cin; ++ic)
							for (int64 iz = 0; iz < Dd; ++iz)
								for (int64 iy = 0; iy < H; ++iy)
									for (int64 ix = 0; ix < W; ++ix) {
										const int64 xi = ((((b * Cin + ic) * Dd + iz) * H + iy) * W + ix);
										for (int64 oc = 0; oc < Cout; ++oc)
											for (int64 kz = 0; kz < kD; ++kz)
												for (int64 ky = 0; ky < kH; ++ky)
													for (int64 kx = 0; kx < kW; ++kx) {
														const int64 od = iz * stride - pad + kz,
																	oy = iy * stride - pad + ky,
																	ox = ix * stride - pad + kx;
														if (od < 0 || od >= oD || oy < 0 || oy >= oH || ox < 0 ||
															ox >= oW)
															continue;
														const int64 wi =
															((((ic * Cout + oc) * kD + kz) * kH + ky) * kW + kx);
														const int64 gi =
															((((b * Cout + oc) * oD + od) * oH + oy) * oW + ox);
														dxp[xi] += gp[gi] * wp[wi];
														dwp[wi] += xp[xi] * gp[gi];
													}
									}
					AccumGrad(n->a, ToDevOf(dX, n->a->value));
					AccumGrad(n->b, ToDevOf(dW, n->b->value));
					break;
				}

				case NkAutoOp::NK_SIGMOID_BCE: { // dLogits = (sigmoid(logits) − cible)/N · dC
					double s = ScalarOf(g);		 // g peut résider sur GPU
					int64 N = NkShapeNumel(n->a->value.Shape());
					double coef = (N > 0) ? s / (double)N : 0.0;
					NkTensor sig = ops::Sigmoid(n->a->value);
					AccumGrad(n->a, ops::MulScalar(ops::Sub(sig, n->b->value), coef));
					break;
				}

				case NkAutoOp::NK_PERMUTE: { // backward = permutation inverse
					const uint32 r = n->a->value.Rank();
					NkShape inv;
					inv.Resize(r);
					for (uint32 i = 0; i < r; ++i)
						inv[(uint32)n->iparam[i]] = i;
					AccumGrad(n->a, g.Permute(inv).Contiguous());
					break;
				}

				case NkAutoOp::NK_GELU: { // dx = g · GELU'(x)
					NkTensor dx = (g.Device() == NkDevice::NK_GPU || n->a->value.Device() == NkDevice::NK_GPU)
									  ? NkGpuGeluBackward(n->a->value, g)
									  : GeluBackwardCpu(n->a->value, g);
					AccumGrad(n->a, dx);
					break;
				}

				case NkAutoOp::NK_EMBEDDING: { // backward = scatter-add du grad vers la table
					const int64 vocab = n->a->value.Shape()[0], d = n->a->value.Shape()[1];
					NkTensor dt = (g.Device() == NkDevice::NK_GPU || n->a->value.Device() == NkDevice::NK_GPU)
									  ? NkGpuEmbeddingBackward(g, n->aux, vocab, d)
									  : EmbeddingBackwardCpu(g, n->aux, vocab, d);
					AccumGrad(n->a, dt);
					break;
				}

				case NkAutoOp::NK_SOFTMAX:
				case NkAutoOp::NK_SOFTMAX_CAUSAL: { // dx = y ⊙ (dy − Σ dy⊙y) ; y = sortie stockée
					NkTensor dx = (g.Device() == NkDevice::NK_GPU || n->value.Device() == NkDevice::NK_GPU)
									  ? NkGpuSoftmaxBackward(n->value, g)
									  : SoftmaxBackwardCpu(n->value, g);
					AccumGrad(n->a, dx);
					break;
				}

				case NkAutoOp::NK_LAYERNORM: { // dx = invstd·(dy − mean(dy) − x̂·mean(dy·x̂))
					NkTensor dx = (g.Device() == NkDevice::NK_GPU || n->a->value.Device() == NkDevice::NK_GPU)
									  ? NkGpuLayerNormStdBackward(n->a->value, g)
									  : LayerNormStdBackwardCpu(n->a->value, g);
					AccumGrad(n->a, dx);
					break;
				}

				case NkAutoOp::NK_UPSAMPLE2X: { // dIn[y,x] = Σ_{dy,dx} dOut[2y+dy, 2x+dx]
					if (g.Device() == NkDevice::NK_GPU) {
						const NkShape &xsu = n->a->value.Shape();
						AccumGrad(n->a, NkGpuUpsample2xBackward(g, xsu[0], xsu[1], xsu[2], xsu[3]));
						break;
					}
					NkTensor go = ToCpuT(g).Contiguous();
					const NkShape &xs = n->a->value.Shape();
					const int64 B = xs[0], C = xs[1], H = xs[2], W = xs[3], oH = 2 * H, oW = 2 * W;
					const float *gp = go.DataAs<float>();
					NkTensor dX = NkTensor::Zeros(xs);
					float *dp = dX.DataAs<float>();
					for (int64 b = 0; b < B; ++b)
						for (int64 c = 0; c < C; ++c)
							for (int64 y = 0; y < H; ++y)
								for (int64 x = 0; x < W; ++x) {
									double s = 0;
									for (int64 dy = 0; dy < 2; ++dy)
										for (int64 dx = 0; dx < 2; ++dx)
											s += gp[(((b * C + c) * oH + (2 * y + dy)) * oW + (2 * x + dx))];
									dp[(((b * C + c) * H + y) * W + x)] = (float)s;
								}
					AccumGrad(n->a, ToDevOf(dX, n->a->value));
					break;
				}
			}
		}

		void NkVar::Backward() {
			if (!mNode)
				return;
			NkVector<NkVarNode *> order, seen;
			CollectTopo(mNode, order, seen);

			// Réinitialise les accumulateurs (forme de la valeur), puis amorce la
			// racine (perte scalaire) à 1.
			for (uint32 i = 0; i < order.Size(); ++i)
				order[i]->grad = ToDevOf(NkTensor::Zeros(order[i]->value.Shape()), order[i]->value);
			mNode->grad = ToDevOf(NkTensor::Ones(mNode->value.Shape()), mNode->value);

			// Remonte de la racine vers les feuilles (ordre post-fixe inversé).
			for (int64 i = (int64)order.Size() - 1; i >= 0; --i)
				BackwardNode(order[(uint32)i]);
		}

		void NkVar::ZeroGrad() {
			if (!mNode)
				return;
			NkVector<NkVarNode *> order, seen;
			CollectTopo(mNode, order, seen);
			for (uint32 i = 0; i < order.Size(); ++i)
				order[i]->grad = NkTensor::Zeros(order[i]->value.Shape());
		}

		// =====================================================================
		// Opérations différentiables (forward + enregistrement)
		// =====================================================================
		namespace autograd {

			NkVar Add(const NkVar &a, const NkVar &b) {
				return NkMakeOp(NkAutoOp::NK_ADD, ops::Add(a.Value(), b.Value()), a.Node(), b.Node());
			}

			NkVar Sub(const NkVar &a, const NkVar &b) {
				return NkMakeOp(NkAutoOp::NK_SUB, ops::Sub(a.Value(), b.Value()), a.Node(), b.Node());
			}

			NkVar Mul(const NkVar &a, const NkVar &b) {
				return NkMakeOp(NkAutoOp::NK_MUL, ops::Mul(a.Value(), b.Value()), a.Node(), b.Node());
			}

			NkVar Matmul(const NkVar &a, const NkVar &b) {
				return NkMakeOp(NkAutoOp::NK_MATMUL, ops::Matmul(a.Value(), b.Value()), a.Node(), b.Node());
			}

			NkVar Relu(const NkVar &a) {
				return NkMakeOp(NkAutoOp::NK_RELU, ops::Relu(a.Value()), a.Node(), nullptr);
			}

			NkVar Sigmoid(const NkVar &a) {
				return NkMakeOp(NkAutoOp::NK_SIGMOID, ops::Sigmoid(a.Value()), a.Node(), nullptr);
			}

			NkVar Tanh(const NkVar &a) {
				return NkMakeOp(NkAutoOp::NK_TANH, ops::Tanh(a.Value()), a.Node(), nullptr);
			}

			NkVar Sum(const NkVar &a) {
				return NkMakeOp(NkAutoOp::NK_SUM, ops::Sum(a.Value()), a.Node(), nullptr);
			}

			NkVar MSE(const NkVar &pred, const NkVar &target) {
				NkTensor diff = ops::Sub(pred.Value(), target.Value());
				NkTensor loss = ops::Mean(ops::Mul(diff, diff)); // scalaire {1}
				return NkMakeOp(NkAutoOp::NK_MSE, loss, pred.Node(), target.Node());
			}

			NkVar SoftmaxCrossEntropy(const NkVar &logits, const NkVar &targetOneHot) {
				// Forward stable : probs = softmax(logits) par ligne ; perte moyenne sur les
				// lignes ACTIVES (cible non tout-zéro). Une ligne dont la cible est entièrement
				// nulle est MASQUÉE : ignorée en loss ET en gradient (instruction-tuning : on
				// n'apprend pas à prédire la question, seulement la réponse).
				//   L = −(1/activeRows) Σ_{b actif} Σ_c onehot[b,c]·log(probs[b,c]).
				NkTensor probs = SoftmaxRows(ToCpuT(logits.Value())); // CPU-only
				NkTensor tc = ToCpuT(targetOneHot.Value()).Contiguous();
				const NkShape &sh = probs.Shape();
				const int64 B = sh.Size() >= 1 ? sh[0] : 1;
				const int64 C = sh.Size() >= 2 ? sh[1] : NkShapeNumel(sh);
				const float *pp = probs.DataAs<float>();
				const float *tp = tc.DataAs<float>();
				double loss = 0.0;
				int64 activeRows = 0;
				for (int64 b = 0; b < B; ++b) {
					double rowSum = 0.0;
					for (int64 c = 0; c < C; ++c)
						rowSum += (double)tp[b * C + c];
					if (rowSum == 0.0)
						continue; // ligne masquée -> ignorée
					++activeRows;
					for (int64 c = 0; c < C; ++c) {
						float t = tp[b * C + c];
						if (t != 0.0f)
							loss += -(double)t * std::log((double)pp[b * C + c] + 1e-12);
					}
				}
				loss = (activeRows > 0) ? loss / (double)activeRows : 0.0;
				NkTensor lossT = NkTensor::Full(NkShape{(int64)1}, loss);
				NkVar r = NkMakeOp(NkAutoOp::NK_SOFTMAX_CE, lossT, logits.Node(), targetOneHot.Node());
				r.Node()->iparam[0] = (int32)activeRows;		 // lu par le backward
				r.Node()->iparam[1] = (activeRows == B) ? 0 : 1; // 1 = présence de lignes masquées
				return r;
			}

			NkVar SoftmaxCrossEntropyIndexed(const NkVar &logits, const NkVar &targetIdx) {
				// Identique à SoftmaxCrossEntropy mais la cible est un vecteur d'INDICES [B] (un id de classe
				// par ligne). idx < 0 => ligne MASQUÉE (ignorée en loss ET en gradient). Évite le one-hot [B,V].
				//   L = −(1/activeRows) Σ_{b actif} log(probs[b, idx[b]]).
				NkTensor probs = SoftmaxRows(ToCpuT(logits.Value())); // CPU-only (comme la version one-hot)
				NkTensor ti = ToCpuT(targetIdx.Value()).Contiguous();
				const NkShape &sh = probs.Shape();
				const int64 B = sh.Size() >= 1 ? sh[0] : 1;
				const int64 C = sh.Size() >= 2 ? sh[1] : NkShapeNumel(sh);
				const float *pp = probs.DataAs<float>();
				const float *tp = ti.DataAs<float>();
				double loss = 0.0;
				int64 activeRows = 0;
				for (int64 b = 0; b < B; ++b) {
					const int64 idx = (int64)((double)tp[b] + 0.5);
					if (tp[b] < 0.f || idx < 0 || idx >= C)
						continue; // ligne masquée / invalide -> ignorée
					++activeRows;
					loss += -std::log((double)pp[b * C + idx] + 1e-12);
				}
				loss = (activeRows > 0) ? loss / (double)activeRows : 0.0;
				NkTensor lossT = NkTensor::Full(NkShape{(int64)1}, loss);
				NkVar r = NkMakeOp(NkAutoOp::NK_SOFTMAX_CE_IDX, lossT, logits.Node(), targetIdx.Node());
				r.Node()->iparam[0] = (int32)activeRows;
				r.Node()->iparam[1] = (activeRows == B) ? 0 : 1;
				return r;
			}

			NkVar SigmoidBCE(const NkVar &logits, const NkVar &target) {
				// BCE-with-logits stable, moyennée :
				//   L = mean( max(x,0) − x·t + log(1 + exp(−|x|)) )
				NkTensor x = ToCpuT(logits.Value()).Contiguous(); // CPU-only
				NkTensor t = ToCpuT(target.Value()).Contiguous();
				const int64 N = NkShapeNumel(x.Shape());
				const float *xp = x.DataAs<float>();
				const float *tp = t.DataAs<float>();
				double loss = 0.0;
				for (int64 i = 0; i < N; ++i) {
					double xi = (double)xp[i], ti = (double)tp[i];
					loss += (xi > 0.0 ? xi : 0.0) - xi * ti + std::log(1.0 + std::exp(-std::fabs(xi)));
				}
				loss = (N > 0) ? loss / (double)N : loss;
				NkTensor lossT = NkTensor::Full(NkShape{(int64)1}, loss);
				return NkMakeOp(NkAutoOp::NK_SIGMOID_BCE, lossT, logits.Node(), target.Node());
			}

			NkVar Upsample2x(const NkVar &a) {
				if (a.Value().Device() == NkDevice::NK_GPU) // résident GPU
					return NkMakeOp(NkAutoOp::NK_UPSAMPLE2X, NkGpuUpsample2x(a.Value()), a.Node(), nullptr);
				NkTensor x = ToCpuT(a.Value()).Contiguous(); // noyau CPU
				const NkShape &xs = x.Shape();
				const int64 B = xs[0], C = xs[1], H = xs[2], W = xs[3], oH = 2 * H, oW = 2 * W;
				NkTensor out = NkTensor::Zeros(NkShape{B, C, oH, oW});
				const float *xp = x.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 b = 0; b < B; ++b)
					for (int64 c = 0; c < C; ++c)
						for (int64 y = 0; y < H; ++y)
							for (int64 x2 = 0; x2 < W; ++x2) {
								const float v = xp[(((b * C + c) * H + y) * W + x2)];
								for (int64 dy = 0; dy < 2; ++dy)
									for (int64 dx = 0; dx < 2; ++dx)
										op[(((b * C + c) * oH + (2 * y + dy)) * oW + (2 * x2 + dx))] = v;
							}
				return NkMakeOp(NkAutoOp::NK_UPSAMPLE2X, ToDevOf(out, a.Value()), a.Node(), nullptr);
			}

			NkVar LayerNorm(const NkVar &x) {
				NkTensor y = (x.Value().Device() == NkDevice::NK_GPU) ? NkGpuLayerNormStd(x.Value())
																	  : LayerNormStdCpu(x.Value());
				return NkMakeOp(NkAutoOp::NK_LAYERNORM, y, x.Node(), nullptr);
			}

			NkVar Softmax(const NkVar &x) {
				return NkMakeOp(NkAutoOp::NK_SOFTMAX, SoftmaxRows(x.Value()), x.Node(), nullptr);
			}

			NkVar SoftmaxCausal(const NkVar &x) {
				NkTensor y = (x.Value().Device() == NkDevice::NK_GPU) ? NkGpuSoftmaxCausal(x.Value())
																	  : SoftmaxCausalCpu(x.Value());
				return NkMakeOp(NkAutoOp::NK_SOFTMAX_CAUSAL, y, x.Node(), nullptr);
			}

			NkVar Gelu(const NkVar &x) {
				NkTensor y = (x.Value().Device() == NkDevice::NK_GPU) ? NkGpuGelu(x.Value()) : GeluCpu(x.Value());
				return NkMakeOp(NkAutoOp::NK_GELU, y, x.Node(), nullptr);
			}

			NkVar Embedding(const NkVar &table, const NkTensor &indices) {
				NkTensor out = (table.Value().Device() == NkDevice::NK_GPU) ? NkGpuEmbedding(table.Value(), indices)
																			: EmbeddingCpu(table.Value(), indices);
				NkVar v = NkMakeOp(NkAutoOp::NK_EMBEDDING, out, table.Node(), nullptr);
				v.Node()->aux = indices; // indices (non différentiables) pour le backward
				return v;
			}

			NkVar Permute(const NkVar &x, const NkShape &order) {
				NkTensor y = x.Value().Permute(order).Contiguous();
				NkVar v = NkMakeOp(NkAutoOp::NK_PERMUTE, y, x.Node(), nullptr);
				for (uint32 i = 0; i < order.Size() && i < 4; ++i)
					v.Node()->iparam[i] = (int32)order[i];
				return v;
			}

			NkVar Conv2D(const NkVar &input, const NkVar &weight, int32 stride, int32 pad) {
				// Conv = im2col + matmul (matmul GPU-accéléré pour les grosses convs).
				NkTensor x = input.Value().Contiguous();
				NkTensor w = weight.Value().Contiguous();
				const NkShape &xs = x.Shape();
				const NkShape &ws = w.Shape();
				const int64 B = xs[0], Cin = xs[1], H = xs[2], W = xs[3];
				const int64 Cout = ws[0], kH = ws[2], kW = ws[3];
				const int64 outH = (H + 2 * pad - kH) / stride + 1;
				const int64 outW = (W + 2 * pad - kW) / stride + 1;
				const int64 K = Cin * kH * kW;

				NkTensor col = Im2Col(x, kH, kW, stride, pad, outH, outW);				  // [M, K]
				NkTensor W2dT = w.Reshape(NkShape{Cout, K}).Transpose(0, 1).Contiguous(); // [K, Cout]
				NkTensor out2d = ops::Matmul(col, W2dT);								  // [M, Cout] (GPU si gros)
				// [B,outH,outW,Cout] -> [B,Cout,outH,outW]
				NkTensor out = out2d.Reshape(NkShape{B, outH, outW, Cout}).Permute(NkShape{0, 3, 1, 2}).Contiguous();

				NkVar v = NkMakeOp(NkAutoOp::NK_CONV2D, out, input.Node(), weight.Node());
				v.Node()->iparam[0] = stride;
				v.Node()->iparam[1] = pad;
				return v;
			}

			NkVar MaxPool2D(const NkVar &input, int32 kernel, int32 stride) {
				if (input.Value().Device() == NkDevice::NK_GPU) { // maxpool résident GPU
					NkTensor argG;
					NkTensor outG = NkGpuMaxPool2D(input.Value(), kernel, stride, argG);
					NkVar v = NkMakeOp(NkAutoOp::NK_MAXPOOL2D, outG, input.Node(), nullptr);
					v.Node()->iparam[0] = kernel;
					v.Node()->iparam[1] = stride;
					v.Node()->aux = argG; // indices argmax (GPU)
					return v;
				}
				NkTensor x = ToCpuT(input.Value()).Contiguous(); // noyau CPU
				const NkShape &xs = x.Shape();
				const int64 B = xs[0], C = xs[1], H = xs[2], W = xs[3];
				const int64 outH = (H - kernel) / stride + 1;
				const int64 outW = (W - kernel) / stride + 1;
				NkTensor out = NkTensor::Zeros(NkShape{B, C, outH, outW});
				NkTensor arg = NkTensor::Zeros(NkShape{B, C, outH, outW}); // argmax (index HW)
				const float *xp = x.DataAs<float>();
				float *op = out.DataAs<float>();
				float *ap = arg.DataAs<float>();
				for (int64 b = 0; b < B; ++b)
					for (int64 c = 0; c < C; ++c)
						for (int64 oy = 0; oy < outH; ++oy)
							for (int64 ox = 0; ox < outW; ++ox) {
								float best = -1e30f;
								int64 bestIdx = 0;
								for (int64 ky = 0; ky < kernel; ++ky)
									for (int64 kx = 0; kx < kernel; ++kx) {
										const int64 iy = oy * stride + ky, ix = ox * stride + kx;
										const float v = xp[((b * C + c) * H + iy) * W + ix];
										if (v > best) {
											best = v;
											bestIdx = iy * W + ix;
										}
									}
								const int64 oidx = ((b * C + c) * outH + oy) * outW + ox;
								op[oidx] = best;
								ap[oidx] = (float)bestIdx;
							}
				NkVar v = NkMakeOp(NkAutoOp::NK_MAXPOOL2D, ToDevOf(out, input.Value()), input.Node(), nullptr);
				v.Node()->iparam[0] = kernel;
				v.Node()->iparam[1] = stride;
				v.Node()->aux = arg; // argmax gardé CPU
				return v;
			}

			NkVar Reshape(const NkVar &a, const NkShape &shape) {
				NkTensor out = a.Value().Contiguous().Reshape(shape);
				return NkMakeOp(NkAutoOp::NK_RESHAPE, out, a.Node(), nullptr);
			}

			NkVar ConvTranspose2D(const NkVar &input, const NkVar &weight, int32 stride, int32 pad) {
				if (input.Value().Device() == NkDevice::NK_GPU) { // résident GPU
					NkTensor outG = NkGpuConvTranspose2D(input.Value(), weight.Value(), stride, pad);
					NkVar v = NkMakeOp(NkAutoOp::NK_CONVT2D, outG, input.Node(), weight.Node());
					v.Node()->iparam[0] = stride;
					v.Node()->iparam[1] = pad;
					return v;
				}
				NkTensor x = ToCpuT(input.Value()).Contiguous();  // [B,Cin,H,W] (CPU-only)
				NkTensor w = ToCpuT(weight.Value()).Contiguous(); // [Cin,Cout,kH,kW]
				const NkShape &xs = x.Shape();
				const NkShape &ws = w.Shape();
				const int64 B = xs[0], Cin = xs[1], H = xs[2], W = xs[3];
				const int64 Cout = ws[1], kH = ws[2], kW = ws[3];
				const int64 outH = (H - 1) * stride - 2 * pad + kH;
				const int64 outW = (W - 1) * stride - 2 * pad + kW;
				NkTensor out = NkTensor::Zeros(NkShape{B, Cout, outH, outW});
				const float *xp = x.DataAs<float>();
				const float *wp = w.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 b = 0; b < B; ++b)
					for (int64 ic = 0; ic < Cin; ++ic)
						for (int64 iy = 0; iy < H; ++iy)
							for (int64 ix = 0; ix < W; ++ix) {
								const float xv = xp[((b * Cin + ic) * H + iy) * W + ix];
								for (int64 oc = 0; oc < Cout; ++oc)
									for (int64 ky = 0; ky < kH; ++ky)
										for (int64 kx = 0; kx < kW; ++kx) {
											const int64 oy = iy * stride - pad + ky, ox = ix * stride - pad + kx;
											if (oy < 0 || oy >= outH || ox < 0 || ox >= outW)
												continue;
											op[((b * Cout + oc) * outH + oy) * outW + ox] +=
												xv * wp[((ic * Cout + oc) * kH + ky) * kW + kx];
										}
							}
				NkVar v = NkMakeOp(NkAutoOp::NK_CONVT2D, ToDevOf(out, input.Value()), input.Node(), weight.Node());
				v.Node()->iparam[0] = stride;
				v.Node()->iparam[1] = pad;
				return v;
			}

			NkVar Exp(const NkVar &a) {
				return NkMakeOp(NkAutoOp::NK_EXP, ops::Exp(a.Value()), a.Node(), nullptr);
			}

			NkVar MulScalar(const NkVar &a, double s) {
				NkVar v = NkMakeOp(NkAutoOp::NK_MULSCALAR, ops::MulScalar(a.Value(), s), a.Node(), nullptr);
				v.Node()->fparam = s;
				return v;
			}

			NkVar AddScalar(const NkVar &a, double s) {
				NkVar v = NkMakeOp(NkAutoOp::NK_ADDSCALAR, ops::AddScalar(a.Value(), s), a.Node(), nullptr);
				v.Node()->fparam = s;
				return v;
			}

			NkVar Conv3D(const NkVar &input, const NkVar &weight, int32 stride, int32 pad) {
				if (input.Value().Device() == NkDevice::NK_GPU) { // résident GPU
					NkVar v = NkMakeOp(NkAutoOp::NK_CONV3D, NkGpuConv3D(input.Value(), weight.Value(), stride, pad),
									   input.Node(), weight.Node());
					v.Node()->iparam[0] = stride;
					v.Node()->iparam[1] = pad;
					return v;
				}
				NkTensor x = ToCpuT(input.Value()).Contiguous();  // [B,Cin,D,H,W] (CPU-only)
				NkTensor w = ToCpuT(weight.Value()).Contiguous(); // [Cout,Cin,kD,kH,kW]
				const NkShape &xs = x.Shape();
				const NkShape &ws = w.Shape();
				const int64 B = xs[0], Cin = xs[1], Dd = xs[2], H = xs[3], W = xs[4];
				const int64 Cout = ws[0], kD = ws[2], kH = ws[3], kW = ws[4];
				const int64 oD = (Dd + 2 * pad - kD) / stride + 1, oH = (H + 2 * pad - kH) / stride + 1,
							oW = (W + 2 * pad - kW) / stride + 1;
				NkTensor out = NkTensor::Zeros(NkShape{B, Cout, oD, oH, oW});
				const float *xp = x.DataAs<float>();
				const float *wp = w.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 b = 0; b < B; ++b)
					for (int64 oc = 0; oc < Cout; ++oc)
						for (int64 od = 0; od < oD; ++od)
							for (int64 oy = 0; oy < oH; ++oy)
								for (int64 ox = 0; ox < oW; ++ox) {
									double s = 0.0;
									for (int64 ic = 0; ic < Cin; ++ic)
										for (int64 kz = 0; kz < kD; ++kz)
											for (int64 ky = 0; ky < kH; ++ky)
												for (int64 kx = 0; kx < kW; ++kx) {
													const int64 iz = od * stride - pad + kz,
																iy = oy * stride - pad + ky,
																ix = ox * stride - pad + kx;
													if (iz < 0 || iz >= Dd || iy < 0 || iy >= H || ix < 0 || ix >= W)
														continue;
													s +=
														(double)xp[((((b * Cin + ic) * Dd + iz) * H + iy) * W + ix)] *
														(double)wp[((((oc * Cin + ic) * kD + kz) * kH + ky) * kW + kx)];
												}
									op[((((b * Cout + oc) * oD + od) * oH + oy) * oW + ox)] = (float)s;
								}
				NkVar v = NkMakeOp(NkAutoOp::NK_CONV3D, ToDevOf(out, input.Value()), input.Node(), weight.Node());
				v.Node()->iparam[0] = stride;
				v.Node()->iparam[1] = pad;
				return v;
			}

			NkVar ConvTranspose3D(const NkVar &input, const NkVar &weight, int32 stride, int32 pad) {
				if (input.Value().Device() == NkDevice::NK_GPU) { // résident GPU
					NkVar v =
						NkMakeOp(NkAutoOp::NK_CONVT3D, NkGpuConvTranspose3D(input.Value(), weight.Value(), stride, pad),
								 input.Node(), weight.Node());
					v.Node()->iparam[0] = stride;
					v.Node()->iparam[1] = pad;
					return v;
				}
				NkTensor x = ToCpuT(input.Value()).Contiguous();  // [B,Cin,D,H,W] (CPU-only)
				NkTensor w = ToCpuT(weight.Value()).Contiguous(); // [Cin,Cout,kD,kH,kW]
				const NkShape &xs = x.Shape();
				const NkShape &ws = w.Shape();
				const int64 B = xs[0], Cin = xs[1], Dd = xs[2], H = xs[3], W = xs[4];
				const int64 Cout = ws[1], kD = ws[2], kH = ws[3], kW = ws[4];
				const int64 oD = (Dd - 1) * stride - 2 * pad + kD, oH = (H - 1) * stride - 2 * pad + kH,
							oW = (W - 1) * stride - 2 * pad + kW;
				NkTensor out = NkTensor::Zeros(NkShape{B, Cout, oD, oH, oW});
				const float *xp = x.DataAs<float>();
				const float *wp = w.DataAs<float>();
				float *op = out.DataAs<float>();
				for (int64 b = 0; b < B; ++b)
					for (int64 ic = 0; ic < Cin; ++ic)
						for (int64 iz = 0; iz < Dd; ++iz)
							for (int64 iy = 0; iy < H; ++iy)
								for (int64 ix = 0; ix < W; ++ix) {
									const float xv = xp[((((b * Cin + ic) * Dd + iz) * H + iy) * W + ix)];
									for (int64 oc = 0; oc < Cout; ++oc)
										for (int64 kz = 0; kz < kD; ++kz)
											for (int64 ky = 0; ky < kH; ++ky)
												for (int64 kx = 0; kx < kW; ++kx) {
													const int64 od = iz * stride - pad + kz,
																oy = iy * stride - pad + ky,
																ox = ix * stride - pad + kx;
													if (od < 0 || od >= oD || oy < 0 || oy >= oH || ox < 0 || ox >= oW)
														continue;
													op[((((b * Cout + oc) * oD + od) * oH + oy) * oW + ox)] +=
														xv * wp[((((ic * Cout + oc) * kD + kz) * kH + ky) * kW + kx)];
												}
								}
				NkVar v = NkMakeOp(NkAutoOp::NK_CONVT3D, ToDevOf(out, input.Value()), input.Node(), weight.Node());
				v.Node()->iparam[0] = stride;
				v.Node()->iparam[1] = pad;
				return v;
			}

		} // namespace autograd

	} // namespace ai
} // namespace nkentseu
