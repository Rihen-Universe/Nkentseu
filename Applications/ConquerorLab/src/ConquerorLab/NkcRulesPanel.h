#pragma once
// =============================================================================
// NkcRulesPanel — panneau ENTIEREMENT auto-genere depuis `GetParamsSchemaJson`.
//
// Il n'y a pas une ligne d'interface par parametre, et c'est le point : quand le
// stagiaire A1 ajoute `fusion_exige_case_libre` a son schema, le reglage
// apparait ici sans qu'on recompile l'atelier. C'est la condition pratique de la
// regle de travail du projet — « toute valeur numerique est un parametre nomme,
// modifiable sans recompilation » (REGLES §1).
//
// Correspondance type -> widget (HANDOFF §b) :
//   int   -> champ numerique (glisser, ou -/+ quand la plage est etroite)
//   bool  -> case a cocher
//   enum  -> liste deroulante
// groupe par le champ "group", dans l'ordre ou le module les declare : c'est lui
// qui sait ce qui compte le plus.
//
// AUCUN ETAT LOCAL DE VALEUR. Le schema est relu a chaque frame et la valeur
// affichee vient du MOTEUR. Consequence voulue : quand le module borne une
// valeur hors plage (le banc d'essai le verifie), l'interface montre la valeur
// bornee, pas celle qu'on a tapee. Un cache local mentirait.
// =============================================================================

#include "NKEditorKit/NkEditorKit.h"

#include "ConquerorLab/NkcSession.h"
#include "ConquerorLab/NkcParamSchema.h"
#include "ConquerorLab/NkcLabTheme.h"
#include "ConquerorLab/NkcDraw.h"

#include <cstdio>

namespace nkentseu {
	namespace conqueror {

		using namespace nkentseu::editorkit;
		using namespace nkentseu::nkgui;

		class NkcRulesPanel : public NkEditorPanel {
			public:
				explicit NkcRulesPanel(NkcSession *s) noexcept
					: NkEditorPanel("Regles", NkEditorDockSide::NK_RIGHT), mS(s) {}

				void OnUI(NkEditorFrameContext &ec) override {
					NkGuiContext &ctx = ec.Ui();
					if (!mS || !mS->Ready()) {
						Text(ctx, "Aucun moteur de regles charge.");
						return;
					}

					const char *json = mS->ParamsSchemaJson();
					if (!NkcParseParamsSchema(json, mParams)) {
						Text(ctx, "Le module n'expose aucun parametre exploitable.");
						Separator(ctx);
						TextWrapped(ctx, json && *json ? json : "(schema vide)");
						return;
					}
					NkcCollectGroups(mParams, mGroups);

					// Bandeau d'avertissement : un parametre modifie en cours de
					// partie ne rejoue pas les coups deja joues. Le dire ici evite
					// de croire a une mesure faite sur deux regles differentes.
					if (mS->ParamsTouched()) {
						const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
						ctx.DL().AddRectFilled(r, NkcFade(NkcPalette::Warn(), 0.22f), ctx.theme.rounding);
						ctx.DL().AddRect(r, NkcPalette::Warn(), 1.f);
						NkcTextCenter(ctx, r, "Reglage modifie — relance la partie pour mesurer",
									  NkcPalette::Text());
					}

					if (Button(ctx, "Relancer la partie")) mS->NewGame();
					ctx.SameLine();
					if (Button(ctx, "Valeurs par defaut")) mS->ResetRulesInstance();
					Separator(ctx);

					for (usize g = 0; g < mGroups.Size(); ++g) {
						if (!CollapsingHeader(ctx, mGroups[g].CStr())) continue;
						for (usize i = 0; i < mParams.Size(); ++i) {
							if (mParams[i].group != mGroups[g]) continue;
							DrawParam(ctx, mParams[i]);
						}
					}
				}

			private:
				void DrawParam(NkGuiContext &ctx, const NkcParam &p) noexcept {
					// Identite basee sur la CLE, jamais sur le libelle : deux
					// parametres peuvent partager un libelle, et un libelle peut
					// changer d'une version du module a l'autre.
					ctx.PushId(p.key.CStr());

					switch (p.type) {
						case NkcParamType::Bool: {
							bool b = p.val != 0;
							if (Checkbox(ctx, p.label.CStr(), b))
								mS->SetParam(p.key.CStr(), b ? 1.0 : 0.0);
							break;
						}
						case NkcParamType::Enum: {
							const int32 sel = (p.val >= 0 && p.val < static_cast<int32>(p.values.Size()))
												  ? p.val
												  : 0;
							const char *preview = p.values.Empty() ? "?" : p.values[static_cast<usize>(sel)].CStr();
							if (BeginCombo(ctx, p.label.CStr(), preview,
										   static_cast<int32>(p.values.Size()))) {
								for (usize k = 0; k < p.values.Size(); ++k)
									if (Selectable(ctx, p.values[k].CStr(), static_cast<int32>(k) == sel)) {
										mS->SetParam(p.key.CStr(), static_cast<float64>(k));
										ctx.ClosePopup();
									}
								EndCombo(ctx);
							}
							break;
						}
						default: {
							int32		 v	   = p.val;
							const int32	 span  = p.mx - p.mn;
							// Plage etroite : le glissement au pixel n'y arrive pas
							// (un delta de 0,3 s'arrondit a zero). On donne les
							// boutons -/+, qui eux avancent toujours d'un cran.
							const bool	 fine  = span > 0 && span <= 24;
							bool		 chg   = false;
							if (fine) {
								chg = InputInt(ctx, p.label.CStr(), v, 1, p.mn, p.mx);
							} else {
								const float32 speed =
									span > 0 ? static_cast<float32>(span) / 220.f : 0.25f;
								chg = DragInt(ctx, p.label.CStr(), v, speed > 0.02f ? speed : 0.02f,
											  p.mn, p.mx);
							}
							if (chg) mS->SetParam(p.key.CStr(), static_cast<float64>(v));
							break;
						}
					}

					// Plage + defaut en infobulle : le schema les porte, autant les
					// rendre lisibles sans encombrer la ligne.
					if (ctx.IsItemHovered()) {
						char tip[256];
						if (!p.help.Empty())
							std::snprintf(tip, sizeof(tip), "%s\n%s   [%d..%d]  defaut %d", p.help.CStr(),
										  p.key.CStr(), p.mn, p.mx, p.def);
						else
							std::snprintf(tip, sizeof(tip), "%s   [%d..%d]  defaut %d", p.key.CStr(), p.mn,
										  p.mx, p.def);
						SetTooltip(ctx, tip);
					}

					ctx.PopId();
				}

			private:
				NkcSession		  *mS = nullptr;
				NkVector<NkcParam> mParams;
				NkVector<NkString> mGroups;
		};

	} // namespace conqueror
} // namespace nkentseu
