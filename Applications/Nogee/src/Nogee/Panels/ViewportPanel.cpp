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

			// ── LA VUE EST LA ZONE DE REFERENCE, PAS UN ELEMENT DANS UN FLUX ──
			// Calibre sur NK3DModeler (NkModelerUI.h, NkLayout::Compute) : chez lui
			// tous les panneaux prennent une fraction bornee par un minimum, et
			// `view` recoit LE RESTE — `view = {leftW, y, W - leftW - rightW, midH}`.
			// La vue n'est pas un panneau parmi d'autres : c'est elle qui herite de
			// l'espace que les autres n'ont pas reclame.
			//
			// Ici, la part du dock revient au shell ; ce que ce panneau controle,
			// c'est ce qu'il fait de SON corps. Il le prend ENTIER (`VisibleRect`)
			// au lieu d'empiler deux lignes de texte puis de prendre le reste —
			// deux lignes qui coutaient ~52 px de hauteur d'image, mesure ci-dessous.
			// Le texte n'est pas perdu : il redescend EN INCRUSTATION dans la vue,
			// comme NK3DModeler dessine son ATH par-dessus sa scene. Une information
			// n'a pas besoin de prendre de la place pour etre lisible.
			const NkRect zone = ctx.VisibleRect();
			const NkGuiId zoneId = ctx.GetId("vp_dropzone");
			ctx.ButtonBehavior(zoneId, zone);
			// La cible du glisser-deposer est le RECT REEL de la vue : le curseur de
			// flux, lui, n'a plus servi a la poser. On le fait donc avancer a la
			// main, sinon les elements suivants (l'incrustation) s'empileraient en
			// haut du panneau au lieu de suivre la vue.
			ctx.layout.prevItem = zone;

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
			// L'ORIGINE DE LA VUE, DEPOSEE et non devinee. Sans elle, la souris
			// arrive en coordonnees FENETRE alors que l'image vit dans ce
			// rectangle-ci : un clic « au centre du cube » viserait un autre pixel,
			// d'autant plus loin que les panneaux de gauche sont larges. Meme geste
			// que NK3DModeler (`Demo3DHostSetView`, main.cpp l.1432).
			NogeeViewport3DSetView(zone.x, zone.y, zone.w, zone.h,
								   ctx.ItemHoverable(zone, zoneId));
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
				char t[220];
				float32 yaw = 0.f, pitch = 0.f;
				NogeeViewport3DGetOrbit(&yaw, &pitch);
				std::snprintf(t, sizeof(t), "scene ECS : %d mesh soumis | camera %.0f/%.0f | vue %dx%d | image %d",
							  (int)NogeeViewport3DDrawCount(), (double)yaw, (double)pitch, (int)zone.w,
							  (int)zone.h, (int)NogeeViewport3DFrameCount());
				TextAt(ctx, {zone.x + 10.f, zone.y + zone.h - 20.f}, t, ctx.theme.textDisabled);

				// ── L'ATH, EN INCRUSTATION : il informe sans couter de surface ──
				// C'est le geste de NK3DModeler, qui dessine ses reperes PAR-DESSUS
				// sa scene plutot qu'au-dessus d'elle. Les deux lignes qui vivaient
				// avant la vue lui prenaient ~52 px de hauteur ; ici elles n'en
				// prennent aucune.
				TextAt(ctx, {zone.x + 10.f, zone.y + 8.f},
					   mLastDropPath[0] != '\0'
						   ? (mSpawnCount > 0 ? "asset recu : entite instanciee (voir l'Outliner)"
											  : "asset recu : livre et journalise")
						   : (mWorld ? "Glisser un MESH du Content Browser dans la vue : il devient une entite."
									 : "Glisser une carte du Content Browser dans la vue."),
					   ctx.theme.textDisabled);
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
