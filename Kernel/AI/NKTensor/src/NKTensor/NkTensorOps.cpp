// =============================================================================
// NkTensorOps.cpp — opérations CPU (NKAI). From-scratch, correct-first.
// (Optimisation SIMD/fusion = « Plus tard » dans la ROADMAP.)
// =============================================================================
#include "NKTensor/NkTensorOps.h"
#include "NKTensor/NkTensorGpu.h" // dispatch GPU (NkGpuAdd / NkGpuMatmul)

#include <cstdio>
#include <cmath>

namespace nkentseu {
	namespace ai {
		namespace ops {

			// Odomètre multi-dim local (le NkTensor.cpp a le sien, non exporté).
			static bool NextIndex(NkShape &idx, const NkShape &shape) {
				for (int32 d = (int32)shape.Size() - 1; d >= 0; --d) {
					if (++idx[(uint32)d] < shape[(uint32)d])
						return true;
					idx[(uint32)d] = 0;
				}
				return false;
			}

			static void ZeroIndex(NkShape &idx, uint32 rank) {
				idx.Resize(rank);
				for (uint32 i = 0; i < rank; i++)
					idx[i] = 0;
			}

			// ---- Broadcasting --------------------------------------------------
			static NkShape BroadcastShape(const NkShape &a, const NkShape &b, bool &ok) {
				ok = true;
				const uint32 ra = (uint32)a.Size(), rb = (uint32)b.Size();
				const uint32 r = ra > rb ? ra : rb;
				NkShape out;
				out.Resize(r);
				for (uint32 i = 0; i < r; i++) {
					const int64 da = (i < r - ra) ? 1 : a[i - (r - ra)];
					const int64 db = (i < r - rb) ? 1 : b[i - (r - rb)];
					if (da == db)
						out[i] = da;
					else if (da == 1)
						out[i] = db;
					else if (db == 1)
						out[i] = da;
					else {
						ok = false;
						out[i] = 0;
					}
				}
				return out;
			}

			// Strides de `in` alignés sur `outShape` (0 = dimension diffusée).
			static NkShape AlignedStrides(const NkShape &inShape, const NkShape &inStrides, const NkShape &outShape) {
				const uint32 ri = (uint32)inShape.Size(), ro = (uint32)outShape.Size();
				NkShape s;
				s.Resize(ro);
				for (uint32 i = 0; i < ro; i++) {
					if (i < ro - ri) {
						s[i] = 0;
					} else {
						const uint32 di = i - (ro - ri);
						s[i] = (inShape[di] == 1) ? 0 : inStrides[di];
					}
				}
				return s;
			}

			// ---- Kernels scalaires --------------------------------------------
			enum class BinOp { Add, Sub, Mul, Div };

			template <typename T> static inline T ApplyBin(BinOp op, T x, T y) {
				switch (op) {
					case BinOp::Add:
						return (T)(x + y);
					case BinOp::Sub:
						return (T)(x - y);
					case BinOp::Mul:
						return (T)(x * y);
					case BinOp::Div:
						return (y != (T)0) ? (T)(x / y) : (T)0;
				}
				return x;
			}
			enum class UnOp { Neg, Abs, Exp, Sqrt, Log, Relu, Sigmoid, Tanh };

			template <typename T> static inline T ApplyUn(UnOp op, T x) {
				switch (op) {
					case UnOp::Neg:
						return (T)(-(x));
					case UnOp::Abs:
						return x < (T)0 ? (T)(-(x)) : x;
					case UnOp::Exp:
						return (T)exp((double)x);
					case UnOp::Sqrt:
						return (T)sqrt((double)x);
					case UnOp::Log: { // log(max(x,eps)) : stabilise log(0) (eps=1e-8, cf NkVar::Log)
						const double xd = (double)x;
						return (T)log(xd > 1e-8 ? xd : 1e-8);
					}
					case UnOp::Relu:
						return x > (T)0 ? x : (T)0;
					case UnOp::Sigmoid:
						return (T)(1.0 / (1.0 + exp(-(double)x)));
					case UnOp::Tanh:
						return (T)tanh((double)x);
				}
				return x;
			}

			// ---- Binaire générique --------------------------------------------
			static NkTensor Binary(const NkTensor &a, const NkTensor &b, BinOp op) {
				if (a.DType() != b.DType()) {
					fprintf(stderr, "[NkTensorOps] binaire : dtype incompatibles (%s vs %s)\n", NkDTypeName(a.DType()),
							NkDTypeName(b.DType()));
					return NkTensor();
				}
				bool ok = false;
				NkShape os = BroadcastShape(a.Shape(), b.Shape(), ok);
				if (!ok) {
					fprintf(stderr, "[NkTensorOps] formes non-broadcastables\n");
					return NkTensor();
				}

				NkTensor out = NkTensor::Empty(os, a.DType());
				const NkShape sa = AlignedStrides(a.Shape(), a.Strides(), os);
				const NkShape sb = AlignedStrides(b.Shape(), b.Strides(), os);
				const nk_size isz = NkDTypeSize(a.DType());
				const char *baseA = (const char *)a.RawData();
				const char *baseB = (const char *)b.RawData();
				char *dst = (char *)out.RawData();

				NK_DTYPE_DISPATCH(a.DType(), T, {
					NkShape idx;
					ZeroIndex(idx, (uint32)os.Size());
					int64 k = 0;
					do {
						int64 oa = 0, ob = 0;
						for (uint32 d = 0; d < os.Size(); d++) {
							oa += idx[d] * sa[d];
							ob += idx[d] * sb[d];
						}
						const T x = *reinterpret_cast<const T *>(baseA + (nk_size)oa * isz);
						const T y = *reinterpret_cast<const T *>(baseB + (nk_size)ob * isz);
						reinterpret_cast<T *>(dst)[k++] = ApplyBin<T>(op, x, y);
					} while (NextIndex(idx, os));
				});
				return out;
			}

			// Les noyaux GPU élémentaires exigent le MÊME nombre d'éléments. Pour un
			// broadcast (ex. biais [1,N] + [B,N]), on retombe sur le CPU (qui gère le
			// broadcast) en PRÉSERVANT le device -> résultat ramené sur GPU.
			// Broadcast d'un VECTEUR sur le dernier axe (biais [1,C]+[..,C], affine LayerNorm) :
			// reste sur GPU. Renvoie invalide si le motif ne s'y prête pas.
			static NkTensor TryRowBroadcast(const NkTensor &a, const NkTensor &b, bool isAdd) {
				const NkTensor &big = (a.Numel() >= b.Numel()) ? a : b;
				const NkTensor &sml = (a.Numel() >= b.Numel()) ? b : a;
				if (big.Rank() >= 1 && sml.Numel() == big.Shape()[big.Rank() - 1] && (big.Numel() % sml.Numel()) == 0)
					return isAdd ? NkGpuAddBroadcastRow(big, sml) : NkGpuMulBroadcastRow(big, sml);
				return NkTensor();
			}

			NkTensor Add(const NkTensor &a, const NkTensor &b) {
				if (a.Device() == NkDevice::NK_GPU || b.Device() == NkDevice::NK_GPU) {
					if (a.Numel() == b.Numel())
						return NkGpuAdd(a, b);
					NkTensor r = TryRowBroadcast(a, b, /*isAdd*/ true); // biais/affine sur GPU
					if (r.IsValid())
						return r;
					return Binary(a.ToCPU(), b.ToCPU(), BinOp::Add).ToGPU(); // broadcast général
				}
				return Binary(a, b, BinOp::Add);
			}

			NkTensor Sub(const NkTensor &a, const NkTensor &b) {
				if (a.Device() == NkDevice::NK_GPU || b.Device() == NkDevice::NK_GPU) {
					if (a.Numel() == b.Numel())
						return NkGpuSub(a, b);
					return Binary(a.ToCPU(), b.ToCPU(), BinOp::Sub).ToGPU(); // broadcast
				}
				return Binary(a, b, BinOp::Sub);
			}

			NkTensor Mul(const NkTensor &a, const NkTensor &b) {
				if (a.Device() == NkDevice::NK_GPU || b.Device() == NkDevice::NK_GPU) {
					if (a.Numel() != b.Numel()) {
						NkTensor r = TryRowBroadcast(a, b, /*isAdd*/ false); // affine sur GPU
						if (r.IsValid())
							return r;
						return Binary(a.ToCPU(), b.ToCPU(), BinOp::Mul).ToGPU(); // broadcast général
					}
					return NkGpuMul(a, b);
				}
				return Binary(a, b, BinOp::Mul);
			}

			NkTensor Div(const NkTensor &a, const NkTensor &b) {
				if (a.Device() == NkDevice::NK_GPU || b.Device() == NkDevice::NK_GPU) {
					if (a.Numel() == b.Numel())
						return NkGpuDiv(a, b);
					return Binary(a.ToCPU(), b.ToCPU(), BinOp::Div).ToGPU(); // broadcast
				}
				return Binary(a, b, BinOp::Div);
			}

			// ---- Unaire générique ---------------------------------------------
			static NkTensor Unary(const NkTensor &a, UnOp op, bool floatOnly) {
				if (floatOnly && !NkDTypeIsFloat(a.DType())) {
					fprintf(stderr, "[NkTensorOps] op unaire flottante sur dtype %s\n", NkDTypeName(a.DType()));
					return NkTensor();
				}
				// Résidence GPU : Neg a un noyau (mul par -1) ; Exp/Sqrt/Abs n'en ont pas
				// encore -> repli CPU en PRÉSERVANT le device (l'op fait un aller-retour).
				if (a.Device() == NkDevice::NK_GPU) {
					if (op == UnOp::Neg)
						return NkGpuMulScalar(a, -1.0);
					return Unary(a.ToCPU(), op, floatOnly).ToGPU();
				}
				NkTensor out = NkTensor::Empty(a.Shape(), a.DType());
				const int64 n = a.Numel();
				const char *base = (const char *)a.RawData();
				char *dst = (char *)out.RawData();
				const nk_size isz = a.ItemSize();
				const bool contig = a.IsContiguous();
				const NkShape &sh = a.Shape();
				const NkShape &st = a.Strides();

				NK_DTYPE_DISPATCH(a.DType(), T, {
					if (contig) {
						const T *s = reinterpret_cast<const T *>(base);
						T *d = reinterpret_cast<T *>(dst);
						for (int64 i = 0; i < n; i++)
							d[i] = ApplyUn<T>(op, s[i]);
					} else {
						NkShape idx;
						ZeroIndex(idx, (uint32)sh.Size());
						int64 k = 0;
						do {
							int64 off = 0;
							for (uint32 d = 0; d < sh.Size(); d++)
								off += idx[d] * st[d];
							const T x = *reinterpret_cast<const T *>(base + (nk_size)off * isz);
							reinterpret_cast<T *>(dst)[k++] = ApplyUn<T>(op, x);
						} while (NextIndex(idx, sh));
					}
				});
				return out;
			}

			NkTensor Neg(const NkTensor &a) {
				return Unary(a, UnOp::Neg, false);
			}

			NkTensor Abs(const NkTensor &a) {
				return Unary(a, UnOp::Abs, false);
			}

			NkTensor Exp(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuExp(a);
				return Unary(a, UnOp::Exp, true);
			}

			NkTensor Sqrt(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuSqrt(a);
				return Unary(a, UnOp::Sqrt, true);
			}

			NkTensor Log(const NkTensor &a) {
				// Pas de noyau GPU dédié : Unary() fait l'aller-retour CPU en préservant le device.
				return Unary(a, UnOp::Log, true);
			}

			NkTensor Relu(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuRelu(a);
				return Unary(a, UnOp::Relu, false);
			}

			NkTensor Sigmoid(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuSigmoid(a);
				return Unary(a, UnOp::Sigmoid, true);
			}

			NkTensor Tanh(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuTanh(a);
				return Unary(a, UnOp::Tanh, true);
			}

			NkTensor AddScalar(const NkTensor &a, double s) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuAddScalar(a, s);
				NkTensor out = NkTensor::Empty(a.Shape(), a.DType());
				const int64 n = a.Numel();
				NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
				NK_DTYPE_DISPATCH(a.DType(), T, {
					const T *src = ac.DataAs<T>();
					T *d = out.DataAs<T>();
					const T sv = (T)s;
					for (int64 i = 0; i < n; i++)
						d[i] = (T)(src[i] + sv);
				});
				return out;
			}

			NkTensor MulScalar(const NkTensor &a, double s) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuMulScalar(a, s);
				NkTensor out = NkTensor::Empty(a.Shape(), a.DType());
				const int64 n = a.Numel();
				NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
				NK_DTYPE_DISPATCH(a.DType(), T, {
					const T *src = ac.DataAs<T>();
					T *d = out.DataAs<T>();
					const T sv = (T)s;
					for (int64 i = 0; i < n; i++)
						d[i] = (T)(src[i] * sv);
				});
				return out;
			}

			// ---- Produit de matrices par LOTS (rang > 2 : dims de tête = lots) ----
			// a[..,M,K] · b[..,K,N] -> [..,M,N]. Dims de tête (lots) identiques des deux côtés.
			static NkTensor MatmulBatched(const NkTensor &a, const NkTensor &b) {
				const uint32 ra = a.Rank(), rb = b.Rank();
				if (ra < 3 || rb != ra) {
					fprintf(stderr, "[NkTensorOps] Matmul batched : rangs incompatibles (a=%u,b=%u)\n", ra, rb);
					return NkTensor();
				}
				int64 batch = 1;
				for (uint32 i = 0; i < ra - 2; ++i) {
					if (a.Shape()[i] != b.Shape()[i]) {
						fprintf(stderr, "[NkTensorOps] Matmul batched : lots differents\n");
						return NkTensor();
					}
					batch *= a.Shape()[i];
				}
				const int64 M = a.Shape()[ra - 2], K = a.Shape()[ra - 1];
				const int64 N = b.Shape()[rb - 1];
				if (b.Shape()[rb - 2] != K) {
					fprintf(stderr, "[NkTensorOps] Matmul batched : K incompatibles\n");
					return NkTensor();
				}
				NkShape outShape;
				outShape.Resize(ra);
				for (uint32 i = 0; i < ra - 2; ++i)
					outShape[i] = a.Shape()[i];
				outShape[ra - 2] = M;
				outShape[ra - 1] = N;
				if (a.Device() == NkDevice::NK_GPU || b.Device() == NkDevice::NK_GPU) {
					NkTensor ar = a.Reshape(NkShape{batch, M, K});
					NkTensor br = b.Reshape(NkShape{batch, K, N});
					return NkGpuBatchedMatmul(ar, br).Reshape(outShape);
				}
				// CPU
				NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
				NkTensor bc = b.IsContiguous() ? b : b.Contiguous();
				NkTensor out = NkTensor::Zeros(outShape, a.DType());
				const float *A = ac.DataAs<float>();
				const float *B = bc.DataAs<float>();
				float *C = out.DataAs<float>();
				for (int64 bi = 0; bi < batch; ++bi) {
					const float *Ab = A + bi * M * K;
					const float *Bb = B + bi * K * N;
					float *Cb = C + bi * M * N;
					for (int64 i = 0; i < M; ++i)
						for (int64 k = 0; k < K; ++k) {
							const float aik = Ab[i * K + k];
							const float *Brow = Bb + k * N;
							float *Crow = Cb + i * N;
							for (int64 j = 0; j < N; ++j)
								Crow[j] += aik * Brow[j];
						}
				}
				return out;
			}

			// ---- Produit de matrices (2D, flottants) --------------------------
			NkTensor Matmul(const NkTensor &a, const NkTensor &b) {
				if (a.Rank() > 2 || b.Rank() > 2)
					return MatmulBatched(a, b); // par lots
				// Dispatch GPU si un opérande y réside (kernel NkSL matmul).
				if (a.Device() == NkDevice::NK_GPU || b.Device() == NkDevice::NK_GPU)
					return NkGpuMatmul(a, b);
				if (a.Rank() != 2 || b.Rank() != 2) {
					fprintf(stderr, "[NkTensorOps] Matmul : rang 2 requis (a=%u, b=%u)\n", a.Rank(), b.Rank());
					return NkTensor();
				}
				if (a.DType() != b.DType() || !NkDTypeIsFloat(a.DType())) {
					fprintf(stderr, "[NkTensorOps] Matmul : flottants de meme dtype requis\n");
					return NkTensor();
				}
				const int64 M = a.Shape()[0], K = a.Shape()[1];
				const int64 K2 = b.Shape()[0], N = b.Shape()[1];
				if (K != K2) {
					fprintf(stderr, "[NkTensorOps] Matmul : dimensions incompatibles (%lldx%lld * %lldx%lld)\n",
							(long long)M, (long long)K, (long long)K2, (long long)N);
					return NkTensor();
				}
				// Auto-accélération GPU pour les GRANDES matrices : résultats identiques
				// au CPU (cf NKGpuBenchTest : ~100-160× dès 256²). Sous le seuil, le
				// transfert + l'init ne valent pas le coup -> on reste CPU. Si le GPU est
				// indispo, ToGPU renvoie un tenseur CPU -> on retombe naturellement sur CPU.
				if (a.DType() == NkDType::NK_F32 && (double)M * (double)N * (double)K >= 8.0e6) {
					NkTensor ag = a.ToGPU();
					if (ag.Device() == NkDevice::NK_GPU) {
						NkTensor bg = b.ToGPU();
						if (bg.Device() == NkDevice::NK_GPU)
							return NkGpuMatmul(ag, bg).ToCPU();
					}
				}

				NkTensor ac = a.IsContiguous() ? a : a.Contiguous();
				NkTensor bc = b.IsContiguous() ? b : b.Contiguous();
				NkTensor out = NkTensor::Zeros(NkShape{M, N}, a.DType());

				NK_DTYPE_DISPATCH_FLOAT(a.DType(), T, {
					const T *A = ac.DataAs<T>();
					const T *B = bc.DataAs<T>();
					T *C = out.DataAs<T>();
					// Ordre i-k-j : accès B et C par lignes (cache-friendly).
					for (int64 i = 0; i < M; i++) {
						for (int64 k = 0; k < K; k++) {
							const T aik = A[i * K + k];
							const T *Brow = &B[k * N];
							T *Crow = &C[i * N];
							for (int64 j = 0; j < N; j++)
								Crow[j] = (T)(Crow[j] + aik * Brow[j]);
						}
					}
				});
				return out;
			}

			// ---- Réductions globales ------------------------------------------
			NkTensor Sum(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuReduceAll(a, 0);
				double acc = 0.0;
				NkShape idx;
				ZeroIndex(idx, a.Rank());
				if (a.Numel() > 0)
					do {
						acc += a.GetItem(idx);
					} while (NextIndex(idx, a.Shape()));
				NkTensor out = NkTensor::Empty(NkShape{1}, a.DType());
				out.SetItem(NkShape{0}, acc);
				return out;
			}

			NkTensor Mean(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuReduceAll(a, 1);
				const int64 n = a.Numel();
				NkTensor s = Sum(a);
				double v = s.GetItem(NkShape{0}) / (n > 0 ? (double)n : 1.0);
				s.SetItem(NkShape{0}, v);
				return s;
			}

			NkTensor Max(const NkTensor &a) {
				if (a.Device() == NkDevice::NK_GPU)
					return NkGpuReduceAll(a, 2);
				NkShape idx;
				ZeroIndex(idx, a.Rank());
				double m = 0.0;
				bool first = true;
				if (a.Numel() > 0)
					do {
						const double v = a.GetItem(idx);
						if (first || v > m) {
							m = v;
							first = false;
						}
					} while (NextIndex(idx, a.Shape()));
				NkTensor out = NkTensor::Empty(NkShape{1}, a.DType());
				out.SetItem(NkShape{0}, m);
				return out;
			}

			// ---- Réductions le long d'un axe ----------------------------------
			enum class RedKind { Sum, Mean, Max, Argmax };

			static NkTensor ReduceAxis(const NkTensor &a, uint32 axis, RedKind kind) {
				const uint32 r = a.Rank();
				if (axis >= r) {
					fprintf(stderr, "[NkTensorOps] axe %u hors rang %u\n", axis, r);
					return NkTensor();
				}

				// Dispatch GPU (résidence). Argmax : calcul sur GPU (indices en f32) puis
				// conversion en i64 (petit téléchargement des seuls indices).
				if (a.Device() == NkDevice::NK_GPU) {
					if (kind == RedKind::Argmax) {
						NkTensor idxF = NkGpuReduceAxis(a, axis, 3).ToCPU();
						NkTensor out = NkTensor::Empty(idxF.Shape(), NkDType::NK_I64);
						NkShape ii;
						ZeroIndex(ii, (uint32)idxF.Shape().Size());
						if (idxF.Numel() > 0)
							do {
								out.SetItem(ii, idxF.GetItem(ii));
							} while (NextIndex(ii, idxF.Shape()));
						return out;
					}
					const int k = (kind == RedKind::Sum) ? 0 : (kind == RedKind::Mean) ? 1 : 2;
					return NkGpuReduceAxis(a, axis, k);
				}

				// Rang 1 : l'axe disparaît -> résultat scalaire {1}.
				if (r == 1) {
					if (kind == RedKind::Sum)
						return Sum(a);
					if (kind == RedKind::Mean)
						return Mean(a);
					if (kind == RedKind::Max)
						return Max(a);
					// Argmax global
					const int64 n = a.Shape()[0];
					double best = 0.0;
					int64 bi = 0;
					bool first = true;
					for (int64 j = 0; j < n; j++) {
						const double v = a.GetItem(NkShape{j});
						if (first || v > best) {
							best = v;
							bi = j;
							first = false;
						}
					}
					NkTensor out = NkTensor::Empty(NkShape{1}, NkDType::NK_I64);
					out.SetItem(NkShape{0}, (double)bi);
					return out;
				}

				const int64 axisSize = a.Shape()[axis];
				NkShape outShape;
				outShape.Resize(r - 1);
				for (uint32 i = 0, oi = 0; i < r; i++)
					if (i != axis)
						outShape[oi++] = a.Shape()[i];

				const NkDType outDt = (kind == RedKind::Argmax) ? NkDType::NK_I64 : a.DType();
				NkTensor out = NkTensor::Empty(outShape, outDt);

				NkShape outIdx;
				ZeroIndex(outIdx, (uint32)outShape.Size());
				NkShape inIdx;
				inIdx.Resize(r);
				do {
					// Reconstruit l'index d'entrée (l'axe réduit sera balayé).
					for (uint32 i = 0, oi = 0; i < r; i++)
						inIdx[i] = (i == axis) ? 0 : outIdx[oi++];

					double acc = 0.0, best = 0.0;
					int64 bi = 0;
					bool first = true;
					for (int64 j = 0; j < axisSize; j++) {
						inIdx[axis] = j;
						const double v = a.GetItem(inIdx);
						switch (kind) {
							case RedKind::Sum:
							case RedKind::Mean:
								acc += v;
								break;
							case RedKind::Max:
								if (first || v > best) {
									best = v;
									first = false;
								}
								break;
							case RedKind::Argmax:
								if (first || v > best) {
									best = v;
									bi = j;
									first = false;
								}
								break;
						}
					}
					double res = 0.0;
					switch (kind) {
						case RedKind::Sum:
							res = acc;
							break;
						case RedKind::Mean:
							res = acc / (axisSize > 0 ? (double)axisSize : 1.0);
							break;
						case RedKind::Max:
							res = best;
							break;
						case RedKind::Argmax:
							res = (double)bi;
							break;
					}
					out.SetItem(outIdx, res);
				} while (NextIndex(outIdx, outShape));
				return out;
			}

			NkTensor Sum(const NkTensor &a, uint32 axis) {
				return ReduceAxis(a, axis, RedKind::Sum);
			}

			NkTensor Mean(const NkTensor &a, uint32 axis) {
				return ReduceAxis(a, axis, RedKind::Mean);
			}

			NkTensor Max(const NkTensor &a, uint32 axis) {
				return ReduceAxis(a, axis, RedKind::Max);
			}

			NkTensor Argmax(const NkTensor &a, uint32 axis) {
				return ReduceAxis(a, axis, RedKind::Argmax);
			}

			// ---- Comparaison ---------------------------------------------------
			bool AllClose(const NkTensor &a, const NkTensor &b, double eps) {
				if (!a.IsValid() || !b.IsValid())
					return false;
				if (a.Rank() != b.Rank())
					return false;
				for (uint32 i = 0; i < a.Rank(); i++)
					if (a.Shape()[i] != b.Shape()[i])
						return false;
				if (a.Numel() == 0)
					return true;
				NkShape idx;
				ZeroIndex(idx, a.Rank());
				do {
					double d = a.GetItem(idx) - b.GetItem(idx);
					if (d < 0)
						d = -d;
					if (d > eps)
						return false;
				} while (NextIndex(idx, a.Shape()));
				return true;
			}

		} // namespace ops
	} // namespace ai
} // namespace nkentseu
