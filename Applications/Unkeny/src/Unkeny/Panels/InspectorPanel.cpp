#include "InspectorPanel.h"
#include "NKUI/NkUIMenu.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

using namespace nkentseu::nkui;
using namespace nkentseu::math;

namespace nkentseu {
    namespace Unkeny {

        void InspectorPanel::Render(NkUIContext& ctx,
                                    NkUIWindowManager& wm,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    NkUILayoutStack& ls,
                                    ecs::NkWorld& world,
                                    const NkSelectionManager& sel,
                                    CommandHistory* hist,
                                    NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Inspector##insp",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            ecs::NkEntityId id = sel.Primary();
            if (!id.IsValid() || !world.IsAlive(id)) {
                NkUI::Text(ctx, ls, dl, font, "Aucune entité sélectionnée",
                           NkColor{130,130,130,255});
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── En-tête : nom de l'entité ─────────────────────────────────────
            {
                auto* n = world.Get<ecs::NkNameComponent>(id);
                char buf[128] = {};
                if (n) NkStrNCpy(buf, n->name, 127);
                NkUI::BeginRow(ctx, ls, 26.f);
                NkUI::SetNextGrow(ctx, ls);
                if (NkUI::InputText(ctx, ls, dl, font, "##entityname", buf, 128)) {
                    if (n) NkStrNCpy(n->name, buf, 127);
                }
                NkUI::EndRow(ctx, ls);
                NkUI::Separator(ctx, ls, dl);
            }

            // ── Composants réfléchis ──────────────────────────────────────────
            const auto& metas = ecs::NkComponentMetaRegistry::Get().All();
            for (nk_usize i = 0; i < metas.Size(); ++i) {
                const auto& meta = metas[i];

                // TODO : obtenir le pointeur via NkWorld::GetRaw(id, typeId)
                // Pour l'instant on utilise une approche par template spécialisé.
                // Cette ligne est un stub — à remplacer quand GetRaw est disponible :
                void* ptr = nullptr; // world.GetRaw(id, meta.typeName);
                if (!ptr) continue;

                RenderComponent(ctx, dl, font, ls, meta, ptr, id, hist);
            }

            NkUI::Separator(ctx, ls, dl);
            RenderAddComponentMenu(ctx, dl, font, ls, world, id);

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        void InspectorPanel::RenderComponent(NkUIContext& ctx,
                                              NkUIDrawList& dl,
                                              NkUIFont& font,
                                              NkUILayoutStack& ls,
                                              const ecs::NkComponentMeta& meta,
                                              void* compPtr,
                                              ecs::NkEntityId /*id*/,
                                              CommandHistory* /*hist*/) noexcept {
            const char* name = meta.displayName ? meta.displayName : meta.typeName;
            bool open = IsSectionOpen(name);

            NkUI::BeginRow(ctx, ls, ctx.GetItemHeight());
            bool toggled = NkUI::TreeNode(ctx, ls, dl, font, name, &open);
            NkUI::EndRow(ctx, ls);
            if (toggled) SetSectionOpen(name, open);
            if (!open) return;

            // Rendu de chaque champ
            for (nk_usize i = 0; i < meta.fields.Size(); ++i) {
                const auto& field = meta.fields[i];
                if (field.hidden) continue;

                NkUI::BeginRow(ctx, ls, ctx.GetItemHeight());

                // Label dans la première colonne (~40%)
                NkUI::SetNextWidth(ctx, ls, 0.40f * ctx.viewW);
                NkUI::Text(ctx, ls, dl, font, field.name,
                           NkColor{180,180,180,255});
                NkUI::SameLine(ctx, ls);

                // Widget dans la deuxième colonne
                NkUI::SetNextGrow(ctx, ls);
                void* fieldPtr = static_cast<nk_uint8*>(compPtr) + field.offset;
                if (!field.readOnly)
                    RenderField(ctx, dl, font, ls, field, fieldPtr);
                else {
                    // Afficher en lecture seule
                    char buf[64] = "?";
                    NkUI::Text(ctx, ls, dl, font, buf,
                               NkColor{150,150,150,255});
                }

                NkUI::EndRow(ctx, ls);
            }
            NkUI::TreePop(ctx, ls);
        }

        bool InspectorPanel::RenderField(NkUIContext& ctx,
                                          NkUIDrawList& dl,
                                          NkUIFont& font,
                                          NkUILayoutStack& ls,
                                          const ecs::NkFieldMeta& f,
                                          void* ptr) noexcept {
            using FT = ecs::NkFieldType;
            bool changed = false;

            switch (f.type) {
            case FT::Bool: {
                bool& v = *static_cast<bool*>(ptr);
                changed = NkUI::Checkbox(ctx, ls, dl, font, "##bool", v);
                break;
            }
            case FT::Float32: {
                float32& v = *static_cast<float32*>(ptr);
                float32 mn = f.rangeMin, mx = f.rangeMax, st = f.step;
                if (mn == 0.f && mx == 0.f) { mn = -1e9f; mx = 1e9f; }
                changed = NkUI::DragFloat(ctx, ls, dl, font, "##f32", v,
                                          st > 0.f ? st : 0.01f, mn, mx);
                break;
            }
            case FT::Int32: {
                int32& v = *static_cast<int32*>(ptr);
                changed = NkUI::DragInt(ctx, ls, dl, font, "##i32", v, 1.f);
                break;
            }
            case FT::Vec3f: {
                float32* v = static_cast<float32*>(ptr);
                // XYZ en ligne via DragFloat3
                changed  = NkUI::DragFloat(ctx, ls, dl, font, "##x", v[0], 0.01f);
                NkUI::SameLine(ctx, ls, 0, 2.f);
                changed |= NkUI::DragFloat(ctx, ls, dl, font, "##y", v[1], 0.01f);
                NkUI::SameLine(ctx, ls, 0, 2.f);
                changed |= NkUI::DragFloat(ctx, ls, dl, font, "##z", v[2], 0.01f);
                break;
            }
            case FT::Vec4f:
            case FT::Color: {
                NkColor col{
                    (nk_uint8)(static_cast<float32*>(ptr)[0] * 255),
                    (nk_uint8)(static_cast<float32*>(ptr)[1] * 255),
                    (nk_uint8)(static_cast<float32*>(ptr)[2] * 255),
                    (nk_uint8)(static_cast<float32*>(ptr)[3] * 255)
                };
                if (NkUI::ColorButton(ctx, ls, dl, font, "##col", col)) {
                    // Ouvrir le colorpicker (TODO : popup)
                    changed = true;
                }
                break;
            }
            case FT::StringFixed: {
                char* buf = static_cast<char*>(ptr);
                int32 maxLen = (f.strMaxLen > 0) ? (int32)f.strMaxLen : 64;
                changed = NkUI::InputText(ctx, ls, dl, font, "##str",
                                          buf, maxLen);
                break;
            }
            case FT::AssetPath: {
                // NkString — afficher le chemin + bouton "..."
                NkString& s = *static_cast<NkString*>(ptr);
                char buf[256];
                NkStrNCpy(buf, s.CStr(), 255);
                if (NkUI::InputText(ctx, ls, dl, font, "##asset", buf, 256)) {
                    s = NkString(buf);
                    changed = true;
                }
                NkUI::SameLine(ctx, ls, 0, 2.f);
                NkUI::SetNextWidth(ctx, ls, 22.f);
                if (NkUI::Button(ctx, ls, dl, font, "...##assetbrowse")) {
                    // TODO : ouvrir AssetBrowser en mode sélection
                }
                break;
            }
            default:
                NkUI::Text(ctx, ls, dl, font, "(non éditable)",
                           NkColor{100,100,100,255});
                break;
            }
            return changed;
        }

        void InspectorPanel::RenderAddComponentMenu(NkUIContext& ctx,
                                                    NkUIDrawList& dl,
                                                    NkUIFont& font,
                                                    NkUILayoutStack& ls,
                                                    ecs::NkWorld& world,
                                                    ecs::NkEntityId id) noexcept {
            NkUI::BeginRow(ctx, ls, 28.f);
            NkUI::SetNextGrow(ctx, ls);
            if (NkUI::Button(ctx, ls, dl, font, "Ajouter un composant")) {
                NkUIMenu::OpenPopup(ctx, "##addcomp");
            }
            NkUI::EndRow(ctx, ls);

            if (NkUIMenu::BeginPopup(ctx, dl, font, "##addcomp", {220, 0})) {
                const auto& metas = ecs::NkComponentMetaRegistry::Get().All();
                for (nk_usize i = 0; i < metas.Size(); ++i) {
                    const auto& m = metas[i];
                    const char* name = m.displayName ? m.displayName : m.typeName;
                    if (NkUIMenu::MenuItem(ctx, dl, font, name)) {
                        if (m.addFn) m.addFn(world, id);
                        NkUIMenu::ClosePopup(ctx);
                    }
                }
                NkUIMenu::EndPopup(ctx, dl);
            }
        }

        bool InspectorPanel::IsSectionOpen(const char* name) noexcept {
            for (nk_uint32 i = 0; i < mSectionCount; ++i)
                if (NkStrEqual(mSectionNames[i], name))
                    return mSectionOpen[i];
            // Nouvelle section — ouverte par défaut
            if (mSectionCount < kMaxSections) {
                NkStrNCpy(mSectionNames[mSectionCount], name, 63);
                mSectionOpen[mSectionCount] = true;
                ++mSectionCount;
            }
            return true;
        }

        void InspectorPanel::SetSectionOpen(const char* name, bool open) noexcept {
            for (nk_uint32 i = 0; i < mSectionCount; ++i) {
                if (NkStrEqual(mSectionNames[i], name)) {
                    mSectionOpen[i] = open;
                    return;
                }
            }
        }

    } // namespace Unkeny
} // namespace nkentseu
