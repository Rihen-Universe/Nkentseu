// -----------------------------------------------------------------------------
// FICHIER: UnkenyEditor/Script/NkEditeurGraphe.cpp
// DESCRIPTION: La colle entre UnkenyEditor et l'editeur de Blueprint a la UE5
//              (NkEditeurBlueprint.h) : ouvrir (un onglet de document par
//              Blueprint), fermer, l'hote (compiler et donner au registre,
//              Simuler), et le dessin dans le CORPS quand son onglet est devant.
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Script/NkEditeurGraphe.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurDocuments.h"
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
					NkEditeurInterface *ui = nullptr;
					/// « Fermer » demande pendant le dessin : fait APRES lui (l'etat
					/// de l'editeur ne doit pas disparaitre sous ses pieds).
					nk_uint64 aFermer = 0;
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
				if (c.s->courant != nullptr) {
					c.aFermer = c.s->courant->id;
				}
			}
			/// Ferme ce que « Fermer » a demande : par la barre s'il y en a une (le
			/// voisin de gauche, ou la scene, passe devant), sinon directement.
			void FermerDemande(Colle &c) {
				if (c.aFermer == 0) {
					return;
				}
				const nk_uint64 id = c.aFermer;
				c.aFermer = 0;
				if (c.ui != nullptr) {
					NkEditeurFermerDocument(*c.m, *c.ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, id));
				} else {
					NkEditeurDetruireGraphe(*c.s, id);
				}
			}
			void HoteJournal(void *d, const char *ligne, bool faute) {
				Colle &c = *static_cast<Colle *>(d);
				c.s->journal.PushBack(NkString(ligne));
				if (faute) {
					c.s->montrerJournal = true;
				}
			}
		} // namespace

		NkHoteBlueprint NkEditeurHoteBlueprint(NkEditeurScripts &s, NkEditeurModele &m, NkEditeurInterface *ui) {
			Colle &c = LaColle();
			c.s = &s;
			c.m = &m;
			c.ui = ui;
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
			return m.scripts != nullptr && m.scripts->courant != nullptr;
		}

		NkEditeurGrapheEtat *NkEditeurGrapheParId(NkEditeurScripts &s, nk_uint64 id) {
			for (uint32 i = 0; i < s.graphes.Size(); ++i) {
				if (s.graphes[i]->id == id) {
					return s.graphes[i];
				}
			}
			return nullptr;
		}

		bool NkEditeurGrapheDevenirCourant(NkEditeurScripts &s, nk_uint64 id) {
			NkEditeurGrapheEtat *e = NkEditeurGrapheParId(s, id);
			if (e != nullptr) {
				s.courant = e;
			}
			return e != nullptr;
		}

		bool NkEditeurOuvrirGraphe(NkEditeurScripts &s, NkEditeurModele &m, const char *chemin, NkEditeurInterface *ui) {
			// Ouvert deux fois : le MEME onglet revient (ses modifications, son
			// historique, sa vue sont gardes). Compare par REFERENCE du projet
			// (« Contenu/Scripts/Porte.nkbp ») : deux ecritures du meme chemin absolu
			// (barres, majuscules du disque) ne font pas deux onglets.
			const NkString ref = chemin != nullptr ? NkEditeurRefScript(m, chemin) : NkString();
			for (uint32 i = 0; chemin != nullptr && i < s.graphes.Size(); ++i) {
				if (s.graphes[i]->chemin == NkString(chemin) || (!ref.Empty() && s.graphes[i]->ref == ref)) {
					s.courant = s.graphes[i];
					if (ui != nullptr) {
						NkEditeurActiverDocument(m, *ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, s.graphes[i]->id));
					}
					return true;
				}
			}
			NkString err;
			NkDocumentBp d;
			if (chemin == nullptr || !NkBpOuvrirDocument(chemin, d, &err)) {
				NkEditeurAnnoncer(m, NkString::Format("Blueprint illisible : %s", err.CStr()).CStr());
				return false;
			}
			NkEditeurBlueprintEtat *pe = memory::NkGetDefaultAllocator().New<NkEditeurBlueprintEtat>();
			NkEditeurBlueprintEtat &e = *pe;
			e.id = s.prochainGraphe++;
			e.doc = d;
			e.chemin = chemin;
			e.ref = ref;
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
			s.graphes.PushBack(pe);
			s.courant = pe;
			// Son onglet entre dans la barre, au premier plan.
			if (ui != nullptr) {
				NkEditeurActiverDocument(m, *ui, NkDoc(NkGenreDocument::NK_BLUEPRINT, e.id));
			}
			return true;
		}

		bool NkEditeurDetruireGraphe(NkEditeurScripts &s, nk_uint64 id) {
			for (uint32 i = 0; i < s.graphes.Size(); ++i) {
				if (s.graphes[i]->id != id) {
					continue;
				}
				NkEditeurBlueprintEtat *e = s.graphes[i];
				s.graphes.Erase(s.graphes.Begin() + i);
				if (s.courant == e) {
					// Le courant devient le dernier ouvert qui reste (ou aucun).
					s.courant = s.graphes.Empty() ? nullptr : s.graphes[s.graphes.Size() - 1];
				}
				memory::NkGetDefaultAllocator().Delete(e);
				return true;
			}
			return false;
		}

		void NkEditeurFermerGraphe(NkEditeurScripts &s) {
			if (s.courant != nullptr) {
				NkEditeurDetruireGraphe(s, s.courant->id);
			}
		}

		bool NkEditeurCompilerGraphe(NkEditeurScripts &s, NkEditeurModele &m) {
			if (s.courant == nullptr) {
				return false;
			}
			NkHoteBlueprint h = NkEditeurHoteBlueprint(s, m);
			return NkEditeurBlueprintCompiler(*s.courant, h);
		}

		graph::NkNodeId NkEditeurGrapheAjouter(NkEditeurScripts &s, const char *type) {
			if (s.courant == nullptr) {
				return graph::NK_NODE_INVALID;
			}
			NkEditeurBlueprintEtat &e = *s.courant;
			const NkRect z = e.zoneToile;
			const NkVec2 c = editorkit::NkCanevasDepuisEcran(e.Toile(), z, NkVec2{z.x + z.w * 0.4f, z.y + z.h * 0.4f});
			return NkEditeurBlueprintPoser(e, type, c.x, c.y);
		}

		NkRect NkEditeurZoneGraphe(const NkEditeurInterface &ui) {
			// Le corps ENTIER sous les onglets (la rangee de la barre d'outils de la
			// scene comprise : l'editeur a la sienne), jusqu'a la barre d'etat --
			// la zone des pages Animation / Animateur.
			return NkRect{ui.ecran.x, ui.barreOutils.y, ui.ecran.w, ui.statut.y - ui.barreOutils.y};
		}

		bool NkEditeurDessinerGraphe(NkEditeurCadre &c) {
			if (c.m.scripts == nullptr || c.m.scripts->courant == nullptr) {
				return false;
			}
			NkEditeurScripts &s = *c.m.scripts;
			NkHoteBlueprint h = NkEditeurHoteBlueprint(s, c.m, &c.ui);
			const NkRect r = NkEditeurZoneGraphe(c.ui);
			c.ctx.dl.AddRectFilled(r, c.pal.fond);
			NkEditeurBlueprintEtat &e = *s.courant;
			NkEditeurBlueprintDessiner(e, h, c.ctx, c.police, r);
			// Une SAISIE de l'editeur (un champ, un menu, du code) garde toutes ses
			// touches ; sinon les raccourcis de l'application ne laissent a la scene,
			// derriere, que Ctrl+W (NkEditeurRaccourcis : un Blueprint devant).
			if (e.saisie) {
				c.ui.toucheChamp = true;
			}
			FermerDemande(LaColle());
			return true;
		}

	} // namespace editeur
} // namespace nkentseu
