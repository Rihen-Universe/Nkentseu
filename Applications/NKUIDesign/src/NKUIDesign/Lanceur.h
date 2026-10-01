#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// Lanceur.h — LE LANCEUR DE NKUIDesign (2026-10-01).
//
// Le composant est celui de toute la famille (NKEditorKit,
// NkProjectLauncherShell.h) ; ce fichier n'en porte que LA TOUCHE de l'atelier
// d'interfaces : son nom, ses calques, son violet, ses gabarits, .nkuidoc.
//
// CE QU'IL OUVRE PASSE PAR LES PORTES DE L'ATELIER, jamais une seconde :
//   - « Document vierge »    -> NouvelOngletVierge (la porte de Ctrl+N) ;
//   - les gabarits livres    -> un onglet par document (OuvrirOngletInactif +
//                               BasculerVers, comme les onglets de demonstration) ;
//   - « Ouvrir... »          -> le selecteur natif, puis la meme porte ;
//   - le document de travail -> il est DEJA charge au demarrage : le lanceur se
//                               retire et le montre.
//   « Via IA » reste « a venir », exactement comme dans le choix « Nouveau
//   projet » de l'atelier : un bouton affiche fait ce qu'il annonce.
//
// ⚠️ LE LANCEUR NE S'OUVRE QU'AU LANCEMENT INTERACTIF (aucun argument). Toutes
//    les sondes, recettes, captures et mises en scene passent des arguments :
//    elles gardent exactement le comportement d'avant.
// =============================================================================

#include "NKEditorKit/NkProjectLauncherShell.h"
#include "NKWindow/Core/NkDialogs.h"
#include "NKWindow/Core/NkLauncher.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"
#include "NKMath/NkColor.h"
#include <cstdio>

namespace nkuidesign {

	struct LanceurEtat {
			nkentseu::editorkit::NkLanceurRecents recents;
			nkentseu::NkVector<nkentseu::NkString> gabarits; ///< chemin par modele (vide = vierge / a venir)
	};
	inline LanceurEtat &EtatLanceur() {
		static LanceurEtat e;
		return e;
	}

	/// Un fichier livre : a cote du document de travail, puis au chemin du depot,
	/// puis en remontant depuis l'executable (meme recherche que les shaders).
	inline nkentseu::NkString LanceurTrouver(const char *nom) {
		using namespace nkentseu;
		NkString c1(nom);
		if (NkFile::Exists(c1.CStr()))
			return c1;
		NkString rel("Applications/NKUIDesign/design/mises_en_scene/");
		rel.Append(nom);
		if (NkFile::Exists(rel.CStr()))
			return rel;
		NkPath d = NkPath::GetExecutableDirectory();
		for (int32 k = 0; k < 8; ++k) {
			NkString s = d.ToString();
			if (s.Empty())
				break;
			s.Append("/");
			s.Append(rel);
			if (NkFile::Exists(s.CStr()))
				return s;
			const NkPath p = d.GetParent();
			if (p.ToString().Empty() || p.ToString() == d.ToString())
				break;
			d = p;
		}
		return NkString();
	}

	/// LA TOUCHE de NKUIDesign.
	inline void RemplirLanceur(nkentseu::editorkit::NkProjectLauncherModel &m) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		using G = NkLanceurGlyphe;
		m.identite.nom = NkString("NKUIDesign");
		m.identite.prefixe = NkString("NK");
		m.identite.sousTitre = NkString("Conception d'interfaces — Nkentseu");
		m.identite.version = NkString("0.1.0");
		m.identite.extensions = NkString(".nkuidoc");
		m.identite.motProjet = NkString("document");
		m.identite.motProjets = NkString("documents");
		m.identite.accent = math::NkColor(0x8E, 0x5B, 0xE8).ToUint32A();
		m.identite.glyphe = G::Calques;

		LanceurEtat &E = EtatLanceur();
		E.gabarits.Clear();
		m.modeles.Clear();
		auto ajoute = [&](const char *nom, const char *cat, const char *desc, G g, uint32 teinte, const NkString &chemin,
						  bool dispo, const char *raison) {
			NkLanceurModele md;
			md.nom = NkString(nom);
			md.categorie = NkString(cat);
			md.description = NkString(desc);
			md.glyphe = g;
			md.couleur = teinte;
			md.disponible = dispo;
			if (raison)
				md.raison = NkString(raison);
			m.modeles.PushBack(md);
			E.gabarits.PushBack(chemin);
		};
		ajoute("Document vierge", "VIERGE", "Une toile vide et une « Page 1 », dans un nouvel onglet.", G::Vide, 0u,
			   NkString(), true, nullptr);
		const NkString landing = LanceurTrouver("demo_landing_page.nkuidoc");
		ajoute("Page d'accueil web", "GABARIT", "Une page d'accueil complete : bandeau, sections, appel a l'action.",
			   G::Document, math::NkColor(0x2E, 0x8E, 0xD8).ToUint32A(), landing, !landing.Empty(), "fichier absent");
		const NkString hud = LanceurTrouver("demo_hud_jeu.nkuidoc");
		ajoute("HUD de jeu", "GABARIT", "Une interface de jeu : jauges, minicarte, inventaire rapide.", G::Manette,
			   math::NkColor(0xE0, 0x7A, 0x2E).ToUint32A(), hud, !hud.Empty(), "fichier absent");
		ajoute("Via IA", "ASSISTANT", "Decrire l'ecran voulu et valider l'apercu propose.", G::Etoile,
			   math::NkColor(0xF2, 0x98, 0x0E).ToUint32A(), NkString(), false, "a venir");

		m.pages.Clear();
		NkLanceurPage pp;
		pp.libelle = NkString("Projets");
		pp.titre = NkString("Documents");
		pp.glyphe = G::Projets;
		m.pages.PushBack(pp);
		NkLanceurPage pa;
		pa.libelle = NkString("Apprendre");
		pa.sousTitre = NkString("Composer une interface, de la page au composant.");
		pa.glyphe = G::Apprendre;
		pa.type = NkLanceurPageType::Liens;
		auto lien = [&](const char *t, const char *d, G g) {
			NkLanceurLien l;
			l.titre = NkString(t);
			l.description = NkString(d);
			l.glyphe = g;
			l.disponible = false;
			l.badge = NkString("a venir");
			pa.liens.PushBack(l);
		};
		lien("Premiers pas", "Pages, formes, groupes : la toile et ses outils.", G::Calques);
		lien("Composants du kit", "Les roles du format, les gabarits et les greffons.", G::Code);
		lien("Exporter", "PNG, SVG et documents .nkgui pour les applications.", G::Lien);
		m.pages.PushBack(pa);
		NkLanceurPage pi;
		pi.libelle = NkString("Installations");
		pi.sousTitre = NkString("Ce qui est installe sur cette machine.");
		pi.glyphe = G::Installations;
		pi.type = NkLanceurPageType::Liens;
		{
			NkLanceurLien l;
			l.titre = NkString("NKUIDesign 0.1.0");
			l.description = NkString("Installe dans ") + NkPath::GetExecutableDirectory().ToString();
			l.glyphe = G::Calques;
			l.badge = NkString("installe");
			pi.liens.PushBack(l);
		}
		m.pages.PushBack(pi);
		m.astuce = NkString("Astuce : Ctrl+N ouvre un document vierge dans un nouvel onglet.");
		m.piedDePage = NkString("Chaque document s'ouvre dans son onglet ; Ctrl+S ecrit le document actif.");
	}

	/// Les recents du lanceur, avec en tete le DOCUMENT DE TRAVAIL s'il existe
	/// (il est deja ouvert : `hote` = 0xFFFFFFFF le signale).
	inline void RafraichirLanceur(nkentseu::editorkit::NkProjectLauncherModel &m, const char *docTravail) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		EtatLanceur().recents.Remplir(m);
		if (docTravail && NkFile::Exists(docTravail)) {
			bool deja = false;
			for (usize i = 0; i < m.projets.Size(); ++i)
				deja = deja || (m.projets[i].chemin == NkString(docTravail));
			if (!deja) {
				NkLanceurProjet p;
				p.nom = NkString("Document de travail");
				p.chemin = NkString(docTravail);
				p.date = NkString("ouvert au demarrage");
				p.epingle = true;
				p.hote = 0xFFFFFFFFu;
				m.projets.Insert(m.projets.Begin(), p);
			}
		}
	}

	/// Le .nkuidoc dans son onglet (deja ouvert = on y bascule). Vrai si montre.
	inline bool LanceurOuvrirDocument(DesignState &d, const nkentseu::NkString &chemin, nkentseu::NkString *erreur) {
		using namespace nkentseu;
		if (chemin.Empty())
			return false;
		const NkString nom = NkPath(chemin.CStr()).GetFileNameWithoutExtension();
		auto montre = [&](const char *comment) {
			EtatLanceur().recents.Toucher(chemin, nom, nkentseu::editorkit::NkLanceurAujourdhui());
			std::printf("[LANCEUR] document %s : %s\n", comment, chemin.CStr());
			std::fflush(stdout);
			return true;
		};
		// Deja ouvert (le document de travail, les onglets de demonstration) :
		// on y BASCULE, on ne l'ouvre pas une seconde fois.
		if (d.cheminActif == chemin)
			return montre("deja actif");
		for (uint32 i = 0; i < (uint32)d.ouverts.Size(); ++i)
			if (d.ouverts[i].chemin == chemin)
				return d.BasculerVers(i) && montre("montre (onglet existant)");
		const NkString texte = NkFile::ReadAllText(chemin.CStr());
		NkUIDocument doc;
		if (texte.Empty() || !doc.Load(texte.Data())) {
			if (erreur)
				*erreur = NkString("Document illisible : ") + chemin;
			return false;
		}
		const int32 onglet = d.OuvrirOngletInactif(doc, chemin.CStr());
		if (onglet < 0)
			return false;
		(void)d.BasculerVers((uint32)onglet);
		return montre("ouvert");
	}

	/// Ce que fait chaque demande du lanceur. VRAI = un document est montre.
	inline bool AgirLanceur(DesignState &d, nkentseu::editorkit::NkLanceurCoquille &l,
							const nkentseu::editorkit::NkProjectLauncherResult &r) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		LanceurEtat &E = EtatLanceur();
		const int32 i = r.index;
		const bool projetValide = i >= 0 && (usize)i < l.modele.projets.Size();
		const uint32 hote = projetValide ? l.modele.projets[(usize)i].hote : 0xFFFFFFFEu;
		NkString err;
		switch (r.action) {
			case NkLanceurAction::NouveauProjet:
				d.NouvelOngletVierge();
				return true;
			case NkLanceurAction::NouveauDepuisModele: {
				if (i < 0 || (usize)i >= l.modele.modeles.Size() || !l.modele.modeles[(usize)i].disponible)
					return false;
				const NkString &c = E.gabarits[(usize)i];
				if (c.Empty()) {
					d.NouvelOngletVierge();
					return true;
				}
				if (LanceurOuvrirDocument(d, c, &err))
					return true;
				l.modele.erreur = err;
				return false;
			}
			case NkLanceurAction::Ouvrir: {
				const NkDialogResult dr = NkDialogs::OpenFileDialog("*.nkuidoc", "Ouvrir un document NKUIDesign");
				if (!dr.confirmed || dr.path.Empty())
					return false;
				if (LanceurOuvrirDocument(d, dr.path, &err))
					return true;
				l.modele.erreur = err;
				return false;
			}
			case NkLanceurAction::OuvrirRecent:
				if (!projetValide)
					return false;
				if (hote == 0xFFFFFFFFu)
					return true; // le document de travail : deja la, on le montre
				if (LanceurOuvrirDocument(d, l.modele.projets[(usize)i].chemin, &err))
					return true;
				l.modele.erreur = err;
				return false;
			case NkLanceurAction::Epingler:
				if (projetValide && hote != 0xFFFFFFFFu)
					E.recents.BasculerEpingle((usize)hote);
				return false;
			case NkLanceurAction::Retirer:
				if (projetValide && hote != 0xFFFFFFFFu)
					E.recents.Retirer((usize)hote);
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
	/// fenetre ni GPU, par le MEME composant et le MEME modele.
	inline int CapturerLanceur(const nkentseu::NkString &chemin, bool clair, const char *docTravail) {
		using namespace nkentseu;
		using namespace nkentseu::editorkit;
		static NkProjectLauncherModel m;
		RemplirLanceur(m);
		EtatLanceur().recents.Charger("NKUIDesign");
		RafraichirLanceur(m, docTravail);
		m.themeSombre = !clair;
		NkLanceurCaptureDesc d;
		d.chemin = chemin.CStr();
		char msg[512];
		const bool ok = NkLanceurCapturer(d, m, clair ? NkTheme::Light() : NkTheme::Dark(), NkProjectLauncherHooks(),
										  NkProjectLauncherStyle(), msg, (int32)sizeof(msg));
		std::printf("[capture-lanceur] NKUIDesign : %s\n", msg);
		std::fflush(stdout);
		return ok ? 0 : 1;
	}

} // namespace nkuidesign
