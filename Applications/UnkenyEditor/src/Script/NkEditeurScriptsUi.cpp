// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurScriptsUi.cpp
// DESCRIPTION: Ce que montre la carte « Scripts » des Details (statut, variables,
//              ajouter, ouvrir), les creations du navigateur, et l'ouverture
//              d'un script par double-clic.
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
		} // namespace

		// =====================================================================
		// CE QUE MONTRE LA CARTE « SCRIPTS » (dessinee par NkEditeurDetails.cpp)
		// =====================================================================
		void NkEditeurVariablesScript(NkEditeurScripts &s, const unkeny::NkScript2D &sc, uint32 k,
									  NkVector<NkVariableScriptMontree> &sortie) {
			sortie.Clear();
			// Celles que la DEFINITION declare (classe C++ ou module Blueprint), avec
			// leur defaut : une valeur n'est ecrite dans l'entite que si on la change.
			const unkeny::NkDefinitionScript *d = k < sc.nombre ? s.registre.Definition(s.registre.Trouver(sc.refs[k])) : nullptr;
			if (d != nullptr && d->classe != nullptr) {
				for (uint32 i = 0; i < d->classe->nbVariables; ++i) {
					NkVariableScriptMontree v;
					v.nom = d->classe->variables[i].nom;
					v.type = static_cast<unkeny::NkTypeVarScript>(d->classe->variables[i].type);
					v.defaut = NkVec2f(d->classe->variables[i].x, d->classe->variables[i].y);
					sortie.PushBack(v);
				}
			} else if (d != nullptr && d->programme != nullptr) {
				const unkeny::NkModuleBp &mod = d->programme->module;
				for (uint32 i = 0; i < mod.variables.Size(); ++i) {
					const unkeny::NkVariableBp &x = mod.variables[i];
					if (!x.exposee) {
						continue;
					}
					NkVariableScriptMontree v;
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
					} else if (x.type == unkeny::NkTypeBp::NK_COULEUR) {
						// (2026-10-01) les variables de l'editeur de Blueprint a la UE5.
						v.type = unkeny::NkTypeVarScript::NK_COULEUR;
						v.defaut = unkeny::NkCouleurVersVar(static_cast<uint32>(x.defaut.i));
					} else if (x.type == unkeny::NkTypeBp::NK_TEXTE) {
						v.type = unkeny::NkTypeVarScript::NK_TEXTE;
						const int32 c = x.defaut.i;
						v.texte = c >= 0 && static_cast<uint32>(c) < mod.constantes.Size() ? mod.constantes[static_cast<uint32>(c)].texte : NkString();
					} else if (x.type == unkeny::NkTypeBp::NK_ENTITE) {
						v.type = unkeny::NkTypeVarScript::NK_ENTITE;
					}
					sortie.PushBack(v);
				}
			}
			// Puis celles du composant que la definition ne connait plus (gardees, sauvees).
			for (uint32 v = 0; v < unkeny::NK_UNKENY_SCRIPT_VARS_MAX; ++v) {
				const unkeny::NkVarScript &x = sc.vars[v];
				if (x.nom[0] == '\0' || x.script != k) {
					continue;
				}
				bool connue = false;
				for (uint32 q = 0; q < sortie.Size(); ++q) {
					connue = connue || sortie[q].nom == x.nom;
				}
				if (!connue) {
					NkVariableScriptMontree d2;
					d2.nom = x.nom;
					d2.type = static_cast<unkeny::NkTypeVarScript>(x.type);
					sortie.PushBack(d2);
				}
			}
		}

		const char *NkEditeurStatutScript(NkEditeurScripts &s, ecs::NkEntityId id, uint32 k, const char *ref, NkString &detail) {
			detail = NkString();
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

		void NkEditeurScriptsAAjouter(NkEditeurModele &m, ecs::NkEntityId id, NkVector<NkString> &sortie) {
			sortie.Clear();
			if (m.scripts == nullptr || !m.scene.Monde().IsAlive(id)) {
				return;
			}
			NkVector<NkString> proposes;
			NkEditeurScriptsProposes(*m.scripts, proposes);
			const unkeny::NkScript2D *sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
			for (uint32 i = 0; i < proposes.Size(); ++i) {
				if (sc == nullptr || unkeny::NkScriptTrouver(*sc, proposes[i].CStr()) < 0) {
					sortie.PushBack(proposes[i]);
				}
			}
		}

		bool NkEditeurAjouterScript(NkEditeurModele &m, ecs::NkEntityId id, const char *ref) {
			if (m.scripts == nullptr || ref == nullptr || !m.scene.Monde().IsAlive(id)) {
				return false;
			}
			NkEditeurScripts &s = *m.scripts;
			NkEditeurRetenir(m);
			if (!m.scene.Monde().Has<unkeny::NkScript2D>(id)) {
				m.scene.Monde().Add<unkeny::NkScript2D>(id, unkeny::NkScript2D());
			}
			unkeny::NkScript2D *sc = m.scene.Monde().Get<unkeny::NkScript2D>(id);
			if (sc == nullptr || unkeny::NkScriptAjouter(*sc, ref) < 0) {
				NkEditeurAnnoncer(m, "Script : plus de place (4 au plus) ou nom trop long");
				return false;
			}
			// Les variables exposees d'une classe C++ prennent leur defaut.
			const unkeny::NkDefinitionScript *d = s.registre.Definition(s.registre.Trouver(ref));
			if (d != nullptr && d->classe != nullptr) {
				for (uint32 v = 0; v < d->classe->nbVariables; ++v) {
					const NkUnkVariableV1 &x = d->classe->variables[v];
					unkeny::NkScriptPoserVariable(*sc, static_cast<uint32>(sc->nombre - 1u), x.nom, static_cast<unkeny::NkTypeVarScript>(x.type),
												  NkVec2f(x.x, x.y));
				}
			}
			NkEditeurAnnoncer(m, NkString::Format("Script ajouté : %s", ref).CStr());
			return true;
		}

		void NkEditeurOuvrirScript(NkEditeurCadre &c, const char *ref) {
			if (c.m.scripts == nullptr || ref == nullptr) {
				return;
			}
			NkEditeurScripts &s = *c.m.scripts;
			if (std::strncmp(ref, unkeny::NK_SCRIPT_PREFIXE_CPP, 4) == 0) {
				const NkString source = SourceDe(s, ref);
				if (source.Empty()) {
					NkEditeurAnnoncer(c.m, NkString::Format("%s : source introuvable dans le Contenu", ref).CStr());
				} else if (!NkEditeurOuvrirScriptCpp(s, c.m, source.CStr())) {
					NkEditeurAnnoncer(c.m, NkString::Format("%s : aucun éditeur n'a pu l'ouvrir (voir le Journal)", ref).CStr());
				}
				return;
			}
			NkEditeurOuvrirGraphe(s, c.m, (s.projet + ref).CStr(), &c.ui);
		}

		NkString NkEditeurEtatCompilationScripts(NkEditeurScripts &s) {
			if (s.etat == NkEtatCompilation::NK_EN_COURS) {
				return NkString("C++ : compilation en cours…");
			}
			if (s.etat == NkEtatCompilation::NK_ECHOUEE) {
				return NkString::Format("C++ : la dernière compilation a échoué (%u erreur(s), voir le Journal)",
										static_cast<unsigned>(s.erreurs.Size()));
			}
			return NkString();
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
						s.ageReleve = 99.f; // compile tout de suite (le releve le voit)
						// Le workspace Jenga du projet est (re)assure, puis NKCode s'ouvre
						// dessus et sur le modele (sans effet dans un banc).
						s.workspaceAssure = false;
						NkEditeurOuvrirScriptCpp(s, m, NkEditeurCheminContenu(m, cree.CStr()).CStr());
					}
					break;
				}
				case NK_SCRIPT_NOUVEAU_BP: {
					const NkString cree = NkEditeurNouveauBlueprint(m, DossierDeCreation(c.ui).CStr());
					if (!cree.Empty()) {
						c.ui.contenuPerime = true;
						NkEditeurOuvrirGraphe(s, m, NkEditeurCheminContenu(m, cree.CStr()).CStr(), &c.ui);
						s.ageReleve = 99.f;
					}
					break;
				}
				case NK_SCRIPT_COMPILER:
					NkEditeurScriptsReleverCpp(s, m);
					NkEditeurScriptsCompiler(s, m);
					break;
				default: {
					// « Ajouter un composant > Script : ... » (2026-10-02) : la liste
					// relevee quand le menu s'est peint (NkEditeurInterface::scriptsProposes).
					const int32 k = action - NK_A_SCRIPT - NK_SCRIPT_AJOUTER;
					if (k >= 0 && k < 90 && static_cast<uint32>(k) < c.ui.scriptsProposes.Size() && m.aSelection) {
						NkEditeurAjouterScript(m, m.selection, c.ui.scriptsProposes[static_cast<uint32>(k)].CStr());
					}
					break;
				}
			}
		}

		bool NkEditeurScriptOuvrirAsset(NkEditeurCadre &c, const char *cheminNav) {
			if (c.m.scripts == nullptr || cheminNav == nullptr) {
				return false;
			}
			const NkString abs = NkEditeurCheminContenu(c.m, cheminNav);
			if (FinitPar(cheminNav, ".nkbp")) {
				NkEditeurOuvrirGraphe(*c.m.scripts, c.m, abs.CStr(), &c.ui);
				return true;
			}
			if (FinitPar(cheminNav, ".cpp") || FinitPar(cheminNav, ".h") || FinitPar(cheminNav, ".hpp")) {
				// NKCode SUR le workspace Jenga du projet (sinon l'editeur du systeme ;
				// le Journal dit pourquoi et quoi faire).
				if (!NkEditeurOuvrirScriptCpp(*c.m.scripts, c.m, abs.CStr())) {
					NkEditeurAnnoncer(c.m, "Aucun éditeur n'a pu être lancé pour ce script (voir le Journal)");
				}
				return true;
			}
			return false;
		}

	} // namespace editeur
} // namespace nkentseu
