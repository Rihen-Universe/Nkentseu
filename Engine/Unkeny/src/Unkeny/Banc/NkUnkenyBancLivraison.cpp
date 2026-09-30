//
// NkUnkenyBancLivraison.cpp
// =============================================================================
// Description :
//   Le banc de la livraison. Sans fenetre, sans GPU : la cuisson ecrit dans un
//   dossier temporaire, la relecture est celle du joueur autonome.
//
// PRE-ENREGISTREMENT (ecrit avant le premier chiffre) :
//   (l1)  une scene (sol, caisse et balle TEXTUREES, blob) cuite puis relue par
//         NkChargerJeu dans une scene NEUVE, avec des textures NEUVES, a LA
//         MEME EMPREINTE que la scene cuite ; ses deux textures sont relues
//   (l1n) la meme scene relue puis une caisse deplacee d'un millimetre :
//         l'empreinte CHANGE (le temoin n'est pas aveugle)
//   (l2)  le fichier d'une texture cuite supprime : la scene se joue quand
//         meme, et le manque NOMME la texture (« unkeny/sim/caisse ») ET son
//         fichier ; (l2n) jeu complet : aucun manque
//   (l2b) sommaire absent : pas jouable, et l'erreur nomme le chemin cherche
//   (l2c) sommaire present, scene absente : l'erreur nomme la scene
//   (l3)  un son cuit puis relu : meme duree, trouve sous son nom
//   (l4)  les fichiers ecrits portent les extensions de la table
//         (NkAssetExtensionFor) et leur en-tete dit la bonne nature
//   (l5)  deux relectures jouees 1 s par NkAvancerPartie : meme empreinte
//         (la partie est deterministe a partir d'un jeu cuit)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Banc/NkUnkenyBancLivraison.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKMemory/NKMemory.h"
#include "NKSerialization/Asset/NkAssetMetadata.h"
#include "Unkeny/Livraison/NkUnkenyLivraison.h"
#include "Unkeny/Partie/NkUnkenyPartie.h"
#include "Unkeny/Rendu/NkUnkenyTextures.h"
#include "Unkeny/Scene/NkUnkenyScene.h"
#include "Unkeny/Simulation/NkUnkenyActeurs.h"
#include "Unkeny/Son/NkUnkenySon.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			int32 gEchecs = 0;
			int32 gReussis = 0;

			void Temoin(bool ok, const char *quoi, float32 valeur) {
				std::printf("  [%s] %-66s %10.4f\n", ok ? " OK " : "ECHEC", quoi, static_cast<double>(valeur));
				(ok ? gReussis : gEchecs)++;
			}

			/// Tout ce qu'une partie relue demande. Sur le TAS : deux scenes et
			/// leurs mondes physiques ne tiennent pas dans une pile de 1 Mo.
			struct NkBancJeu {
					NkScene scene;
					NkTextures2D textures;
					NkSons2D sons;
					NkJeuCharge jeu;
			};

			void Composer(NkScene &s, NkRessourcesSim &res) {
				NkSceneConfig cfg;
				cfg.physique = true;
				cfg.particules = true;
				s.Init(cfg);
				NkRemettreNomsSim();
				const ecs::NkEntityId sol = s.Creer("Sol", NkVec2f(0.f, -4.5f));
				NkSprite2D sp;
				sp.taille = NkVec2f(22.f, 1.f);
				sp.couleur = 0x3E4756FFu;
				sp.visible = false;
				s.Monde().Add<NkSprite2D>(sol, sp);
				NkCollisionneur2D col;
				col.demiTaille = NkVec2f(11.f, 0.5f);
				s.Monde().Add<NkCollisionneur2D>(sol, col);
				NkCorps2D corps;
				corps.type = NkTypeCorps::NK_STATIQUE;
				s.AjouterCorps(sol, corps);
				NkPoserActeurSim(s, NkActeurSim::NK_CAISSE, NkVec2f(-3.f, -3.f), &res);
				NkPoserActeurSim(s, NkActeurSim::NK_BALLE, NkVec2f(2.f, -3.f), &res);
				NkPoserActeurSim(s, NkActeurSim::NK_BLOB, NkVec2f(0.f, -1.f), &res);
				s.Camera().PoserCentre(NkVec2f(0.5f, -1.f));
				s.Camera().PoserZoom(40.f);
			}

			bool Contient(const NkVector<NkString> &lignes, const char *a, const char *b) {
				for (usize i = 0; i < lignes.Size(); ++i) {
					if (lignes[i].Find(a) != NkString::npos && (b == nullptr || lignes[i].Find(b) != NkString::npos)) {
						return true;
					}
				}
				return false;
			}

			ecs::NkEntityId Premiere(NkScene &s, const char *prefixe) {
				ecs::NkEntityId t = ecs::NkEntityId::Invalid();
				s.Monde().Query<NkEtiquette>().ForEach([&](ecs::NkEntityId id, NkEtiquette &e) {
					if (!t.IsValid() && std::strncmp(e.nom, prefixe, std::strlen(prefixe)) == 0) {
						t = id;
					}
				});
				return t;
			}

			NkAssetType NatureDe(const NkString &chemin) {
				NkAssetMetadata meta;
				NkString err;
				if (!NkAssetIO::ReadMetadata(chemin.CStr(), meta, &err)) {
					return NkAssetType::Unknown;
				}
				return meta.type;
			}
		} // namespace

		int32 NkUnkenyLancerBancLivraison() {
			gEchecs = 0;
			gReussis = 0;
			std::printf("\nUnkeny — banc de la livraison (cuire, relire, jouer)\n\n");
			auto &tas = memory::NkGetDefaultAllocator();
			const NkString dossier = (NkDirectory::GetTempDirectory() / "unkeny_banc_livraison").ToString() + "/";
			NkDirectory::Delete(dossier.CStr(), true);

			// ── La scene de l'editeur, et sa cuisson ────────────────────────────
			NkBancJeu *source = tas.New<NkBancJeu>();
			NkRessourcesSim res;
			NkCreerRessourcesSim(res, &source->textures, nullptr);
			Composer(source->scene, res);
			NkDemandeCuisson demande;
			demande.dossier = dossier;
			demande.nomJeu = "Banc";
			demande.vueLargeur = 800.f;
			demande.vueHauteur = 450.f;
			NkSonACuire son;
			son.nom = "banc/bip";
			son.frequence = 22050;
			son.mono.Resize(2205u);
			for (usize i = 0; i < son.mono.Size(); ++i) {
				son.mono[i] = (i % 50u < 25u) ? 0.25f : -0.25f;
			}
			demande.sons.PushBack(son);
			NkRapportCuisson rapport;
			const bool cuit = NkCuireJeu(source->scene, source->textures, demande, rapport);
			const uint64 empreinteEditeur = NkEmpreinteScene(source->scene, &source->textures);

			// (l1)
			NkBancJeu *relu = tas.New<NkBancJeu>();
			const bool jouable = NkChargerJeu(dossier.CStr(), relu->scene, relu->textures, nullptr, relu->jeu);
			Temoin(cuit && jouable && rapport.empreinte == empreinteEditeur && relu->jeu.empreinteLue == empreinteEditeur &&
					   relu->jeu.EmpreinteIdentique() && relu->jeu.textures == 2u && rapport.erreurs.Empty(),
				   "(l1) cuite puis relue : meme empreinte, deux textures relues", static_cast<float32>(relu->jeu.textures));

			// (l1n)
			const ecs::NkEntityId caisse = Premiere(relu->scene, "Caisse");
			const NkTransform2D *tc = caisse.IsValid() ? relu->scene.Monde().Get<NkTransform2D>(caisse) : nullptr;
			if (tc != nullptr) {
				relu->scene.TeleporterEntite(caisse, tc->position + NkVec2f(0.001f, 0.f));
			}
			Temoin(tc != nullptr && NkEmpreinteScene(relu->scene, &relu->textures) != empreinteEditeur,
				   "(l1n) une caisse deplacee d'un millimetre : l'empreinte change", 0.001f);

			// (l2n) le jeu complet ne signale rien
			Temoin(jouable && relu->jeu.manquantes.Empty(), "(l2n) jeu complet : aucun manque",
				   static_cast<float32>(relu->jeu.manquantes.Size()));

			// (l3) le son, relu sous son nom (moteur a sortie nulle : pas de carte)
			NkBancJeu *avecSon = tas.New<NkBancJeu>();
			const bool moteur = avecSon->sons.Demarrer(true);
			NkChargerJeu(dossier.CStr(), avecSon->scene, avecSon->textures, &avecSon->sons, avecSon->jeu);
			const uint32 idSon = avecSon->sons.Trouver("banc/bip");
			const float32 duree = idSon != 0u ? avecSon->sons.Duree(idSon) : 0.f;
			Temoin(moteur && avecSon->jeu.sons == 1u && idSon != 0u && math::NkAbs(duree - 0.1f) < 1.0e-4f,
				   "(l3) son cuit puis relu : trouve sous son nom, meme duree (s)", duree);
			avecSon->sons.Arreter();
			tas.Delete(avecSon);

			// (l4) les extensions et la nature des fichiers
			NkString fTex;
			NkString fSon;
			for (usize i = 0; i < rapport.fichiers.Size(); ++i) {
				if (rapport.fichiers[i].StartsWith("textures/") && fTex.Empty()) {
					fTex = rapport.fichiers[i];
				}
				if (rapport.fichiers[i].StartsWith("sons/")) {
					fSon = rapport.fichiers[i];
				}
			}
			const NkString extTex = NkString(".") + NkAssetExtensionFor(NkAssetType::Texture2D);
			const NkString extSon = NkString(".") + NkAssetExtensionFor(NkAssetType::Sound);
			Temoin(fTex.EndsWith(extTex.CStr()) && fSon.EndsWith(extSon.CStr()) &&
					   NatureDe(dossier + fTex) == NkAssetType::Texture2D && NatureDe(dossier + fSon) == NkAssetType::Sound,
				   "(l4) .nktex et .nksnd, et leur en-tete dit Texture2D / Sound", static_cast<float32>(rapport.fichiers.Size()));

			// (l5) deux relectures jouees 1 s : la meme partie
			NkBancJeu *a = tas.New<NkBancJeu>();
			NkBancJeu *b = tas.New<NkBancJeu>();
			NkChargerJeu(dossier.CStr(), a->scene, a->textures, nullptr, a->jeu);
			NkChargerJeu(dossier.CStr(), b->scene, b->textures, nullptr, b->jeu);
			for (int32 k = 0; k < 60; ++k) {
				NkAvancerPartie(a->scene, 1.f / 60.f);
				NkAvancerPartie(b->scene, 1.f / 60.f);
			}
			const uint64 ea = NkEmpreinteScene(a->scene, &a->textures);
			const uint64 eb = NkEmpreinteScene(b->scene, &b->textures);
			Temoin(ea != 0u && ea == eb && ea != empreinteEditeur, "(l5) deux relectures jouees 1 s : meme empreinte", 60.f);
			tas.Delete(a);
			tas.Delete(b);

			// (l2) une texture cuite supprimee
			const NkString fCaisse = dossier + "textures/unkeny_sim_caisse." + NkAssetExtensionFor(NkAssetType::Texture2D);
			const bool supprimee = NkFile::Delete(fCaisse.CStr());
			NkBancJeu *manque = tas.New<NkBancJeu>();
			const bool jouableQuandMeme = NkChargerJeu(dossier.CStr(), manque->scene, manque->textures, nullptr, manque->jeu);
			Temoin(supprimee && jouableQuandMeme && Contient(manque->jeu.manquantes, "unkeny/sim/caisse", "unkeny_sim_caisse.nktex"),
				   "(l2) texture supprimee : jouable, et le manque la NOMME (nom + fichier)",
				   static_cast<float32>(manque->jeu.manquantes.Size()));
			tas.Delete(manque);

			// (l2c) la scene absente
			const bool sansScene = NkFile::Delete((dossier + "scene.nkscene").CStr());
			NkBancJeu *vide = tas.New<NkBancJeu>();
			const bool jouableSansScene = NkChargerJeu(dossier.CStr(), vide->scene, vide->textures, nullptr, vide->jeu);
			Temoin(sansScene && !jouableSansScene && vide->jeu.erreur.Find("scene.nkscene") != NkString::npos,
				   "(l2c) scene absente : pas jouable, l'erreur nomme la scene", 0.f);

			// (l2b) le sommaire absent
			const bool sansSommaire = NkFile::Delete((dossier + NK_LIVRAISON_SOMMAIRE).CStr());
			const bool jouableSansSommaire = NkChargerJeu(dossier.CStr(), vide->scene, vide->textures, nullptr, vide->jeu);
			Temoin(sansSommaire && !jouableSansSommaire && vide->jeu.erreur.Find(NK_LIVRAISON_SOMMAIRE) != NkString::npos &&
					   vide->jeu.erreur.Find("unkeny_banc_livraison") != NkString::npos,
				   "(l2b) sommaire absent : pas jouable, l'erreur nomme le chemin", 0.f);
			tas.Delete(vide);

			tas.Delete(relu);
			tas.Delete(source);
			NkDirectory::Delete(dossier.CStr(), true);
			std::printf("\n%s : %d reussis, %d echec%s\n", gEchecs == 0 ? "BANC LIVRAISON REUSSI" : "BANC LIVRAISON EN ECHEC", gReussis,
						gEchecs, gEchecs > 1 ? "s" : "");
			return gEchecs == 0 ? 0 : 1;
		}

	} // namespace unkeny
} // namespace nkentseu
