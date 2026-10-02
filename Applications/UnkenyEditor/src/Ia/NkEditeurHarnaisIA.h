//
// NkEditeurHarnaisIA.h
// =============================================================================
// Description :
//   LE HARNAIS COMMUN du banc et des captures de l'IA : un projet dans le
//   dossier temporaire, l'editeur hors ecran (NkEditeurBancTrame), l'IA
//   demarree SANS rien ecrire chez l'utilisateur (son dossier de reglages est
//   temporaire, `persister` est faux), et un FAUX SERVEUR local qui parle
//   Ollama, OpenAI et Anthropic (NKConverse/NkConverseFauxServeur.h).
//
// Caracteristiques :
//   - Les gestes passent par l'ENTREE de la trame (clic sur le composeur,
//     frappe, Entree, clic sur « Confirmer »), jamais par la souris de
//     l'utilisateur : la regle de la maison (aucune injection globale).
//   - La cle OpenAI du banc est posee par une variable PROPRE au fournisseur
//     (NK_IA_CLE_OPENAI_LOCAL), qui passe avant OPENAI_API_KEY : une vraie cle
//     de la machine ne part JAMAIS, meme vers le faux serveur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURHARNAISIA_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURHARNAISIA_H__

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurBancTrame.h"
#include "Ia/NkEditeurIA.h"

#include "NKConverse/NkConverseFauxServeur.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"

#include <cstdio>
#include <cstdlib>

namespace nkentseu {
	namespace editeur {

		struct NkHarnaisIA {
				memory::NkAllocator &al = memory::NkGetDefaultAllocator();
				NkEditeurModele *pm = nullptr;
				NkEditeurBancTrame *pt = nullptr;
				NkEditeurIA *pia = nullptr;
				converse::NkFauxServeurIA serveur;
				NkString racine, projet, dossierIA;
				static constexpr const char *kCle = "sk-banc-1234";

				NkEditeurModele &M() {
					return *pm;
				}
				NkEditeurBancTrame &T() {
					return *pt;
				}
				NkEditeurIA &IA() {
					return *pia;
				}

				bool Ouvrir(const char *nom, float32 w, float32 h) {
					const char *tmp = std::getenv("TEMP");
					racine = NkString(tmp != nullptr ? tmp : "/tmp") + "/" + nom + "/";
					NkDirectory::Delete(racine.CStr(), true);
					projet = racine + "Projet/";
					dossierIA = racine + "ia";
					NkDirectory::CreateRecursive((projet + "Contenu/Scenes").CStr());
					NkDirectory::CreateRecursive(dossierIA.CStr());
#if defined(_WIN32)
					_putenv_s("NK_IA_CLE_OPENAI_LOCAL", kCle);
#else
					setenv("NK_IA_CLE_OPENAI_LOCAL", kCle, 1);
#endif
					// LE FAUX SERVEUR : un modele a outils, un modele sans.
					converse::NkFauxServeurIA::Modele q;
					q.nom = NkString("qwen2.5:7b-instruct");
					q.outils = true;
					serveur.modeles.PushBack(q);
					converse::NkFauxServeurIA::Modele l;
					l.nom = NkString("llama3.2:1b");
					l.outils = false;
					serveur.modeles.PushBack(l);
					serveur.cleAttendue = NkString(kCle);
					if (!serveur.Demarrer()) {
						std::printf("[ia] le faux serveur ne demarre pas (aucun port libre a partir de 39410)\n");
						return false;
					}
					pm = al.New<NkEditeurModele>();
					NkEditeurModele &m = *pm;
					NkCreerRessourcesSim(m.ressources, &m.textures, nullptr);
					NkEditeurNouvelleScene(m);
					m.chemin = projet + "Contenu/Scenes/Banc.nkscene";
					NkEditeurSauver(m);
					pt = al.New<NkEditeurBancTrame>(m);
					NkEditeurBancTrame &t = *pt;
					t.W = w;
					t.H = h;
					t.pctx->Init(static_cast<int32>(w), static_cast<int32>(h));
					// LE DEFAUT : l'onglet IA du groupe Details | Monde, au premier plan.
					t.Ui().iaPlace = 0;
					t.Ui().ongletDroite = NK_ONGLET_IA;
					t.Ui().voirIA = true;
					t.Ui().voirTiroir = false;
					t.Ui().cadrageEnAttente = true;
					pia = al.New<NkEditeurIA>();
					NkEditeurIA &ia = *pia;
					ia.persister = false;
					NkEditeurIADemarrer(ia, m, dossierIA.CStr());
					// Les fournisseurs du banc pointent sur le FAUX serveur.
					for (usize i = 0; i < ia.fournisseurs.Size(); ++i) {
						converse::NkReglagesFournisseur &r = ia.fournisseurs[i];
						if (r.genre == converse::NkGenreFournisseur::NK_OLLAMA) {
							r.adresse = serveur.Adresse();
							r.modele = NkString("qwen2.5:7b-instruct");
						} else if (r.genre == converse::NkGenreFournisseur::NK_OPENAI) {
							r.adresse = serveur.Adresse() + "/v1";
							r.modele = NkString("qwen2.5:7b-instruct");
						} else if (r.genre == converse::NkGenreFournisseur::NK_ANTHROPIC)
							r.adresse = serveur.Adresse();
					}
					NkEditeurIAChoisir(ia, 0);
					return true;
				}

				void Fermer() {
					serveur.Arreter();
					if (pt)
						al.Delete(pt);
					if (pia) {
						if (pm)
							pm->ia = nullptr;
						al.Delete(pia);
					}
					if (pm)
						al.Delete(pm);
					pt = nullptr;
					pia = nullptr;
					pm = nullptr;
					NkDirectory::Delete(racine.CStr(), true);
				}

				/// Des trames jusqu'a ce que le tour en vol se termine (repos, ou une
				/// confirmation attendue). `longueurs` : les longueurs DISTINCTES du
				/// texte de prose vues pendant le flux (la preuve qu'il a grandi).
				bool Attendre(float64 maxS = 15.0, NkVector<uint32> *longueurs = nullptr) {
					NkChrono horloge;
					for (;;) {
						T().Trame();
						NkEditeurIA &ia = IA();
						if (longueurs && ia.blocFlux != 0u) {
							uint32 k = 0u;
							if (ia.pan.Fil().TrouverParId(ia.blocFlux, k)) {
								const uint32 n = static_cast<uint32>(ia.pan.Fil().At(k).texte.Length());
								bool vu = false;
								for (usize i = 0; i < longueurs->Size(); ++i)
									vu = vu || (*longueurs)[i] == n;
								if (!vu)
									longueurs->PushBack(n);
							}
						}
						if (ia.phase != NkPhaseIA::NK_ATTENTE)
							return true;
						if (horloge.Elapsed().ToSeconds() > maxS)
							return false;
						NkChrono::Sleep(static_cast<int64>(2));
					}
				}

				/// Une demande TAPEE : un clic dans le composeur, la frappe, Entree.
				bool Dire(const char *texte) {
					T().Trame();
					editorkit::NkAiRectPublie cad;
					NkEditeurIA &ia = IA();
					if (!ia.pan.planChrome.Trouver(0u, editorkit::NkAiPiece::ComposeurCadre, cad))
						return false;
					T().Clic(0, ia.pan.rect.x + cad.x + cad.w * 0.5f, ia.pan.rect.y + cad.y + cad.h * 0.5f);
					T().Taper(texte);
					T().Touche(nkgui::NkGuiKey::Enter);
					return ia.phase != NkPhaseIA::NK_REPOS || ia.pan.Fil().Taille() > 0u;
				}

				/// Un clic sur le bouton `indice` du bloc `bloc` (« Confirmer » = 0).
				bool CliquerAction(uint32 bloc, uint32 indice) {
					T().Trame();
					NkEditeurIA &ia = IA();
					editorkit::NkAiRectPublie ab;
					if (!ia.pan.planFil.TrouverIndice(bloc, editorkit::NkAiPiece::Action, indice, ab))
						return false;
					T().Clic(0, ia.pan.filOx + ab.x + ab.w * 0.5f, ia.pan.filOy + ab.y + ab.h * 0.5f);
					return true;
				}

				/// Rend le clavier a l'editeur (Echap dans le composeur), puis une
				/// touche avec Ctrl.
				void ToucheEditeur(nkgui::NkGuiKey k, bool ctrl) {
					T().Touche(nkgui::NkGuiKey::Escape);
					T().Touche(k, ctrl);
				}

				uint32 NbEntites() {
					uint32 n = 0u;
					M().scene.Monde().Query<NkTransform2D>().ForEach([&](ecs::NkEntityId, NkTransform2D &) { ++n; });
					return n;
				}
				/// Le dernier bloc du fil d'un type donne (0 : aucun).
				uint32 DernierBloc(editorkit::NkAiBloc type) {
					editorkit::NkAiFil &f = IA().pan.Fil();
					for (uint32 i = f.Taille(); i > 0u; --i)
						if (f.At(i - 1u).type == type)
							return f.At(i - 1u).id;
					return 0u;
				}
				NkString TexteBloc(uint32 id) {
					uint32 k = 0u;
					if (IA().pan.Fil().TrouverParId(id, k))
						return IA().pan.Fil().At(k).texte;
					return NkString();
				}
		};

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURHARNAISIA_H__
