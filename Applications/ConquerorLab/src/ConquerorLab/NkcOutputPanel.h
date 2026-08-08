#pragma once
// =============================================================================
// NkcOutputPanel — « Sortie » : ce que les modules du stagiaire ecrivent.
//
// POURQUOI CE PANNEAU EXISTE
// --------------------------
// Un module est lie statiquement a sa propre copie de Nkentseu : son `logger`
// n'est pas celui de l'atelier. Sans ce panneau, un stagiaire qui veut afficher
// un etat pour comprendre son bug ecrit dans le vide — et deboguer un moteur de
// regles a l'aveugle est le meilleur moyen de perdre une semaine.
//
// Le panneau « Modules » montre ce que dit le COMPILATEUR ; celui-ci montre ce
// que dit le CODE une fois qu'il tourne. Les deux sont necessaires, et les
// melanger rendrait les deux illisibles.
//
// LE PLUS RECENT EN HAUT — et pourquoi ce n'est pas un caprice
// ------------------------------------------------------------
// L'habitude, pour une console, c'est le plus recent EN BAS avec defilement
// automatique. NKGui n'expose aucun defilement programmatique : son etat de
// scroll vit dans `ctx.scrollVals`, indexe par identifiant interne. Aller l'y
// ecrire depuis un panneau serait un couplage a de la mecanique privee, qui
// casserait au premier remaniement de NKGui.
//
// On retourne donc le probleme au lieu de le forcer : le plus recent EN HAUT.
// La derniere ligne est alors toujours visible, sans defilement, sans etat a
// maintenir, et sans jamais arracher a l'utilisateur la ligne qu'il lisait. Le
// panneau Journal fait deja ce choix — l'atelier reste coherent.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"

#include "ConquerorLab/NkcModuleLog.h"
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcDraw.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		inline const char *NkcLogLevelName(NkcLogLevel l) noexcept {
			switch (l) {
				case NkcLogLevel::Trace: return "TRACE";
				case NkcLogLevel::Debug: return "DEBUG";
				case NkcLogLevel::Info:	 return "INFO";
				case NkcLogLevel::Warn:	 return "WARN";
				case NkcLogLevel::Error: return "ERREUR";
				case NkcLogLevel::Fatal: return "FATAL";
			}
			return "?";
		}

		/// Couleur par niveau. Les trois premiers restent sobres : si tout est
		/// colore, plus rien ne ressort, et c'est justement l'avertissement qu'on
		/// veut voir de loin.
		inline NkColor NkcLogLevelColor(NkcLogLevel l) noexcept {
			switch (l) {
				case NkcLogLevel::Trace: return NkcFade(NkcPalette::TextDim(), 0.6f);
				case NkcLogLevel::Debug: return NkcPalette::TextDim();
				case NkcLogLevel::Info:	 return NkcPalette::Text();
				case NkcLogLevel::Warn:	 return NkcPalette::Accent();
				case NkcLogLevel::Error:
				case NkcLogLevel::Fatal: return NkcPalette::Error();
			}
			return NkcPalette::Text();
		}

		class NkcOutputPanel : public NkEditorPanel {
			public:
				explicit NkcOutputPanel(NkcModuleLog *log) noexcept
					: NkEditorPanel("Sortie", NkEditorDockSide::NK_BOTTOM), mLog(log) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext &ctx = ec.Ui();
					if (!mLog) {
						Text(ctx, "Journal des modules indisponible.");
						return;
					}

					// ---- barre d'outils -------------------------------------
					if (Button(ctx, "Vider")) mLog->Clear();
					ctx.SameLine();
					if (BeginCombo(ctx, "Niveau minimal",
								   NkcLogLevelName(static_cast<NkcLogLevel>(mMinLevel)), 6)) {
						for (int32 i = 0; i <= 5; ++i)
							if (Selectable(ctx, NkcLogLevelName(static_cast<NkcLogLevel>(i)),
										   mMinLevel == i))
								mMinLevel = i;
						EndCombo(ctx);
					}

					// ---- compteurs ------------------------------------------
					const uint32 total	 = mLog->Total();
					const uint32 dropped = mLog->Dropped();
					char		 head[192];
					if (dropped > 0) {
						// Dire ce qu'on a jete, TOUJOURS. Un journal qui perd des
						// lignes en silence fait chercher un bug qui n'existe pas.
						std::snprintf(head, sizeof(head),
									  "%u lignes  —  %u perdues (tampon plein : %u lignes)",
									  static_cast<unsigned>(total), static_cast<unsigned>(dropped),
									  static_cast<unsigned>(kLogCapacity));
					} else {
						std::snprintf(head, sizeof(head), "%u lignes", static_cast<unsigned>(total));
					}
					NkcText(ctx, ctx.layout.cursor.x, ctx.layout.cursor.y, head,
							dropped > 0 ? NkcPalette::Accent() : NkcPalette::TextDim(), 0.f);
					ctx.layout.cursor.y += NkcLineH(ctx) + ctx.S(4.f);
					Separator(ctx);

					if (total == 0) {
						Text(ctx, "Rien pour l'instant.");
						Text(ctx, "Dans votre module : NKC_LOG_INFO(\"...\") — ou logger.Infof(...)");
						Text(ctx, "apres avoir ecrit NKC_MODULE_LOGGING(rules) une fois.");
						return;
					}

					// ---- lignes ---------------------------------------------
					const float32 lineH = NkcLineH(ctx) + ctx.S(3.f);
					NkGuiDrawList &dl	= ctx.DL();

					mLog->ForEachNewestFirst([&](uint32, const NkcLogLine &l) {
						if (static_cast<int32>(l.level) < mMinLevel) return;

						const NkRect  row = ctx.NextItemRect(0.f, lineH);
						const NkColor col = NkcLogLevelColor(l.level);

						// Filet de niveau a gauche : lisible en un coup d'oeil,
						// et lisible aussi en niveaux de gris.
						if (l.level >= NkcLogLevel::Warn)
							dl.AddRectFilled({row.x, row.y, ctx.S(3.f), row.h}, col, 1.f);

						char line[512];
						if (l.repeat > 1) {
							std::snprintf(line, sizeof(line), "[%s] %s  (x%u)", l.module, l.text,
										  static_cast<unsigned>(l.repeat));
						} else {
							std::snprintf(line, sizeof(line), "[%s] %s", l.module, l.text);
						}
						NkcText(ctx, row.x + ctx.S(10.f), row.y + ctx.S(1.f), line, col,
								row.w - ctx.S(14.f));
					});
				}

			private:
				NkcModuleLog *mLog		= nullptr;
				int32		  mMinLevel = 0;
		};

	} // namespace conqueror
} // namespace nkentseu
