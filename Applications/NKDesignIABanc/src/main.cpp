// =============================================================================
// NKDesignIABanc — LE JEU D'EPREUVE DE L'IA DE DESIGN : trois taux, pas un avis.
//
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'IL MESURE, ET POURQUOI EN TROIS NIVEAUX SEPARES
// =============================================================================
//  « L'IA repond » ne veut rien dire. Le critere est « LE DOCUMENT S'OUVRE », et
//  il se decompose en trois etages qui echouent pour des raisons DIFFERENTES :
//
//    N1  le modele rend quelque chose   -> code 0 et sortie non vide
//    N2  ce quelque chose est un document -> `ValidateReply` : en-tete `nkuidoc`,
//        structure chargeable, composants du REGISTRE, et REJEU identique
//    N3  le document s'ouvre            -> greffe par la meme porte que la main,
//        mise en page, puis ecriture `.nkgui` (le pont existe depuis le 14/09)
//
//  Les melanger donnerait un taux unique qui ne dit pas quoi reparer. Un modele
//  qui parle et n'ecrit pas le format, un modele qui ecrit le format et invente
//  un composant, un document juste que le pont ne sait pas traduire : trois
//  maladies, trois remedes.
//
//  ⚠️ LE MONTAGE N'EST PAS FAIT ICI, ET C'EST VOULU. Le `.nkgui` produit est
//     donne a `NKGuiMonteTest --monter=<fichier>`, un AUTRE binaire, qui le lit,
//     le valide et le MONTE en widgets reels. Deux instruments sans code commun :
//     si les deux s'accordent, ce n'est pas parce qu'ils partagent un bogue.
//
// =============================================================================
//  CE QU'IL NE FAIT PAS
// =============================================================================
//  - il n'ouvre AUCUNE fenetre et ne touche pas au GPU : le modele vit dans SON
//    processus (contrat a deux trous `{invite}` / `{sortie}`), et ce banc ne fait
//    que l'appeler ;
//  - il ne juge pas la BEAUTE. Le rejeu verifie la fidelite, pas le gout. Un
//    ecran bien forme et laid passe les trois niveaux ;
//  - il n'ecrit rien dans le document de travail de Rodolf : chaque demande part
//    d'un document NEUF.
//
// ⚠️ LE CATALOGUE EST PEUPLE ICI PAR LES MEMES TROIS SOURCES que
//    `PanneauToile::PeuplerRegistre` (Panels.h) : la table des basiques, le
//    navigateur de contenu, l'arbre. C'est une SECONDE ECRITURE de la meme
//    regle, et je le declare : la condition de reouverture est « le jour ou un
//    composant apparait dans l'un et pas dans l'autre ». La porte propre serait
//    que `PeuplerRegistre` descende hors d'un en-tete de panneaux ; ce n'est pas
//    mon territoire aujourd'hui, et un banc qui n'a pas le meme catalogue que
//    l'application mesurerait une autre application.
// =============================================================================

// ⚠️ `using namespace nkentseu;` AVANT LES EN-TETES DE NKUIDesign, ET CE N'EST PAS
//    UN CONFORT : `NkDocStringPool.h` (et, a sa suite, `Document.h`) ecrivent
//    `NkString` et `nk_size` SANS QUALIFICATION a l'interieur de `namespace
//    nkuidesign`. Dans `NKUIDesign/main.cpp` cela compile parce qu'un en-tete
//    inclus plus tot a deja deverse `using namespace nkentseu` au niveau global.
//    Ici, premier consommateur a ne pas l'avoir par hasard, la construction s'est
//    arretee sur neuf erreurs -- exactement « un en-tete partage inclut ce qu'il
//    utilise », mesure une fois de plus. Je le contourne au lieu de refactorer les
//    en-tetes d'un autre chantier ; CONDITION DE RETRAIT de ces deux lignes : le
//    jour ou `NkDocStringPool.h` qualifie ses types.
#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
using namespace nkentseu;

#include "NKEditorKit/Components/NkContentBrowserModel.h"
#include "NKEditorKit/Components/NkTreeViewModel.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKTime/NkChrono.h"
#include "NKUIDesign/ComposantsBase.h"
#include "NKUIDesign/DesignAI.h"
#include "NKUIDesign/Document.h"
#include "NKUIDesign/Layout.h"
#include "NKUIDesign/NkGuiEcrire.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nkuidesign;

namespace {

	// ── OUTILS DE CHEMIN ET DE CHAINE (zero-STL) ─────────────────────────────
	void Joindre(char *dst, usize cap, const char *a, const char *b) {
		usize n = 0;
		for (; a && a[n] && n + 1 < cap; ++n)
			dst[n] = a[n];
		for (usize i = 0; b && b[i] && n + 1 < cap; ++i, ++n)
			dst[n] = b[i];
		dst[n] = 0;
	}

	bool CommencePar(const char *s, const char *p) {
		usize i = 0;
		for (; p[i]; ++i)
			if (s[i] != p[i])
				return false;
		return true;
	}

	/// Le registre, peuple comme l'application le peuple. Voir l'avertissement
	/// de l'en-tete : deux ecritures de la meme regle, declarees comme telles.
	void PeuplerCatalogue() {
		uint32 nbBase = 0;
		(void)basiques::NkTableBasiques(nbBase);
		for (uint32 i = 0; i < nbBase; ++i)
			nkentseu::editorkit::NkComponentRegistry::Register(basiques::NkDeclBasique(i));
		nkentseu::editorkit::NkComponentRegistry::Register(nkentseu::editorkit::NkContentBrowserDecl());
		nkentseu::editorkit::NkComponentRegistry::Register(nkentseu::editorkit::NkTreeViewDecl());
	}

	struct Ligne {
			char id[16] = {0};
			char texte[512] = {0};
	};

	/// Lit `demandes.txt` : les lignes vides et celles qui commencent par '#'
	/// sont ignorees ; le reste s'ecrit « id | phrase ».
	uint32 LireDemandes(const char *chemin, Ligne *out, uint32 cap) {
		const NkString txt = NkFile::ReadAllText(NkPath(chemin));
		if (txt.Empty())
			return 0;
		uint32 nb = 0;
		const char *p = txt.CStr();
		while (*p && nb < cap) {
			const char *fin = p;
			while (*fin && *fin != '\n')
				++fin;
			// La ligne brute, sans le retour chariot de Windows.
			char brut[640];
			usize n = 0;
			for (const char *q = p; q < fin && n + 1 < sizeof(brut); ++q)
				if (*q != '\r')
					brut[n++] = *q;
			brut[n] = 0;
			p = *fin ? fin + 1 : fin;
			if (n == 0 || brut[0] == '#')
				continue;
			// couper sur la barre verticale
			char *bar = nullptr;
			for (usize i = 0; i < n; ++i)
				if (brut[i] == '|') {
					bar = brut + i;
					break;
				}
			if (!bar)
				continue;
			*bar = 0;
			// l'identifiant : sans espaces
			usize k = 0;
			for (usize i = 0; brut[i] && k + 1 < sizeof(out[nb].id); ++i)
				if (brut[i] != ' ' && brut[i] != '\t')
					out[nb].id[k++] = brut[i];
			out[nb].id[k] = 0;
			// la phrase : sans l'espace de tete
			const char *ph = bar + 1;
			while (*ph == ' ')
				++ph;
			usize m = 0;
			for (; ph[m] && m + 1 < sizeof(out[nb].texte); ++m)
				out[nb].texte[m] = ph[m];
			out[nb].texte[m] = 0;
			if (out[nb].id[0] && out[nb].texte[0])
				++nb;
		}
		return nb;
	}

	/// Un dorsal de banc : il rend un texte FIXE, sans modele et sans processus.
	/// C'est LE ZERO du banc — il prouve que les trois niveaux savent dire OUI
	/// quand la reponse est bonne, donc qu'un 0/12 mesure le modele et non le
	/// banc. Sans lui, un banc qui refuse tout serait indiscernable d'un banc
	/// qui marche face a un modele faible.
	class DorsalTemoin final : public NkIDesignBackend {
		public:
			bool Complete(const NkDesignRequest &, NkDesignReply &out) override {
				out.text = NkString("nkuidoc 1\n"
									"titre = Temoin\n"
									"noeud 0\n"
									"  libelle = Ecran\n"
									"  agencement = column\n"
									"  ecart = 8\n"
									"  marge = 12\n"
									"  enfants = 1 2\n"
									"noeud 1\n"
									"  libelle = Titre\n"
									"  composant = etiquette\n"
									"noeud 2\n"
									"  libelle = Valider\n"
									"  composant = bouton\n");
				out.success = true;
				return true;
			}
			bool IsAvailable() const override {
				return true;
			}
			const char *Name() const override {
				return "temoin";
			}
	};

	struct Bilan {
			uint32 n1 = 0, n2 = 0, n3 = 0, total = 0;
	};

} // namespace

int main(int argc, char **argv) {
	const char *fDemandes = "Applications/NKUIDesign/exemples/ia/demandes.txt";
	const char *dSortie = "Build/ia-jeu";
	const char *dorsal = "processus";
	const char *seul = nullptr; // --seule=d01 : une seule demande
	for (int a = 1; a < argc; ++a) {
		if (CommencePar(argv[a], "--demandes="))
			fDemandes = argv[a] + 11;
		else if (CommencePar(argv[a], "--sortie="))
			dSortie = argv[a] + 9;
		else if (CommencePar(argv[a], "--dorsal="))
			dorsal = argv[a] + 9;
		else if (CommencePar(argv[a], "--seule="))
			seul = argv[a] + 8;
		else if (std::strcmp(argv[a], "--aide") == 0) {
			std::printf("NKDesignIABanc --demandes=<f> --sortie=<dossier> "
						"[--dorsal=processus|temoin] [--seule=<id>]\n"
						"  processus : le gabarit de NK_DESIGN_CMD / NK_DESIGN_EXE "
						"(NKDesignLLM)\n"
						"  temoin    : LE ZERO du banc -- une reponse fixe et juste\n");
			return 0;
		}
	}

	PeuplerCatalogue();
	NkDirectory::CreateRecursive(dSortie);

	Ligne demandes[64];
	const uint32 nbD = LireDemandes(fDemandes, demandes, 64);
	if (nbD == 0) {
		std::printf("AUCUNE DEMANDE LUE dans %s -- rien n'a ete mesure.\n", fDemandes);
		return 2;
	}

	// ── LE DORSAL ────────────────────────────────────────────────────────────
	DorsalTemoin temoin;
	NkDesignAI ia;
	if (std::strcmp(dorsal, "temoin") == 0) {
		ia.SetBackend(&temoin);
	} else {
		NkDesignBackendProcessus &proc = NkDesignBackendProcessus::ParDefaut();
		char inv[512], sor[512];
		Joindre(inv, sizeof(inv), dSortie, "/invite_courante.txt");
		Joindre(sor, sizeof(sor), dSortie, "/sortie_courante.txt");
		proc.invitePath = NkString(inv);
		proc.sortiePath = NkString(sor);
		if (!proc.IsAvailable()) {
			std::printf("AUCUN GABARIT : pose NK_DESIGN_EXE (chemin de NKDesignLLM.exe) "
						"ou NK_DESIGN_CMD. Rien n'a ete mesure.\n");
			return 2;
		}
		ia.SetBackend(&proc);
	}

	NkString catalogue;
	NkDesignAI::BuildCatalog(catalogue);
	uint32 nbComposants = 0;
	for (uint32 i = 0; i < (uint32)catalogue.Size(); ++i)
		if (catalogue.Data()[i] == '\n' && i + 10 < (uint32)catalogue.Size()
			&& CommencePar(catalogue.Data() + i + 1, "composant "))
			++nbComposants;
	if (catalogue.Size() > 0 && CommencePar(catalogue.Data(), "composant "))
		++nbComposants;

	std::printf("=== JEU D'EPREUVE DE L'IA DE DESIGN ===\n");
	std::printf("dorsal        : %s\n", ia.Backend()->Name());
	std::printf("demandes      : %u (lues dans %s)\n", nbD, fDemandes);
	std::printf("catalogue     : %u composant(s) declares au registre\n", nbComposants);
	std::printf("sortie        : %s\n\n", dSortie);

	Bilan b;
	for (uint32 i = 0; i < nbD; ++i) {
		const Ligne &d = demandes[i];
		if (seul && std::strcmp(seul, d.id) != 0)
			continue;
		++b.total;

		// L'INVITE EXACTE, celle que l'application enverrait — batie par le meme
		// code, jamais recopiee ici.
		NkString invite;
		ia.BatirInviteComplete(d.texte, invite);
		char cheminInv[512];
		char nomInv[64];
		Joindre(nomInv, sizeof(nomInv), "/", d.id);
		char nomInv2[80];
		Joindre(nomInv2, sizeof(nomInv2), nomInv, "_invite.txt");
		Joindre(cheminInv, sizeof(cheminInv), dSortie, nomInv2);
		{
			NkString pleine(invite);
			pleine.Append("\n--- composants declares ---\n");
			pleine.Append(catalogue);
			NkFile::WriteAllText(NkPath(cheminInv), pleine);
		}

		// ── N1 : le dorsal rend-il quelque chose ? ───────────────────────────
		NkUIDocument doc;
		doc.NewDocument("Jeu", NkAuthor::Humain);
		NkChrono chrono;
		chrono.Reset();
		const NkAIResult res = ia.Ask(d.texte, doc, 0);
		const float64 msEcoule = chrono.Elapsed().ToMilliseconds();
		const NkString &brut = ia.LastReply();
		char cheminRep[512], nomRep[80];
		Joindre(nomRep, sizeof(nomRep), nomInv, "_reponse.txt");
		Joindre(cheminRep, sizeof(cheminRep), dSortie, nomRep);
		if (brut.Size() > 0)
			NkFile::WriteAllText(NkPath(cheminRep), brut);

		const bool n1 = brut.Size() > 0;
		if (n1)
			++b.n1;
		const bool n2 = res.Accepted();
		if (n2)
			++b.n2;

		// ── N3 : le document s'ouvre-t-il ? ──────────────────────────────────
		// Greffe faite (Ask pose deja), mise en page, puis `.nkgui` sur le
		// disque. Le MONTAGE est l'affaire de l'autre binaire.
		bool n3 = false;
		uint32 lignesNkgui = 0;
		uint32 rolesDuComposant = 0;
		uint32 composantsSansRole = 0;
		if (n2) {
			NkPaintRect sfc = {0.f, 0.f, 1200.f, 800.f};
			NkLayoutResult lay;
			NkComputeLayout(doc, sfc, lay);
			char cheminG[512], nomG[80];
			Joindre(nomG, sizeof(nomG), nomInv, ".nkgui");
			Joindre(cheminG, sizeof(cheminG), dSortie, nomG);
			guifmt::NkEcritRapport rap;
			n3 = guifmt::NkEcrireNkgui(doc, lay, cheminG, rap);
			rolesDuComposant = rap.rolesDuComposant;
			composantsSansRole = rap.composantsSansRole;
			if (n3) {
				const NkString t = NkFile::ReadAllText(NkPath(cheminG));
				for (uint32 k = 0; k < (uint32)t.Size(); ++k)
					if (t.Data()[k] == '\n')
						++lignesNkgui;
			}
		}
		if (n3)
			++b.n3;

		std::printf("%-4s N1 %s (%u o, %.0f ms) | N2 %-42s | N3 %s", d.id, n1 ? "oui" : "NON",
					(uint32)brut.Size(), msEcoule, NkAIVerdictName(res.verdict),
					n3 ? "oui" : (n2 ? "NON" : "-"));
		// ⚠️ LE REFUS SE PUBLIE A COTE DU SUCCES. Un composant du registre sans
		//    equivalent dans le vocabulaire `.nkgui` (les composites du kit) sort
		//    en CONTENEUR : le document se monte et ne montre pas ce qui a ete
		//    demande. Taire ce compte rendrait N3 vert sur un document creux --
		//    exactement le defaut que la course du temoin a trouve le 18/09.
		if (n3) {
			std::printf(" (%u noeuds, %u lignes de .nkgui, %u role(s) tire(s) du composant",
						res.nodesAdded, lignesNkgui, rolesDuComposant);
			if (composantsSansRole > 0)
				std::printf(", %u composant(s) SANS ROLE", composantsSansRole);
			std::printf(")");
		}
		if (!n2 && res.detail.Size() > 0)
			std::printf("  [%s]", res.detail.CStr());
		std::printf("\n");
		std::fflush(stdout);
	}

	std::printf("\n=== TAUX, sur %u demande(s) ===\n", b.total);
	std::printf("  N1  le modele rend quelque chose : %u / %u\n", b.n1, b.total);
	std::printf("  N2  le document se LIT           : %u / %u\n", b.n2, b.total);
	std::printf("  N3  le document S'OUVRE (.nkgui) : %u / %u\n", b.n3, b.total);
	std::printf("\nLe MONTAGE se mesure a part : NKGuiMonteTest --monter=<fichier.nkgui>\n");
	return 0;
}

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
