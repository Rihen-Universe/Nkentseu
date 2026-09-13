// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Nogee/Panels/ViewportPanel.cpp — LA VUE DE SCENE (et la cible du glisser §9)
// =============================================================================
// 2026-09-13 : ce panneau EST devenu un viewport. La scene ECS est rendue hors
// ecran par `NogeeViewport3D` (seule unite a voir NKRenderer) et posee ici en
// une image. Le panneau, lui, ne rend toujours RIEN par lui-meme : il declare la
// taille qu'il veut voir et pose la texture — un panneau decrit, il ne rend pas.
// =============================================================================
#include "ViewportPanel.h"
#include "Nogee/Viewport/NogeeViewport3D.h" // facade OPAQUE (aucun type NKRenderer)
#include "Nogee/Editor/AssetManager.h"		// NkAssetType + DetectType (neutre)
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
			// ⚠️ SANS CE COMPOSANT L'ENTITE N'EST JAMAIS DESSINEE, et personne ne
			// le disait. `NkRenderSystem::SubmitMeshes` (NkRenderSystem.cpp l.142)
			// interroge `Query<NkTransform, NkMeshComponent, NkMaterialComponent>` :
			// un mesh sans materiau n'entre pas dans la requete — il n'est meme pas
			// IMPORTE, puisque l'import paresseux est dans le corps du ForEach.
			// L'entite apparaissait donc dans l'Outliner et jamais a l'ecran, ce qui
			// est exactement le mur de ce chantier. Slot 0 vide = materiau par
			// defaut du renderer, ce qui est le bon comportement au depot d'un mesh
			// nu (Unity/Unreal font pareil).
			mWorld->Add<ecs::NkMaterialComponent>(id, ecs::NkMaterialComponent{});
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

			Text(ctx, NogeeViewport3DReady()
						  ? "Viewport — scene ECS rendue par NkRenderSystem (cible hors ecran partagee)."
						  : "Viewport — rendu de scene indisponible : pile GPU non montee (voir le journal).");
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
			// ⚠️ PAS `AvailHeight()`, ET LA RAISON EST DANS LE KIT, PAS ICI.
			// `AvailHeight()` rend la hauteur restante dans la REGION DE LAYOUT ;
			// dans un cadre defilant celle-ci vaut DELIBEREMENT 1.0e6
			// (NkGuiWidgets.cpp l.3801), parce que le contenu d'un panneau
			// defilant n'est pas borne par ce qu'on en voit — c'est le rognage qui
			// borne. Elle rendait donc 999936, ce qui est CORRECT pour du contenu
			// qui coule et absurde pour dimensionner une cible de rendu.
			// Ce qui MANQUAIT, c'etait la question inverse — « combien est
			// reellement visible » — et personne ne pouvait la poser. Elle existe
			// depuis aujourd'hui dans NKGui (`VisibleHeight`), ou elle sert a tous
			// les panneaux, pas au seul viewport.
			float32 h = ctx.VisibleHeight();
			if (h < 40.f)
				h = 40.f;
			const NkRect zone = ctx.NextItemRect(-1.f, h);
			const NkGuiId zoneId = ctx.GetId("vp_dropzone");
			ctx.ButtonBehavior(zoneId, zone);

			// Couleurs par JETONS de theme, jamais en dur (directive planches) :
			// fond le plus sombre du theme pour une zone en retrait, texte grise.
			ctx.DL().AddRectFilled(zone, ctx.theme.bgPrimary);

			// ── LA SCENE ECS, POSEE ICI ──────────────────────────────────────
			// La taille voulue est DECLAREE (le pont ne refait sa cible que si
			// elle change vraiment) ; le rendu, lui, a eu lieu bien avant, dans le
			// crochet preUI, frame device ouverte et passe backbuffer pas encore
			// commencee. UV Y inverse : les cibles de rendu ont leur origine en
			// bas a gauche (meme geste que NkAnimaEditor/Panels.h l.212).
			// La borne a 4096 RESTE, et ce n'est pas une ceinture de trop : une
			// cible de rendu est la seule chose ici dont une taille aberrante coute
			// des gigaoctets (5,6 Go mesures le 13/09 avant arret). Une garde qui
			// ne sert jamais est une garde qui n'a rien coute.
			const float32 vw = zone.w > 4096.f ? 4096.f : zone.w;
			const float32 vh = zone.h > 4096.f ? 4096.f : zone.h;
			if (vw > 1.f && vh > 1.f)
				NogeeViewport3DResize(static_cast<uint32>(vw), static_cast<uint32>(vh));
			const bool has3D = NogeeViewport3DReady();
			if (has3D) {
				ctx.DL().AddImage(kNogeeViewportTexId, zone, nkgui::NkVec2{0.f, 1.f}, nkgui::NkVec2{1.f, 0.f},
								  nkgui::NkColor{255, 255, 255, 255});
			}
			ctx.DL().AddRect(zone, ctx.theme.border, 1.f);
			if (!has3D) {
				TextAt(ctx, {zone.x + 12.f, zone.y + 10.f}, "zone de depot (type \"asset\")",
					   ctx.theme.textDisabled);
			} else {
				// Temoin numerique LISIBLE A L'ECRAN : le nombre d'entites que
				// NkRenderSystem peut soumettre, recompte par le pont avec le MEME
				// predicat. Un viewport qui montre du noir et « 0 » ne ment pas ;
				// un viewport qui montre du noir sans chiffre, si.
				char t[160];
				float32 yaw = 0.f, pitch = 0.f;
				NogeeViewport3DGetOrbit(&yaw, &pitch);
				std::snprintf(t, sizeof(t), "scene ECS : %d mesh soumis | camera %.0f/%.0f | image %d",
							  (int)NogeeViewport3DDrawCount(), (double)yaw, (double)pitch,
							  (int)NogeeViewport3DFrameCount());
				TextAt(ctx, {zone.x + 10.f, zone.y + zone.h - 20.f}, t, ctx.theme.textDisabled);
			}

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
