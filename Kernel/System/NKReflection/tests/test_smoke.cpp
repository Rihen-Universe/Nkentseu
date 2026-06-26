#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKReflection/NkType.h"
#include "NKReflection/NkRegistry.h"
#include "NKReflection/NkReflectVariant.h"
#include "NKContainers/String/NkString.h"

#include <cstring>

using namespace nkentseu::reflection;

TEST_CASE(NKReflectionSmoke, TypeCategoryBasics) {
    const NkType& ti = NkTypeOf<nkentseu::nk_int32>();
    const NkType& tf = NkTypeOf<nkentseu::nk_float32>();

    ASSERT_EQUAL(NkTypeCategory::NK_INT32, ti.GetCategory());
    ASSERT_EQUAL(NkTypeCategory::NK_FLOAT32, tf.GetCategory());
    ASSERT_TRUE(ti.GetSize() == sizeof(nkentseu::nk_int32));
}

TEST_CASE(NKReflectionSmoke, RegistryTypeAccessor) {
    const NkType* t = NkRegistry::Get().GetType<nkentseu::nk_uint64>();
    ASSERT_NOT_NULL(t);
    ASSERT_TRUE(t->GetSize() == sizeof(nkentseu::nk_uint64));
}

// ---------------------------------------------------------------------------
// PHASE 1 : NkReflectVariant (valeur type-erased)
// ---------------------------------------------------------------------------

TEST_CASE(NKReflectionSmoke, VariantPrimitiveRoundTrip) {
    NkReflectVariant vi = NkReflectVariant::From<nkentseu::nk_int32>(42);
    ASSERT_TRUE(vi.IsValid());
    ASSERT_EQUAL(NkTypeCategory::NK_INT32, vi.GetCategory());

    nkentseu::nk_int32 out = 0;
    ASSERT_TRUE(vi.Get<nkentseu::nk_int32>(out));
    ASSERT_TRUE(out == 42);

    // Coercions
    ASSERT_TRUE(vi.ToInt64() == 42);
    ASSERT_TRUE(vi.ToFloat64() == 42.0);
    ASSERT_TRUE(vi.ToBool() == true);
}

TEST_CASE(NKReflectionSmoke, VariantFloatAndString) {
    NkReflectVariant vf = NkReflectVariant::From<nkentseu::nk_float32>(2.5f);
    ASSERT_TRUE(vf.ToFloat64() == 2.5);

    nkentseu::NkString hello("hello");
    NkReflectVariant vs = NkReflectVariant::From<nkentseu::NkString>(hello);
    ASSERT_TRUE(vs.IsValid());
    ASSERT_EQUAL(NkTypeCategory::NK_STRING, vs.GetCategory());

    nkentseu::NkString back;
    ASSERT_TRUE(vs.Get<nkentseu::NkString>(back));
    ASSERT_TRUE(std::strcmp(back.CStr(), "hello") == 0);

    // Copie profonde : la copie est independante de la source.
    NkReflectVariant copy = vs;
    nkentseu::NkString back2;
    ASSERT_TRUE(copy.Get<nkentseu::NkString>(back2));
    ASSERT_TRUE(std::strcmp(back2.CStr(), "hello") == 0);
}

// ---------------------------------------------------------------------------
// PHASE 1 : classe reflechie + Get/SetValueGeneric + auto-register
// ---------------------------------------------------------------------------

// Note de design : NKENTSEU_REFLECT_PROPERTY se termine par `private:`. Pour
// garder les membres accessibles dans le test, on declare chaque membre dans une
// section `public:` puis on reflechit via NKENTSEU_REFLECT_PROPERTY (qui ajoute
// l'accesseur + le registrar d'auto-enregistrement).
class Monster {
    NKENTSEU_REFLECT_CLASS(Monster)
public:
    nkentseu::nk_int32 health;
    NKENTSEU_REFLECT_PROPERTY(health)
public:
    nkentseu::nk_float32 speed;
    NKENTSEU_REFLECT_PROPERTY(speed)
public:
    nkentseu::NkString name;
    NKENTSEU_REFLECT_PROPERTY(name)
};

// Enregistrement de la classe dans le registre global (auto avant main()).
NKENTSEU_REGISTER_CLASS(Monster)

TEST_CASE(NKReflectionSmoke, ClassAutoRegisterProperties) {
    const NkClass& cls = Monster::GetStaticClass();

    // Les 3 proprietes doivent etre auto-enregistrees par NKENTSEU_PROPERTY.
    ASSERT_TRUE(cls.GetPropertyCount() == 3);

    ASSERT_NOT_NULL(cls.GetProperty("health"));
    ASSERT_NOT_NULL(cls.GetProperty("speed"));
    ASSERT_NOT_NULL(cls.GetProperty("name"));

    // La classe doit etre trouvable via le registre global.
    const NkClass* found = NkRegistry::Get().FindClass("Monster");
    ASSERT_NOT_NULL(found);
}

TEST_CASE(NKReflectionSmoke, GetSetValueGenericRoundTrip) {
    Monster m;
    m.health = 100;
    m.speed = 3.5f;
    m.name = nkentseu::NkString("Orc");

    const NkClass& cls = Monster::GetStaticClass();
    const NkProperty* healthProp = cls.GetProperty("health");
    const NkProperty* speedProp = cls.GetProperty("speed");
    const NkProperty* nameProp = cls.GetProperty("name");
    ASSERT_NOT_NULL(healthProp);
    ASSERT_NOT_NULL(speedProp);
    ASSERT_NOT_NULL(nameProp);

    // GetValueGeneric : lecture type-erased.
    NkReflectVariant vh = healthProp->GetValueGeneric(&m);
    ASSERT_TRUE(vh.IsValid());
    ASSERT_TRUE(vh.ToInt64() == 100);

    NkReflectVariant vs = speedProp->GetValueGeneric(&m);
    ASSERT_TRUE(vs.ToFloat64() == 3.5);

    NkReflectVariant vn = nameProp->GetValueGeneric(&m);
    ASSERT_EQUAL(NkTypeCategory::NK_STRING, vn.GetCategory());
    ASSERT_TRUE(std::strcmp(vn.ToString().CStr(), "Orc") == 0);

    // SetValueGeneric : ecriture type-erased -> l'instance doit changer.
    ASSERT_TRUE(healthProp->SetValueGeneric(&m, NkReflectVariant::From<nkentseu::nk_int32>(250)));
    ASSERT_TRUE(m.health == 250);

    ASSERT_TRUE(speedProp->SetValueGeneric(&m, NkReflectVariant::From<nkentseu::nk_float32>(9.0f)));
    ASSERT_TRUE(m.speed == 9.0f);

    ASSERT_TRUE(nameProp->SetValueGeneric(&m, NkReflectVariant::From<nkentseu::NkString>(nkentseu::NkString("Goblin"))));
    ASSERT_TRUE(std::strcmp(m.name.CStr(), "Goblin") == 0);

    // Round-trip complet : relire apres ecriture.
    ASSERT_TRUE(healthProp->GetValueGeneric(&m).ToInt64() == 250);

    // Coercion numerique : ecrire un int dans speed (float).
    ASSERT_TRUE(speedProp->SetValueGeneric(&m, NkReflectVariant::From<nkentseu::nk_int32>(12)));
    ASSERT_TRUE(m.speed == 12.0f);
}
