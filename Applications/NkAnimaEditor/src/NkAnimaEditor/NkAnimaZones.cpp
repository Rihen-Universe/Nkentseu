// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkAnimaZones.cpp — CE QUE PEIGNENT LES ZONES HÔTES.
// =============================================================================

#include "NkAnimaZones.h"

#include "AnimBridge.h"

namespace nkanima {

	namespace {

		// ── LA BANDE DES CLÉS ──────────────────────────────────────────────
		//  Les clés de l'animation courante, plus le curseur de lecture. Elle
		//  rend VRAI parce qu'elle peint ; le jour où elle n'a rien à peindre,
		//  elle le DIT au lieu de se taire.
		bool ZoneApercuCles(nkgui::NkGuiContext &ctx, const nkgui::NkRect &z, void *) {
			auto &dl = ctx.DL();
			dl.AddRectFilled(z, ctx.theme.track, ctx.theme.rounding);
			dl.AddRect(z, ctx.theme.border, 1.f, ctx.theme.rounding);

			const float32 dur = AnimDuration();
			if (!AnimLoaded() || dur <= 0.f) {
				if (ctx.font && ctx.font->Valid())
					dl.AddText(ctx.font->Face(), ctx.font->TexId(), {z.x + 6.f, z.y + 4.f},
							   "aucun clip chargé", ctx.theme.textDisabled);
				// SERVIE : elle dit qu'il n'y a rien, et ce n'est pas rien.
				return true;
			}

			NkVector<float32> temps;
			AnimGetKeyTimes(temps);
			for (usize i = 0; i < temps.Size(); ++i) {
				const float32 t = temps[(uint32)i] / dur;
				const float32 x = z.x + 2.f + t * (z.w - 4.f);
				dl.AddRectFilled({x - 1.f, z.y + 3.f, 2.f, z.h - 6.f}, ctx.theme.accent, 0.f);
			}
			// Le curseur de lecture, en blanc : il bouge, donc la zone est VIVANTE.
			const float32 cx = z.x + 2.f + (AnimCursor() / dur) * (z.w - 4.f);
			dl.AddRectFilled({cx - 1.f, z.y + 1.f, 2.f, z.h - 2.f},
							 nkgui::NkColor{255, 255, 255, 255}, 0.f);
			return true;
		}

		// ── LE SQUELETTE, EN PROJECTION FRONTALE ───────────────────────────
		//  `AnimGetSkeleton` rend les positions et le parent de chaque joint :
		//  de quoi tracer la chaîne osseuse. Elle sert à autre chose qu'à
		//  décorer — elle montre TOUT DE SUITE si le clip a un squelette,
		//  combien de joints, et si la pose bouge quand le curseur se déplace.
		//
		//  ⚠️ PROJECTION ORTHOGONALE X/Y, ET C'EST DIT PLUTÔT QUE SUGGÉRÉ. Il
		//     n'y a ni caméra ni perspective ici : un personnage de profil s'y
		//     verra écrasé. Le viseur 3D est à côté pour ça. Une vue qui
		//     prétendrait être 3D sans l'être coûterait plus que ce schéma
		//     assumé.
		bool ZoneSquelette(nkgui::NkGuiContext &ctx, const nkgui::NkRect &z, void *) {
			auto &dl = ctx.DL();
			dl.AddRectFilled(z, ctx.theme.track, ctx.theme.rounding);
			dl.AddRect(z, ctx.theme.border, 1.f, ctx.theme.rounding);

			const uint32 n = AnimLoaded() ? AnimJointCount() : 0u;
			if (n == 0u) {
				if (ctx.font && ctx.font->Valid())
					dl.AddText(ctx.font->Face(), ctx.font->TexId(), {z.x + 6.f, z.y + 4.f},
							   "aucun squelette chargé", ctx.theme.textDisabled);
				return true;
			}

			NkVector<NkVec3f> pos;
			NkVector<int32> parent;
			AnimGetSkeleton(pos, parent);
			if (pos.Size() == 0u)
				return true;

			// ⚠️ CADRAGE SUR L'ÉTENDUE RÉELLE. Un squelette de 1,80 m et un de
			//    18 cm doivent tous deux remplir la zone : un facteur en dur
			//    n'en montrerait qu'un des deux, et l'autre serait un point ou
			//    débordrait sans qu'on sache pourquoi.
			float32 minX = pos[0].x, maxX = pos[0].x, minY = pos[0].y, maxY = pos[0].y;
			for (usize i = 1; i < pos.Size(); ++i) {
				const NkVec3f &p = pos[(uint32)i];
				if (p.x < minX)
					minX = p.x;
				if (p.x > maxX)
					maxX = p.x;
				if (p.y < minY)
					minY = p.y;
				if (p.y > maxY)
					maxY = p.y;
			}
			const float32 etX = (maxX - minX) > 0.0001f ? (maxX - minX) : 1.f;
			const float32 etY = (maxY - minY) > 0.0001f ? (maxY - minY) : 1.f;
			const float32 marge = 6.f;
			const float32 echX = (z.w - 2.f * marge) / etX;
			const float32 echY = (z.h - 2.f * marge) / etY;
			const float32 ech = (echX < echY) ? echX : echY;
			const float32 cx = z.x + z.w * 0.5f, cy = z.y + z.h * 0.5f;
			const float32 mx = (minX + maxX) * 0.5f, my = (minY + maxY) * 0.5f;

			// Y monte dans le monde, descend à l'écran : on inverse.
			auto vers = [&](const NkVec3f &p) -> nkgui::NkVec2 {
				return {cx + (p.x - mx) * ech, cy - (p.y - my) * ech};
			};

			for (usize i = 0; i < pos.Size(); ++i) {
				const int32 par = (i < parent.Size()) ? parent[(uint32)i] : -1;
				if (par >= 0 && (usize)par < pos.Size())
					dl.AddLine(vers(pos[(uint32)par]), vers(pos[(uint32)i]), ctx.theme.border, 1.f);
			}
			for (usize i = 0; i < pos.Size(); ++i) {
				const nkgui::NkVec2 p = vers(pos[(uint32)i]);
				dl.AddRectFilled({p.x - 1.5f, p.y - 1.5f, 3.f, 3.f}, ctx.theme.accent, 0.f);
			}
			return true;
		}

		const nkgui::NkZoneNommee kZones[] = {
			{"apercu_cles", &ZoneApercuCles, nullptr},
			{"squelette", &ZoneSquelette, nullptr},
		};

	} // namespace

	const nkgui::NkZoneNommee *ZonesAnimation(uint32 &outNombre) noexcept {
		outNombre = (uint32)(sizeof(kZones) / sizeof(kZones[0]));
		return kZones;
	}

} // namespace nkanima
