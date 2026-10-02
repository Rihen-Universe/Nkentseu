#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkAnimaLanceur.h — LE LANCEUR DE NkAnimaEditor (2026-10-01).
//
// Le composant est celui de toute la famille (NKEditorKit,
// NkProjectLauncherShell.h) ; ce fichier n'en porte que LA TOUCHE de
// l'editeur d'animation : son nom, son glyphe (un os), son cyan, ses
// personnages de depart, l'extension des modeles qu'il ouvre.
//
// CE QUI EST OUVERT, C'EST UN PERSONNAGE ANIME (.glb / .gltf / .fbx) : la
// porte est `AnimInit`, celle-la meme que la ligne de commande empruntait. Les
// « modeles » sont les personnages livres dans Resources/Models (resolus comme
// NKRenderer resout ses shaders : l'application se lance de n'importe ou) ;
// un fichier absent se montre « a venir », jamais un faux bouton.
//
// ⚠️ LE LANCEUR NE S'OUVRE QU'AU LANCEMENT INTERACTIF : sans modele sur la
//    ligne de commande, sans sonde. Un appel scripte (`NkAnimaEditor.exe
//    modele.glb`, `--sonde-coquille`, les variables NK_*) garde exactement le
//    comportement d'avant.
// =============================================================================

#include "NKEditorKit/NkProjectLauncherShell.h"
#include "NKWindow/Core/NkDialogs.h"
#include "NKWindow/Core/NkLauncher.h"
#include "NKFileSystem/NkPath.h"
#include "NKMath/NkColor.h"
#include "AnimBridge.h"
#include <cstdio>

namespace nkanima {

	struct NkAnimaLanceurEtat {
			nkentseu::editorkit::NkLanceurRecents recents;
			nkentseu::NkVector<nkentseu::NkString> chemins; ///< un par modele, resolu
	};
	inline NkAnimaLanceurEtat &AnimaLanceurEtat() {
		static NkAnimaLanceurEtat e;
		return e;
	}

	/// LA TOUCHE de NkAnimaEditor.
	inline void AnimaRemplirLanceur(nkentseu::editorkit::NkProjectLauncherModel &m) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		using G = NkLanceurGlyphe;
		m.identite.nom = NkString("NkAnimaEditor");
		m.identite.prefixe = NkString("NkAnima");
		m.identite.sousTitre = NkString("Animation squelettique — Nkentseu");
		m.identite.version = NkString("0.1.0");
		m.identite.extensions = NkString(".glb .gltf .fbx .nkskel .nkanim");
		m.identite.accent = math::NkColor(0x00, 0xA8, 0xD0).ToUint32A(); // le cyan de l'editeur
		m.identite.glyphe = G::Os;

		NkAnimaLanceurEtat &E = AnimaLanceurEtat();
		E.chemins.Clear();
		m.modeles.Clear();
		struct D {
				const char *nom, *cat, *desc, *chemin;
				G glyphe;
				uint32 teinte;
		};
		const D kModeles[] = {
			{"Homme Cesium", "PERSONNAGE", "Un humain qui marche : squelette, peau et un cycle de marche a retoucher.",
			 "Resources/Models/CesiumMan/CesiumMan.glb", G::Personnage, 0u},
			{"Renard", "QUADRUPEDE", "Un renard et ses trois allures : la locomotion a quatre pattes.",
			 "Resources/Models/Fox/Fox.glb", G::Paysage, math::NkColor(0xE0, 0x7A, 0x2E).ToUint32A()},
			{"Robot BrainStem", "MECANIQUE", "Un robot articule a nombreux os : la cinematique d'une machine.",
			 "Resources/Models/BrainStem/BrainStem.glb", G::Reglages, math::NkColor(0x8E, 0x6B, 0xD8).ToUint32A()},
			{"Peau simple", "APPRENDRE", "Deux os et un ruban de peau : le plus petit squelette pour comprendre.",
			 "Resources/Models/SimpleSkin/SimpleSkin.gltf", G::Os, math::NkColor(0x2E, 0xA0, 0x6B).ToUint32A()},
		};
		for (const D &d : kModeles) {
			NkLanceurModele md;
			md.nom = NkString(d.nom);
			md.categorie = NkString(d.cat);
			md.description = NkString(d.desc);
			md.glyphe = d.glyphe;
			md.couleur = d.teinte;
			const NkString c = AnimCheminRessource(d.chemin);
			md.disponible = NkFile::Exists(c.CStr());
			if (!md.disponible)
				md.raison = NkString("fichier absent");
			E.chemins.PushBack(c);
			m.modeles.PushBack(md);
		}

		m.pages.Clear();
		NkLanceurPage pp;
		pp.libelle = NkString("Projets");
		pp.glyphe = G::Projets;
		m.pages.PushBack(pp);
		NkLanceurPage pa;
		pa.libelle = NkString("Apprendre");
		pa.sousTitre = NkString("Les gestes de l'animation par poses-cles.");
		pa.glyphe = G::Apprendre;
		pa.type = NkLanceurPageType::Liens;
		auto lien = [&](NkLanceurPage &pg, const char *t, const char *dsc, G g) {
			NkLanceurLien l;
			l.titre = NkString(t);
			l.description = NkString(dsc);
			l.glyphe = g;
			l.disponible = false;
			l.badge = NkString("a venir");
			pg.liens.PushBack(l);
		};
		lien(pa, "La timeline", "Inserer, deplacer et supprimer des cles ; jouer et parcourir le temps.", G::Horloge);
		lien(pa, "Editer une pose", "Saisir un os, le tourner, enregistrer la pose en cle.", G::Os);
		lien(pa, "Physique et equilibre", "Le centre de masse, le polygone d'appui et le ragdoll.", G::Personnage);
		m.pages.PushBack(pa);
		NkLanceurPage pi;
		pi.libelle = NkString("Installations");
		pi.sousTitre = NkString("Ce qui est installe sur cette machine.");
		pi.glyphe = G::Installations;
		pi.type = NkLanceurPageType::Liens;
		{
			NkLanceurLien l;
			l.titre = NkString("NkAnimaEditor 0.1.0");
			l.description = NkString("Installe dans ") + NkPath::GetExecutableDirectory().ToString();
			l.glyphe = G::Os;
			l.badge = NkString("installe");
			pi.liens.PushBack(l);
		}
		m.pages.PushBack(pi);
		m.astuce = NkString("Astuce : Espace joue ou met en pause, I insere une cle a la tete de lecture.");
		m.piedDePage = NkString("Ouvre les personnages animes glTF et FBX (squelette et peau).");
	}

	inline void AnimaRafraichirLanceur(void *, nkentseu::editorkit::NkLanceurCoquille &l) {
		AnimaLanceurEtat().recents.Remplir(l.modele);
	}

	inline bool AnimaChargerDepuisLanceur(nkentseu::editorkit::NkLanceurCoquille &l, const nkentseu::NkString &chemin,
										  const nkentseu::NkString &nom) {
		using namespace nkentseu;
		if (chemin.Empty() || !AnimInit(chemin.CStr())) {
			l.modele.erreur = NkString("Chargement impossible (personnage anime attendu) : ") + chemin;
			return false;
		}
		AnimaLanceurEtat().recents.Toucher(chemin, nom, editorkit::NkLanceurAujourdhui());
		std::printf("[LANCEUR] personnage ouvert : %s\n", chemin.CStr());
		std::fflush(stdout);
		return true;
	}

	/// Ce que fait chaque demande du lanceur. VRAI = un personnage est ouvert.
	inline bool AnimaAgirLanceur(void *, nkentseu::editorkit::NkLanceurCoquille &l,
								 const nkentseu::editorkit::NkProjectLauncherResult &r) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		NkAnimaLanceurEtat &E = AnimaLanceurEtat();
		const int32 i = r.index;
		const bool projetValide = i >= 0 && (usize)i < l.modele.projets.Size();
		switch (r.action) {
			case NkLanceurAction::NouveauProjet:
			case NkLanceurAction::NouveauDepuisModele: {
				const int32 k = (r.action == NkLanceurAction::NouveauProjet) ? 0 : i;
				if (k < 0 || (usize)k >= l.modele.modeles.Size() || !l.modele.modeles[(usize)k].disponible)
					return false;
				return AnimaChargerDepuisLanceur(l, E.chemins[(usize)k], l.modele.modeles[(usize)k].nom);
			}
			case NkLanceurAction::Ouvrir: {
				const NkDialogResult d =
					NkDialogs::OpenFileDialog("*.glb;*.gltf;*.fbx;*.nkskel;*.nkanim", "Ouvrir un personnage anime (glTF, FBX, squelette 2D)");
				if (!d.confirmed || d.path.Empty())
					return false;
				return AnimaChargerDepuisLanceur(l, d.path, NkPath(d.path.CStr()).GetFileNameWithoutExtension());
			}
			case NkLanceurAction::OuvrirRecent:
				if (projetValide && l.modele.projets[(usize)i].etat == 0u)
					return AnimaChargerDepuisLanceur(l, l.modele.projets[(usize)i].chemin, l.modele.projets[(usize)i].nom);
				return false;
			case NkLanceurAction::Epingler:
				if (projetValide)
					E.recents.BasculerEpingle((usize)l.modele.projets[(usize)i].hote);
				return false;
			case NkLanceurAction::Retirer:
				if (projetValide)
					E.recents.Retirer((usize)l.modele.projets[(usize)i].hote);
				return false;
			case NkLanceurAction::Purger:
				for (isize k = (isize)E.recents.entrees.Size() - 1; k >= 0; --k)
					if (!NkFile::Exists(E.recents.entrees[(usize)k].chemin.CStr()))
						E.recents.Retirer((usize)k);
				return false;
			case NkLanceurAction::OuvrirLien:
				if (r.url && *r.url)
					NkLauncher::OpenURL(r.url);
				return false;
			default:
				return false;
		}
	}

	/// `--capture-lanceur=FICHIER.png [--theme-lanceur=clair]` : la photo sans
	/// fenetre ni GPU du lanceur, par le MEME composant et le MEME modele.
	inline int AnimaCapturerLanceur(const nkentseu::NkString &chemin, bool clair) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		static NkProjectLauncherModel m;
		AnimaRemplirLanceur(m);
		AnimaLanceurEtat().recents.Charger("NkAnimaEditor");
		AnimaLanceurEtat().recents.Remplir(m);
		m.themeBasculable = true;
		m.themeSombre = !clair;
		NkLanceurCaptureDesc d;
		d.chemin = chemin.CStr();
		char msg[512];
		const bool ok = NkLanceurCapturer(d, m, clair ? NkTheme::Light() : NkTheme::Dark(), NkProjectLauncherHooks(),
										  NkProjectLauncherStyle(), msg, (int32)sizeof(msg));
		std::printf("[capture-lanceur] NkAnimaEditor : %s\n", msg);
		std::fflush(stdout);
		return ok ? 0 : 1;
	}

} // namespace nkanima
