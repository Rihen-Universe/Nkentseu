// -----------------------------------------------------------------------------
// @File    NogeeInterfacePanneaux.cpp
// @Brief   Les PANNEAUX de Nogee, dessines par les pieces partagees de la
//          famille (NKEditorKit/Famille) : Placer des acteurs, Outliner, Details
//          (en cartes) et Monde, le tiroir du bas (Contenu, Journal, Terminal).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA TOUCHE DE NOGEE (document 02 §0) : ses acteurs sont les primitives 3D, la
// camera et les lumieres de Noge ; ses cartes sont les composants de Noge
// (Transform, Maillage, Materiau, Lumiere, Camera, Corps rigide,
// Collisionneur, Hierarchie) ; son Contenu montre des maillages, des images,
// des materiaux et des scenes.
// -----------------------------------------------------------------------------

#include "Nogee/Editeur/NogeeInterface.h"

#include "NKEditorKit/Components/NkSilhouettes.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Physics/NkPhysics.h"
#include "Noge/ECS/Components/Rendering/NkRenderComponents.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nogee {

		using namespace editorkit;
		using namespace ecs;
		using math::NkVec3f;
		using nkgui::NkColor;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		namespace {

			// =================================================================
			// LES ICONES DU CATALOGUE (tracees : la police n'a pas ces glyphes)
			// =================================================================
			void IconeCube(nkgui::NkGuiDrawList &dl, float32 cx, float32 cy, float32 r, const NkColor &dessus, const NkColor &g,
						   const NkColor &d) {
				const NkVec2 h{cx, cy - r}, hg{cx - r * 0.87f, cy - r * 0.5f}, hd{cx + r * 0.87f, cy - r * 0.5f};
				const NkVec2 c{cx, cy}, bg{cx - r * 0.87f, cy + r * 0.5f}, bd{cx + r * 0.87f, cy + r * 0.5f}, b{cx, cy + r};
				dl.AddTriangleFilled(h, hd, c, dessus);
				dl.AddTriangleFilled(h, c, hg, dessus);
				dl.AddTriangleFilled(hg, c, b, g);
				dl.AddTriangleFilled(hg, b, bg, g);
				dl.AddTriangleFilled(c, hd, bd, d);
				dl.AddTriangleFilled(c, bd, b, d);
			}

			NkColor Teinte(const NkColor &c, float32 k) {
				auto f = [k](uint8 v) {
					const float32 x = static_cast<float32>(v) * k;
					return static_cast<uint8>(x > 255.f ? 255.f : x);
				};
				return NkColor{f(c.r), f(c.g), f(c.b), c.a};
			}

			void IconeElement(void *user, nkgui::NkGuiDrawList &dl, int32 k, const NkRect &r, const NkColor &texte) {
				(void)user;
				int32 n = 0;
				const NogeeElement *cat = NogeeCatalogue(n);
				if (k < 0 || k >= n) {
					return;
				}
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				const float32 s = (r.w < r.h ? r.w : r.h) * 0.36f;
				const NkColor gris{190, 196, 206, 255};
				const NkColor bleu{17, 119, 209, 255};
				const NkColor ambre{242, 152, 14, 255};
				switch (cat[k].genre) {
					case NogeeGenre::Cube:
						IconeCube(dl, cx, cy, s, Teinte(gris, 1.1f), Teinte(gris, 0.8f), Teinte(gris, 0.55f));
						break;
					case NogeeGenre::CubePhysique:
						IconeCube(dl, cx, cy, s, NkColor{235, 70, 55, 255}, NkColor{200, 45, 35, 255}, NkColor{140, 30, 25, 255});
						break;
					case NogeeGenre::Sol:
						dl.AddTriangleFilled(NkVec2{cx - s * 1.2f, cy + s * 0.2f}, NkVec2{cx, cy - s * 0.5f}, NkVec2{cx + s * 1.2f, cy + s * 0.2f},
											 NkColor{110, 140, 115, 255});
						dl.AddTriangleFilled(NkVec2{cx - s * 1.2f, cy + s * 0.2f}, NkVec2{cx + s * 1.2f, cy + s * 0.2f}, NkVec2{cx, cy + s * 0.9f},
											 NkColor{90, 115, 95, 255});
						break;
					case NogeeGenre::Sphere:
					case NogeeGenre::SpherePhysique: {
						const NkColor base = cat[k].genre == NogeeGenre::Sphere ? gris : NkColor{242, 200, 46, 255};
						dl.AddCircleFilled(NkVec2{cx, cy}, s, Teinte(base, 0.75f));
						dl.AddCircleFilled(NkVec2{cx - s * 0.2f, cy - s * 0.2f}, s * 0.72f, base);
						dl.AddCircleFilled(NkVec2{cx - s * 0.38f, cy - s * 0.38f}, s * 0.22f, NkColor{255, 255, 255, 190});
						break;
					}
					case NogeeGenre::Cylindre:
						dl.AddRectFilled(NkRect{cx - s * 0.8f, cy - s * 0.6f, s * 1.6f, s * 1.3f}, Teinte(gris, 0.75f));
						dl.AddEllipseFilled(NkVec2{cx, cy + s * 0.7f}, s * 0.8f, s * 0.3f, Teinte(gris, 0.75f));
						dl.AddEllipseFilled(NkVec2{cx, cy - s * 0.6f}, s * 0.8f, s * 0.3f, Teinte(gris, 1.1f));
						break;
					case NogeeGenre::Plan:
						dl.AddTriangleFilled(NkVec2{cx - s * 1.1f, cy + s * 0.1f}, NkVec2{cx, cy - s * 0.5f}, NkVec2{cx + s * 1.1f, cy + s * 0.1f},
											 Teinte(gris, 0.95f));
						dl.AddTriangleFilled(NkVec2{cx - s * 1.1f, cy + s * 0.1f}, NkVec2{cx + s * 1.1f, cy + s * 0.1f}, NkVec2{cx, cy + s * 0.7f},
											 Teinte(gris, 0.95f));
						break;
					case NogeeGenre::Capsule:
						dl.AddRectFilled(NkRect{cx - s * 0.55f, cy - s * 0.95f, s * 1.1f, s * 1.9f}, Teinte(gris, 0.9f), s * 0.55f);
						dl.AddCircleFilled(NkVec2{cx - s * 0.2f, cy - s * 0.5f}, s * 0.18f, NkColor{255, 255, 255, 170});
						break;
					case NogeeGenre::Cone:
						dl.AddTriangleFilled(NkVec2{cx, cy - s}, NkVec2{cx + s * 0.85f, cy + s * 0.6f}, NkVec2{cx - s * 0.85f, cy + s * 0.6f},
											 Teinte(gris, 0.9f));
						dl.AddEllipseFilled(NkVec2{cx, cy + s * 0.6f}, s * 0.85f, s * 0.28f, Teinte(gris, 0.65f));
						break;
					case NogeeGenre::Camera:
						dl.AddRectFilled(NkRect{cx - s, cy - s * 0.55f, s * 1.3f, s * 1.1f}, texte, 2.f);
						dl.AddTriangleFilled(NkVec2{cx + s * 0.3f, cy}, NkVec2{cx + s, cy - s * 0.55f}, NkVec2{cx + s, cy + s * 0.55f}, texte);
						dl.AddCircleFilled(NkVec2{cx - s * 0.35f, cy}, s * 0.28f, bleu);
						break;
					case NogeeGenre::LumiereDirectionnelle:
						for (int32 j = 0; j < 8; ++j) {
							const float32 a = 0.785398f * static_cast<float32>(j);
							dl.AddLine(NkVec2{cx + std::cos(a) * s * 0.6f, cy + std::sin(a) * s * 0.6f},
									   NkVec2{cx + std::cos(a) * s, cy + std::sin(a) * s}, ambre, 1.6f);
						}
						dl.AddCircleFilled(NkVec2{cx, cy}, s * 0.45f, ambre);
						break;
					case NogeeGenre::LumierePonctuelle:
						dl.AddCircleFilled(NkVec2{cx, cy - s * 0.2f}, s * 0.65f, NkColor{255, 214, 120, 255});
						dl.AddRectFilled(NkRect{cx - s * 0.3f, cy + s * 0.45f, s * 0.6f, s * 0.45f}, texte, 1.f);
						break;
					case NogeeGenre::LumiereProjecteur:
						dl.AddTriangleFilled(NkVec2{cx, cy - s}, NkVec2{cx + s * 0.9f, cy + s * 0.8f}, NkVec2{cx - s * 0.9f, cy + s * 0.8f},
											 NkColor{255, 214, 120, 150});
						dl.AddCircleFilled(NkVec2{cx, cy - s * 0.85f}, s * 0.3f, texte);
						break;
					default: {
						const NkVec2 pts[4] = {NkVec2{cx, cy - s}, NkVec2{cx + s, cy}, NkVec2{cx, cy + s}, NkVec2{cx - s, cy}};
						dl.AddPolyline(pts, 4, texte, 1.6f, true);
						break;
					}
				}
			}

			// ── Les rappels de l'Outliner ──────────────────────────────────────
			NogeeInterface &Ui(void *u) {
				return *static_cast<NogeeInterface *>(u);
			}
			NkEntityId EntiteDeLigne(NogeeInterface &ui, int32 index) {
				return (index > 0 && index < static_cast<int32>(ui.mArbreEntites.Size()) + 1)
						   ? ui.mArbreEntites[static_cast<uint32>(index - 1)]
						   : NkEntityId::Invalid();
			}
			void SurActiver(void *u, int32 index) {
				NogeeInterface &ui = Ui(u);
				const NkEntityId e = EntiteDeLigne(ui, index);
				if (e.IsValid()) {
					ui.M().Choisir(e);
					ui.M().Cadrer(e);
				}
			}
			void SurDrapeau(void *u, int32 index, bool oeil) {
				NogeeInterface &ui = Ui(u);
				const NkEntityId e = EntiteDeLigne(ui, index);
				if (!e.IsValid()) {
					return;
				}
				if (oeil) {
					ui.M().BasculerCache(e);
				} else {
					ui.M().BasculerVerrou(e);
				}
			}
			void SurRenommer(void *u, int32 index, const char *nom) {
				NogeeInterface &ui = Ui(u);
				const NkEntityId e = EntiteDeLigne(ui, index);
				if (e.IsValid()) {
					ui.M().Renommer(e, nom);
				}
			}
			void SurMenu(void *u, int32 index, float32 x, float32 y) {
				NogeeInterface &ui = Ui(u);
				const NkEntityId e = EntiteDeLigne(ui, index);
				if (e.IsValid()) {
					ui.M().Choisir(e);
					ui.mMenus.Ouvrir(NOGEE_MENU_CTX_ENTITE, NkRect{x, y, 0.f, 0.f});
				} else {
					ui.mPointMenu = NkVec3f{ui.M().orbite.pivot.x, 0.f, ui.M().orbite.pivot.z};
					ui.mMenus.Ouvrir(NOGEE_MENU_CTX_VIDE, NkRect{x, y, 0.f, 0.f});
				}
			}
			void SurNouvelle(void *u) {
				Ui(u).Executer(NOGEE_A_NOUVELLE_ENTITE);
			}
			void SurRattacher(void *u, int32 source, int32 cible) {
				NogeeInterface &ui = Ui(u);
				const NkEntityId s = EntiteDeLigne(ui, source);
				const NkEntityId c = EntiteDeLigne(ui, cible);
				if (s.IsValid() && ui.M().etat == NogeeEtatJeu::Edition) {
					ui.M().Retenir();
					ui.M().Rattacher(s, c);
				}
			}

			nk_uint64 IdNoeud(NkEntityId e) {
				return e.Pack() + 2u; // 0 = aucun, 1 = la racine
			}

			NkFamilleNature NatureFamille(NogeeNature n) {
				switch (n) {
					case NogeeNature::Maillage: return NkFamilleNature::Maillage;
					case NogeeNature::Rigide:   return NkFamilleNature::Rigide;
					case NogeeNature::Decor:    return NkFamilleNature::Decor;
					case NogeeNature::Lumiere:  return NkFamilleNature::Lumiere;
					case NogeeNature::Camera:   return NkFamilleNature::Camera;
					default:                    return NkFamilleNature::Entite;
				}
			}

			const NkFamilleNatureFichier kNatures[] = {
				{".nkscene", "Scène", NkRole::AccentUi, static_cast<uint8>(NkAssetIcone::Texte)},
				{".obj", "Maillage", NkRole::TypeMesh, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".gltf", "Maillage", NkRole::TypeMesh, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".glb", "Maillage", NkRole::TypeMesh, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".fbx", "Maillage", NkRole::TypeMesh, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".png", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".jpg", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".jpeg", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".tga", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".hdr", "Image", NkRole::TypeTex, static_cast<uint8>(NkAssetIcone::Image)},
				{".nkmat", "Matériau", NkRole::TypeMat, static_cast<uint8>(NkAssetIcone::Texte)},
				{".mtl", "Matériau", NkRole::TypeMat, static_cast<uint8>(NkAssetIcone::Texte)},
				{".wav", "Son", NkRole::StatusWarn, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".ogg", "Son", NkRole::StatusWarn, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".mp3", "Son", NkRole::StatusWarn, static_cast<uint8>(NkAssetIcone::Inconnu)},
				{".ttf", "Police", NkRole::AxisZ, static_cast<uint8>(NkAssetIcone::Texte)},
				{".otf", "Police", NkRole::AxisZ, static_cast<uint8>(NkAssetIcone::Texte)},
				{".nksl", "Nuanceur", NkRole::TypeAnim, static_cast<uint8>(NkAssetIcone::Code)},
				{".glsl", "Nuanceur", NkRole::TypeAnim, static_cast<uint8>(NkAssetIcone::Code)},
			};

			bool FinitPar(const NkString &s, const char *fin) {
				const usize n = std::strlen(fin);
				if (s.Length() < n) {
					return false;
				}
				const char *p = s.CStr() + s.Length() - n;
				for (usize i = 0; i < n; ++i) {
					char a = p[i];
					a = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
					if (a != fin[i]) {
						return false;
					}
				}
				return true;
			}

			/// Les noms des primitives (Maillage : Forme).
			const char *const kFormes[6] = {"Cube", "Sphère", "Cylindre", "Plan", "Capsule", "Cône"};
			const char *const kPrimitives[6] = {"primitive:cube", "primitive:sphere", "primitive:cylinder",
												"primitive:plane", "primitive:capsule", "primitive:cone"};
		} // namespace

		// =====================================================================
		// PLACER DES ACTEURS
		// =====================================================================
		void NogeeInterface::Placer(NkFamilleCtx &c) {
			if (!mPlan.voirPlacer) {
				return;
			}
			int32 n = 0;
			const NogeeElement *cat = NogeeCatalogue(n);
			static NkVector<NkFamilleElementPlacer> elements;
			if (elements.Size() != static_cast<usize>(n)) {
				elements.Clear();
				for (int32 k = 0; k < n; ++k) {
					NkFamilleElementPlacer e;
					e.nom = cat[k].nom;
					e.aide = cat[k].aide;
					e.onglets = cat[k].onglets;
					elements.PushBack(e);
				}
			}
			const NkFamillePlacerResultat r =
				NkFamilleDessinerPlacer(c, mPlan.placer, mPlacer, elements.Data(), n, &IconeElement, this, mPlan.viseur);
			NogeeModele &m = M();
			if (m.etat != NogeeEtatJeu::Edition) {
				if (r.clic >= 0 || r.lache >= 0) {
					m.Annoncer("En jeu, on ne pose rien : arrêtez d'abord (■)", 2);
				}
				return;
			}
			if (r.clic >= 0) {
				(void)m.PoserElement(r.clic, PointDePose(r.clic, true, NkVec2{0.f, 0.f}));
			} else if (r.lache >= 0) {
				// Au point du lacher, sur le sol.
				mPointMenu = PointAuSol(r.lachePos);
				(void)m.PoserElement(r.lache, PointDePose(r.lache, false, r.lachePos));
			}
		}

		// =====================================================================
		// L'OUTLINER
		// =====================================================================
		void NogeeInterface::Outliner(NkFamilleCtx &c) {
			NogeeModele &m = M();
			m.Entites(mArbreEntites);
			if (!mPlan.voirOutliner) {
				return;
			}
			NkTreeViewModel &a = mOutliner.arbre;
			a.nodes.Clear();
			NkTreeNode racine;
			racine.id = kNkFamilleRacine;
			racine.parent = -1;
			racine.label = NkString("Scène");
			racine.kindLabel = "Monde";
			racine.kindRole = static_cast<uint16>(NkRole::TextMuted);
			a.nodes.PushBack(racine);
			// L'arbre en ordre PREFIXE : chaque racine, puis ses enfants.
			NkVector<NkEntityId> ordre;
			NkVector<int32> parents;
			auto Ranger = [&](auto &&soi, NkEntityId e, int32 parent, uint32 profondeur) -> void {
				NkTreeNode n;
				n.id = IdNoeud(e);
				n.parent = parent;
				n.label = NkString(m.Nom(e));
				n.kindLabel = m.Type(e); // une chaine STATIQUE (le noeud ne garde qu'un pointeur)
				const NogeeNature nat = m.Nature(e);
				n.kindRole = static_cast<uint16>(nat == NogeeNature::Lumiere || nat == NogeeNature::Camera ? NkRole::AccentSel
																										 : NkRole::TextMuted);
				const bool cache = m.EstCache(e);
				const bool verrou = m.EstVerrouille(e);
				bool cacheHerite = false, verrouHerite = false;
				for (NkEntityId p = m.Parent(e); p.IsValid(); p = m.Parent(p)) {
					cacheHerite = cacheHerite || m.EstCache(p);
					verrouHerite = verrouHerite || m.EstVerrouille(p);
				}
				n.icon = NkFamilleIconeNoeud(NatureFamille(nat), cache, verrou, cacheHerite, verrouHerite);
				n.hidden = cache;
				const int32 moi = static_cast<int32>(a.nodes.Size());
				a.nodes.PushBack(n);
				ordre.PushBack(e);
				if (profondeur + 2u >= static_cast<uint32>(NkTreeViewModel::kMaxDepth)) {
					return;
				}
				for (uint32 k = 0; k < mArbreEntites.Size(); ++k) {
					if (m.Parent(mArbreEntites[k]) == e) {
						soi(soi, mArbreEntites[k], moi, profondeur + 1u);
					}
				}
			};
			for (uint32 k = 0; k < mArbreEntites.Size(); ++k) {
				const NkEntityId p = m.Parent(mArbreEntites[k]);
				bool parentConnu = false;
				for (uint32 j = 0; j < mArbreEntites.Size() && p.IsValid(); ++j) {
					parentConnu = parentConnu || mArbreEntites[j] == p;
				}
				if (!parentConnu) {
					Ranger(Ranger, mArbreEntites[k], 0, 0u);
				}
			}
			mArbreEntites = ordre; // l'indice de ligne - 1 = l'entite
			// La selection du MODELE est celle que l'arbre montre.
			a.chosen.Clear();
			a.active = m.SelectionValide() ? IdNoeud(m.selection) : 0u;
			if (a.active != 0u) {
				a.chosen.PushBack(a.active);
			}
			NkFamilleOutlinerRappels r;
			r.user = this;
			r.activer = &SurActiver;
			r.drapeau = &SurDrapeau;
			r.renommer = &SurRenommer;
			r.menu = &SurMenu;
			r.nouvelle = &SurNouvelle;
			r.rattacher = &SurRattacher;
			const NkString pied = NkString::Format("%u entités (%u sél.)", static_cast<unsigned>(mArbreEntites.Size()),
												   m.SelectionValide() ? 1u : 0u);
			const NkFamilleOutlinerResultat res = NkFamilleDessinerOutliner(c, mPlan.outliner, mOutliner, r, pied.CStr(), mDt);
			if (res.selectionChangee) {
				const NkEntityId e = EntiteDeLigne(*this, res.actif);
				if (e.IsValid()) {
					m.Choisir(e);
				} else {
					m.Deselectionner();
				}
			}
		}

		// =====================================================================
		// LES DETAILS
		// =====================================================================
		bool NogeeInterface::PeutAjouter(NogeeComposant k) const {
			const NogeeModele &m = M();
			if (!m.SelectionValide()) {
				return false;
			}
			const NkWorld &w = const_cast<NogeeModele &>(m).Monde();
			const NkEntityId id = m.selection;
			switch (k) {
				case NogeeComposant::Maillage:      return !w.Has<NkMeshComponent>(id);
				case NogeeComposant::Lumiere:       return !w.Has<NkLightComponent>(id);
				case NogeeComposant::Camera:        return !w.Has<NkCameraComponent>(id);
				case NogeeComposant::Corps:         return !w.Has<NkRigidbody3D>(id);
				case NogeeComposant::Collisionneur: return !w.Has<NkCollider3D>(id);
				default:                            return false;
			}
		}

		void NogeeInterface::AjouterComposant(NogeeComposant k, int32 variante) {
			NogeeModele &m = M();
			if (!m.SelectionValide() || m.etat != NogeeEtatJeu::Edition) {
				return;
			}
			NkWorld &w = m.Monde();
			const NkEntityId id = m.selection;
			m.Retenir();
			// variante < 0 : RETIRER le composant (le menu « ⋮ » d'une carte).
			if (variante < 0) {
				switch (k) {
					case NogeeComposant::Maillage:      w.Remove<NkMeshComponent>(id); break;
					case NogeeComposant::Materiau:      w.Remove<NkMaterialComponent>(id); break;
					case NogeeComposant::Lumiere:       w.Remove<NkLightComponent>(id); break;
					case NogeeComposant::Camera:        w.Remove<NkCameraComponent>(id); break;
					case NogeeComposant::Corps:         w.Remove<NkRigidbody3D>(id); break;
					case NogeeComposant::Collisionneur: w.Remove<NkCollider3D>(id); break;
					default:                            return;
				}
				m.modifie = true;
				m.Annoncer("Composant retiré");
				return;
			}
			switch (k) {
				case NogeeComposant::Maillage: {
					NkMeshComponent mesh;
					mesh.meshPath = kPrimitives[variante >= 0 && variante < 6 ? variante : 0];
					w.Add<NkMeshComponent>(id, mesh);
					if (!w.Has<NkMaterialComponent>(id)) {
						NkMaterialComponent mat;
						mat.SetColor(0, NkColor4(0.78f, 0.80f, 0.84f));
						w.Add<NkMaterialComponent>(id, mat);
					}
					break;
				}
				case NogeeComposant::Lumiere: {
					NkLightComponent l;
					l.type = variante == 0 ? NkLightType::Directional : (variante == 2 ? NkLightType::Spot : NkLightType::Point);
					l.intensity = variante == 0 ? 3.f : 8.f;
					w.Add<NkLightComponent>(id, l);
					break;
				}
				case NogeeComposant::Camera: {
					NkCameraComponent cam;
					cam.aspect = 0.f;
					cam.priority = 10;
					w.Add<NkCameraComponent>(id, cam);
					break;
				}
				case NogeeComposant::Corps: {
					NkRigidbody3D rb;
					rb.bodyType = NkBodyType::Dynamic;
					w.Add<NkRigidbody3D>(id, rb);
					if (!w.Has<NkCollider3D>(id)) {
						w.Add<NkCollider3D>(id);
					}
					break;
				}
				case NogeeComposant::Collisionneur: {
					NkCollider3D col;
					col.shape = variante == 1 ? NkCollider3DShape::Sphere : (variante == 2 ? NkCollider3DShape::Capsule : NkCollider3DShape::Box);
					w.Add<NkCollider3D>(id, col);
					break;
				}
				default:
					return;
			}
			m.modifie = true;
			m.Annoncer("Composant ajouté");
		}

		void NogeeInterface::Cartes(NkFamilleCtx &c, NkFamilleInspecteur &I, NkEntityId id) {
			NogeeModele &m = M();
			NkWorld &w = m.Monde();
			const bool edition = m.etat == NogeeEtatJeu::Edition;
			// Les valeurs se modifient sur des COPIES : l'instantane de l'historique
			// (Retenir) doit voir l'etat d'AVANT, une fois par geste.
			static bool sEnCours = false;
			if (!c.ctx.input.mouseDown[0] && c.ctx.inputId == nkgui::NKGUI_ID_NONE) {
				sEnCours = false;
			}
			auto Avant = [&]() {
				if (!sEnCours && edition) {
					m.Retenir();
					sEnCours = true;
				}
				m.modifie = true;
			};
			auto Menu = [&](bool clic, NogeeComposant k) {
				if (clic) {
					mCarteMenu = static_cast<int32>(k);
					const nkgui::NkVec2 p = c.ctx.input.mousePos;
					mMenus.Ouvrir(NOGEE_MENU_CARTE, NkRect{p.x - 160.f, p.y, 0.f, 0.f});
				}
			};
			bool menu = false;

			// ── TRANSFORM ─────────────────────────────────────────────────────
			if (NkTransform *tf = w.Get<NkTransform>(id)) {
				if (I.Carte(static_cast<int32>(NogeeComposant::Transform), "Transform", NkFamilleIconeCarte::Transform,
							NkFamilleCategorie::General, nullptr, &menu)) {
					float32 p[3] = {tf->localPosition.x, tf->localPosition.y, tf->localPosition.z};
					const math::NkEulerAngle e = static_cast<math::NkEulerAngle>(tf->localRotation);
					static uint64 sEulerDe = ~0ull;
					static float32 sEuler[3] = {0.f, 0.f, 0.f};
					if (sEulerDe != id.Pack() || !sEnCours) {
						sEulerDe = id.Pack();
						sEuler[0] = e.pitch.Deg();
						sEuler[1] = e.yaw.Deg();
						sEuler[2] = e.roll.Deg();
					}
					float32 r[3] = {sEuler[0], sEuler[1], sEuler[2]};
					float32 s[3] = {tf->localScale.x, tf->localScale.y, tf->localScale.z};
					static bool sVerrou = true;
					bool remise = false;
					const bool pm = p[0] != 0.f || p[1] != 0.f || p[2] != 0.f;
					if (I.Vecteur("Position (m)", p, 3, 0.05f, &remise, pm)) {
						Avant();
						tf->localPosition = NkVec3f{p[0], p[1], p[2]};
					}
					if (remise) {
						Avant();
						tf->localPosition = NkVec3f{0.f, 0.f, 0.f};
					}
					remise = false;
					const bool rm = std::fabs(r[0]) + std::fabs(r[1]) + std::fabs(r[2]) > 1.0e-3f;
					if (I.Vecteur("Rotation (°)", r, 3, 0.5f, &remise, rm)) {
						Avant();
						sEuler[0] = r[0];
						sEuler[1] = r[1];
						sEuler[2] = r[2];
						tf->localRotation = math::NkQuatf(math::NkEulerAngle(math::NkAngle(r[0]), math::NkAngle(r[1]), math::NkAngle(r[2])));
					}
					if (remise) {
						Avant();
						sEuler[0] = sEuler[1] = sEuler[2] = 0.f;
						tf->localRotation = math::NkQuatf::Identity();
					}
					remise = false;
					const bool sm = s[0] != 1.f || s[1] != 1.f || s[2] != 1.f;
					if (I.Vecteur("Échelle", s, 3, 0.01f, &remise, sm, &sVerrou)) {
						Avant();
						tf->localScale = NkVec3f{s[0], s[1], s[2]};
					}
					if (remise) {
						Avant();
						tf->localScale = NkVec3f{1.f, 1.f, 1.f};
					}
					tf->worldDirty = true;
					I.FinCarte();
				}
				Menu(menu, NogeeComposant::Transform);
			}

			// ── MAILLAGE ──────────────────────────────────────────────────────
			if (NkMeshComponent *mesh = w.Get<NkMeshComponent>(id)) {
				if (I.Carte(static_cast<int32>(NogeeComposant::Maillage), "Maillage", NkFamilleIconeCarte::Maillage,
							NkFamilleCategorie::Rendu, &mesh->visible, &menu)) {
					int32 forme = -1;
					for (int32 k = 0; k < 6; ++k) {
						if (mesh->meshPath == NkString(kPrimitives[k])) {
							forme = k;
						}
					}
					if (forme >= 0) {
						// Les six primitives, en deux rangees de trois boutons segmentes.
						int32 nouvelle = forme;
						int32 a = forme < 3 ? forme : -1;
						if (I.Choix("Forme", kFormes, 3, a) && a >= 0) {
							nouvelle = a;
						}
						int32 b = forme >= 3 ? forme - 3 : -1;
						if (I.Choix("", kFormes + 3, 3, b) && b >= 0) {
							nouvelle = b + 3;
						}
						if (nouvelle != forme) {
							Avant();
							mesh->meshPath = kPrimitives[nouvelle];
							mesh->meshHandle = 0;
						}
					} else {
						I.Info("Fichier", mesh->meshPath.CStr());
					}
					bool ombres = mesh->castShadow;
					if (I.Case("Projette une ombre", ombres)) {
						Avant();
						mesh->castShadow = ombres;
					}
					bool recoit = mesh->receiveShadow;
					if (I.Case("Reçoit les ombres", recoit)) {
						Avant();
						mesh->receiveShadow = recoit;
					}
					I.FinCarte();
				}
				Menu(menu, NogeeComposant::Maillage);
			}

			// ── MATERIAU ──────────────────────────────────────────────────────
			if (NkMaterialComponent *mat = w.Get<NkMaterialComponent>(id)) {
				if (mat->slotCount > 0 && I.Carte(static_cast<int32>(NogeeComposant::Materiau), "Matériau", NkFamilleIconeCarte::Materiau,
												  NkFamilleCategorie::Rendu, nullptr, &menu)) {
					NkMaterialSlot &sl = mat->slots[0];
					float32 col[4] = {sl.albedo.r, sl.albedo.g, sl.albedo.b, sl.albedo.a};
					if (I.Couleur("Couleur", col)) {
						Avant();
						sl.albedo = NkColor4(col[0], col[1], col[2], col[3]);
					}
					float32 metal = sl.metallic;
					if (I.Glissiere("Métal", metal, 0.f, 1.f)) {
						Avant();
						sl.metallic = metal;
					}
					float32 rug = sl.roughness;
					if (I.Glissiere("Rugosité", rug, 0.f, 1.f)) {
						Avant();
						sl.roughness = rug;
					}
					I.FinCarte();
				}
				Menu(menu, NogeeComposant::Materiau);
			}

			// ── LUMIERE ───────────────────────────────────────────────────────
			if (NkLightComponent *l = w.Get<NkLightComponent>(id)) {
				bool allumee = !w.Has<NkInactive>(id);
				if (I.Carte(static_cast<int32>(NogeeComposant::Lumiere), "Lumière", NkFamilleIconeCarte::Lumiere,
							NkFamilleCategorie::Rendu, &allumee, &menu)) {
					static const char *const kTypes[3] = {"Directionnelle", "Ponctuelle", "Projecteur"};
					int32 t = l->type == NkLightType::Directional ? 0 : (l->type == NkLightType::Spot ? 2 : 1);
					if (I.Choix("Type", kTypes, 3, t)) {
						Avant();
						l->type = t == 0 ? NkLightType::Directional : (t == 2 ? NkLightType::Spot : NkLightType::Point);
					}
					float32 col[4] = {l->color.r, l->color.g, l->color.b, l->color.a};
					if (I.Couleur("Couleur", col)) {
						Avant();
						l->color = NkColor4(col[0], col[1], col[2], col[3]);
					}
					float32 v = l->intensity;
					if (I.Nombre("Intensité", v, 0.05f, 0.f, 1000.f)) {
						Avant();
						l->intensity = v;
					}
					if (l->type != NkLightType::Directional) {
						v = l->range;
						if (I.Nombre("Portée (m)", v, 0.1f, 0.f, 1000.f)) {
							Avant();
							l->range = v;
						}
					}
					if (l->type == NkLightType::Spot) {
						v = l->innerAngle;
						if (I.Nombre("Angle intérieur (°)", v, 0.5f, 0.f, 89.f)) {
							Avant();
							l->innerAngle = v;
						}
						v = l->outerAngle;
						if (I.Nombre("Angle extérieur (°)", v, 0.5f, 0.f, 89.f)) {
							Avant();
							l->outerAngle = v;
						}
					}
					bool ombres = l->castShadow;
					if (I.Case("Ombres", ombres)) {
						Avant();
						l->castShadow = ombres;
					}
					I.FinCarte();
				}
				if (allumee != !w.Has<NkInactive>(id)) {
					m.Activer(id, allumee);
				}
				Menu(menu, NogeeComposant::Lumiere);
			}

			// ── CAMERA ────────────────────────────────────────────────────────
			if (NkCameraComponent *cam = w.Get<NkCameraComponent>(id)) {
				if (I.Carte(static_cast<int32>(NogeeComposant::Camera), "Caméra", NkFamilleIconeCarte::Camera,
							NkFamilleCategorie::Rendu, nullptr, &menu)) {
					static const char *const kProj[2] = {"Perspective", "Orthographique"};
					int32 p = cam->projection == NkCameraProjection::Perspective ? 0 : 1;
					if (I.Choix("Projection", kProj, 2, p)) {
						Avant();
						cam->projection = p == 0 ? NkCameraProjection::Perspective : NkCameraProjection::Orthographic;
					}
					float32 v = cam->fovDeg;
					if (p == 0 && I.Nombre("Champ (°)", v, 0.5f, 5.f, 170.f)) {
						Avant();
						cam->fovDeg = v;
					}
					v = cam->orthoSize;
					if (p == 1 && I.Nombre("Taille (m)", v, 0.05f, 0.01f, 1000.f)) {
						Avant();
						cam->orthoSize = v;
					}
					v = cam->nearClip;
					if (I.Nombre("Plan proche (m)", v, 0.01f, 0.001f, 100.f)) {
						Avant();
						cam->nearClip = v;
					}
					v = cam->farClip;
					if (I.Nombre("Plan lointain (m)", v, 1.f, 1.f, 100000.f)) {
						Avant();
						cam->farClip = v;
					}
					int32 pr = cam->priority;
					if (I.Entier("Priorité", pr, -100, 100)) {
						Avant();
						cam->priority = pr;
					}
					I.Ligne("En jeu, la caméra de plus haute priorité filme.");
					I.FinCarte();
				}
				Menu(menu, NogeeComposant::Camera);
			}

			// ── CORPS RIGIDE ──────────────────────────────────────────────────
			if (NkRigidbody3D *rb = w.Get<NkRigidbody3D>(id)) {
				if (I.Carte(static_cast<int32>(NogeeComposant::Corps), "Corps rigide", NkFamilleIconeCarte::Corps,
							NkFamilleCategorie::Physique, nullptr, &menu)) {
					static const char *const kTypes[3] = {"statique", "cinématique", "dynamique"};
					int32 t = rb->bodyType == NkBodyType::Static ? 0 : (rb->bodyType == NkBodyType::Kinematic ? 1 : 2);
					if (I.Choix("Corps", kTypes, 3, t)) {
						Avant();
						rb->bodyType = t == 0 ? NkBodyType::Static : (t == 1 ? NkBodyType::Kinematic : NkBodyType::Dynamic);
					}
					float32 v = rb->friction;
					if (I.Glissiere("Friction", v, 0.f, 1.f)) {
						Avant();
						rb->friction = v;
					}
					v = rb->restitution;
					if (I.Glissiere("Rebond", v, 0.f, 1.f)) {
						Avant();
						rb->restitution = v;
					}
					v = rb->gravityScale;
					if (I.Nombre("Gravité (x)", v, 0.05f, -10.f, 10.f)) {
						Avant();
						rb->gravityScale = v;
					}
					bool fige = rb->freezeRotX && rb->freezeRotY && rb->freezeRotZ;
					if (I.Case("Rotation figée", fige)) {
						Avant();
						rb->freezeRotX = rb->freezeRotY = rb->freezeRotZ = fige;
					}
					I.FinCarte();
				}
				Menu(menu, NogeeComposant::Corps);
			}

			// ── COLLISIONNEUR ─────────────────────────────────────────────────
			if (NkCollider3D *col = w.Get<NkCollider3D>(id)) {
				if (I.Carte(static_cast<int32>(NogeeComposant::Collisionneur), "Collisionneur", NkFamilleIconeCarte::Collisionneur,
							NkFamilleCategorie::Physique, &col->enabled, &menu)) {
					static const char *const kFormesCol[3] = {"Boîte", "Sphère", "Capsule"};
					int32 f = col->shape == NkCollider3DShape::Sphere ? 1 : (col->shape == NkCollider3DShape::Capsule ? 2 : 0);
					if (I.Choix("Forme", kFormesCol, 3, f)) {
						Avant();
						col->shape = f == 1 ? NkCollider3DShape::Sphere : (f == 2 ? NkCollider3DShape::Capsule : NkCollider3DShape::Box);
					}
					if (f == 0) {
						float32 t[3] = {col->boxSize.x, col->boxSize.y, col->boxSize.z};
						if (I.Vecteur("Taille (m)", t, 3, 0.01f)) {
							Avant();
							col->boxSize = {t[0], t[1], t[2]};
						}
					} else if (f == 1) {
						float32 r = col->sphereRadius;
						if (I.Nombre("Rayon (m)", r, 0.01f, 0.001f, 1000.f)) {
							Avant();
							col->sphereRadius = r;
						}
					} else {
						float32 r = col->capsuleRadius;
						if (I.Nombre("Rayon (m)", r, 0.01f, 0.001f, 1000.f)) {
							Avant();
							col->capsuleRadius = r;
						}
						float32 h = col->capsuleHeight;
						if (I.Nombre("Hauteur (m)", h, 0.01f, 0.001f, 1000.f)) {
							Avant();
							col->capsuleHeight = h;
						}
					}
					bool declencheur = col->isTrigger;
					if (I.Case("Déclencheur", declencheur)) {
						Avant();
						col->isTrigger = declencheur;
					}
					I.FinCarte();
				}
				Menu(menu, NogeeComposant::Collisionneur);
			}

			// ── HIERARCHIE ────────────────────────────────────────────────────
			if (I.Carte(static_cast<int32>(NogeeComposant::Hierarchie), "Hiérarchie", NkFamilleIconeCarte::Hierarchie,
						NkFamilleCategorie::Acteur, nullptr, &menu)) {
				const NkEntityId p = m.Parent(id);
				I.Info("Parent", p.IsValid() ? m.Nom(p) : "— (racine de la scène)");
				uint32 enfants = 0;
				for (uint32 k = 0; k < mArbreEntites.Size(); ++k) {
					enfants += m.Parent(mArbreEntites[k]) == id ? 1u : 0u;
				}
				I.Info("Enfants", NkString::Format("%u", static_cast<unsigned>(enfants)).CStr());
				if (I.Boutons("Détacher du parent", nullptr, p.IsValid() && edition) == 0) {
					Executer(NOGEE_A_DETACHER);
				}
				I.FinCarte();
			}
		}

		void NogeeInterface::Monde(NkFamilleCtx &c, const NkRect &zone) {
			auto &dl = c.ctx.dl;
			const NogeeModele &m = M();
			float32 y = zone.y + 10.f;
			const float32 lh = NkFamilleHauteurLigne(c.police, 16.f) + 8.f;
			auto Ligne = [&](const char *cle, const char *valeur) {
				NkFamilleTexte(dl, c.petite, zone.x + 12.f, y + 3.f, cle, mPal.attenue);
				NkFamilleTexte(dl, c.police, zone.x + zone.w * 0.42f, y, valeur, mPal.texte, zone.w * 0.55f);
				y += lh;
			};
			NkFamilleTexteGras(dl, c.police, zone.x + 12.f, y, "Le monde de la scène", mPal.texte);
			y += lh + 4.f;
			Ligne("Fichier", m.chemin.CStr());
			Ligne("Entités", NkString::Format("%u", static_cast<unsigned>(mArbreEntites.Size())).CStr());
			Ligne("État", m.etat == NogeeEtatJeu::Edition ? "édition" : (m.etat == NogeeEtatJeu::Jeu ? "en jeu" : "en pause"));
			Ligne("Rendu", "NkEngineLayer + NkRenderSystem (vue hors écran)");
			Ligne("Physique", "NkPhysicsSystem (pas fixe, en jeu seulement)");
			Ligne("Modifiée", m.modifie ? "oui" : "non");
			y += 6.f;
			const NkRect b{zone.x + 12.f, y, zone.w - 24.f, 24.f};
			if (NkFamilleBouton(c, b, m.voirGrille ? "Cacher la grille" : "Montrer la grille")) {
				Executer(NOGEE_A_GRILLE);
			}
		}

		void NogeeInterface::Details(NkFamilleCtx &c) {
			if (!mPlan.voirDetails || mPlan.details.w < 8.f || mPlan.details.h < 60.f) {
				return;
			}
			NogeeModele &m = M();
			const NkRect &zone = mPlan.details;
			mCtx.dl.AddRectFilled(zone, mPal.panneau);
			static const char *const kOnglets[2] = {"Détails", "Monde"};
			const float32 ongletsH = NkFamilleCotes::kOngletPanneau;
			(void)NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 2, mOngletDroite);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			mCtx.dl.PushClipRect(contenu, true);
			if (mOngletDroite == 1) {
				Monde(c, contenu);
				mCtx.dl.PopClipRect();
				return;
			}
			NkFamilleInspecteur I(c, mDetails, contenu);
			if (!m.SelectionValide()) {
				I.Vide("Sélectionnez une entité pour voir ses détails.",
					   "Cliquez-la dans la vue ou l'Outliner, ou posez-en une (Placer des acteurs).");
				mCtx.dl.PopClipRect();
				return;
			}
			const NkEntityId id = m.selection;
			bool actif = m.EstActive(id);
			const bool actifAvant = actif;
			NkString nouveau;
			const NkString type = NkString::Format("type : %s%s", m.Type(id),
												   actif ? "" : "  —  désactivée (ni rendue, ni simulée)");
			if (I.Entete(id.Pack(), m.Nom(id), &actif, type.CStr(), mMenus.menu == NOGEE_MENU_COMPOSANT, &nouveau)) {
				mMenus.Ouvrir(NOGEE_MENU_COMPOSANT, mDetails.ajouter);
			}
			if (actif != actifAvant) {
				m.Activer(id, actif);
			}
			if (!nouveau.Empty()) {
				m.Renommer(id, nouveau.CStr());
			}
			// L'arbre des composants : ceux que l'entite porte.
			NkFamilleComposant comps[8];
			int32 n = 0;
			NkWorld &w = m.Monde();
			comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Transform), "Transform", NkFamilleIconeCarte::Transform};
			if (w.Has<NkMeshComponent>(id)) {
				comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Maillage), "Maillage", NkFamilleIconeCarte::Maillage};
			}
			if (w.Has<NkMaterialComponent>(id)) {
				comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Materiau), "Matériau", NkFamilleIconeCarte::Materiau};
			}
			if (w.Has<NkLightComponent>(id)) {
				comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Lumiere), "Lumière", NkFamilleIconeCarte::Lumiere};
			}
			if (w.Has<NkCameraComponent>(id)) {
				comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Camera), "Caméra", NkFamilleIconeCarte::Camera};
			}
			if (w.Has<NkRigidbody3D>(id)) {
				comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Corps), "Corps rigide", NkFamilleIconeCarte::Corps};
			}
			if (w.Has<NkCollider3D>(id)) {
				comps[n++] =
					NkFamilleComposant{static_cast<int32>(NogeeComposant::Collisionneur), "Collisionneur", NkFamilleIconeCarte::Collisionneur};
			}
			comps[n++] = NkFamilleComposant{static_cast<int32>(NogeeComposant::Hierarchie), "Hiérarchie", NkFamilleIconeCarte::Hierarchie};
			char cle[32];
			std::snprintf(cle, sizeof(cle), "e%llu", static_cast<unsigned long long>(id.Pack()));
			if (I.Debut(cle, m.Nom(id), comps, n)) {
				Cartes(c, I, id);
				NkRect ajout;
				if (I.Fin("Ajouter un composant", mMenus.menu == NOGEE_MENU_COMPOSANT, &ajout)) {
					mMenus.Ouvrir(NOGEE_MENU_COMPOSANT, ajout);
				}
			}
			mCtx.dl.PopClipRect();
		}

		// =====================================================================
		// LE TIROIR : Contenu, Journal, Terminal
		// =====================================================================
		void NogeeInterface::Tiroir(NkFamilleCtx &c) {
			const NkRect &zone = mPlan.tiroir;
			if (!mPlan.voirTiroir || zone.w < 8.f || zone.h < 40.f) {
				return;
			}
			static const char *const kOnglets[3] = {"Contenu", "Journal", "Terminal"};
			const float32 ongletsH = NkFamilleCotes::kOngletPanneau;
			(void)NkFamilleOnglets(c, NkRect{zone.x, zone.y, zone.w, ongletsH}, kOnglets, 3, mOngletTiroir);
			const NkRect contenu{zone.x, zone.y + ongletsH, zone.w, zone.h - ongletsH};
			mCtx.dl.PushClipRect(contenu, true);
			if (mOngletTiroir == 0) {
				if (NkFamilleDessinerContenu(c, contenu, mContenu, kNatures, static_cast<int32>(sizeof(kNatures) / sizeof(kNatures[0])),
											 mDt)) {
					// Le double-clic d'un fichier : une scene s'ouvre, un maillage se pose.
					NogeeModele &m = M();
					const NkString f = mContenu.ouvert;
					if (FinitPar(f, ".nkscene")) {
						if (m.etat != NogeeEtatJeu::Edition) {
							m.Arreter();
						}
						(void)m.Ouvrir(f.CStr());
					} else if ((FinitPar(f, ".obj") || FinitPar(f, ".gltf") || FinitPar(f, ".glb") || FinitPar(f, ".fbx")) &&
							   m.etat == NogeeEtatJeu::Edition) {
						m.Retenir();
						int32 nCat = 0;
						(void)NogeeCatalogue(nCat);
						const NkEntityId e = m.Poser(NogeeGenre::ActeurVide, PointDePose(-1, true, NkVec2{0.f, 0.f}),
													 FinitPar(f, ".obj") ? "Maillage importé" : "Modèle importé");
						NkMeshComponent mesh;
						mesh.meshPath = f;
						m.Monde().Add<NkMeshComponent>(e, mesh);
						NkMaterialComponent mat;
						mat.SetColor(0, NkColor4(0.85f, 0.85f, 0.85f));
						m.Monde().Add<NkMaterialComponent>(e, mat);
						m.Choisir(e);
						m.modifie = true;
						m.Annoncer(NkString::Format("Maillage posé : %s", f.CStr()).CStr());
					} else {
						m.Annoncer(NkString::Format("Ouvrir « %s » : pas encore dans Nogee (R33 : chaque asset s'ouvre)", f.CStr()).CStr(), 2);
					}
				}
			} else if (mOngletTiroir == 1) {
				NkFamilleDessinerJournal(c, contenu, mJournal, mDt);
			} else {
				mTerminal.Dessiner(mCtx, mCtx.dl, contenu, mTheme);
			}
			mCtx.dl.PopClipRect();
		}

	} // namespace nogee
} // namespace nkentseu
