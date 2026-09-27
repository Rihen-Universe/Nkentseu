#pragma once
// -----------------------------------------------------------------------------
// @File    SondeImages.h
// @Brief   `--sonde-images` : une image demandee par un document arrive-t-elle
//          vraiment dans la draw-list, et AUX BONNES PROPORTIONS ?
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  SANS FENETRE ET SANS GPU, ET C'EST POSSIBLE PARCE QUE LA DRAW-LIST EST DU CPU
// =============================================================================
//  `NkGuiDrawList::cmds` et `::vtx` sont publics. On peut donc verifier qu'une
//  image a ete EMISE -- une commande portant un `texId` de l'espace des images --
//  et lire le rectangle de ses sommets, sans jamais televerser un octet.
//
//  Le chargeur est remplace par un FAUX qui rend une taille et ne touche a rien.
//  C'est ce qui permet a cette sonde de tourner sur la machine d'integration.
//
// =============================================================================
//  LE CRITERE QUI COMPTE N'EST PAS LE COMPTE
// =============================================================================
//  🔴 C4, LES PROPORTIONS, est le seul qui attrape le defaut le plus probable.
//     Etirer une image pour remplir son rectangle ne fait pas planter, ne change
//     aucun compteur, et passe tous les bancs : ca se voit a l'oeil sur une
//     photo, et pas du tout sur un carre. Le faux chargeur rend donc du
//     **200 x 100** place dans une tuile **96 x 96** : le rectangle dessine doit
//     faire **96 x 48**. Un `AddImage(tex, r, ...)` naif -- celui qu'on ecrit en
//     premier -- rendrait 96 x 96 et passerait C1, C2 et C3.
//
//  ⚠️ ET LE DOCUMENT EST EN DUR DANS CE FICHIER. L'ecrire dans
//     `exemples/` l'aurait fait ramasser par l'aller-retour, qui balaie ce
//     dossier : le corpus passerait de 33 a 34 et tous les chiffres deja
//     publies deviendraient faux. Ce depot a deja paye exactement ca ce matin --
//     un vidage ecrit en `.nkgui` dans le dossier qu'il analysait, corpus 46 -> 47.
// -----------------------------------------------------------------------------

#ifndef __NKENTSEU_NKUIDESIGN_SONDEIMAGES_H__
#define __NKENTSEU_NKUIDESIGN_SONDEIMAGES_H__

#include "NKGui/Doc/NkGuiImages.h"
#include "NKGui/Doc/NkGuiMonteur.h"
#include "NKSerialization/NkGui/NkGuiArchive.h"

namespace nkuidesign {

	namespace sondeimg {

		using nkentseu::NkArchive;
		using nkentseu::NkString;
		using nkentseu::NkStringView;
		using nkentseu::float32;
		using nkentseu::uint32;
		using namespace nkentseu::nkgui;

		/// Le document de mesure. Trois images : deux noms distincts et un REPETE,
		/// pour que le cache ait quelque chose a prouver.
		inline const char *NkIDocument() {
			return "nkgui 0.4\n"
				   "\n"
				   "widgets \"images\" {\n"
				   "  VBox \"col\" {\n"
				   "    Tile \"t.un\"          { label = \"Un\",   image = \"a.png\", size = (96, 96) }\n"
				   "    Tile \"t.deux\"        { label = \"Deux\",  image = \"b.png\", size = (96, 96) }\n"
				   "    Tile \"t.trois\"       { label = \"Trois\", image = \"a.png\", size = (96, 96) }\n"
				   "  }\n"
				   "}\n";
		}

		/// Combien de fois le faux chargeur a ete appele. C'est LUI qui prouve le
		/// cache -- `duCache` est un compteur du registre, et un registre qui se
		/// declare son propre cache ne prouve rien. Deux compteurs, aucun code commun.
		inline uint32 &NkIAppels() {
			static uint32 n = 0;
			return n;
		}

		/// 200 x 100 : DELIBEREMENT pas carre. Voir C4 dans l'en-tete.
		inline bool NkIFauxChargeur(const char *nom, uint32 texId, NkVec2 &taille,
									void *user) noexcept {
			(void)nom;
			(void)texId;
			(void)user;
			++NkIAppels();
			taille = NkVec2(200.f, 100.f);
			return true;
		}

		/// Le rectangle couvert par une commande, lu dans ses sommets.
		inline bool NkIRectDeCmd(const NkGuiDrawList &dl, const NkGuiDrawCmd &c,
								 NkRect &out) noexcept {
			if (c.idxCount == 0u || c.idxOffset + c.idxCount > (uint32)dl.idx.Size())
				return false;
			float32 x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
			for (uint32 i = 0; i < c.idxCount; ++i) {
				const uint32 vi = dl.idx[c.idxOffset + i];
				if (vi >= (uint32)dl.vtx.Size())
					return false;
				const NkVec2 p = dl.vtx[vi].pos;
				if (p.x < x0) x0 = p.x;
				if (p.y < y0) y0 = p.y;
				if (p.x > x1) x1 = p.x;
				if (p.y > y1) y1 = p.y;
			}
			out = NkRect{x0, y0, x1 - x0, y1 - y0};
			return true;
		}

		/// Monte le document et rend le rapport. La draw-list reste accessible.
		inline void NkIMonter(NkGuiContext &ctx, NkGuiMonteRapport &rap) noexcept {
			NkArchive doc;
			nkentseu::NkGuiDiag diag;
			const char *src = NkIDocument();
			uint32 n = 0;
			while (src[n] != '\0') ++n;
			if (!NkGuiArchive::Read(src, n, doc, diag))
				return;
			ctx.viewW = 640;
			ctx.viewH = 480;
			ctx.BeginFrame(0.016f);
			ctx.BeginLayout(NkRect{0.f, 0.f, 640.f, 480.f});
			ctx.DL().Reset();
			NkGuiMonteEtat etat;
			NkGuiMonteur::Monter(ctx, doc, "images", etat, rap, nullptr);
			ctx.EndFrame();
		}

	} // namespace sondeimg

	/// `--sonde-images` : 0 si tout passe, 1 sinon.
	inline int SondeImages() noexcept {
		using namespace sondeimg;
		printf("=== SONDE IMAGES — un `image:` d'un document arrive-t-il dessine ? ===\n");
		uint32 ko = 0u;

		// ── C1 : SANS CHARGEUR, RIEN N'EST PEINT ET TOUT EST COMPTE ────────
		// L'ordre compte : ce cas doit passer AVANT qu'un chargeur soit pose,
		// sinon le cache garderait des entrees et le cas ne serait plus « sans
		// chargeur » mais « deja charge ».
		NkGuiImagesOublier();
		{
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			NkIMonter(ctx, rap);
			const NkGuiImagesRapport &r = NkGuiImagesReleve();
			const bool ok = (rap.imagesPeintes == 0u) && (rap.imagesNonResolues == 3u)
							&& (r.sansChargeur == 3u);
			if (!ok) ++ko;
			printf("  C1 sans chargeur   peintes=%u nonResolues=%u sansChargeur=%u  %s\n",
				   rap.imagesPeintes, rap.imagesNonResolues, r.sansChargeur,
				   ok ? "OK" : "<<< KO");
			// Le MARQUEUR : sans lui, une image manquante serait un trou muet.
			uint32 traits = 0;
			for (uint32 i = 0; i < (uint32)ctx.DL().cmds.Size(); ++i)
				if (ctx.DL().cmds[i].texId == 0u) ++traits;
			const bool okM = traits > 0u;
			if (!okM) ++ko;
			printf("  C1b marqueur       commandes non texturees=%u  %s\n", traits,
				   okM ? "OK" : "<<< KO");
		}

		// ── C2 et C3 : AVEC UN FAUX CHARGEUR ───────────────────────────────
		NkGuiImagesOublier();
		NkIAppels() = 0u;
		NkGuiPoserChargeurImage(&NkIFauxChargeur, nullptr);
		NkRect rImage{0.f, 0.f, 0.f, 0.f};
		{
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			NkIMonter(ctx, rap);
			uint32 cmdsImage = 0u, idsDistincts = 0u;
			uint32 vus[8] = {0};
			for (uint32 i = 0; i < (uint32)ctx.DL().cmds.Size(); ++i) {
				const NkGuiDrawCmd &c = ctx.DL().cmds[i];
				if (c.texId < kNkGuiImageTexId0) continue;
				++cmdsImage;
				if (rImage.w <= 0.f) (void)NkIRectDeCmd(ctx.DL(), c, rImage);
				bool deja = false;
				for (uint32 k = 0; k < idsDistincts; ++k)
					if (vus[k] == c.texId) deja = true;
				if (!deja && idsDistincts < 8u) vus[idsDistincts++] = c.texId;
			}
			const bool c2 = (rap.imagesPeintes == 3u) && (cmdsImage == 3u);
			// DEUX noms distincts pour trois usages : le troisieme REUTILISE le
			// premier, donc deux numeros, pas trois.
			const bool c3 = (idsDistincts == 2u) && (NkIAppels() == 2u);
            if (!c2) ++ko;
			if (!c3) ++ko;
			printf("  C2 peintes         peintes=%u commandesImage=%u  %s\n", rap.imagesPeintes,
				   cmdsImage, c2 ? "OK" : "<<< KO");
			printf("  C3 cache           texIds distincts=%u appels du chargeur=%u  %s\n",
				   idsDistincts, NkIAppels(), c3 ? "OK" : "<<< KO");
		}

		// ── C4 : LES PROPORTIONS — le critere qui attrape l'etirement ──────
		{
			// 200x100 dans une tuile 96x96 doit donner 96x48.
			const bool c4 = rImage.w > 0.f && rImage.h > 0.f
							&& rImage.w > 95.f && rImage.w < 97.f
							&& rImage.h > 47.f && rImage.h < 49.f;
			if (!c4) ++ko;
			printf("  C4 proportions     rectangle dessine = %.1f x %.1f (attendu 96,0 x 48,0)  %s\n",
				   (double)rImage.w, (double)rImage.h, c4 ? "OK" : "<<< KO");
			if (!c4)
				printf("       ^ une image ETIREE rendrait 96,0 x 96,0 et passerait C1..C3.\n");
		}

		// ── C5 : UN FICHIER QUI N'EXISTE PAS EST NOMME ─────────────────────
		NkGuiImagesOublier();
		NkGuiPoserChargeurImage(
			[](const char *, uint32, NkVec2 &, void *) noexcept { return false; },
			nullptr);
		{
			NkGuiContext ctx;
			NkGuiMonteRapport rap;
			NkIMonter(ctx, rap);
			const NkGuiImagesRapport &r = NkGuiImagesReleve();
			const bool c5 = (rap.imagesPeintes == 0u) && (r.introuvables >= 2u)
							&& !r.premiereIntrouvable.Empty();
			if (!c5) ++ko;
			printf("  C5 introuvable     introuvables=%u premiere=\"%s\"  %s\n", r.introuvables,
				   r.premiereIntrouvable.CStr(), c5 ? "OK" : "<<< KO");
		}

		NkGuiPoserChargeurImage(nullptr, nullptr);
		NkGuiImagesOublier();
		printf("=== %s (ko = %u) ===\n", ko == 0u ? "TOUT PASSE" : "KO", ko);
		return ko == 0u ? 0 : 1;
	}

} // namespace nkuidesign

#endif // __NKENTSEU_NKUIDESIGN_SONDEIMAGES_H__

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
