#pragma once
// =============================================================================
// UObject.h  —  Style Unreal Engine
// Classe racine de tout l'écosystème UE.
// Fournit : réflexion basique, nommage, garbage collection (ref counting),
//           et le système de propriétés (UPROPERTY simulé).
// =============================================================================
#include <string>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <functional>
#include <vector>
#include <cstdio>

namespace UE {

// ---------------------------------------------------------------------------
// Macro simulant GENERATED_BODY() / UCLASS()
// En vrai UE : génère du code de réflexion par l'UHT (Unreal Header Tool)
// ---------------------------------------------------------------------------
#define UCLASS(...)                                                    \
    public:                                                            \
        static const char* StaticClass() { return __CLASS_NAME__; }   \
        virtual const char* GetClass() const { return __CLASS_NAME__; }

#define DECLARE_CLASS(ClassName)                     \
    private:                                         \
        static constexpr const char* __CLASS_NAME__ = #ClassName;

// Simule UPROPERTY : en vrai UE, expose à l'éditeur, réseau, Blueprint
#define UPROPERTY(...)  /* annotation — aucun effet en C++ pur */
#define UFUNCTION(...)  /* annotation */

// ---------------------------------------------------------------------------
// FName : nom immuable haché (simplifié ici en string)
// En vrai UE : table de hachage globale, comparaison en O(1)
// ---------------------------------------------------------------------------
struct FName {
    std::string value;
    FName() = default;
    explicit FName(const char* s)        : value(s)  {}
    explicit FName(std::string s)        : value(std::move(s)) {}
    bool operator==(const FName& o) const { return value == o.value; }
    bool operator!=(const FName& o) const { return value != o.value; }
    const char* c_str() const { return value.c_str(); }
};

// ---------------------------------------------------------------------------
// Delegate mono-cast simplifié
// En vrai UE : DECLARE_DELEGATE, DECLARE_MULTICAST_DELEGATE macros
// ---------------------------------------------------------------------------
template<typename... Args>
class TDelegate {
    std::function<void(Args...)> mFn;
public:
    void BindLambda(std::function<void(Args...)> fn) { mFn = std::move(fn); }
    void Execute(Args... args) const { if (mFn) mFn(std::forward<Args>(args)...); }
    bool IsBound() const { return static_cast<bool>(mFn); }
    void Unbind() { mFn = nullptr; }
};

template<typename... Args>
class TMulticastDelegate {
    std::vector<std::function<void(Args...)>> mListeners;
public:
    void AddLambda(std::function<void(Args...)> fn) {
        mListeners.push_back(std::move(fn));
    }
    void Broadcast(Args... args) const {
        for (auto& fn : mListeners) fn(args...);
    }
    void Clear() { mListeners.clear(); }
    bool HasAny() const { return !mListeners.empty(); }
};

// ---------------------------------------------------------------------------
// UObject  —  classe racine
// Tout objet Unreal (UActorComponent, AActor, UGameInstance…) en dérive.
// ---------------------------------------------------------------------------
class UObject {
    DECLARE_CLASS(UObject)
public:
    UCLASS()

    virtual ~UObject() = default;

    // ── Identité ───────────────────────────────────────────────────────────
    const FName& GetName() const { return mName; }
    void         SetName(FName n) { mName = std::move(n); }

    // ── Propriétés génériques (simule le système de réflexion UE) ─────────
    // En vrai UE : stockées dans des FProperty avec UHT
    void   SetProperty(const std::string& key, float val)       { mFloatProps[key] = val; }
    float  GetPropertyFloat(const std::string& key, float def=0.f) const {
        auto it = mFloatProps.find(key); return it!=mFloatProps.end()?it->second:def;
    }
    void   SetProperty(const std::string& key, bool val)        { mBoolProps[key] = val; }
    bool   GetPropertyBool(const std::string& key, bool def=false) const {
        auto it = mBoolProps.find(key); return it!=mBoolProps.end()?it->second:def;
    }

    // ── Marqué pour la GC ─────────────────────────────────────────────────
    bool IsPendingKill() const { return mPendingKill; }

protected:
    void MarkPendingKill() { mPendingKill = true; }

private:
    FName mName;
    bool  mPendingKill = false;
    std::unordered_map<std::string, float> mFloatProps;
    std::unordered_map<std::string, bool>  mBoolProps;
};

} // namespace UE
