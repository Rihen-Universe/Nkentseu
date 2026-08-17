// =============================================================================
// Nogee/Panels/ViewportPanel.cpp — zone centrale, cible du glisser-deposer §9
// (cf. .h : ce panneau N'EST PAS un viewport, et il le dit a l'ecran).
// =============================================================================
#include "ViewportPanel.h"
#include "NKGui/NKGui.h"
#include "NKLogger/NkLog.h"
#include <cstdio>

namespace nkentseu {
	namespace noge {

		using namespace nkgui;

		void ViewportPanel::OnUI(editorkit::NkEditorFrameContext &ec) {
			NkGuiContext &ctx = ec.Ui();

			Text(ctx, "Viewport — rendu de scene : pas encore cable (ROADMAP §10sexies).");
			if (mLastDropPath[0] != '\0') {
				char line[300];
				std::snprintf(line, sizeof(line), "Dernier asset recu : %s (spawn non cable — journal)",
							  mLastDropPath);
				Text(ctx, line);
			} else {
				Text(ctx, "Glisser une carte du Content Browser ici : la livraison est journalisee.");
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
						std::snprintf(msg, sizeof(msg),
									  "[ViewportPanel] MESURE : charge 'asset' livree : '%s' — spawn non "
									  "cable (rendu de scene absent), journal seulement\n",
									  mLastDropPath);
						logger.Info(msg);
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
