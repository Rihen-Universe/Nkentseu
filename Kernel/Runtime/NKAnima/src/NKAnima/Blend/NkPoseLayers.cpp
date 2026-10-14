// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Blend/NkPoseLayers.cpp — la pile de couches de poses fabriquees (voir NkPoseLayers.h).
// =============================================================================
#include "NKAnima/Blend/NkPoseLayers.h"
#include "NKAnima/Realtime/NkRtRig.h" // les mutations des bancs

namespace nkentseu {
	namespace anim {

		namespace {
			/// Le poids d'une couche. MUTATION layers_no_weight : il est ignore (1 partout) --
			/// une couche a moitie posee s'impose en entier.
			float32 Poids(const NkPoseLayer &l) {
				if (rt::NkRtMutated(rt::NK_RT_MUT_LAYERS_NO_WEIGHT)) {
					return 1.f;
				}
				return l.weight < 0.f ? 0.f : (l.weight > 1.f ? 1.f : l.weight);
			}
			/// Le poids de la couche sur un os : 1 sans masque, et 1 au-dela du masque (la
			/// meme regle que NkBlendPose et NkApplyAdditive).
			float32 PoidsOs(const NkPoseLayer &l, uint32 bone) {
				return (l.boneWeights.Empty() || bone >= (uint32)l.boneWeights.Size()) ? 1.f : l.boneWeights[bone];
			}
		} // namespace

		NkPoseLayer &NkPoseLayerStack::Add(const char *name, NkPoseLayerMode mode, float32 weight) {
			NkPoseLayer l;
			l.name = NkString(name != nullptr ? name : "");
			l.mode = mode;
			l.weight = weight;
			mLayers.PushBack(l);
			return mLayers[mLayers.Size() - 1];
		}

		void NkPoseLayerStack::Evaluate(const NkAnimPose &base, NkAnimPose &out) const {
			if (&out != &base) {
				out = base;
			}
			for (const NkPoseLayer &l : mLayers) {
				const float32 w = Poids(l);
				if (w <= 0.f) {
					continue; // la couche ne touche a rien, au bit pres
				}
				const NkVector<float32> *os = l.boneWeights.Empty() ? nullptr : &l.boneWeights;
				if (l.mode == NkPoseLayerMode::NK_POSE_ADDITIVE) {
					NkApplyAdditive(out, l.pose, w, os);
				} else {
					NkBlendPose(out, l.pose, w, os);
				}
			}
		}

		void NkPoseLayerStack::Contributions(uint32 bone, NkVector<float32> &parts, float32 &basePart) const {
			parts.Resize(mLayers.Size());
			float32 reste = 1.f; // ce que les remplacements poses au-dessus laissent passer
			for (usize i = mLayers.Size(); i-- > 0;) {
				const NkPoseLayer &l = mLayers[i];
				float32 w = Poids(l) * PoidsOs(l, bone);
				w = w < 0.f ? 0.f : (w > 1.f ? 1.f : w);
				if (l.mode == NkPoseLayerMode::NK_POSE_ADDITIVE) {
					parts[i] = w;
				} else {
					parts[i] = w * reste;
					reste *= 1.f - w;
				}
			}
			basePart = reste;
		}

	} // namespace anim
} // namespace nkentseu
