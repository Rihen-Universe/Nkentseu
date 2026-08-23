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

					DrawBoardLibrary(ctx);
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
				// -------------------------------------------------------------
				// LES GRILLES SONT DES DONNEES (REGLES §4). On ne les compile pas :
				// on depose un .json dans un dossier et il apparait ici. L'atelier
				// ne fait que passer la chaine a `LoadBoardJson` — c'est le MODULE
				// qui la lit, donc un moteur de stagiaire qui accepte un champ de
				// plus le verra sans qu'on touche a ce panneau.
				// -------------------------------------------------------------
				void DrawBoardLibrary(NkGuiContext &ctx) noexcept {
					NkcBoardLibrary				 &lib   = mS->Boards();
					const NkVector<NkcBoardFile> &files = lib.Files();

					Text(ctx, "Plateau");

					// `libelle`, pas `name` : ce qui se LIT ici doit decrire ce qu'on
					// verra a l'ecran. Le nom de FICHIER reste dans `name`, et c'est
					// lui que citent les messages qui servent a retrouver le fichier
					// sur le disque.
					const char *preview = (mBoardSel >= 0 && static_cast<usize>(mBoardSel) < files.Size())
											  ? files[static_cast<usize>(mBoardSel)].libelle.CStr()
											  : (files.Empty() ? "(aucun fichier)" : "(choisir)");
					if (BeginCombo(ctx, "Grille", preview, static_cast<int32>(files.Size()))) {
						for (usize i = 0; i < files.Size(); ++i)
							if (Selectable(ctx, files[i].libelle.CStr(), static_cast<int32>(i) == mBoardSel)) {
								mBoardSel = static_cast<int32>(i);
								mS->LoadBoard(i);
								ctx.ClosePopup();
							}
						EndCombo(ctx);
					}

					// FORME DES CELLULES — presentation seule. Elle ne touche ni au
					// voisinage, ni aux coups legaux, ni au resultat : c'est
					// exactement pour cela qu'elle a sa place ici et pas dans le
					// contrat. Deux plateaux identiques dessines en carres et en
					// pastilles se LISENT tres differemment, et savoir lequel se lit
					// le mieux est une vraie question de conception.
					{
						const NkcCellShape cur = mS->ShapeOverride();
						const char *lbl = (cur == NkcCellShape::Auto)
											  ? "Selon le plateau"
											  : NkcCellShapeName(cur);
						if (BeginCombo(ctx, "Forme des cellules", lbl, 4)) {
							for (int32 i = 0; i <= 3; ++i) {
								const NkcCellShape s = static_cast<NkcCellShape>(i);
								const char *n = (s == NkcCellShape::Auto) ? "Selon le plateau"
																		  : NkcCellShapeName(s);
								if (Selectable(ctx, n, s == cur)) {
									mS->SetShapeOverride(s);
									ctx.ClosePopup();
								}
							}
							EndCombo(ctx);
						}
					}

					if (Button(ctx, "Rafraichir")) lib.Refresh();
					ctx.SameLine();
					// Exporter le plateau courant : le point de depart naturel pour
					// fabriquer une variante — on part d'un fichier forcement valide
					// plutot que d'un format decrit dans une doc.
					if (Button(ctx, "Exporter le plateau courant"))
						mS->ExportBoard("plateau_exporte.json");

					char buf[512];
					std::snprintf(buf, sizeof(buf), "Depose tes .json ici : %s", lib.Dir().CStr());
					Text(ctx, buf);

					if (!lib.Message().Empty()) {
						const NkRect r = ctx.NextItemRect(0.f, ctx.ItemHeight());
						// La gravite VIENT de la bibliotheque. Elle etait devinee ici en
						// cherchant des mots dans le texte, sensible a la casse : le
						// message « Fichier ILLISIBLE » ne contenait pas « illisible »
						// et s'affichait donc en VERT, comme une reussite.
						NkColor teinte = NkcPalette::Ok();
						switch (lib.MessageKind()) {
							case NkcMsgKind::Erreur:		teinte = NkcPalette::Error(); break;
							case NkcMsgKind::Avertissement: teinte = NkcPalette::Warn(); break;
							case NkcMsgKind::Info:			teinte = NkcPalette::Border(); break;
							default:						teinte = NkcPalette::Ok(); break;
						}
						ctx.DL().AddRectFilled(r, NkcFade(teinte, 0.22f), ctx.theme.rounding);
						NkcTextCenter(ctx, r, lib.Message().CStr(), NkcPalette::Text());
					}
				}

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
				int32			   mBoardSel = -1;
		};

	} // namespace conqueror
} // namespace nkentseu
