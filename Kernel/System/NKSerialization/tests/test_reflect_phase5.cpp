// =============================================================================
// tests/test_reflect_phase5.cpp
// =============================================================================
// Tests Phase 5 du chantier NKReflection :
//   1. API INSPECTEUR     : EnumerateEditableProperties + Set/GetPropertyByName
//   2. INVOCATION METHODES: MakeFromMember N-args + InvokeVariant generique
//   3. SERIALISATION PROF.: Transform{Vec3,Quat,Vec3} round-trip JSON
// Standalone (sans framework externe), sur le modele de test_reflect_phase3.cpp.
// =============================================================================

#include <cstdio>
#include <cstring>

#include "NKReflection/NkRegistry.h"
#include "NKReflection/NkInspector.h"
#include "NKReflection/NkMethod.h"
#include "NKReflection/NkMathReflect.h"
#include "NKReflection/NkReflectVariant.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/Reflection/NkReflectSerializer.h"

#include "NKMath/NkVec.h"
#include "NKMath/NkQuat.h"

using namespace nkentseu;
using namespace nkentseu::reflection;
using namespace nkentseu::math;   // NkVec2f/3f/4f, NkQuatf, NkMat4f, NkColor(F)

// ─── helpers ─────────────────────────────────────────────────────────────────
static int s_pass = 0;
static int s_fail = 0;

#define EXPECT_TRUE(expr) do { if (!(expr)) { \
    printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #expr); ++s_fail; \
} else { ++s_pass; } } while(0)

#define EXPECT_EQ(a, b)  EXPECT_TRUE((a) == (b))
#define EXPECT_STR(a, b) EXPECT_TRUE((a) != nullptr && strcmp((a),(b)) == 0)
#define EXPECT_NEAR(a, b) EXPECT_TRUE(((a)-(b) < 0.001f) && ((b)-(a) < 0.001f))

// =============================================================================
// ENUM DE TEST (categorie NK_ENUM)
// =============================================================================
enum class WeaponType : nk_uint8 { Sword = 0, Bow = 1, Staff = 2 };
NKENTSEU_REFLECT_ENUM(WeaponType, Sword, Bow, Staff)

// =============================================================================
// CLASSE INSPECTEUR : props variees (range / tooltip / enum / hidden / readonly)
// =============================================================================
class Character {
    NKENTSEU_REFLECT_CLASS(Character)
public:
    nk_int32 level;                 // int avec range
    NKENTSEU_REFLECT_PROPERTY(level)
public:
    nk_float32 stamina;             // float simple
    NKENTSEU_REFLECT_PROPERTY(stamina)
public:
    NkString displayName;           // string avec tooltip
    NKENTSEU_REFLECT_PROPERTY(displayName)
public:
    WeaponType weapon;              // enum
    NKENTSEU_REFLECT_PROPERTY(weapon)
public:
    // Propriete CACHEE de l'editeur (ne doit pas apparaitre dans l'enumeration).
    NKENTSEU_PROPERTY_FLAGS(nk_int32, internalId, NK_REFLECT_HIDE_EDITOR)
public:
    // Propriete READ-ONLY (apparait mais readOnly==true, SetPropertyByName echoue).
    NKENTSEU_PROPERTY_FLAGS(nk_int32, hashCode, NK_REFLECT_READONLY)
public:
    Character()
        : level(1), stamina(50.0f), displayName("Hero"),
          weapon(WeaponType::Sword), internalId(999), hashCode(0xABCD) {}
};

static void WireCharacterMeta() {
    const auto& cls = Character::GetStaticClass();
    NkRegistry::Get().RegisterClass(&cls);

    NkProperty& levelProp = const_cast<NkProperty&>(*cls.GetProperty("level"));
    levelProp.SetRange(1.0f, 99.0f);
    levelProp.SetCategory("Stats");

    NkProperty& nameProp = const_cast<NkProperty&>(*cls.GetProperty("displayName"));
    nameProp.SetTooltip("Nom affiche du personnage");
    nameProp.SetDisplayName("Nom");
}

// =============================================================================
// TEST 1 : INSPECTEUR
// =============================================================================
static const NkEditableProperty* FindProp(const NkVector<NkEditableProperty>& v,
                                          const char* name) {
    for (nk_usize i = 0; i < v.Size(); ++i) {
        if (v[i].name && strcmp(v[i].name, name) == 0) {
            return &v[i];
        }
    }
    return nullptr;
}

static void TestInspector() {
    printf("[TEST] Inspecteur : EnumerateEditableProperties\n");

    Character c;
    NkVector<NkEditableProperty> props = EnumerateEditableProperties(c);

    // 'internalId' (HIDE_EDITOR) doit etre ABSENT. Les autres presents :
    // level, stamina, displayName, weapon, hashCode = 5.
    EXPECT_TRUE(FindProp(props, "internalId") == nullptr);
    EXPECT_TRUE(FindProp(props, "level") != nullptr);
    EXPECT_TRUE(FindProp(props, "hashCode") != nullptr);
    EXPECT_EQ(props.Size(), (nk_usize)5);

    // -- level : valeur courante, range, categorie, non readonly --
    const NkEditableProperty* lvl = FindProp(props, "level");
    if (lvl) {
        EXPECT_EQ(lvl->category, NkTypeCategory::NK_INT32);
        EXPECT_EQ(lvl->value.ToInt64(), (nk_int64)1);
        EXPECT_TRUE(lvl->hasRange);
        EXPECT_NEAR(lvl->rangeMin, 1.0f);
        EXPECT_NEAR(lvl->rangeMax, 99.0f);
        EXPECT_STR(lvl->group, "Stats");
        EXPECT_TRUE(!lvl->readOnly);
        EXPECT_TRUE(!lvl->hidden);
    }

    // -- displayName : tooltip + displayName --
    const NkEditableProperty* nm = FindProp(props, "displayName");
    if (nm) {
        EXPECT_EQ(nm->category, NkTypeCategory::NK_STRING);
        EXPECT_STR(nm->tooltip, "Nom affiche du personnage");
        EXPECT_STR(nm->displayName, "Nom");
        EXPECT_TRUE(nm->value.ToString() == NkString("Hero"));
    }

    // -- weapon : enum --
    const NkEditableProperty* wp = FindProp(props, "weapon");
    if (wp) {
        EXPECT_EQ(wp->category, NkTypeCategory::NK_ENUM);
        EXPECT_EQ(wp->value.ToInt64(), (nk_int64)WeaponType::Sword);
    }

    // -- hashCode : read-only --
    const NkEditableProperty* hc = FindProp(props, "hashCode");
    if (hc) {
        EXPECT_TRUE(hc->readOnly);
    }

    printf("[TEST] Inspecteur : Set/GetPropertyByName (live-edit)\n");

    // Live-edit d'un int : l'instance doit changer.
    EXPECT_TRUE(SetPropertyValue(c, "level", (nk_int32)42));
    EXPECT_EQ(c.level, 42);

    // Live-edit d'un float.
    EXPECT_TRUE(SetPropertyValue(c, "stamina", (nk_float32)73.5f));
    EXPECT_NEAR(c.stamina, 73.5f);

    // Live-edit d'une string.
    EXPECT_TRUE((SetPropertyValue<Character, NkString>(c, "displayName", NkString("Mage"))));
    EXPECT_TRUE(c.displayName == NkString("Mage"));

    // Live-read : doit refleter la modification.
    NkReflectVariant rv = GetPropertyByName(c, "level");
    EXPECT_EQ(rv.ToInt64(), (nk_int64)42);

    // Read-only : l'ecriture doit ECHOUER et ne PAS modifier l'instance.
    nk_int32 before = c.hashCode;
    EXPECT_TRUE(!SetPropertyValue(c, "hashCode", (nk_int32)0));
    EXPECT_EQ(c.hashCode, before);

    // Propriete inexistante : echec propre.
    EXPECT_TRUE(!SetPropertyValue(c, "doesNotExist", (nk_int32)1));
}

// =============================================================================
// TEST 2 : INVOCATION DE METHODES (args > 0)
// =============================================================================
class Calculator {
public:
    nk_int32 Add(nk_int32 a, nk_int32 b)            { return a + b; }
    nk_float32 Scale(nk_float32 v, nk_float32 k)    { return v * k; }
    nk_int32 Sum3(nk_int32 a, nk_int32 b, nk_int32 c) { return a + b + c; }
    void Accumulate(nk_int32 v)                     { mTotal += v; }
    nk_int32 GetTotal() const                       { return mTotal; }
    nk_int32 mTotal = 0;
};

static void TestMethodInvocation() {
    printf("[TEST] Invocation methode 2-args : Add(int,int)\n");

    Calculator calc;
    const NkType& intType = NkTypeOf<nk_int32>();

    // -- Add(3,4) via Invoke (void**) brut --
    NkMethod add = NkMethod::MakeFromMember(&calc, &Calculator::Add, "Add", intType);
    EXPECT_TRUE(add.HasInvoke());
    EXPECT_EQ(add.GetParameterCount(), (nk_usize)2);
    EXPECT_EQ(&add.GetParameterType(0), &intType);

    nk_int32 a = 3, b = 4;
    void* rawArgs[] = { &a, &b };
    void* res = add.Invoke(nullptr, rawArgs);
    EXPECT_TRUE(res != nullptr);
    if (res) { EXPECT_EQ(*static_cast<nk_int32*>(res), 7); }

    // -- Add(10,32) via InvokeVariant (type-erased / Blueprint) --
    NkReflectVariant args[2] = {
        NkReflectVariant::From<nk_int32>(10),
        NkReflectVariant::From<nk_int32>(32),
    };
    NkReflectVariant ret = add.InvokeVariant(&calc, args, 2);
    EXPECT_TRUE(ret.IsValid());
    EXPECT_EQ(ret.ToInt64(), (nk_int64)42);

    printf("[TEST] Invocation methode float 2-args : Scale(float,float)\n");
    NkMethod scale = NkMethod::MakeFromMember(&calc, &Calculator::Scale, "Scale",
                                              NkTypeOf<nk_float32>());
    NkReflectVariant sargs[2] = {
        NkReflectVariant::From<nk_float32>(2.5f),
        NkReflectVariant::From<nk_float32>(4.0f),
    };
    NkReflectVariant sret = scale.InvokeVariant(&calc, sargs, 2);
    EXPECT_TRUE(sret.IsValid());
    EXPECT_NEAR((nk_float32)sret.ToFloat64(), 10.0f);

    printf("[TEST] Invocation methode 3-args : Sum3(int,int,int)\n");
    NkMethod sum3 = NkMethod::MakeFromMember(&calc, &Calculator::Sum3, "Sum3", intType);
    EXPECT_EQ(sum3.GetParameterCount(), (nk_usize)3);
    NkReflectVariant args3[3] = {
        NkReflectVariant::From<nk_int32>(100),
        NkReflectVariant::From<nk_int32>(20),
        NkReflectVariant::From<nk_int32>(3),
    };
    NkReflectVariant ret3 = sum3.InvokeVariant(&calc, args3, 3);
    EXPECT_EQ(ret3.ToInt64(), (nk_int64)123);

    printf("[TEST] Invocation methode void 1-arg + methode const 0-arg\n");
    NkMethod acc = NkMethod::MakeFromMember(&calc, &Calculator::Accumulate,
                                            "Accumulate", NkTypeOf<void>());
    NkReflectVariant accArg[1] = { NkReflectVariant::From<nk_int32>(5) };
    NkReflectVariant accRet = acc.InvokeVariant(&calc, accArg, 1);
    EXPECT_TRUE(!accRet.IsValid());     // void -> variant invalide
    EXPECT_EQ(calc.mTotal, 5);

    // Methode CONST 0-arg (surcharge const + flag NK_MCONST).
    NkMethod getTotal = NkMethod::MakeFromMember(&calc, &Calculator::GetTotal,
                                                 "GetTotal", intType);
    EXPECT_TRUE(getTotal.IsConst());
    NkReflectVariant tot = getTotal.InvokeVariant(&calc, nullptr, 0);
    EXPECT_EQ(tot.ToInt64(), (nk_int64)5);
}

// =============================================================================
// TEST 3 : SERIALISATION PROFONDE (Transform avec Vec3/Quat)
// =============================================================================
class Transform {
    NKENTSEU_REFLECT_CLASS(Transform)
public:
    NkVec3f position;
    NKENTSEU_REFLECT_PROPERTY(position)
public:
    NkQuatf rotation;
    NKENTSEU_REFLECT_PROPERTY(rotation)
public:
    NkVec3f scale;
    NKENTSEU_REFLECT_PROPERTY(scale)
public:
    Transform()
        : position(0, 0, 0), rotation(0, 0, 0, 1), scale(1, 1, 1) {}
};

static void TestDeepSerialization() {
    printf("[TEST] Serialisation profonde : Transform{Vec3,Quat,Vec3}\n");

    // Rend NkVec3f / NkQuatf reflechis (chaque composante = NkProperty float).
    NkRegisterMathReflection();
    EXPECT_TRUE(NkTypeOf<NkVec3f>().GetClass() != nullptr);
    EXPECT_TRUE(NkTypeOf<NkQuatf>().GetClass() != nullptr);

    const auto& tcls = Transform::GetStaticClass();
    NkRegistry::Get().RegisterClass(&tcls);

    Transform src;
    src.position = NkVec3f(1.5f, -2.0f, 3.25f);
    src.rotation = NkQuatf(0.0f, 0.7071f, 0.0f, 0.7071f);
    src.scale    = NkVec3f(2.0f, 2.0f, 2.0f);

    // -- Serialise vers JSON --
    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeReflected(&tcls, &src, ar));

    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    printf("    JSON:\n%s\n", json.CStr());

    // Le JSON doit contenir des sous-objets imbriques avec x/y/z(/w).
    EXPECT_TRUE(json.Contains(NkStringView("position")));
    EXPECT_TRUE(json.Contains(NkStringView("rotation")));

    // -- Round-trip : relit dans une instance vierge --
    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) { printf("    parse err: %s\n", err.CStr()); }

    Transform dst; // identite par defaut
    EXPECT_TRUE(NkReflectSerializer::DeserializeReflected(&tcls, &dst, ar2));

    // -- Verifie que chaque composante a survecu au round-trip --
    EXPECT_NEAR(dst.position.x, 1.5f);
    EXPECT_NEAR(dst.position.y, -2.0f);
    EXPECT_NEAR(dst.position.z, 3.25f);

    EXPECT_NEAR(dst.rotation.x, 0.0f);
    EXPECT_NEAR(dst.rotation.y, 0.7071f);
    EXPECT_NEAR(dst.rotation.z, 0.0f);
    EXPECT_NEAR(dst.rotation.w, 0.7071f);

    EXPECT_NEAR(dst.scale.x, 2.0f);
    EXPECT_NEAR(dst.scale.y, 2.0f);
    EXPECT_NEAR(dst.scale.z, 2.0f);
}

// =============================================================================
// TEST 4 : SERIALISATION PROFONDE MATRICE + COULEURS (NkMathReflect etendu)
// =============================================================================
class RenderState {
    NKENTSEU_REFLECT_CLASS(RenderState)
public:
    NkMat4f transform;          // 16 floats column-major
    NKENTSEU_REFLECT_PROPERTY(transform)
public:
    NkColorF tint;              // 4 floats r/g/b/a
    NKENTSEU_REFLECT_PROPERTY(tint)
public:
    NkColor border;             // 4 uint8 r/g/b/a
    NKENTSEU_REFLECT_PROPERTY(border)
public:
    RenderState() {}
};

static void TestMatrixColorSerialization() {
    printf("[TEST] Serialisation profonde : RenderState{Mat4f, ColorF, Color}\n");

    // Rend NkMat4f / NkColorF / NkColor reflechis (chaque composante editable).
    NkRegisterMathReflection();
    EXPECT_TRUE(NkTypeOf<NkMat4f>().GetClass() != nullptr);
    EXPECT_TRUE(NkTypeOf<NkColorF>().GetClass() != nullptr);
    EXPECT_TRUE(NkTypeOf<NkColor>().GetClass() != nullptr);

    // Verifie le bon nombre de composantes reflechies.
    EXPECT_EQ(NkTypeOf<NkMat4f>().GetClass()->GetPropertyCount(), (nk_usize)16);
    EXPECT_EQ(NkTypeOf<NkColorF>().GetClass()->GetPropertyCount(), (nk_usize)4);
    EXPECT_EQ(NkTypeOf<NkColor>().GetClass()->GetPropertyCount(), (nk_usize)4);

    const auto& cls = RenderState::GetStaticClass();
    NkRegistry::Get().RegisterClass(&cls);

    RenderState src;
    // Remplit la matrice avec 16 valeurs distinctes m0..m15.
    for (int i = 0; i < 16; ++i) {
        src.transform.data[i] = static_cast<nk_float32>(i) * 1.25f - 3.0f;
    }
    src.tint    = NkColorF(0.25f, 0.5f, 0.75f, 1.0f);
    src.border  = NkColor((nk_uint8)10, (nk_uint8)20, (nk_uint8)30, (nk_uint8)200);

    // -- Serialise vers JSON --
    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeReflected(&cls, &src, ar));

    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    printf("    JSON:\n%s\n", json.CStr());

    EXPECT_TRUE(json.Contains(NkStringView("transform")));
    EXPECT_TRUE(json.Contains(NkStringView("tint")));
    EXPECT_TRUE(json.Contains(NkStringView("border")));
    EXPECT_TRUE(json.Contains(NkStringView("m15")));

    // -- Round-trip --
    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) { printf("    parse err: %s\n", err.CStr()); }

    RenderState dst; // identite par defaut
    EXPECT_TRUE(NkReflectSerializer::DeserializeReflected(&cls, &dst, ar2));

    // -- Verifie que CHAQUE composante de la matrice a survecu --
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(dst.transform.data[i], src.transform.data[i]);
    }

    // -- Couleur flottante --
    EXPECT_NEAR(dst.tint.r, 0.25f);
    EXPECT_NEAR(dst.tint.g, 0.5f);
    EXPECT_NEAR(dst.tint.b, 0.75f);
    EXPECT_NEAR(dst.tint.a, 1.0f);

    // -- Couleur uint8 (round-trip exact attendu) --
    EXPECT_EQ((nk_int32)dst.border.r, (nk_int32)10);
    EXPECT_EQ((nk_int32)dst.border.g, (nk_int32)20);
    EXPECT_EQ((nk_int32)dst.border.b, (nk_int32)30);
    EXPECT_EQ((nk_int32)dst.border.a, (nk_int32)200);
}

// =============================================================================
// TEST 5 : SERIALISATION PROFONDE ANGLES (NkAngle + NkEulerAngle)
// =============================================================================
class Orientation {
    NKENTSEU_REFLECT_CLASS(Orientation)
public:
    NkAngle heading;            // 1 float "degrees" (wrap (-180,180])
    NKENTSEU_REFLECT_PROPERTY(heading)
public:
    NkEulerAngle rotation;      // 3 floats pitch/yaw/roll
    NKENTSEU_REFLECT_PROPERTY(rotation)
public:
    Orientation() {}
};

static void TestAngleSerialization() {
    printf("[TEST] Serialisation profonde : Orientation{NkAngle, NkEulerAngle}\n");

    NkRegisterMathReflection();
    EXPECT_TRUE(NkTypeOf<NkAngle>().GetClass() != nullptr);
    EXPECT_TRUE(NkTypeOf<NkEulerAngle>().GetClass() != nullptr);

    EXPECT_EQ(NkTypeOf<NkAngle>().GetClass()->GetPropertyCount(), (nk_usize)1);
    EXPECT_EQ(NkTypeOf<NkEulerAngle>().GetClass()->GetPropertyCount(), (nk_usize)3);

    const auto& cls = Orientation::GetStaticClass();
    NkRegistry::Get().RegisterClass(&cls);

    Orientation src;
    // Valeurs DANS l'intervalle canonique (-180,180] pour round-trip exact.
    src.heading = NkAngle(135.0f);
    src.rotation = NkEulerAngle(NkAngle(45.0f), NkAngle(-90.0f), NkAngle(30.0f));

    NkArchive ar;
    EXPECT_TRUE(NkReflectSerializer::SerializeReflected(&cls, &src, ar));

    NkString json;
    EXPECT_TRUE(NkJSONWriter::WriteArchive(ar, json, true));
    printf("    JSON:\n%s\n", json.CStr());

    EXPECT_TRUE(json.Contains(NkStringView("heading")));
    EXPECT_TRUE(json.Contains(NkStringView("degrees")));
    EXPECT_TRUE(json.Contains(NkStringView("pitch")));
    EXPECT_TRUE(json.Contains(NkStringView("yaw")));
    EXPECT_TRUE(json.Contains(NkStringView("roll")));

    // -- Round-trip --
    NkArchive ar2;
    NkString err;
    EXPECT_TRUE(NkJSONReader::ReadArchive(json.View(), ar2, &err));
    if (!err.Empty()) { printf("    parse err: %s\n", err.CStr()); }

    Orientation dst;
    EXPECT_TRUE(NkReflectSerializer::DeserializeReflected(&cls, &dst, ar2));

    EXPECT_NEAR(dst.heading.Deg(), 135.0f);
    EXPECT_NEAR(dst.rotation.pitch.Deg(), 45.0f);
    EXPECT_NEAR(dst.rotation.yaw.Deg(), -90.0f);
    EXPECT_NEAR(dst.rotation.roll.Deg(), 30.0f);
}

// =============================================================================
// MAIN
// =============================================================================
int main() {
    printf("======================================================\n");
    printf(" NKReflection - Tests Phase 5 (inspecteur + invocation + deep)\n");
    printf("======================================================\n");

    WireCharacterMeta();

    TestInspector();
    TestMethodInvocation();
    TestDeepSerialization();
    TestMatrixColorSerialization();
    TestAngleSerialization();

    printf("======================================================\n");
    printf("  Results: %d passed, %d failed\n", s_pass, s_fail);
    printf("======================================================\n");
    return s_fail == 0 ? 0 : 1;
}

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
