#pragma once
// =============================================================================
// NkRender3D.h  — NKRenderer v4.0  (Tools/Render3D/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
	namespace renderer {

		class NkShadowSystem;
		class NkVirtualShadowMaps;
		class NkEnvironmentSystem;
		class NkShaderLibrary;
		class NkResources;

		// (NkViewMode et NkSceneContext sont definis dans Core/NkRendererTypes.h)

		class NkRender3D {
			public:
				NkRender3D() = default;
				~NkRender3D();

				bool Init(NkIDevice *device, NkMeshSystem *mesh, NkMaterialSystem *mat, NkRenderGraph *graph,
						  NkVirtualShadowMaps *shadow, NkEnvironmentSystem *env, NkShaderLibrary *shaderLib,
						  NkResources *resources, uint32 framesInFlight = 1);
				void Shutdown();

				// Notification de redimensionnement (propage par NkRendererImpl).
				// Les RT sont geres par le PostProcess/le RenderGraph ; ici on cache
				// juste la taille courante pour le viewport implicite.
				void OnResize(uint32 w, uint32 h) {
					mW = w;
					mH = h;
				}

				// ── Frame ────────────────────────────────────────────────────────────
				// ResetFrame doit etre appelee UNE FOIS par frame, avant toute passe.
				// Reset le compteur d'UBO objet partage entre toutes les passes de la
				// frame (shadow + opaque + skinned + passes RT comme planar reflection).
				// BeginScene ne le fait PAS — sinon une 2e passe (ex: passe miroir) reset
				// l'index et la 1ere passe relit des UBOs ecrases au moment du Execute().
				void ResetFrame();
				void BeginScene(const NkSceneContext &ctx);
				void Flush(NkICommandBuffer *cmd);

				// ── DEFERRED v1 (cf. NkRendererImpl, branche cfg.deferred) ──────
				// FlushDeferredGeometry : rend les OPAQUES simples dans le G-buffer
				// (pipeline DeferredGeom MRT). Ne consomme PAS la scene (mInScene
				// reste vrai) — FlushForwardRest termine la frame.
				void FlushDeferredGeometry(NkICommandBuffer *cmd);
				// RenderDeferredLighting : passe fullscreen qui lit le G-buffer +
				// lumieres/ombres/IBL du set global et ecrit le HDR.
				void RenderDeferredLighting(NkICommandBuffer *cmd, ::nkentseu::NkTextureHandle texA,
											::nkentseu::NkTextureHandle texN, ::nkentseu::NkTextureHandle texE,
											::nkentseu::NkTextureHandle texD);
				// FlushForwardRest : tout le reste en FORWARD par-dessus le HDR
				// (skybox, instancies, skins, grille, transparents, debug).
				void FlushForwardRest(NkICommandBuffer *cmd);
				// Surcharge render-to-texture : utilise renderPass au lieu du RP Geometry du graph.
				void Flush(NkICommandBuffer *cmd, NkRenderPassHandle renderPass);

				// Flush la queue actuelle dans un RT donne, avec une matrice mirror
				// pre-multipliee a chaque transform de drawcall. Utilise par
				// NkPlanarReflectionSystem pour la passe miroir auto. Ne consomme
				// PAS les queues (les memes drawcalls seront re-flushed par Flush
				// principal) et ne reset PAS mInScene.
				// mirrorMat : typiquement diag(1,-1,1,1) pour un sol Y=0.
				// mirrorViewProj : viewProj mirror passe au shader via uCam pour
				// l'echantillonnage RT cote materiau reflechissant. Optionnel.
				// clipPlane = (Nx, Ny, Nz, d) — si ||N|| > 0, les drawcalls dont
				// le centre AABB verifie dot(N, center) + d <= 0 sont skip
				// (utilise par PlanarReflection pour ne miroirser que les objets
				// du cote actif du plan ; le sol lui-meme et les objets de l'autre
				// cote sont exclus).
				void FlushIntoRT(NkICommandBuffer *cmd, NkRenderPassHandle renderPass, const NkMat4f &mirrorMat,
								 const NkMat4f &mirrorViewProj, const NkVec4f &clipPlane = {0.f, 0.f, 0.f, 0.f});

				// Renvoie true entre BeginScene et le Flush principal.
				bool IsInScene() const {
					return mInScene;
				}

				// Override mCtx.mirrorViewProj depuis l'exterieur (NkPlanarReflection
				// System notamment) : permet d'injecter la viewProj miroir dans le
				// CameraUBO sans toucher au context utilisateur. Utile pour les
				// materiaux qui samplent un RT planar via screen-UV (ReflFloor).
				void SetMirrorViewProj(const NkMat4f &m) {
					mCtx.mirrorViewProj = m;
				}

				// Phase M.2 : Material Parameter Collection (pool de params
				// partages, set=0 binding=25). Bind l'UBO dans tous les global
				// set rings. Si nullptr, le binding=25 reste invalide -> les
				// shaders qui en dependent ne fonctionnent pas (mais ceux qui
				// ne le declarent pas continuent de tourner normalement).
				void SetMaterialCollection(class NkMaterialCollection *mpc);

				// Phase H.6 : injecter le NkVoxelAOSystem. Bind immediatement
				// la texture 3D voxel au binding=27 sur tous les sets du ring.
				// Le pre-bind du Init est skip car mVoxelAO=nullptr a ce moment.
				void SetVoxelAO(class NkVoxelAOSystem *vao);

				// Render des opaques castShadow=true depuis la perspective de la
				// lumiere (lightVP = lightProj * lightView), dans le FBO shadow
				// currentement bindé. Appele par NkShadowSystem dans la passe
				// Shadows du RenderGraph. Reutilise mUBOObject pour le model.
				void RenderShadowPass(NkICommandBuffer *cmd, const NkMat4f &lightVP);

				// AABB monde englobant TOUS les casters d'ombre de la frame courante
				// (mShadowCasters + mInstanced). Utilise par NkVirtualShadowMaps pour
				// auto-fitter la cascade directionnelle a la scene (couverture complete
				// + resolution optimale, sans clipping ni swimming). Fallback [-1,1]^3
				// si aucun caster.
				NkAABB GetShadowCasterBounds() const;

				// ── Stats frustum culling (frame en cours de soumission) ────────
				// Opaque : cull au Submit (les casters d'ombre sont collectes
				// AVANT le cull). Instancie : cull par batch au Flush (la passe
				// shadow itere mInstanced complet ; la passe miroir ne cull pas).
				struct NkCullStats {
						uint32 opaqueSubmitted = 0;
						uint32 opaqueCulled = 0;
						uint32 instancedBatches = 0;
						uint32 instancedCulled = 0;
				};

				const NkCullStats &GetCullStats() const noexcept {
					return mCullStats;
				}

				// Acces au scene context courant (pour NkShadowSystem qui a besoin
				// de la light direction + camera frustum pour le fitting).
				const NkSceneContext &GetSceneContext() const noexcept {
					return mCtx;
				}

				// Accesseurs pour NkVirtualShadowMaps (ring UBO multi-frame).
				uint32 GetFrameSlot() const noexcept {
					return mFrameSlot;
				}

				uint32 GetFramesInFlight() const noexcept {
					return mFramesInFlight;
				}

				// ── Submit ───────────────────────────────────────────────────────────
				void Submit(const NkDrawCall3D &dc);
				void SubmitMany(const NkDrawCall3D *dcs, uint32 count);
				void SubmitInstanced(const NkDrawCallInstanced &dc);
				void SubmitSkinned(const NkDrawCallSkinned &dc);
				void SubmitSkinnedTinted(const NkDrawCallSkinned &dc, NkVec3f tint, float32 alpha = 1.f);

				void SetWireframe(bool e) {
					mWireframe = e;
				}

				bool IsWireframe() const {
					return mWireframe;
				}

				// ── WIREFRAME N-GON (sans diagonales de triangulation) ──────────────
				// Le wireframe RASTERISEUR (SetWireframe) ne connait que des triangles : il
				// dessine donc la DIAGONALE de chaque quad, ce que Blender ne montre pas.
				// Ce mode-ci remplace le rendu des maillages opaques/instancies/skinnes par
				// un BATCH PERSISTANT d'aretes N-GON fourni par l'application
				// (SetNgonWireLines) : les aretes d'une primitive sont calculees UNE fois
				// (quadify) puis re-emises pour chacune de ses instances, et le buffer GPU
				// n'est reecrit que pour les objets dont la transform a change.
				// LIMITE INTRINSEQUE (vraie aussi dans Blender) : un maillage purement
				// TRIANGULE n'a aucune diagonale a cacher — toutes ses aretes sont reelles.
				void SetNgonWireframe(bool e) {
					mNgonWire = e;
				}

				bool IsNgonWireframe() const {
					return mNgonWire;
				}

				// Batch complet (vertices = { x,y,z, r,g,b,a }, stride 28) — (re)cree le buffer.
				void SetNgonWireLines(const float *verts, uint32 vertexCount);
				// Mise a jour PARTIELLE d'une tranche (un objet qui a bouge) : aucune
				// reconstruction du reste du batch.
				void UpdateNgonWireLines(const float *verts, uint32 firstVertex, uint32 vertexCount);
				void ClearNgonWire();

				// Mode de shading (indépendant du wireframe) : 0=RENDERED (PBR éclairé),
				// 1=SOLID/UNLIT (plat, phare caméra, sans lumières de scène). Écrit dans
				// le CameraUBO (uCam.viewMode) et consommé par pbr.frag.
				void SetViewMode(int32 m) {
					mViewMode = m;
				}

				int32 ViewMode() const {
					return mViewMode;
				}

				// MatCap (mode SOLID/WIREFRAME) : index 0..29 dans l'ATLAS des 30 boules
				// generees par NkMatcapLibrary (basiques, ceramiques, damiers de controle
				// des normales et des reflexions, argiles, jade/resine/nacre/peau, metaux
				// dont un anisotrope brosse, toon). NkMatcapLibrary::Name(id) donne le nom
				// affichable, et GenerateBall() la vignette pour un selecteur d'interface.
				static const int32 kMatcapCount = 30;

				void SetMatcap(int32 id) {
					mMatcapId = ((id % kMatcapCount) + kMatcapCount) % kMatcapCount;
				}

				int32 Matcap() const {
					return mMatcapId;
				}

				// Remplace À CHAUD la boule matcap texture (preset Chrome/binding 28) par une
				// texture chargée par l'utilisateur (.exr/.png décodé). tex invalide -> revient
				// à la boule chrome générée. Rebinde tous les sets globaux immédiatement.
				void SetMatcapTexture(NkTextureHandle tex);

				// Contrôle de la force du terme ambient IBL (0=aucun, 1=complet).
				// Défaut 0.3 — réduit le blanchiment par le ciel procédural.
				void SetIBLStrength(float32 s) {
					mIBLStrength = s;
				}

				// Phase N v0.5 : active/desactive le rendu de la skybox HDR en
				// background. Necessite un NkEnvironmentSystem charge (HDR ou
				// procedural) pour avoir un cubemap a sampler.
				void SetSkyboxEnabled(bool e) {
					mDrawSkybox = e;
				}

				bool IsSkyboxEnabled() const {
					return mDrawSkybox;
				}

				// Grille infinie style Blender (plan y=0). Activer + configurer :
				//   grid.cellColor.w  = opacité de l'intérieur (0 = voir à travers, 1 = opaque)
				//   grid.lineColor.w  = alpha des lignes (restent visibles indépendamment)
				//   axes X rouge / Z bleu ; cellSize / majorEvery / fadeEnd réglables.
				void SetInfiniteGridEnabled(bool e) {
					mDrawGrid = e;
				}

				bool IsInfiniteGridEnabled() const {
					return mDrawGrid;
				}

				void SetInfiniteGridParams(const NkInfiniteGridParams &p) {
					mGridParams = p;
				}

				NkInfiniteGridParams &GetInfiniteGridParams() {
					return mGridParams;
				}

				float32 GetIBLStrength() const {
					return mIBLStrength;
				}

				// Phase E.6 : bind une texture comme cookie 3D au slot [0..7].
				// Le `cookieIdx` dans NkLightDesc reference ce slot. Surtout
				// utile pour les SPOT lights qui projettent un motif
				// (faisceau de fenetre, lampe-torche pattern, gobo etc).
				static constexpr uint32 kMaxCookies3D = 8;	   // sampler2D, spot+directional
				static constexpr uint32 kMaxCookiesCube3D = 4; // samplerCube, point lights

				// Phase F.B.1 : taille du pool d'ObjectUBO par frame. Chaque drawcall
				// (shadow + opaque + skinned) consume un slot. Si le total des draws
				// d'une frame > kMaxObjectsPerFrame, on overflow et on log un warning.
				// NkVSM v0 (2026-05-23) : avec multi-light shadows, le total scale
				// = (4 cascades + 6*Npoint_casters + Nspot_casters) * objets +
				// geometry pass. Pour Demo3D : 17 slots * 20 objets + 20 = 360.
				// 1024 = marge 3x sans exploser le descriptor pool Vulkan (qui
				// alloue maxSets=8192 et UNIFORM_BUFFER=4096 dans NkVulkanDevice).
				// V1 future : dynamic offsets UBO (1 buffer + per-draw offset) pour
				// scale a 10k+ draws sans alloc de descriptor sets supplementaires.
				// Capacité INITIALE du pool object-UBO. La capacité courante
				// (mObjectPoolCap) CROÎT dynamiquement (GrowObjectPool) quand une frame
				// dépasse la capacité — cf. ResetFrame. Le plafond dur borne la conso
				// mémoire ET reste sous le descriptor pool VK (bumpé en conséquence).
				static constexpr uint32 kObjectPoolInitial = 1024;
				// Plafond = 4096 : à 2 frames, 2 UBO/set → 16384 descripteurs UBO,
				// exactement ce que le pool descriptor VK bumpé (20480) autorise.
				static constexpr uint32 kObjectPoolHardMax = 4096;
				static constexpr uint32 kObjectUBOBytes = 224; // == sizeof(ObjectUBO)
				uint32 mObjectPoolCap = kObjectPoolInitial;	   // capacité courante (dynamique)

				// Nombre max de bones par skeleton (taille de l'UBO bones[N],
				// std140). DOIT matcher mat4 bones[64] dans skin.vert.vk.glsl.
				// Squelettes plus grands : clamp cote FlushSkinned (les indices
				// > 63 sont clampes a 63 dans le shader). 64 mat4 = 4096 octets,
				// sous la limite UBO 16 Ko garantie partout (VK/GL/DX).
				static constexpr uint32 kMaxBonesUBO = 64;
				static constexpr uint32 kMaxInstancesUBO = 128; // instances/draw (cf instanced.nksl)
				void SetLightCookie3D(uint32 slot, NkTextureHandle tex);

				// E.6b : bind une cubemap comme cookie pour point lights
				// (slot [0..3]). Utiliser cookieIdx dans NkLightDesc pour
				// referencer ce slot. Sample base sur la direction light→frag.
				void SetLightCookieCube3D(uint32 slot, NkTextureHandle cubeTex);

				// ── DEBUG : dessin direct dans swapchain (bypass Geometry pass) ──
				// Cree un pipeline minimal (shader trivial, pas d'UBO, pas de set,
				// depthTest=off) compatible avec swapchain RP fallback et dessine
				// un triangle NDC. Permet d'isoler si le bug est dans le Geometry
				// pass / HDR transient FB ou plus profond.
				void DebugDrawDirectSwapchain(NkICommandBuffer *cmd);

				// ── Debug gizmos ─────────────────────────────────────────────────────
				// overlay=true : ligne dessinée SANS depth-test (toujours au-dessus de la
				// scène, façon gizmo Blender). Défaut false = depth-test normal (occlusion).
				void DrawDebugLine(NkVec3f a, NkVec3f b, NkVec4f color, float32 life = 0.f, bool overlay = false);
				// Triangle debug PLEIN (alpha-blend). Utile pour surligner des faces
				// sélectionnées (éditeur), des zones, etc. overlay=true -> sans depth-test.
				void DrawDebugTriangle(NkVec3f a, NkVec3f b, NkVec3f c, NkVec4f color, float32 life = 0.f,
									   bool overlay = false);

				// ── Edit overlay PERSISTANT (cage/faces/points d'un Edit Mode) ───────
				// Buffers GPU gardés d'une frame à l'autre : on N'UPLOADE QUE quand la
				// donnée change (entrée/sélection/drag). Rendu chaque frame par le GPU
				// sans reconstruction CPU -> reste fluide même sur mesh dense. Les
				// vertices sont { pos.x,pos.y,pos.z, r,g,b,a } (7 float, stride 28).
				// depthTest=false -> mode X-ray (dessiné au-dessus de tout).
				void SetEditOverlayLines(const float *verts, uint32 vertexCount);
				void SetEditOverlayTris(const float *verts, uint32 vertexCount);
				void SetEditOverlayPoints(const float *verts, uint32 vertexCount);
				void SetEditOverlayXray(bool xray);
				void ClearEditOverlay();
				void DrawDebugSphere(NkVec3f c, float32 r, NkVec4f color);
				void DrawDebugCircle(NkVec3f c, float32 r, NkVec3f normal, NkVec4f color);
				void DrawDebugAABB(const NkAABB &box, NkVec4f color);
				void DrawDebugAxes(const NkMat4f &t, float32 size = 1.f);
				void DrawDebugGrid(NkVec3f origin, float32 spacing, int32 lines, NkVec4f color);
				void DrawDebugArrow(NkVec3f from, NkVec3f to, NkVec4f color);

				// ── Sélection « outline silhouette » façon Blender ───────────────────
				// OPTION DISTINCTE des marqueurs AABB du gizmo (SetDrawObjectBounds /
				// NK_GIZMO_OBB, qui restent disponibles en parallèle). Un fin liseré
				// (défaut orange ~{1,0.45,0.05}) SUIT LA SILHOUETTE des maillages soumis
				// via SubmitSelection, obtenu par un post-process de détection de bord :
				//   1) passe MASQUE : les objets sélectionnés sont rendus SEULS (blanc plein)
				//      dans une cible offscreen R8 (silhouette) ;
				//   2) passe PLEIN ÉCRAN : dilatation-différence du masque -> liseré composité
				//      par-dessus l'image finale (alpha-blend), épaisseur constante en pixels.
				// Les deux passes sont ajoutées au RenderGraph (SelectionMask + SelectionOutline)
				// quand l'option est active ; un changement d'état déclenche un rebuild du graph
				// (cf. ConsumeSelOutlineGraphDirty, consommé par NkRendererImpl::BeginFrame).
				void SetSelectionOutline(bool enabled, NkVec4f color = {1.f, 0.45f, 0.05f, 1.f},
										 float32 thicknessPx = 2.5f);
				bool IsSelectionOutlineEnabled() const {
					return mSelOutline;
				}
				NkVec4f SelectionOutlineColor() const {
					return mSelOutlineColor;
				}
				float32 SelectionOutlineThickness() const {
					return mSelOutlineThickness;
				}
				// Soumet un mesh à la file de sélection de la frame (rendu SEUL dans le
				// masque de silhouette). À rappeler chaque frame : la file est vidée par
				// BeginScene comme les autres files de soumission.
				void SubmitSelection(const NkDrawCall3D &dc);
				bool HasSelection() const {
					return !mSelection.Empty();
				}
				// true si l'état enable a changé depuis le dernier appel (consommé par
				// NkRendererImpl::BeginFrame pour rebuild le RenderGraph au bon moment).
				bool ConsumeSelOutlineGraphDirty() {
					bool d = mSelOutlineGraphDirty;
					mSelOutlineGraphDirty = false;
					return d;
				}
				// Appelées par le RenderGraph (NkRendererImpl) :
				//  - RenderSelectionMask : rend la file de sélection (blanc) dans la cible masque.
				//  - CompositeSelectionOutline : liseré plein écran sur la cible finale (maskTex lu).
				void RenderSelectionMask(NkICommandBuffer *cmd);
				void CompositeSelectionOutline(NkICommandBuffer *cmd, NkTextureHandle maskTex);

			private:
				struct SortedDC {
						NkDrawCall3D dc;
						float32 depth;
				};

				struct DebugLine {
						NkVec3f a, b;
						NkVec4f color;
						float32 life;
						bool overlay;
				};

				struct DebugTri {
						NkVec3f a, b, c;
						NkVec4f color;
						float32 life;
						bool overlay;
				};

				float32 mIBLStrength = 0.3f;
				NkIDevice *mDevice = nullptr;
				NkMeshSystem *mMesh = nullptr;
				NkMaterialSystem *mMat = nullptr;
				NkRenderGraph *mGraph = nullptr;
				NkVirtualShadowMaps *mShadow = nullptr;
				NkEnvironmentSystem *mEnv = nullptr;
				class NkVoxelAOSystem *mVoxelAO = nullptr; // Phase H.6
				NkShaderLibrary *mShaderLib = nullptr;
				NkResources *mResources = nullptr;

				NkSceneContext mCtx;
				bool mInScene = false;
				bool mWireframe = false;
				int32 mViewMode = 0;   // 0=rendered(lit) 1=solid(unlit)
				int32 mMatcapId = 0;		// index de matcap dans l'atlas (mode solid)
				bool mMatcapCustom = false; // true = texture utilisateur (boule SEULE, pas l'atlas)
				uint32 mW = 0, mH = 0; // taille courante (mise a jour par OnResize)
				NkCullStats mCullStats; // stats frustum culling (reset par frame)

				// Fallback material instance : utilise pour les drawcalls sans
				// material custom. Le shader PBR canonical sample tAlbedo dans
				// set=2 binding=3 (convention NkMaterialSystem), donc set=2 doit
				// toujours etre bind. Cree lazy au premier FlushOpaque a partir
				// de mMat->DefaultPBR().
				NkMatInstHandle mFallbackMatInst;

				NkVector<SortedDC> mOpaque;
				// Casters d'ombre : liste DISTINCTE de mOpaque. Un objet hors du
				// frustum CAMERA est cull de mOpaque (pas rendu a l'ecran), mais
				// son ombre peut malgre tout tomber dans la zone visible. La
				// passe shadow doit donc voir TOUS les casters, pas seulement
				// ceux visibles a la camera -> on les collecte ici sans culling
				// camera (RenderShadowPass itere sur cette liste).
				NkVector<SortedDC> mShadowCasters;
				NkVector<SortedDC> mTransparent;
				NkVector<NkDrawCallInstanced> mInstanced;
				NkVector<NkDrawCallSkinned> mSkinned;
				NkVector<DebugLine> mDebugLines;
				NkVector<DebugTri> mDebugTris;

				// Ring buffers per-frame UBOs (taille = NkRendererConfig::framesInFlight,
				// clampe a [1,3]). mFrameSlot tourne 0..N-1 a chaque BeginScene.
				// Camera + Lights : 1 UBO par frame (donnees globales, ecrites une fois
				// par frame dans UploadUBOs).
				NkVector<NkBufferHandle> mUBOCameraRing;
				NkVector<NkBufferHandle> mUBOLightsRing;
				// Phase Planar Reflection fix 2026-05-24 : UBO Camera dedie pour
				// la mirror pass. Sans ca, FlushIntoRT (mirror) puis Flush
				// principal ecrasent le meme UBO -> mirror pass lit le state
				// final (main) au lieu du state mirror.
				NkVector<NkBufferHandle> mUBOCameraMirrorRing;

				// Phase F.B.1 : pool d'ObjectUBO (frame x drawIdx). Vulkan interdit
				// vkCmdUpdateBuffer dans un renderPass actif, donc on ne peut pas
				// re-uploader le meme UBO entre deux draws. Solution : 1 UBO + 1
				// descriptor set par drawcall, tous pre-alloues a Init. WriteBuffer
				// (memcpy via mapped pointer) est legal dans le renderPass.
				// mObjectDrawIdx compte les draws consommes pour la frame courante,
				// reset a 0 dans BeginScene. Shadow + opaque + skinned partagent le
				// meme compteur monotone (ordre : shadow d'abord, puis opaque, puis
				// skinned).
				NkVector<NkVector<NkBufferHandle>> mUBOObjectPool; // [frame][drawIdx]
				// Bones UBO : UN uniform buffer (mat4 bones[64], std140) PAR
				// frame-in-flight (ring), bind au binding=2 des sets objet de la
				// frame i a Init. Sans ring (1 seul buffer partage), la frame N+1
				// reecrivait le buffer pendant que le GPU lisait encore la frame N
				// (Vulkan multi-frame) -> course -> clignotement. Indexe par
				// mFrameSlot comme mUBOCameraRing/mUBOObjectPool.
				// Migration UBO (ex-SSBO) : un UBO est portable et solide sur les
				// 4 backends (GL/VK/DX11/DX12) — le SSBO StructuredBuffer/SRV ne
				// remontait pas au shader sur DX11/DX12 (skin invisible) et
				// creait une course sur Vulkan. 64 bones max (=4096 octets).
				NkVector<NkBufferHandle> mUBOBonesRing;	   // [frame]
				NkVector<NkBufferHandle> mUBOInstanceRing; // [frame] models[128]+tints[128] (instancing GPU)
				NkTextureHandle mDefaultCubeWhite;		   // E.6b : fallback cube cookie
				NkTextureHandle mMatcapTex;				   // boule matcap (mode solid, binding 28)
				uint32 mFramesInFlight = 1;
				uint32 mFrameSlot = 0;
				uint32 mObjectDrawIdx = 0;

				// Descriptor sets: set 0 = per-frame (camera+lights+shadow+env+shadowMap),
				//                  set 1 = per-object (model+bones)
				NkDescSetHandle mGlobalLayout;
				NkVector<NkDescSetHandle> mGlobalSetRing;
				// Phase Planar Reflection fix : descriptor set mirror dedie,
				// bind a mUBOCameraMirrorRing[slot] au lieu de mUBOCameraRing.
				// Tous les autres bindings (lights, shadow, env, voxel, etc.)
				// sont identiques au ring main.
				NkVector<NkDescSetHandle> mGlobalSetMirrorRing;
				NkDescSetHandle mObjectLayout;
				// Phase F.B.1 : pool de descriptor sets per-object (frame x drawIdx).
				// Chaque set est pre-bind a son UBO du pool a Init (1:1).
				NkVector<NkVector<NkDescSetHandle>> mObjectSetPool;

				// RP override pour Flush(cmd, rp) : permet aux passes RT (planar
				// reflection) de compiler leurs pipelines avec le RP du RT au
				// lieu du Geometry RP qui n'existe pas encore a la 1re frame
				// (FB cree lazy par le RenderGraph). Reset apres chaque Flush.
				NkRenderPassHandle mPendingRP{};

				// Mirror matrix pre-multipliee aux transforms de drawcalls quand
				// != identity. Utilisee par FlushIntoRT (NkPlanarReflectionSystem)
				// pour reflechir la scene dans un RT sans toucher les drawcalls
				// d'origine. Reset apres chaque FlushIntoRT.
				NkMat4f mPendingMirror = NkMat4f::Identity();
				bool mPendingMirrorActive = false;
				NkMat4f mPendingMirrorViewProj = NkMat4f::Identity();
				// Clip plane = (Nx, Ny, Nz, d). Si ||N|| > 0, les drawcalls dont
				// le centre AABB verifie dot(N, center) + d <= 0 sont skip
				// (filtre cote actif du plan dans la passe miroir).
				NkVec4f mPendingClipPlane = {0.f, 0.f, 0.f, 0.f};

				// PBR pipeline + shader (charges depuis Resources/Shaders/PBR/GL/).
				// Le pipeline est cree paresseusement au 1er FlushOpaque, quand
				// le RP de la pass Geometry du RenderGraph est connu (Vulkan/DX12
				// exigent la compatibilite RP a la creation). mPBRPipelineRP
				// track quel RP a servi a creer le pipeline pour le recreer si
				// le RP change (ex : resize swapchain, toggle PostProcess).
				::nkentseu::NkShaderHandle mPBRShader; // RHI shader handle
				NkPipelineHandle mPBRPipeline;		   // pipeline graphics PBR
				NkRenderPassHandle mPBRPipelineRP{};

				// Shadow pipeline + shader (depth-only, reutilise dans les passes
				// de shadow map du NkShadowSystem). Partage mObjectLayout avec PBR.
				::nkentseu::NkShaderHandle mShadowShader;
				NkPipelineHandle mShadowPipeline;

				// NkVSM v2 : shadow ALPHA-TESTED (feuillage/masked). Variante du
				// pipeline Shadow qui sample l'albedo du material (set=2 = layout
				// universel NkMaterialSystem) et discard sous cutoff 0.5. Selectionne
				// par-caster dans RenderShadowPass quand matInst->mCastShadowAlphaTest.
				::nkentseu::NkShaderHandle mShadowAlphaShader;
				NkPipelineHandle mShadowAlphaPipeline;

				// ── DEFERRED v1 ───────────────────────────────────────────────
				// Pipeline G-buffer fill (DeferredGeom, MRT 3 cibles + depth) +
				// pipeline lighting fullscreen (DeferredLight, lit le G-buffer).
				// Voir NkRendererImpl::RebuildRenderGraph (branche cfg.deferred).
				::nkentseu::NkShaderHandle mDeferredGeomShader;
				NkPipelineHandle mDeferredGeomPipeline;
				::nkentseu::NkShaderHandle mDeferredLightShader;
				NkPipelineHandle mDeferredLightPipeline;
				NkDescSetHandle mGBufLayout; // set=1 du lighting : 4 samplers (A/N/E/depth)
				NkDescSetHandle mGBufSet;
				bool EnsureDeferredGeomPipeline(NkRenderPassHandle rp);
				bool EnsureDeferredLightPipeline(NkRenderPassHandle rp);

				// Shadow INSTANCIÉ : projette les instances (mInstanced) dans l'atlas
				// d'ombre en 1 draw/batch (InstanceUBO set=1 binding=4 + lightVP push
				// constant), au lieu d'un slot d'ObjectUBO par instance (qui débordait
				// le pool). Pool de buffers d'instances dédié (data indépendante de la
				// lumière → write-once par batch/invocation, pas de hazard). Le set
				// objet (binding1 identité + binding4 = buffer d'instances) est pris
				// dans mObjectSetPool (1 slot/batch, négligeable).
				::nkentseu::NkShaderHandle mShadowInstanceShader;
				NkPipelineHandle mShadowInstancePipeline;
				NkVector<NkVector<NkBufferHandle>> mUBOShadowInstPool; // [frame][idx] models[128]+tints[128]
				uint32 mShadowInstIdx = 0;
				static constexpr uint32 kShadowInstPoolCap = 128; // batches×invocations/frame

				// Skinning GPU : shader + pipeline dedies. Le pipeline skin utilise
				// un vertex layout NkVertexSkinned (pos/nrm/tan/uv/uv2/color +
				// boneIdx vec4 + boneWeight vec4) et lit l'UBO de bones (mUBOBonesRing)
				// bind dans le set objet (set=1, binding=2). Pipeline lazy comme PBR.
				::nkentseu::NkShaderHandle mSkinShader;
				NkPipelineHandle mSkinPipeline;
				NkRenderPassHandle mSkinPipelineRP{};

				// ── GPU instancing 1-draw (instanced.nksl, opt-in NK_INSTANCING_GPU) ──
				// Shader instancié : layout vertex STANDARD (NkVertex3D) + lit la
				// matrice modèle/tint par instance dans mUBOInstanceRing[frame]
				// (bindé au set objet, comme les bones). Si le pipeline n'est pas
				// créé, FlushInstanced retombe sur l'expansion object-UBO (correcte).
				::nkentseu::NkShaderHandle mInstanceShader;
				NkPipelineHandle mInstancePipeline;
				NkRenderPassHandle mInstancePipelineRP{};

				// ── Phase N v0.5 : Background HDR Skybox ───────────────────────
				// Skybox dessinee en debut de Flush (avant FlushOpaque) avec
				// depth=1.0 LEQUAL, donc cachee par tout objet opaque dessine
				// apres. Sample le cubemap prefilter (binding=9) du set global.
				// mDrawSkybox = true active le rendu, mis par SetSkyboxEnabled().
				::nkentseu::NkShaderHandle mSkyboxShader;
				NkPipelineHandle mSkyboxPipeline;
				NkRenderPassHandle mSkyboxPipelineRP{};
				bool mDrawSkybox = false;

				// Grille infinie style Blender (plan y=0). Grand quad suivant la caméra,
				// rendu APRÈS l'opaque (depth test read-only + alpha blend). Params
				// (couleurs, opacité intérieur, taille cellule, fondu) via push constant.
				::nkentseu::NkShaderHandle mGridShader;
				NkPipelineHandle mGridPipeline;
				NkRenderPassHandle mGridPipelineRP{};
				bool mDrawGrid = false;
				NkInfiniteGridParams mGridParams;

				void UploadUBOs(NkICommandBuffer *cmd);

				// Phase N v0.5 : Skybox lazy pipeline + draw call.
				bool EnsureSkyboxPipeline(NkRenderPassHandle currentRP);
				void DrawSkybox(NkICommandBuffer *cmd);
				bool EnsureGridPipeline(NkRenderPassHandle currentRP);
				void DrawGrid(NkICommandBuffer *cmd);
				void FlushOpaque(NkICommandBuffer *cmd);
				void FlushTransparent(NkICommandBuffer *cmd);
				void FlushInstanced(NkICommandBuffer *cmd);
				void FlushSkinned(NkICommandBuffer *cmd);
				void FlushDebug(NkICommandBuffer *cmd, NkRenderPassHandle currentRP, NkDescSetHandle gs);
				void SortDrawCalls();

				// Cree (ou recree) le pipeline PBR pour qu'il soit compatible
				// avec le RP courant de la pass Geometry. Lazy : appelee au
				// debut de Flush() seulement quand la pass Geometry a deja
				// execute au moins une fois (donc son fb est cache). Idempotent
				// si le RP n'a pas change. Retourne false si shader ou create
				// ont echoue.
				bool EnsurePBRPipeline(NkRenderPassHandle currentRP);

				// Agrandit le pool object-UBO (buffers + descriptor sets) à `newCap`
				// pour CHAQUE frame-in-flight. Appelé HORS render pass (depuis
				// ResetFrame) — Vulkan interdit l'alloc dans une passe active. Idempotent
				// si newCap <= capacité courante ; borné par kObjectPoolHardMax.
				void GrowObjectPool(uint32 newCap);

				// Cree (lazy) le pipeline de skinning GPU, compatible avec le RP
				// courant. Vertex layout NkVertexSkinned + shader "Skin". Le SSBO
				// de bones est lie au set objet (set=1, binding=2). Idempotent.
				bool EnsureSkinPipeline(NkRenderPassHandle currentRP);

				// Cree (lazy) le pipeline d'instancing GPU (shader "Instanced",
				// vertex layout STANDARD NkVertex3D). Le buffer d'instances est lie
				// au set objet (binding 2, comme les bones). Renvoie false si le
				// shader instancié n'a pas pu être chargé/compilé. Idempotent.
				bool EnsureInstancePipeline(NkRenderPassHandle currentRP);

				// ── DEBUG triangle minimal (isolation bug PBR Vulkan) ────────
				// Mode 0 = PBR normal. Mode 1 = triangle non-indexed (cmd->Draw).
				// Mode 2 = triangle indexed (cmd->DrawIndexed). Permet de tester
				// le pipeline VK le plus simple possible (pas d'UBO, pas de set,
				// shader trivial) pour isoler ou se trouve le bug.
				static constexpr int kDebugTriangleMode = 0; // 0|1|2

				bool mDebugInited = false;
				::nkentseu::NkShaderHandle mDebugShader;
				NkPipelineHandle mDebugPipeline;
				NkRenderPassHandle mDebugPipelineRP{};
				NkBufferHandle mDebugVBO; // 3 vertices vec3
				NkBufferHandle mDebugIBO; // 3 indices uint32

				bool EnsureDebugTriangle(NkRenderPassHandle currentRP);
				void DebugDrawTriangleNoIdx(NkICommandBuffer *cmd);
				void DebugDrawTriangleIdx(NkICommandBuffer *cmd);

				// ── Debug LINE renderer (gizmos, squelettes IK, axes…) ──────────
				// Vrai rendu des lignes accumulées (DrawDebugLine/Sphere/Grid/…),
				// avant : FlushDebug ne faisait que gérer la durée de vie (stub).
				// Vertex = pos vec3 + couleur vec4 (stride 28), topologie LINE_LIST,
				// VBO dynamique réuploadé par frame, transformé par la CameraUBO.
				::nkentseu::NkShaderHandle mLineShader;
				NkPipelineHandle mLinePipeline;
				NkRenderPassHandle mLinePipelineRP{};
				NkPipelineHandle mLinePipelineNoDepth; // depth-test OFF (overlay gizmo)
				// RING PAR FRAME EN VOL : ce VBO est reecrit A CHAQUE FRAME (les gizmos /
				// marqueurs sont des primitives « une frame »). Un buffer unique = le CPU
				// memcpy dans le buffer que le GPU lit encore pour une frame precedente
				// (jusqu'a framesInFlight-1 frames en vol) -> lignes dechirees. Un buffer
				// par slot supprime la course. Cf. NgonWireBufferForFrame.
				NkVector<NkBufferHandle> mLineVBORing;
				NkVector<uint32> mLineVBORingCap;
				// Triangles debug pleins (alpha-blend) : mêmes shader/VBO logique que
				// les lignes mais topologie TRIANGLE_LIST + blend.
				NkPipelineHandle mTriPipeline;
				NkPipelineHandle mTriPipelineNoDepth;
				NkRenderPassHandle mTriPipelineRP{};
				// Meme ring par frame en vol que mLineVBORing (reecrit chaque frame).
				NkVector<NkBufferHandle> mTriVBORing;
				NkVector<uint32> mTriVBORingCap;
				bool EnsureDebugTriOverlayPipeline(NkRenderPassHandle currentRP);
				// Helper commun aux deux rings ci-dessus : rend le buffer du SLOT COURANT,
				// (re)cree si trop petit, puis y ecrit `vcount` vertices.
				NkBufferHandle DebugRingUpload(NkVector<NkBufferHandle> &ring, NkVector<uint32> &caps, const void *v,
											   uint32 vcount, uint32 strideBytes);
				// Edit overlay persistant (uploadé seulement au changement).
				// Egalement RINGE : pendant un drag/une operation modale il est reconstruit
				// a chaque frame, donc soumis a la meme course que les VBO debug.
				NkVector<NkBufferHandle> mEditLineRing, mEditTriRing, mEditPointRing;
				NkVector<uint32> mEditLineRingCap, mEditTriRingCap, mEditPointRingCap;
				NkVector<float32> mEditLineCPU, mEditTriCPU, mEditPointCPU; // copie CPU = autorite
				NkVector<uint8> mEditLineDirty, mEditTriDirty, mEditPointDirty; // 1 = slot perime
				uint32 mEditLineN = 0, mEditTriN = 0, mEditPointN = 0;		 // vertices actifs
				// ── Batch persistant d'aretes n-gon (wireframe sans diagonales) ──────────
				// RING PAR FRAME EN VOL (même idiome que mUBOCameraRing / mUBOBonesRing /
				// mGlobalSetRing). CAUSE DU CLIGNOTEMENT corrigée ici : le batch était UN
				// SEUL buffer, réécrit CHAQUE FRAME (la scène contient un cube animé, donc sa
				// tranche est retransformée à chaque image). Avec framesInFlight = 2, la frame
				// N+1 écrivait dans le buffer que le GPU était encore en train de lire pour la
				// frame N -> lignes corrompues une image sur deux = scintillement rapide.
				// Désormais chaque frame en vol a SON buffer ; la copie CPU du batch est
				// l'autorité, et chaque slot est remis à niveau (uniquement sur la PLAGE
				// modifiée) juste avant d'être dessiné.
				NkVector<NkBufferHandle> mNgonWireRing;
				NkVector<uint32> mNgonWireRingCap; // capacité (vertices) par slot
				NkVector<uint32> mNgonWireDirtyLo; // plage à re-uploader par slot (en vertices)
				NkVector<uint32> mNgonWireDirtyHi; // hi <= lo => slot à jour
				NkVector<float32> mNgonWireCPU;	   // copie CPU = autorité du batch
				uint32 mNgonWireN = 0;
				bool mNgonWire = false;
				void NgonWireMarkDirty(uint32 firstVertex, uint32 count); // marque TOUS les slots
				NkBufferHandle NgonWireBufferForFrame();				  // slot courant, remis à niveau
				bool mEditOverlayNoDepth = false;							 // X-ray
				// Point sprite écran-constant (marqueurs de vertices façon Blender).
				::nkentseu::NkShaderHandle mEditPointShader;
				NkPipelineHandle mEditPointPipeline, mEditPointPipelineNoDepth;
				NkRenderPassHandle mEditPointPipelineRP{};
				bool EnsureEditPointPipeline(NkRenderPassHandle currentRP);
				// stride en OCTETS d'un vertex (7*float lignes/tris, 9*float points sprite).
				// Copie dans la copie CPU (autorite) et marque TOUS les slots perimes.
				void UploadEditBuf(NkVector<float32> &cpu, NkVector<uint8> &dirty, const float *v, uint32 vcount,
								   uint32 floatsPerVertex);
				// Slot courant remis a niveau depuis la copie CPU, juste avant le draw.
				NkBufferHandle EditBufForFrame(NkVector<NkBufferHandle> &ring, NkVector<uint32> &caps,
											   NkVector<uint8> &dirty, const NkVector<float32> &cpu, uint32 vcount,
											   uint32 floatsPerVertex);
				bool EnsureDebugLinePipeline(NkRenderPassHandle currentRP);

				// ── Sélection « outline silhouette » (post-process edge-detect) ──────
				// DÉFAUT = true : le liseré silhouette est l'indicateur de sélection PAR
				// DÉFAUT (à la place de l'AABB/OBB du gizmo, désormais opt-in). Ne coûte
				// rien tant que rien n'est soumis via SubmitSelection (masque vide). Les
				// passes SelectionMask/SelectionOutline sont ajoutées au graph tant que
				// cette option est active (cf. NkRendererImpl::BuildDefaultRenderGraph).
				bool mSelOutline = true;
				bool mSelOutlineGraphDirty = false; // enable a changé -> rebuild du graph
				NkVec4f mSelOutlineColor = {1.f, 0.45f, 0.05f, 1.f};
				float32 mSelOutlineThickness = 2.5f;
				NkVector<NkDrawCall3D> mSelection; // file de la frame (vidée par BeginScene)
				// Passe MASQUE : rend les objets sélectionnés en blanc (silhouette).
				::nkentseu::NkShaderHandle mSelMaskShader;
				NkPipelineHandle mSelMaskPipeline;
				NkRenderPassHandle mSelMaskPipelineRP{};
				// Passe OUTLINE : fullscreen edge-detect du masque -> liseré composité.
				::nkentseu::NkShaderHandle mSelOutlineShader;
				NkPipelineHandle mSelOutlinePipeline;
				NkRenderPassHandle mSelOutlinePipelineRP{};
				NkDescSetHandle mSelTexLayout; // 1 sampler (le masque)
				NkDescSetHandle mSelTexSet;
				bool EnsureSelMaskPipeline(NkRenderPassHandle currentRP);
				bool EnsureSelOutlinePipeline(NkRenderPassHandle currentRP);
		};

	} // namespace renderer
} // namespace nkentseu
