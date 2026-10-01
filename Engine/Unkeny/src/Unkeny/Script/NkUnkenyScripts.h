//
// NkUnkenyScripts.h
// =============================================================================
// Description :
//   Le REGISTRE des scripts (NkScripts2D : nom <-> definition, Blueprint ou
//   classe C++) et l'HOTE qui les fait tourner dans une scene (NkHoteScripts2D,
//   document 01, § 4.4). L'hote implemente LA table C (NkUnkHoteV1) que
//   Blueprint et C++ appellent tous deux.
//
// Caracteristiques :
//   - L'hote s'inscrit DEUX fois dans la scene : « scripts.pas_fixe »
//     (NK_PAS_FIXE, ordre -100 : avant les systemes du jeu, avant la physique)
//     et « scripts.trame » (NK_TRAME). Les systemes survivent a Init : il ne
//     s'inscrit qu'une fois.
//   - L'ORDRE, qui est un contrat (temoin x1) :
//       pas fixe : Debut des nouveaux, puis « Pas fixe » (le lieu des forces) ;
//       trame    : Debut des nouveaux, puis Contact / Zone, puis Action, puis
//                  Tick.
//     Debut passe avant tout autre evenement de l'entite, quelle que soit la
//     phase. Sur une entite, les scripts recoivent chaque evenement DANS
//     L'ORDRE DE LEUR LISTE (decision Q1 de Rihen). Entre entites : l'ordre de
//     la scene, qui n'est PAS un contrat.
//   - Chaque passage RELEVE d'abord les entites a scripts, puis appelle les
//     scripts HORS de toute requete NKECS : un script peut creer, detruire,
//     allumer ou eteindre sans invalider un parcours. Chaque appel est precede
//     d'un IsAlive.
//   - L'etat d'EXECUTION (debut fait, faute, instance C++) vit ICI, indexe par
//     entite (NkEntityId::Pack) : une entite restauree par Arreter est nouvelle
//     pour l'hote, et recoit son Debut.
//   - Une instance en FAUTE (budget epuise, exception C++) est desactivee
//     SEULE : la scene et les autres continuent ; le journal dit le script,
//     l'entite, l'evenement et, pour un Blueprint, le noeud.
//   - Rechargement a chaud : quand une definition est REMPLACEE (nouveau
//     module C++, Blueprint recompile), MigrerInstances detruit chaque instance
//     par l'ANCIEN code, la recree par le nouveau, rend l'etat prive
//     (Sauver/Relire) et envoie « Recharge » a la place de « Debut ». Les
//     variables, elles, sont dans le composant : elles restent.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYSCRIPTS_H__
#define __NKENTSEU_UNKENY_NKUNKENYSCRIPTS_H__

#include "NKContainers/Associative/NkUnorderedMap.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKECS/NkECSDefines.h"
#include "Unkeny/Script/NkUnkenyBpMachine.h"
#include "Unkeny/Script/NkUnkenyScript.h"
#include "Unkeny/Script/NkUnkenyScriptABI.h"

namespace nkentseu {
	namespace unkeny {

		class NkScene;
		class NkActions;
		class NkLiaisons;
		class NkSons2D;
		enum class NkPhaseSysteme : uint8;

		/// Le prefixe d'une classe C++ dans un nom de script : « cpp:Porte ».
		constexpr const char *NK_SCRIPT_PREFIXE_CPP = "cpp:";

		enum class NkGenreScript : uint8 { NK_AUCUN = 0, NK_BLUEPRINT, NK_CPP };

		/// Une definition : ce que designe un nom de script.
		struct NkDefinitionScript {
				NkString nom;
				NkGenreScript genre = NkGenreScript::NK_AUCUN;
				/// Augmente a chaque remplacement : l'hote voit qu'il doit migrer.
				uint32 version = 1u;
				const NkUnkClasseV1 *classe = nullptr; ///< C++ ; nul = classe disparue du module
				NkProgrammeBp *programme = nullptr;	   ///< Blueprint (possede) ; nul = refuse
				/// Non vide : la definition est connue mais inutilisable, et pourquoi.
				NkString erreur;
		};

		class NkScripts2D {
			public:
				NkScripts2D() = default;
				~NkScripts2D();
				NkScripts2D(const NkScripts2D &) = delete;
				NkScripts2D &operator=(const NkScripts2D &) = delete;

				/// L'identifiant du script `nom` (jamais 0), ou 0 s'il est inconnu.
				uint32 Trouver(const char *nom) const noexcept;
				const NkDefinitionScript *Definition(uint32 id) const noexcept;
				uint32 Nombre() const noexcept {
					return static_cast<uint32>(mDefs.Size());
				}
				/// La i-eme definition (0..Nombre-1) ; son identifiant est i + 1.
				const NkDefinitionScript *DefinitionA(uint32 i) const noexcept {
					return i < mDefs.Size() ? &mDefs[i] : nullptr;
				}
				/// Change a CHAQUE enregistrement : l'hote re-resout ce qu'il ne
				/// connaissait pas.
				uint32 Generation() const noexcept {
					return mGeneration;
				}

				/// Enregistre chaque classe du module sous « cpp:<Classe> ». Une classe
				/// DEJA connue est remplacee (version + 1) ; une classe du module
				/// precedent `ancien` absente du nouveau devient « disparue » (le
				/// composant garde sa reference : rien n'est perdu si elle revient).
				/// false : module nul ou d'ABI majeure differente (rien ne change).
				bool EnregistrerModuleCpp(const NkUnkModuleV1 *module, NkString *erreur = nullptr,
										  const NkUnkModuleV1 *ancien = nullptr);

				/// Enregistre (ou REMPLACE) le Blueprint `nom` depuis son module. Le
				/// module est VERIFIE : refuse, il reste connu (le jeu le dit) mais ne
				/// s'execute pas. Rend l'identifiant.
				uint32 EnregistrerBlueprint(const char *nom, const NkModuleBp &module, NkString *erreur = nullptr);
				/// Depuis un fichier .nkbp (section MODL). Un .nkbp sans module (jamais
				/// compile) est connu et refuse, en le disant.
				uint32 ChargerBlueprint(const char *nom, const char *chemin, NkString *erreur = nullptr);
				/// Depuis une charge « NKBP » en memoire (le jeu cuit).
				uint32 ChargerBlueprintOctets(const char *nom, const uint8 *octets, usize taille,
											  NkString *erreur = nullptr);

				void Vider();

			private:
				uint32 Assurer(const char *nom, NkGenreScript genre);
				NkVector<NkDefinitionScript> mDefs;
				uint32 mGeneration = 1u;
		};

		/// Une ligne du journal des scripts.
		struct NkLigneScript {
				NkString texte;
				bool faute = false;
		};

		/// (2026-10-01) Un PASSAGE d'evenement dans un Blueprint trace : quand,
		/// quel evenement, sur quelle entite, et par quels noeuds (le premier, le
		/// dernier, combien). La console de simulation de l'editeur le montre.
		struct NkPassageBp {
				float32 temps = 0.f;
				uint32 genre = 0u;
				NkString parametre; ///< l'action, le repartiteur
				NkString entite;	///< le nom de l'entite
				uint32 fonction = 0u;
				uint32 premier = 0u, dernier = 0u; ///< noeuds (codes de la table de lignes)
				uint32 nbNoeuds = 0u;
				bool faute = false;
		};

		/// LA TRACE d'un Blueprint (« Simuler » dans l'editeur : les fils qui ont
		/// servi brillent, chaque noeud dit « x3 », les noeuds jamais atteints
		/// s'estompent). Compte, pour chaque noeud, les EVENEMENTS qui y sont
		/// passes (pas les instructions : un « Si » evalue deux fois compte une).
		struct NkTraceBp {
				NkString script;
				NkVector<uint32> noeuds; ///< codes de noeud (table de lignes)
				NkVector<uint32> comptes;
				NkVector<NkPassageBp> passages; ///< les derniers (64 au plus)
				uint32 evenements = 0u;
				uint32 Compte(uint32 noeud) const noexcept {
					for (uint32 i = 0; i < noeuds.Size(); ++i) {
						if (noeuds[i] == noeud) {
							return comptes[i];
						}
					}
					return 0u;
				}
		};

		class NkHoteScripts2D {
			public:
				NkHoteScripts2D();
				~NkHoteScripts2D();
				NkHoteScripts2D(const NkHoteScripts2D &) = delete;
				NkHoteScripts2D &operator=(const NkHoteScripts2D &) = delete;

				/// S'inscrit dans `scene` (deux systemes). Une seconde fois sur la meme
				/// scene ne fait rien. false : deja branche sur une autre.
				bool Brancher(NkScene &scene, NkScripts2D &scripts);
				void Debrancher();
				bool Branche() const noexcept {
					return mScene != nullptr;
				}

				/// Ce que lisent « Action pressee » et « Valeur d'action ». `liaisons`
				/// (facultatif) donne les NOMS ; sans elles, les noms standard.
				void PoserActions(const NkActions *actions, const NkLiaisons *liaisons = nullptr) noexcept {
					mActions = actions;
					mLiaisons = liaisons;
				}
				/// Facultatif : sans lui, « Jouer un son » refuse et le reste tourne.
				void PoserSons(NkSons2D *sons) noexcept {
					mSons = sons;
				}

				/// Ou va le journal (en plus de Journal()) : l'onglet Journal de
				/// l'editeur, le journal du jeu. `faute` : une ligne d'erreur.
				using NkSortieJournal = void (*)(void *donnees, const char *ligne, bool faute);
				void PoserSortie(NkSortieJournal sortie, void *donnees) noexcept {
					mSortie = sortie;
					mSortieDonnees = donnees;
				}
				const NkVector<NkLigneScript> &Journal() const noexcept {
					return mJournal;
				}
				void ViderJournal() noexcept {
					mJournal.Clear();
				}

				/// « Arreter » : detruit les instances C++ (par leur code), oublie
				/// l'etat d'execution et le temps. A appeler AVANT Restaurer.
				void Arreter();

				/// Apres le remplacement d'une definition (rechargement a chaud) :
				/// migre chaque instance dont la version a change. A appeler TANT QUE
				/// l'ancien module est charge (son code detruit ses instances).
				uint32 MigrerInstances();

				/// Le passage d'une phase (les systemes l'appellent ; un banc aussi).
				void Passage(NkPhaseSysteme phase, float32 dt);

				const NkUnkHoteV1 &Table() const noexcept {
					return mTable;
				}
				float32 Temps() const noexcept {
					return mTemps;
				}
				uint32 NbInstances() const noexcept;
				/// Les fautes depuis le dernier Arreter.
				uint32 NbFautes() const noexcept {
					return mFautes;
				}
				/// Les gestes REFUSES par l'hote (entite morte, valeur non finie).
				uint32 NbRefus() const noexcept {
					return mRefus;
				}
				/// L'instance C++ du script `emplacement` de `id` (bancs), ou nul.
				void *InstanceCpp(ecs::NkEntityId id, uint32 emplacement) const noexcept;
				/// L'instance est-elle en faute ?
				bool EnFaute(ecs::NkEntityId id, uint32 emplacement) const noexcept;

				// --- La TRACE des Blueprints (l'editeur, « Simuler ») -----------
				void ActiverTrace(bool active) noexcept {
					mTraceActive = active;
				}
				bool TraceActive() const noexcept {
					return mTraceActive;
				}
				/// La trace du script `nom` (« Contenu/Scripts/Porte.nkbp »), ou nul.
				const NkTraceBp *Trace(const char *nom) const noexcept;
				void ViderTrace() noexcept {
					mTraces.Clear();
				}

				/// (2026-10-01) Un REPARTITEUR appele sur `cible` : l'evenement
				/// personnalise est mis en FILE et livre a la fin du passage en cours
				/// (jamais pendant l'appel qui l'a demande : pas de reentree dans la
				/// machine). false : cible morte, ou plus de 8 parametres.
				bool Diffuser(NkUnkEntite cible, NkUnkEntite source, const char *nom, const NkValeurBp *args, uint32 n,
							  const NkTypeBp *types, const NkModuleBp *module);
				/// Livre la file (le passage le fait seul ; un banc aussi).
				void LivrerDiffusions();

				// --- Pour la table C (NkUnkenyScripts.cpp), pas pour l'appelant ---
				NkScene *Scene() const noexcept {
					return mScene;
				}
				NkScripts2D *Registre() const noexcept {
					return mScripts;
				}
				const NkActions *Actions() const noexcept {
					return mActions;
				}
				const NkLiaisons *Liaisons() const noexcept {
					return mLiaisons;
				}
				NkSons2D *Sons() const noexcept {
					return mSons;
				}
				void Ecrire(const char *ligne, bool faute);
				void CompterRefus() noexcept {
					++mRefus;
				}

			private:
				struct NkInstanceScript {
						uint32 script = 0u; ///< identifiant dans le registre, 0 = aucun
						uint32 version = 0u;
						uint32 generation = 0u; ///< celle du registre a la derniere resolution
						char ref[NK_UNKENY_SCRIPT_NOM_MAX] = {};
						void *cpp = nullptr;
						const NkUnkClasseV1 *classe = nullptr; ///< la classe qui l'a cree
						NkVector<int32> actions;			   ///< Blueprint : l'action de chaque entree
						bool debut = false;
						bool faute = false;
						bool signale = false; ///< « inconnu » / « refuse » deja dit
				};
				struct NkEntiteScripts {
						uint64 pack = 0u;
						NkInstanceScript inst[NK_UNKENY_SCRIPTS_MAX];
						bool vu = false;
				};

				NkEntiteScripts *Fiche(uint64 pack, bool creer);
				void Purger();
				void Lier(ecs::NkEntityId id, NkEntiteScripts &f);
				void LierInstance(ecs::NkEntityId id, NkScript2D &s, uint32 k, NkInstanceScript &inst);
				void Liberer(NkInstanceScript &inst);
				void Migrer(ecs::NkEntityId id, uint32 k, NkInstanceScript &inst, const NkDefinitionScript &def);
				/// Livre `ev` aux scripts de l'entite, dans l'ordre de la liste.
				void Livrer(ecs::NkEntityId id, NkUnkEvenementV1 ev);
				void Envoyer(ecs::NkEntityId id, uint32 k, NkInstanceScript &inst, const NkUnkEvenementV1 &ev);
				void ExecuterBp(ecs::NkEntityId id, uint32 k, NkInstanceScript &inst, const NkDefinitionScript &def,
								const NkUnkEvenementV1 &ev);
				void Faute(ecs::NkEntityId id, NkInstanceScript &inst, const char *script, const char *raison);
				const char *NomAction(int32 i) const noexcept;

				NkScene *mScene = nullptr;
				NkScripts2D *mScripts = nullptr;
				const NkActions *mActions = nullptr;
				const NkLiaisons *mLiaisons = nullptr;
				NkSons2D *mSons = nullptr;
				uint32 mSysPasFixe = 0u, mSysTrame = 0u;
				NkUnkHoteV1 mTable;
				NkVector<NkEntiteScripts> mFiches;
				NkUnorderedMap<uint64, uint32> mIndex;
				NkVector<ecs::NkEntityId> mListe;			///< le releve du passage (reutilise)
				NkVector<NkValeurBp> mRegistres;			///< le cadre de la VM (reutilise)
				NkVector<NkValeurBp> mVariables;			///< les variables d'un appel (reutilise)
				NkVector<NkString> mTextes;					///< les textes fabriques pendant un appel
				// --- La trace -------------------------------------------------
				bool mTraceActive = false;
				NkVector<NkTraceBp> mTraces;
				NkVector<uint32> mVus; ///< les noeuds vus pendant l'appel en cours
				// --- Les repartiteurs -------------------------------------------
				struct NkDiffusionBp {
						uint64 cible = 0u, source = 0u;
						NkString nom;
						uint32 n = 0u;
						NkValeurBp args[8];
						NkTypeBp types[8] = {};
						NkString textes[8];
				};
				NkVector<NkDiffusionBp> mFile;
				const NkDiffusionBp *mDiffusion = nullptr; ///< celle qu'on livre
				const NkModuleBp *mModuleCourant = nullptr; ///< celui qui s'execute (ses textes)
				uint64 mSoiCourant = 0u;					///< l'entite dont le script s'execute
				static bool DiffuserBp(void *donnees, NkUnkEntite cible, const char *nom, const NkValeurBp *args, uint32 n,
									   const NkTypeBp *types);
				static void TracerBp(void *donnees, uint32 fonction, uint32 noeud);
				NkVector<NkLigneScript> mJournal;
				NkSortieJournal mSortie = nullptr;
				void *mSortieDonnees = nullptr;
				float32 mTemps = 0.f;
				uint32 mFautes = 0u;
				uint32 mRefus = 0u;
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYSCRIPTS_H__
