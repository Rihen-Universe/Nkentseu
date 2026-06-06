// =============================================================================
// NkShaderAnnotations.cpp
// Implementation du preprocesseur d'annotations semantiques pour GLSL Vulkan.
// =============================================================================
#include "NKSL/ShaderConvert/NkShaderAnnotations.h"

#include "NKLogger/NkLog.h"

#include <cstring>
#include <cctype>

namespace nkentseu {

    // =========================================================================
    // Helpers de tokenisation
    // =========================================================================

    static bool IsIdentStart(char c) { return (c=='_' || (c>='a'&&c<='z') || (c>='A'&&c<='Z')); }
    static bool IsIdentCont (char c) { return IsIdentStart(c) || (c>='0'&&c<='9'); }
    static bool IsSpaceCh   (char c) { return c==' ' || c=='\t' || c=='\r'; }

    // Avance i en sautant les espaces (pas les retours ligne, pour preserver le compte)
    static void SkipSpaces(const char* s, uint32 len, uint32& i) {
        while (i < len && IsSpaceCh(s[i])) ++i;
    }

    // Lit un identifiant a partir de i, avance i a la fin.
    static NkString ReadIdent(const char* s, uint32 len, uint32& i) {
        uint32 start = i;
        while (i < len && IsIdentCont(s[i])) ++i;
        return NkString(s + start, i - start);
    }

    // =========================================================================
    // Resolution kind a partir du nom d'annotation (sans @)
    // =========================================================================
    static bool ResolveKind(const NkString& name, NkShaderAnnotationKind& out) {
        if (name == "param")        { out = NkShaderAnnotationKind::NK_PARAM;         return true; }
        if (name == "color")        { out = NkShaderAnnotationKind::NK_COLOR;         return true; }
        if (name == "range")        { out = NkShaderAnnotationKind::NK_RANGE;         return true; }
        if (name == "texture2D")    { out = NkShaderAnnotationKind::NK_TEXTURE_2D;    return true; }
        if (name == "cubemap")      { out = NkShaderAnnotationKind::NK_TEXTURE_CUBE;  return true; }
        if (name == "texture3D")    { out = NkShaderAnnotationKind::NK_TEXTURE_3D;    return true; }
        if (name == "textureArray") { out = NkShaderAnnotationKind::NK_TEXTURE_ARRAY; return true; }
        if (name == "enum")         { out = NkShaderAnnotationKind::NK_ENUM;          return true; }
        if (name == "group")        { out = NkShaderAnnotationKind::NK_GROUP;         return true; }
        if (name == "hidden")       { out = NkShaderAnnotationKind::NK_HIDDEN;        return true; }
        if (name == "material")     { out = NkShaderAnnotationKind::NK_MATERIAL;      return true; }
        if (name == "stage")        { out = NkShaderAnnotationKind::NK_STAGE;         return true; }
        if (name == "entry")        { out = NkShaderAnnotationKind::NK_ENTRY;         return true; }
        // Alias types natifs
        if (name == "float")        { out = NkShaderAnnotationKind::NK_TYPE_FLOAT;    return true; }
        if (name == "int")          { out = NkShaderAnnotationKind::NK_TYPE_INT;      return true; }
        if (name == "bool")         { out = NkShaderAnnotationKind::NK_TYPE_BOOL;     return true; }
        if (name == "vec2")         { out = NkShaderAnnotationKind::NK_TYPE_VEC2;     return true; }
        if (name == "vec3")         { out = NkShaderAnnotationKind::NK_TYPE_VEC3;     return true; }
        if (name == "vec4")         { out = NkShaderAnnotationKind::NK_TYPE_VEC4;     return true; }
        if (name == "ivec2")        { out = NkShaderAnnotationKind::NK_TYPE_IVEC2;    return true; }
        if (name == "ivec3")        { out = NkShaderAnnotationKind::NK_TYPE_IVEC3;    return true; }
        if (name == "ivec4")        { out = NkShaderAnnotationKind::NK_TYPE_IVEC4;    return true; }
        if (name == "mat3")         { out = NkShaderAnnotationKind::NK_TYPE_MAT3;     return true; }
        if (name == "mat4")         { out = NkShaderAnnotationKind::NK_TYPE_MAT4;     return true; }
        return false;
    }

    // Helper : pre-remplit le glslType pour les alias type natifs.
    // Apres ApplyArgs, ces alias deviennent equivalents a @param(type=X, ...).
    static const char* GlslTypeForKind(NkShaderAnnotationKind k) {
        switch (k) {
            case NkShaderAnnotationKind::NK_TYPE_FLOAT: return "float";
            case NkShaderAnnotationKind::NK_TYPE_INT:   return "int";
            case NkShaderAnnotationKind::NK_TYPE_BOOL:  return "bool";
            case NkShaderAnnotationKind::NK_TYPE_VEC2:  return "vec2";
            case NkShaderAnnotationKind::NK_TYPE_VEC3:  return "vec3";
            case NkShaderAnnotationKind::NK_TYPE_VEC4:  return "vec4";
            case NkShaderAnnotationKind::NK_TYPE_IVEC2: return "ivec2";
            case NkShaderAnnotationKind::NK_TYPE_IVEC3: return "ivec3";
            case NkShaderAnnotationKind::NK_TYPE_IVEC4: return "ivec4";
            case NkShaderAnnotationKind::NK_TYPE_MAT3:  return "mat3";
            case NkShaderAnnotationKind::NK_TYPE_MAT4:  return "mat4";
            default:                                    return nullptr;
        }
    }

    // =========================================================================
    // Parser argument (raw "key=value" ou "value" positionnel)
    // Supporte : strings "foo", tuples (1,2,3), nombres, identifiants, bool
    // =========================================================================
    struct ArgKV {
        NkString                key;     // vide si positionnel
        NkShaderAnnotationValue val;
    };

    // Parse une valeur isolee (string / nombre / tuple / ident).
    // Retourne true si parsing OK, false sinon.
    static bool ParseValue(const char* s, uint32 len, uint32& i, NkShaderAnnotationValue& out) {
        SkipSpaces(s, len, i);
        if (i >= len) return false;

        // String literale "..."
        if (s[i] == '"') {
            ++i;
            uint32 start = i;
            while (i < len && s[i] != '"' && s[i] != '\n') ++i;
            if (i >= len || s[i] != '"') return false;
            out.type = NkShaderAnnotationValue::Type::NK_STRING;
            out.s    = NkString(s + start, i - start);
            ++i; // skip "
            return true;
        }

        // Tuple (a, b, c, d) -> vec2/vec3/vec4
        if (s[i] == '(') {
            ++i;
            float32 vals[4] = {0,0,0,0};
            uint32 count = 0;
            while (count < 4 && i < len) {
                SkipSpaces(s, len, i);
                // Lire un nombre
                uint32 numStart = i;
                if (s[i] == '-' || s[i] == '+') ++i;
                while (i < len && (s[i]=='.' || (s[i]>='0'&&s[i]<='9'))) ++i;
                if (numStart == i) break;
                NkString numStr(s + numStart, i - numStart);
                vals[count++] = (float32)atof(numStr.CStr());
                SkipSpaces(s, len, i);
                if (i < len && s[i] == ',') { ++i; continue; }
                break;
            }
            SkipSpaces(s, len, i);
            if (i < len && s[i] == ')') ++i;
            out.f[0] = vals[0]; out.f[1] = vals[1]; out.f[2] = vals[2]; out.f[3] = vals[3];
            if      (count == 2) out.type = NkShaderAnnotationValue::Type::NK_VEC2;
            else if (count == 3) out.type = NkShaderAnnotationValue::Type::NK_VEC3;
            else if (count == 4) out.type = NkShaderAnnotationValue::Type::NK_VEC4;
            else { out.type = NkShaderAnnotationValue::Type::NK_FLOAT; out.f[0] = vals[0]; }
            return true;
        }

        // Nombre
        if (s[i] == '-' || s[i] == '+' || (s[i] >= '0' && s[i] <= '9') || s[i] == '.') {
            uint32 numStart = i;
            bool isFloat = false;
            if (s[i] == '-' || s[i] == '+') ++i;
            while (i < len && (s[i]=='.' || (s[i]>='0'&&s[i]<='9'))) {
                if (s[i] == '.') isFloat = true;
                ++i;
            }
            NkString numStr(s + numStart, i - numStart);
            if (isFloat) {
                out.type = NkShaderAnnotationValue::Type::NK_FLOAT;
                out.f[0] = (float32)atof(numStr.CStr());
            } else {
                out.type = NkShaderAnnotationValue::Type::NK_INT;
                out.i    = atoi(numStr.CStr());
            }
            return true;
        }

        // Identifiant : true / false / nom de type GLSL / valeur enum
        if (IsIdentStart(s[i])) {
            NkString ident = ReadIdent(s, len, i);
            if (ident == "true")  { out.type = NkShaderAnnotationValue::Type::NK_BOOL;   out.b = true;  return true; }
            if (ident == "false") { out.type = NkShaderAnnotationValue::Type::NK_BOOL;   out.b = false; return true; }
            out.type = NkShaderAnnotationValue::Type::NK_STRING;
            out.s    = ident;
            return true;
        }

        return false;
    }

    // Parse arguments d'annotation : (key=val, key=val, ...) ou (val, val, ...)
    // i pointe sur '(' au debut. A la fin, i pointe juste apres ')'.
    static bool ParseArgList(const char* s, uint32 len, uint32& i, NkVector<ArgKV>& out) {
        if (i >= len || s[i] != '(') return true;  // pas d'args = OK
        ++i;
        while (i < len) {
            SkipSpaces(s, len, i);
            if (i < len && s[i] == ')') { ++i; return true; }

            ArgKV arg;
            // Detecter key=value : on regarde si on a un ident suivi de '='
            uint32 save = i;
            if (IsIdentStart(s[i])) {
                NkString maybeKey = ReadIdent(s, len, i);
                SkipSpaces(s, len, i);
                if (i < len && s[i] == '=') {
                    ++i;  // skip =
                    arg.key = maybeKey;
                    if (!ParseValue(s, len, i, arg.val)) return false;
                } else {
                    // Pas un key=value, c'etait une valeur positionnelle (ident)
                    i = save;
                    if (!ParseValue(s, len, i, arg.val)) return false;
                }
            } else {
                if (!ParseValue(s, len, i, arg.val)) return false;
            }
            out.PushBack(arg);
            SkipSpaces(s, len, i);
            if (i < len && s[i] == ',') { ++i; continue; }
            if (i < len && s[i] == ')') { ++i; return true; }
            return false;
        }
        return false;
    }

    // =========================================================================
    // Application des arguments parses sur une annotation generique
    // (positionnel : name est le premier arg si non-keyword)
    // =========================================================================
    static void ApplyArgs(const NkVector<ArgKV>& args, NkShaderAnnotation& a) {
        for (uint32 idx = 0; idx < args.Size(); ++idx) {
            const ArgKV& kv = args[idx];
            // Mapping des keys nommees
            if (kv.key == "name") {
                if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) a.name = kv.val.s;
            }
            else if (kv.key == "label")   { if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) a.label   = kv.val.s; }
            else if (kv.key == "tooltip") { if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) a.tooltip = kv.val.s; }
            else if (kv.key == "group")   { if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) a.group   = kv.val.s; }
            else if (kv.key == "min")     { a.minValue = kv.val; }
            else if (kv.key == "max")     { a.maxValue = kv.val; }
            else if (kv.key == "default") { a.defaultValue = kv.val; }
            else if (kv.key == "slot")    { if (kv.val.type == NkShaderAnnotationValue::Type::NK_INT) a.slot = (uint32)kv.val.i; }
            else if (kv.key == "srgb")    { if (kv.val.type == NkShaderAnnotationValue::Type::NK_BOOL) a.srgb = kv.val.b; }
            else if (kv.key == "type")    { if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) a.glslType = kv.val.s; }
            else if (kv.key == "values") {
                // @enum(values=(a,b,c)) -> parse comme tuple ? Notre parser le voit comme
                // vec2/3/4 si numerique. Pour strings, ParseValue le voit comme string
                // si premier char est ident. On supporte la forme :
                //   @enum(values=("A","B","C"))  -> parse tuple de strings ? Pas supporte
                // Workaround pour v1 : list comma-separated dans une seule string.
                if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) {
                    // Split sur ',' (simple)
                    const NkString& s = kv.val.s;
                    NkString cur;
                    for (uint32 k = 0; k <= s.Size(); ++k) {
                        char c = (k < s.Size()) ? s.CStr()[k] : ',';
                        if (c == ',') { if (!cur.Empty()) { a.enumValues.PushBack(cur); cur.Clear(); } }
                        else if (c != ' ' && c != '"') { cur += NkString(&c, 1); }
                    }
                }
            }
            else if (kv.key == "path" || kv.key == "default_path") {
                if (kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) a.defaultPath = kv.val.s;
            }
            else if (kv.key.Empty()) {
                // Positionnel : par convention le 1er positionnel = name
                if (idx == 0 && a.name.Empty() && kv.val.type == NkShaderAnnotationValue::Type::NK_STRING) {
                    a.name = kv.val.s;
                }
            }
        }
    }

    // =========================================================================
    // Parser principal
    // =========================================================================
    NkShaderAnnotationResult NkShaderAnnotationParser::Parse(
        const NkString& rawSource, NkSLStage hintStage)
    {
        NkShaderAnnotationResult res;
        res.metadata.stage         = hintStage;
        res.metadata.stageExplicit = false;
        res.metadata.entryPoint    = "main";

        const char* s   = rawSource.CStr();
        const uint32 len = rawSource.Size();

        // Output buffer : on remplace les annotations par des newlines pour
        // preserver les line numbers (les erreurs glslang resteront alignees).
        NkString out;
        out.Reserve(len);

        uint32 i = 0;
        uint32 currentLine = 1;
        NkString currentGroup;  // @group dernier en cours

        while (i < len) {
            // Detecter @ au debut d'une annotation (apres espaces sur la ligne)
            // Verifier que '@' n'est pas dans une string ou commentaire :
            // pour v1 on assume que les annotations sont en debut de ligne
            // (apres optionnels espaces).
            if (s[i] == '\n') { out += "\n"; ++i; ++currentLine; continue; }

            // Sur debut de ligne, on tente de lire une annotation
            bool atLineStart = (out.Size() == 0) || out.CStr()[out.Size() - 1] == '\n';
            if (atLineStart) {
                uint32 j = i;
                SkipSpaces(s, len, j);
                if (j < len && s[j] == '@') {
                    ++j;
                    if (j < len && IsIdentStart(s[j])) {
                        NkString annotName = ReadIdent(s, len, j);
                        NkShaderAnnotationKind kind;
                        if (!ResolveKind(annotName, kind)) {
                            // Annotation inconnue : on ne strip pas, on la garde (peut etre du code legit)
                            // Continue normal copy
                        } else {
                            SkipSpaces(s, len, j);
                            NkVector<ArgKV> args;
                            if (j < len && s[j] == '(') {
                                if (!ParseArgList(s, len, j, args)) {
                                    res.errors += "Annotation @";
                                    res.errors += annotName;
                                    res.errors += " : args malformes\n";
                                }
                            }
                            // Construire l'annotation
                            NkShaderAnnotation a;
                            a.kind       = kind;
                            a.sourceLine = currentLine;
                            a.group      = currentGroup;
                            // Pre-remplir glslType pour les alias type natifs.
                            // Si l'utilisateur passe aussi type="vec3" en arg, ApplyArgs
                            // l'ecrasera (mais ce sera la meme valeur).
                            if (const char* nativeT = GlslTypeForKind(kind)) {
                                a.glslType = NkString(nativeT);
                            }
                            ApplyArgs(args, a);

                            // Traitements speciaux
                            if (kind == NkShaderAnnotationKind::NK_MATERIAL) {
                                if (!a.name.Empty()) res.metadata.materialName = a.name;
                            }
                            else if (kind == NkShaderAnnotationKind::NK_GROUP) {
                                // @group("foo") modifie le groupe courant pour les annotations suivantes
                                if (!a.name.Empty()) currentGroup = a.name;
                                else if (!args.Empty() && args[0].val.type == NkShaderAnnotationValue::Type::NK_STRING) {
                                    currentGroup = args[0].val.s;
                                }
                            }
                            else if (kind == NkShaderAnnotationKind::NK_STAGE) {
                                // @stage(vertex|fragment|compute|geometry|tess_ctrl|tess_eval)
                                if (!a.name.Empty()) {
                                    NkString stg = a.name;
                                    if      (stg == "vertex")    res.metadata.stage = NkSLStage::NK_VERTEX;
                                    else if (stg == "fragment")  res.metadata.stage = NkSLStage::NK_FRAGMENT;
                                    else if (stg == "compute")   res.metadata.stage = NkSLStage::NK_COMPUTE;
                                    else if (stg == "geometry")  res.metadata.stage = NkSLStage::NK_GEOMETRY;
                                    res.metadata.stageExplicit = true;
                                }
                            }
                            else if (kind == NkShaderAnnotationKind::NK_ENTRY) {
                                if (!a.name.Empty()) res.metadata.entryPoint = a.name;
                            }
                            else {
                                res.metadata.annotations.PushBack(a);
                            }

                            // Skip jusqu'a la fin de ligne (replace par newline pour line preservation)
                            // j est apres ')'. Si reste du contenu sur la ligne ne soit pas un commentaire,
                            // c'est une erreur, mais pour v1 on tolere.
                            while (j < len && s[j] != '\n') ++j;
                            // Emit un newline pour preserver la line number
                            out += "\n";
                            i = j;
                            if (i < len && s[i] == '\n') { ++i; ++currentLine; }
                            continue;
                        }
                    }
                }
            }

            // Copy normal : preserver les newlines (compte des lignes)
            if (s[i] == '\n') { out += "\n"; ++i; ++currentLine; continue; }
            out += NkString(s + i, 1);
            ++i;
        }

        res.cleanSource = out;
        res.success     = true;
        return res;
    }

    NkString NkShaderAnnotationParser::StripAnnotations(const NkString& rawSource) {
        return Parse(rawSource).cleanSource;
    }

} // namespace nkentseu
