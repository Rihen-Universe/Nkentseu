// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurScriptsUi.cpp
// DESCRIPTION: Le bloc « Scripts » des Details, les creations du navigateur, et
//              l'ouverture d'un script par double-clic.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurScriptsUi.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurInterface.h"
#include "Script/NkEditeurGraphe.h"
#include "Script/NkEditeurScripts.h"

#include "NKFileSystem/NkFile.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		namespace {
			bool FinitPar(const char *s, const char *fin) {
				const usize a = std::strlen(s), b = std::strlen(fin);
				if (b > a) {
					return false;
				}
				for (usize i = 0; i < b; ++i) {
					char x = s[a - b + i], y = fin[i];
					x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
					if (x != y) {
						return false;
					}
				}
				return true;
			}
			/// Le fichier source qui declare la classe `classe` (« cpp:Porte »).
			NkString SourceDe(NkEditeurScripts &s, const char *ref) {
				const char *nom = std::strncmp(ref, unkeny::NK_SCRIPT_PREFIXE_CPP, 4) == 0 ? ref + 4 : ref;
				for (uint32 i = 0; i < s.sources.Size(); ++i) {
					const NkString texte = NkFile::ReadAllText(s.sources[i].CStr());
					NkVector<NkString> classes;
					NkEditeurScriptsClassesDe(texte.CStr(), classes);
					for (uint32 k = 0; k < classes.Size(); ++k) {
						if (classes[k] == nom) {
							return s.sources[i];
						}
					}
				}
				return NkString();
			}
			/// Le dossier du navigateur ou creer : le dossier courant du Contenu ;
			/// a la racine, « Contenu/Scripts ».
			NkString DossierDeCreation(NkEditeurInterface &ui) {
				if (ui.contenuProjet && !ui.contenuDossier.Empty()) {
					return NkString(NK_CONTENU_RACINE) + "/" + ui.contenuDossier;
				}
				return NkString(NK_SCRIPTS_DOSSIER);
			}
			/// Une variable que la definition d'un script DECLARE.
			struct VarDeclaree {
					NkString nom;
					unkeny::NkTypeVarScript type = unkeny::NkTypeVarScript::NK_REEL;
					NkVec2f defaut{0.f, 0.f};
			};
			void Declarees(const unkeny::NkDefinitionScript *d, NkVector<VarDeclaree> &sortie) {
				if (d == nullptr) {
					return;
				}
				if (d->classe != nullptr) {
					for (uint32 i = 0; i < d->classe->nbVariables; ++i) {
						VarDeclaree v;
						v.nom = d->classe->variables[i].nom;
						v.type = static_cast<unkeny::NkTypeVarScript>(d->classe->variables[i].type);
						v.defaut = NkVec2f(d->classe->variables[i].x, d->classe->variables[i].y);
						sortie.PushBack(v);
					}
				} else if (d->programme != nullptr) {
					const unkeny::NkModuleBp &m = d->programme->module;
					for (uint32 i = 0; i < m.variables.Size(); ++i) {
						const unkeny::NkVariableBp &x = m.variables[i];
						if (!x.exposee) {
							continue;
						}
						VarDeclaree v;
						v.nom = x.nom;
						v.defaut = NkVec2f(x.defaut.x, x.defaut.y);
						if (x.type == unkeny::NkTypeBp::NK_ENTIER) {
							v.type = unkeny::NkTypeVarScript::NK_ENTIER;
							v.defaut = NkVec2f(static_cast<float32>(x.defaut.i), 0.f);
						} else if (x.type == unkeny::NkTypeBp::NK_BOOLEEN) {
							v.type = unkeny::NkTypeVarScript::NK_BOOLEEN;
							v.defaut = NkVec2f(x.defaut.i != 0 ? 1.f : 0.f, 0.f);
						} else if (x.type == unkeny::NkTypeBp::NK_VEC2) {
							v.type = unkeny::NkTypeVarScript::NK_VEC2;
						}
						sortie.PushBack(v);
					}
				}
			}

			const char *Statut(NkEditeurScripts &s, ecs::NkEntityId id, uint32 k, const char *ref, NkString &detail) {
				const unkeny::NkDefinitionScript *d = s.registre.Definition(s.registre.Trouver(ref));
				if (d == nullptr) {
					detail = std::strncmp(ref, unkeny::NK_SCRIPT_PREFIXE_CPP, 4) == 0 ? "classe C++ pas (encore) compilée"
																						: "Blueprint introuvable dans le projet";
					return "inconnu";
				}
				if (!d->erreur.Empty()) {
					detail = d->erreur;
					return "refusé";
				}
				if (s.hote.EnFaute(id, k)) {
					detail = "voir le Journal ; Arrêter puis Jouer le relance";
					return "EN FAUTE";
				}
				return d->genre == unkeny::NkGenreScript::NK_CPP ? "C++" : "Blueprint";
			}
		} // namespace

		void NkEditeurBlocScript(NkEditeurCadre &c, ecs::NkEntityId id) {
			if (c.m.scripts == nullptr) {
				return;
			}
			NkEditeurScripts &s = *c.m.scripts;
			NkEditeurModele &m = c.m;
			nkgui::NkGuiContext &ctx = c.ctx;
			ctx.PushId("scripts");
			nkgui::Separator(ctx);
			nkgui::Text(ctx, "Scripts (exécutés dans cet ordre)");
			unkeny::NkScript2D *sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
			if (sc == nullptr || sc->nombre == 0u) {
				nkgui::TextWrapped(ctx, "Aucun script. Ajoutez un Blueprint ou une classe C++ ci-dessous ; "
										"créez-en un par « Contenu > + Ajouter > Script C++ / Blueprint ».");
			}
			for (uint32 k = 0; sc != nullptr && k < sc->nombre; ++k) {
				ctx.PushId(sc->refs[k]);
				NkString detail;
				const char *statut = Statut(s, id, k, sc->refs[k], detail);
				nkgui::Text(ctx, NkString::Format("%u. %s  [%s]", static_cast<unsigned>(k + 1u), sc->refs[k], statut).CStr());
				if (!detail.Empty()) {
					nkgui::TextWrapped(ctx, detail.CStr());
				}
				bool actif = sc->actifs[k];
				if (nkgui::Checkbox(ctx, "actif", actif)) {
					NkEditeurRetenir(m);
					sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
					if (sc == nullptr) {
						ctx.PopId();
						break;
					}
					sc->actifs[k] = actif;
				}
				ctx.SameLine();
				int32 geste = 0;
				ctx.BeginDisabled(k == 0u);
				if (nkgui::Button(ctx, "Monter")) {
					geste = 1;
				}
				ctx.EndDisabled();
				ctx.SameLine();
				ctx.BeginDisabled(k + 1u >= sc->nombre);
				if (nkgui::Button(ctx, "Descendre")) {
					geste = 2;
				}
				ctx.EndDisabled();
				ctx.SameLine();
				if (nkgui::Button(ctx, "Ouvrir")) {
					geste = 3;
				}
				ctx.SameLine();
				if (nkgui::Button(ctx, "Retirer")) {
					geste = 4;
				}
				// Les VARIABLES de ce script (sauvees par nom, modifiables en jeu) : celles
				// que la DEFINITION declare (classe C++ ou module Blueprint), avec leur
				// defaut tant que le composant ne les porte pas -- une valeur n'est
				// ecrite dans l'entite que si on la change -- puis celles du composant
				// que la definition ne connait plus (gardees, sauvees).
				NkVector<VarDeclaree> decl;
				Declarees(s.registre.Definition(s.registre.Trouver(sc->refs[k])), decl);
				for (uint32 v = 0; v < unkeny::NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
					const unkeny::NkVarScript &x = sc->vars[v];
					bool connue = false;
					for (uint32 q = 0; q < decl.Size(); ++q) {
						connue = connue || decl[q].nom == x.nom;
					}
					if (x.nom[0] != '\0' && x.script == k && !connue) {
						VarDeclaree d;
						d.nom = x.nom;
						d.type = static_cast<unkeny::NkTypeVarScript>(x.type);
						decl.PushBack(d);
					}
				}
				for (uint32 q = 0; q < decl.Size() && geste == 0; ++q) {
					const VarDeclaree &d = decl[q];
					const unkeny::NkVarScript *x = unkeny::NkScriptVariable(*sc, k, d.nom.CStr());
					NkVec2f val = x != nullptr ? x->valeur : d.defaut;
					bool change = false;
					ctx.PushId(d.nom.CStr());
					switch (d.type) {
						case unkeny::NkTypeVarScript::NK_ENTIER: {
							int32 i = static_cast<int32>(val.x);
							if (nkgui::DragInt(ctx, d.nom.CStr(), i)) {
								val.x = static_cast<float32>(i);
								change = true;
							}
							break;
						}
						case unkeny::NkTypeVarScript::NK_BOOLEEN: {
							bool b = val.x != 0.f;
							if (nkgui::Checkbox(ctx, d.nom.CStr(), b)) {
								val.x = b ? 1.f : 0.f;
								change = true;
							}
							break;
						}
						case unkeny::NkTypeVarScript::NK_VEC2: {
							change = nkgui::DragFloat(ctx, NkString::Format("%s.x", d.nom.CStr()).CStr(), val.x, 0.05f) || change;
							change = nkgui::DragFloat(ctx, NkString::Format("%s.y", d.nom.CStr()).CStr(), val.y, 0.05f) || change;
							break;
						}
						default:
							change = nkgui::DragFloat(ctx, d.nom.CStr(), val.x, 0.05f);
							break;
					}
					ctx.PopId();
					if (change) {
						unkeny::NkScriptPoserVariable(*sc, k, d.nom.CStr(), d.type, val);
					}
				}
				ctx.PopId();
				if (geste == 1 || geste == 2) {
					NkEditeurRetenir(m);
					sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
					if (sc != nullptr) {
						unkeny::NkScriptEchanger(*sc, k, geste == 1 ? k - 1u : k + 1u);
					}
					break;
				}
				if (geste == 3) {
					const NkString ref(sc->refs[k]);
					if (std::strncmp(ref.CStr(), unkeny::NK_SCRIPT_PREFIXE_CPP, 4) == 0) {
						const NkString source = SourceDe(s, ref.CStr());
						if (source.Empty() || (s.ouvrirTexteExterne && !NkEditeurOuvrirTexteExterne(source.CStr()))) {
							NkEditeurAnnoncer(m, NkString::Format("%s : source introuvable dans le Contenu", ref.CStr()).CStr());
						}
					} else {
						NkEditeurOuvrirGraphe(s, m, (s.projet + ref).CStr());
					}
					break;
				}
				if (geste == 4) {
					NkEditeurRetenir(m);
					sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
					if (sc != nullptr) {
						unkeny::NkScriptRetirer(*sc, k);
					}
					break;
				}
			}
			// ── Ajouter un script : les Blueprints du projet et les classes C++ ──
			NkVector<NkString> proposes;
			NkEditeurScriptsProposes(s, proposes);
			nkgui::Text(ctx, "Ajouter un script :");
			uint32 montres = 0;
			for (uint32 i = 0; i < proposes.Size(); ++i) {
				sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
				if (sc != nullptr && unkeny::NkScriptTrouver(*sc, proposes[i].CStr()) >= 0) {
					continue; // deja sur l'entite
				}
				++montres;
				ctx.PushId(proposes[i].CStr());
				if (nkgui::Button(ctx, NkString::Format("+ %s", proposes[i].CStr()).CStr())) {
					NkEditeurRetenir(m);
					if (!m.scene.Monde().Has<unkeny::NkScript2D>(id)) {
						m.scene.Monde().Add<unkeny::NkScript2D>(id, unkeny::NkScript2D());
					}
					sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
					if (sc == nullptr || unkeny::NkScriptAjouter(*sc, proposes[i].CStr()) < 0) {
						NkEditeurAnnoncer(m, "Script : plus de place (4 au plus) ou nom trop long");
					} else {
						// Les variables exposees d'une classe C++ prennent leur defaut.
						const unkeny::NkDefinitionScript *d = s.registre.Definition(s.registre.Trouver(proposes[i].CStr()));
						if (d != nullptr && d->classe != nullptr) {
							for (uint32 v = 0; v < d->classe->nbVariables; ++v) {
								const NkUnkVariableV1 &x = d->classe->variables[v];
								unkeny::NkScriptPoserVariable(*sc, static_cast<uint32>(sc->nombre - 1u), x.nom,
															  static_cast<unkeny::NkTypeVarScript>(x.type), NkVec2f(x.x, x.y));
							}
						}
						NkEditeurAnnoncer(m, NkString::Format("Script ajouté : %s", proposes[i].CStr()).CStr());
					}
				}
				ctx.PopId();
			}
			if (montres == 0u) {
				nkgui::TextWrapped(ctx, proposes.Empty() ? "Aucun script dans le projet : Contenu > + Ajouter > Script C++ ou Blueprint."
														 : "Tous les scripts du projet sont déjà sur cette entité.");
			}
			// L'etat de la compilation C++.
			if (s.etat == NkEtatCompilation::NK_EN_COURS) {
				nkgui::Text(ctx, "C++ : compilation en cours…");
			} else if (s.etat == NkEtatCompilation::NK_ECHOUEE) {
				nkgui::TextWrapped(ctx, NkString::Format("C++ : la dernière compilation a échoué (%u erreur(s), voir le Journal)",
														 static_cast<unsigned>(s.erreurs.Size()))
											.CStr());
			}
			ctx.PopId();
		}

		void NkEditeurActionScript(NkEditeurCadre &c, int32 action) {
			if (c.m.scripts == nullptr) {
				return;
			}
			NkEditeurScripts &s = *c.m.scripts;
			NkEditeurModele &m = c.m;
			switch (action - NK_A_SCRIPT) {
				case NK_SCRIPT_NOUVEAU_CPP: {
					const NkString cree = NkEditeurNouveauScriptCpp(m, DossierDeCreation(c.ui).CStr());
					if (!cree.Empty()) {
						c.ui.contenuPerime = true;
						if (s.ouvrirTexteExterne) {
							NkEditeurOuvrirTexteExterne(NkEditeurCheminContenu(m, cree.CStr()).CStr());
						}
						s.ageReleve = 99.f; // compile tout de suite (le releve le voit)
					}
					break;
				}
				case NK_SCRIPT_NOUVEAU_BP: {
					const NkString cree = NkEditeurNouveauBlueprint(m, DossierDeCreation(c.ui).CStr());
					if (!cree.Empty()) {
						c.ui.contenuPerime = true;
						NkEditeurOuvrirGraphe(s, m, NkEditeurCheminContenu(m, cree.CStr()).CStr());
						s.ageReleve = 99.f;
					}
					break;
				}
				case NK_SCRIPT_COMPILER:
					NkEditeurScriptsReleverCpp(s, m);
					NkEditeurScriptsCompiler(s, m);
					break;
				default:
					break;
			}
		}

		bool NkEditeurScriptOuvrirAsset(NkEditeurCadre &c, const char *cheminNav) {
			if (c.m.scripts == nullptr || cheminNav == nullptr) {
				return false;
			}
			const NkString abs = NkEditeurCheminContenu(c.m, cheminNav);
			if (FinitPar(cheminNav, ".nkbp")) {
				NkEditeurOuvrirGraphe(*c.m.scripts, c.m, abs.CStr());
				return true;
			}
			if (FinitPar(cheminNav, ".cpp") || FinitPar(cheminNav, ".h") || FinitPar(cheminNav, ".hpp")) {
				if (c.m.scripts->ouvrirTexteExterne && !NkEditeurOuvrirTexteExterne(abs.CStr())) {
					NkEditeurAnnoncer(c.m, "Aucun éditeur de texte n'a pu être lancé pour ce script");
				}
				return true;
			}
			return false;
		}

	} // namespace editeur
} // namespace nkentseu
