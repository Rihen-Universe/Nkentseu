#pragma once
// -----------------------------------------------------------------------------
// @File    SondeLangues.h
// @Brief   `--sonde-langues` : un document traduit-il ses libelles, et la bascule
//          a chaud laisse-t-elle les identites tranquilles ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  CE QU'ELLE PROUVE, ET CE QU'ELLE NE PROUVE PAS
// =============================================================================
//  Rodolf, 27/09 : le mecanisme multilingue descend de NKCode vers NKGui, et
//  NKUIDesign en est le premier utilisateur.
//
//  🔴 C5 EST LE CRITERE QUI JUSTIFIE QUE LE MECANISME VIVE DANS NKGui. Rodolf
//     l'a vu avant moi : « ça vit à la racine et ça pourrait trouver une parade
//     aux identifiants qui changent à chaud. » La parade, c'est que NKGui -- et
//     lui seul -- sait qu'un libelle est une traduction, donc peut rendre la
//     partie qui NE BOUGE PAS : la cle. C5 mesure exactement ca.
//
//  ⚠️ ET ELLE NE PROUVE PAS QUE LE DEFAUT EST CORRIGE. `NkGuiFormeStable` rend
//     la bonne chaine, mais les six sites du monteur ne s'en servent pas encore :
//     `TreeNode(ctx, label)` fabrique son identite EN INTERNE a partir du
//     libelle, et il n'existe pas d'`TreeNodeEx(ctx, id, label)` pour l'en
//     empecher. Poser la forme stable sur `SetNodeOpen` seul donnerait DEUX
//     identites en desaccord -- pire que l'instabilite actuelle, qui est au moins
//     coherente. Le chantier reste ouvert, et cette sonde ne pretend pas le
//     fermer.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDELANGUES_H__
#define __NKENTSEU_NKUIDESIGN_SONDELANGUES_H__

#include "NKGui/Doc/NkGuiLangues.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"
#include "NkCoquilleDocument.h"  // C7 : le VRAI document de NKUIDesign
#include "NkUIDesignLangues.h"	 // C7 : et LA table de cette application

namespace nkuidesign {

	namespace sondelang {

		using nkentseu::NkArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::int32;
		using nkentseu::uint32;
		using namespace nkentseu::nkgui;

		/// La table de NKUIDesign. Colonnes : fr, en, es, pt, de, it, ru, gom --
		/// DANS CET ORDRE, celui de `NkGuiCodeLangue`.
		///
		/// ⚠️ `design.recharger` N'A QUE LE FRANCAIS ET L'ANGLAIS, exprès : c'est le
		///    cas qui exerce le repli sur l'anglais (C3). Une table entierement
		///    remplie ne l'exercerait jamais.
		/// ⚠️ ET `design.nouveau` N'EST PAS DANS LA TABLE DU TOUT, pour exercer le
		///    repli sur la CLE (C4).
		inline const NkGuiTraduction *NkLTable(int32 &n) {
			static const NkGuiTraduction T[] = {
				{"design.enregistrer",
				 {"Enregistrer", "Save", "Guardar", "Guardar", "Speichern", "Salva",
				  "Сохранить", "Nzé"}},
				{"design.recharger", {"Recharger", "Reload", "", "", "", "", "", ""}},
			};
			n = (int32)(sizeof(T) / sizeof(T[0]));
			return T;
		}

		inline const char *NkLDoc() {
			return "nkgui 0.4\n"
				   "\n"
				   "widgets \"langues\" {\n"
				   "  VBox \"col\" {\n"
				   "    Button \"design.enregistrer\"  { label = \"@t:design.enregistrer\" }\n"
				   "    Button \"design.recharger\"    { label = \"@t:design.recharger\" }\n"
				   "    Button \"design.nouveau\"      { label = \"@t:design.nouveau\" }\n"
				   "    Button \"design.brut\"         { label = \"Texte en dur\" }\n"
				   "  }\n"
				   "}\n";
		}

		/// Monte et rend le libelle releve pour l'identifiant demande.
		inline NkString NkLLibelle(const char *idVoulu) noexcept {
			NkArchive doc;
			nkentseu::NkGuiDiag diag;
			const char *src = NkLDoc();
			uint32 n = 0;
			while (src[n] != '\0') ++n;
			NkString rien;
			if (!NkGuiArchive::Read(src, n, doc, diag))
				return rien;
			NkGuiContext ctx;
			NkGuiMonteEtat etat;
			NkGuiMonteRapport rap;
			NkGuiMonteur::Preparer(doc, etat);
			ctx.viewW = 640;
			ctx.viewH = 480;
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(NkRect{0.f, 0.f, 640.f, 480.f});
			ctx.DL().Reset();
			NkGuiMonteur::Monter(ctx, doc, "langues", etat, rap, nullptr);
			ctx.EndFrame();
			// Le releve du monteur porte chaque widget monte, avec son identifiant.
			// On n'inspecte donc pas l'ecran : on lit ce qu'il dit avoir monte.
			for (uint32 i = 0; i < (uint32)rap.items.Size(); ++i)
				if (rap.items[i].id.Compare(idVoulu) == 0)
					return rap.items[i].id; // l'id, stable -- le libelle se lit autrement
			return rien;
		}

	} // namespace sondelang

	/// `--sonde-langues` : 0 si tout passe, 1 sinon.
	inline int SondeLangues() noexcept {
		using namespace sondelang;
		printf("=== SONDE LANGUES — `@t:cle` et la bascule a chaud ===\n");
		uint32 ko = 0u;

		NkGuiOublierTablesLangues();
		NkGuiOublierSurcharges();
		NkGuiLanguesRemiseAZero();
		int32 nT = 0;
		const NkGuiTraduction *T = NkLTable(nT);
		(void)NkGuiPoserTableLangues(T, nT);

		// ── C1 : LA MEME CLE REND DEUX TEXTES DIFFERENTS ───────────────────
		{
			NkGuiPoserLangue(0); // francais
			const NkString fr(NkGuiTexteLangue("design.enregistrer"));
			NkGuiPoserLangue(1); // anglais
			const NkString en(NkGuiTexteLangue("design.enregistrer"));
			NkGuiPoserLangue(7); // Ghɔmáláʼ
			const NkString gom(NkGuiTexteLangue("design.enregistrer"));
			const bool ok = fr.Compare("Enregistrer") == 0 && en.Compare("Save") == 0
							&& gom.Compare("Nzé") == 0;
			if (!ok) ++ko;
			printf("  C1 traduction      fr=\"%s\" en=\"%s\" gom=\"%s\"  %s\n", fr.CStr(),
				   en.CStr(), gom.CStr(), ok ? "OK" : "<<< KO");
		}

		// ── C2 : LE DOCUMENT EST TRADUIT PAR LE MONTEUR ────────────────────
		{
			NkArchive doc;
			nkentseu::NkGuiDiag diag;
			const char *src = NkLDoc();
			uint32 n = 0;
			while (src[n] != '\0') ++n;
			NkString vuFr, vuEn;
			if (NkGuiArchive::Read(src, n, doc, diag)) {
				const NkArchiveNode *corps = doc.FindNode(NkStringView(NkGuiArchive::KeyBody()));
				// On relit le libelle PAR LA MEME PORTE que le monteur (`NkGTexte`),
				// ce qui est le point : si cette porte ne traduit pas, aucun document
				// ne sera traduit, quoi que la table contienne.
				const NkArchive *bouton = nullptr;
				if (corps && corps->array.Size() > 0 && corps->array[0].IsObject()) {
					const NkArchiveNode *cw =
						corps->array[0].object->FindNode(NkStringView(NkGuiArchive::KeyBody()));
					if (cw && cw->array.Size() > 0 && cw->array[0].IsObject()) {
						const NkArchiveNode *cv = cw->array[0].object->FindNode(
							NkStringView(NkGuiArchive::KeyBody()));
						if (cv && cv->array.Size() > 0 && cv->array[0].IsObject())
							bouton = cv->array[0].object;
					}
				}
				if (bouton) {
					NkGuiPoserLangue(0);
					vuFr = NkGTexte(*bouton, "label", "");
					NkGuiPoserLangue(1);
					vuEn = NkGTexte(*bouton, "label", "");
				}
			}
			const bool ok = vuFr.Compare("Enregistrer") == 0 && vuEn.Compare("Save") == 0;
			if (!ok) ++ko;
			printf("  C2 document        label fr=\"%s\" en=\"%s\"  %s\n", vuFr.CStr(),
				   vuEn.CStr(), ok ? "OK" : "<<< KO");
		}

		// ── C3 : LE REPLI SUR L'ANGLAIS ────────────────────────────────────
		{
			NkGuiPoserLangue(2); // espagnol, colonne VIDE pour cette cle
			const NkString es(NkGuiTexteLangue("design.recharger"));
			const bool ok = es.Compare("Reload") == 0;
			if (!ok) ++ko;
			printf("  C3 repli anglais   es=\"%s\" (colonne vide -> anglais)  %s\n", es.CStr(),
				   ok ? "OK" : "<<< KO");
		}

		// ── C4 : LE REPLI SUR LA CLE, ET IL SE COMPTE ──────────────────────
		{
			NkGuiLanguesRemiseAZero();
			NkGuiPoserLangue(0);
			const NkString abs(NkGuiTexteLangue("design.nouveau"));
			const NkGuiLanguesRapport &r = NkGuiLanguesReleve();
			const bool ok = abs.Compare("design.nouveau") == 0 && r.repliCle == 1u
							&& r.derniereSansTraduction.Compare("design.nouveau") == 0;
			if (!ok) ++ko;
			printf("  C4 repli cle       \"%s\", repliCle=%u, derniere=\"%s\"  %s\n", abs.CStr(),
				   r.repliCle, r.derniereSansTraduction.CStr(), ok ? "OK" : "<<< KO");
			printf("       ^ laid, VISIBLE, corrigeable — une chaine vide aurait donne un\n");
			printf("         bouton sans texte, que personne ne signale.\n");
		}

		// ── C5 : 🔴 LA FORME STABLE NE BOUGE PAS AVEC LA LANGUE ────────────
		{
			const NkString brut("@t:design.enregistrer");
			NkGuiPoserLangue(0);
			const NkString s0 = NkGuiFormeStable(brut);
			const NkString t0(NkGuiTexteLangue("design.enregistrer"));
			NkGuiPoserLangue(7);
			const NkString s7 = NkGuiFormeStable(brut);
			const NkString t7(NkGuiTexteLangue("design.enregistrer"));
			// LE TEXTE change, LA FORME STABLE ne change pas. Les deux moities du
			// critere comptent : si le texte ne changeait pas non plus, la stabilite
			// serait vraie pour une mauvaise raison.
			const bool texteChange = t0.Compare(t7) != 0;
			const bool formeStable = s0.Compare(s7) == 0
									 && s0.Compare("design.enregistrer") == 0;
			const bool ok = texteChange && formeStable;
			if (!ok) ++ko;
			printf("  C5 forme stable    texte \"%s\" -> \"%s\" (change) | forme \"%s\" -> "
				   "\"%s\" (fixe)  %s\n",
				   t0.CStr(), t7.CStr(), s0.CStr(), s7.CStr(), ok ? "OK" : "<<< KO");
		}

		// ── C6 : L'ANALYSE D'UN TAMPON `cle=valeur` ────────────────────────
		{
			// L'hote OUVRE, NKGui ANALYSE. On lui donne des octets, pas un chemin.
			static const char kLang[] = "# un commentaire\n"
										"\n"
										"design.enregistrer = Sauver\r\n"
										"// une autre forme de commentaire\n"
										"  design.nouveau=Nouveau document  \n"
										"ligne sans egal\n"
										"design.equation = a=b+c\n";
			NkGuiOublierSurcharges();
			const uint32 n = NkGuiChargerSurcharges(kLang, (uint32)(sizeof(kLang) - 1u), 0);
			NkGuiPoserLangue(0);
			const NkString a(NkGuiTexteLangue("design.enregistrer"));
			const NkString b(NkGuiTexteLangue("design.nouveau"));
			const NkString c(NkGuiTexteLangue("design.equation"));
			// La surcharge bat la table ; l'espace INTERIEUR d'une valeur est garde ;
			// le premier `=` seul coupe (sinon `a=b+c` serait perdu).
			const bool ok = n == 3u && a.Compare("Sauver") == 0 && b.Compare("Nouveau document") == 0
							&& c.Compare("a=b+c") == 0;
			if (!ok) ++ko;
			printf("  C6 analyse .lang   %u paires | \"%s\" | \"%s\" | \"%s\"  %s\n", n, a.CStr(),
				   b.CStr(), c.CStr(), ok ? "OK" : "<<< KO");
		}

		// ── C7 : 🔴 LE VRAI DOCUMENT, ET AUCUNE CLE ORPHELINE ──────────────
		{
			// C1..C6 eprouvent le MECANISME sur une table de sonde. C7 eprouve
			// l'APPLICATION : chaque `@t:` ecrit dans `Interface.nkgui` a-t-il une
			// entree dans la table de NKUIDesign ?
			//
			// Sans lui, une cle oubliee retomberait sur elle-meme -- le bouton
			// afficherait `design.exporterPoints` au lieu de « Exporter... ». Visible,
			// oui, mais seulement par quelqu'un qui regarde CE bouton-la, dans CETTE
			// langue. Le compteur, lui, les voit toutes d'un coup.
			NkGuiOublierSurcharges();
			NkGuiOublierTablesLangues();
			NkUIDesignPoserLangues();
			NkGuiLanguesRemiseAZero();
			NkGuiPoserLangue(0);

			NkCoquilleDocument coq;
			const bool lu = coq.ChargerDepuisDossier("Resources/Interface/NKUIDesign");
			uint32 montes = 0u;
			if (lu) {
				uint32 nR = 0u;
				const char *const *racines = NkCoquilleDocument::NomsRacines(nR);
				for (uint32 i = 0; i < nR; ++i) {
					NkGuiContext ctx;
					ctx.viewW = 1280;
					ctx.viewH = 200;
					ctx.BeginFrame(0.016f);
					ctx.BeginLayout(NkRect{0.f, 0.f, 1280.f, 200.f});
					ctx.DL().Reset();
					coq.bande.Monter(ctx, racines[i]);
					ctx.EndFrame();
					montes += coq.bande.rap.montes;
				}
			}
			const NkGuiLanguesRapport &r = NkGuiLanguesReleve();
			const bool ok = lu && r.demandes > 0u && r.repliCle == 0u;
			if (!ok) ++ko;
			printf("  C7 vrai document   lu=%s | demandes=%u servies=%u repliCle=%u  %s\n",
				   lu ? "oui" : "NON", r.demandes, r.serviesParTable, r.repliCle,
				   ok ? "OK" : "<<< KO");
			if (r.repliCle > 0u)
				printf("       ^ cle SANS traduction, la derniere : \"%s\"\n",
					   r.derniereSansTraduction.CStr());
		}

		NkGuiOublierSurcharges();
		NkGuiOublierTablesLangues();
		NkGuiPoserLangue(0);
		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDELANGUES_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
