//
// NkEditeurReferences.cpp
// =============================================================================
// Les references d'assets des Details (NkEditeurReferences.h).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Editeur/NkEditeurReferences.h"

#include "Editeur/NkEditeurActions.h"
#include "Editeur/NkEditeurContenu.h"

#include "NKEditorKit/Components/NkContentBrowserDisque.h"

namespace nkentseu {
	namespace editeur {

		namespace {
			/// Le nom affichable d'une entite (pour l'annonce).
			NkString NomEntite(NkEditeurModele &m, ecs::NkEntityId id) {
				const NkEtiquette *e = m.scene.Monde().Get<NkEtiquette>(id);
				return e != nullptr && e->nom[0] != '\0' ? NkString(e->nom) : NkString("l'entité");
			}
		} // namespace

		bool NkEditeurEstImage(const char *cheminNav) noexcept {
			return cheminNav != nullptr && NkEditeurNatureFichier(cheminNav).type == NkAssetType::Texture2D;
		}

		bool NkEditeurTextureSprite(NkEditeurModele &m, ecs::NkEntityId id, const char *cheminNav) {
			if (!m.scene.Monde().IsAlive(id) || !m.scene.Monde().Has<NkSprite2D>(id)) {
				NkEditeurAnnoncer(m, "Texture : choisissez une entité qui a un sprite");
				return false;
			}
			if (!NkEditeurEstImage(cheminNav) || !NkEditeurCheminEstContenu(cheminNav)) {
				NkEditeurAnnoncer(m, "Texture : seule une image du Contenu se pose sur un sprite");
				return false;
			}
			// ABSOLU : NkTextures2D prefixe un chemin relatif par sa racine d'assets.
			const NkString abs = NkEditeurCheminContenuAbsolu(m, cheminNav);
			const uint32 tex = m.textures.Charger(abs.CStr());
			int32 tw = 0, th = 0;
			if (tex == 0u || !m.textures.Taille(tex, tw, th) || tw <= 0 || th <= 0) {
				NkEditeurAnnoncer(m, NkString::Format("Image illisible : %s", cheminNav).CStr());
				return false;
			}
			const NkSprite2D *avant = m.scene.Monde().Get<NkSprite2D>(id);
			if (avant->texId == tex && avant->uv0.x == 0.f && avant->uv0.y == 0.f && avant->uv1.x == 1.f && avant->uv1.y == 1.f) {
				return true; // deja elle : rien a retenir
			}
			NkEditeurRetenir(m);
			NkSprite2D *s = m.scene.Monde().Get<NkSprite2D>(id);
			s->texId = tex;
			// L'image ENTIERE : une region d'atlas d'avant n'a pas de sens ici.
			s->uv0 = NkVec2f(0.f, 0.f);
			s->uv1 = NkVec2f(1.f, 1.f);
			NkEditeurAnnoncer(m, NkString::Format("Texture « %s » posée sur « %s »", editorkit::NkDisqueNom(cheminNav).CStr(),
												  NomEntite(m, id).CStr())
									 .CStr());
			return true;
		}

		bool NkEditeurSansTexture(NkEditeurModele &m, ecs::NkEntityId id) {
			if (!m.scene.Monde().IsAlive(id) || !m.scene.Monde().Has<NkSprite2D>(id)) {
				return false;
			}
			if (m.scene.Monde().Get<NkSprite2D>(id)->texId == 0u) {
				return true;
			}
			NkEditeurRetenir(m);
			NkSprite2D *s = m.scene.Monde().Get<NkSprite2D>(id);
			s->texId = 0u;
			s->uv0 = NkVec2f(0.f, 0.f);
			s->uv1 = NkVec2f(1.f, 1.f);
			NkEditeurAnnoncer(m, NkString::Format("« %s » : plus de texture", NomEntite(m, id).CStr()).CStr());
			return true;
		}

		NkString NkEditeurNavDeTexture(NkEditeurModele &m, uint32 texId) {
			if (texId == 0u) {
				return NkString();
			}
			const char *nom = m.textures.Nom(texId);
			if (nom == nullptr || nom[0] == '\0') {
				return NkString();
			}
			return NkEditeurNavigateurDe(m, nom);
		}

		void NkEditeurImagesDuContenu(NkEditeurModele &m, NkVector<NkString> &sortie, uint32 maxi) {
			NkEditeurAssetsDuContenu(m, NkAssetType::Texture2D, sortie, maxi);
		}

		NkString NkEditeurNomControleur(const char *cheminNav) {
			const char *nom = cheminNav;
			for (const char *p = cheminNav; p != nullptr && *p != '\0'; ++p) {
				if (*p == '/' || *p == '\\') {
					nom = p + 1;
				}
			}
			NkString n(nom != nullptr ? nom : "");
			const usize point = n.RFind('.');
			if (point != NkString::npos && point > 0u) {
				n = NkString(n.SubStr(0, point));
			}
			if (n.Length() >= static_cast<usize>(NK_UNKENY_ANIM_MODELE_MAX)) {
				n = NkString(n.SubStr(0, static_cast<usize>(NK_UNKENY_ANIM_MODELE_MAX - 1)));
			}
			return n;
		}

		bool NkEditeurAjouterControleur(NkEditeurModele &m, ecs::NkEntityId id, const char *cheminNav) {
			const NkString nom = NkEditeurNomControleur(cheminNav);
			const NkString disque = NkEditeurCheminContenu(m, cheminNav);
			return !nom.Empty() && NkEditeurAjouterAnimateur(m, id, nom.CStr(), disque.CStr());
		}

		bool NkEditeurRetrouverControleur(NkEditeurModele &m, const char *modele) {
			NkVector<NkString> ctl;
			NkEditeurAssetsDuContenu(m, NkAssetType::AnimationController, ctl, 200u);
			for (uint32 k = 0; k < ctl.Size(); ++k) {
				if (NkEditeurNomControleur(ctl[k].CStr()) == NkString(modele)) {
					return NkChargerModeleAnimateur(modele, NkEditeurCheminContenu(m, ctl[k].CStr()).CStr());
				}
			}
			return false;
		}

		void NkEditeurAssetsDuContenu(NkEditeurModele &m, NkAssetType type, NkVector<NkString> &sortie, uint32 maxi) {
			sortie.Clear();
			NkVector<NkString> dossiers;
			dossiers.PushBack(NkString()); // la racine d'abord
			NkVector<NkString> sous;
			NkEditeurDossiersContenu(m, sous);
			for (uint32 i = 0; i < sous.Size(); ++i) {
				dossiers.PushBack(sous[i]);
			}
			NkVector<NkElementContenu> elements;
			for (uint32 d = 0; d < dossiers.Size() && sortie.Size() < maxi; ++d) {
				NkEditeurListerContenu(m, dossiers[d].CStr(), elements);
				for (uint32 i = 0; i < elements.Size() && sortie.Size() < maxi; ++i) {
					const NkElementContenu &e = elements[i];
					if (!e.dossier && e.nature.type == type) {
						NkString nav(NK_CONTENU_RACINE);
						nav.Append('/');
						nav.Append(e.relatif);
						sortie.PushBack(nav);
					}
				}
			}
		}

	} // namespace editeur
} // namespace nkentseu
