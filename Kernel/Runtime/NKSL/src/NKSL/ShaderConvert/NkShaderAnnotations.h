#pragma once
// =============================================================================
// NkShaderAnnotations.h
//
// Preprocesseur d'annotations semantiques pour shaders GLSL Vulkan natif.
//
// Permet a l'utilisateur d'enrichir un shader GLSL VK avec des metadonnees
// haut niveau qui seront lues par l'UI editeur de materiaux, MAIS qui sont
// STRIPPEES avant la compilation glslang (pour rester en GLSL VK valide).
//
// CONVENTION : un fichier par stage (.vert.vk.glsl, .frag.vk.glsl, .comp.vk.glsl).
// Le stage est auto-deduit du nom de fichier. L'annotation @stage() est
// optionnelle (override explicite).
//
// EXEMPLE D'USAGE :
//   @material(name="PBR")
//
//   @param(name="metallic", type=float, min=0, max=1, default=0.0, label="Metallic")
//   @param(name="roughness", type=float, min=0, max=1, default=0.5, group="Surface")
//   @color(name="tint", default=(1,1,1,1), srgb=true)
//   @texture2D(name="diffuse", slot=0, label="Albedo Map", default="white.png")
//   @cubemap(name="env", slot=8, label="Environment")
//   @enum(name="shadingMode", values=("Standard","Anisotropic","ClearCoat"),
//         default="Standard", group="Advanced")
//   @group("Surface")  // section UI suivante
//   @hidden(name="debugFlag")
//
//   layout(set=1, binding=1) uniform ObjUBO { ... } uObj;
//   // ... reste du shader GLSL VK natif (preserve a l'identique)
//
// PIPELINE COMPLET :
//   1. NkShaderAnnotationParser::Parse(rawSource)
//        -> NkShaderAnnotationResult { cleanSource, metadata, errors }
//   2. cleanSource passe a glslang (compile en SPIR-V)
//   3. metadata stocke par NkShaderLibrary, expose plus tard a l'UI
//
// VOLONTAIREMENT NON-SUPPORTE :
//   - Multi-stage dans un seul fichier (@vertex {} / @fragment {}) :
//     convention = 1 fichier par stage.
//   - Logique de codegen / transformation de code : c'est le job de
//     NkShaderConverter (glslang + SPIRV-Cross), pas du preprocesseur.
// =============================================================================
#include "NKSL/NkSLTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {

    // =========================================================================
    // Types d'annotations
    //
    // @param est la forme generique : @param(name="x", type=float|vec3|..., ...).
    // Alias typés (plus concis dans les shaders) : @float, @int, @bool, @vec2,
    // @vec3, @vec4, @mat3, @mat4, @ivec2, @ivec3, @ivec4. Tous remplissent
    // glslType automatiquement, le reste des arguments (name, min, max, default,
    // group, label) est identique a @param.
    //
    // Exemples :
    //   @float(name="metallic", min=0, max=1, default=0.0, label="Metallic")
    //   @vec3 (name="tint",    default=(1,1,1))
    //   @mat4 (name="custom")
    //   @color(name="albedo",  default=(1,1,1), srgb=true)
    //   @bool (name="useNormalMap", default=true)
    // =========================================================================
    enum class NkShaderAnnotationKind : uint32 {
        NK_PARAM,          // @param sur uniform numerique generique
        NK_COLOR,          // @color sur uniform vec3/vec4
        NK_RANGE,          // @range sur uniform numerique (slider sans default-edit)
        NK_TEXTURE_2D,     // @texture2D sur sampler2D
        NK_TEXTURE_CUBE,   // @cubemap sur samplerCube
        NK_TEXTURE_3D,     // @texture3D sur sampler3D
        NK_TEXTURE_ARRAY,  // @textureArray sur sampler2DArray
        NK_ENUM,           // @enum sur const int (dropdown UI)
        NK_GROUP,          // @group("section") - separateur UI
        NK_HIDDEN,         // @hidden marque un param a ne pas exposer
        NK_MATERIAL,       // @material(name=) header du shader
        NK_STAGE,          // @stage(vertex|fragment|compute) override explicite
        NK_ENTRY,          // @entry("main") override point d'entree
        // ── Alias types natifs (= @param + glslType pre-rempli) ─────────────
        NK_TYPE_FLOAT,
        NK_TYPE_INT,
        NK_TYPE_BOOL,
        NK_TYPE_VEC2,
        NK_TYPE_VEC3,
        NK_TYPE_VEC4,
        NK_TYPE_IVEC2,
        NK_TYPE_IVEC3,
        NK_TYPE_IVEC4,
        NK_TYPE_MAT3,
        NK_TYPE_MAT4,
    };

    // =========================================================================
    // Valeur typee (pour default values, min, max, etc.)
    // =========================================================================
    struct NkShaderAnnotationValue {
        enum class Type : uint32 { NK_NONE, NK_INT, NK_FLOAT, NK_VEC2, NK_VEC3, NK_VEC4, NK_STRING, NK_BOOL };
        Type    type = Type::NK_NONE;
        float32 f[4] = {0, 0, 0, 0};   // vecN values (f[0] = scalaire)
        int32   i    = 0;
        bool    b    = false;
        NkString s;                     // strings + paths
    };

    // =========================================================================
    // Annotation parsee (forme generique, kind discrimine les champs utiles)
    // =========================================================================
    struct NkShaderAnnotation {
        NkShaderAnnotationKind kind = NkShaderAnnotationKind::NK_PARAM;
        NkString               name;            // ex: "metallic"
        NkString               label;           // ex: "Metallic"
        NkString               tooltip;
        NkString               group;           // ex: "Surface"

        // Type GLSL associe (deduit du shader, ex: "float", "vec3", "sampler2D")
        NkString               glslType;

        // Range / defaults pour params numeriques
        NkShaderAnnotationValue defaultValue;
        NkShaderAnnotationValue minValue;
        NkShaderAnnotationValue maxValue;

        // Texture slot (pour @texture2D/@cubemap/...)
        uint32                 slot          = 0;
        NkString               defaultPath;
        bool                   srgb          = false;  // @color srgb option

        // Enum values
        NkVector<NkString>     enumValues;

        // Source line (pour erreurs)
        uint32                 sourceLine    = 0;
    };

    // =========================================================================
    // Metadata d'un shader (resultat global du parsing)
    // =========================================================================
    struct NkShaderMetadata {
        NkString                    materialName;     // de @material(name="...")
        NkSLStage                   stage = NkSLStage::NK_VERTEX;
        bool                        stageExplicit = false; // true si @stage() present
        NkString                    entryPoint = "main";

        NkVector<NkShaderAnnotation> annotations;     // toutes les annotations parsees

        // Helpers
        const NkShaderAnnotation* FindByName(const NkString& n) const {
            for (const auto& a : annotations) if (a.name == n) return &a;
            return nullptr;
        }
    };

    // =========================================================================
    // Resultat du preprocessing
    // =========================================================================
    struct NkShaderAnnotationResult {
        bool             success = false;
        NkString         cleanSource;   // source GLSL VK avec annotations strippees
        NkShaderMetadata metadata;
        NkString         errors;
    };

    // =========================================================================
    // Parser d'annotations
    // =========================================================================
    class NkShaderAnnotationParser {
        public:
            // Parse un source GLSL VK avec annotations semantiques @xxx.
            // Retourne :
            //   - cleanSource : source GLSL VK sans les @xxx (mais avec lignes preservees
            //     pour que les line numbers d'erreurs glslang restent valides)
            //   - metadata : annotations extraites + materialName/stage/entry
            //   - errors : diagnostics (warnings non-bloquants si annotation mal formee)
            //
            // hintStage : stage suggere depuis le nom de fichier (.vert/.frag/.comp).
            //             Override par @stage() si present.
            static NkShaderAnnotationResult Parse(const NkString& rawSource, NkSLStage hintStage = NkSLStage::NK_VERTEX);

            // Strip uniquement (sans extraction de metadata).
            // Plus rapide quand on s'en moque (compile only).
            static NkString StripAnnotations(const NkString& rawSource);
    };

} // namespace nkentseu
