// =============================================================================
// Nogee/Panels/ViewportPanel.cpp — zone centrale, cible du glisser-deposer §9
// (cf. .h : ce panneau N'EST PAS un viewport, et il le dit a l'ecran).
// =============================================================================
#include "ViewportPanel.h"
#include "Nogee/Editor/AssetManager.h" // NkAssetType + DetectType (neutre)
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h" // NkMeshComponent
#include "NKGui/NKGui.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace noge {

		using namespace nkgui;

		void ViewportPanel::Bind(ecs::NkWorld *world, ecs::NkSceneGraph *scene, NkSelectionManager *sel,
								 const char *projectDir) noexcept {
			mWorld = world;
			mScene = scene;
			mSel = sel;
			std::snprintf(mProjectDir, sizeof(mProjectDir), "%s", projectDir ? projectDir : ".");
		}

		bool ViewportPanel::SpawnMeshFromAsset(const char *relPath) noexcept {
			if (!mWorld || !mScene || !relPath || !relPath[0])
				return false;
			if (AssetManager::DetectType(relPath) != NkAssetType::Mesh) {
				char msg[340];
				std::snprintf(msg, sizeof(msg),
							  "[ViewportPanel] depot '%s' : type non instanciable (seul un MESH devient une "
							  "entite) — livre, journalise, rien de spawne\n",
							  relPath);
				logger.Info(msg);
				return false;
			}

			// Nom d'entite = nom de fichier sans extension (ce que Unity/Unreal
			// font au drop) ; chemin projet/rel dans meshPath, parce que
			// NkMeshSystem::Import ouvre depuis le cwd (cf. NkRenderSystem l.153).
			const char *base = relPath;
			for (const char *c = relPath; *c; ++c)
				if (*c == '/' || *c == '\\')
					base = c + 1;
			char name[ecs::NkName::kMaxLen];
			std::snprintf(name, sizeof(name), "%s", base);
			if (char *dot = std::strrchr(name, '.'))
				*dot = '\0';
			if (!name[0])
				std::snprintf(name, sizeof(name), "Mesh");

			char full[520];
			const bool relIsAbs = (relPath[0] == '/' || relPath[0] == '\\' || (relPath[0] && relPath[1] == ':'));
			if (relIsAbs || mProjectDir[0] == '\0' || (mProjectDir[0] == '.' && mProjectDir[1] == '\0'))
				std::snprintf(full, sizeof(full), "%s", relPath);
			else
				std::snprintf(full, sizeof(full), "%s/%s", mProjectDir, relPath);

			const ecs::NkEntityId id = mScene->SpawnNode(name);
			if (!id.IsValid())
				return false;
			// Meme complement que les TEMOIN_* du shell : SpawnNode pose la
			// hierarchie, pas NkName/NkTransform (composants disjoints, cf. carnet).
			mWorld->Add<ecs::NkName>(id, ecs::NkName(name));
			mWorld->Add<ecs::NkTransform>(id);
			ecs::NkMeshComponent mesh;
			mesh.meshPath = NkString(full);
			mWorld->Add<ecs::NkMeshComponent>(id, mesh);
			if (mSel)
				mSel->Select(id);

			mLastSpawned = id;
			++mSpawnCount;
			char msg[640];
			std::snprintf(msg, sizeof(msg),
						  "[ViewportPanel] TEMOIN : entite '%s' instanciee (NkMeshComponent.meshPath='%s') — "
						  "visible dans l'Outliner/Details ; le rendu 3D attend le palier B\n",
						  name, full);
			logger.Info(msg);
			return true;
		}

		void ViewportPanel::OnUI(editorkit::NkEditorFrameContext &ec) {
			NkGuiContext &ctx = ec.Ui();

			Text(ctx, "Viewport — rendu de scene : pas encore cable (ROADMAP §10sexies).");
			if (mLastDropPath[0] != '\0') {
				char line[300];
				std::snprintf(line, sizeof(line), "Dernier asset recu : %s (%s)", mLastDropPath,
							  mSpawnCount > 0 ? "entite instanciee, voir l'Outliner" : "livre, journal");
				Text(ctx, line);
			} else {
				Text(ctx, mWorld ? "Glisser un MESH du Content Browser ici : il devient une entite (Outliner)."
								 : "Glisser une carte du Content Browser ici : la livraison est journalisee.");
			}

			// ── La zone de depot occupe tout le reste, soumise comme un VRAI
			// widget (ButtonBehavior pose lastItemId/lastItemRect — c'est ce que
			// BeginDropTarget consomme). ───────────────────────────────────────
			float32 h = ctx.AvailHeight();
			if (h < 40.f)
				h = 40.f;
			const NkRect zone = ctx.NextItemRect(-1.f, h);
			const NkGuiId zoneId = ctx.GetId("vp_dropzone");
			ctx.ButtonBehavior(zoneId, zone);

			// Couleurs par JETONS de theme, jamais en dur (directive planches) :
			// fond le plus sombre du theme pour une zone en retrait, texte grise.
			ctx.DL().AddRectFilled(zone, ctx.theme.bgPrimary);
			ctx.DL().AddRect(zone, ctx.theme.border, 1.f);
			TextAt(ctx, {zone.x + 12.f, zone.y + 10.f}, "zone de depot (type \"asset\")",
				   ctx.theme.textDisabled);

			if (BeginDropTarget(ctx)) {
				int32 sz = 0;
				if (const void *p = AcceptDragPayload(ctx, "asset", &sz)) {
					// Contrat de la source (ContentBrowserPanel) : chaine
					// zero-terminee, chemin relatif ENTIER. On verifie plutot que
					// de supposer.
					const char *path = static_cast<const char *>(p);
					if (sz > 0 && sz <= static_cast<int32>(sizeof(mLastDropPath)) && path[sz - 1] == '\0') {
						std::snprintf(mLastDropPath, sizeof(mLastDropPath), "%s", path);
						++mDropCount;
						char msg[340];
						std::snprintf(msg, sizeof(msg), "[ViewportPanel] MESURE : charge 'asset' livree : '%s'\n",
									  mLastDropPath);
						logger.Info(msg);
						// Palier A : un mesh devient une entite ; le reste est journalise.
						SpawnMeshFromAsset(mLastDropPath);
					}
				}
				EndDropTarget(ctx);
			}

			// Sonde drag-drop (--dragdrop-test) : rect ecran reel de la zone.
			if (mProbeEnabled) {
				mProbeRect = zone;
				mProbeRectValid = true;
			}
		}

	} // namespace noge
} // namespace nkentseu
