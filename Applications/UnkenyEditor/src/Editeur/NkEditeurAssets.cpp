//
// NkEditeurAssets.cpp
// =============================================================================
// Description :
//   Les ONGLETS D'ASSETS (voir NkEditeurAssets.h) : ouvrir, activer, fermer ;
//   l'editeur de chaque genre a la place de la vue ; le MODE PREFAB (la scene
//   mise de cote par NkScene::Photographier, comme « Jouer ») ; les reglages
//   caches du Contenu (`Contenu/.nkreglages`).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurAssets.h"
#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"
#include "Editeur/NkEditeurReferences.h"

#include "NKCanvas/App/NkCanvasTexte.h"
#include "NKFileSystem/NkFile.h"
#include "NKGui/Widgets/NkGuiWidgets.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace editeur {

		using nkgui::NkColor;
		using nkgui::NkGuiContext;
		using nkgui::NkRect;
		using nkgui::NkVec2;

		/// L'identifiant de texture de l'APERCU d'une police : un seul a la fois
		/// (l'onglet actif), televerse par l'application (NkEditeurApp::OnDraw).
		static constexpr uint32 NK_TEX_APERCU_POLICE = 0x4E4B5041u; // 'NKPA'
		/// La taille a laquelle l'apercu est rasterise ; les autres en sont des
		/// mises a l'echelle (AddTextScaled).
		static constexpr float32 NK_TAILLE_APERCU = 44.f;

		// =====================================================================
		// LE GENRE
		// =====================================================================
		NkGenreAsset NkEditeurGenreAsset(const char *cheminNav) {
			switch (NkEditeurNatureFichier(cheminNav).type) {
				case NkAssetType::Texture2D:
					return NkGenreAsset::NK_TEXTURE;
				case NkAssetType::Font:
					return NkGenreAsset::NK_POLICE;
				case NkAssetType::Sound:
					return NkGenreAsset::NK_SON;
				case NkAssetType::Prefab:
					return NkGenreAsset::NK_PREFAB;
				case NkAssetType::AnimationController:
					return NkGenreAsset::NK_CONTROLEUR;
				default:
					return NkGenreAsset::NK_AUCUN;
			}
		}

		const char *NkNomGenreAsset(NkGenreAsset g) noexcept {
			switch (g) {
				case NkGenreAsset::NK_TEXTURE:
					return "Texture";
				case NkGenreAsset::NK_POLICE:
					return "Police";
				case NkGenreAsset::NK_SON:
					return "Son";
				case NkGenreAsset::NK_PREFAB:
					return "Prefab";
				case NkGenreAsset::NK_CONTROLEUR:
					return "Contrôleur d'animation";
				default:
					return "Asset";
			}
		}

		// =====================================================================
		// LES REGLAGES CACHES DU CONTENU
		// =====================================================================
		namespace {
			NkString FichierReglages(NkEditeurModele &m) {
				NkString f = NkEditeurDossierContenu(m);
				f.Append("/.nkreglages");
				return f;
			}

			/// Les lignes « rel<TAB>cle<TAB>valeur » du fichier.
			void LireLignes(NkEditeurModele &m, NkVector<NkString> &lignes) {
				lignes.Clear();
				const NkString texte = NkFile::ReadAllText(FichierReglages(m).CStr());
				NkString ligne;
				for (usize i = 0; i <= texte.Length(); ++i) {
					const char ch = i < texte.Length() ? texte.CStr()[i] : '\n';
					if (ch == '\n' || ch == '\r') {
						if (!ligne.Empty()) {
							lignes.PushBack(ligne);
						}
						ligne = NkString();
					} else {
						ligne.Append(ch);
					}
				}
			}

			/// La valeur de `cle` pour `rel`, ou faux.
			bool Valeur(const NkVector<NkString> &lignes, const NkString &rel, const char *cle, NkString &valeur) {
				NkString debut(rel);
				debut.Append('\t');
				debut.Append(cle);
				debut.Append('\t');
				for (uint32 i = 0; i < lignes.Size(); ++i) {
					if (lignes[i].StartsWith(debut.CStr())) {
						valeur = NkString(lignes[i].CStr() + debut.Length());
						return true;
					}
				}
				return false;
			}

			/// Remplace les reglages de `rel` par `paires` (cle, valeur, cle, valeur...).
			bool Ecrire(NkEditeurModele &m, const NkString &rel, const NkVector<NkString> &paires) {
				NkVector<NkString> lignes;
				LireLignes(m, lignes);
				NkString debut(rel);
				debut.Append('\t');
				NkString texte;
				for (uint32 i = 0; i < lignes.Size(); ++i) {
					if (!lignes[i].StartsWith(debut.CStr())) {
						texte.Append(lignes[i].CStr());
						texte.Append('\n');
					}
				}
				for (uint32 i = 0; i + 1u < paires.Size(); i += 2u) {
					texte.Append(NkString::Format("%s\t%s\t%s\n", rel.CStr(), paires[i].CStr(), paires[i + 1u].CStr()).CStr());
				}
				return NkFile::WriteAllText(FichierReglages(m).CStr(), texte.CStr());
			}
		} // namespace

		bool NkEditeurLireReglagesTexture(NkEditeurModele &m, const char *cheminNav, NkReglagesTexture &r) {
			r = NkReglagesTexture();
			NkVector<NkString> lignes;
			LireLignes(m, lignes);
			const NkString rel = NkEditeurRelatifContenu(cheminNav);
			NkString v;
			bool trouve = false;
			if (Valeur(lignes, rel, "filtrage", v)) {
				r.pixel = v == NkString("pixel");
				trouve = true;
			}
			if (Valeur(lignes, rel, "pivot", v)) {
				std::sscanf(v.CStr(), "%f %f", &r.pivot.x, &r.pivot.y);
				trouve = true;
			}
			if (Valeur(lignes, rel, "pixelsParUnite", v)) {
				std::sscanf(v.CStr(), "%f", &r.pixelsParUnite);
				r.pixelsParUnite = r.pixelsParUnite < 1.f ? 1.f : r.pixelsParUnite;
				trouve = true;
			}
			return trouve;
		}

		bool NkEditeurEcrireReglagesTexture(NkEditeurModele &m, const char *cheminNav, const NkReglagesTexture &r) {
			NkVector<NkString> p;
			p.PushBack(NkString("filtrage"));
			p.PushBack(NkString(r.pixel ? "pixel" : "lisse"));
			p.PushBack(NkString("pivot"));
			p.PushBack(NkString::Format("%.4f %.4f", static_cast<double>(r.pivot.x), static_cast<double>(r.pivot.y)));
			p.PushBack(NkString("pixelsParUnite"));
			p.PushBack(NkString::Format("%.3f", static_cast<double>(r.pixelsParUnite)));
			return Ecrire(m, NkEditeurRelatifContenu(cheminNav), p);
		}

		bool NkEditeurLireReglagesSon(NkEditeurModele &m, const char *cheminNav, NkReglagesSon &r) {
			r = NkReglagesSon();
			NkVector<NkString> lignes;
			LireLignes(m, lignes);
			const NkString rel = NkEditeurRelatifContenu(cheminNav);
			NkString v;
			bool trouve = false;
			if (Valeur(lignes, rel, "volume", v)) {
				std::sscanf(v.CStr(), "%f", &r.volume);
				trouve = true;
			}
			if (Valeur(lignes, rel, "boucle", v)) {
				r.boucle = v == NkString("1");
				trouve = true;
			}
			return trouve;
		}

		bool NkEditeurEcrireReglagesSon(NkEditeurModele &m, const char *cheminNav, const NkReglagesSon &r) {
			NkVector<NkString> p;
			p.PushBack(NkString("volume"));
			p.PushBack(NkString::Format("%.3f", static_cast<double>(r.volume)));
			p.PushBack(NkString("boucle"));
			p.PushBack(NkString(r.boucle ? "1" : "0"));
			return Ecrire(m, NkEditeurRelatifContenu(cheminNav), p);
		}

		// =====================================================================
		// LE MODE PREFAB
		// =====================================================================
		namespace {
			NkOngletAsset *Onglet(NkEditeurInterface &ui, int32 k) {
				return k >= 0 && static_cast<uint32>(k) < ui.onglets.Size() ? ui.onglets[static_cast<uint32>(k)] : nullptr;
			}

			/// La scene de cote, l'editeur sur le prefab seul.
			bool EntrerPrefab(NkEditeurCadre &c, int32 k) {
				NkEditeurModele &m = c.m;
				NkOngletAsset *o = Onglet(c.ui, k);
				if (o == nullptr || o->prefab == 0u) {
					return false;
				}
				if (m.etat != NkEtatJeu::NK_EDITION) {
					NkEditeurAnnoncer(m, "Arrêtez le jeu avant d'ouvrir un prefab");
					return false;
				}
				memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
				NkModePrefab *mp = tas.New<NkModePrefab>();
				m.scene.Photographier(mp->photo);
				mp->camera = m.scene.Camera();
				mp->historique = m.historique;
				mp->selection = m.aSelection && m.scene.Monde().IsAlive(m.selection) ? m.scene.AssurerUid(m.selection) : 0u;
				mp->onglet = k;
				NkEditeurOublierHistorique(m);
				if (o->prefabPhotoValide) {
					// On y revient : l'edition en cours, telle qu'on l'a laissee.
					m.scene.Restaurer(o->prefabPhoto);
				} else {
					// Une scene VIDE de la meme configuration (la matiere garde ses
					// reglages, pas ses corps) ; puis le prefab, a l'origine.
					unkeny::NkScene::NkPhoto vide;
					vide.valide = true;
					vide.particules.reglages = mp->photo.particules.reglages;
					vide.prochainUid = mp->photo.prochainUid;
					m.scene.Restaurer(vide);
					const ecs::NkEntityId r = m.prefabs.Instancier(m.scene, o->prefab, NkVec2f(0.f, 0.f));
					o->racineUid = r.IsValid() ? m.scene.AssurerUid(r) : 0u;
				}
				m.scene.Camera().PoserCentre(NkVec2f(0.f, 0.f));
				const ecs::NkEntityId r = o->racineUid != 0u ? m.scene.EntiteParUid(o->racineUid) : ecs::NkEntityId::Invalid();
				m.selection = r;
				m.aSelection = r.IsValid();
				c.ui.modePrefab = mp;
				NkEditeurAnnoncer(m, NkString::Format("Prefab ouvert : %s — Enregistrer (Ctrl+S) met à jour ses instances",
													  NkEditeurRelatifContenu(o->nav.CStr()).CStr())
										 .CStr());
				return true;
			}

			/// La scene revient ; les prefabs enregistres sont relus : leurs instances
			/// suivent (NkPrefabs2D::Charger compare a l'ANCIEN modele, surcharges gardees).
			void SortirPrefab(NkEditeurCadre &c) {
				NkEditeurModele &m = c.m;
				NkModePrefab *mp = c.ui.modePrefab;
				if (mp == nullptr) {
					return;
				}
				if (NkOngletAsset *o = Onglet(c.ui, mp->onglet)) {
					m.scene.Photographier(o->prefabPhoto);
					o->prefabPhotoValide = true;
				}
				m.scene.Restaurer(mp->photo);
				m.scene.Camera() = mp->camera;
				m.historique = mp->historique;
				const ecs::NkEntityId sel = mp->selection != 0u ? m.scene.EntiteParUid(mp->selection) : ecs::NkEntityId::Invalid();
				m.selection = sel;
				m.aSelection = sel.IsValid();
				uint32 suivies = 0u;
				for (uint32 i = 0; i < mp->aRelire.Size(); ++i) {
					NkString erreur;
					const uint32 id = m.prefabs.Charger(m.scene, mp->aRelire[i].CStr(), m.RessourcesScene(), &erreur);
					NkVector<ecs::NkEntityId> inst;
					if (id != 0u) {
						m.prefabs.Instances(m.scene, id, inst);
					}
					suivies += static_cast<uint32>(inst.Size());
				}
				if (!mp->aRelire.Empty()) {
					NkEditeurAnnoncer(m, NkString::Format("Prefab enregistré : %u instance(s) de la scène mises à jour", suivies).CStr());
				}
				memory::NkGetDefaultAllocator().Delete(mp);
				c.ui.modePrefab = nullptr;
			}
		} // namespace

		NkOngletAsset *NkEditeurPrefabOuvert(NkEditeurInterface &ui) noexcept {
			return ui.modePrefab != nullptr ? Onglet(ui, ui.modePrefab->onglet) : nullptr;
		}

		bool NkEditeurEnregistrerPrefabOuvert(NkEditeurCadre &c) {
			NkEditeurModele &m = c.m;
			NkOngletAsset *o = NkEditeurPrefabOuvert(c.ui);
			if (o == nullptr) {
				return false;
			}
			const ecs::NkEntityId r = o->racineUid != 0u ? m.scene.EntiteParUid(o->racineUid) : ecs::NkEntityId::Invalid();
			if (!r.IsValid()) {
				NkEditeurAnnoncer(m, "Prefab : sa racine a disparu, rien n'est enregistré");
				return true;
			}
			// Un prefab DE TRAVAIL, sous un autre nom : le modele enregistre (celui
			// des instances de la scene) reste l'ANCIEN jusqu'au retour, pour que
			// leurs surcharges se lisent contre lui.
			NkString travail("__edition__:");
			travail.Append(o->disque.CStr());
			const uint32 id = m.prefabs.Creer(m.scene, r, travail.CStr());
			const bool ok = id != 0u && m.prefabs.Enregistrer(m.scene, id, o->disque.CStr(), m.RessourcesScene());
			if (ok) {
				bool deja = false;
				for (uint32 i = 0; i < c.ui.modePrefab->aRelire.Size(); ++i) {
					deja = deja || c.ui.modePrefab->aRelire[i] == o->disque;
				}
				if (!deja) {
					c.ui.modePrefab->aRelire.PushBack(o->disque);
				}
				o->modifie = false;
			}
			NkEditeurAnnoncer(m, ok ? NkString::Format("Prefab enregistré : %s (ses instances suivent au retour à la scène)",
													   NkEditeurRelatifContenu(o->nav.CStr()).CStr())
										  .CStr()
									: "Prefab : écriture impossible");
			return true;
		}

		// =====================================================================
		// OUVRIR, ACTIVER, FERMER
		// =====================================================================
		bool NkEditeurOuvrirAsset(NkEditeurCadre &c, const char *cheminNav) {
			NkEditeurModele &m = c.m;
			NkEditeurInterface &ui = c.ui;
			const NkGenreAsset genre = NkEditeurGenreAsset(cheminNav);
			if (genre == NkGenreAsset::NK_AUCUN) {
				return false;
			}
			// ⚓ (fusion du 02/10) UN CONTROLEUR S'OUVRE DANS LA PAGE ANIMATEUR, son
			// graphe editable (NkEditeurPagesAnim.h) : c'est son editeur. L'onglet
			// en lecture ne s'ouvre que si le fichier ne se lit pas (il le dit).
			if (genre == NkGenreAsset::NK_CONTROLEUR) {
				const NkString disque = NkEditeurCheminContenuAbsolu(m, cheminNav);
				anim::NkAnimStateMachine essai;
				if (essai.LoadBinary(disque) && NkEditeurOuvrirAnimateur(c, disque.CStr())) {
					return true;
				}
			}
			for (uint32 k = 0; k < ui.onglets.Size(); ++k) {
				if (ui.onglets[k]->nav == NkString(cheminNav)) {
					NkEditeurActiverOnglet(c, static_cast<int32>(k));
					return true;
				}
			}
			// Un prefab se lit DANS LA SCENE : on en sort d'abord si l'on est dans un autre.
			if (genre == NkGenreAsset::NK_PREFAB && ui.modePrefab != nullptr) {
				NkEditeurActiverOnglet(c, -1);
			}
			memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
			NkOngletAsset *o = tas.New<NkOngletAsset>();
			o->nav = NkString(cheminNav);
			o->disque = NkEditeurCheminContenuAbsolu(m, cheminNav);
			o->genre = genre;
			bool ok = true;
			switch (genre) {
				case NkGenreAsset::NK_TEXTURE:
					NkEditeurLireReglagesTexture(m, cheminNav, o->texture);
					o->texId = m.textures.Charger(o->disque.CStr());
					ok = o->texId != 0u;
					break;
				case NkGenreAsset::NK_POLICE:
					o->police = tas.New<nkgui::NkGuiFont>();
					o->policeOk = o->police->LoadFromFile(o->disque.CStr(), NK_TAILLE_APERCU, false);
					o->police->texId = NK_TEX_APERCU_POLICE;
					ok = o->policeOk;
					break;
				case NkGenreAsset::NK_SON:
					NkEditeurLireReglagesSon(m, cheminNav, o->son);
					if (ui.sonsApercu == nullptr) {
						ui.sonsApercu = tas.New<NkSons2D>();
						ui.sonsApercu->Demarrer(ui.sonsMuets);
					}
					o->sonId = ui.sonsApercu->Charger(o->disque.CStr());
					ok = o->sonId != 0u;
					break;
				case NkGenreAsset::NK_PREFAB: {
					NkString erreur;
					o->prefab = m.prefabs.Charger(m.scene, o->disque.CStr(), m.RessourcesScene(), &erreur);
					ok = o->prefab != 0u;
					if (!ok) {
						NkEditeurAnnoncer(m, NkString::Format("Prefab illisible : %s", erreur.CStr()).CStr());
						tas.Delete(o);
						return true;
					}
					break;
				}
				case NkGenreAsset::NK_CONTROLEUR:
					// Le fichier ne se lit pas comme machine (voir plus haut) : l'onglet
					// en lecture le dit.
					o->modele = NkEditeurNomControleur(cheminNav);
					ok = NkChargerModeleAnimateur(o->modele.CStr(), o->disque.CStr());
					break;
				default:
					break;
			}
			if (!ok) {
				NkEditeurAnnoncer(m, NkString::Format("%s illisible : %s (l'onglet le dit)", NkNomGenreAsset(genre), cheminNav).CStr());
			}
			ui.onglets.PushBack(o);
			NkEditeurActiverOnglet(c, static_cast<int32>(ui.onglets.Size()) - 1);
			return true;
		}

		void NkEditeurActiverOnglet(NkEditeurCadre &c, int32 k) {
			NkEditeurInterface &ui = c.ui;
			if (k >= static_cast<int32>(ui.onglets.Size())) {
				return;
			}
			k = k < 0 ? -1 : k;
			// (fusion du 02/10) Un onglet d'asset ou la scene passe DEVANT les pages
			// Animation / Animateur.
			ui.pagesAnim.actif = 0;
			if (k == ui.ongletActif) {
				return;
			}
			// Quitter un prefab rend la scene (et relit ce qui a ete enregistre).
			if (ui.modePrefab != nullptr) {
				SortirPrefab(c);
			}
			// Un son qui jouait se tait quand on quitte son onglet.
			if (NkOngletAsset *avant = Onglet(ui, ui.ongletActif)) {
				if (avant->voix != 0u && ui.sonsApercu != nullptr) {
					ui.sonsApercu->Couper(avant->voix, 0.05f);
					avant->voix = 0u;
				}
			}
			NkOngletAsset *o = Onglet(ui, k);
			if (o != nullptr && o->genre == NkGenreAsset::NK_PREFAB && !EntrerPrefab(c, k)) {
				return;
			}
			if (o != nullptr && o->genre == NkGenreAsset::NK_POLICE && o->policeOk) {
				ui.policeApercu = o->police;
				ui.policeApercuSale = true;
			}
			ui.ongletActif = k;
			ui.menu = NkMenuEditeur::NK_AUCUN;
		}

		void NkEditeurFermerOnglet(NkEditeurCadre &c, int32 k) {
			NkEditeurInterface &ui = c.ui;
			NkOngletAsset *o = Onglet(ui, k);
			if (o == nullptr) {
				return;
			}
			if (ui.ongletActif == k) {
				NkEditeurActiverOnglet(c, -1);
			}
			memory::NkAllocator &tas = memory::NkGetDefaultAllocator();
			if (o->voix != 0u && ui.sonsApercu != nullptr) {
				ui.sonsApercu->Couper(o->voix, 0.f);
			}
			if (ui.policeApercu == o->police) {
				ui.policeApercu = nullptr;
			}
			if (o->police != nullptr) {
				tas.Delete(o->police);
			}
			tas.Delete(o);
			ui.onglets.Erase(ui.onglets.Begin() + k);
			if (ui.ongletActif > k) {
				--ui.ongletActif;
			}
		}

		void NkEditeurFermerTousOnglets(NkEditeurCadre &c) {
			NkEditeurActiverOnglet(c, -1);
			while (!c.ui.onglets.Empty()) {
				NkEditeurFermerOnglet(c, static_cast<int32>(c.ui.onglets.Size()) - 1);
			}
			if (c.ui.sonsApercu != nullptr) {
				c.ui.sonsApercu->Arreter();
				memory::NkGetDefaultAllocator().Delete(c.ui.sonsApercu);
				c.ui.sonsApercu = nullptr;
			}
		}

		bool NkEditeurAssetALaPlaceDeLaVue(const NkEditeurInterface &ui) noexcept {
			return ui.ongletActif >= 0 && static_cast<uint32>(ui.ongletActif) < ui.onglets.Size() &&
				   ui.onglets[static_cast<uint32>(ui.ongletActif)]->genre != NkGenreAsset::NK_PREFAB;
		}

		// =====================================================================
		// LES ONGLETS, a droite de celui de la scene
		// =====================================================================
		void NkEditeurDessinerOngletsAssets(NkEditeurCadre &c, float32 x) {
			NkEditeurInterface &ui = c.ui;
			auto &dl = c.ctx.dl;
			const nkgui::NkGuiInput &in = c.ctx.input;
			const NkRect &b = ui.barreOnglets;
			ui.ongletsRects.Clear();
			ui.ongletsFermer.Clear();
			int32 aFermer = -1, aActiver = -2;
			for (uint32 k = 0; k < ui.onglets.Size(); ++k) {
				const NkOngletAsset *o = ui.onglets[k];
				NkString nom = NkEditeurRelatifContenu(o->nav.CStr());
				const char *court = nom.CStr();
				for (const char *p = court; *p != '\0'; ++p) {
					court = *p == '/' ? p + 1 : court;
				}
				const NkString libelle(court);
				const float32 tw = renderer::NkTexteLargeur(c.police, libelle.CStr());
				const float32 w = 12.f + 14.f + tw + 8.f + 20.f;
				const NkRect r{x, b.y + 3.f, w, b.h - 3.f};
				const bool actif = ui.ongletActif == static_cast<int32>(k);
				const bool survol = NkEditeurDans(r, in.mousePos);
				dl.AddRectFilled(r, actif ? c.pal.panneau : (survol ? c.pal.boutonSurvol : c.pal.fond), 2.f);
				if (actif) {
					dl.AddRectFilled(NkRect{r.x, r.y, r.w, 2.f}, c.pal.accent);
				}
				if (o->modifie) {
					dl.AddCircleFilled(NkVec2{r.x + 16.f, r.y + r.h * 0.5f}, 3.5f, c.pal.selection);
				} else if (o->genre == NkGenreAsset::NK_PREFAB) {
					// Un prefab : le losange bleu d'Unreal (trace : la police ne l'a pas).
					const NkVec2 m{r.x + 16.f, r.y + r.h * 0.5f};
					const NkColor bleu{90, 150, 235, 255};
					dl.AddTriangleFilled(NkVec2{m.x, m.y - 5.f}, NkVec2{m.x + 5.f, m.y}, NkVec2{m.x, m.y + 5.f}, bleu);
					dl.AddTriangleFilled(NkVec2{m.x, m.y - 5.f}, NkVec2{m.x, m.y + 5.f}, NkVec2{m.x - 5.f, m.y}, bleu);
				}
				const float32 ty = r.y + (r.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f;
				renderer::NkTexte(dl, c.police, r.x + 26.f, ty, libelle.CStr(), actif ? c.pal.texte : c.pal.attenue);
				const NkRect fermer{r.x + r.w - 24.f, r.y + (r.h - 16.f) * 0.5f, 16.f, 16.f};
				const bool surX = NkEditeurDans(fermer, in.mousePos);
				if (surX) {
					dl.AddRectFilled(fermer, c.pal.boutonSurvol, 2.f);
				}
				const NkColor tx = surX ? c.pal.texte : c.pal.attenue;
				dl.AddLine(NkVec2{fermer.x + 4.5f, fermer.y + 4.5f}, NkVec2{fermer.x + 11.5f, fermer.y + 11.5f}, tx, 1.4f);
				dl.AddLine(NkVec2{fermer.x + 11.5f, fermer.y + 4.5f}, NkVec2{fermer.x + 4.5f, fermer.y + 11.5f}, tx, 1.4f);
				ui.ongletsRects.PushBack(r);
				ui.ongletsFermer.PushBack(fermer);
				if (in.mouseClicked[0] && surX) {
					aFermer = static_cast<int32>(k);
				} else if (in.mouseClicked[0] && survol) {
					aActiver = static_cast<int32>(k);
				}
				x += w + 2.f;
			}
			if (aFermer >= 0) {
				NkEditeurFermerOnglet(c, aFermer);
			} else if (aActiver >= -1) {
				NkEditeurActiverOnglet(c, aActiver);
			}
		}

		// =====================================================================
		// L'EDITEUR D'ASSET, A LA PLACE DE LA VUE
		// =====================================================================
		namespace {
			/// La colonne des reglages : des rangees « nom | valeur » au rythme des
			/// Details (24 px), libelle a gauche, champ a droite.
			struct NkColonne {
					NkEditeurCadre &c;
					NkRect zone;
					float32 y;
					int32 n = 0;
			};

			NkRect Rangee(NkColonne &k, const char *libelle, float32 h = 24.f) {
				auto &dl = k.c.ctx.dl;
				const NkRect r{k.zone.x, k.y, k.zone.w, h};
				dl.AddRectFilled(r, (k.n++ % 2) == 0 ? k.c.pal.panneau : k.c.pal.entete);
				const float32 col = r.w * 0.42f;
				if (libelle != nullptr && libelle[0] != '\0') {
					renderer::NkTexte(dl, k.c.police, r.x + 10.f, r.y + (r.h - renderer::NkTexteHauteurLigne(k.c.police, 16.f)) * 0.5f, libelle,
									  k.c.pal.texte, col - 14.f);
				}
				k.y += h;
				return NkRect{r.x + col, r.y + 2.f, r.w - col - 8.f, r.h - 4.f};
			}

			void Titre(NkColonne &k, const char *t) {
				auto &dl = k.c.ctx.dl;
				const NkRect r{k.zone.x, k.y + 6.f, k.zone.w, 24.f};
				dl.AddRectFilled(r, k.c.pal.entete, 3.f);
				renderer::NkTexte(dl, k.c.police, r.x + 10.f, r.y + (r.h - renderer::NkTexteHauteurLigne(k.c.police, 16.f)) * 0.5f, t, k.c.pal.texte);
				renderer::NkTexte(dl, k.c.police, r.x + 10.6f, r.y + (r.h - renderer::NkTexteHauteurLigne(k.c.police, 16.f)) * 0.5f, t, k.c.pal.texte);
				k.y += 32.f;
				k.n = 0;
			}

			void Info(NkColonne &k, const char *libelle, const char *valeur) {
				const NkRect ch = Rangee(k, libelle);
				renderer::NkTexte(k.c.ctx.dl, k.c.police, ch.x + 4.f, ch.y + (ch.h - renderer::NkTexteHauteurLigne(k.c.police, 16.f)) * 0.5f, valeur,
								  k.c.pal.attenue, ch.w - 4.f);
			}

			/// Un choix segmente ; rend vrai s'il change.
			bool Choix(NkColonne &k, const char *libelle, const char *const *noms, int32 n, int32 &v, NkRect *champ = nullptr) {
				const NkRect ch = Rangee(k, libelle);
				if (champ != nullptr) {
					*champ = ch;
				}
				const float32 w = ch.w / static_cast<float32>(n);
				bool change = false;
				for (int32 i = 0; i < n; ++i) {
					const NkRect r{ch.x + static_cast<float32>(i) * w, ch.y, w - 2.f, ch.h};
					if (NkEditeurBouton(k.c, r, "", i == v, true) && i != v) {
						v = i;
						change = true;
					}
					renderer::NkTexteDansBoite(k.c.ctx.dl, k.c.petite, r, noms[i], i == v ? k.c.pal.surAccent : k.c.pal.texte);
				}
				return change;
			}

			/// Un bouton sur toute la largeur ; rend vrai s'il est clique.
			bool Bouton(NkColonne &k, const char *texte, NkRect *rect = nullptr, bool actif = true) {
				const NkRect r{k.zone.x + 8.f, k.y + 6.f, k.zone.w - 16.f, 26.f};
				k.y += 36.f;
				if (rect != nullptr) {
					*rect = r;
				}
				const bool clic = NkEditeurBouton(k.c, r, "", false, actif);
				renderer::NkTexteDansBoite(k.c.ctx.dl, k.c.police, r, texte, actif ? k.c.pal.texte : k.c.pal.attenue);
				return clic && actif;
			}

			/// Le DAMIER sous une image (la transparence se voit).
			void Damier(nkgui::NkGuiDrawList &dl, const NkRect &r, float32 pas) {
				const NkColor a{58, 58, 64, 255}, b{78, 78, 86, 255};
				for (float32 y = r.y; y < r.y + r.h; y += pas) {
					for (float32 x = r.x; x < r.x + r.w; x += pas) {
						const int32 i = static_cast<int32>((x - r.x) / pas) + static_cast<int32>((y - r.y) / pas);
						const float32 w = (x + pas > r.x + r.w) ? r.x + r.w - x : pas;
						const float32 h = (y + pas > r.y + r.h) ? r.y + r.h - y : pas;
						dl.AddRectFilled(NkRect{x, y, w, h}, (i % 2) == 0 ? a : b);
					}
				}
			}

			void EditeurTexture(NkEditeurCadre &c, NkOngletAsset &o, const NkRect &apercu, NkColonne &k) {
				NkEditeurModele &m = c.m;
				auto &dl = c.ctx.dl;
				int32 tw = 0, th = 0;
				const bool lue = o.texId != 0u && m.textures.Taille(o.texId, tw, th) && tw > 0 && th > 0;
				// ── L'apercu : damier, l'image ajustee puis zoomee (molette), et le
				//    PIVOT (une croix) ──
				Damier(dl, apercu, 16.f);
				if (NkEditeurDans(apercu, c.ctx.input.mousePos) && c.ctx.input.wheel != 0.f) {
					o.zoom *= c.ctx.input.wheel > 0.f ? 1.25f : 0.8f;
					o.zoom = o.zoom < 0.1f ? 0.1f : (o.zoom > 32.f ? 32.f : o.zoom);
				}
				NkRect img{0.f, 0.f, 0.f, 0.f};
				if (lue) {
					const float32 ajuste = math::NkMin((apercu.w - 40.f) / static_cast<float32>(tw), (apercu.h - 40.f) / static_cast<float32>(th));
					const float32 e = ajuste * o.zoom;
					img = NkRect{apercu.x + (apercu.w - static_cast<float32>(tw) * e) * 0.5f, apercu.y + (apercu.h - static_cast<float32>(th) * e) * 0.5f,
								 static_cast<float32>(tw) * e, static_cast<float32>(th) * e};
					dl.PushClipRect(apercu, true);
					const uint8 *px = m.textures.Pixels(o.texId);
					if (o.texture.pixel && px != nullptr && tw * th <= 128 * 128) {
						// « Pixel » : chaque pixel est un carre net (le plus proche).
						for (int32 y = 0; y < th; ++y) {
							for (int32 x = 0; x < tw; ++x) {
								const uint8 *p = px + (static_cast<usize>(y) * static_cast<usize>(tw) + static_cast<usize>(x)) * 4u;
								if (p[3] != 0u) {
									dl.AddRectFilled(NkRect{img.x + static_cast<float32>(x) * e, img.y + static_cast<float32>(y) * e, e + 0.5f, e + 0.5f},
													 NkColor{p[0], p[1], p[2], p[3]});
								}
							}
						}
					} else {
						dl.AddImage(o.texId, img, NkVec2{0.f, 0.f}, NkVec2{1.f, 1.f}, NkColor{255, 255, 255, 255});
					}
					dl.AddRect(img, c.pal.bord, 1.f);
					const NkVec2 pv{img.x + img.w * o.texture.pivot.x, img.y + img.h * (1.f - o.texture.pivot.y)};
					dl.AddLine(NkVec2{pv.x - 8.f, pv.y}, NkVec2{pv.x + 8.f, pv.y}, c.pal.selection, 2.f);
					dl.AddLine(NkVec2{pv.x, pv.y - 8.f}, NkVec2{pv.x, pv.y + 8.f}, c.pal.selection, 2.f);
					dl.PopClipRect();
				} else {
					renderer::NkTexteCentre(dl, c.police, apercu.x + apercu.w * 0.5f, apercu.y + apercu.h * 0.5f, "Image illisible", c.pal.attenue);
				}
				c.ui.assetApercu = img;
				// ── Les reglages ──
				Titre(k, "Texture");
				Info(k, "Taille", lue ? NkString::Format("%d x %d px", tw, th).CStr() : "—");
				Info(k, "Zoom de l'aperçu", NkString::Format("%.0f %%  (molette)", static_cast<double>(o.zoom * 100.f)).CStr());
				static const char *kFiltres[2] = {"Lisse", "Pixel"};
				int32 f = o.texture.pixel ? 1 : 0;
				if (Choix(k, "Filtrage", kFiltres, 2, f, &c.ui.assetFiltrage)) {
					o.texture.pixel = f == 1;
					o.modifie = true;
				}
				Titre(k, "Sprite posé avec elle");
				NkGuiContext &ctx = c.ctx;
				ctx.PushId(o.nav.CStr());
				NkRect ch = Rangee(k, "Pivot (0..1)");
				c.ui.assetPivot = ch;
				const float32 moitie = (ch.w - 4.f) * 0.5f;
				ctx.SetNextItemRect(NkRect{ch.x, ch.y, moitie, ch.h});
				o.modifie |= nkgui::DragFloat(ctx, "##px", o.texture.pivot.x, 0.01f, 0.f, 1.f);
				ctx.SetNextItemRect(NkRect{ch.x + moitie + 4.f, ch.y, moitie, ch.h});
				o.modifie |= nkgui::DragFloat(ctx, "##py", o.texture.pivot.y, 0.01f, 0.f, 1.f);
				ch = Rangee(k, "Pixels par unité");
				ctx.SetNextItemRect(ch);
				o.modifie |= nkgui::DragFloat(ctx, "##ppu", o.texture.pixelsParUnite, 1.f, 1.f, 4096.f);
				ctx.PopId();
				if (lue) {
					Info(k, "Taille d'un sprite",
						 NkString::Format("%.2f x %.2f m", static_cast<double>(static_cast<float32>(tw) / o.texture.pixelsParUnite),
										  static_cast<double>(static_cast<float32>(th) / o.texture.pixelsParUnite))
							 .CStr());
				}
				if (Bouton(k, o.modifie ? "Enregistrer les réglages" : "Réglages enregistrés", &c.ui.assetEnregistrer, o.modifie)) {
					o.modifie = !NkEditeurEcrireReglagesTexture(m, o.nav.CStr(), o.texture);
					NkEditeurAnnoncer(m, o.modifie ? "Réglages de la texture : écriture impossible" : "Réglages de la texture enregistrés");
				}
				// Les sprites de la scene qui l'utilisent prennent pivot et taille.
				uint32 n = 0u;
				m.scene.Monde().Query<NkSprite2D>().ForEach([&](ecs::NkEntityId, NkSprite2D &s) { n += s.texId == o.texId ? 1u : 0u; });
				if (Bouton(k, NkString::Format("Appliquer aux sprites qui l'utilisent (%u)", n).CStr(), nullptr, n > 0u && lue)) {
					NkEditeurRetenir(m);
					m.scene.Monde().Query<NkSprite2D>().ForEach([&](ecs::NkEntityId, NkSprite2D &s) {
						if (s.texId == o.texId) {
							s.pivot = o.texture.pivot;
							s.taille = NkVec2f(static_cast<float32>(tw) / o.texture.pixelsParUnite, static_cast<float32>(th) / o.texture.pixelsParUnite);
						}
					});
					NkEditeurAnnoncer(m, NkString::Format("Pivot et taille appliqués à %u sprite(s)", n).CStr());
				}
				Info(k, "Note", "« Pixel » : l'aperçu ; le rendu suivra (NKCanvas)");
			}

			void EditeurPolice(NkEditeurCadre &c, NkOngletAsset &o, const NkRect &apercu, NkColonne &k) {
				auto &dl = c.ctx.dl;
				dl.AddRectFilled(apercu, c.pal.fond);
				Titre(k, "Police");
				Info(k, "Fichier", editorkit::NkDisqueNom(o.nav.CStr()).CStr());
				if (!o.policeOk || o.police == nullptr || o.police->Face() == nullptr) {
					Info(k, "État", "illisible (TTF / OTF attendu)");
					renderer::NkTexteCentre(dl, c.police, apercu.x + apercu.w * 0.5f, apercu.y + apercu.h * 0.5f, "Police illisible", c.pal.attenue);
					return;
				}
				Info(k, "Hauteur de ligne", NkString::Format("%.1f px à %.0f px", static_cast<double>(o.police->LineHeight()),
															 static_cast<double>(NK_TAILLE_APERCU))
												.CStr());
				Info(k, "Tailles montrées", "12, 18, 28, 44 px");
				// L'apercu : la meme phrase en quatre tailles, puis l'alphabet.
				static const float32 kTailles[4] = {12.f, 18.f, 28.f, 44.f};
				const char *phrase = "Portez ce vieux whisky au juge blond qui fume";
				float32 y = apercu.y + 24.f;
				const auto *face = o.police->Face();
				dl.PushClipRect(apercu, true);
				c.ui.assetTailles = 0;
				for (int32 i = 0; i < 4; ++i) {
					const float32 e = kTailles[i] / NK_TAILLE_APERCU;
					renderer::NkTexte(dl, c.petite, apercu.x + 20.f, y, NkString::Format("%.0f px", static_cast<double>(kTailles[i])).CStr(),
									  c.pal.attenue);
					y += 16.f;
					dl.AddTextScaled(face, o.police->TexId(), NkVec2{apercu.x + 20.f, y + o.police->Ascent() * e}, phrase, c.pal.texte, e,
									 apercu.w - 40.f);
					y += o.police->LineHeight() * e + 14.f;
					++c.ui.assetTailles;
				}
				const float32 e = 28.f / NK_TAILLE_APERCU;
				dl.AddTextScaled(face, o.police->TexId(), NkVec2{apercu.x + 20.f, y + o.police->Ascent() * e},
								 "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz", c.pal.texte, e, apercu.w - 40.f);
				y += o.police->LineHeight() * e + 8.f;
				dl.AddTextScaled(face, o.police->TexId(), NkVec2{apercu.x + 20.f, y + o.police->Ascent() * e}, "0123456789 éèàçù !?.,;:()",
								 c.pal.texte, e, apercu.w - 40.f);
				dl.PopClipRect();
			}

			void EditeurSon(NkEditeurCadre &c, NkOngletAsset &o, const NkRect &apercu, NkColonne &k) {
				NkEditeurModele &m = c.m;
				auto &dl = c.ctx.dl;
				NkSons2D *sons = c.ui.sonsApercu;
				const bool lu = sons != nullptr && o.sonId != 0u;
				const bool joue = lu && o.voix != 0u && sons->Joue(o.voix);
				if (!joue) {
					o.voix = 0u;
				}
				// L'apercu : un haut-parleur, et l'etat.
				dl.AddRectFilled(apercu, c.pal.fond);
				const float32 cx = apercu.x + apercu.w * 0.5f, cy = apercu.y + apercu.h * 0.45f;
				const NkColor col = joue ? c.pal.accent : c.pal.attenue;
				dl.AddRectFilled(NkRect{cx - 40.f, cy - 16.f, 22.f, 32.f}, col, 2.f);
				dl.AddTriangleFilled(NkVec2{cx - 18.f, cy - 16.f}, NkVec2{cx + 10.f, cy - 40.f}, NkVec2{cx + 10.f, cy + 40.f}, col);
				dl.AddTriangleFilled(NkVec2{cx - 18.f, cy - 16.f}, NkVec2{cx + 10.f, cy + 40.f}, NkVec2{cx - 18.f, cy + 16.f}, col);
				for (int32 i = 1; i <= (joue ? 3 : 1); ++i) {
					const float32 rx = cx + 18.f + static_cast<float32>(i) * 12.f;
					dl.AddLine(NkVec2{rx, cy - 10.f * static_cast<float32>(i)}, NkVec2{rx, cy + 10.f * static_cast<float32>(i)}, col, 2.f);
				}
				renderer::NkTexteCentre(dl, c.police, cx, cy + 70.f, joue ? "Lecture…" : (lu ? "À l'arrêt" : "Son illisible"), c.pal.texte);
				Titre(k, "Son");
				Info(k, "Durée", lu ? NkString::Format("%.2f s", static_cast<double>(sons->Duree(o.sonId))).CStr() : "—");
				Info(k, "Sortie", sons != nullptr && sons->Actif() ? "NKAudio" : "aucune (pas de périphérique)");
				NkGuiContext &ctx = c.ctx;
				ctx.PushId(o.nav.CStr());
				NkRect ch = Rangee(k, "Volume");
				ctx.SetNextItemRect(ch);
				o.modifie |= nkgui::SliderFloat(ctx, "##vol", o.son.volume, 0.f, 2.f);
				ch = Rangee(k, "En boucle");
				ctx.SetNextItemRect(NkRect{ch.x, ch.y, ch.h, ch.h});
				o.modifie |= nkgui::Checkbox(ctx, "##boucle", o.son.boucle);
				ctx.PopId();
				NkRect rj;
				if (Bouton(k, joue ? "■  Arrêter" : "▶  Lire", &rj, lu)) {
					if (joue) {
						sons->Couper(o.voix, 0.05f);
						o.voix = 0u;
					} else {
						o.voix = sons->Jouer(o.sonId, o.son.volume, 1.f, 0.f, o.son.boucle, "SFX");
					}
				}
				c.ui.assetLire = rj;
				if (Bouton(k, o.modifie ? "Enregistrer les réglages" : "Réglages enregistrés", nullptr, o.modifie)) {
					o.modifie = !NkEditeurEcrireReglagesSon(m, o.nav.CStr(), o.son);
				}
			}

			void EditeurControleur(NkEditeurCadre &c, NkOngletAsset &o, const NkRect &apercu, NkColonne &k) {
				auto &dl = c.ctx.dl;
				dl.AddRectFilled(apercu, c.pal.fond);
				const anim::NkAnimStateMachine *mach = NkModeleAnimateur(o.modele.CStr());
				Titre(k, "Contrôleur d'animation");
				Info(k, "Modèle", o.modele.CStr());
				if (mach == nullptr) {
					Info(k, "État", "illisible (.nkanimctl attendu)");
					return;
				}
				Info(k, "États", NkString::Format("%d", mach->GetStateCount()).CStr());
				Info(k, "Transitions", NkString::Format("%u", mach->GetTransitionCount()).CStr());
				Info(k, "Paramètres", NkString::Format("%u", mach->GetParamCount()).CStr());
				for (uint32 i = 0; i < mach->GetParamCount(); ++i) {
					static const char *kGenres[3] = {"bool", "réel", "déclencheur"};
					Info(k, mach->GetParamName(i).CStr(),
						 NkString::Format("%s = %.2f", kGenres[static_cast<int32>(mach->GetParamKind(i)) % 3], static_cast<double>(mach->GetParamValue(i)))
							 .CStr());
				}
				Bouton(k, "Éditer le graphe (NKEditorKit, à venir)", nullptr, false);
				// L'apercu : les etats, indentes par niveau (le graphe en texte).
				float32 y = apercu.y + 20.f;
				const float32 lh = renderer::NkTexteHauteurLigne(c.police, 16.f) + 6.f;
				renderer::NkTexte(dl, c.police, apercu.x + 20.f, y, "États (lecture) :", c.pal.attenue);
				y += lh + 4.f;
				c.ui.assetEtats = 0;
				for (int32 s = 0; s < mach->GetStateCount() && y < apercu.y + apercu.h - lh; ++s) {
					const float32 x = apercu.x + 28.f + 22.f * static_cast<float32>(mach->GetStateDepth(s));
					NkString t(mach->GetStateName(s).CStr());
					if (mach->IsSubMachine(s)) {
						t.Append("  (sous-machine)");
					} else {
						t.Append(NkString::Format("  · clip %d", mach->GetStateTag(s)).CStr());
					}
					const NkRect r{x, y, renderer::NkTexteLargeur(c.police, t.CStr()) + 20.f, lh - 2.f};
					dl.AddRectFilled(r, mach->IsSubMachine(s) ? c.pal.entete : c.pal.panneau, 3.f);
					dl.AddRect(r, c.pal.bord, 1.f, 3.f);
					renderer::NkTexte(dl, c.police, r.x + 10.f, r.y + 2.f, t.CStr(), c.pal.texte);
					y += lh;
					++c.ui.assetEtats;
				}
			}
		} // namespace

		void NkEditeurDessinerAsset(NkEditeurCadre &c) {
			NkEditeurInterface &ui = c.ui;
			auto &dl = c.ctx.dl;
			// ── Le MODE PREFAB : un bandeau sur la vue (la vue reste celle de l'editeur) ──
			if (NkOngletAsset *p = NkEditeurPrefabOuvert(ui)) {
				// En BAS de la vue : le haut est a la barre flottante des outils.
				const NkRect b{ui.viseur.x, ui.viseur.y + ui.viseur.h - 30.f, ui.viseur.w, 30.f};
				dl.AddRectFilled(b, NkColor{40, 70, 120, 230});
				const NkString t = NkString::Format("Prefab : %s — l'éditeur travaille sur le prefab seul", NkEditeurRelatifContenu(p->nav.CStr()).CStr());
				renderer::NkTexte(dl, c.police, b.x + 10.f, b.y + (b.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f, t.CStr(),
								  NkColor{235, 240, 250, 255}, b.w - 360.f);
				const NkRect rs{b.x + b.w - 340.f, b.y + 3.f, 180.f, b.h - 6.f};
				const NkRect rr{b.x + b.w - 154.f, b.y + 3.f, 146.f, b.h - 6.f};
				ui.assetEnregistrer = rs;
				ui.assetRevenir = rr;
				if (NkEditeurBouton(c, rs, "", false, true)) {
					NkEditeurEnregistrerPrefabOuvert(c);
				}
				renderer::NkTexteDansBoite(dl, c.police, rs, "Enregistrer (Ctrl+S)", c.pal.texte);
				if (NkEditeurBouton(c, rr, "", false, true)) {
					NkEditeurActiverOnglet(c, -1);
				}
				renderer::NkTexteDansBoite(dl, c.police, rr, "Revenir à la scène", c.pal.texte);
				return;
			}
			if (!NkEditeurAssetALaPlaceDeLaVue(ui)) {
				return;
			}
			NkOngletAsset &o = *ui.onglets[static_cast<uint32>(ui.ongletActif)];
			const NkRect zone = ui.vue;
			dl.AddRectFilled(zone, c.pal.fond);
			// L'en-tete : le genre et le chemin.
			const NkRect tete{zone.x, zone.y, zone.w, 30.f};
			dl.AddRectFilled(tete, c.pal.entete);
			dl.AddRectFilled(NkRect{tete.x, tete.y + tete.h - 1.f, tete.w, 1.f}, c.pal.bord);
			renderer::NkTexte(dl, c.police, tete.x + 10.f, tete.y + (tete.h - renderer::NkTexteHauteurLigne(c.police, 16.f)) * 0.5f,
							  NkString::Format("%s  —  %s", NkNomGenreAsset(o.genre), o.nav.CStr()).CStr(), c.pal.texte, tete.w - 20.f);
			const float32 wCol = zone.w > 700.f ? 330.f : zone.w * 0.45f;
			const NkRect apercu{zone.x + 6.f, tete.y + tete.h + 6.f, zone.w - wCol - 18.f, zone.h - tete.h - 12.f};
			NkColonne k{c, NkRect{apercu.x + apercu.w + 6.f, apercu.y, wCol, apercu.h}, apercu.y};
			dl.AddRectFilled(k.zone, c.pal.panneau);
			dl.AddRect(apercu, c.pal.bord, 1.f);
			switch (o.genre) {
				case NkGenreAsset::NK_TEXTURE:
					EditeurTexture(c, o, apercu, k);
					break;
				case NkGenreAsset::NK_POLICE:
					EditeurPolice(c, o, apercu, k);
					break;
				case NkGenreAsset::NK_SON:
					EditeurSon(c, o, apercu, k);
					break;
				case NkGenreAsset::NK_CONTROLEUR:
					EditeurControleur(c, o, apercu, k);
					break;
				default:
					break;
			}
		}

	} // namespace editeur
} // namespace nkentseu
