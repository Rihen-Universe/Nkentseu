//
// NkUnkenyPrefab.cpp
// =============================================================================
// Description :
//   Les prefabs d'Unkeny : capture, instanciation, fusion a trois (ancien
//   modele / nouveau modele / instance), fichier .nkprefab.
//
// Caracteristiques :
//   - Rien n'est refait a cote de la scene : une instance nait par
//     NkScene::RefaireEntites (le chemin de Restaurer et du chargement), un
//     prefab s'ecrit par l'ecriture d'entite de la sauvegarde
//     (NkUnkenySauvegardeInterne.h). Deux chemins pour refaire un corps rigide
//     finiraient par diverger.
//   - La fusion compare CHAMP PAR CHAMP. Les composants d'Unkeny sont decrits
//     ici (kChamps*) ; ceux du jeu par leur declaration (PhotographierAussi<T>
//     avec champs) ; un composant du jeu non decrit se compare en entier.
//
// Algorithmes implementes :
//   - Fusion a trois par champ : instance == ancien  =>  instance = nouveau
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Scene/NkUnkenyPrefab.h"

#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/NkArchive.h"
#include "Unkeny/Scene/NkUnkenySauvegardeInterne.h"

#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			using NkPhotoEntite = NkScene::NkPhotoEntite;

			NkStringView V(const char *s) {
				return NkStringView(s);
			}

			// ---- Les composants d'Unkeny, champ par champ, pour la fusion ------
			const NkChampSauve kChampsEtiquette[] = {
				NK_UNKENY_CHAMP(NkEtiquette, nom, NkTypeChamp::NK_TEXTE),
			};
			const NkChampSauve kChampsSprite[] = {
				NK_UNKENY_CHAMP(NkSprite2D, taille, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkSprite2D, pivot, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkSprite2D, couleur, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkSprite2D, texId, NkTypeChamp::NK_TEXTURE),
				NK_UNKENY_CHAMP(NkSprite2D, uv0, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkSprite2D, uv1, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkSprite2D, couche, NkTypeChamp::NK_I32),
				NK_UNKENY_CHAMP(NkSprite2D, visible, NkTypeChamp::NK_BOOL),
			};
			const NkChampSauve kChampsCollisionneur[] = {
				NK_UNKENY_CHAMP(NkCollisionneur2D, forme, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkCollisionneur2D, demiTaille, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkCollisionneur2D, rayon, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkCollisionneur2D, decalage, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkCollisionneur2D, couche, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkCollisionneur2D, masque, NkTypeChamp::NK_U32),
				NK_UNKENY_CHAMP(NkCollisionneur2D, declencheur, NkTypeChamp::NK_BOOL),
			};
			const NkChampSauve kChampsCorps[] = {
				NK_UNKENY_CHAMP(NkCorps2D, type, NkTypeChamp::NK_U8),
				NK_UNKENY_CHAMP(NkCorps2D, masse, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkCorps2D, amortissementLineaire, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkCorps2D, amortissementAngulaire, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkCorps2D, echelleGravite, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkCorps2D, rotationBloquee, NkTypeChamp::NK_BOOL),
				NK_UNKENY_CHAMP(NkCorps2D, friction, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkCorps2D, rebond, NkTypeChamp::NK_F32),
				// Le lien vers le solveur : propre a CHAQUE instance, jamais fusionne.
				NK_UNKENY_CHAMP_TRANSITOIRE(NkCorps2D, corpsId, NkTypeChamp::NK_U32),
			};
			/// La place d'un ENFANT dans son parent.
			const NkChampSauve kChampsLocal[] = {
				NK_UNKENY_CHAMP(NkTransform2D, position, NkTypeChamp::NK_VEC2),
				NK_UNKENY_CHAMP(NkTransform2D, rotation, NkTypeChamp::NK_F32),
				NK_UNKENY_CHAMP(NkTransform2D, echelle, NkTypeChamp::NK_VEC2),
			};
			/// La RACINE : sa position et sa rotation sont celles de l'instance.
			const NkChampSauve kChampsRacine[] = {
				NK_UNKENY_CHAMP(NkTransform2D, echelle, NkTypeChamp::NK_VEC2),
			};
			template <typename T, uint32 N> constexpr uint32 NbChamps(const T (&)[N]) noexcept {
				return N;
			}

			/// Fusion a trois, champ par champ : un champ que l'instance n'a pas
			/// touche (egal a l'ancien modele) prend la nouvelle valeur ; un champ
			/// touche est une SURCHARGE et reste. Rend vrai si l'instance change.
			bool Fusionner(const NkChampSauve *champs, uint32 n, uint8 *inst, const uint8 *ancien, const uint8 *nouveau) {
				bool change = false;
				for (uint32 c = 0; c < n; ++c) {
					const NkChampSauve &ch = champs[c];
					if (ch.transitoire) {
						continue;
					}
					if (NkChampEgal(ch, inst, ancien) && !NkChampEgal(ch, inst, nouveau)) {
						NkChampCopier(ch, inst, nouveau);
						change = true;
					}
				}
				return change;
			}

			/// Les champs ou `inst` differe de `modele`, « Composant.champ ».
			void Differences(const char *composant, const NkChampSauve *champs, uint32 n, const uint8 *inst,
							 const uint8 *modele, NkVector<NkString> &out) {
				for (uint32 c = 0; c < n; ++c) {
					if (!champs[c].transitoire && !NkChampEgal(champs[c], inst, modele)) {
						out.PushBack(NkString::Format("%s.%s", composant, champs[c].nom));
					}
				}
			}

			/// Ou trouver, dans les octets d'un NOEUD, le composant du copieur `k`
			/// de la scene : par le NOM (la disposition du prefab peut venir d'une
			/// scene qui les a declares dans un autre ordre), par le rang et la
			/// taille pour un composant sans nom.
			struct NkCarte {
					NkVector<int32> rang;
					NkVector<uint32> decalage;
			};

			NkCarte Carte(const NkPrefab2D &p, const NkScene &scene) {
				NkCarte c;
				const uint32 n = scene.NbComposantsPhoto();
				c.rang.Resize(n);
				c.decalage.Resize(n);
				for (uint32 k = 0; k < n; ++k) {
					c.rang[k] = -1;
					c.decalage[k] = 0;
					const char *nom = scene.NomComposantPhoto(k);
					uint32 dec = 0;
					for (uint32 d = 0; d < p.disposition.Size(); ++d) {
						const NkDispositionPrefab &dp = p.disposition[d];
						const bool memeNom = nom != nullptr && !dp.nom.Empty() && std::strcmp(dp.nom.CStr(), nom) == 0;
						const bool anonyme = nom == nullptr && dp.nom.Empty() && d == k;
						if ((memeNom || anonyme) && dp.taille == scene.TailleComposantPhoto(k)) {
							c.rang[k] = static_cast<int32>(d);
							c.decalage[k] = dec;
							break;
						}
						dec += dp.taille;
					}
				}
				return c;
			}

			const uint8 *OctetsNoeud(const NkCarte &c, const NkPhotoEntite &n, uint32 k) {
				if (k >= c.rang.Size() || c.rang[k] < 0) {
					return nullptr;
				}
				if ((n.extraPresents & (1u << static_cast<uint32>(c.rang[k]))) == 0u) {
					return nullptr;
				}
				return n.extra.Data() + c.decalage[k];
			}

			/// Le noeud `n` du prefab, dans la disposition de `scene`.
			NkPhotoEntite VersScene(const NkCarte &c, const NkPhotoEntite &n, NkScene &scene) {
				NkPhotoEntite e = n;
				e.extra.Clear();
				e.extraPresents = 0u;
				uint32 total = 0;
				for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
					total += scene.TailleComposantPhoto(k);
				}
				e.extra.Resize(total);
				uint32 dec = 0;
				for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
					const uint32 t = scene.TailleComposantPhoto(k);
					if (const uint8 *src = OctetsNoeud(c, n, k)) {
						std::memcpy(e.extra.Data() + dec, src, t);
						e.extraPresents |= 1u << k;
					}
					dec += t;
				}
				return e;
			}

			/// Dans les octets `octets` du copieur `k`, remplace chaque valeur NK_ENTITE
			/// par `traduire(valeur)`.
			template <typename Fn> void TraduireEntites(NkScene &scene, uint32 k, uint8 *octets, Fn &&traduire) {
				uint32 nb = 0;
				const NkChampSauve *champs = scene.ChampsComposantPhoto(k, nb);
				for (uint32 c = 0; champs != nullptr && c < nb; ++c) {
					if (champs[c].type != NkTypeChamp::NK_ENTITE) {
						continue;
					}
					for (uint32 i = 0; i < champs[c].nombre; ++i) {
						uint8 *p = octets + champs[c].decalage + i * champs[c].Pas();
						uint64 v = 0;
						std::memcpy(&v, p, sizeof(v));
						v = traduire(v);
						std::memcpy(p, &v, sizeof(v));
					}
				}
			}

			const NkPhotoEntite *Noeud(const NkPrefab2D &p, uint64 numero) {
				for (uint32 i = 0; i < p.noeuds.Size(); ++i) {
					if (p.noeuds[i].uid == numero) {
						return &p.noeuds[i];
					}
				}
				return nullptr;
			}

			/// Les entites d'une instance : numero de noeud -> entite.
			struct NkMembre {
					uint64 noeud = 0;
					ecs::NkEntityId entite;
			};
			void Membres(NkScene &scene, uint32 prefab, ecs::NkEntityId racine, NkVector<NkMembre> &out) {
				out.Clear();
				scene.Monde().Query<NkInstancePrefab2D>().ForEach([&](ecs::NkEntityId id, NkInstancePrefab2D &ip) {
					if (ip.prefab == prefab && ip.racine == racine) {
						NkMembre m;
						m.noeud = ip.noeud;
						m.entite = id;
						out.PushBack(m);
					}
				});
			}
			ecs::NkEntityId MembreDe(const NkVector<NkMembre> &m, uint64 noeud) {
				for (uint32 i = 0; i < m.Size(); ++i) {
					if (m[i].noeud == noeud) {
						return m[i].entite;
					}
				}
				return ecs::NkEntityId::Invalid();
			}

			/// Fusion d'un composant d'Unkeny tenu en valeur (etiquette, sprite...).
			/// `aI` / `aA` / `aN` : l'instance, l'ancien, le nouveau l'ont-ils ?
			/// Rend vrai si l'instance change (valeur ou presence).
			template <typename T>
			bool FusionnerComposant(bool &aI, T &inst, bool aA, const T &ancien, bool aN, const T &nouveau,
									const NkChampSauve *champs, uint32 n) {
				if (aA && aN) {
					return aI && Fusionner(champs, n, reinterpret_cast<uint8 *>(&inst),
										   reinterpret_cast<const uint8 *>(&ancien), reinterpret_cast<const uint8 *>(&nouveau));
				}
				if (!aA && aN && !aI) {
					inst = nouveau; // le modele l'a gagne : l'instance aussi
					aI = true;
					return true;
				}
				if (aA && !aN && aI) {
					// Le modele l'a perdu : l'instance aussi, sauf si on l'y a retouche.
					bool touche = false;
					for (uint32 c = 0; c < n; ++c) {
						touche = touche || (!champs[c].transitoire &&
											!NkChampEgal(champs[c], reinterpret_cast<const uint8 *>(&inst),
														 reinterpret_cast<const uint8 *>(&ancien)));
					}
					if (!touche) {
						aI = false;
						return true;
					}
				}
				return false;
			}

			template <typename T> void PoserOuRetirer(ecs::NkWorld &w, ecs::NkEntityId id, bool present, const T &v) {
				if (present) {
					w.Add<T>(id, v);
				} else if (w.Has<T>(id)) {
					w.Remove<T>(id);
				}
			}
		} // namespace

		// =====================================================================
		uint32 NkPrefabs2D::Trouver(const char *nom) const noexcept {
			if (nom == nullptr || nom[0] == '\0') {
				return 0u;
			}
			for (uint32 i = 0; i < mPrefabs.Size(); ++i) {
				if (std::strcmp(mPrefabs[i].nom.CStr(), nom) == 0) {
					return i + 1u;
				}
			}
			return 0u;
		}

		bool NkPrefabs2D::Capturer(NkScene &scene, ecs::NkEntityId racine, uint32 idPrefab, const NkPrefab2D *ancien,
								   NkPrefab2D &out) {
			if (!scene.Monde().IsAlive(racine)) {
				return false;
			}
			NkVector<ecs::NkEntityId> ids;
			ids.PushBack(racine);
			{
				NkVector<ecs::NkEntityId> desc;
				scene.Descendants(racine, desc);
				for (uint32 i = 0; i < desc.Size(); ++i) {
					ids.PushBack(desc[i]);
				}
			}
			out.noeuds.Clear();
			out.prochainNoeud = ancien != nullptr ? ancien->prochainNoeud : 1u;

			// 1. Un NUMERO de noeud par entite. Une entite qui etait deja un noeud de
			//    CE prefab garde le sien : c'est ce qui relie les instances a leur
			//    modele d'une version a l'autre. La racine garde celui de l'ancienne
			//    racine, quoi qu'elle soit.
			NkVector<uint64> numeros;
			numeros.Resize(ids.Size());
			for (uint32 i = 0; i < ids.Size(); ++i) {
				uint64 n = 0;
				if (ancien != nullptr) {
					if (i == 0u && ancien->noeuds.Size() > 0u) {
						n = ancien->noeuds[0].uid;
					} else if (const NkInstancePrefab2D *ip = scene.Monde().Get<NkInstancePrefab2D>(ids[i])) {
						if (ip->prefab == idPrefab && Noeud(*ancien, ip->noeud) != nullptr) {
							n = ip->noeud;
						}
					}
				}
				for (uint32 j = 0; j < i && n != 0u; ++j) {
					if (numeros[j] == n) {
						n = 0; // deja pris (une copie d'un noeud) : un numero neuf
					}
				}
				if (n == 0u) {
					n = out.prochainNoeud++;
				}
				numeros[i] = n;
			}
			auto numeroDeUid = [&](uint64 uid) -> uint64 {
				for (uint32 i = 0; i < ids.Size(); ++i) {
					if (uid != 0u && scene.Uid(ids[i]) == uid) {
						return numeros[i];
					}
				}
				return 0u; // hors du prefab : la reference ne le suit pas
			};

			// 2. La photo de chaque entite, dans le vocabulaire des noeuds.
			const int32 kInstance = scene.IndexComposantPhoto("NkInstancePrefab2D");
			for (uint32 i = 0; i < ids.Size(); ++i) {
				NkPhotoEntite e;
				scene.PhotographierEntite(ids[i], e);
				e.parentUid = i == 0u ? 0u : numeroDeUid(e.parentUid);
				e.uid = numeros[i];
				e.aMou = false;			// la matiere n'est pas dans un prefab (en-tete)
				e.corps.corpsId = 0u;
				if (kInstance >= 0) {
					e.extraPresents &= ~(1u << static_cast<uint32>(kInstance));
				}
				uint32 dec = 0;
				for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
					if ((e.extraPresents & (1u << k)) != 0u) {
						TraduireEntites(scene, k, e.extra.Data() + dec, numeroDeUid);
					}
					dec += scene.TailleComposantPhoto(k);
				}
				out.noeuds.PushBack(e);
			}
			out.disposition.Clear();
			for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
				NkDispositionPrefab d;
				d.nom = scene.NomComposantPhoto(k) != nullptr ? NkString(scene.NomComposantPhoto(k)) : NkString();
				d.taille = scene.TailleComposantPhoto(k);
				out.disposition.PushBack(d);
			}
			// 3. La source EST une instance (UE5, Unity) : chaque entite porte son
			//    noeud. Sans cela, une entite ajoutee a la source serait, a la
			//    propagation, un noeud NOUVEAU que l'instance-source n'a pas — et on
			//    lui en ferait une copie.
			for (uint32 i = 0; i < ids.Size(); ++i) {
				NkInstancePrefab2D ip;
				ip.prefab = idPrefab;
				ip.noeud = static_cast<uint32>(numeros[i]);
				ip.racine = racine;
				scene.Monde().Add<NkInstancePrefab2D>(ids[i], ip);
			}
			return true;
		}

		uint32 NkPrefabs2D::Creer(NkScene &scene, ecs::NkEntityId racine, const char *nom) {
			if (nom == nullptr || nom[0] == '\0') {
				return 0u;
			}
			if (const uint32 deja = Trouver(nom)) {
				// Le meme nom : c'est une NOUVELLE VERSION, pas un second prefab.
				MettreAJour(scene, deja, racine);
				return deja;
			}
			// La source DEVIENT la premiere instance (UE5, Unity) : la retoucher
			// puis « mettre a jour le prefab » est le geste attendu. Capturer la
			// marque ; il lui faut donc l'identifiant que le prefab va recevoir.
			const uint32 id = static_cast<uint32>(mPrefabs.Size()) + 1u;
			NkPrefab2D p;
			if (!Capturer(scene, racine, id, nullptr, p)) {
				return 0u;
			}
			p.nom = nom;
			p.revision = 1;
			mPrefabs.PushBack(p);
			return id;
		}

		ecs::NkEntityId NkPrefabs2D::Instancier(NkScene &scene, uint32 prefab, const NkVec2f &position) {
			const NkPrefab2D *p = Prefab(prefab);
			if (p == nullptr || p->noeuds.Size() == 0u) {
				return ecs::NkEntityId::Invalid();
			}
			const NkCarte carte = Carte(*p, scene);
			// Le monde de chaque noeud, depuis la racine posee a `position` :
			// RefaireEntites pose les mondes TELS QUELS (c'est ce qui rend une
			// photo exacte), il faut donc les lui donner justes.
			NkVector<NkPhotoEntite> lot;
			for (uint32 i = 0; i < p->noeuds.Size(); ++i) {
				NkPhotoEntite e = VersScene(carte, p->noeuds[i], scene);
				if (i == 0u) {
					e.transform.position = position;
					e.parentUid = 0u;
				} else {
					NkTransform2D parent = lot[0].transform;
					for (uint32 j = 0; j < i; ++j) {
						if (lot[j].uid == e.parentUid) {
							parent = lot[j].transform;
						}
					}
					e.transform = NkComposer2D(parent, e.local);
				}
				// Le corps nait LA, immobile : l'etat capture etait celui de la source.
				e.etatRigide.position = math::NkVec3f(e.transform.position.x, e.transform.position.y, 0.f);
				e.etatRigide.orientation.x = 0.f;
				e.etatRigide.orientation.y = 0.f;
				e.etatRigide.orientation.z = math::NkSin(e.transform.rotation * 0.5f);
				e.etatRigide.orientation.w = math::NkCos(e.transform.rotation * 0.5f);
				e.etatRigide.linearVelocity = math::NkVec3f(0.f, 0.f, 0.f);
				e.etatRigide.angularVelocity = math::NkVec3f(0.f, 0.f, 0.f);
				lot.PushBack(e);
			}
			// Des identites NEUVES : les numeros de noeud ne servent qu'a rebrancher
			// parents et references DANS l'instance.
			NkVector<ecs::NkEntityId> crees;
			scene.RefaireEntites(lot, &crees, true);
			if (crees.Size() == 0u) {
				return ecs::NkEntityId::Invalid();
			}
			for (uint32 i = 0; i < crees.Size(); ++i) {
				NkInstancePrefab2D ip;
				ip.prefab = prefab;
				ip.noeud = static_cast<uint32>(p->noeuds[i].uid);
				ip.racine = crees[0];
				scene.Monde().Add<NkInstancePrefab2D>(crees[i], ip);
			}
			return crees[0];
		}

		uint32 NkPrefabs2D::MettreAJour(NkScene &scene, uint32 prefab, ecs::NkEntityId source) {
			if (Prefab(prefab) == nullptr) {
				return 0u;
			}
			const NkPrefab2D ancien = mPrefabs[prefab - 1u];
			NkPrefab2D nouveau;
			if (!Capturer(scene, source, prefab, &ancien, nouveau)) {
				return 0u;
			}
			nouveau.nom = ancien.nom;
			nouveau.revision = ancien.revision + 1u;
			mPrefabs[prefab - 1u] = nouveau;
			return Propager(scene, prefab, ancien);
		}

		uint32 NkPrefabs2D::Propager(NkScene &scene, uint32 prefab, const NkPrefab2D &ancien) {
			const NkPrefab2D &nouveau = mPrefabs[prefab - 1u];
			const NkCarte carteA = Carte(ancien, scene);
			const NkCarte carteN = Carte(nouveau, scene);
			const int32 kInstance = scene.IndexComposantPhoto("NkInstancePrefab2D");
			ecs::NkWorld &w = scene.Monde();
			NkVector<ecs::NkEntityId> racines;
			Instances(scene, prefab, racines);
			uint32 faites = 0;
			NkVector<NkMembre> membres;
			NkVector<uint8> bi;
			NkVector<uint8> ba;
			NkVector<uint8> bn;
			for (uint32 r = 0; r < racines.Size(); ++r) {
				const ecs::NkEntityId racine = racines[r];
				Membres(scene, prefab, racine, membres);
				// Les numeros de noeud du modele -> les identites de CETTE instance.
				auto versUid = [&](uint64 numero) -> uint64 {
					return numero != 0u ? scene.Uid(MembreDe(membres, numero)) : 0u;
				};

				for (uint32 i = 0; i < nouveau.noeuds.Size(); ++i) {
					const NkPhotoEntite &n = nouveau.noeuds[i];
					const NkPhotoEntite *a = Noeud(ancien, n.uid);
					ecs::NkEntityId e = MembreDe(membres, n.uid);

					if (!w.IsAlive(e)) {
						// Un noeud NOUVEAU dans le modele : il nait dans l'instance,
						// sous l'entite de son noeud parent.
						const ecs::NkEntityId parent = MembreDe(membres, n.parentUid);
						if (i == 0u || !w.IsAlive(parent)) {
							continue;
						}
						NkPhotoEntite pe = VersScene(carteN, n, scene);
						pe.uid = 0u;
						pe.parentUid = 0u;
						pe.transform = NkComposer2D(*w.Get<NkTransform2D>(parent), n.local);
						pe.etatRigide.position = math::NkVec3f(pe.transform.position.x, pe.transform.position.y, 0.f);
						pe.etatRigide.linearVelocity = math::NkVec3f(0.f, 0.f, 0.f);
						uint32 dec = 0;
						for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
							if ((pe.extraPresents & (1u << k)) != 0u) {
								TraduireEntites(scene, k, pe.extra.Data() + dec, versUid);
							}
							dec += scene.TailleComposantPhoto(k);
						}
						NkVector<NkPhotoEntite> un;
						un.PushBack(pe);
						NkVector<ecs::NkEntityId> crees;
						scene.RefaireEntites(un, &crees, true);
						if (crees.Size() == 1u) {
							scene.Rattacher(crees[0], parent, true);
							NkInstancePrefab2D ip;
							ip.prefab = prefab;
							ip.noeud = static_cast<uint32>(n.uid);
							ip.racine = racine;
							w.Add<NkInstancePrefab2D>(crees[0], ip);
							NkMembre m;
							m.noeud = n.uid;
							m.entite = crees[0];
							membres.PushBack(m);
						}
						continue;
					}

					// --- Les composants d'Unkeny, champ par champ ----------------
					NkPhotoEntite vide;
					const NkPhotoEntite &av = a != nullptr ? *a : vide;
					{
						bool aI = w.Has<NkEtiquette>(e);
						NkEtiquette v = aI ? *w.Get<NkEtiquette>(e) : NkEtiquette();
						if (FusionnerComposant(aI, v, av.aEtiquette, av.etiquette, n.aEtiquette, n.etiquette, kChampsEtiquette,
											   NbChamps(kChampsEtiquette))) {
							PoserOuRetirer(w, e, aI, v);
						}
					}
					{
						bool aI = w.Has<NkSprite2D>(e);
						NkSprite2D v = aI ? *w.Get<NkSprite2D>(e) : NkSprite2D();
						if (FusionnerComposant(aI, v, av.aSprite, av.sprite, n.aSprite, n.sprite, kChampsSprite,
											   NbChamps(kChampsSprite))) {
							PoserOuRetirer(w, e, aI, v);
						}
					}
					bool refaireCorps = false;
					{
						bool aI = w.Has<NkCollisionneur2D>(e);
						NkCollisionneur2D v = aI ? *w.Get<NkCollisionneur2D>(e) : NkCollisionneur2D();
						if (FusionnerComposant(aI, v, av.aCollisionneur, av.collisionneur, n.aCollisionneur, n.collisionneur,
											   kChampsCollisionneur, NbChamps(kChampsCollisionneur))) {
							if (!aI) {
								scene.RetirerCorps(e); // pas de corps sans forme (regle de l'editeur)
							}
							PoserOuRetirer(w, e, aI, v);
							refaireCorps = true;
						}
					}
					{
						bool aI = w.Has<NkCorps2D>(e);
						NkCorps2D v = aI ? *w.Get<NkCorps2D>(e) : NkCorps2D();
						const bool avant = aI;
						if (FusionnerComposant(aI, v, av.aCorps, av.corps, n.aCorps, n.corps, kChampsCorps,
											   NbChamps(kChampsCorps))) {
							if (!aI) {
								scene.RetirerCorps(e);
							} else if (!avant) {
								v.corpsId = 0u;
								scene.AjouterCorps(e, v);
							} else {
								w.Add<NkCorps2D>(e, v);
								refaireCorps = true;
							}
						}
						if (refaireCorps && w.Has<NkCorps2D>(e)) {
							scene.ActualiserCorps(e);
						}
					}
					// --- La place : le local d'un enfant, l'echelle d'une racine ---
					if (i == 0u) {
						NkTransform2D t = *w.Get<NkTransform2D>(e);
						if (Fusionner(kChampsRacine, NbChamps(kChampsRacine), reinterpret_cast<uint8 *>(&t),
									  reinterpret_cast<const uint8 *>(&av.transform), reinterpret_cast<const uint8 *>(&n.transform))) {
							*w.Get<NkTransform2D>(e) = t;
						}
					} else {
						const ecs::NkEntityId parentModele = MembreDe(membres, n.parentUid);
						const ecs::NkEntityId parentAncien = a != nullptr ? MembreDe(membres, a->parentUid) : parentModele;
						// Le modele a change de parent, et l'instance avait garde
						// l'ancien : elle suit. Rattachee ailleurs a la main : elle reste.
						if (w.IsAlive(parentModele) && scene.Parent(e) != parentModele && scene.Parent(e) == parentAncien) {
							scene.Rattacher(e, parentModele, true);
						}
						if (const NkTransform2D *loc = scene.Local(e)) {
							NkTransform2D l = *loc;
							if (a != nullptr &&
								Fusionner(kChampsLocal, NbChamps(kChampsLocal), reinterpret_cast<uint8 *>(&l),
										  reinterpret_cast<const uint8 *>(&a->local), reinterpret_cast<const uint8 *>(&n.local))) {
								scene.PoserLocal(e, l);
							}
						}
					}
					// --- Les composants du jeu -----------------------------------
					NkPhotoEntite pi;
					scene.PhotographierEntite(e, pi);
					uint32 dec = 0;
					for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
						const uint32 t = scene.TailleComposantPhoto(k);
						const uint32 decK = dec;
						dec += t;
						if (static_cast<int32>(k) == kInstance) {
							continue; // le lien a son prefab n'est pas une donnee du modele
						}
						const uint8 *srcA = a != nullptr ? OctetsNoeud(carteA, *a, k) : nullptr;
						const uint8 *srcN = OctetsNoeud(carteN, n, k);
						const bool aI = (pi.extraPresents & (1u << k)) != 0u;
						// Les deux versions du modele, dans le vocabulaire de l'instance.
						ba.Resize(t);
						bn.Resize(t);
						if (srcA != nullptr) {
							std::memcpy(ba.Data(), srcA, t);
							TraduireEntites(scene, k, ba.Data(), versUid);
						}
						if (srcN != nullptr) {
							std::memcpy(bn.Data(), srcN, t);
							TraduireEntites(scene, k, bn.Data(), versUid);
						}
						bi.Resize(t);
						std::memcpy(bi.Data(), pi.extra.Data() + decK, t);
						uint32 nb = 0;
						const NkChampSauve *champs = scene.ChampsComposantPhoto(k, nb);
						if (srcA != nullptr && srcN != nullptr) {
							if (!aI) {
								continue; // retire a la main de l'instance : surcharge
							}
							bool change = false;
							if (champs != nullptr) {
								change = Fusionner(champs, nb, bi.Data(), ba.Data(), bn.Data());
							} else if (std::memcmp(bi.Data(), ba.Data(), t) == 0 && std::memcmp(bi.Data(), bn.Data(), t) != 0) {
								std::memcpy(bi.Data(), bn.Data(), t); // non decrit : le composant entier
								change = true;
							}
							if (change) {
								scene.EcrireComposantPhoto(k, e, bi.Data());
							}
						} else if (srcA == nullptr && srcN != nullptr && !aI) {
							scene.EcrireComposantPhoto(k, e, bn.Data());
						} else if (srcA != nullptr && srcN == nullptr && aI) {
							// Le modele l'a perdu : l'instance aussi, sauf si on l'y a retouche.
							bool touche = false;
							if (champs != nullptr) {
								for (uint32 c = 0; c < nb; ++c) {
									touche = touche || (!champs[c].transitoire && !NkChampEgal(champs[c], bi.Data(), ba.Data()));
								}
							} else {
								touche = std::memcmp(bi.Data(), ba.Data(), t) != 0;
							}
							if (!touche) {
								scene.RetirerComposantPhoto(k, e);
							}
						}
					}
				}
				// Les noeuds que le modele n'a plus : partis de l'instance aussi.
				for (uint32 m = 0; m < membres.Size(); ++m) {
					if (Noeud(nouveau, membres[m].noeud) == nullptr && w.IsAlive(membres[m].entite)) {
						scene.Detruire(membres[m].entite);
					}
				}
				++faites;
			}
			scene.PropagerHierarchie();
			return faites;
		}

		void NkPrefabs2D::Instances(NkScene &scene, uint32 prefab, NkVector<ecs::NkEntityId> &out) {
			out.Clear();
			ecs::NkWorld &w = scene.Monde();
			w.Query<NkInstancePrefab2D>().ForEach([&](ecs::NkEntityId, NkInstancePrefab2D &ip) {
				if (ip.prefab != prefab || !w.IsAlive(ip.racine)) {
					return;
				}
				for (uint32 i = 0; i < out.Size(); ++i) {
					if (out[i] == ip.racine) {
						return;
					}
				}
				out.PushBack(ip.racine);
			});
		}

		void NkPrefabs2D::Surcharges(NkScene &scene, ecs::NkEntityId entite, NkVector<NkString> &out) {
			out.Clear();
			ecs::NkWorld &w = scene.Monde();
			const NkInstancePrefab2D *ip = w.Get<NkInstancePrefab2D>(entite);
			const NkPrefab2D *p = ip != nullptr ? Prefab(ip->prefab) : nullptr;
			const NkPhotoEntite *n = p != nullptr ? Noeud(*p, ip->noeud) : nullptr;
			if (n == nullptr) {
				return;
			}
			const bool racine = p->noeuds.Size() > 0u && p->noeuds[0].uid == n->uid;
			auto presence = [&](const char *nom, bool inst, bool modele) {
				if (inst != modele) {
					out.PushBack(NkString::Format("%s (%s)", nom, inst ? "ajoute" : "retire"));
				}
				return inst && modele;
			};
			if (presence("NkEtiquette", w.Has<NkEtiquette>(entite), n->aEtiquette)) {
				Differences("NkEtiquette", kChampsEtiquette, NbChamps(kChampsEtiquette),
							reinterpret_cast<const uint8 *>(w.Get<NkEtiquette>(entite)), reinterpret_cast<const uint8 *>(&n->etiquette), out);
			}
			if (presence("NkSprite2D", w.Has<NkSprite2D>(entite), n->aSprite)) {
				Differences("NkSprite2D", kChampsSprite, NbChamps(kChampsSprite),
							reinterpret_cast<const uint8 *>(w.Get<NkSprite2D>(entite)), reinterpret_cast<const uint8 *>(&n->sprite), out);
			}
			if (presence("NkCollisionneur2D", w.Has<NkCollisionneur2D>(entite), n->aCollisionneur)) {
				Differences("NkCollisionneur2D", kChampsCollisionneur, NbChamps(kChampsCollisionneur),
							reinterpret_cast<const uint8 *>(w.Get<NkCollisionneur2D>(entite)),
							reinterpret_cast<const uint8 *>(&n->collisionneur), out);
			}
			if (presence("NkCorps2D", w.Has<NkCorps2D>(entite), n->aCorps)) {
				Differences("NkCorps2D", kChampsCorps, NbChamps(kChampsCorps),
							reinterpret_cast<const uint8 *>(w.Get<NkCorps2D>(entite)), reinterpret_cast<const uint8 *>(&n->corps), out);
			}
			if (racine) {
				Differences("NkTransform2D", kChampsRacine, NbChamps(kChampsRacine),
							reinterpret_cast<const uint8 *>(w.Get<NkTransform2D>(entite)),
							reinterpret_cast<const uint8 *>(&n->transform), out);
			} else if (const NkTransform2D *l = scene.Local(entite)) {
				Differences("NkLocal2D", kChampsLocal, NbChamps(kChampsLocal), reinterpret_cast<const uint8 *>(l),
							reinterpret_cast<const uint8 *>(&n->local), out);
			}
			const NkCarte carte = Carte(*p, scene);
			const int32 kInstance = scene.IndexComposantPhoto("NkInstancePrefab2D");
			NkVector<NkMembre> membres;
			Membres(scene, ip->prefab, ip->racine, membres);
			auto versUid = [&](uint64 numero) -> uint64 {
				return numero != 0u ? scene.Uid(MembreDe(membres, numero)) : 0u;
			};
			NkPhotoEntite pi;
			scene.PhotographierEntite(entite, pi);
			NkVector<uint8> bm;
			uint32 dec = 0;
			for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
				const uint32 t = scene.TailleComposantPhoto(k);
				const uint32 decK = dec;
				dec += t;
				if (static_cast<int32>(k) == kInstance) {
					continue;
				}
				const char *nom = scene.NomComposantPhoto(k) != nullptr ? scene.NomComposantPhoto(k) : "?";
				const uint8 *src = OctetsNoeud(carte, *n, k);
				const bool aI = (pi.extraPresents & (1u << k)) != 0u;
				if (!presence(nom, aI, src != nullptr)) {
					continue;
				}
				bm.Resize(t);
				std::memcpy(bm.Data(), src, t);
				TraduireEntites(scene, k, bm.Data(), versUid);
				uint32 nb = 0;
				const NkChampSauve *champs = scene.ChampsComposantPhoto(k, nb);
				if (champs != nullptr) {
					Differences(nom, champs, nb, pi.extra.Data() + decK, bm.Data(), out);
				} else if (std::memcmp(pi.extra.Data() + decK, bm.Data(), t) != 0) {
					out.PushBack(NkString(nom));
				}
			}
		}

		// =====================================================================
		// Le fichier .nkprefab
		// =====================================================================
		bool NkPrefabs2D::EnregistrerJSON(NkScene &scene, uint32 prefab, NkString &json, const NkRessourcesScene &ressources) {
			const NkPrefab2D *p = Prefab(prefab);
			if (p == nullptr) {
				return false;
			}
			const NkCarte carte = Carte(*p, scene);
			NkArchive a;
			a.SetString(V("format"), V("unkeny.prefab"));
			a.SetInt32(V("version"), 1);
			a.SetString(V("nom"), p->nom.View());
			a.SetUInt32(V("revision"), p->revision);
			a.SetUInt32(V("prochainNoeud"), p->prochainNoeud);
			NkVector<NkArchive> noeuds;
			for (uint32 i = 0; i < p->noeuds.Size(); ++i) {
				noeuds.PushBack(interne::EcrireEntite(VersScene(carte, p->noeuds[i], scene), scene, ressources));
			}
			a.SetObjectArray(V("noeuds"), noeuds);
			return NkJSONWriter::WriteArchive(a, json, true, 1);
		}

		bool NkPrefabs2D::Enregistrer(NkScene &scene, uint32 prefab, const char *chemin, const NkRessourcesScene &ressources) {
			NkString json;
			if (chemin == nullptr || !EnregistrerJSON(scene, prefab, json, ressources)) {
				return false;
			}
			if (!NkFile::WriteAllText(chemin, json.CStr())) {
				logger.Warn("[unkeny] ecriture du prefab impossible : {0}", chemin);
				return false;
			}
			return true;
		}

		bool NkPrefabs2D::ChargerJSON(NkScene &scene, const char *nom, NkStringView json, const NkRessourcesScene &ressources,
									  uint32 &id, NkString *erreur) {
			id = 0u;
			auto echec = [&](const char *pourquoi) {
				if (erreur != nullptr) {
					*erreur = pourquoi;
				}
				logger.Warn("[unkeny] prefab non charge : {0}", pourquoi);
				return false;
			};
			NkArchive a;
			NkString err;
			if (!NkJSONReader::ReadArchive(json, a, &err)) {
				return echec("JSON illisible");
			}
			NkString format;
			if (!a.GetString(V("format"), format) || !(format == "unkeny.prefab")) {
				return echec("ce n'est pas un prefab Unkeny (champ \"format\")");
			}
			nk_int32 version = 0;
			if (!a.GetInt32(V("version"), version) || version < 1 || version > 1) {
				return echec("version de prefab inconnue");
			}
			NkPrefab2D p;
			p.nom = nom != nullptr && nom[0] != '\0' ? NkString(nom) : NkString();
			if (p.nom.Empty() && !a.GetString(V("nom"), p.nom)) {
				return echec("prefab sans nom");
			}
			nk_uint32 u = 0;
			if (a.GetUInt32(V("revision"), u)) {
				p.revision = u;
			}
			if (a.GetUInt32(V("prochainNoeud"), u)) {
				p.prochainNoeud = u;
			}
			NkVector<NkArchive> noeuds;
			(void)a.GetObjectArray(V("noeuds"), noeuds);
			for (uint32 i = 0; i < noeuds.Size(); ++i) {
				NkPhotoEntite e;
				if (!interne::LireEntite(noeuds[i], scene, ressources, e, err)) {
					return echec(err.CStr());
				}
				if (e.uid == 0u) {
					return echec("noeud sans numero");
				}
				if (e.uid >= p.prochainNoeud) {
					p.prochainNoeud = static_cast<uint32>(e.uid) + 1u;
				}
				p.noeuds.PushBack(e);
			}
			if (p.noeuds.Size() == 0u) {
				return echec("prefab vide");
			}
			for (uint32 k = 0; k < scene.NbComposantsPhoto(); ++k) {
				NkDispositionPrefab d;
				d.nom = scene.NomComposantPhoto(k) != nullptr ? NkString(scene.NomComposantPhoto(k)) : NkString();
				d.taille = scene.TailleComposantPhoto(k);
				p.disposition.PushBack(d);
			}
			if (const uint32 deja = Trouver(p.nom.CStr())) {
				// Le fichier a change : les instances suivent, leurs surcharges gardees.
				const NkPrefab2D ancien = mPrefabs[deja - 1u];
				mPrefabs[deja - 1u] = p;
				Propager(scene, deja, ancien);
				id = deja;
				return true;
			}
			mPrefabs.PushBack(p);
			id = static_cast<uint32>(mPrefabs.Size());
			return true;
		}

		uint32 NkPrefabs2D::Charger(NkScene &scene, const char *chemin, const NkRessourcesScene &ressources, NkString *erreur) {
			if (chemin == nullptr || chemin[0] == '\0') {
				return 0u;
			}
			const NkString json = NkFile::ReadAllText(chemin);
			if (json.Empty()) {
				if (erreur != nullptr) {
					*erreur = NkString("fichier absent ou vide : ") + chemin;
				}
				return 0u;
			}
			uint32 id = 0u;
			return ChargerJSON(scene, chemin, json.View(), ressources, id, erreur) ? id : 0u;
		}

	} // namespace unkeny
} // namespace nkentseu
