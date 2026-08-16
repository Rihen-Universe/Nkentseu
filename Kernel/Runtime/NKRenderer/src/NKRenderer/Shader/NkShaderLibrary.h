#pragma once
// =============================================================================
// NkShaderLibrary.h  — NKRenderer v4.0  (Shader/)
// Cache de shaders compilés, hot-reload en développement.
// =============================================================================
#include "NkShaderBackend.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
	namespace renderer {

		struct NkShaderProgram {
				// ATTENTION : `NkShaderHandle` ici est resolu selon l'ordre d'include
				// dans le .cpp consommateur (peut etre soit le RHI, soit le renderer-side
				// wrapper). Pour eviter ce piege, on force explicitement les types
				// qualifies (::nkentseu::NkShaderHandle = RHI) :
				::nkentseu::NkShaderHandle handle;	  // Renderer-side ID (cache lookup, hot-reload key)
				::nkentseu::NkShaderHandle rhiHandle; // RHI shader handle (a passer aux pipelines)
				NkString name;
				NkString vertPath, fragPath, geomPath, compPath;
				NkVector<uint8> vertBytecode, fragBytecode, geomBytecode;
				uint64 vertMtime = 0, fragMtime = 0, geomMtime = 0;
				bool valid = false;
		};

		class NkShaderLibrary {
			public:
				NkShaderLibrary() = default;
				~NkShaderLibrary();

				bool Init(NkIDevice *device, NkGraphicsApi api, bool useNkSL = false);
				void Shutdown();

				// NB : tous les handles renvoyes/recus sont les handles RHI
				// (::nkentseu::NkShaderHandle). On les passe directement a
				// NkGraphicsPipelineDesc::shader.

				// ── Chargement depuis fichier ────────────────────────────────────────
				::nkentseu::NkShaderHandle LoadVF(const NkString &vertPath, const NkString &fragPath,
												  const NkString &name = "");
				::nkentseu::NkShaderHandle LoadVGF(const NkString &vert, const NkString &geom, const NkString &frag,
												   const NkString &name = "");
				::nkentseu::NkShaderHandle LoadCompute(const NkString &compPath, const NkString &name = "");

				// ── Chargement depuis source ─────────────────────────────────────────
				// backendOverride : si non-null, compile via ce backend au lieu de
				// mBackend (utilisé pour le chemin NkSL par-shader, opt-in).
				::nkentseu::NkShaderHandle CompileVF(const NkString &vertSrc, const NkString &fragSrc,
													 const NkString &name = "",
													 NkShaderBackend *backendOverride = nullptr);

				// ── User-override / fallback ─────────────────────────────────────────
				// Cherche d'abord un fichier shader user-fourni dans
				//   Resources/NKRenderer/Shaders/<materialName>/<Backend>/<material>.<stage>.<ext>
				// Si trouve, le compile depuis le disque (permet override sans recompilation).
				// Sinon, fallback aux sources embarquees (raw string passees en parametre).
				// L'extension/backend depend de l'api (GL -> .gl.glsl, VK -> .vk.glsl, etc).
				::nkentseu::NkShaderHandle LoadOrCompileVF(const NkString &materialName, const NkString &fallbackVS,
														   const NkString &fallbackFS);

				// ── Hot-reload (polling en développement) ────────────────────────────
				void PollHotReload();

				bool HasPendingReloads() const {
					return mPendingReload;
				}

				// ── Accès ────────────────────────────────────────────────────────────
				::nkentseu::NkShaderHandle Find(const NkString &name) const;
				const NkShaderProgram *Get(::nkentseu::NkShaderHandle h) const;

				// Retourne le handle RHI shader stocke par Recompile/CompileVF.
				// C'est ce handle qu'on passe aux NkGraphicsPipelineDesc::shader.
				// Si le program est invalide, renvoie un handle null.
				::nkentseu::NkShaderHandle GetRHIHandle(::nkentseu::NkShaderHandle h) const;

				// ── Santé des programmes — de quoi afficher « shaders 4/21 » ─────────
				// Demandé par l'agent Noge : dans Nogee, les échecs de création RHI
				// ne se voient qu'en `fprintf(stderr)` (NkShaderLibrary.cpp, branche
				// `if (!prog.valid)`). Un échec qu'on ne peut que lire dans un flot
				// n'est pas un état ; ces deux compteurs en font un nombre, que le
				// shell d'éditeur peut poser dans une pastille de barre d'état.
				//
				// PÉRIMÈTRE — et il compte, parce qu'il décide si le rapport est
				// honnête : on compte les programmes ENREGISTRÉS DANS CETTE
				// bibliothèque, échecs INCLUS. `Alloc()` est appelé
				// inconditionnellement, y compris quand `prog.valid == false` : un
				// programme dont `CreateShader` a échoué reste donc dans `mPrograms`.
				// C'est ce qui permet à `GetProgramCount()` de valoir 21 et non 4 —
				// sans cette propriété, le couple rapporterait « 4/4 » et serait un
				// voyant qui donne le feu vert à la panne qu'il doit signaler.
				//
				// Ce que ces compteurs NE disent PAS : rien sur les shaders jamais
				// demandés. Un shader qui n'a pas été chargé n'est ni valide ni en
				// échec — il est absent des deux nombres.
				uint32 GetValidProgramCount() const;
				uint32 GetProgramCount() const;

				// ── Libération ───────────────────────────────────────────────────────
				void Release(::nkentseu::NkShaderHandle &h);
				void ReleaseAll();

			private:
				NkIDevice *mDevice = nullptr;
				NkGraphicsApi mApi = NkGraphicsApi::NK_GFX_API_OPENGL;
				NkShaderBackend *mBackend = nullptr;
				// Backend NkSL secondaire (toujours créé) : utilisé par LoadOrCompileVF
				// pour les matériaux ayant un .nksl en VRAI dialecte (opt-in par-shader,
				// sans flipper le backend global). Les autres restent en .vk.glsl.
				NkShaderBackend *mNkSLBackend = nullptr;
				NkHashMap<uint64, NkShaderProgram> mPrograms;
				NkHashMap<NkString, ::nkentseu::NkShaderHandle> mByName;
				NkHashMap<NkString, uint64> mFileMtime;
				uint64 mNextId = 1;
				bool mPendingReload = false;

				::nkentseu::NkShaderHandle Alloc(NkShaderProgram &prog);
				bool Recompile(NkShaderProgram &prog);
				uint64 GetFileMtime(const NkString &path);
				NkString ReadFile(const NkString &path);
		};

	} // namespace renderer
} // namespace nkentseu
