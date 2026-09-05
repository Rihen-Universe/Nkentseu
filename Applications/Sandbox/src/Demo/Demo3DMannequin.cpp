// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// Demo3DMannequin.cpp — sonde « vêtements sur un mannequin en mouvement » (en-tête).
#include "Demo3DMannequin.h"
#include "NKRenderer/Mesh/NkFBXLoader.h"
#include "NKTime/NkChrono.h"
#include "NKMath/NkFunctions.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {

	using namespace renderer;
	using namespace physics;
	using nkentseu::math::NkMat4f;
	using nkentseu::math::NkVec3f;

	namespace {

		struct GarmentSlot {
				NkGarment garment;
				NkMeshHandle mesh, meshFlip;
				NkVector<NkVertex3D> verts;
				NkVector<uint32> idx, idxFlip;
				NkVector<NkVec3f> normals;
				NkVec3f tint{1.f, 1.f, 1.f};
				// maximums sur la course (à partir de l'image 60 : le T-pose -> marche est un transitoire)
				float32 penMax = 0.f, pinMax = 0.f, stretchMax = 0.f, msMax = 0.f;
				uint32 insideMax = 0, insideSum = 0, degenerate = 0;
				uint32 insideNearestMax = 0, insideNearestSum = 0;
				float32 insideDepthMax = 0.f;
				NkVector<NkVec3f> freePos; // positions des particules LIBRES (les epinglees sont hors mesure)
				NkBodySDF sdf, sdfPrev;	  // champ PROPRE au vêtement (boîte de ses particules)
				float64 msSdfSum = 0.0;
				float32 msSdfMax = 0.f, cellSize = 0.f;
				uint32 sdfCells = 0, sdfSkipped = 0;
				float64 stretchSum = 0.0;
				uint32 stretchOver5 = 0;
				float32 sdfPenMax = 0.f;
				uint32 sdfContactsMax = 0, capsContactsMax = 0;
				float64 msSum = 0.0;
				uint32 frames = 0;
		};

		NkVec3f Tint(NkGarmentKind k) {
			switch (k) {
				case NK_GARMENT_CAPE: return {0.97f, 0.60f, 0.16f};	 // orange Rihen
				case NK_GARMENT_FOULARD: return {0.85f, 0.18f, 0.18f};
				case NK_GARMENT_JUPE: return {0.90f, 0.90f, 0.95f};
				case NK_GARMENT_TSHIRT: return {0.95f, 0.95f, 0.95f};
				case NK_GARMENT_CHEMISE: return {0.60f, 0.75f, 0.95f};
				case NK_GARMENT_ROBE: return {0.55f, 0.20f, 0.50f};
				case NK_GARMENT_PANTALON: return {0.20f, 0.25f, 0.40f};
				default: return {1.f, 1.f, 1.f};
			}
		}

		bool EndsWithNoCase(const char *s, const char *suffix) {
			const size_t n = std::strlen(s), m = std::strlen(suffix);
			if (m > n)
				return false;
			for (size_t i = 0; i < m; ++i) {
				char a = s[n - m + i], b = suffix[i];
				if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
				if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
				if (a != b)
					return false;
			}
			return true;
		}

		bool LoadModel(const char *path, NkGLTFMeshData &out) {
			if (EndsWithNoCase(path, ".fbx"))
				return LoadFBX(NkString(path), out);
			return LoadGLTF(NkString(path), out);
		}

		NkVec3f Pos(const NkMat4f &m) {
			return m.TransformPoint(NkVec3f{0.f, 0.f, 0.f});
		}

		// Direction transformee SANS la translation. ⚠ `NkMat4f::TransformVector` NE COMPILE PAS :
		// elle est declaree rendant un NkVec3T et son corps rend `(*this) * NkVec4T(v, 0)`, un
		// NkVec4T -- personne ne l'avait instanciee (NkGLTFLoader.cpp:179 note le meme defaut et
		// le contourne aussi). Trou prealable, nomme pour l'agent NKMath, non corrige ici.
		NkVec3f Dir(const NkMat4f &m, const NkVec3f &v) {
			return m.TransformPoint(v) - m.TransformPoint(NkVec3f{0.f, 0.f, 0.f});
		}

	} // namespace

	struct Demo3DMannequinProbe {
			NkGLTFMeshData model, animFile;
			bool animFromFile = false;
			NkVector<NkMat4f> bind, clipWorld, world, palette;
			NkVector<int32> parent;
			NkVector<NkString> names;
			NkVector<const char *> namePtrs;
			NkSkeletonBind skel;
			NkHumanoidMap map;
			NkMannequin mannequin;
			NkBodyMeasures measures;
			NkVector<NkSkinSample> samples;
			NkVector<NkVertex3D> bodyVerts;
			NkVector<NkVec3f> bodyPos;
			NkMeshHandle bodyMesh;
			NkMeshInsideTester inside, restInside;
			NkBodySDF sdf, sdfPrev;	  // le corps vu comme un champ de distance : pose de fin, pose de début
			bool useSdf = true;
			bool sdfPerGarment = false; // NK_MANNEQUIN_SDF_BOX=1 : un champ par vêtement, sur SA boîte
			uint32 sdfEvery = 1;	  // reconstruction toutes les N images (mesure du compromis)
			float64 msSdfSum = 0.0;
			float32 msSdfMax = 0.f;
			float32 sdfCalib = 0.f;
			NkVector<NkVec3f> bodyPosRest; // la peau au repos (l'ajustement des patrons la lit)
			NkVector<GarmentSlot *> garments;
			NkHat hat;
			bool hasHat = false;
			NkVector<collision::NkShape> shapes;
			int32 anim = -1;
			float32 duration = 0.f, time = 0.f, speed = 1.2f, turnAt = 0.f, yaw = 0.f;
			NkVec3f pos{0.f, 0.f, 0.f}, forward0{0.f, 0.f, 1.f};
			bool rootMotion = false;
			NkVec3f rootRef{0.f, 0.f, 0.f};
			// marche SYNTHETISEE : locaux de repos + rotations aux hanches / genoux / epaules.
			// Mesure du 05/09 : aucun clip de MARCHE n'est lisible ici -- XBot.glb et XBot.fbx ne
			// portent qu'un clip de 0,03-0,07 s (T-pose), et le chargeur FBX rend 0 animation sur
			// les Walking.fbx de Mixamo (65 nodes lus, aucune courbe). Chantier NKRenderer nomme.
			NkVector<NkMat4f> localBind;
			bool synthWalk = false;
			float32 walkAmp = 25.f, walkFreq = 1.f;
			float64 msSkinSum = 0.0, msInsideSum = 0.0, msSimSum = 0.0;
			bool testInside = true;
			float32 msSimMax = 0.f;
			uint64 frames = 0;
			bool flip = false;
	};

	// ── Chargement, mesures, construction ─────────────────────────────────
	Demo3DMannequinProbe *Demo3DMannequinInit(NkMeshSystem *meshSys) {
		const char *on = std::getenv("NK_MANNEQUIN_PROBE");
		if (!on || on[0] != '1')
			return nullptr;
		auto *p = new Demo3DMannequinProbe();
		const char *modelPath = std::getenv("NK_MANNEQUIN_MODEL");
		if (!modelPath || !modelPath[0])
			modelPath = "Resources/Models/CesiumMan/CesiumMan.glb";
		NkChrono loadClock;
		if (!LoadModel(modelPath, p->model) || !p->model.isSkinned || p->model.skinJoints.Empty()) {
			std::fprintf(stderr, "[MANNEQUIN PROBE] ECHEC : %s -- charge=%d skinne=%d joints=%u sommets=%u animations=%u (mesure : ce chemin ne donne pas un corps skinne)\n",
						 modelPath, (int)p->model.IsValid(), (int)p->model.isSkinned, (uint32)p->model.skinJoints.Size(),
						 (uint32)p->model.vertices.Size(), (uint32)p->model.animations.Size());
			delete p;
			return nullptr;
		}
		const float64 msLoad = loadClock.Elapsed().milliseconds;
		const uint32 nj = (uint32)p->model.skinJoints.Size();
		std::fprintf(stderr, "[MANNEQUIN PROBE] modele %s : %u sommets, %u triangles, %u joints, %u animation(s), charge en %.0f ms\n",
					 modelPath, (uint32)p->model.vertices.Size(), (uint32)p->model.indices.Size() / 3u, nj,
					 (uint32)p->model.animations.Size(), msLoad);
		for (uint32 a = 0; a < (uint32)p->model.animations.Size(); ++a)
			std::fprintf(stderr, "[MANNEQUIN PROBE]   animation %u : '%s', %.2f s, %u canaux\n", a, p->model.animations[a].name.CStr(),
						 p->model.animations[a].duration, (uint32)p->model.animations[a].channels.Size());
		// fichier d'animation à part (Mixamo « sans peau ») : MESURÉ, pas encore consommé
		if (const char *ap = std::getenv("NK_MANNEQUIN_ANIM"); ap && ap[0]) {
			const bool ok = LoadModel(ap, p->animFile);
			std::fprintf(stderr, "[MANNEQUIN PROBE] fichier d'animation %s : charge=%d skinne=%d joints=%u nodes=%u sommets=%u animations=%u",
						 ap, (int)ok, (int)p->animFile.isSkinned, (uint32)p->animFile.skinJoints.Size(), (uint32)p->animFile.nodes.Size(),
						 (uint32)p->animFile.vertices.Size(), (uint32)p->animFile.animations.Size());
			for (uint32 a = 0; a < (uint32)p->animFile.animations.Size(); ++a)
				std::fprintf(stderr, " | anim %u '%s' %.2f s %u canaux", a, p->animFile.animations[a].name.CStr(),
							 p->animFile.animations[a].duration, (uint32)p->animFile.animations[a].channels.Size());
			std::fprintf(stderr, "\n");
			if (ok && p->animFile.isSkinned && !p->animFile.animations.Empty() && p->model.animations.Empty()) {
				// le fichier d'animation porte AUSSI la peau (export Mixamo « with skin ») : il devient le corps
				std::fprintf(stderr, "[MANNEQUIN PROBE]   -> il porte la peau et un clip : c'est lui qui sert de corps anime\n");
				p->animFromFile = true;
			} else if (ok && !p->animFile.isSkinned) {
				std::fprintf(stderr, "[MANNEQUIN PROBE]   -> clip SANS peau : aucun chemin d'evaluation ici (EvaluateGLTFWorldJoints exige isSkinned) ; reciblage NkAnima par noms = chantier nomme, non fait\n");
			}
		}
		NkGLTFMeshData &M = p->animFromFile ? p->animFile : p->model;
		const uint32 njm = (uint32)M.skinJoints.Size();
		// pose de repos
		EvaluateGLTFWorldJoints(M, -1, 0.f, p->bind, p->parent);
		p->names.Resize(njm);
		p->namePtrs.Resize(njm);
		for (uint32 j = 0; j < njm; ++j) {
			const int32 ni = M.skinJoints[j];
			p->names[j] = (ni >= 0 && ni < (int32)M.nodes.Size()) ? M.nodes[(uint32)ni].name : NkString("");
			p->namePtrs[j] = p->names[j].CStr();
		}
		p->skel.parent = p->parent.Data();
		p->skel.world = p->bind.Data();
		p->skel.names = p->namePtrs.Data();
		p->skel.count = njm;
		// la peau au repos : échantillons (position skinnée à la pose de repos, os, poids)
		p->palette.Resize(njm);
		for (uint32 j = 0; j < njm; ++j)
			p->palette[j] = p->bind[j] * M.inverseBind[j];
		const uint32 nv = (uint32)M.skinnedVertices.Size();
		p->samples.Resize(nv);
		for (uint32 v = 0; v < nv; ++v) {
			const NkVertexSkinned &sv = M.skinnedVertices[v];
			NkSkinSample &s = p->samples[v];
			NkVec3f acc{0.f, 0.f, 0.f};
			for (uint32 k = 0; k < 4; ++k) {
				const int32 b = (int32)(sv.boneIdx[k] + 0.5f);
				s.bone[k] = (b >= 0 && (uint32)b < njm) ? b : -1;
				s.w[k] = sv.boneWeight[k];
				if (s.bone[k] >= 0 && s.w[k] > 0.f)
					acc += p->palette[(uint32)s.bone[k]].TransformPoint(sv.pos) * s.w[k];
			}
			s.pos = acc;
		}
		// ── TÉMOIN DU SKINNING (2026-09-05, Rodolf : « les vertices sont saccadés ») ──────────
		// Quatre mesures, avant tout vêtement : (1) la pose de repos skinnée doit rendre le
		// maillage BRUT (si elle ne le rend pas, la faute est dans la LIAISON : indices de
		// joints, matrices inverses, poids) ; (2) somme des poids ; (3) bornes des indices ;
		// (4) ordre topologique du tableau de parents (un enfant avant son parent lit une
		// matrice périmée : membres en épines et saccades d'une image à l'autre).
		{
			float32 restMax = 0.f, wMin = 1e30f, wMax = -1e30f;
			uint32 badIdx = 0, zeroW = 0, worstV = 0;
			for (uint32 v = 0; v < nv; ++v) {
				const NkVertexSkinned &sv = M.skinnedVertices[v];
				float32 wsum = 0.f;
				for (uint32 k = 0; k < 4; ++k) {
					wsum += sv.boneWeight[k];
					const int32 b = (int32)(sv.boneIdx[k] + 0.5f);
					if (sv.boneWeight[k] > 0.f && (b < 0 || (uint32)b >= njm))
						++badIdx;
				}
				if (wsum < wMin) wMin = wsum;
				if (wsum > wMax) wMax = wsum;
				if (wsum <= 1e-6f) ++zeroW;
				const float32 d = (p->samples[v].pos - M.vertices[v].pos).Len();
				if (d > restMax) { restMax = d; worstV = v; }
			}
			uint32 topoBad = 0;
			for (uint32 j = 0; j < njm; ++j)
				if (p->parent[j] >= 0 && (uint32)p->parent[j] >= j)
					++topoBad;
			std::fprintf(stderr, "[SKINNING TEMOIN] (1) pose de repos contre maillage brut : ecart max %.4f m (sommet %u) -- attendu < 1e-4 | (2) somme des poids [%.4f, %.4f], %u sommets a poids nul | (3) %u indices d'os hors bornes (0..%u) | (4) %u os declares AVANT leur parent (ordre topologique %s)\n",
						 restMax, worstV, wMin, wMax, zeroW, badIdx, njm - 1, topoBad, topoBad ? "ROUGE" : "vert");
			if (restMax > 1e-4f) {
				const NkVertexSkinned &sv = M.skinnedVertices[worstV];
				std::fprintf(stderr, "[SKINNING TEMOIN]   sommet %u : brut (%.3f %.3f %.3f) -> repos skinne (%.3f %.3f %.3f) | os %d/%d/%d/%d poids %.3f/%.3f/%.3f/%.3f\n",
							 worstV, M.vertices[worstV].pos.x, M.vertices[worstV].pos.y, M.vertices[worstV].pos.z,
							 p->samples[worstV].pos.x, p->samples[worstV].pos.y, p->samples[worstV].pos.z,
							 (int)(sv.boneIdx[0] + 0.5f), (int)(sv.boneIdx[1] + 0.5f), (int)(sv.boneIdx[2] + 0.5f),
							 (int)(sv.boneIdx[3] + 0.5f), sv.boneWeight[0], sv.boneWeight[1], sv.boneWeight[2], sv.boneWeight[3]);
			}
		}
		// rôles, capsules, mesures
		const bool resolved = p->map.Resolve(p->skel);
		if (const char *q = std::getenv("NK_MANNEQUIN_QUANTILE"); q && q[0])
			p->mannequin.params.quantile = (float32)std::atof(q);
		p->mannequin.Build(p->skel, p->samples.Data(), nv, &p->map);
		uint32 estimated = 0;
		NkMeasureBody(p->mannequin, p->map, p->skel, p->measures, &estimated);
		std::fprintf(stderr, "[MANNEQUIN PROBE] roles : %u / %u trouves (resolu=%d) --", p->map.Found(), (uint32)NK_HB_COUNT, (int)resolved);
		for (uint32 r = 0; r < NK_HB_COUNT; ++r)
			if (p->map.joint[r] >= 0)
				std::fprintf(stderr, " %s=%s", NkHumanoidMap::RoleName((NkHumanBone)r), p->names[(uint32)p->map.joint[r]].CStr());
		std::fprintf(stderr, "\n");
		const NkBodyMeasures &m = p->measures;
		std::fprintf(stderr, "[MANNEQUIN PROBE] capsules %u (quantile %.2f) | mesures : taille %.2f m, epaules %.2f m, hanches %.2f m, jambe %.2f m, bras %.2f m | rayons (mm) taille %.0f poitrine %.0f cou %.0f tete %.0f bassin %.0f cuisse %.0f mollet %.0f bras %.0f avant-bras %.0f (%u estimes) | avant (%.2f %.2f %.2f)\n",
					 p->mannequin.CapsuleCount(), p->mannequin.params.quantile, m.height, m.shoulderWidth, m.hipWidth, m.legLength, m.armLength,
					 1000.f * m.waistRadius, 1000.f * m.chestRadius, 1000.f * m.neckRadius, 1000.f * m.headRadius, 1000.f * m.pelvisRadius,
					 1000.f * m.thighRadius, 1000.f * m.calfRadius, 1000.f * m.upperArmRadius, 1000.f * m.forearmRadius, estimated,
					 m.forward.x, m.forward.y, m.forward.z);
		for (uint32 c = 0; c < p->mannequin.CapsuleCount(); ++c) {
			const NkMannequinCapsule &cap = p->mannequin.Capsule(c);
			std::fprintf(stderr, "[MANNEQUIN PROBE]   capsule %2u : %s -> %s rayon %.0f mm (%u sommets)\n", c,
						 p->names[(uint32)cap.owner].CStr(), cap.joint >= 0 ? p->names[(uint32)cap.joint].CStr() : "?", 1000.f * cap.radius,
						 cap.samples);
		}
		if (!m.valid) {
			std::fprintf(stderr, "[MANNEQUIN PROBE] ECHEC : mesures impossibles (roles manquants) -- pas de vetement\n");
		}
		// clip et mouvement de racine
		p->anim = M.animations.Empty() ? -1 : 0;
		p->duration = p->anim >= 0 ? M.animations[0].duration : 0.f;
		if (p->anim >= 0 && p->map.joint[NK_HB_HIPS] >= 0) {
			NkVector<NkMat4f> w;
			NkVector<int32> pr;
			const uint32 hips = (uint32)p->map.joint[NK_HB_HIPS];
			EvaluateGLTFWorldJoints(M, p->anim, 0.f, w, pr);
			const NkVec3f h0 = Pos(w[hips]);
			float32 maxD = 0.f;
			for (uint32 k = 1; k <= 8; ++k) {
				EvaluateGLTFWorldJoints(M, p->anim, p->duration * 0.99f * (float32)k / 8.f, w, pr);
				const NkVec3f d = Pos(w[hips]) - h0;
				const float32 dh = NkVec3f{d.x, 0.f, d.z}.Len();
				if (dh > maxD)
					maxD = dh;
			}
			p->rootMotion = maxD > 0.05f;
			p->rootRef = h0;
			std::fprintf(stderr, "[MANNEQUIN PROBE] clip '%s' %.2f s : deplacement horizontal max des hanches sur le clip %.3f m -> %s\n",
						 M.animations[0].name.CStr(), p->duration, maxD,
						 p->rootMotion ? "MOUVEMENT DE RACINE, verrouille (in place impose par la sonde)" : "in place (rien a verrouiller)");
		}
		// locaux de repos (pour la marche synthetisee) : local = inverse(monde du parent) x monde
		p->localBind.Resize(njm);
		for (uint32 j = 0; j < njm; ++j) {
			const int32 par = p->parent[j];
			p->localBind[j] = (par >= 0 && (uint32)par < njm) ? p->bind[(uint32)par].Inverse() * p->bind[j] : p->bind[j];
		}
		// marche synthetisee si le clip ne marche pas (duree < 0,5 s) ; NK_MANNEQUIN_WALK=0/1 force
		p->synthWalk = p->duration < 0.5f;
		if (const char *w = std::getenv("NK_MANNEQUIN_WALK"); w && w[0])
			p->synthWalk = w[0] != '0';
		if (const char *a = std::getenv("NK_MANNEQUIN_AMP"); a && a[0])
			p->walkAmp = (float32)std::atof(a);
		if (const char *i = std::getenv("NK_MANNEQUIN_INSIDE"); i && i[0] == '0')
			p->testInside = false;
		std::fprintf(stderr, "[MANNEQUIN PROBE] marche : %s (clip %.2f s, amplitude %.0f deg, %.1f pas/s)\n",
					 p->synthWalk ? "SYNTHETISEE par la sonde (aucun clip de marche lisible : XBot.glb 0,07 s, XBot.fbx 0,03 s, Walking.fbx 0 animation)"
								  : "le clip du modele",
					 p->duration, p->walkAmp, p->walkFreq);
		if (const char *s = std::getenv("NK_MANNEQUIN_SPEED"); s && s[0])
			p->speed = (float32)std::atof(s);
		if (const char *s = std::getenv("NK_MANNEQUIN_TURN"); s && s[0])
			p->turnAt = (float32)std::atof(s);
		if (const char *ps = std::getenv("NK_MANNEQUIN_POS"); ps && ps[0]) {
			float32 tv[3] = {0.f, 0.f, 0.f};
			int32 k = 0;
			const char *c = ps;
			while (k < 3 && *c) {
				tv[k++] = (float32)std::atof(c);
				while (*c && *c != ',')
					++c;
				if (*c == ',')
					++c;
			}
			p->pos = {tv[0], tv[1], tv[2]};
		}
		p->forward0 = m.valid ? m.forward : NkVec3f{0.f, 0.f, 1.f};
		if (const char *f = std::getenv("NK_MANNEQUIN_FLIP"); f && f[0] == '1')
			p->flip = true;
		// le corps : maillage dynamique (peau CPU), dessiné ET testé
		p->bodyVerts.Resize(nv);
		p->bodyPos.Resize(nv);
		p->bodyPosRest.Resize(nv);
		for (uint32 v = 0; v < nv; ++v) {
			p->bodyVerts[v] = M.vertices[v];
			p->bodyVerts[v].pos = p->samples[v].pos;
			p->bodyPos[v] = p->samples[v].pos;
			p->bodyPosRest[v] = p->samples[v].pos;
		}
		// le corps AU REPOS : c'est contre CE maillage que les patrons sont ajustés
		p->restInside.Build(p->bodyPosRest.Data(), nv, M.indices.Data(), (uint32)M.indices.Size() / 3u);
		{
			NkMeshDesc md = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), p->bodyVerts.Data(), nv, M.indices.Data(),
											   (uint32)M.indices.Size());
			md.dynamic = true;
			md.debugName = "Demo3D_Mannequin";
			p->bodyMesh = meshSys->Create(md);
		}
		// ── LE CHAMP DE DISTANCE, construit puis CALIBRÉ avant de servir à juger ────────────
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF"); e && e[0] == '0')
			p->useSdf = false;
		// NK_MANNEQUIN_SDF_SIGN=w : signe par NOMBRE D ENROULEMENT (corps non etanches, Jacobson 2013)
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_SIGN"); e && (e[0] == 'w' || e[0] == 'W'))
			p->sdf.params.sign = NkSDFSign::NK_WINDING;
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_SIGNRES"); e && e[0])
			p->sdf.params.signResolution = (uint32)std::atoi(e);
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_BOX"); e && e[0] == '1')
			p->sdfPerGarment = true;
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_CELL"); e && e[0])
			p->sdf.params.targetCellSize = (float32)std::atof(e) * 0.001f; // en mm
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_MAXCELLS"); e && e[0])
			p->sdf.params.maxCells = (uint32)std::atoi(e);
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_RES"); e && e[0])
			p->sdf.params.resolution = (uint32)std::atoi(e);
		if (const char *e = std::getenv("NK_MANNEQUIN_SDF_EVERY"); e && e[0])
			p->sdfEvery = (uint32)math::NkMax(1, std::atoi(e));
		if (p->useSdf) {
			NkChrono sc;
			p->sdf.Build(p->bodyPosRest.Data(), nv, M.indices.Data(), (uint32)M.indices.Size() / 3u);
			const float64 msB = sc.Elapsed().milliseconds;
			uint32 okA = 0, totA = 0, neg = 0, totNeg = 0;
			bool centerIn = false;
			p->sdfCalib = p->sdf.Calibrate(p->bodyPosRest.Data(), M.indices.Data(), (uint32)M.indices.Size() / 3u, 0.01f,
										   &okA, &totA, &neg, &totNeg, &centerIn);
			const NkBodySDFStats &st = p->sdf.Stats();
			std::fprintf(stderr, "[SDF] grille %ux%ux%u = %u cellules de %.1f mm (bande %u), construit en %.1f ms | valeurs [%.3f ; %.3f] m, %u cellules dedans (%.1f %%)\n",
						 st.nx, st.ny, st.nz, st.cells, 1000.f * st.cellSize, st.bandCells, msB, st.minValue, st.maxValue,
						 st.negativeCells, 100.f * (float32)st.negativeCells / (float32)st.cells);
			std::fprintf(stderr, "[SDF CALIBRATION] positif : %u / %u centres de triangles rentres de 1 cm dits DEDANS (%.1f %%, 100 attendu) | negatif : %u / %u points lointains dits dedans (0 attendu) | centre du corps dedans : %s\n",
						 okA, totA, 100.f * p->sdfCalib, neg, totNeg, centerIn ? "OUI" : "NON (ROUGE)");
			// MUTATION : signe inversé -> la calibration doit rougir (un champ qui ne sait pas
			// rougir ne prouve rien). Restauré par une reconstruction, pas par une négation.
			{
				NkBodySDF mut;
				mut.params = p->sdf.params;
				mut.Build(p->bodyPosRest.Data(), nv, M.indices.Data(), (uint32)M.indices.Size() / 3u);
				mut.MutateFlipSign();
				uint32 mOk = 0, mTot = 0;
				const float32 taux = mut.Calibrate(p->bodyPosRest.Data(), M.indices.Data(), (uint32)M.indices.Size() / 3u,
												   0.01f, &mOk, &mTot, nullptr, nullptr, nullptr);
				std::fprintf(stderr, "[SDF MUTATION] signe inverse : le positif tombe a %.1f %% (%u / %u) -- le temoin sait rougir\n",
							 100.f * taux, mOk, mTot);
			}
		}
		// ── CONTRÔLE du test point-dans-maillage, sur CE corps (avant tout verdict) ─────────
		// Un test qui ment sur le corps rendrait faux tout ce qui suit. Deux épreuves :
		// NÉGATIVE (des points manifestement DEHORS, sur une sphère de 3 m) -> 0 attendu ;
		// POSITIVE (le centre de chaque triangle rentré de 1 cm sous la surface) -> tout attendu.
		{
			NkVector<NkVec3f> outPts, inPts;
			NkVec3f mn = p->bodyPosRest[0], mx = p->bodyPosRest[0];
			for (uint32 v = 1; v < nv; ++v) {
				const NkVec3f &q = p->bodyPosRest[v];
				mn.x = math::NkMin(mn.x, q.x); mn.y = math::NkMin(mn.y, q.y); mn.z = math::NkMin(mn.z, q.z);
				mx.x = math::NkMax(mx.x, q.x); mx.y = math::NkMax(mx.y, q.y); mx.z = math::NkMax(mx.z, q.z);
			}
			const NkVec3f c = (mn + mx) * 0.5f;
			for (uint32 k = 0; k < 512; ++k) {
				const float32 a = 6.2831853f * (float32)k / 512.f, e = 3.14159265f * (float32)(k % 17) / 17.f;
				outPts.PushBack(c + NkVec3f{3.f * math::NkSin(e) * math::NkCos(a), 3.f * math::NkCos(e), 3.f * math::NkSin(e) * math::NkSin(a)});
			}
			const uint32 nt = (uint32)M.indices.Size() / 3u;
			for (uint32 t = 0; t < nt; t += 37) {
				const NkVec3f &a = p->bodyPosRest[M.indices[t * 3]], &b = p->bodyPosRest[M.indices[t * 3 + 1]],
							  &d = p->bodyPosRest[M.indices[t * 3 + 2]];
				NkVec3f n = (b - a).Cross(d - a);
				const float32 l = n.Len();
				if (l < 1e-9f)
					continue;
				n = n * (1.f / l);
				inPts.PushBack((a + b + d) * (1.f / 3.f) - n * 0.01f); // 1 cm sous la face
			}
			const uint32 faux = p->restInside.CountInside(outPts.Data(), (uint32)outPts.Size(), nullptr);
			const uint32 vrai = p->restInside.CountInside(inPts.Data(), (uint32)inPts.Size(), nullptr);
			const uint32 fauxN = p->restInside.CountInsideNearest(outPts.Data(), (uint32)outPts.Size(), nullptr);
			const uint32 vraiN = p->restInside.CountInsideNearest(inPts.Data(), (uint32)inPts.Size(), nullptr);
			std::fprintf(stderr, "[DEDANS CONTROLE] PARITE  -- negatif : %u / %u points a 3 m dits DEDANS (0 attendu) | positif : %u / %u centres de triangles rentres de 1 cm (%.1f %%, 100 attendu si la surface est FERMEE)\n",
						 faux, (uint32)outPts.Size(), vrai, (uint32)inPts.Size(), 100.f * (float32)vrai / (float32)inPts.Size());
			// LE SENS DES FACES : la normale geometrique (b-a)x(c-a) contre la normale du FICHIER.
			// Si une part des triangles est retournee, aucun test de signe ne peut marcher.
			{
				uint32 nt2 = (uint32)M.indices.Size() / 3u, opposees = 0, comptees = 0;
				for (uint32 t = 0; t < nt2; ++t) {
					const uint32 ia = M.indices[t * 3], ib = M.indices[t * 3 + 1], ic = M.indices[t * 3 + 2];
					NkVec3f n = (p->bodyPosRest[ib] - p->bodyPosRest[ia]).Cross(p->bodyPosRest[ic] - p->bodyPosRest[ia]);
					const float32 l = n.Len();
					if (l < 1e-12f)
						continue;
					n = n * (1.f / l);
					const NkVec3f nf = M.vertices[ia].normal;
					if (nf.Len() < 0.5f)
						continue;
					++comptees;
					if (n.Dot(nf) < 0.f)
						++opposees;
				}
				std::fprintf(stderr, "[DEDANS CONTROLE] SENS DES FACES : %u / %u triangles dont la normale geometrique est OPPOSEE a la normale du fichier (%.1f %%)\n",
							 opposees, comptees, 100.f * (float32)opposees / (float32)(comptees ? comptees : 1u));
			}
			std::fprintf(stderr, "[DEDANS CONTROLE] PLUS PROCHE TRIANGLE -- negatif : %u / %u (0 attendu) | positif : %u / %u (%.1f %%, 100 attendu ; ne suppose AUCUNE surface fermee)\n",
						 fauxN, (uint32)outPts.Size(), vraiN, (uint32)inPts.Size(), 100.f * (float32)vraiN / (float32)inPts.Size());
		}
		// les vêtements
		NkGarmentParams gp;
		if (const char *e = std::getenv("NK_GARMENT_IT"); e && e[0])
			gp.iterations = (uint32)std::atoi(e);
		if (const char *e = std::getenv("NK_GARMENT_SUB"); e && e[0])
			gp.substeps = (uint32)std::atoi(e);
		gp.substeps = gp.substeps < 1 ? 1 : gp.substeps;
		const char *list = std::getenv("NK_GARMENTS");
		if (!list || !list[0])
			list = "cape,jupe,foulard,chapeau";
		char buf[256];
		std::strncpy(buf, list, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		char *tok = std::strtok(buf, ",; ");
		while (tok && m.valid) {
			NkGarmentKind kind;
			if (std::strcmp(tok, "chapeau") == 0) {
				p->hasHat = p->hat.Build(m, p->map, p->skel);
				std::fprintf(stderr, "[MANNEQUIN PROBE] chapeau : %s (rayon %.0f mm, hauteur %.0f mm, os %s)\n", p->hasHat ? "attache a la tete" : "ECHEC",
							 1000.f * p->hat.radius, 1000.f * p->hat.height, p->hasHat ? p->names[(uint32)p->hat.bone].CStr() : "?");
			} else if (NkGarmentFromName(tok, kind)) {
				auto *g = new GarmentSlot();
				NkChrono bc;
				// le patron est ajusté hors des capsules ET hors du MAILLAGE au repos (mesure : les
				// capsules seules laissent 136-142 particules de jupe dans le corps, en-tête de Build)
				const bool ok = g->garment.Build(kind, m, p->map, p->skel, gp, &p->mannequin, &p->restInside);
				const float64 msB = bc.Elapsed().milliseconds;
				g->garment.SnapPins(p->bind.Data(), njm);
				g->garment.cloth.params.clock = [] { return NkChrono::Now().seconds; };
				if (const char *e = std::getenv("NK_GARMENT_PINBLEND"); e && e[0])
					g->garment.cloth.params.sdfPinBlendRings = (uint32)std::atoi(e); // balayage de la zone de transition
				g->tint = Tint(kind);
				g->garment.cloth.Triangles(g->idx);
				g->idxFlip.Resize((uint32)g->idx.Size());
				for (uint32 t = 0; t + 2 < (uint32)g->idx.Size(); t += 3) {
					g->idxFlip[t] = g->idx[t];
					g->idxFlip[t + 1] = g->idx[t + 2];
					g->idxFlip[t + 2] = g->idx[t + 1];
				}
				const uint32 n = g->garment.cloth.ParticleCount();
				g->verts.Resize(n);
				for (uint32 i = 0; i < n; ++i) {
					NkVertex3D &v = g->verts[i];
					v.pos = g->garment.cloth.Positions()[i];
					v.normal = {0.f, 1.f, 0.f};
					v.tangent = {1.f, 0.f, 0.f};
					v.uv = {0.f, 0.f};
					v.uv2 = v.uv;
					v.color = 0xFFFFFFFFu;
				}
				NkMeshDesc md = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), g->verts.Data(), n, g->idx.Data(), (uint32)g->idx.Size());
				md.dynamic = true;
				md.debugName = "Demo3D_Vetement";
				g->mesh = meshSys->Create(md);
				NkMeshDesc mf = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), g->verts.Data(), n, g->idxFlip.Data(), (uint32)g->idxFlip.Size());
				mf.dynamic = true;
				mf.debugName = "Demo3D_Vetement_envers";
				g->meshFlip = meshSys->Create(mf);
				const NkClothStats &st = g->garment.cloth.Stats();
				(void)st;
				std::fprintf(stderr, "[MANNEQUIN PROBE] vetement %-8s : %s, %u particules, %u contraintes, %u panneaux, %u coutures, %u epingles, %.3f kg, construit en %.1f ms, %u x %u\n",
							 NkGarmentName(kind), ok ? "ok" : "ECHEC", n, g->garment.cloth.ConstraintCount(), g->garment.pieces, g->garment.seams,
							 (uint32)g->garment.pins.Size(), g->garment.mass, msB, gp.substeps, gp.iterations);
				p->garments.PushBack(g);
			} else {
				std::fprintf(stderr, "[MANNEQUIN PROBE] vetement inconnu : '%s'\n", tok);
			}
			tok = std::strtok(nullptr, ",; ");
		}
		std::fprintf(stderr, "[MANNEQUIN PROBE] avance %.2f m/s, demi-tour a %.1f s (0 = jamais), depart (%.2f %.2f %.2f), retourne les triangles=%d\n",
					 p->speed, p->turnAt, p->pos.x, p->pos.y, p->pos.z, (int)p->flip);
		return p;
	}

	// ── Un pas ───────────────────────────────────────────────────────────
	void Demo3DMannequinUpdate(Demo3DMannequinProbe *p, NkMeshSystem *meshSys, uint64 frame) {
		if (!p)
			return;
		const float32 dt = 1.f / 60.f;
		NkGLTFMeshData &M = p->animFromFile ? p->animFile : p->model;
		const uint32 nj = (uint32)M.skinJoints.Size();
		// 1. squelette : clip in place + avance pilotée par la sonde
		if (p->turnAt > 0.f && p->time > p->turnAt) {
			float32 f = (p->time - p->turnAt) / 1.f;
			if (f > 1.f)
				f = 1.f;
			p->yaw = 3.14159265f * f;
		}
		const NkMat4f rot = NkMat4f::RotationY(math::NkAngle(p->yaw * 180.f / 3.14159265f));
		const NkVec3f heading = Dir(rot, p->forward0);
		p->pos += heading * (p->speed * dt);
		p->time += dt;
		NkVector<int32> pr;
		if (p->synthWalk) {
			// FK sur les locaux de repos, avec une rotation aux hanches (jambes en opposition),
			// aux genoux (flexion de la jambe arriere) et aux epaules (bras en opposition aux jambes)
			const float32 w = 2.f * 3.14159265f * p->walkFreq;
			const float32 amp = p->walkAmp * 3.14159265f / 180.f;
			const float32 legA = amp * math::NkSin(w * p->time);
			const float32 armA = 0.8f * amp * math::NkSin(w * p->time);
			const int32 *J = p->map.joint;
			p->clipWorld.Resize(nj);
			for (uint32 j = 0; j < nj; ++j) {
				NkMat4f local = p->localBind[j];
				float32 ang = 0.f;
				const int32 jj = (int32)j;
				if (jj == J[NK_HB_L_UPPER_LEG]) ang = legA;
				else if (jj == J[NK_HB_R_UPPER_LEG]) ang = -legA;
				else if (jj == J[NK_HB_L_UPPER_ARM]) ang = -armA;
				else if (jj == J[NK_HB_R_UPPER_ARM]) ang = armA;
				else if (jj == J[NK_HB_L_LOWER_LEG]) ang = -0.6f * (legA < 0.f ? legA : 0.f);
				else if (jj == J[NK_HB_R_LOWER_LEG]) ang = 0.6f * (legA > 0.f ? legA : 0.f);
				if (ang != 0.f) {
					// rotation autour de l'axe DROITE du corps, exprimee dans le repere du parent
					const int32 par = p->parent[j];
					const NkMat4f &pw = (par >= 0 && (uint32)par < nj) ? p->bind[(uint32)par] : NkMat4f::Identity();
					const NkVec3f axis = Dir(pw.Inverse(), p->measures.right);
					local = local * NkMat4f::Rotation(axis, math::NkAngle(ang * 180.f / 3.14159265f));
				}
				const int32 par = p->parent[j];
				p->clipWorld[j] = (par >= 0 && (uint32)par < nj) ? p->clipWorld[(uint32)par] * local : local;
			}
			// hauteur : un petit rebond a deux fois la cadence
			const float32 bob = 0.02f * math::NkSin(2.f * w * p->time);
			for (uint32 j = 0; j < nj; ++j)
				p->clipWorld[j] = NkMat4f::Translate(NkVec3f{0.f, bob, 0.f}) * p->clipWorld[j];
		} else {
			EvaluateGLTFWorldJoints(M, p->anim, p->time, p->clipWorld, pr);
		}
		NkVec3f lock{0.f, 0.f, 0.f};
		if (p->rootMotion && p->map.joint[NK_HB_HIPS] >= 0) {
			const NkVec3f d = Pos(p->clipWorld[(uint32)p->map.joint[NK_HB_HIPS]]) - p->rootRef;
			lock = {-d.x, 0.f, -d.z}; // la racine reste à l'origine du clip (la hauteur garde son rebond)
		}
		const NkMat4f base = NkMat4f::Translate(p->pos) * rot * NkMat4f::Translate(lock);
		p->world.Resize(nj);
		for (uint32 j = 0; j < nj; ++j)
			p->world[j] = base * p->clipWorld[j];
		// 2. peau CPU : positions et normales
		NkChrono skinClock;
		p->palette.Resize(nj);
		for (uint32 j = 0; j < nj; ++j)
			p->palette[j] = p->world[j] * M.inverseBind[j];
		const uint32 nv = (uint32)M.skinnedVertices.Size();
		for (uint32 v = 0; v < nv; ++v) {
			const NkVertexSkinned &sv = M.skinnedVertices[v];
			NkVec3f pos{0.f, 0.f, 0.f}, nrm{0.f, 0.f, 0.f};
			for (uint32 k = 0; k < 4; ++k) {
				const float32 w = sv.boneWeight[k];
				if (w <= 0.f)
					continue;
				const int32 b = (int32)(sv.boneIdx[k] + 0.5f);
				if (b < 0 || (uint32)b >= nj)
					continue;
				pos += p->palette[(uint32)b].TransformPoint(sv.pos) * w;
				nrm += Dir(p->palette[(uint32)b], sv.normal) * w;
			}
			p->bodyPos[v] = pos;
			p->bodyVerts[v].pos = pos;
			const float32 l = nrm.Len();
			p->bodyVerts[v].normal = l > 1e-9f ? nrm * (1.f / l) : NkVec3f{0.f, 1.f, 0.f};
		}
		if (meshSys && p->bodyMesh.IsValid())
			meshSys->UpdateVertices(p->bodyMesh, p->bodyVerts.Data(), nv);
		p->msSkinSum += skinClock.Elapsed().milliseconds;
		// TÉMOIN (5) : allongement des arêtes du corps. Une « épine » est une arête que la pose a
		// étirée : on la nomme avec ses deux sommets et leurs os dominants.
		if ((frame % 60u) == 0u) {
			float32 worst = 1.f;
			uint32 wa = 0, wb = 0, over2 = 0;
			const uint32 *I = M.indices.Data();
			const uint32 nt = (uint32)M.indices.Size() / 3u;
			for (uint32 t = 0; t < nt; ++t)
				for (uint32 e = 0; e < 3; ++e) {
					const uint32 a = I[t * 3 + e], b = I[t * 3 + (e + 1) % 3];
					const float32 r0 = (p->bodyPosRest[a] - p->bodyPosRest[b]).Len();
					if (r0 < 1e-6f)
						continue;
					const float32 r = (p->bodyPos[a] - p->bodyPos[b]).Len() / r0;
					if (r > 2.f)
						++over2;
					if (r > worst) {
						worst = r;
						wa = a;
						wb = b;
					}
				}
			auto dom = [&](uint32 v) {
				const NkVertexSkinned &sv = M.skinnedVertices[v];
				int32 best = -1;
				float32 bw = -1.f;
				for (uint32 k = 0; k < 4; ++k)
					if (sv.boneWeight[k] > bw) {
						bw = sv.boneWeight[k];
						best = (int32)(sv.boneIdx[k] + 0.5f);
					}
				return best;
			};
			const int32 da = dom(wa), db = dom(wb);
			std::fprintf(stderr, "[SKINNING TEMOIN] image %llu : allongement max d'arete x%.2f (sommets %u/%u, os dominants %s / %s), %u aretes au-dela de x2 sur %u -- attendu x1,00 et 0\n",
						 (unsigned long long)frame, worst, wa, wb,
						 da >= 0 && (uint32)da < (uint32)p->names.Size() ? p->names[(uint32)da].CStr() : "?",
						 db >= 0 && (uint32)db < (uint32)p->names.Size() ? p->names[(uint32)db].CStr() : "?", over2, nt * 3u);
		}
		// 3. capsules posées, épingles, pas
		p->mannequin.Pose(p->world.Data(), nj, p->shapes);
		// le CHAMP DE DISTANCE de cette pose (toutes les sdfEvery images)
		if (p->useSdf && !p->sdfPerGarment && (frame % p->sdfEvery) == 0u) {
			NkChrono sc;
			// le champ de l'image précédente devient celui du DÉBUT de pas (les deux sont interpolés
			// par sous-pas, comme les capsules) -- copie par échange de contenu, une seule construction
			p->sdfPrev = p->sdf;
			p->sdf.Build(p->bodyPos.Data(), nv, M.indices.Data(), (uint32)M.indices.Size() / 3u);
			const float32 ms = (float32)sc.Elapsed().milliseconds;
			p->msSdfSum += ms;
			if (ms > p->msSdfMax)
				p->msSdfMax = ms;
		}
		NkChrono insideClock;
		if (p->testInside)
			p->inside.Build(p->bodyPos.Data(), nv, M.indices.Data(), (uint32)M.indices.Size() / 3u);
		const float64 msBuild = insideClock.Elapsed().milliseconds;
		float64 msInside = msBuild;
		const bool count = frame >= 30; // les 30 premieres images sont la mise en place du vetement
		for (uint32 g = 0; g < (uint32)p->garments.Size(); ++g) {
			GarmentSlot *s = p->garments[g];
			NkCloth &c = s->garment.cloth;
			c.colliders.Clear();
			for (uint32 k = 0; k < (uint32)p->shapes.Size(); ++k)
				c.colliders.PushBack(p->shapes[k]);
			if (p->useSdf && p->sdfPerGarment) {
				// LA BOÎTE DU VÊTEMENT : ses particules, dilatées de l'épaisseur, de la marge de
				// déplacement d'un pas (vitesse du corps x dt) et d'une cellule -- à nombre de
				// cellules égal, un volume plus petit donne une cellule bien plus fine.
				NkVec3f gmn, gmx;
				if (c.Bounds(gmn, gmx)) {
					const float32 pad = c.params.thickness + 0.06f;
					s->sdfPrev = s->sdf;
					s->sdf.params = p->sdf.params;
					s->sdf.params.useBounds = true;
					s->sdf.params.boundsMin = gmn - NkVec3f{pad, pad, pad};
					s->sdf.params.boundsMax = gmx + NkVec3f{pad, pad, pad};
					NkChrono gc;
					s->sdf.Build(p->bodyPos.Data(), nv, M.indices.Data(), (uint32)M.indices.Size() / 3u);
					const float32 gms = (float32)gc.Elapsed().milliseconds;
					if (count) {
						s->msSdfSum += gms;
						if (gms > s->msSdfMax)
							s->msSdfMax = gms;
					}
					s->cellSize = s->sdf.Stats().cellSize;
					s->sdfCells = s->sdf.Stats().cells;
					s->sdfSkipped = s->sdf.Stats().skippedTriangles;
				}
			}
			c.bodySDF = p->useSdf ? (p->sdfPerGarment ? (s->sdf.Valid() ? &s->sdf : nullptr)
													  : (p->sdf.Valid() ? &p->sdf : nullptr))
								  : nullptr;
			c.bodySDFPrev = p->useSdf ? (p->sdfPerGarment ? (s->sdfPrev.Valid() ? &s->sdfPrev : nullptr)
														  : (p->sdfPrev.Valid() ? &p->sdfPrev : nullptr))
									  : nullptr;
			s->garment.UpdatePins(p->world.Data(), nj);
			NkChrono simClock;
			c.Step(dt, p->time);
			const float32 ms = (float32)simClock.Elapsed().milliseconds;
			NkChrono ic;
			int32 first = -1;
			// la PARITÉ filtre (rapide) ; le TRIANGLE LE PLUS PROCHE tranche (la parité ment sur une
			// coque non fermée : contrôle à l'init). C'est le second chiffre qui répond à Rodolf.
			// On ne compte QUE les particules LIBRES : une epinglee suit son os, elle est sous la peau
			// par construction (mesure : les « 9 a 13 du foulard en permanence » etaient ses 29 epingles).
			s->freePos.Resize(c.ParticleCount());
			uint32 nFree = 0;
			for (uint32 q = 0; q < c.ParticleCount(); ++q)
				if (c.InvMasses()[q] > 0.f)
					s->freePos[nFree++] = c.Positions()[q];
			const uint32 in = p->testInside ? p->inside.CountInside(s->freePos.Data(), nFree, &first) : 0u;
			float32 depth = 0.f;
			const uint32 inN = p->testInside ? p->inside.CountInsideNearest(s->freePos.Data(), nFree, &depth) : 0u;
			if (count && inN > s->insideNearestMax) {
				s->insideNearestMax = inN;
				s->insideDepthMax = depth;
			}
			if (count)
				s->insideNearestSum += inN;
			msInside += ic.Elapsed().milliseconds;
			const NkClothStats &st = c.Stats();
			if (count) {
				if (st.maxSdfPenetration > s->sdfPenMax) s->sdfPenMax = st.maxSdfPenetration;
				s->sdfContactsMax = st.sdfContacts > s->sdfContactsMax ? st.sdfContacts : s->sdfContactsMax;
				s->capsContactsMax = st.contacts > s->capsContactsMax ? st.contacts : s->capsContactsMax;
				if (st.maxPenetration > s->penMax) s->penMax = st.maxPenetration;
				if (st.maxPinError > s->pinMax) s->pinMax = st.maxPinError;
				if (st.maxStretch > s->stretchMax) s->stretchMax = st.maxStretch;
				s->stretchSum += st.maxStretch;
				if (st.maxStretch > 0.05f) ++s->stretchOver5;
				if (in > s->insideMax) s->insideMax = in;
				if (ms > s->msMax) s->msMax = ms;
				s->insideSum += in;
				s->msSum += ms;
				s->degenerate = st.degenerateEdges;
				++s->frames;
				p->msSimSum += ms;
			}
			// dessin : sommets + normales
			c.ComputeNormals(s->normals);
			for (uint32 i = 0; i < c.ParticleCount(); ++i) {
				s->verts[i].pos = c.Positions()[i];
				s->verts[i].normal = s->normals[i];
			}
			if (meshSys) {
				meshSys->UpdateVertices(s->mesh, s->verts.Data(), c.ParticleCount());
				meshSys->UpdateVertices(s->meshFlip, s->verts.Data(), c.ParticleCount());
			}
			if ((frame % 60u) == 0u) {
				const NkClothProfile &pf = c.Profile();
				std::fprintf(stderr, "[MANNEQUIN PROBE] image %llu t=%.2f s %-8s : pas %.2f ms (moy %.2f, max %.2f) | penetration capsules %.3f mm | DANS LE MAILLAGE %u / %u particules (max %u, premiere %d) | epingles %.3f mm | etirement %.2f %% (%u degenerees) | contacts %u, capsules elaguees %u / %u | profil pred %.2f struct %.2f cis %.2f flex %.2f coll %.2f\n",
							 (unsigned long long)frame, p->time, NkGarmentName(s->garment.kind), ms, s->frames ? (float32)(s->msSum / (float64)s->frames) : ms, s->msMax,
							 1000.f * st.maxPenetration, in, c.ParticleCount(), s->insideMax, first, 1000.f * st.maxPinError, 100.f * st.maxStretch,
							 st.degenerateEdges, st.contacts, st.collidersCulled, (uint32)c.colliders.Size(), pf.predict, pf.structural, pf.shear, pf.bend,
							 pf.colliders);
			}
		}
		p->msInsideSum += msInside;
		if ((frame % 60u) == 0u) {
			const NkVec3f hp = p->map.joint[NK_HB_HIPS] >= 0 ? Pos(p->world[(uint32)p->map.joint[NK_HB_HIPS]]) : p->pos;
			std::fprintf(stderr, "[MANNEQUIN PROBE] image %llu : hanches en (%.2f %.2f %.2f), cap %.0f deg | peau %u sommets %.2f ms | grille+test dedans %.2f ms (%u triangles) | %u capsules\n",
						 (unsigned long long)frame, hp.x, hp.y, hp.z, p->yaw * 180.f / 3.14159265f, nv, (float32)skinClock.Elapsed().milliseconds,
						 (float32)msInside, (uint32)M.indices.Size() / 3u, (uint32)p->shapes.Size());
		}
		++p->frames;
	}

	// ── Dessin ───────────────────────────────────────────────────────────
	void Demo3DMannequinDraw(Demo3DMannequinProbe *p, NkRender3D *r3d, NkMeshHandle cylinder) {
		if (!p || !r3d)
			return;
		auto bounds = [](const NkVec3f *X, uint32 n, NkVec3f &mn, NkVec3f &mx) {
			mn = {1e30f, 1e30f, 1e30f};
			mx = {-1e30f, -1e30f, -1e30f};
			for (uint32 i = 0; i < n; ++i) {
				mn.x = math::NkMin(mn.x, X[i].x); mn.y = math::NkMin(mn.y, X[i].y); mn.z = math::NkMin(mn.z, X[i].z);
				mx.x = math::NkMax(mx.x, X[i].x); mx.y = math::NkMax(mx.y, X[i].y); mx.z = math::NkMax(mx.z, X[i].z);
			}
			mn -= NkVec3f{0.05f, 0.05f, 0.05f};
			mx += NkVec3f{0.05f, 0.05f, 0.05f};
		};
		if (p->bodyMesh.IsValid()) {
			NkDrawCall3D dc;
			dc.mesh = p->bodyMesh;
			bounds(p->bodyPos.Data(), (uint32)p->bodyPos.Size(), dc.aabb.min, dc.aabb.max);
			dc.tint = {0.04f, 0.33f, 0.37f}; // pétrole Rihen #0A555F
			dc.metallic = 0.f;
			dc.roughness = 0.7f;
			r3d->Submit(dc);
		}
		for (uint32 g = 0; g < (uint32)p->garments.Size(); ++g) {
			GarmentSlot *s = p->garments[g];
			const NkCloth &c = s->garment.cloth;
			NkDrawCall3D dc;
			dc.mesh = p->flip ? s->meshFlip : s->mesh;
			bounds(c.Positions(), c.ParticleCount(), dc.aabb.min, dc.aabb.max);
			dc.tint = s->tint;
			dc.metallic = 0.f;
			dc.roughness = 0.9f;
			r3d->Submit(dc);
			NkDrawCall3D back = dc; // l'envers : un tissu se voit des deux côtés
			back.mesh = p->flip ? s->mesh : s->meshFlip;
			back.castShadow = false;
			r3d->Submit(back);
		}
		if (p->hasHat && cylinder.IsValid()) {
			const uint32 nj = (uint32)p->world.Size();
			NkDrawCall3D dc;
			dc.mesh = cylinder;
			const NkMat4f w = p->hat.World(p->world.Data(), nj);
			dc.transform = w * NkMat4f::Scale(NkVec3f{2.f * p->hat.radius, p->hat.height, 2.f * p->hat.radius});
			const NkVec3f c = Pos(w);
			const float32 e = math::NkMax(p->hat.radius, p->hat.height);
			dc.aabb = {c - NkVec3f{e, e, e}, c + NkVec3f{e, e, e}};
			dc.tint = {0.12f, 0.12f, 0.14f};
			dc.metallic = 0.f;
			dc.roughness = 0.6f;
			r3d->Submit(dc);
		}
	}

	// ── Bilan ────────────────────────────────────────────────────────────
	void Demo3DMannequinReport(Demo3DMannequinProbe *p) {
		if (!p)
			return;
		std::fprintf(stderr, "[SDF BILAN] reconstruction toutes les %u images : %.2f ms en moyenne, %.2f ms au pire (resolution %u, calibration %.1f %%)\n",
					 p->sdfEvery, p->frames ? (float32)(p->msSdfSum / (float64)(p->frames / p->sdfEvery + 1u)) : 0.f, p->msSdfMax,
					 p->sdf.params.resolution, 100.f * p->sdfCalib);
		std::fprintf(stderr, "[MANNEQUIN BILAN] %llu images (mesures a partir de la 30e), peau %.2f ms/image, grille+tests dedans %.2f ms/image, simulation totale %.2f ms/image\n",
					 (unsigned long long)p->frames, p->frames ? (float32)(p->msSkinSum / (float64)p->frames) : 0.f,
					 p->frames ? (float32)(p->msInsideSum / (float64)p->frames) : 0.f,
					 p->frames > 30 ? (float32)(p->msSimSum / (float64)(p->frames - 30)) : 0.f);
		for (uint32 g = 0; g < (uint32)p->garments.Size(); ++g) {
			GarmentSlot *s = p->garments[g];
			std::fprintf(stderr, "[MANNEQUIN BILAN] %-8s : %u particules | pas moyen %.2f ms, max %.2f | penetration max dans les capsules %.3f mm | particules DANS LE MAILLAGE : max %u sur une image, moyenne %.2f | epingles max %.3f mm | etirement max %.2f %% (%u aretes degenerees)\n",
						 NkGarmentName(s->garment.kind), s->garment.cloth.ParticleCount(), s->frames ? (float32)(s->msSum / (float64)s->frames) : 0.f,
						 s->msMax, 1000.f * s->penMax, s->insideNearestMax, s->frames ? (float32)s->insideNearestSum / (float32)s->frames : 0.f,
						 1000.f * s->pinMax, 100.f * s->stretchMax, s->degenerate);
			if (p->sdfPerGarment)
				std::fprintf(stderr, "[MANNEQUIN BILAN]            champ PROPRE : cellule %.1f mm, %u cellules, %u triangles sautes, %.2f ms/image (max %.2f)\n",
							 1000.f * s->cellSize, s->sdfCells, s->sdfSkipped,
							 s->frames ? (float32)(s->msSdfSum / (float64)s->frames) : 0.f, s->msSdfMax);
			std::fprintf(stderr, "[MANNEQUIN BILAN]            etirement : max %.2f %% (un PIC), moyenne par image %.2f %%, %u images sur %u au-dessus de 5 %%\n",
						 100.f * s->stretchMax, s->frames ? 100.f * (float32)(s->stretchSum / (float64)s->frames) : 0.f, s->stretchOver5, s->frames);
			std::fprintf(stderr, "[MANNEQUIN BILAN]            (parite, indicative sur une coque non fermee : max %u, moyenne %.2f | profondeur max sous la peau %.1f mm) | CHAMP : penetration max %.3f mm, contacts max %u par le champ contre %u par les capsules\n",
						 s->insideMax, s->frames ? (float32)s->insideSum / (float32)s->frames : 0.f, 1000.f * s->insideDepthMax,
						 1000.f * s->sdfPenMax, s->sdfContactsMax, s->capsContactsMax);
		}
	}

} // namespace nkentseu
