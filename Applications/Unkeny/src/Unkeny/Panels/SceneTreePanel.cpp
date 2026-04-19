#include "SceneTreePanel.h"
#include "NKUI/NkUIMenu.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cstdio>

using namespace nkentseu::nkui;

namespace nkentseu {
    namespace Unkeny {

        void SceneTreePanel::Render(NkUIContext& ctx,
                                    NkUIWindowManager& wm,
                                    NkUIDrawList& dl,
                                    NkUIFont& font,
                                    NkUILayoutStack& ls,
                                    ecs::NkWorld& world,
                                    ecs::NkSceneGraph* scene,
                                    NkSelectionManager& sel,
                                    CommandHistory* hist,
                                    NkUIRect rect) noexcept {
            NkUIWindow::SetNextWindowPos({rect.x, rect.y});
            NkUIWindow::SetNextWindowSize({rect.w, rect.h});

            if (!NkUIWindow::Begin(ctx, wm, dl, font, ls, "Scene##tree",
                                   nullptr,
                                   NkUIWindowFlags::NK_NO_MOVE |
                                   NkUIWindowFlags::NK_NO_RESIZE)) {
                NkUIWindow::End(ctx, wm, dl, ls);
                return;
            }

            // ── Barre de titre du panel ────────────────────────────────────────
            NkUI::BeginRow(ctx, ls, 22.f);
            NkUI::Text(ctx, ls, dl, font, "Scène",
                       NkColor{180, 200, 255, 255});
            NkUI::SameLine(ctx, ls, 0.f, -1.f);

            // Bouton "+" pour créer une entité vide
            NkUI::SetNextWidth(ctx, ls, 22.f);
            if (NkUI::Button(ctx, ls, dl, font, "+##addentity")) {
                if (scene) {
                    ecs::NkEntityId newId = scene->SpawnNode("Entity");
                    sel.Select(newId);
                }
            }
            NkUI::EndRow(ctx, ls);
            NkUI::Separator(ctx, ls, dl);

            // ── Arbre des entités ─────────────────────────────────────────────
            // Collecter les racines (sans parent valide)
            NkVector<ecs::NkEntityId> roots;
            world.Query<const ecs::NkSceneNode>()
                 .Without<ecs::NkInactiveComponent>()
                 .ForEach([&](ecs::NkEntityId id, const ecs::NkSceneNode&) {
                     const ecs::NkParent* p = world.Get<ecs::NkParent>(id);
                     if (!p || !p->entity.IsValid())
                         roots.PushBack(id);
                 });

            for (nk_usize i = 0; i < roots.Size(); ++i) {
                RenderEntity(ctx, dl, font, ls, world,
                             roots[i], sel, hist, 0);
            }

            // Menu contextuel (clic droit sur entité)
            if (mContextMenuEntity.IsValid()) {
                RenderContextMenu(ctx, dl, font, world,
                                  mContextMenuEntity, scene, sel);
            }

            NkUIWindow::End(ctx, wm, dl, ls);
        }

        void SceneTreePanel::RenderEntity(NkUIContext& ctx,
                                          NkUIDrawList& dl,
                                          NkUIFont& font,
                                          NkUILayoutStack& ls,
                                          ecs::NkWorld& world,
                                          ecs::NkEntityId id,
                                          NkSelectionManager& sel,
                                          CommandHistory* hist,
                                          nk_uint32 depth) noexcept {
            const ecs::NkSceneNode* node = world.Get<ecs::NkSceneNode>(id);
            if (!node) return;

            const ecs::NkChildren* children = world.Get<ecs::NkChildren>(id);
            bool hasChildren = children && children->count > 0;
            bool isSelected  = sel.IsSelected(id);
            bool isOpen      = IsOpen(id);

            // Indentation
            if (depth > 0)
                NkUI::Spacing(ctx, ls, (float32)depth * 12.f);

            // Couleur de fond si sélectionné
            if (isSelected) {
                ctx.PushStyleColor(NkStyleVar::NK_STYLE_BG_ACTIVE,
                                   NkColor{60, 100, 160, 200});
            }

            // Mode renommage
            if (mRenamingEntity == id) {
                NkUI::SetNextWidth(ctx, ls, 0.f);
                bool confirmed = NkUI::InputText(ctx, ls, dl, font,
                                                 "##rename",
                                                 mRenameBuffer,
                                                 (int32)sizeof(mRenameBuffer));
                if (confirmed || ctx.input.IsKeyDown(NkKey::NK_RETURN) ||
                    ctx.input.IsKeyDown(NkKey::NK_ESCAPE)) {
                    if (confirmed && strlen(mRenameBuffer) > 0) {
                        if (ecs::NkSceneNode* n = world.Get<ecs::NkSceneNode>(id))
                            NkStrNCpy(n->name, mRenameBuffer, 63);
                    }
                    mRenamingEntity = ecs::NkEntityId::Invalid();
                }
            } else {
                // TreeNode (expandable si a des enfants)
                char label[96];
                snprintf(label, sizeof(label), "%s##%u_%u",
                         node->name[0] ? node->name : "Entity",
                         id.index, id.gen);

                bool openNow = isOpen;
                bool clicked = false;

                if (hasChildren) {
                    clicked = NkUI::TreeNode(ctx, ls, dl, font, label, &openNow);
                    if (openNow != isOpen) SetOpen(id, openNow);
                } else {
                    // Feuille : juste un item cliquable
                    NkUI::SetNextWidth(ctx, ls, 0.f);
                    NkUI::BeginRow(ctx, ls, ctx.GetItemHeight());
                    NkUI::Spacing(ctx, ls, 16.f); // icone
                    clicked = NkUI::InvisibleButton(ctx, ls,
                        label, {-1.f, ctx.GetItemHeight()});
                    if (!clicked) {
                        // Dessiner le texte manuellement dans la zone
                        NkVec2 cursor = ctx.GetCursor();
                        dl.AddText({cursor.x + 16.f, cursor.y + 2.f},
                                   node->name, NkColor{210, 210, 210, 255});
                    }
                    NkUI::EndRow(ctx, ls);
                }

                // Sélection au clic
                if (clicked) {
                    if (ctx.input.IsKeyDown(NkKey::NK_LEFT_CONTROL) ||
                        ctx.input.IsKeyDown(NkKey::NK_RIGHT_CONTROL))
                        sel.SelectToggle(id);
                    else
                        sel.Select(id);
                }

                // Clic droit → menu contextuel
                if (ctx.IsHovered(NkUI::TreeNode ? NkRect{} : NkRect{}) &&
                    ctx.input.IsMouseClicked(1)) {
                    mContextMenuEntity = id;
                    NkUIMenu::OpenContextMenu(ctx, "##entity_ctx");
                }
            }

            if (isSelected)
                ctx.PopStyle();

            // Rendu récursif des enfants
            if (hasChildren && IsOpen(id)) {
                for (nk_uint32 i = 0; i < children->count; ++i) {
                    RenderEntity(ctx, dl, font, ls, world,
                                 children->children[i], sel, hist, depth + 1);
                }
                if (hasChildren)
                    NkUI::TreePop(ctx, ls);
            }
        }

        void SceneTreePanel::RenderContextMenu(NkUIContext& ctx,
                                               NkUIDrawList& dl,
                                               NkUIFont& font,
                                               ecs::NkWorld& world,
                                               ecs::NkEntityId id,
                                               ecs::NkSceneGraph* scene,
                                               NkSelectionManager& sel) noexcept {
            if (!NkUIMenu::BeginContextMenu(ctx, dl, font, "##entity_ctx")) {
                mContextMenuEntity = ecs::NkEntityId::Invalid();
                return;
            }

            if (NkUIMenu::MenuItem(ctx, dl, font, "Renommer", "F2")) {
                mRenamingEntity = id;
                const ecs::NkSceneNode* n = world.Get<ecs::NkSceneNode>(id);
                if (n) NkStrNCpy(mRenameBuffer, n->name, 63);
                else mRenameBuffer[0] = '\0';
                mContextMenuEntity = ecs::NkEntityId::Invalid();
            }

            if (NkUIMenu::MenuItem(ctx, dl, font, "Dupliquer", "Ctrl+D")) {
                if (scene) {
                    ecs::NkEntityId dup = scene->SpawnNode("Entity_dup");
                    sel.Select(dup);
                }
                mContextMenuEntity = ecs::NkEntityId::Invalid();
            }

            NkUIMenu::Separator(ctx, dl);

            if (NkUIMenu::MenuItem(ctx, dl, font, "Supprimer", "Suppr")) {
                if (scene) scene->DestroyRecursive(id);
                sel.Clear();
                mContextMenuEntity = ecs::NkEntityId::Invalid();
            }

            NkUIMenu::EndContextMenu(ctx);
        }

        bool SceneTreePanel::IsOpen(ecs::NkEntityId id) const noexcept {
            for (nk_uint32 i = 0; i < mOpenCount; ++i)
                if (mOpenNodes[i] == id) return true;
            return false;
        }

        void SceneTreePanel::SetOpen(ecs::NkEntityId id, bool open) noexcept {
            if (open) {
                if (!IsOpen(id) && mOpenCount < kMaxOpen)
                    mOpenNodes[mOpenCount++] = id;
            } else {
                for (nk_uint32 i = 0; i < mOpenCount; ++i) {
                    if (mOpenNodes[i] == id) {
                        mOpenNodes[i] = mOpenNodes[--mOpenCount];
                        return;
                    }
                }
            }
        }

    } // namespace Unkeny
} // namespace nkentseu
