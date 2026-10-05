//
// NkRigSuggestions.cpp
// =============================================================================
// Description :
//   Les conseils du rig (voir NkRigSuggestions.h).
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis (« Rihen »)
// Copyright: (c) 2022-2026 TEUGUIA TADJUIDJE Rodolf Séderis — Rihen Universe.
//            Tous droits réservés. Logiciel propriétaire : voir LICENSE.
//            Copie, reproduction, modification, redistribution et usage par une
//            IA interdits sans autorisation écrite.
// =============================================================================

#include "NKAnima/Rig/NkRigSuggestions.h"
#include "NKAnima/Rig/NkRigMath.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace anim {

		using math::NkMat4f;
		using math::NkVec3f;
		using namespace rigm;

		const char *NkRigSuggestionKindName(NkRigSuggestionKind k) {
			switch (k) {
				case NkRigSuggestionKind::NK_RigSuggestionKind_Placement: return "placement";
				case NkRigSuggestionKind::NK_RigSuggestionKind_Naming:	  return "nommage";
				case NkRigSuggestionKind::NK_RigSuggestionKind_Weights:	  return "poids";
				case NkRigSuggestionKind::NK_RigSuggestionKind_Step:	  return "etape";
				default:												  return "?";
			}
		}

		namespace {
			/// « coude.L » -> « coude » (le nom d'un repere sans son cote).
			NkString SansCote(const char *id) {
				NkString s(id);
				if (s.EndsWith(".L") || s.EndsWith(".R")) {
					return NkString(id, (usize)(std::strlen(id) - 2u));
				}
				return s;
			}

			/// Les reperes d'EXTREMITE (sur la surface ou au-dela) : ils peuvent etre
			/// hors du volume sans etre faux.
			bool Extremite(const char *id) {
				static const char *const kBouts[] = {"sommet_tete", "main", "orteils", "museau", "queue_bout", "patte_av", "patte_ar", "debut", "fin"};
				const NkString base = SansCote(id);
				for (const char *b : kBouts) {
					if (base == NkString(b)) {
						return true;
					}
				}
				return false;
			}

			/// La hauteur du maillage (Y, dans le repere de l'objet), au moins 1e-3.
			float32 Hauteur(const NkRigDocument &d, const NkMat4f &objet) {
				const NkSkinMesh &m = d.mesh;
				if (m.VertexCount() == 0) {
					return 1.f;
				}
				float32 y0 = 1e30f;
				float32 y1 = -1e30f;
				for (uint32 v = 0; v < m.VertexCount(); ++v) {
					const float32 y = Point(objet, m.positions[v]).y;
					y0 = y < y0 ? y : y0;
					y1 = y > y1 ? y : y1;
				}
				return y1 - y0 > 1e-3f ? y1 - y0 : 1e-3f;
			}

			NkString Cote(const char *nom, bool gauche) {
				NkString s(nom);
				s.Append(gauche ? ".L" : ".R");
				return s;
			}

			// ── LA STRUCTURE D'UN SQUELETTE ─────────────────────────────────────
			struct Structure {
					NkVector<NkVec3f> tete;
					NkVector<NkVec3f> queue;
					NkVector<NkVector<int32>> enfants;
					float32 xc = 0.f;
					float32 yMin = 0.f;
					float32 h = 1.f;
			};

			void Lire(const NkArmature &a, const NkMat4f &objet, Structure &s) {
				const uint32 n = a.Count();
				s.tete.Resize(n);
				s.queue.Resize(n);
				s.enfants.Resize(n);
				float32 y0 = 1e30f;
				float32 y1 = -1e30f;
				for (uint32 i = 0; i < n; ++i) {
					s.tete[i] = Point(objet, a.bones[i].head);
					s.queue[i] = Point(objet, a.bones[i].tail);
					const float32 lo = s.tete[i].y < s.queue[i].y ? s.tete[i].y : s.queue[i].y;
					const float32 hi = s.tete[i].y > s.queue[i].y ? s.tete[i].y : s.queue[i].y;
					y0 = lo < y0 ? lo : y0;
					y1 = hi > y1 ? hi : y1;
					if (a.bones[i].parent >= 0 && (uint32)a.bones[i].parent < n) {
						s.enfants[(uint32)a.bones[i].parent].PushBack((int32)i);
					}
				}
				s.yMin = y0;
				s.h = y1 - y0 > 1e-4f ? y1 - y0 : 1e-4f;
			}

			uint32 TailleSousArbre(const Structure &s, int32 b) {
				uint32 n = 1;
				for (uint32 k = 0; k < (uint32)s.enfants[(uint32)b].Size(); ++k) {
					n += TailleSousArbre(s, s.enfants[(uint32)b][k]);
				}
				return n;
			}

			/// La feuille du sous-arbre de `b` qui maximise `score` (et le chemin vers elle).
			template <typename F>
			void MeilleureFeuille(const Structure &s, int32 b, F score, float32 &meilleur, int32 &feuille) {
				const float32 v = score(b);
				if (s.enfants[(uint32)b].Empty() && v > meilleur) {
					meilleur = v;
					feuille = b;
				}
				for (uint32 k = 0; k < (uint32)s.enfants[(uint32)b].Size(); ++k) {
					MeilleureFeuille(s, s.enfants[(uint32)b][k], score, meilleur, feuille);
				}
			}

			/// Le chemin de `de` (inclus) a `vers` (inclus), par les parents de `vers`.
			NkVector<int32> Chemin(const NkArmature &a, int32 de, int32 vers) {
				NkVector<int32> inverse;
				for (int32 c = vers, garde = 0; c >= 0 && garde < 512; c = a.bones[(uint32)c].parent, ++garde) {
					inverse.PushBack(c);
					if (c == de) {
						break;
					}
				}
				NkVector<int32> r;
				for (int32 i = (int32)inverse.Size() - 1; i >= 0; --i) {
					r.PushBack(inverse[(uint32)i]);
				}
				return r;
			}

			bool Dans(const NkVector<int32> &v, int32 x) {
				for (uint32 i = 0; i < (uint32)v.Size(); ++i) {
					if (v[i] == x) {
						return true;
					}
				}
				return false;
			}
		} // namespace

		uint32 NkRigGuessHumanoidNames(const NkArmature &a, const NkMat4f &objet, NkVector<NkString> &noms) {
			const uint32 n = a.Count();
			noms.Clear();
			noms.Resize(n);
			if (n < 5) {
				return 0;
			}
			Structure s;
			Lire(a, objet, s);
			// LA RACINE : celle qui porte le plus d'os.
			int32 racine = -1;
			uint32 plus = 0;
			for (uint32 i = 0; i < n; ++i) {
				if (a.bones[i].parent < 0) {
					const uint32 t = TailleSousArbre(s, (int32)i);
					if (t > plus) {
						plus = t;
						racine = (int32)i;
					}
				}
			}
			if (racine < 0) {
				return 0;
			}
			s.xc = s.tete[(uint32)racine].x;
			// LA COLONNE : de la racine a la feuille CENTRALE la plus haute.
			float32 meilleur = -1e30f;
			int32 sommet = -1;
			MeilleureFeuille(
				s, racine,
				[&](int32 b) {
					const float32 x = (s.tete[(uint32)b].x + s.queue[(uint32)b].x) * 0.5f;
					return std::fabs(x - s.xc) < 0.08f * s.h ? s.queue[(uint32)b].y : -1e30f;
				},
				meilleur, sommet);
			if (sommet < 0) {
				return 0;
			}
			NkVector<int32> colonne = Chemin(a, racine, sommet);
			// Une racine AU SOL (le « root » d'UE5) n'est pas le bassin.
			uint32 debut = 0;
			if (colonne.Size() >= 4u && s.tete[(uint32)colonne[0]].y < s.yMin + 0.25f * s.h) {
				noms[(uint32)colonne[0]] = NkString("root");
				debut = 1;
			}
			const uint32 m = (uint32)colonne.Size() - debut;
			if (m < 2) {
				return 0;
			}
			noms[(uint32)colonne[debut]] = NkString("hips");
			noms[(uint32)colonne[debut + m - 1u]] = NkString("head");
			if (m >= 3) {
				noms[(uint32)colonne[debut + m - 2u]] = NkString("neck");
			}
			for (uint32 k = 1; k + 2u < m; ++k) {
				const int32 b = colonne[debut + k];
				if (k == 1) {
					noms[(uint32)b] = NkString("spine");
				} else if (k == 2) {
					noms[(uint32)b] = NkString("chest");
				} else {
					noms[(uint32)b] = NkString::Format("chest.%03u", k - 2u);
				}
			}
			// LES MEMBRES : les branches de la colonne qui ne sont pas la colonne.
			uint32 bras[2] = {0, 0};
			uint32 jambes[2] = {0, 0};
			const float32 hanches = s.tete[(uint32)colonne[debut]].y;
			for (uint32 c = debut; c < (uint32)colonne.Size(); ++c) {
				const int32 os = colonne[c];
				for (uint32 k = 0; k < (uint32)s.enfants[(uint32)os].Size(); ++k) {
					const int32 e = s.enfants[(uint32)os][k];
					if (Dans(colonne, e)) {
						continue;
					}
					// Une JAMBE descend sous les hanches ; un BRAS part sur le cote.
					float32 bas = -1e30f; // le max de -y : le point le plus BAS
					int32 pied = -1;
					MeilleureFeuille(
						s, e,
						[&](int32 b) {
							const float32 y = s.queue[(uint32)b].y < s.tete[(uint32)b].y ? s.queue[(uint32)b].y : s.tete[(uint32)b].y;
							return -y;
						},
						bas, pied);
					float32 loin = -1e30f;
					int32 main = -1;
					MeilleureFeuille(
						s, e, [&](int32 b) { return std::fabs(s.queue[(uint32)b].x - s.xc); }, loin, main);
					const bool jambe = pied >= 0 && -bas < hanches - 0.25f * s.h;
					const bool unBras = !jambe && main >= 0 && loin > 0.15f * s.h;
					if (!jambe && !unBras) {
						continue;
					}
					const int32 bout = jambe ? pied : main;
					const bool gauche = s.queue[(uint32)bout].x > s.xc;
					const NkVector<int32> chaine = Chemin(a, e, bout);
					const uint32 nc = (uint32)chaine.Size();
					static const char *const kBras4[4] = {"shoulder", "upperarm", "forearm", "hand"};
					static const char *const kBras3[3] = {"upperarm", "forearm", "hand"};
					static const char *const kJambe4[4] = {"thigh", "shin", "foot", "toe"};
					static const char *const kJambe3[3] = {"thigh", "shin", "foot"};
					const char *const *table = jambe ? (nc >= 4 ? kJambe4 : kJambe3) : (nc >= 4 ? kBras4 : kBras3);
					const uint32 nt = nc >= 4 ? 4u : 3u;
					const char *dernier = table[nt - 1u];
					for (uint32 q = 0; q < nc; ++q) {
						// Au-dela du gabarit (des doigts d'un seul tenant) : « hand.001 »...
						const NkString base = q < nt ? NkString(table[q]) : NkString::Format("%s.%03u", dernier, q - nt + 1u);
						noms[(uint32)chaine[q]] = Cote(base.CStr(), gauche);
					}
					(jambe ? jambes : bras)[gauche ? 0 : 1] += 1u;
				}
			}
			const bool humanoide = bras[0] == 1u && bras[1] == 1u && jambes[0] == 1u && jambes[1] == 1u;
			if (!humanoide) {
				for (uint32 i = 0; i < n; ++i) {
					noms[i] = NkString();
				}
				return 0;
			}
			uint32 nommes = 0;
			for (uint32 i = 0; i < n; ++i) {
				nommes += noms[i].Empty() ? 0u : 1u;
			}
			return nommes;
		}

		// =====================================================================
		// LES CONSEILS
		// =====================================================================
		namespace {
			void Placement(const NkRigDocument &d, const NkRigSuggestOptions &o, float32 h, NkVector<NkRigSuggestion> &out) {
				const NkVector<NkRigLandmark> &l = d.landmarks;
				// LA SYMETRIE des paires .L / .R.
				for (uint32 i = 0; i < (uint32)l.Size(); ++i) {
					if (NkArmature::SideOfName(l[i].id.CStr()) != NkBoneSide::NK_BoneSide_Gauche) {
						continue;
					}
					const int32 j = NkRigFindLandmark(l, NkArmature::MirrorName(l[i].id.CStr()).CStr());
					if (j < 0) {
						continue;
					}
					const NkRigLandmark &g = l[i];
					const NkRigLandmark &dr = l[(uint32)j];
					const float32 ecart = Dist(g.position, MiroirX(dr.position));
					if (ecart <= o.symmetryTolerance * h) {
						continue;
					}
					// Le repere deplace A LA MAIN fait reference ; sinon, la moyenne.
					NkVec3f pg = Lerp(g.position, MiroirX(dr.position), 0.5f);
					if (g.manuel && !dr.manuel) {
						pg = g.position;
					} else if (dr.manuel && !g.manuel) {
						pg = MiroirX(dr.position);
					}
					NkRigSuggestion s;
					s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Placement;
					s.key = NkString::Format("placement:symetrie:%s", SansCote(g.id.CStr()).CStr());
					s.title = NkString::Format("Rendre symétriques « %s » et « %s »", g.label.CStr(), dr.label.CStr());
					s.detail = NkString::Format("Écart de %.1f cm entre la paire et son miroir X%s.", (double)(ecart * 100.f),
												(g.manuel != dr.manuel) ? " ; le repère placé à la main fait référence" : " ; les deux vont à leur moyenne");
					s.confidence = 0.95f;
					s.action.kind = NkRigActionKind::NK_RigActionKind_Move_Landmarks;
					s.action.ids.PushBack(g.id);
					s.action.positions.PushBack(pg);
					s.action.ids.PushBack(dr.id);
					s.action.positions.PushBack(MiroirX(pg));
					out.PushBack(s);
				}
				// LES REPERES INTERIEURS sortis du volume.
				if (d.mesh.TriangleCount() == 0) {
					return;
				}
				for (uint32 i = 0; i < (uint32)l.Size(); ++i) {
					if (Extremite(l[i].id.CStr()) || d.mesh.Contains(l[i].position)) {
						continue;
					}
					// La SURFACE la plus proche, puis le centre de ses voisins : la coupe
					// locale du membre (un genou pousse de 30 cm devant la jambe n'a
					// aucun sommet autour de lui -- son voisin de surface, si).
					float32 dMin = 1e30f;
					uint32 proche = 0;
					for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
						const float32 dv = Dist(d.mesh.positions[v], l[i].position);
						if (dv < dMin) {
							dMin = dv;
							proche = v;
						}
					}
					const float32 r = 0.06f * h;
					NkVec3f somme = V(0.f, 0.f, 0.f);
					uint32 nb = 0;
					for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
						if (Dist(d.mesh.positions[v], d.mesh.positions[proche]) < r) {
							somme = Add(somme, d.mesh.positions[v]);
							++nb;
						}
					}
					if (nb == 0) {
						continue;
					}
					const NkVec3f c = Mul(somme, 1.f / (float32)nb);
					if (!d.mesh.Contains(c)) {
						continue;
					}
					NkRigSuggestion s;
					s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Placement;
					s.key = NkString::Format("placement:volume:%s", l[i].id.CStr());
					s.title = NkString::Format("Ramener « %s » dans le volume", l[i].label.CStr());
					s.detail = NkString::Format("Le repère est hors du maillage ; le centre des %u sommets de la surface la plus proche (à %.1f cm) "
												"est dedans.",
												nb, (double)(Dist(c, l[i].position) * 100.f));
					s.confidence = 0.85f;
					s.action.kind = NkRigActionKind::NK_RigActionKind_Move_Landmarks;
					s.action.ids.PushBack(l[i].id);
					s.action.positions.PushBack(c);
					out.PushBack(s);
				}
			}

			void Nommage(const NkRigDocument &d, const NkRigSuggestOptions &o, float32 h, NkVector<NkRigSuggestion> &out) {
				const NkArmature &a = d.armature;
				if (a.Count() == 0) {
					return;
				}
				// LE GABARIT HUMANOIDE, reconnu par la structure.
				NkVector<NkString> noms;
				NkVector<uint8> couvert;
				couvert.Resize(a.Count(), 0);
				if (NkRigGuessHumanoidNames(a, o.objet, noms) > 0) {
					NkRigSuggestion s;
					s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Naming;
					s.key = NkString("nommage:gabarit");
					s.confidence = 0.8f;
					s.action.kind = NkRigActionKind::NK_RigActionKind_Rename_Bones;
					NkString exemples;
					for (uint32 i = 0; i < a.Count(); ++i) {
						if (noms[i].Empty()) {
							continue;
						}
						couvert[i] = 1;
						if (noms[i] == a.bones[i].name) {
							continue;
						}
						s.action.bones.PushBack((int32)i);
						s.action.names.PushBack(noms[i]);
						if (s.action.bones.Size() <= 3u) {
							exemples.Append(NkString::Format("%s%s -> %s", exemples.Empty() ? "" : ", ", a.bones[i].name.CStr(), noms[i].CStr()).CStr());
						}
					}
					if (!s.action.bones.Empty()) {
						s.title = NkString::Format("Renommer %u os selon le gabarit humanoïde", (uint32)s.action.bones.Size());
						const uint32 autres = (uint32)s.action.bones.Size() > 3u ? (uint32)s.action.bones.Size() - 3u : 0u;
						s.detail = NkString::Format("Structure reconnue (colonne, deux bras, deux jambes) : %s%s. Les pistes suivent leurs os ; "
													"les noms du gabarit donnent ensuite les contrôles IK.",
													exemples.CStr(), autres > 0 ? NkString::Format(" (et %u autres)", autres).CStr() : "");
						out.PushBack(s);
					}
				}
				// LE COTE MANQUANT d'un os hors du milieu.
				{
					NkRigSuggestion s;
					s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Naming;
					s.key = NkString("nommage:cote");
					s.confidence = 0.75f;
					s.action.kind = NkRigActionKind::NK_RigActionKind_Rename_Bones;
					for (uint32 i = 0; i < a.Count(); ++i) {
						if (couvert[i] != 0 || NkArmature::SideOfName(a.bones[i].name.CStr()) != NkBoneSide::NK_BoneSide_Centre) {
							continue;
						}
						const NkVec3f mil = Point(o.objet, Lerp(a.bones[i].head, a.bones[i].tail, 0.5f));
						if (std::fabs(mil.x) < 0.05f * h) {
							continue;
						}
						s.action.bones.PushBack((int32)i);
						s.action.names.PushBack(Cote(a.bones[i].name.CStr(), mil.x > 0.f));
					}
					if (!s.action.bones.Empty()) {
						s.title = NkString::Format("Nommer le côté de %u os (.L / .R)", (uint32)s.action.bones.Size());
						s.detail = NkString::Format("« %s » est hors du milieu sans côté dans son nom : la symétrie X, les couleurs et le miroir "
													"des poids l'ignorent.", a.bones[(uint32)s.action.bones[0]].name.CStr());
						out.PushBack(s);
					}
				}
				// LE MIROIR MANQUANT (le premier os de la chaine seulement : son
				// miroir entraine ses descendants de cote).
				for (uint32 i = 0; i < a.Count(); ++i) {
					// Un os que le gabarit renomme : son conseil a lui couvre le cas.
					if (couvert[i] != 0) {
						continue;
					}
					const NkString m = NkArmature::MirrorName(a.bones[i].name.CStr());
					if (m.Empty() || a.Find(m.CStr()) >= 0) {
						continue;
					}
					const int32 p = a.bones[i].parent;
					if (p >= 0) {
						const NkString mp = NkArmature::MirrorName(a.bones[(uint32)p].name.CStr());
						if (!mp.Empty() && a.Find(mp.CStr()) < 0) {
							continue;
						}
					}
					// LE PENDANT EXISTE SOUS UN AUTRE NOM (CesiumMan : « Skeleton_arm_joint_L__4_ »
					// face a « Skeleton_arm_joint_R ») : c'est un NOM a corriger, pas un os a creer.
					const NkVec3f th = MiroirX(Point(o.objet, a.bones[i].head));
					const NkVec3f tt = MiroirX(Point(o.objet, a.bones[i].tail));
					int32 pendant = -1;
					for (uint32 j = 0; j < a.Count() && pendant < 0; ++j) {
						const bool face = j != i && Dist(Point(o.objet, a.bones[j].head), th) < 0.02f * h &&
										  Dist(Point(o.objet, a.bones[j].tail), tt) < 0.02f * h;
						pendant = face ? (int32)j : -1;
					}
					if (pendant >= 0) {
						NkRigSuggestion s;
						s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Naming;
						s.key = NkString::Format("nommage:pendant:%s", a.bones[i].name.CStr());
						s.title = NkString::Format("Renommer « %s » en « %s » (le pendant de « %s »)", a.bones[(uint32)pendant].name.CStr(), m.CStr(),
												   a.bones[i].name.CStr());
						s.detail = NkString("Les deux os sont en miroir X mais leurs noms ne se répondent pas : la symétrie et le miroir des poids "
											"ne les apparient pas.");
						s.confidence = 0.7f;
						s.action.kind = NkRigActionKind::NK_RigActionKind_Rename_Bones;
						s.action.bones.PushBack(pendant);
						s.action.names.PushBack(m);
						out.PushBack(s);
						continue;
					}
					NkRigSuggestion s;
					s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Naming;
					s.key = NkString::Format("nommage:miroir:%s", a.bones[i].name.CStr());
					s.title = NkString::Format("Créer le miroir de « %s » (%s)", a.bones[i].name.CStr(), m.CStr());
					s.detail = NkString("Un os de côté sans son pendant : la symétrie X le crée (avec ses descendants de côté).");
					s.confidence = 0.6f;
					s.action.kind = NkRigActionKind::NK_RigActionKind_Symmetrize_Bone;
					s.action.bones.PushBack((int32)i);
					out.PushBack(s);
				}
			}

			void Poids(const NkRigDocument &d, NkVector<NkRigSuggestion> &out) {
				if (d.armature.Count() == 0 || d.weights.VertexCount() != d.mesh.VertexCount() || d.mesh.VertexCount() == 0) {
					return;
				}
				const NkWeightCheck c = d.CheckWeights();
				NkRigSuggestion s;
				s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Weights;
				if (c.sansPoids > 0) {
					s.key = NkString("poids:auto");
					s.title = c.sansPoids == c.vertices ? NkString("Poser les poids automatiques (chaleur)")
														: NkString::Format("%u sommets sans poids : poids automatiques (chaleur)", c.sansPoids);
					s.detail = NkString("La diffusion de chaleur donne à chaque sommet les os qui l'entourent (Blender : Automatic Weights).");
					s.confidence = 0.9f;
					s.action.kind = NkRigActionKind::NK_RigActionKind_Auto_Weights;
					s.action.method = NkAutoWeightMethod::NK_AutoWeightMethod_Chaleur;
					out.PushBack(s);
					return;
				}
				if (c.sommesFausses > 0) {
					NkRigSuggestion n = s;
					n.key = NkString("poids:normaliser");
					n.title = NkString::Format("Normaliser les poids (%u sommets à somme fausse)", c.sommesFausses);
					n.detail = NkString("Une somme loin de 1 gonfle ou rétrécit la peau quand l'os bouge.");
					n.confidence = 0.95f;
					n.action.kind = NkRigActionKind::NK_RigActionKind_Normalize_Weights;
					out.PushBack(n);
				}
				if (c.tropInfluences > 0) {
					NkRigSuggestion n = s;
					n.key = NkString("poids:limiter");
					n.title = NkString::Format("Limiter à 4 os par sommet (%u sommets)", c.tropInfluences);
					n.detail = NkString("Le GPU ne lit que 4 influences : les autres seraient perdues sans renormalisation.");
					n.confidence = 0.9f;
					n.action.kind = NkRigActionKind::NK_RigActionKind_Limit_Weights;
					out.PushBack(n);
				}
			}

			void Etapes(const NkRigDocument &d, const NkRigSuggestOptions &o, NkVector<NkRigSuggestion> &out) {
				if (d.mesh.VertexCount() == 0) {
					return;
				}
				NkRigSuggestion s;
				s.kind = NkRigSuggestionKind::NK_RigSuggestionKind_Step;
				s.confidence = 0.9f;
				if (d.armature.Count() == 0 && d.landmarks.Empty()) {
					// Le gabarit : un maillage plus long que haut est un quadrupede.
					NkVec3f mn = V(1e30f, 1e30f, 1e30f);
					NkVec3f mx = V(-1e30f, -1e30f, -1e30f);
					for (uint32 v = 0; v < d.mesh.VertexCount(); ++v) {
						const NkVec3f p = Point(o.objet, d.mesh.positions[v]);
						mn = V(p.x < mn.x ? p.x : mn.x, p.y < mn.y ? p.y : mn.y, p.z < mn.z ? p.z : mn.z);
						mx = V(p.x > mx.x ? p.x : mx.x, p.y > mx.y ? p.y : mx.y, p.z > mx.z ? p.z : mx.z);
					}
					const bool quadrupede = (mx.z - mn.z) > 1.2f * (mx.y - mn.y);
					s.key = NkString("etape:detecter");
					s.action.kind = NkRigActionKind::NK_RigActionKind_Detect_Landmarks;
					s.action.rigTemplate = quadrupede ? NkRigTemplate::NK_RigTemplate_Quadrupede : NkRigTemplate::NK_RigTemplate_Humanoide;
					s.title = NkString::Format("Détecter les repères du rig automatique (%s)", NkRigTemplateName(s.action.rigTemplate));
					s.detail = NkString("Le maillage n'a pas de squelette : les repères se placent par la géométrie, puis se corrigent à la main.");
					out.PushBack(s);
					return;
				}
				if (d.armature.Count() == 0) {
					s.key = NkString("etape:construire");
					s.action.kind = NkRigActionKind::NK_RigActionKind_Build_Rig;
					s.action.rigTemplate = d.rigTemplate;
					s.title = NkString("Construire le rig depuis les repères (avec les poids automatiques)");
					s.detail = NkString::Format("%u repères posés : l'armature, ses contrôles IK et les poids en une seule opération.",
												(uint32)d.landmarks.Size());
					out.PushBack(s);
					return;
				}
				if (d.controls.Empty()) {
					NkVector<NkRigControl> essai;
					NkRigGenerateControls(NkRigTemplate::NK_RigTemplate_Humanoide, d.armature, essai);
					uint32 ik = 0;
					for (uint32 c = 0; c < (uint32)essai.Size(); ++c) {
						ik += essai[c].kind == NkRigControlKind::NK_RigControlKind_IK_Deux_Os ? 1u : 0u;
					}
					if (ik > 0) {
						s.key = NkString("etape:controles");
						s.action.kind = NkRigActionKind::NK_RigActionKind_Generate_Controls;
						s.action.rigTemplate = NkRigTemplate::NK_RigTemplate_Humanoide;
						s.title = NkString::Format("Créer les contrôles du gabarit (%u IK)", ik);
						s.detail = NkString("Les os portent les noms du gabarit humanoïde : bras et jambes reçoivent leur IK à deux os, "
											"la colonne, la racine et le regard leurs poignées.");
						out.PushBack(s);
					}
				}
			}
		} // namespace

		void NkRigSuggest(const NkRigDocument &doc, const NkRigSuggestOptions &opt, NkVector<NkRigSuggestion> &out) {
			out.Clear();
			const float32 h = Hauteur(doc, opt.objet);
			Etapes(doc, opt, out);
			Placement(doc, opt, h, out);
			Nommage(doc, opt, h, out);
			Poids(doc, out);
		}

		// =====================================================================
		// APPLIQUER
		// =====================================================================
		bool NkRigApplySuggestion(NkRigDocument &d, const NkRigSuggestion &s, NkString *effet) {
			const NkRigAction &a = s.action;
			NkString dit;
			bool ok = false;
			switch (a.kind) {
				case NkRigActionKind::NK_RigActionKind_Move_Landmarks: {
					d.Begin("Conseil : repères", NK_RigPart_Reperes);
					uint32 n = 0;
					for (uint32 i = 0; i < (uint32)a.ids.Size() && i < (uint32)a.positions.Size(); ++i) {
						const int32 k = NkRigFindLandmark(d.landmarks, a.ids[i].CStr());
						if (k < 0) {
							continue;
						}
						d.landmarks[(uint32)k].position = a.positions[i];
						d.landmarks[(uint32)k].manuel = true;
						++n;
					}
					if (n == 0) {
						d.Cancel();
						break;
					}
					d.Touch(NK_RigPart_Reperes);
					dit = NkString::Format("%u repère(s) déplacé(s)", n);
					ok = true;
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Rename_Bones: {
					const uint32 n = a.bones.Size() < a.names.Size() ? (uint32)a.bones.Size() : (uint32)a.names.Size();
					bool valide = n > 0;
					for (uint32 i = 0; i < n && valide; ++i) {
						valide = a.bones[i] >= 0 && (uint32)a.bones[i] < d.armature.Count() && !a.names[i].Empty();
					}
					if (!valide) {
						break;
					}
					d.Begin("Conseil : noms des os", NK_RigPart_Armature);
					// DEUX TEMPS : des noms provisoires d'abord, sinon « hand.L » donne a un
					// os pendant qu'un AUTRE le porte encore deviendrait « hand.L.001 ».
					for (uint32 i = 0; i < n; ++i) {
						(void)d.armature.Rename((uint32)a.bones[i], NkString::Format("__nk_provisoire_%u", i).CStr());
					}
					for (uint32 i = 0; i < n; ++i) {
						(void)d.armature.Rename((uint32)a.bones[i], a.names[i].CStr());
					}
					d.Touch(NK_RigPart_Armature);
					dit = NkString::Format("%u os renommé(s)", n);
					ok = true;
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Symmetrize_Bone: {
					if (a.bones.Empty() || a.bones[0] < 0) {
						break;
					}
					const int32 m = d.SymmetrizeBone((uint32)a.bones[0]);
					ok = m >= 0;
					dit = ok ? NkString::Format("miroir : %s", d.armature.bones[(uint32)m].name.CStr()) : NkString();
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Detect_Landmarks: {
					ok = d.DetectLandmarks(a.rigTemplate, nullptr);
					dit = NkString::Format("%u repères détectés", (uint32)d.landmarks.Size());
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Build_Rig: {
					ok = d.BuildRig(a.rigTemplate, true, NkAutoWeightMethod::NK_AutoWeightMethod_Chaleur, nullptr);
					dit = NkString::Format("rig construit : %u os, %u contrôles", d.armature.Count(), (uint32)d.controls.Size());
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Auto_Weights: {
					ok = d.AutoWeights(a.method, nullptr);
					dit = NkString("poids automatiques posés");
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Normalize_Weights: {
					d.NormalizeWeights();
					ok = true;
					dit = NkString("poids normalisés");
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Limit_Weights: {
					d.LimitWeights(4);
					ok = true;
					dit = NkString("4 os au plus par sommet");
					break;
				}
				case NkRigActionKind::NK_RigActionKind_Generate_Controls: {
					const uint32 n = d.GenerateControls(a.rigTemplate);
					ok = n > 0;
					dit = NkString::Format("%u contrôle(s) créé(s)", n);
					break;
				}
				default:
					break;
			}
			if (effet != nullptr) {
				*effet = ok ? dit : NkString();
			}
			return ok;
		}

	} // namespace anim
} // namespace nkentseu
