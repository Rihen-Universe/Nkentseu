// =============================================================================
// NkGuiFont.cpp — chargement police + atlas (Phase 3).
// =============================================================================
#include "NKGui/Core/NkGuiFont.h"
#include "NKPlatform/NkEnv.h" // (21/10) env::GetEnvVar : la contre-epreuve des cadres
#include "NKCore/Text/NkSortie.h" // (26/10) NkPrintfSortie, pas printf (garde std)
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace nkgui {

		// Plage de glyphes ETENDUE (au-dela du Latin-1) pour l'Unicode courant :
		// Latin etendu, ponctuation generale, fleches, box-drawing, blocs, formes
		// geometriques. L'atlas saute les glyphes absents de la police.
		static const uint32 *NkGlyphRanges() noexcept {
			static const uint32 kRanges[] = {0x0020, 0x00FF, // Latin + Latin-1
											 0x0100, 0x024F, // Latin etendu-A/B
											 0x0250, 0x02AF, // API (phonetique)
											 0x0300, 0x036F, // diacritiques
											 0x0370, 0x03FF, // grec
											 0x0400, 0x04FF, // cyrillique
											 0x2000, 0x206F, // ponctuation generale (tirets, guillemets, puces...)
											 0x2070, 0x209F, // exposants / indices
											 0x20A0, 0x20CF, // symboles monetaires (€, £, ¥...)
											 0x2100, 0x214F, // symboles type-lettre (™, №, Ω...)
											 0x2150, 0x218F, // formes numeriques (fractions, chiffres romains)
											 0x2190, 0x21FF, // fleches
											 0x2200, 0x22FF, // operateurs mathematiques
											 0x2300, 0x23FF, // technique divers (symboles, fleches retour...)
											 0x2460, 0x24FF, // alphanumeriques encercles (①, Ⓐ...)
											 0x2500, 0x257F, // box-drawing
											 0x2580, 0x259F, // blocs
											 0x25A0, 0x25FF, // formes geometriques
											 0x2600, 0x26FF, // symboles divers (☀, ⚠, ★...)
											 0x2700, 0x27BF, // dingbats (✓, ✗, ✔, ✦...)
											 0x2B00, 0x2BFF, // fleches & symboles divers
											 0xFB00, 0xFB06, // ligatures latines (ﬀ, ﬁ, ﬂ...)
											 0};
			return kRanges;
		}

		// Plages couvertes par la police de REPLI « broad » (large couverture).
		static const uint32 *NkBroadRanges() noexcept {
			static const uint32 r[] = {
				0x0020, 0x024F, 0x0250, 0x02FF, 0x0300, 0x036F, // Latin, API, diacritiques
				0x0370, 0x052F, 0x0590, 0x05FF, 0x0600, 0x06FF, // grec, cyrillique, hebreu, arabe
				0x1E00, 0x1EFF, 0x1F00, 0x1FFF,					// latin etendu add., grec etendu
				0x2000, 0x2BFF, 0xFB00, 0xFB4F, 0};
			return r;
		}

		static const uint32 *NkCjkRanges() noexcept {
			static const uint32 r[] = {
				0x2E80, 0x2EFF, 0x3000, 0x303F, 0x3040, 0x30FF,	  // radicaux, ponctuation, kana
				0x3100, 0x312F, 0x3130, 0x318F, 0x31A0, 0x31FF,	  // bopomofo, jamo, ext bopomofo
				0x3400, 0x4DBF, 0x4E00, 0x9FFF,					  // ext-A + unified (ideogrammes)
				0xAC00, 0xD7A3, 0xF900, 0xFAFF, 0xFF00, 0xFFEF, 0 // hangul, compat, pleine chasse
			};
			return r;
		}

		static const uint32 *NkEmojiRanges() noexcept {
			static const uint32 r[] = {
				0x2600,	 0x27BF,  0x2B00, 0x2BFF, // symboles divers, dingbats
				0x1F000, 0x1FAFF, 0				  // emoji (mahjong, pictogrammes, emoticones...)
			};
			return r;
		}

		// Chemins des polices de repli EXTERNES (poses par l'app via NkSetFallbackFontPaths).
		static char gFbBroad[600] = {}, gFbCjk[600] = {}, gFbEmoji[600] = {};

		static void NkCopyPath(char *dst, const char *src) {
			dst[0] = '\0';
			if (src) {
				usize i = 0;
				for (; src[i] && i + 1 < 600; ++i)
					dst[i] = src[i];
				dst[i] = '\0';
			}
		}

		void NkSetFallbackFontPaths(const char *broad, const char *cjk, const char *emoji) noexcept {
			NkCopyPath(gFbBroad, broad);
			NkCopyPath(gFbCjk, cjk);
			NkCopyPath(gFbEmoji, emoji);
		}

		static bool NkFileExists(const char *p) noexcept {
			if (!p || !*p)
				return false;
			std::FILE *f = std::fopen(p, "rb");
			if (f) {
				std::fclose(f);
				return true;
			}
			return false;
		}

		// ── (26/10) LES IDEOGRAMMES A LA DEMANDE (voir NkGuiFont.h) ─────────────────────
		static int32 gCjkALaDemande = -1; // -1 : pas encore lu (NK_POLICE_CJK)
		static NkGuiFont *gPolicesDifferees[32] = {};

		void NkSetCjkALaDemande(bool aLaDemande) noexcept {
			gCjkALaDemande = aLaDemande ? 1 : 0;
		}

		bool NkCjkALaDemande() noexcept {
			if (gCjkALaDemande < 0) {
				const char *v = env::GetEnvVar("NK_POLICE_CJK");
				const char *imm = "immediat";
				bool immediat = v != nullptr;
				for (usize k = 0; immediat && (v[k] || imm[k]); ++k)
					immediat = v[k] == imm[k];
				gCjkALaDemande = immediat ? 0 : 1;
			}
			return gCjkALaDemande == 1;
		}

		static void NkInscrireDifferee(NkGuiFont *f) noexcept {
			for (NkGuiFont *&e : gPolicesDifferees)
				if (e == f)
					return;
			for (NkGuiFont *&e : gPolicesDifferees)
				if (!e) {
					e = f;
					return;
				}
		}

		NkGuiFont::~NkGuiFont() {
			for (NkGuiFont *&e : gPolicesDifferees)
				if (e == this)
					e = nullptr;
		}

		static bool NkEstIdeogramme(uint32 c) noexcept {
			const uint32 *r = NkCjkRanges();
			for (usize i = 0; r[i]; i += 2u)
				if (c >= r[i] && c <= r[i + 1u])
					return true;
			return false;
		}

		void NkGuiPolicesSignalerTexte(const NkFont *face, const char *debut, const char *fin) noexcept {
			if (!face || !debut)
				return;
			NkGuiFont *f = nullptr;
			for (NkGuiFont *e : gPolicesDifferees)
				if (e && e->face == face && !e->cjkCharge && !e->cjkDemande) {
					f = e;
					break;
				}
			if (!f)
				return; // la face n'attend aucun ideogramme : rien a lire
			const char *p = debut;
			const char *e = fin;
			if (!e) {
				e = debut;
				while (*e)
					++e;
			}
			while (p < e) {
				if ((unsigned char)*p < 0xE2u) { // ASCII, latin, grec, cyrillique... : pas d'ideogramme possible
					++p;
					continue;
				}
				const NkFontCodepoint c = NkFontDecodeUTF8(&p, e);
				if (c == 0u)
					break;
				if (NkEstIdeogramme((uint32)c) && !face->FindGlyphNoFallback(c)) {
					f->cjkDemande = true;
					if (env::GetEnvVar("NK_TRACE_POLICE"))
						NkPrintfSortie("[police] ideogramme U+%04X demande : l'atlas se refait a l'image suivante\n", (unsigned)c);
					return;
				}
			}
		}

		uint32 NkGuiPolicesAppliquerDemandes(NkGuiTeleverserPolice televerser, void *user) noexcept {
			// `NK_POLICE_CJK=sans-repli` : la CONTRE-EPREUVE du banc (la demande n'est jamais servie : le texte
			// chinois reste en glyphes de repli)
			{
				const char *v = env::GetEnvVar("NK_POLICE_CJK");
				const char *sr = "sans-repli";
				bool sans = v != nullptr;
				for (usize k = 0; sans && (v[k] || sr[k]); ++k)
					sans = v[k] == sr[k];
				if (sans)
					return 0u;
			}
			uint32 n = 0u;
			for (NkGuiFont *f : gPolicesDifferees) {
				if (!f || !f->cjkDemande || f->cjkCharge)
					continue;
				const uint32 tex = f->texId;
				const bool ok = f->depuisFichier ? f->LoadFromFile(f->cheminCharge, f->tailleChargee, f->replisExternes)
												 : f->LoadEmbedded(f->idCharge, f->tailleChargee, f->replisExternes);
				f->texId = tex;
				if (ok && televerser)
					(void)televerser(f->texId, f->pixels, f->atlasW, f->atlasH, user);
				if (env::GetEnvVar("NK_TRACE_POLICE"))
					NkPrintfSortie("[police] atlas refait avec les ideogrammes : %d x %d (%s)\n", (int)f->atlasW, (int)f->atlasH,
								ok ? "ok" : "ECHEC");
				++n;
			}
			return n;
		}

		// Fusionne les polices de repli externes presentes (merge -> comble les manquants).
		// ext=false : n'incorpore AUCUN repli externe (broad/CJK/emoji = plusieurs milliers
		// de glyphes) -> atlas MINUSCULE + build tres rapide. Reserve aux polices MONOSPACE
		// (code) : leur police embarquee couvre deja Latin-1/etendu, box-drawing, fleches...
		// (26/10) `avecCjk` faux : les ideogrammes attendent d'etre demandes (NkGuiFont.h).
		static void NkMergeFallback(NkFontAtlas &atlas, float32 sizePx, bool ext, bool avecCjk = true) noexcept {
			if (!ext)
				return;
			// (21/10) LES CADRES ET LES BLOCS (U+2500-U+259F) : « ╔═╗ », « █ ». Rodolf : « l'affichage dans
			// l'assistant n'est pas correct » -- la banniere de `jenga build --verbose`, collee dans
			// l'Assistant IA de NKCode, s'y dessinait en « ? » : ni la police d'interface (Karla, Inter)
			// ni le repli « broad » (Noto Sans) n'ont ces glyphes, et le glyphe de repli de NKFont est
			// « ? ». La police monospace embarquee (DejaVu Sans Mono, celle du code et du terminal) les a
			// tous : elle les PRETE, en fusion, AVANT les replis externes (le premier glyphe gagne, et
			// l'atlas a un plafond de glyphes que les ideogrammes atteignent). Ses traits occupent toute
			// la hauteur de la ligne : les cadres se joignent. Contre-epreuve du banc de NKCode
			// (tools/banc_cadres.sh) : NK_POLICE_MUTATION=sans-cadres, l'etat d'avant.
			{
				const char *mut = env::GetEnvVar("NK_POLICE_MUTATION");
				const char *nom = "sans-cadres";
				bool sans = mut != nullptr;
				for (usize k = 0; sans && (mut[k] || nom[k]); ++k)
					sans = mut[k] == nom[k];
				if (!sans) {
					static const uint32 kCadres[] = {0x2500, 0x259F, 0};
					NkFontConfig cadres;
					cadres.glyphRanges = kCadres;
					cadres.mergeMode = true;
					(void)NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::DejaVuSansMono, sizePx > 0.f ? sizePx : 16.f,
													 &cadres);
				}
			}
			auto add = [&](const char *path, const uint32 *ranges) {
				const bool present = path && path[0] && NkFileExists(path);
				if (!present) {
					if (path && path[0] && std::getenv("NK_TRACE_POLICE"))
						std::printf("[police] repli ABSENT du disque : %s\n", path);
					return;
				}
				NkFontConfig fb;
				fb.glyphRanges = ranges;
				fb.mergeMode = true;
				const bool ok = atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &fb) != nullptr;
				if (std::getenv("NK_TRACE_POLICE"))
					std::printf("[police] repli %s : %s\n", ok ? "FUSIONNE" : "REFUSE PAR L'ATLAS", path);
			};
			add(gFbBroad, NkBroadRanges());
			if (avecCjk)
				add(gFbCjk, NkCjkRanges());
			add(gFbEmoji, NkEmojiRanges());
		}

		bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback) noexcept {
			atlas.Clear();
			face = nullptr;
			pixels = nullptr; // rechargeable
			NkFontConfig cfg;
			cfg.glyphRanges = NkGlyphRanges();
			face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
			if (!face)
				return false;
			// (26/10) LES IDEOGRAMMES A LA DEMANDE : sans demande, l'atlas ne les porte pas ; la police s'inscrit pour
			// se refaire a l'identique quand un texte les demandera
			depuisFichier = false;
			idCharge = id;
			tailleChargee = sizePx;
			replisExternes = extFallback;
			const bool differe = extFallback && gFbCjk[0] && NkCjkALaDemande() && !cjkDemande;
			cjkCharge = extFallback && gFbCjk[0] && !differe;
			if (differe)
				NkInscrireDifferee(this);
			NkMergeFallback(atlas, sizePx, extFallback, !differe); // repli pour les glyphes manquants
			if (!atlas.Build())
				return false;
			int32 bpp = 0;
			atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
			dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
			if (std::getenv("NK_TRACE_POLICE"))
				std::printf("[police] atlas %d x %d apres fusion (sans ideogrammes il tient en quelques centaines de px de cote ; avec, plusieurs milliers)\n", (int)atlasW, (int)atlasH);
			return dirty;
		}

		bool NkGuiFont::LoadFromFile(const char *path, float32 sizePx, bool extFallback) noexcept {
			if (!path || !*path)
				return false;
			atlas.Clear();
			face = nullptr;
			pixels = nullptr; // rechargeable
			NkFontConfig cfg;
			cfg.glyphRanges = NkGlyphRanges();
			face = atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &cfg);
			if (!face)
				return false;
			// (26/10) les ideogrammes a la demande, comme LoadEmbedded
			if (path != cheminCharge) {
				usize i = 0;
				for (; path[i] && i + 1u < sizeof(cheminCharge); ++i)
					cheminCharge[i] = path[i];
				cheminCharge[i] = 0;
			}
			depuisFichier = true;
			tailleChargee = sizePx;
			replisExternes = extFallback;
			const bool differe = extFallback && gFbCjk[0] && NkCjkALaDemande() && !cjkDemande;
			cjkCharge = extFallback && gFbCjk[0] && !differe;
			if (differe)
				NkInscrireDifferee(this);
			NkMergeFallback(atlas, sizePx > 0.f ? sizePx : 16.f, extFallback, !differe);
			if (!atlas.Build())
				return false;
			int32 bpp = 0;
			atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
			dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
			return dirty;
		}

	} // namespace nkgui
} // namespace nkentseu
