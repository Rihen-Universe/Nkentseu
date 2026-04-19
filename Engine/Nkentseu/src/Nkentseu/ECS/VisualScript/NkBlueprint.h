// =============================================================================
// FICHIER: NKECS/VisualScript/NkBlueprint.h
// DESCRIPTION: Système de Scripting Visuel (Blueprints) Dynamique & Extensible.
// =============================================================================
#pragma once
#include "../NkECSDefines.h"
#include "../Core/NkTypeRegistry.h"
#include "../World/NkWorld.h"
#include "../GameObject/NkGameObject.h"
#include "../Components/Physics/NkPhysics.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <cstring>

namespace nkentseu {
    namespace ecs {
        namespace blueprint {

            // ============================================================================
            // 1. SYSTÈME DE TYPES DYNAMIQUES
            // ============================================================================

            // Types primitifs connus (optimisés via union)
            enum class NkPinPrimitiveType : uint8 {
                None, Exec, Bool, Int, Float, Vec2, Vec3, Vec4, String, EntityId, GameObject
            };

            // Identifiant pour les types personnalisés (Structs, Classes, Enums)
            struct NkTypeId {
                uint64 Hash = 0;          // Hash FNV1a du nom du type
                const char* Name = "";
                
                NkTypeId() noexcept = default;
                NkTypeId(const char* name) noexcept : Name(name), Hash(detail::FNV1a(name)) {}
                bool operator==(const NkTypeId& o) const noexcept { return Hash == o.Hash && Name == o.Name; }
            };

            // Type d'une Pin (soit Primitif, soit Custom)
            struct NkPinType {
                NkPinPrimitiveType Primitive = NkPinPrimitiveType::None;
                NkTypeId Custom; // Rempli si Primitive == None

                [[nodiscard]] bool IsExec() const noexcept { return Primitive == NkPinPrimitiveType::Exec; }
                [[nodiscard]] bool IsData() const noexcept { return !IsExec(); }
                [[nodiscard]] bool IsCustom() const noexcept { return Primitive == NkPinPrimitiveType::None && Custom.Hash != 0; }
            };

            // Direction d'une Pin
            enum class NkPinDirection : uint8 { Input, Output };

            // ============================================================================
            // 2. VALEURS DE PINS (POLYMORPHISME LÉGER)
            // ============================================================================
            
            // Structure capable de contenir n'importe quelle donnée supportée par le Blueprint
            struct NkValue {
                NkPinType Type;
                union Data {
                    bool b;
                    int32 i;
                    float f;
                    NkVec3 v3;
                    uint64 entityId; // Packé
                    void* ptr;       // Pour Struct*, Object*, Event*
                } data;

                NkValue() noexcept { std::memset(this, 0, sizeof(NkValue)); }
                
                // Helpers de construction rapide
                static NkValue Exec() noexcept { return {}; } // Exec n'a pas de data
                static NkValue Bool(bool v) noexcept { NkValue val; val.Type.Primitive = NkPinPrimitiveType::Bool; val.data.b = v; return val; }
                static NkValue Int(int32 v) noexcept { NkValue val; val.Type.Primitive = NkPinPrimitiveType::Int; val.data.i = v; return val; }
                static NkValue Float(float v) noexcept { NkValue val; val.Type.Primitive = NkPinPrimitiveType::Float; val.data.f = v; return val; }
                static NkValue Vec3(NkVec3 v) noexcept { NkValue val; val.Type.Primitive = NkPinPrimitiveType::Vec3; val.data.v3 = v; return val; }
                static NkValue Entity(NkEntityId id) noexcept { NkValue val; val.Type.Primitive = NkPinPrimitiveType::EntityId; val.data.entityId = id.Pack(); return val; }
                static NkValue Ptr(void* p, const NkTypeId& type) noexcept { NkValue val; val.Type.Custom = type; val.data.ptr = p; return val; }
                static NkValue String(const char* s) noexcept { /* Simplifié: stocke pointeur const char* dans union ou utilise ptr */ 
                    NkValue val; val.Type.Primitive = NkPinPrimitiveType::String; val.data.ptr = (void*)s; return val; 
                }

                // Accesseurs typés
                [[nodiscard]] bool AsBool() const noexcept { return data.b; }
                [[nodiscard]] int32 AsInt() const noexcept { return data.i; }
                [[nodiscard]] float AsFloat() const noexcept { return data.f; }
                [[nodiscard]] NkVec3 AsVec3() const noexcept { return data.v3; }
                [[nodiscard]] NkEntityId AsEntityId() const noexcept { return NkEntityId::Unpack(data.entityId); }
                template<typename T> [[nodiscard]] T* AsPtr() const noexcept { return static_cast<T*>(data.ptr); }
            };

            // ============================================================================
            // 3. DÉFINITION DES PINS
            // ============================================================================
            struct NkBlueprintPin {
                std::string Name;
                NkPinType   Type;
                NkPinDirection Direction;
                NkValue     DefaultValue; // Valeur par défaut si non connectée
                bool        IsConnected = false;
            };

            // ============================================================================
            // 4. CLASSE DE BASE DES NŒUDS
            // ============================================================================
            class NkBlueprintNode {
                public:
                    std::string Name;
                    NkTypeId NodeTypeId; // Identifiant du type de nœud
                    bool Enabled = true;

                    std::vector<NkBlueprintPin> Inputs;
                    std::vector<NkBlueprintPin> Outputs;

                    virtual ~NkBlueprintNode() noexcept = default;

                    // Exécution du nœud. Contexte fournit World, Self, et accès aux données.
                    virtual void Execute(NkWorld& /*world*/, NkEntityId /*self*/, float /*dt*/) noexcept = 0;

                    // Helper pour récupérer une valeur d'entrée (fallback sur Default si non connecté)
                    const NkValue& GetInput(uint32 index) const noexcept {
                        return Inputs[index].IsConnected ? Inputs[index].DefaultValue : Inputs[index].DefaultValue; // Simplifié: DefaultValue est écrasé par la connexion
                    }

                    // Helper pour définir une valeur de sortie
                    void SetOutput(uint32 index, const NkValue& value) noexcept {
                        if (index < Outputs.size()) Outputs[index].DefaultValue = value;
                    }

                    [[nodiscard]] virtual const char* GetCategory() const noexcept { return "Base"; }
            };

            // ============================================================================
            // 5. REGISTRE & USINE DE NŒUDS (EXTENSIBLE)
            // ============================================================================
            class NkNodeRegistry {
                public:
                    using FactoryFn = std::function<std::unique_ptr<NkBlueprintNode>()>;
                    
                    [[nodiscard]] static NkNodeRegistry& Global() noexcept {
                        static NkNodeRegistry instance; return instance;
                    }

                    // Enregistre un type de nœud
                    template<typename T>
                    static void Register(const char* name) noexcept {
                        auto& reg = Global();
                        if (reg.mCount >= 512) return;
                        Entry& e = reg.mEntries[reg.mCount++];
                        std::strncpy(e.Name, name, 127);
                        e.Factory = [] { return std::make_unique<T>(); };
                    }

                    // Crée une instance
                    [[nodiscard]] std::unique_ptr<NkBlueprintNode> Create(const char* name) noexcept {
                        for (uint32 i = 0; i < mCount; ++i) {
                            if (std::strcmp(mEntries[i].Name, name) == 0) return mEntries[i].Factory();
                        }
                        return nullptr;
                    }

                private:
                    struct Entry { char Name[128] = {}; FactoryFn Factory; };
                    Entry mEntries[512] = {}; uint32 mCount = 0;
            };

            #define NK_REGISTER_BLUEPRINT_NODE(Type)                                              \
            namespace { struct _NkNodeReg_##Type {                                              \
                _NkNodeReg_##Type() noexcept { NkNodeRegistry::Global().Register<Type>(#Type); } \
            } _sNkNodeReg_##Type; }

            // ============================================================================
            // 6. EXEMPLES DE NŒUDS (CE QUE VOUS AVEZ DEMANDÉ)
            // ============================================================================

            // --- A. Nœuds Événements (Events) ---
            // Permet de déclencher des logiques personnalisées ou prédéfinies.
            class NkNodeEventBeginPlay : public NkBlueprintNode {
                public:
                    NkNodeEventBeginPlay() noexcept {
                        Name = "EventBeginPlay";
                        Outputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {}
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Events"; }
            };

            // Événement personnalisé avec paramètres (ex: OnDamageTaken)
            // Les pins d'entrée sont ajoutées dynamiquement ou via template
            class NkNodeEventCustom : public NkBlueprintNode {
                public:
                    NkNodeEventCustom() noexcept {
                        Name = "EventCustom";
                        Outputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        // Exemple : Pin de donnée custom "Damage"
                        Inputs.push_back({"Damage", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {}
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Events"; }
            };

            // --- B. Nœuds de Fonctions / Méthodes (CallFunction) ---
            // Simule l'appel à une fonction C++ ou un script.
            // Entrées : Exec + Args | Sorties : Exec + Return
            class NkNodeCallFunction : public NkBlueprintNode {
                public:
                    NkNodeCallFunction() noexcept {
                        Name = "PrintString";
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Inputs.push_back({"InString", NkPinType{NkPinPrimitiveType::String}, NkPinDirection::Input});
                        Outputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        // Exécution simple : print
                        const char* msg = GetInput(1).AsPtr<const char>();
                        if (msg) { /* Log::Info(msg); */ }
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "FlowControl"; }
            };

            // --- C. Nœuds de Structures (Make Vector3) ---
            // Prend des primitives en entrée, sort une Struct (ici Vec3).
            class NkNodeMakeVector3 : public NkBlueprintNode {
                public:
                    NkNodeMakeVector3() noexcept {
                        Name = "MakeVector3";
                        Inputs.push_back({"X", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input, NkValue::Float(0)});
                        Inputs.push_back({"Y", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input, NkValue::Float(0)});
                        Inputs.push_back({"Z", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input, NkValue::Float(0)});
                        Outputs.push_back({"ReturnValue", NkPinType{NkPinPrimitiveType::Vec3}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        float x = GetInput(0).AsFloat();
                        float y = GetInput(1).AsFloat();
                        float z = GetInput(2).AsFloat();
                        SetOutput(0, NkValue::Vec3({x, y, z}));
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Structs"; }
            };

            // --- D. Nœuds d'Énumération (Switch Enum) ---
            // Prend un Enum en entrée, sorties multiples basées sur la valeur.
            class NkNodeSwitchInt : public NkBlueprintNode {
                public:
                    NkNodeSwitchInt() noexcept {
                        Name = "SwitchInt";
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Inputs.push_back({"Selection", NkPinType{NkPinPrimitiveType::Int}, NkPinDirection::Input, NkValue::Int(0)});
                        
                        // Sorties Exec : 0, 1, Default
                        Outputs.push_back({"0", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"1", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"Default", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        int sel = GetInput(1).AsInt();
                        if (sel == 0) SetOutput(0, NkValue::Exec()); // Active la pin 0
                        else if (sel == 1) SetOutput(1, NkValue::Exec());
                        else SetOutput(2, NkValue::Exec()); // Default
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "FlowControl"; }
            };

            // --- E. Nœuds Mathématiques (Add, Multiply...) ---
            class NkNodeAddFloat : public NkBlueprintNode {
                public:
                    NkNodeAddFloat() noexcept {
                        Name = "AddFloat";
                        Inputs.push_back({"A", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input, NkValue::Float(0)});
                        Inputs.push_back({"B", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input, NkValue::Float(0)});
                        Outputs.push_back({"Result", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        SetOutput(0, NkValue::Float(GetInput(0).AsFloat() + GetInput(1).AsFloat()));
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Math"; }
            };

            // --- F. Nœuds Physique / Monde (Raycast, SpawnActor) ---
            // Exemple de Raycast qui retourne un HitActor (GameObject)
            class NkNodeRaycast : public NkBlueprintNode {
                public:
                    NkNodeRaycast() noexcept {
                        Name = "Raycast";
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Inputs.push_back({"Start", NkPinType{NkPinPrimitiveType::Vec3}, NkPinDirection::Input});
                        Inputs.push_back({"End", NkPinType{NkPinPrimitiveType::Vec3}, NkPinDirection::Input});
                        
                        Outputs.push_back({"Hit", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"Miss", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"HitActor", NkPinType{NkPinPrimitiveType::GameObject}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld& world, NkEntityId self, float) noexcept override {
                        // Logique fictive de Raycast
                        NkVec3 start = GetInput(1).AsVec3();
                        NkVec3 end = GetInput(2).AsVec3();
                        
                        bool hit = false;
                        NkEntityId hitId = NkEntityId::Invalid();
                        
                        // ... Appel au système de physique ici ...
                        // if (world.PhysicsSystem.Raycast(start, end, hitId)) ...

                        if (hit) {
                            SetOutput(0, NkValue::Exec()); // Hit Exec
                            SetOutput(2, NkValue::Entity(hitId)); // Hit Actor
                        } else {
                            SetOutput(1, NkValue::Exec()); // Miss Exec
                        }
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Physics"; }
            };

            // --- G. Nœuds de Spawn (SpawnActor) ---
            class NkNodeSpawnActor : public NkBlueprintNode {
                public:
                    NkNodeSpawnActor() noexcept {
                        Name = "SpawnActor";
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Inputs.push_back({"Class/Name", NkPinType{NkPinPrimitiveType::String}, NkPinDirection::Input, NkValue::String("DefaultActor")});
                        Inputs.push_back({"Location", NkPinType{NkPinPrimitiveType::Vec3}, NkPinDirection::Input, NkValue::Vec3(NkVec3::Zero())});
                        
                        Outputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"NewActor", NkPinType{NkPinPrimitiveType::GameObject}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld& world, NkEntityId, float) noexcept override {
                        // const char* className = GetInput(1).AsPtr<const char>(); // Simplifié
                        NkVec3 loc = GetInput(2).AsVec3();
                        
                        // Spawn via Factory
                        // auto go = world.CreateGameObject(className); 
                        // go.SetPosition(loc);
                        
                        // Simulation pour l'exemple
                        NkEntityId newId = world.CreateEntity();
                        SetOutput(0, NkValue::Exec());
                        SetOutput(1, NkValue::Entity(newId));
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Spawning"; }
            };

            // ============================================================================
            // 7. GRAPHE & COMPOSANT BLUEPRINT (INCHANGÉ MAIS COMPATIBLE)
            // ============================================================================
            
            // Structure de connexion simple
            struct NkBlueprintConnection {
                uint32 SourceNode = 0; uint32 SourcePin = 0;
                uint32 TargetNode = 0; uint32 TargetPin = 0;
            };

            class NkBlueprintGraph {
                public:
                    std::vector<std::unique_ptr<NkBlueprintNode>> Nodes;
                    std::vector<NkBlueprintConnection> Connections;
                    uint32 EntryNodeIndex = 0; // Index du nœud de départ (ex: EventBeginPlay)

                    void AddNode(std::unique_ptr<NkBlueprintNode> node) noexcept {
                        Nodes.push_back(std::move(node));
                    }

                    void Link(uint32 srcNode, uint32 srcPin, uint32 tgtNode, uint32 tgtPin) noexcept {
                        Connections.push_back({srcNode, srcPin, tgtNode, tgtPin});
                        // Marquer la pin cible comme connectée (logique simplifiée)
                        if (tgtNode < Nodes.size() && tgtPin < Nodes[tgtNode]->Inputs.size()) {
                            Nodes[tgtNode]->Inputs[tgtPin].IsConnected = true;
                        }
                    }

                    // Exécution itérative (parcourt le graphe depuis EntryNode)
                    void Execute(NkWorld& world, NkEntityId self, float dt) noexcept {
                        if (EntryNodeIndex >= Nodes.size()) return;
                        
                        // Pile d'exécution (Stack) pour le flux "Exec"
                        std::vector<uint32> execStack;
                        execStack.push_back(EntryNodeIndex);

                        while (!execStack.empty()) {
                            uint32 currentNodeIdx = execStack.back();
                            execStack.pop_back();

                            if (currentNodeIdx >= Nodes.size()) continue;
                            NkBlueprintNode* node = Nodes[currentNodeIdx].get();
                            if (!node || !node->Enabled) continue;

                            // 1. Propager les données (Inputs) depuis les connexions
                            ResolveInputs(currentNodeIdx);

                            // 2. Exécuter la logique du nœud
                            node->Execute(world, self, dt);

                            // 3. Suivre les sorties "Exec" pour continuer le flux
                            for (uint32 i = 0; i < node->Outputs.size(); ++i) {
                                if (node->Outputs[i].Type.IsExec()) {
                                    // Si la pin de sortie Exec a une valeur (activée), on cherche où elle mène
                                    // Ici on considère que si le nœud est exécuté, il active ses sorties Exec
                                    // (Simplification: dans un vrai moteur, on vérifie si Output[i] contient un Exec token)
                                    
                                    uint32 nextNode = FindTargetNode(currentNodeIdx, i);
                                    if (nextNode < Nodes.size()) execStack.push_back(nextNode);
                                }
                            }
                        }
                    }

                private:
                    void ResolveInputs(uint32 nodeIdx) noexcept {
                        // Pour chaque pin d'entrée, vérifier si connectée.
                        // Si oui, exécuter le nœud source pour remplir la pin de sortie, puis copier.
                        NkBlueprintNode* node = Nodes[nodeIdx].get();
                        if (!node) return;

                        for (uint32 i = 0; i < node->Inputs.size(); ++i) {
                            // Trouver la connexion entrante
                            uint32 srcNode = 0, srcPin = 0;
                            bool found = false;
                            for (const auto& c : Connections) {
                                if (c.TargetNode == nodeIdx && c.TargetPin == i) {
                                    srcNode = c.SourceNode; srcPin = c.SourcePin; found = true; break;
                                }
                            }

                            if (found) {
                                // Exécuter le nœud source pour qu'il calcule sa sortie
                                // Attention: récursion potentielle, nécessite un cache ou DAG topologique en prod.
                                // Ici on suppose que les données sont déjà calculées ou que c'est un simple getter.
                                // Pour un système complet, il faut évaluer les dépendances de données avant l'exécution.
                            }
                        }
                    }

                    uint32 FindTargetNode(uint32 srcNode, uint32 srcPin) const noexcept {
                        for (const auto& c : Connections) {
                            if (c.SourceNode == srcNode && c.SourcePin == srcPin) return c.TargetNode;
                        }
                        return 0xFFFFFFFFu;
                    }
            };

            // Composant ECS attaché à une entité
            class NkBlueprintComponent : public NkScriptComponent {
                public:
                    NkBlueprintGraph Graph;
                    
                    void OnStart(NkWorld& world, NkEntityId self) noexcept override {
                        // Exécuter le nœud "EventBeginPlay" s'il existe
                        TriggerEvent(world, self, "EventBeginPlay");
                    }

                    void OnUpdate(NkWorld& world, NkEntityId self, float dt) noexcept override {
                        TriggerEvent(world, self, "EventTick");
                    }

                    [[nodiscard]] const char* GetTypeName() const noexcept override { return "NkBlueprintComponent"; }

                private:
                    void TriggerEvent(NkWorld& world, NkEntityId self, const char* eventName) noexcept {
                        // Cherche le nœud avec ce nom et l'exécute
                        for (uint32 i = 0; i < Graph.Nodes.size(); ++i) {
                            if (Graph.Nodes[i] && Graph.Nodes[i]->Name == eventName) {
                                Graph.EntryNodeIndex = i;
                                Graph.Execute(world, self, 0.f);
                            }
                        }
                    }
            };

                        // ============================================================================
            // --- H. NŒUDS DE CAST (CONVERSION DE TYPES) ---
            // ============================================================================

            // Cast primitif : Int → Float
            class NkNodeCastIntToFloat : public NkBlueprintNode {
                public:
                    NkNodeCastIntToFloat() noexcept {
                        Name = "CastIntToFloat";
                        Inputs.push_back({"Input", NkPinType{NkPinPrimitiveType::Int}, NkPinDirection::Input});
                        Outputs.push_back({"Output", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        int32 val = GetInput(0).AsInt();
                        SetOutput(0, NkValue::Float(static_cast<float>(val)));
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Utilities"; }
            };

            // Cast primitif : Float → Int (tronque)
            class NkNodeCastFloatToInt : public NkBlueprintNode {
                public:
                    NkNodeCastFloatToInt() noexcept {
                        Name = "CastFloatToInt";
                        Inputs.push_back({"Input", NkPinType{NkPinPrimitiveType::Float}, NkPinDirection::Input});
                        Outputs.push_back({"Output", NkPinType{NkPinPrimitiveType::Int}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        float val = GetInput(0).AsFloat();
                        SetOutput(0, NkValue::Int(static_cast<int32>(val)));
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Utilities"; }
            };

            // Cast : EntityId → GameObject (vérifie validité)
            class NkNodeCastEntityToGameObject : public NkBlueprintNode {
                public:
                    NkNodeCastEntityToGameObject() noexcept {
                        Name = "CastEntityToGameObject";
                        Inputs.push_back({"Input", NkPinType{NkPinPrimitiveType::EntityId}, NkPinDirection::Input});
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Outputs.push_back({"AsGameObject", NkPinType{NkPinPrimitiveType::GameObject}, NkPinDirection::Output});
                        Outputs.push_back({"Success", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"Failed", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    void Execute(NkWorld& world, NkEntityId, float) noexcept override {
                        NkEntityId id = GetInput(0).AsEntityId();
                        if (id.IsValid() && world.IsAlive(id)) {
                            SetOutput(0, NkValue::Entity(id));
                            SetOutput(1, NkValue::Exec()); // Success
                        } else {
                            SetOutput(2, NkValue::Exec()); // Failed
                        }
                    }
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Utilities"; }
            };

            // Cast dynamique vers un type custom (avec vérification via NkTypeId)
            class NkNodeCastToType : public NkBlueprintNode {
                public:
                    NkTypeId TargetType;
                    
                    NkNodeCastToType() noexcept = default;
                    explicit NkNodeCastToType(const NkTypeId& targetType) noexcept : TargetType(targetType) {
                        Name = "CastToType";
                        Inputs.push_back({"Input", NkPinType{NkPinPrimitiveType::None, NkTypeId("void*")}, NkPinDirection::Input});
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Outputs.push_back({"AsType", NkPinType{NkPinPrimitiveType::None, TargetType}, NkPinDirection::Output});
                        Outputs.push_back({"Success", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"Failed", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    
                    void Execute(NkWorld&, NkEntityId, float) noexcept override {
                        void* ptr = GetInput(0).AsPtr<void>();
                        NkTypeId inputType = GetInput(0).Type.Custom;
                        
                        // Vérification basique par hash (en prod : système de réflexion complet)
                        if (ptr && inputType.Hash == TargetType.Hash) {
                            SetOutput(0, NkValue::Ptr(ptr, TargetType));
                            SetOutput(1, NkValue::Exec()); // Success
                        } else {
                            SetOutput(2, NkValue::Exec()); // Failed
                        }
                    }
                    
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Utilities"; }
            };

            // Cast vers un composant ECS spécifique (template)
            template<typename T>
            class NkNodeCastToComponent : public NkBlueprintNode {
                public:
                    NkNodeCastToComponent() noexcept {
                        Name = "CastTo" + std::string(typeid(T).name());
                        Inputs.push_back({"Target", NkPinType{NkPinPrimitiveType::EntityId}, NkPinDirection::Input});
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Outputs.push_back({"AsComponent", NkPinType{NkPinPrimitiveType::None, NkTypeId(typeid(T).name())}, NkPinDirection::Output});
                        Outputs.push_back({"Success", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"Failed", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    
                    void Execute(NkWorld& world, NkEntityId, float) noexcept override {
                        NkEntityId target = GetInput(0).AsEntityId();
                        if (target.IsValid() && world.IsAlive(target)) {
                            if (T* comp = world.Get<T>(target)) {
                                SetOutput(0, NkValue::Ptr(static_cast<void*>(comp), NkTypeId(typeid(T).name())));
                                SetOutput(1, NkValue::Exec()); // Success
                                return;
                            }
                        }
                        SetOutput(2, NkValue::Exec()); // Failed
                    }
                    
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "ECS"; }
            };

            // Cast vers une classe d'acteur dérivée (NkActor → NkCharacter)
            class NkNodeCastActorToClass : public NkBlueprintNode {
                public:
                    NkTypeId TargetClassType;
                    
                    NkNodeCastActorToClass() noexcept = default;
                    explicit NkNodeCastActorToClass(const NkTypeId& targetType) noexcept : TargetClassType(targetType) {
                        Name = "CastActorToClass";
                        Inputs.push_back({"Actor", NkPinType{NkPinPrimitiveType::GameObject}, NkPinDirection::Input});
                        Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
                        Outputs.push_back({"AsClass", NkPinType{NkPinPrimitiveType::None, TargetClassType}, NkPinDirection::Output});
                        Outputs.push_back({"Success", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                        Outputs.push_back({"Failed", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
                    }
                    
                    void Execute(NkWorld& world, NkEntityId, float) noexcept override {
                        NkEntityId actorId = GetInput(0).AsEntityId();
                        if (actorId.IsValid() && world.IsAlive(actorId)) {
                            // En production : utiliser dynamic_cast ou système de réflexion
                            // Ici : vérification simplifiée via NkTag ou métadonnées
                            if (auto* tag = world.Get<NkTag>(actorId)) {
                                // Exemple : vérifier un tag spécifique
                                // if (tag->Has(NkTagBit::Character) && TargetClassType.Hash == NkTypeId("NkCharacter").Hash)
                                SetOutput(0, NkValue::Entity(actorId));
                                SetOutput(1, NkValue::Exec()); // Success
                                return;
                            }
                        }
                        SetOutput(2, NkValue::Exec()); // Failed
                    }
                    
                    [[nodiscard]] const char* GetCategory() const noexcept override { return "Actors"; }
            };

            // Enregistrement des nœuds de cast
            NK_REGISTER_BLUEPRINT_NODE(NkNodeCastIntToFloat)
            NK_REGISTER_BLUEPRINT_NODE(NkNodeCastFloatToInt)
            NK_REGISTER_BLUEPRINT_NODE(NkNodeCastEntityToGameObject)
            // NK_REGISTER_BLUEPRINT_NODE(NkNodeCastToType) // Requiert un type cible spécifique
            // Les NkNodeCastToComponent<T> doivent être instanciés manuellement pour chaque type T

        } // namespace blueprint
    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKBLUEPRINT.H
// =============================================================================
/*
// -----------------------------------------------------------------------------
// Exemple 1 : Construction d'un Blueprint "Math & Spawn"
// -----------------------------------------------------------------------------
void Exemple_BlueprintMathSpawn(nkentseu::ecs::NkWorld& world) {
    using namespace nkentseu::ecs::blueprint;

    auto go = world.CreateGameObject("Spawner");
    auto* bp = go.Add<NkBlueprintComponent>();

    // 1. Node Event
    auto eventTick = std::make_unique<NkNode_EventBeginPlay>();
    bp->Graph.Nodes.push_back(std::move(eventTick)); // Index 0

    // 2. Node Math (Add 100.0f)
    auto addNode = std::make_unique<NkNode_AddFloat>();
    bp->Graph.Nodes.push_back(std::move(addNode));   // Index 1

    // 3. Node Spawn (Spawn à la position X=100)
    auto spawnNode = std::make_unique<NkNode_SpawnActor>();
    bp->Graph.Nodes.push_back(std::move(spawnNode)); // Index 2

    // Connexions : Event -> Spawn
    bp->Graph.Link(0, 0, 2, 0); // Event.OutExec -> Spawn.InExec
    
    // Connexions Données : Math.Result -> Spawn.Location.X (Simplifié, faudrait un MakeVec3)
    // Ici on connecte l'idée.
    
    // Exécution
    // Le système NkScriptSystem appellera bp->OnStart() -> bp->OnUpdate(dt)
    // Le graphe s'exécutera.
}

// -----------------------------------------------------------------------------
// Exemple 2 : Création de Nœuds Personnalisés (Custom Struct/Class)
// -----------------------------------------------------------------------------

struct MyDamageEvent {
    float Amount = 0.f;
    NkEntityId Attacker = NkEntityId::Invalid();
};
// Enregistrement du type pour le système de pins
NK_REGISTER_TYPE(MyDamageEvent); // Macro hypothétique

class NkNode_ApplyDamage : public NkBlueprintNode {
    public:
        NkNode_ApplyDamage() noexcept {
            Name = "ApplyDamage";
            // Pin d'entrée de type Custom (Struct MyDamageEvent)
            Inputs.push_back({"Event", NkPinType{NkTypeId("MyDamageEvent")}, NkPinDirection::Input});
            Inputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Input});
            Outputs.push_back({"Exec", NkPinType{NkPinPrimitiveType::Exec}, NkPinDirection::Output});
        }
        void Execute(NkWorld& world, NkEntityId self, float) noexcept override {
            // Récupérer la struct via le pointeur générique
            auto* dmgEvent = GetInput(0).AsPtr<MyDamageEvent>();
            if (dmgEvent) {
                // Logique de dégâts...
                printf("Appliqué %.2f dégâts\n", dmgEvent->Amount);
            }
        }
        [[nodiscard]] const char* GetCategory() const noexcept override { return "Gameplay"; }
};
NK_REGISTER_BLUEPRINT_NODE(NkNode_ApplyDamage);
*/