// =============================================================================
// Nogee/Panels/DetailsPanel.cpp — portage NKUI -> NKGui, vise sur §8 (cf. .h)
// =============================================================================
#include "DetailsPanel.h"
#include "NKGui/NKGui.h"
#include "Noge/ECS/NkEcsUtil.h"
#include "Noge/ECS/Components/Core/NkCoreComponents.h"
#include "Noge/ECS/Components/Core/NkTransform.h"
#include "Noge/ECS/Components/Core/NkTag.h"
#include "Noge/ECS/Systems/NkReflectComponents.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace noge {

		using namespace nkgui;

		namespace {
			// §8 : « code couleur (rouge/vert/bleu) sur le label » des axes.
			// Ce sont des couleurs SEMANTIQUES (l'axe X est rouge partout dans
			// l'industrie), pas un habillage de theme : elles restent donc en
			// dur, comme les niveaux de log de la Console.
			constexpr NkColor kAxisX{232, 90, 90, 255};
			constexpr NkColor kAxisY{120, 200, 110, 255};
			constexpr NkColor kAxisZ{90, 150, 240, 255};
			constexpr NkColor kMuted{150, 150, 150, 255};
		} // namespace

		bool DetailsPanel::PassesFilter(const char *label) const noexcept {
			if (mFilterBuf[0] == '\0')
				return true;
			// Sensible a la casse : meme convention que la Console et que le
			// World Outliner. Aucun helper insensible n'existe dans le depot.
			return label && std::strstr(label, mFilterBuf) != nullptr;
		}

		// =====================================================================
		// Une ligne « Label  [X][Y][Z] », labels colores.
		// =====================================================================
		bool DetailsPanel::RenderVec3Row(NkGuiContext &ctx, const char *label, float32 *v,
										 const bool *lockUniform) noexcept {
			if (!PassesFilter(label))
				return false;

			// §8 : « label a gauche (35% de largeur), champ editable a droite ».
			const float32 lineH = ctx.font ? ctx.font->LineHeight() : 16.f;
			const NkRect lr = ctx.NextItemRect(0.35f * ctx.ContentWidth(), lineH);
			TextAt(ctx, {lr.x, lr.y}, label, kMuted);

			// Memoriser l'avant pour detecter QUEL axe a bouge (cadenas).
			const float32 before[3] = {v[0], v[1], v[2]};
			bool changed = false;

			char id[32];
			const NkColor cols[3] = {kAxisX, kAxisY, kAxisZ};
			const char *axis[3] = {"X", "Y", "Z"};

			for (int32 i = 0; i < 3; ++i) {
				ctx.SameLine();
				const NkRect ar = ctx.NextItemRect(12.f, lineH);
				TextAt(ctx, {ar.x, ar.y}, axis[i], cols[i]);

				ctx.SameLine();
				std::snprintf(id, sizeof(id), "##%s_%d", label, i);
				if (DragFloat(ctx, id, v[i], 0.01f))
					changed = true;
			}

			// §8 : cadenas — editer un axe applique le meme FACTEUR aux trois.
			// Un facteur, pas un report de valeur : c'est ce qui preserve les
			// proportions non uniformes, et c'est le sens du cadenas.
			if (changed && lockUniform && *lockUniform) {
				for (int32 i = 0; i < 3; ++i) {
					if (v[i] != before[i] && before[i] != 0.f) {
						const float32 factor = v[i] / before[i];
						for (int32 j = 0; j < 3; ++j) {
							if (j != i)
								v[j] = before[j] * factor;
						}
						break;
					}
				}
			}
			return changed;
		}

		// =====================================================================
		// §8 : Transform, toujours en haut et NON repliable.
		// Acces TYPE (`world.Get<NkTransform>`) — c'est precisement ce que la
		// boucle par reflexion ne peut pas faire sans `GetRaw`.
		// =====================================================================
		void DetailsPanel::RenderTransform(NkGuiContext &ctx, ecs::NkEntityId id) noexcept {
			ecs::NkTransform *tr = mWorld->Get<ecs::NkTransform>(id);
			if (!tr) {
				Text(ctx, "(pas de composant Transform)");
				return;
			}

			Text(ctx, "Transform");
			Separator(ctx);

			float32 pos[3] = {tr->localPosition.x, tr->localPosition.y, tr->localPosition.z};
			if (RenderVec3Row(ctx, "Position", pos, nullptr))
				tr->SetLocalPosition(pos[0], pos[1], pos[2]);

			// ⛔ ROTATION EN LECTURE SEULE — cf. l'en-tete : la conversion
			// quaternion <-> Euler passe par NkAngle et demande un temoin. Ecrire
			// l'aller-retour a l'aveugle produirait une derive silencieuse.
			if (PassesFilter("Rotation")) {
				const float32 lineH = ctx.font ? ctx.font->LineHeight() : 16.f;
				const NkRect lr = ctx.NextItemRect(0.35f * ctx.ContentWidth(), lineH);
				TextAt(ctx, {lr.x, lr.y}, "Rotation", kMuted);
				ctx.SameLine();
				char q[96];
				std::snprintf(q, sizeof(q), "quat %.3f %.3f %.3f %.3f (lecture seule)", tr->localRotation.x,
							  tr->localRotation.y, tr->localRotation.z, tr->localRotation.w);
				const NkRect qr = ctx.NextItemRect(-1.f, lineH);
				TextAt(ctx, {qr.x, qr.y}, q, kMuted);
			}

			float32 scl[3] = {tr->localScale.x, tr->localScale.y, tr->localScale.z};
			if (RenderVec3Row(ctx, "Scale", scl, &mLockScale)) {
				// `SetLocalScale` n'a pas de surcharge (x,y,z) — seulement
				// (NkVec3f) et (float32 uniforme). On passe donc par le vecteur.
				tr->SetLocalScale(math::NkVec3f{scl[0], scl[1], scl[2]});
			}

			ctx.SameLine();
			Checkbox(ctx, "Lier##scale_lock", mLockScale);
		}

		// =====================================================================
		// Corps du panneau.
		// =====================================================================
		void DetailsPanel::OnUI(editorkit::NkEditorFrameContext &ec) {
			NkGuiContext &ctx = ec.Ui();

			if (!mWorld) {
				Text(ctx, "Aucun monde lie.");
				return;
			}

			const ecs::NkEntityId id = mSel ? mSel->Primary() : ecs::NkEntityId::Invalid();
			if (!id.IsValid() || !mWorld->IsAlive(id)) {
				Text(ctx, "Aucune entite selectionnee");
				return;
			}

			// ── §8 : en-tete, nom de l'acteur EDITABLE ────────────────────────
			if (ecs::NkName *n = mWorld->Get<ecs::NkName>(id))
				InputText(ctx, "##dp_name", n->value, static_cast<int32>(ecs::NkName::kMaxLen));
			else
				Text(ctx, "(entite sans nom)");

			// ── §8 : recherche de propriete (filtre en direct) ────────────────
			InputText(ctx, "Rechercher##dp_filter", mFilterBuf, static_cast<int32>(sizeof(mFilterBuf)));
			Separator(ctx);

			// ── §8 : Transform, toujours en haut ──────────────────────────────
			RenderTransform(ctx, id);

			Separator(ctx);

			// ── Le reste des proprietes : BLOQUE, et on le DIT a l'ecran ──────
			// Le panneau NKUI, lui, itere sur les composants reflechis et
			// `continue` a chaque tour faute de `GetRaw` : il ne dessine rien, et
			// rien ne signale que quelque chose manque. On refuse de reproduire
			// ce silence.
			Text(ctx, "Composants reflechis : indisponibles");
			Text(ctx, "NkWorld::GetRaw(id, typeId) n'existe pas — sans acces");
			Text(ctx, "generique a la memoire d'un composant, aucune propriete");
			Text(ctx, "reflechie ne peut etre lue ni editee.");

			Separator(ctx);
			RenderAddComponentMenu(ctx, id);
		}

		// =====================================================================
		// « Ajouter un composant » : fonctionne, parce qu'il n'a besoin que de
		// `meta.addFn` — jamais d'un acces memoire generique.
		// =====================================================================
		void DetailsPanel::RenderAddComponentMenu(NkGuiContext &ctx, ecs::NkEntityId id) noexcept {
			if (Button(ctx, "Ajouter un composant"))
				ctx.OpenPopup(ctx.GetId("##dp_addcomp"));

			if (BeginPopupMenu(ctx, "##dp_addcomp")) {
				const auto &metas = ecs::NkComponentMetaRegistry::Get().All();
				for (nk_usize i = 0; i < metas.Size(); ++i) {
					const auto &m = metas[i];
					const char *name = m.displayName ? m.displayName : m.typeName;
					if (MenuItem(ctx, name)) {
						if (m.addFn)
							m.addFn(*mWorld, id);
						ctx.ClosePopup();
					}
				}
				EndPopupMenu(ctx);
			}
		}

	} // namespace noge
} // namespace nkentseu
