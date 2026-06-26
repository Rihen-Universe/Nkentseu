#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorInspector.h
// @Brief   Inspecteur GENERIQUE pilote par NKReflection (PropertyGrid).
// @Author  Rihen
// @License Proprietary - Free to use and modify
//
// editorkit::DrawInspector(ctx, obj, cls) genere, pour chaque propriete editable
// d'une classe reflechie, le widget NKGui adapte a sa categorie (label | champ),
// et reecrit la valeur en live (SetPropertyByName). Sous-objets NK_CLASS rendus
// recursivement en sections repliables ; enums en combo ; readOnly grise.
//
// HEADER-ONLY + OPT-IN : NON inclus par l'umbrella NkEditorKit.h, pour ne PAS
// forcer la dependance NKReflection sur les editeurs qui n'en veulent pas. Un
// editeur qui inspecte des objets reflechis inclut CE header + ajoute la dep
// `NKReflection` a son .jenga.
// -----------------------------------------------------------------------------

#include "NKGui/NKGui.h"
#include "NKReflection/NkRegistry.h"
#include "NKReflection/NkInspector.h"
#include "NKReflection/NkReflectVariant.h"
#include "NKReflection/NkEnumDescriptor.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace editorkit {

        // Dessine un PropertyGrid editable pour `obj` (instance de `cls` reflechie).
        inline void DrawInspector(nkgui::NkGuiContext& ctx, void* obj, const reflection::NkClass* cls) {
            using namespace nkentseu::nkgui;
            using namespace nkentseu::reflection;
            if (!cls || !obj) return;

            NkVector<NkEditableProperty> props = EnumerateEditableProperties(cls, obj);
            const float32 colsW[] = { -2.f, -3.f };   // colonne LIBELLE (poids 2) | CHAMP (poids 3)

            for (usize i = 0; i < props.Size(); ++i) {
                const NkEditableProperty& p = props[i];
                if (p.hidden) continue;
                const char* lbl = p.displayName ? p.displayName : p.name;

                if (p.category == NkTypeCategory::NK_CLASS) {          // sous-objet -> section recursive
                    const NkProperty* prop = cls->GetProperty(p.name);
                    void*           sub    = prop ? prop->GetValuePtr(obj) : nullptr;
                    const NkClass*  subCls = p.type->GetClass();
                    if (sub && subCls && CollapsingHeader(ctx, lbl)) DrawInspector(ctx, sub, subCls);
                    continue;
                }

                if (p.readOnly) ctx.BeginDisabled(true);
                ctx.PushId(p.name);                                   // id unique -> widgets a label "" (id-only)
                BeginRow(ctx, 0.f, colsW, 2);
                Text(ctx, lbl);
                switch (p.category) {
                    case NkTypeCategory::NK_BOOL: {
                        bool v = p.value.ToBool();
                        if (Checkbox(ctx, "", v)) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<bool>(v));
                        break;
                    }
                    case NkTypeCategory::NK_FLOAT32:
                    case NkTypeCategory::NK_FLOAT64: {
                        float32 v = static_cast<float32>(p.value.ToFloat64());
                        const bool ch = p.hasRange ? SliderFloat(ctx, "", v, p.rangeMin, p.rangeMax)
                                                   : DragFloat(ctx, "", v, 0.05f);
                        if (ch) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<float32>(v));
                        break;
                    }
                    case NkTypeCategory::NK_INT8:  case NkTypeCategory::NK_INT16:
                    case NkTypeCategory::NK_INT32: case NkTypeCategory::NK_INT64:
                    case NkTypeCategory::NK_UINT8: case NkTypeCategory::NK_UINT16:
                    case NkTypeCategory::NK_UINT32: case NkTypeCategory::NK_UINT64: {
                        int32 v = static_cast<int32>(p.value.ToInt64());
                        const bool ch = p.hasRange ? DragInt(ctx, "", v, 0.25f, static_cast<int32>(p.rangeMin), static_cast<int32>(p.rangeMax))
                                                   : DragInt(ctx, "", v);
                        if (ch) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<int32>(v));
                        break;
                    }
                    case NkTypeCategory::NK_STRING: {
                        char buf[256] = {};
                        NkString s; p.value.Get<NkString>(s);
                        const char* csr = s.CStr();
                        int32 n = 0; while (csr && csr[n] && n < 255) { buf[n] = csr[n]; ++n; }
                        buf[n] = '\0';
                        if (InputText(ctx, "", buf, 256)) SetPropertyByName(cls, obj, p.name, NkReflectVariant::From<NkString>(NkString(buf)));
                        break;
                    }
                    case NkTypeCategory::NK_ENUM: {
                        const NkEnumDescriptor* ed = NkRegistry::Get().FindEnum(p.type->GetName());
                        const int64 cur = p.value.ToInt64();
                        const char* curName = ed ? ed->ToName(cur) : nullptr;
                        if (BeginCombo(ctx, "", curName ? curName : "?", ed ? static_cast<int32>(ed->GetCount()) : 0)) {
                            if (ed) for (usize j = 0; j < ed->GetCount(); ++j) {
                                const NkEnumEntry& e = ed->GetEntryAt(j);
                                if (Selectable(ctx, e.name, e.value == cur)) {
                                    int32 ev = static_cast<int32>(e.value);
                                    SetPropertyByName(cls, obj, p.name, NkReflectVariant::FromRaw(p.type, &ev));
                                    ctx.ClosePopup();
                                }
                            }
                            EndCombo(ctx);
                        }
                        break;
                    }
                    default:
                        Text(ctx, p.isContainer ? "(liste)" : "(non gere)");
                        break;
                }
                EndRow(ctx);
                ctx.PopId();
                if (p.readOnly) ctx.EndDisabled();
            }
        }

        // Surcharge typee : deduit la classe via T::GetStaticClass().
        template <typename T>
        inline void DrawInspector(nkgui::NkGuiContext& ctx, T& obj) {
            DrawInspector(ctx, &obj, &T::GetStaticClass());
        }

    } // namespace editorkit
} // namespace nkentseu
