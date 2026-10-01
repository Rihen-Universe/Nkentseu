// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurGraphe.cpp
// DESCRIPTION: La colle entre UnkenyEditor et l'editeur de Blueprint a la UE5
//              (NkEditeurBlueprint.h) : ouvrir, fermer, l'hote (compiler et
//              donner au registre, Simuler), et le dessin EN PLEIN.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurGraphe.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurInterface.h"
#include "Script/NkBpCatalogue.h"
#include "Script/NkEditeurScripts.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {
			struct Colle {
					NkEditeurScripts *s = nullptr;
					NkEditeurModele *m = nullptr;
			};
			/// Une colle par trame (l'hote ne garde que des pointeurs).
			Colle &LaColle() {
				static Colle c;
				return c;
			}

			bool HoteCompiler(void *d, NkEditeurBlueprintEtat &e, NkErreurBp &err, NkString &message) {
				Colle &c = *static_cast<Colle *>(d);
				NkEditeurScripts &s = *c.s;
				unkeny::NkModuleBp module;
				const bool ok = NkBpEnregistrerDocument(e.chemin.CStr(), e.doc, err, &module);
				if (!ok) {
					return false;
				}
				NkString refus;
				s.registre.EnregistrerBlueprint(e.ref.CStr(), module, &refus);
				const uint32 migrees = s.hote.MigrerInstances();
				// La trace parle de l'ANCIEN graphe : elle repart de zero.
				s.hote.ViderTrace();
				message = NkString::Format("Compilé et enregistré : %u fonction(s), %u variable(s)%s", static_cast<unsigned>(module.fonctions.Size()),
										   static_cast<unsigned>(module.variables.Size()), migrees > 0u ? " — rechargé dans la partie en cours" : "");
				// Le releve du disque ne le relira pas une seconde fois.
				for (uint32 i = 0; i < s.blueprints.Size(); ++i) {
					if (s.blueprints[i] == e.ref) {
						s.empreintesBp[i] = -1;
					}
				}
				return true;
			}
			const unkeny::NkTraceBp *HoteTrace(void *d, const char *ref) {
				Colle &c = *static_cast<Colle *>(d);
				return c.s->hote.Trace(ref);
			}
			void HoteSimuler(void *d, bool demarrer) {
				Colle &c = *static_cast<Colle *>(d);
				if (demarrer) {
					c.s->hote.ViderTrace();
					c.s->hote.ActiverTrace(true);
					if (c.m->etat == NkEtatJeu::NK_EDITION) {
						NkEditeurJouer(*c.m);
					}
				} else if (c.m->etat != NkEtatJeu::NK_EDITION) {
					NkEditeurArreter(*c.m); // la trace reste lisible jusqu'au prochain Simuler
				}
			}
			bool HoteEnJeu(void *d) {
				Colle &c = *static_cast<Colle *>(d);
				return c.m->etat != NkEtatJeu::NK_EDITION;
			}
			void HoteFermer(void *d) {
				Colle &c = *static_cast<Colle *>(d);
				NkEditeurFermerGraphe(*c.s);
			}
			void HoteJournal(void *d, const char *ligne, bool faute) {
				Colle &c = *static_cast<Colle *>(d);
				c.s->journal.PushBack(NkString(ligne));
				if (faute) {
					c.s->montrerJournal = true;
				}
			}
		} // namespace

		NkHoteBlueprint NkEditeurHoteBlueprint(NkEditeurScripts &s, NkEditeurModele &m) {
			Colle &c = LaColle();
			c.s = &s;
			c.m = &m;
			NkHoteBlueprint h;
			h.donnees = &c;
			h.Compiler = &HoteCompiler;
			h.Trace = &HoteTrace;
			h.Simuler = &HoteSimuler;
			h.EnJeu = &HoteEnJeu;
			h.Fermer = &HoteFermer;
			h.Journal = &HoteJournal;
			return h;
		}

		bool NkEditeurGrapheOuvert(const NkEditeurModele &m) {
			return m.scripts != nullptr && m.scripts->graphe.ouvert;
		}

		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin) {
			NkEditeurBlueprintEtat &e = s.graphe;
			NkString err;
			NkDocumentBp d;
			if (!NkBpOuvrirDocument(chemin, d, &err)) {
				NkEditeurAnnoncer(m, NkString::Format("Blueprint illisible : %s", err.CStr()).CStr());
				return false;
			}
			e.doc = d;
			e.chemin = chemin;
			e.ref = NkEditeurRefScript(m, chemin);
			// Le nom : le fichier sans dossier ni extension.
			const char *base = std::strrchr(e.chemin.CStr(), '/');
			const char *base2 = std::strrchr(e.chemin.CStr(), '\\');
			base = base2 != nullptr && (base == nullptr || base2 > base) ? base2 : base;
			e.nom = base != nullptr ? NkString(base + 1) : e.chemin;
			if (e.nom.EndsWith(".nkbp")) {
				e.nom = NkString(e.nom.CStr(), e.nom.Length() - 5u);
			}
			NkEditeurBlueprintCharger(e);
			e.ouvert = true;
			e.message = NkString::Format("Ouvert : %s — clic droit dans le vide pour poser un nœud ; « Compiler » enregistre.", e.ref.CStr());
			e.erreur = false;
			return true;
		}

		void NkEditeurFermerGraphe(NkEditeurScripts &s) {
			s.graphe.ouvert = false;
			s.graphe.menu.ouvert = false;
			s.graphe.popup.ouvert = false;
			s.graphe.codeNoeud = graph::NK_NODE_INVALID;
		}

		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m) {
			NkHoteBlueprint h = NkEditeurHoteBlueprint(s, m);
			const bool ok = NkEditeurBlueprintCompiler(s.graphe, h);
			if (!ok) {
				s.montrerJournal = true;
			}
			return ok;
		}

		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type) {
			NkEditeurBlueprintEtat &e = s.graphe;
			const NkRect z = e.zoneToile;
			const NkVec2 c = editorkit::NkCanevasDepuisEcran(e.Toile(), z, NkVec2{z.x + z.w * 0.4f, z.y + z.h * 0.4f});
			return NkEditeurBlueprintPoser(e, type, c.x, c.y);
		}

		void NkEditeurDessinerGraphe(NkEditeurCadre &c) {
			if (c.m.scripts == nullptr || !c.m.scripts->graphe.ouvert) {
				return;
			}
			NkEditeurScripts &s = *c.m.scripts;
			NkHoteBlueprint h = NkEditeurHoteBlueprint(s, c.m);
			// EN PLEIN : les panneaux de la scene se replient le temps de l'edition
			// (ils reviennent a la fermeture). L'integration en onglet le fera mieux.
			if (s.pleinEcran && !s.panneauxCaches) {
				s.panneauxCaches = true;
				s.voirPlacer = c.ui.voirPlacer;
				s.voirOutliner = c.ui.voirOutliner;
				s.voirDetails = c.ui.voirDetails;
				s.voirTiroir = c.ui.voirTiroir;
				c.ui.voirPlacer = c.ui.voirOutliner = c.ui.voirDetails = c.ui.voirTiroir = false;
				NkEditeurPlanifier(c.ui, c.ui.ecran.w, c.ui.ecran.h);
			}
			const NkRect r = c.ui.vue;
			if (NkEditeurBlueprintDessiner(s.graphe, h, c.ctx, c.police, r)) {
				// Le clavier est a l'editeur de Blueprint : les raccourcis de la scene
				// (Suppr, Espace, Ctrl+S, Q/W/E/R) se taisent.
				c.ui.toucheChamp = true;
			}
			if (!s.graphe.ouvert && s.panneauxCaches) {
				s.panneauxCaches = false;
				c.ui.voirPlacer = s.voirPlacer;
				c.ui.voirOutliner = s.voirOutliner;
				c.ui.voirDetails = s.voirDetails;
				c.ui.voirTiroir = s.voirTiroir;
			}
		}

	} // namespace editeur
} // namespace nkentseu
