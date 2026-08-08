#pragma once
// =============================================================================
// NkcModulesPanel — le tableau de bord du stagiaire.
//
// C'EST LE SEUL RETOUR QU'IL AURA. Il n'ouvre pas de terminal, ne lance pas de
// build : il ecrit un `.cpp`, il sauvegarde, et c'est ICI qu'il apprend si son
// module compile, se charge, et repond au contrat. La sortie du compilateur est
// donc affichee INTEGRALEMENT, en monospace, defilable — pas resumee en
// « erreur de compilation », qui ne se debugge pas.
//
// Sur Android et Web, il n'y a pas de compilateur sur l'appareil : seuls les
// modules compiles DANS l'application existent. Le panneau le dit franchement
// plutot que d'afficher une liste vide qu'on prendrait pour une panne.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"

#include "ConquerorLab/NkcSession.h"
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcDraw.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkcModulesPanel : public NkEditorPanel {
			public:
				explicit NkcModulesPanel(NkcSession *s) noexcept
					: NkEditorPanel("Modules", NkEditorDockSide::NK_BOTTOM), mS(s) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext  &ctx	= ec.Ui();
					NkcModuleHost *host = mS ? mS->Host() : nullptr;
					if (!host) {
						Text(ctx, "Catalogue indisponible.");
						return;
					}

					DrawHeader(ctx, *host);

					// INCOHERENCE DU MOTEUR COURANT — en haut, en rouge, avant tout
					// le reste. C'est une contradiction interne au module : elle ne
					// provoque aucune erreur, elle rend juste l'IA silencieusement
					// plus faible. Si on ne la crie pas ici, personne ne la voit.
					if (mS) {
						const char *inc = mS->Coherence();
						if (inc && *inc) {
							const float32 h = NkcLineH(ctx) * 3.f + ctx.S(14.f);
							const NkRect  r = ctx.NextItemRect(0.f, h);
							ctx.DL().AddRectFilled(r, NkcFade(NkcPalette::Error(), 0.20f),
												   ctx.theme.rounding);
							ctx.DL().AddRectFilled({r.x, r.y, ctx.S(3.f), r.h},
												   NkcPalette::Error(), 1.f);
							NkcText(ctx, r.x + ctx.S(10.f), r.y + ctx.S(5.f), inc,
									NkcPalette::Text(), r.w - ctx.S(16.f));
						}
					}

					Separator(ctx);

					Text(ctx, "Moteurs de regles");
					const NkVector<NkcRulesEntry> &rules = host->Rules();
					for (usize i = 0; i < rules.Size(); ++i) {
						ctx.PushId(&rules[i]);
						const bool selected = (mS->RulesIndex() == i);
						if (DrawEntry(ctx, rules[i], selected, true) && rules[i].Usable())
							mS->UseRules(i);
						ctx.PopId();
					}

					Separator(ctx);
					Text(ctx, "Intelligences artificielles");
					const NkVector<NkcAIEntry> &ais = host->Ais();
					for (usize i = 0; i < ais.Size(); ++i) {
						ctx.PushId(&ais[i]);
						DrawEntry(ctx, ais[i], false, false);
						ctx.PopId();
					}
				}

			private:
				void DrawHeader(NkGuiContext &ctx, NkcModuleHost &host) noexcept {
					char buf[512];

					if (!NkcModuleHost::SupportsDynamic()) {
						const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(r, NkcFade(NkcPalette::Warn(), 0.22f), ctx.theme.rounding);
						NkcTextCenter(ctx, r,
									  "Pas de compilateur sur cette plateforme — seuls les modules "
									  "internes sont disponibles.",
									  NkcPalette::Text());
						return;
					}

					if (Button(ctx, "Rechercher et compiler")) mS->ScanModules();
					ctx.SameLine();
					bool autoScan = mS->AutoScan();
					if (Checkbox(ctx, "Surveiller les dossiers", autoScan)) mS->SetAutoScan(autoScan);

					if (host.CanCompile())
						std::snprintf(buf, sizeof(buf), "Compilateur : %s", host.CompilerPath().CStr());
					else
						std::snprintf(buf, sizeof(buf),
									  "AUCUN compilateur trouve — pose NK_CXX sur le chemin de clang++.");
					Text(ctx, buf);
					if (!host.CanCompile()) return;

					// Les chemins EN CLAIR : « ou est-ce que je pose mon fichier ? »
					// est la premiere question, et elle ne doit pas se poser deux fois.
					std::snprintf(buf, sizeof(buf), "Regles : %s", host.RulesDir().CStr());
					Text(ctx, buf);
					std::snprintf(buf, sizeof(buf), "IA     : %s", host.AiDir().CStr());
					Text(ctx, buf);
				}

				/// Une entree : pastille d'etat, libelle, source, et le log s'il y a
				/// quelque chose a lire. Renvoie true si la ligne a ete cliquee.
				bool DrawEntry(NkGuiContext &ctx, const NkcModuleEntry &e, bool selected,
							   bool selectable) noexcept {
					NkColor		tint;
					const char *state;
					switch (e.status) {
						case NkcModuleStatus::Builtin:		tint = NkcPalette::Accent();	state = "interne";	break;
						case NkcModuleStatus::Ready:		tint = NkcPalette::Ok();		state = "pret";		break;
						case NkcModuleStatus::CompileError: tint = NkcPalette::Error();		state = "erreur de compilation"; break;
						default:							tint = NkcPalette::Warn();		state = "refuse au chargement";	 break;
					}

					const float32 h	  = ctx.ItemHeight();
					const NkRect  row = ctx.NextItemRect(0.f, h);
					NkGuiDrawList &dl = ctx.DL();

					const bool hovered = selectable && ctx.InputHits(row) && ctx.popupDepth == 0;
					if (selected)	   dl.AddRectFilled(row, NkcFade(NkcPalette::Accent(), 0.22f), ctx.theme.rounding);
					else if (hovered)  dl.AddRectFilled(row, NkcPalette::ButtonHover(), ctx.theme.rounding);

					dl.AddRectFilled({row.x + ctx.S(4.f), row.y + ctx.S(6.f), ctx.S(4.f), h - ctx.S(12.f)},
									 tint, 2.f);

					char buf[320];
					std::snprintf(buf, sizeof(buf), "%s   —   %s", e.label.CStr(), state);
					NkcText(ctx, row.x + ctx.S(16.f), row.y + (h - NkcLineH(ctx)) * 0.5f, buf,
							e.Usable() ? NkcPalette::Text() : NkcPalette::TextDim(), row.w - ctx.S(24.f));

					if (!e.sourcePath.Empty()) {
						std::snprintf(buf, sizeof(buf), "    %s", e.sourcePath.CStr());
						Text(ctx, buf);
					}

					// Le log n'est deplie que quand il porte une INFORMATION : afficher
					// « Compile et charge. » sous chaque module noierait les erreurs.
					const bool isError = (e.status == NkcModuleStatus::CompileError ||
										  e.status == NkcModuleStatus::LoadError);
					if (isError && !e.log.Empty()) {
						const float32 lines = 8.f;
						const NkRect  box	= ctx.NextItemRect(0.f, NkcLineH(ctx) * lines + ctx.S(12.f));
						dl.AddRectFilled(box, NkcPalette::Track(), ctx.theme.rounding);
						dl.AddRect(box, NkcPalette::Error(), 1.f);
						if (BeginChild(ctx, "log", {box.x + ctx.S(6.f), box.y + ctx.S(6.f),
													box.w - ctx.S(12.f), box.h - ctx.S(12.f)},
									   false, true)) {
							TextWrapped(ctx, e.log.CStr(), box.w - ctx.S(24.f));
							EndChild(ctx);
						}
					}

					return selectable && ctx.ClickIn(row);
				}

			private:
				NkcSession *mS = nullptr;
		};

	} // namespace conqueror
} // namespace nkentseu
