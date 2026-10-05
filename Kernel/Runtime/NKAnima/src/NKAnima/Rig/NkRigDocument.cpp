// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkRigDocument.cpp — le document du rig et son annulation (voir NkRigDocument.h).
// =============================================================================
#include "NKAnima/Rig/NkRigDocument.h"
#include "NKAnima/Rig/NkRigMath.h"

#include <cmath>
#include <cstring>

namespace nkentseu {
	namespace anim {

		using math::NkMat4f;
		using math::NkVec3f;
		using namespace rigm;

		namespace {
			constexpr uint32 kHistoireMax = 128;
		}

		// =====================================================================
		// LE MAILLAGE
		// =====================================================================
		void NkRigDocument::SetMesh(const NkVec3f *pos, uint32 n, const uint32 *idx, uint32 nIdx) {
			mesh = NkSkinMesh();
			mesh.positions.Resize(n);
			for (uint32 i = 0; i < n; ++i) {
				mesh.positions[i] = pos[i];
			}
			mesh.indices.Resize(nIdx);
			for (uint32 i = 0; i < nIdx; ++i) {
				mesh.indices[i] = idx[i] < n ? idx[i] : 0u;
			}
			mesh.BuildTopology();
			mesh.BuildMirror();
			mesh.ComputeNormals();
			shapes.SetBasis(mesh.positions.Data(), n);
			weights.Resize(n);
			ClearHistory();
			Touch(NK_RigPart_Tout);
		}

		// =====================================================================
		// L'ANNULATION
		// =====================================================================
		NkRigDocument::Photo NkRigDocument::Prendre(const char *label, uint32 parts) const {
			Photo p;
			p.label = NkString(label != nullptr ? label : "");
			p.parts = parts;
			if (parts & NK_RigPart_Armature) {
				p.armature = armature;
			}
			if (parts & NK_RigPart_Poids) {
				p.weights = weights;
			}
			if (parts & NK_RigPart_Formes) {
				p.shapes = shapes;
			}
			if (parts & NK_RigPart_Reperes) {
				p.landmarks = landmarks;
				p.rigTemplate = rigTemplate;
			}
			if (parts & NK_RigPart_Controles) {
				p.controls = controls;
			}
			return p;
		}

		void NkRigDocument::Rendre(const Photo &p) {
			if (p.parts & NK_RigPart_Armature) {
				// L'uid suivant ne RECULE jamais : un os cree apres l'annulation ne doit pas
				// reprendre l'identite d'un os annule (ses pistes d'animation la portent).
				const uint32 suivant = armature.nextUid;
				armature = p.armature;
				armature.nextUid = suivant > armature.nextUid ? suivant : armature.nextUid;
			}
			if (p.parts & NK_RigPart_Poids) {
				weights = p.weights;
			}
			if (p.parts & NK_RigPart_Formes) {
				shapes = p.shapes;
			}
			if (p.parts & NK_RigPart_Reperes) {
				landmarks = p.landmarks;
				rigTemplate = p.rigTemplate;
			}
			if (p.parts & NK_RigPart_Controles) {
				controls = p.controls;
			}
		}

		void NkRigDocument::Begin(const char *label, uint32 parts, uint32 fusion) {
			if (fusion != 0 && fusion == mFusion && !mUndo.Empty() && (mUndo[(uint32)mUndo.Size() - 1u].parts & parts) == parts) {
				return; // le geste continue : la photo d'avant son debut suffit
			}
			mUndo.PushBack(Prendre(label, parts));
			mRedo.Clear();
			mFusion = fusion;
			if (mUndo.Size() > kHistoireMax) {
				mUndo.RemoveAt(0);
			}
		}

		void NkRigDocument::Cancel() {
			if (!mUndo.Empty()) {
				mUndo.PopBack();
			}
			mFusion = 0;
		}

		void NkRigDocument::Touch(uint32 parts) {
			++revision;
			changes |= parts;
		}

		bool NkRigDocument::Undo() {
			if (mUndo.Empty() || mTrait) {
				return false;
			}
			const Photo p = mUndo[(uint32)mUndo.Size() - 1u];
			mRedo.PushBack(Prendre(p.label.CStr(), p.parts));
			Rendre(p);
			mUndo.PopBack();
			mFusion = 0;
			Touch(p.parts);
			return true;
		}

		bool NkRigDocument::Redo() {
			if (mRedo.Empty() || mTrait) {
				return false;
			}
			const Photo p = mRedo[(uint32)mRedo.Size() - 1u];
			mUndo.PushBack(Prendre(p.label.CStr(), p.parts));
			Rendre(p);
			mRedo.PopBack();
			mFusion = 0;
			Touch(p.parts);
			return true;
		}

		const char *NkRigDocument::UndoLabel() const {
			return mUndo.Empty() ? "" : mUndo[(uint32)mUndo.Size() - 1u].label.CStr();
		}

		const char *NkRigDocument::RedoLabel() const {
			return mRedo.Empty() ? "" : mRedo[(uint32)mRedo.Size() - 1u].label.CStr();
		}

		// =====================================================================
		// LE MODE EDITION
		// =====================================================================
		int32 NkRigDocument::MirrorBone(uint32 i) const {
			if (i >= armature.Count()) {
				return -1;
			}
			const NkString n = NkArmature::MirrorName(armature.bones[i].name.CStr());
			const int32 j = n.Empty() ? -1 : armature.Find(n.CStr());
			return j == (int32)i ? -1 : j;
		}

		int32 NkRigDocument::AddBone(const char *name, const NkVec3f &head, const NkVec3f &tail, int32 parent, bool connected, uint32 fusion) {
			Begin("Ajouter un os", NK_RigPart_Armature, fusion);
			const int32 i = armature.Add(name, head, tail, parent, connected);
			if (i >= 0 && symetrie && armature.Side((uint32)i) != NkBoneSide::NK_BoneSide_Centre) {
				(void)armature.Symmetrize((uint32)i);
			}
			Touch(NK_RigPart_Armature);
			return i;
		}

		int32 NkRigDocument::ExtrudeBone(uint32 i, const NkVec3f &tail, uint32 fusion) {
			if (i >= armature.Count()) {
				return -1;
			}
			Begin("Extruder un os", NK_RigPart_Armature, fusion);
			const int32 k = armature.Extrude(i, tail);
			if (k >= 0 && symetrie && armature.Side((uint32)k) != NkBoneSide::NK_BoneSide_Centre) {
				(void)armature.Symmetrize((uint32)k);
			}
			Touch(NK_RigPart_Armature);
			return k;
		}

		bool NkRigDocument::SubdivideBone(uint32 i, uint32 cuts) {
			if (i >= armature.Count()) {
				return false;
			}
			Begin("Subdiviser un os", NK_RigPart_Armature);
			const NkString miroir = symetrie && MirrorBone(i) >= 0 ? armature.bones[(uint32)MirrorBone(i)].name : NkString();
			if (!armature.Subdivide(i, cuts)) {
				Cancel();
				return false;
			}
			if (!miroir.Empty()) {
				const int32 m = armature.Find(miroir.CStr());
				if (m >= 0) {
					(void)armature.Subdivide((uint32)m, cuts);
				}
			}
			Touch(NK_RigPart_Armature);
			return true;
		}

		bool NkRigDocument::DeleteBone(uint32 i) {
			if (i >= armature.Count()) {
				return false;
			}
			Begin("Supprimer un os", NK_RigPart_Armature | NK_RigPart_Poids | NK_RigPart_Controles);
			const NkString miroir = symetrie && MirrorBone(i) >= 0 ? armature.bones[(uint32)MirrorBone(i)].name : NkString();
			auto Retirer = [&](uint32 b) {
				NkVector<int32> heritier;
				heritier.Resize(armature.Count(), -1);
				for (uint32 k = 0; k < armature.Count(); ++k) {
					heritier[k] = armature.bones[k].parent;
				}
				NkVector<int32> remap;
				armature.Remove(b, &remap);
				weights.RemapBones(remap, heritier);
				// Les controles : indices remappes, un controle qui perd un os disparait.
				for (uint32 c = 0; c < (uint32)controls.Size();) {
					bool perdu = false;
					for (uint32 k = 0; k < (uint32)controls[c].chain.Size(); ++k) {
						const int32 o = controls[c].chain[k];
						const int32 n = (o >= 0 && (uint32)o < (uint32)remap.Size()) ? remap[(uint32)o] : -1;
						perdu = perdu || n < 0;
						controls[c].chain[k] = n;
					}
					if (perdu) {
						controls.RemoveAt(c);
					} else {
						++c;
					}
				}
			};
			Retirer(i);
			if (!miroir.Empty()) {
				const int32 m = armature.Find(miroir.CStr());
				if (m >= 0) {
					Retirer((uint32)m);
				}
			}
			Touch(NK_RigPart_Armature | NK_RigPart_Poids | NK_RigPart_Controles);
			return true;
		}

		void NkRigDocument::MoveHead(uint32 i, const NkVec3f &p, uint32 fusion) {
			if (i >= armature.Count()) {
				return;
			}
			Begin("Deplacer la tete d'un os", NK_RigPart_Armature, fusion);
			armature.SetHead(i, p);
			const int32 m = symetrie ? MirrorBone(i) : -1;
			if (m >= 0) {
				armature.SetHead((uint32)m, MiroirX(p));
			}
			Touch(NK_RigPart_Armature);
		}

		void NkRigDocument::MoveTail(uint32 i, const NkVec3f &p, uint32 fusion) {
			if (i >= armature.Count()) {
				return;
			}
			Begin("Deplacer la queue d'un os", NK_RigPart_Armature, fusion);
			armature.SetTail(i, p);
			const int32 m = symetrie ? MirrorBone(i) : -1;
			if (m >= 0) {
				armature.SetTail((uint32)m, MiroirX(p));
			}
			Touch(NK_RigPart_Armature);
		}

		void NkRigDocument::TranslateBone(uint32 i, const NkVec3f &d, uint32 fusion) {
			if (i >= armature.Count()) {
				return;
			}
			Begin("Deplacer un os", NK_RigPart_Armature, fusion);
			armature.Translate(i, d);
			const int32 m = symetrie ? MirrorBone(i) : -1;
			if (m >= 0) {
				armature.Translate((uint32)m, MiroirX(d));
			}
			Touch(NK_RigPart_Armature);
		}

		void NkRigDocument::SetRoll(uint32 i, float32 r, uint32 fusion) {
			if (i >= armature.Count()) {
				return;
			}
			Begin("Roulis d'un os", NK_RigPart_Armature, fusion);
			armature.SetRoll(i, r);
			const int32 m = symetrie ? MirrorBone(i) : -1;
			if (m >= 0) {
				armature.SetRoll((uint32)m, -r);
			}
			Touch(NK_RigPart_Armature);
		}

		bool NkRigDocument::SetParent(uint32 i, int32 parent, bool connected) {
			if (i >= armature.Count()) {
				return false;
			}
			Begin("Changer de parent", NK_RigPart_Armature);
			if (!armature.SetParent(i, parent, connected)) {
				Cancel();
				return false;
			}
			const int32 m = symetrie ? MirrorBone(i) : -1;
			if (m >= 0) {
				int32 pm = parent;
				if (parent >= 0) {
					const int32 mp = MirrorBone((uint32)parent);
					pm = mp >= 0 ? mp : parent;
				}
				(void)armature.SetParent((uint32)m, pm, connected);
			}
			Touch(NK_RigPart_Armature);
			return true;
		}

		NkString NkRigDocument::RenameBone(uint32 i, const char *name) {
			if (i >= armature.Count() || name == nullptr || name[0] == '\0') {
				return NkString();
			}
			Begin("Renommer un os", NK_RigPart_Armature);
			const NkString r = armature.Rename(i, name);
			Touch(NK_RigPart_Armature);
			return r;
		}

		void NkRigDocument::SetDeform(uint32 i, bool deform) {
			if (i >= armature.Count() || armature.bones[i].deform == deform) {
				return;
			}
			Begin(deform ? "Os deformant" : "Os de controle", NK_RigPart_Armature);
			armature.bones[i].deform = deform;
			Touch(NK_RigPart_Armature);
		}

		int32 NkRigDocument::SymmetrizeBone(uint32 i) {
			if (i >= armature.Count()) {
				return -1;
			}
			Begin("Symetriser un os", NK_RigPart_Armature);
			const uint32 n = armature.Count();
			const int32 m = armature.Symmetrize(i);
			// Ses descendants de cote suivent (un bras entier, pas seulement l'epaule).
			for (uint32 k = 0; k < n; ++k) {
				if (k != i && armature.IsAncestor(i, k) && armature.Side(k) != NkBoneSide::NK_BoneSide_Centre) {
					(void)armature.Symmetrize(k);
				}
			}
			if (m < 0) {
				Cancel();
				return -1;
			}
			Touch(NK_RigPart_Armature);
			return m;
		}

		uint32 NkRigDocument::SymmetrizeAll() {
			Begin("Symetriser l'armature", NK_RigPart_Armature);
			const uint32 n = armature.Count();
			uint32 faits = 0;
			// Dans l'ordre des parents : le miroir d'un enfant trouve celui de son parent.
			NkVector<uint32> ordre;
			armature.Topo(ordre);
			for (uint32 o = 0; o < (uint32)ordre.Size(); ++o) {
				const uint32 k = ordre[o];
				if (k < n && armature.Side(k) == NkBoneSide::NK_BoneSide_Gauche && armature.Symmetrize(k) >= 0) {
					++faits;
				}
			}
			if (faits == 0) {
				Cancel();
				return 0;
			}
			Touch(NK_RigPart_Armature);
			return faits;
		}

		// =====================================================================
		// LE RIG AUTOMATIQUE
		// =====================================================================
		bool NkRigDocument::DetectLandmarks(NkRigTemplate t, NkAutoRigReport *report) {
			Begin("Detecter les reperes", NK_RigPart_Reperes);
			rigTemplate = t;
			NkAutoRigOptions opt;
			const bool ok = NkRigDetectLandmarks(mesh, t, opt, landmarks, report);
			Touch(NK_RigPart_Reperes);
			return ok;
		}

		bool NkRigDocument::MoveLandmark(const char *id, const NkVec3f &p, uint32 fusion) {
			if (NkRigFindLandmark(landmarks, id) < 0) {
				return false;
			}
			Begin("Deplacer un repere", NK_RigPart_Reperes, fusion);
			(void)NkRigMoveLandmark(landmarks, id, p, symetrie);
			Touch(NK_RigPart_Reperes);
			return true;
		}

		bool NkRigDocument::BuildRig(NkRigTemplate t, bool poidsAuto, NkAutoWeightMethod methode, NkAutoWeightReport *report) {
			Begin("Rig automatique", NK_RigPart_Armature | NK_RigPart_Poids | NK_RigPart_Controles | NK_RigPart_Reperes);
			if (landmarks.Empty() || rigTemplate != t) {
				rigTemplate = t;
				NkAutoRigOptions o;
				(void)NkRigDetectLandmarks(mesh, t, o, landmarks, nullptr);
			}
			NkAutoRigOptions opt;
			NkArmature neuve;
			if (!NkRigBuildArmature(t, landmarks, opt, neuve)) {
				Rendre(mUndo[(uint32)mUndo.Size() - 1u]);
				Cancel();
				return false;
			}
			// Des uid NEUFS : les os d'avant ne pretent pas leur identite (ni leurs pistes).
			const uint32 base = armature.nextUid;
			for (uint32 k = 0; k < neuve.Count(); ++k) {
				neuve.bones[k].uid = base + k;
			}
			neuve.nextUid = base + neuve.Count();
			armature = neuve;
			NkRigGenerateControls(t, armature, controls);
			weights.Resize(mesh.VertexCount());
			for (uint32 v = 0; v < weights.VertexCount(); ++v) {
				NkSkinInfluence *s = weights.Of(v);
				for (uint32 k = 0; k < NkSkinWeights::kSlots; ++k) {
					s[k] = NkSkinInfluence{};
				}
			}
			if (poidsAuto) {
				NkAutoWeightOptions wo;
				wo.method = methode;
				(void)NkAutoWeights(mesh, armature, wo, weights, report);
			}
			Touch(NK_RigPart_Armature | NK_RigPart_Poids | NK_RigPart_Controles | NK_RigPart_Reperes);
			return true;
		}

		void NkRigDocument::ImportJoints(const NkMat4f *world, const int32 *parent, const NkString *names, uint32 n) {
			armature = NkArmature::FromJoints(world, parent, names, n);
			Touch(NK_RigPart_Armature);
		}

		uint32 NkRigDocument::GenerateControls(NkRigTemplate t) {
			NkVector<NkRigControl> neufs;
			NkRigGenerateControls(t, armature, neufs);
			if (neufs.Empty()) {
				return 0;
			}
			Begin("Controles du gabarit", NK_RigPart_Controles);
			controls = neufs;
			Touch(NK_RigPart_Controles);
			return (uint32)controls.Size();
		}

		// =====================================================================
		// LA PEAU
		// =====================================================================
		bool NkRigDocument::AutoWeights(NkAutoWeightMethod methode, NkAutoWeightReport *report) {
			Begin("Poids automatiques", NK_RigPart_Poids);
			NkAutoWeightOptions o;
			o.method = methode;
			if (!NkAutoWeights(mesh, armature, o, weights, report)) {
				Cancel();
				return false;
			}
			Touch(NK_RigPart_Poids);
			return true;
		}

		void NkRigDocument::BeginStroke(const char *label) {
			if (mTrait) {
				return;
			}
			Begin(label, NK_RigPart_Poids);
			mTrait = true;
			mTraitChanges = 0;
		}

		uint32 NkRigDocument::Dab(int32 bone, const NkWeightBrush &brush, const uint32 *verts, const float32 *factors, uint32 n) {
			if (!mTrait) {
				BeginStroke();
			}
			NkWeightBrush b = brush;
			b.symmetryX = brush.symmetryX;
			const uint32 c = NkApplyBrush(weights, mesh, armature, bone, b, verts, factors, n);
			mTraitChanges += c;
			if (c > 0) {
				Touch(NK_RigPart_Poids);
			}
			return c;
		}

		void NkRigDocument::EndStroke() {
			if (!mTrait) {
				return;
			}
			mTrait = false;
			if (mTraitChanges == 0) {
				Cancel();
			}
		}

		bool NkRigDocument::SetVertexWeight(uint32 v, int32 bone, float32 w, bool normaliser) {
			if (v >= weights.VertexCount() || bone < 0 || (uint32)bone >= armature.Count()) {
				return false;
			}
			Begin("Poids d'un sommet", NK_RigPart_Poids);
			NkWeightBrush b;
			b.mode = NkBrushMode::NK_BrushMode_Remplacer;
			b.value = w;
			b.strength = 1.f;
			b.autoNormalize = normaliser;
			const float32 un = 1.f;
			(void)NkApplyBrush(weights, mesh, armature, bone, b, &v, &un, 1);
			Touch(NK_RigPart_Poids);
			return true;
		}

		void NkRigDocument::NormalizeWeights() {
			Begin("Normaliser les poids", NK_RigPart_Poids);
			weights.NormalizeAll();
			Touch(NK_RigPart_Poids);
		}

		void NkRigDocument::LimitWeights(uint32 n) {
			Begin("Limiter les influences", NK_RigPart_Poids);
			weights.LimitAll(n);
			Touch(NK_RigPart_Poids);
		}

		uint32 NkRigDocument::PruneWeights(float32 seuil) {
			Begin("Nettoyer les poids", NK_RigPart_Poids);
			const uint32 r = weights.Prune(seuil);
			if (r == 0) {
				Cancel();
				return 0;
			}
			Touch(NK_RigPart_Poids);
			return r;
		}

		uint32 NkRigDocument::MirrorWeights(bool depuisGauche) {
			Begin("Poids en miroir", NK_RigPart_Poids);
			const uint32 r = weights.MirrorX(mesh, armature, depuisGauche);
			if (r == 0) {
				Cancel();
				return 0;
			}
			Touch(NK_RigPart_Poids);
			return r;
		}

		// =====================================================================
		// LES FORMES
		// =====================================================================
		int32 NkRigDocument::AddShape(const char *name) {
			Begin("Nouvelle forme", NK_RigPart_Formes);
			const int32 k = shapes.AddFromBasis(name);
			if (k < 0) {
				Cancel();
				return -1;
			}
			Touch(NK_RigPart_Formes);
			return k;
		}

		int32 NkRigDocument::AddShapeFromPositions(const char *name, const NkVec3f *pos, uint32 n) {
			Begin("Forme depuis la pose", NK_RigPart_Formes);
			const int32 k = shapes.AddFromPositions(name, pos, n);
			if (k < 0) {
				Cancel();
				return -1;
			}
			Touch(NK_RigPart_Formes);
			return k;
		}

		int32 NkRigDocument::AddShapeFromMix(const char *name) {
			Begin("Forme depuis le melange", NK_RigPart_Formes);
			const int32 k = shapes.AddFromMix(name);
			if (k < 0) {
				Cancel();
				return -1;
			}
			Touch(NK_RigPart_Formes);
			return k;
		}

		bool NkRigDocument::RemoveShape(uint32 k) {
			if (k == 0 || k >= shapes.Count()) {
				return false;
			}
			Begin("Supprimer une forme", NK_RigPart_Formes);
			shapes.Remove(k);
			Touch(NK_RigPart_Formes);
			return true;
		}

		NkString NkRigDocument::RenameShape(uint32 k, const char *name) {
			if (k >= shapes.Count() || name == nullptr || name[0] == '\0') {
				return NkString();
			}
			Begin("Renommer une forme", NK_RigPart_Formes);
			const NkString r = shapes.Rename(k, name);
			Touch(NK_RigPart_Formes);
			return r;
		}

		void NkRigDocument::SetShapeValue(uint32 k, float32 v) {
			if (k == 0 || k >= shapes.Count()) {
				return;
			}
			Begin("Valeur d'une forme", NK_RigPart_Formes, 0x10000u + k);
			shapes.SetValue(k, v);
			Touch(NK_RigPart_Formes);
		}

		bool NkRigDocument::SetShapeRelative(uint32 k, int32 r) {
			if (k == 0 || k >= shapes.Count() || r < 0 || (uint32)r >= shapes.Count() || r == (int32)k) {
				return false;
			}
			Begin("Forme relative", NK_RigPart_Formes);
			shapes.keys[k].relativeTo = r;
			Touch(NK_RigPart_Formes);
			return true;
		}

		bool NkRigDocument::SetShapeDriver(uint32 k, const NkShapeDriver &d) {
			if (k == 0 || k >= shapes.Count()) {
				return false;
			}
			Begin("Pilote d'une forme", NK_RigPart_Formes);
			shapes.keys[k].driver = d;
			Touch(NK_RigPart_Formes);
			return true;
		}

		void NkRigDocument::MoveShapeVertices(uint32 k, const uint32 *verts, const float32 *factors, uint32 n, const NkVec3f &delta, uint32 fusion) {
			if (k >= shapes.Count() || n == 0) {
				return;
			}
			Begin("Sculpter une forme", NK_RigPart_Formes, fusion);
			shapes.MoveVertices(k, verts, factors, n, delta, symetrie, &mesh.mirror);
			Touch(NK_RigPart_Formes);
		}

		int32 NkRigDocument::MirrorShape(uint32 k) {
			if (k == 0 || k >= shapes.Count()) {
				return -1;
			}
			Begin("Forme en miroir", NK_RigPart_Formes);
			const int32 j = shapes.Mirror(k, mesh.mirror);
			if (j < 0) {
				Cancel();
				return -1;
			}
			Touch(NK_RigPart_Formes);
			return j;
		}

		const char *const *NkRigDocument::FaceShapeNames(uint32 &n) {
			static const char *const kNoms[] = {"machoire_ouverte", "sourire", "joues_gonflees", "sourcils_leves",
												"clin_oeil.L", "clin_oeil.R", "levres_pincees"};
			n = (uint32)(sizeof(kNoms) / sizeof(kNoms[0]));
			return kNoms;
		}

		namespace {
			/// Une bosse lisse : 1 au centre, 0 a +/- `demi`.
			float32 Bosse(float32 t, float32 c, float32 demi) {
				const float32 u = (t - c) / demi;
				const float32 a = 1.f - u * u;
				return a > 0.f ? a * a : 0.f;
			}
			float32 Rampe(float32 t, float32 a, float32 b) {
				float32 u = (t - a) / (b - a);
				u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
				return u * u * (3.f - 2.f * u);
			}
		} // namespace

		uint32 NkRigDocument::AddFaceShapes(const NkVector<NkString> &noms, NkString *pourquoi) {
			// LA TETE : l'os « head », sinon les reperes menton / sommet.
			NkVec3f base, haut;
			const int32 tete = armature.Find("head");
			if (tete >= 0) {
				base = armature.bones[(uint32)tete].head;
				haut = armature.bones[(uint32)tete].tail;
			} else if (NkRigFindLandmark(landmarks, "menton") >= 0 && NkRigFindLandmark(landmarks, "sommet_tete") >= 0) {
				base = landmarks[(uint32)NkRigFindLandmark(landmarks, "menton")].position;
				haut = landmarks[(uint32)NkRigFindLandmark(landmarks, "sommet_tete")].position;
			} else {
				if (pourquoi != nullptr) {
					*pourquoi = NkString("aucune tete reperee : il faut un os « head » ou les reperes « menton » et « sommet_tete »");
				}
				return 0;
			}
			const float32 H = Dist(base, haut);
			if (!(H > 1e-5f) || shapes.VertexCount() == 0) {
				if (pourquoi != nullptr) {
					*pourquoi = NkString("tete degeneree ou maillage vide");
				}
				return 0;
			}
			const NkVec3f c = Lerp(base, haut, 0.42f);
			NkVector<uint32> zone;
			// Une COPIE : les formes ajoutees plus bas reallouent le tableau des formes.
			const NkVector<NkVec3f> b0 = shapes.keys[0].positions;
			for (uint32 v = 0; v < (uint32)b0.Size(); ++v) {
				if (Dist(b0[v], c) < H * 0.75f && b0[v].y > base.y - 0.08f * H) {
					zone.PushBack(v);
				}
			}
			if (zone.Empty()) {
				if (pourquoi != nullptr) {
					*pourquoi = NkString("aucun sommet dans la tete");
				}
				return 0;
			}
			Begin("Formes de visage", NK_RigPart_Formes);
			uint32 creees = 0;
			for (uint32 n = 0; n < (uint32)noms.Size(); ++n) {
				const NkString &nom = noms[n];
				uint32 nb = 0;
				const char *const *kNoms = FaceShapeNames(nb);
				bool connu = false;
				for (uint32 q = 0; q < nb; ++q) {
					connu = connu || nom == NkString(kNoms[q]);
				}
				if (!connu) {
					continue;
				}
				const int32 k = shapes.AddFromBasis(nom.CStr());
				if (k < 0) {
					continue;
				}
				NkVector<NkVec3f> &p = shapes.keys[(uint32)k].positions;
				for (uint32 z = 0; z < (uint32)zone.Size(); ++z) {
					const uint32 v = zone[z];
					const NkVec3f u = Mul(Sub(b0[v], c), 1.f / H);
					const float32 devant = Rampe(u.z, -0.05f, 0.25f);
					const float32 sx = u.x >= 0.f ? 1.f : -1.f;
					NkVec3f d = V(0.f, 0.f, 0.f);
					if (nom == NkString("machoire_ouverte")) {
						const float32 w = Rampe(-u.y, 0.02f, 0.3f) * Bosse(u.x, 0.f, 0.5f) * (0.35f + 0.65f * devant);
						d = V(0.f, -0.12f * w, -0.03f * w);
					} else if (nom == NkString("sourire")) {
						const float32 w = Bosse(u.y, -0.2f, 0.14f) * Bosse(std::fabs(u.x), 0.22f, 0.16f) * devant;
						d = V(sx * 0.02f * w, 0.035f * w, -0.015f * w);
					} else if (nom == NkString("joues_gonflees")) {
						const float32 w = Bosse(u.y, -0.08f, 0.2f) * Bosse(std::fabs(u.x), 0.32f, 0.2f) * Rampe(u.z, -0.25f, 0.1f);
						d = V(sx * 0.045f * w, 0.f, 0.015f * w);
					} else if (nom == NkString("sourcils_leves")) {
						const float32 w = Bosse(u.y, 0.2f, 0.12f) * Bosse(u.x, 0.f, 0.38f) * devant;
						d = V(0.f, 0.04f * w, 0.005f * w);
					} else if (nom == NkString("clin_oeil.L") || nom == NkString("clin_oeil.R")) {
						const float32 cote = nom == NkString("clin_oeil.L") ? 1.f : -1.f;
						const float32 w = Bosse(u.y, 0.08f, 0.1f) * Bosse(u.x * cote, 0.18f, 0.13f) * devant;
						d = V(0.f, -0.03f * w, -0.02f * w);
					} else if (nom == NkString("levres_pincees")) {
						const float32 w = Bosse(u.y, -0.2f, 0.1f) * Bosse(u.x, 0.f, 0.18f) * devant;
						d = V(-u.x * 0.25f * w, -(u.y + 0.2f) * 0.3f * w, -0.025f * w);
					}
					p[v] = Add(p[v], Mul(d, H));
				}
				++creees;
			}
			if (creees == 0) {
				Cancel();
				if (pourquoi != nullptr) {
					*pourquoi = NkString("aucun nom de forme reconnu");
				}
				return 0;
			}
			Touch(NK_RigPart_Formes);
			return creees;
		}

	} // namespace anim
} // namespace nkentseu
