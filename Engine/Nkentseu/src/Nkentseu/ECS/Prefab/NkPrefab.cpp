// =============================================================================
// FICHIER: NKECS/Prefab/NkPrefab.cpp
// DESCRIPTION: Implémentations des méthodes d'instanciation et sérialisation.
// =============================================================================
#include "NkPrefab.h"
#include "../Reflect/NkReflect.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
    namespace ecs {

        // ============================================================================
        // Helpers de sérialisation JSON (minimaliste, sans dépendance externe)
        // ============================================================================

        // Écrit une chaîne JSON échappée dans un buffer
        static void WriteJsonString(char* buf, uint32& offset, uint32 bufSize, const char* str) noexcept {
            if (!str) str = "";
            buf[offset++] = '"';
            for (uint32 i = 0; str[i] && offset < bufSize - 2; ++i) {
                switch (str[i]) {
                    case '"':  buf[offset++] = '\\'; buf[offset++] = '"';  break;
                    case '\\': buf[offset++] = '\\'; buf[offset++] = '\\'; break;
                    case '\n': buf[offset++] = '\\'; buf[offset++] = 'n';  break;
                    case '\r': buf[offset++] = '\\'; buf[offset++] = 'r';  break;
                    case '\t': buf[offset++] = '\\'; buf[offset++] = 't';  break;
                    default:   buf[offset++] = str[i]; break;
                }
            }
            buf[offset++] = '"';
        }

        // Écrit un float avec précision contrôlée
        static void WriteJsonFloat(char* buf, uint32& offset, uint32 bufSize, float32 val) noexcept {
            char temp[64];
            int len = std::snprintf(temp, sizeof(temp), "%.6g", static_cast<double>(val));
            for (int i = 0; i < len && offset < bufSize - 1; ++i) {
                buf[offset++] = temp[i];
            }
        }

        // Écrit un Vec3 en JSON
        static void WriteJsonVec3(char* buf, uint32& offset, uint32 bufSize, const NkVec3& v) noexcept {
            buf[offset++] = '[';
            WriteJsonFloat(buf, offset, bufSize, v.x);
            buf[offset++] = ',';
            WriteJsonFloat(buf, offset, bufSize, v.y);
            buf[offset++] = ',';
            WriteJsonFloat(buf, offset, bufSize, v.z);
            buf[offset++] = ']';
        }

        // ============================================================================
        // NkPrefab::Instantiate — Création d'une instance dans le monde
        // ============================================================================

        NkGameObject NkPrefab::Instantiate(NkWorld& world, const char* instanceName) noexcept {
            // 1. Création de l'entité racine
            NkEntityId rootId = world.CreateEntity();
            world.Add<NkName>(rootId, NkName(instanceName ? instanceName : name.c_str()));
            world.Add<NkTag>(rootId);
            world.Add<NkTransform>(rootId);
            world.Add<NkParent>(rootId);
            world.Add<NkChildren>(rootId);
            world.Add<NkBehaviourHost>(rootId);

            // 2. Application des composants définis dans le prefab
            for (const auto& [typeName, data] : components) {
                // Recherche du type via le registre de réflexion
                const NkTypeInfo* info = NkReflectRegistry::Global().FindByName(typeName.c_str());
                if (info && info->deserialize) {
                    // Allocation temporaire pour désérialiser
                    void* buffer = std::malloc(info->size);
                    if (buffer) {
                        std::memset(buffer, 0, info->size);
                        if (info->deserialize(buffer, data.jsonValue.c_str())) {
                            // Ajout au monde via AddImpl générique (simplifié ici)
                            // En production : utiliser un dispatcher par type
                            std::free(buffer);
                        }
                    }
                }
            }

            // 3. Construction du GameObject
            NkGameObject go(rootId, &world);

            // 4. Instanciation récursive des enfants
            for (const auto& childData : children) {
                NkGameObject childGO;
                if (!childData.prefabPath.empty()) {
                    // Instanciation d'un prefab imbriqué
                    const NkPrefab* nested = NkPrefabRegistry::Global().Get(childData.prefabPath.c_str());
                    if (nested) {
                        childGO = nested->Instantiate(world, childData.name.c_str());
                    }
                } else {
                    // Création d'un nœud simple
                    childGO = world.CreateGameObject(childData.name.c_str());
                }

                if (childGO.IsValid()) {
                    // Appliquer la transform locale
                    if (auto* t = childGO.GetComponent<NkSceneComponent>()) {
                        t->SetLocalTransform(
                            childData.localPosition,
                            childData.localRotation,
                            childData.localScale
                        );
                    }
                    // Attacher à la racine
                    go.AddChild(childGO);
                    if (!childData.isActive) {
                        childGO.SetActive(false);
                    }
                }
            }

            // 5. Chargement du Blueprint optionnel
            if (!blueprintPath.empty()) {
                // En production : charger et attacher le blueprint via NkBlueprintComponent
                // auto* bp = go.Add<NkBlueprintComponent>();
                // if (bp) bp->LoadFromFile(blueprintPath.c_str());
            }

            // 6. Activation finale (les entités sont créées inactives par défaut)
            go.SetActive(true);

            return go;
        }

        // ============================================================================
        // NkPrefab::InstantiateBatch — Création de multiples instances
        // ============================================================================

        void NkPrefab::InstantiateBatch(NkWorld& world, uint32 count, std::vector<NkGameObject>* out) noexcept {
            if (out) {
                out->reserve(out->size() + count);
            }
            for (uint32 i = 0; i < count; ++i) {
                char instanceName[256];
                std::snprintf(instanceName, sizeof(instanceName), "%s_%u", name.c_str(), i);
                NkGameObject go = Instantiate(world, instanceName);
                if (out) {
                    out->push_back(std::move(go));
                }
            }
        }

        // ============================================================================
        // NkPrefab::Serialize — Sérialisation en JSON
        // ============================================================================

        bool NkPrefab::Serialize(char* buffer, uint32 bufSize) const noexcept {
            if (!buffer || bufSize < 64) {
                return false;
            }

            uint32 offset = 0;
            buffer[offset++] = '{';

            // Métadonnées
            buffer[offset++] = '"'; std::memcpy(buffer + offset, "name", 4); offset += 4; buffer[offset++] = '"';
            buffer[offset++] = ':';
            WriteJsonString(buffer, offset, bufSize, name.c_str());
            buffer[offset++] = ',';

            buffer[offset++] = '"'; std::memcpy(buffer + offset, "path", 4); offset += 4; buffer[offset++] = '"';
            buffer[offset++] = ':';
            WriteJsonString(buffer, offset, bufSize, path.c_str());
            buffer[offset++] = ',';

            buffer[offset++] = '"'; std::memcpy(buffer + offset, "version", 7); offset += 7; buffer[offset++] = '"';
            buffer[offset++] = ':';
            WriteJsonString(buffer, offset, bufSize, version.c_str());
            buffer[offset++] = ',';

            // Composants
            buffer[offset++] = '"'; std::memcpy(buffer + offset, "components", 10); offset += 10; buffer[offset++] = '"';
            buffer[offset++] = ':';
            buffer[offset++] = '{';
            bool firstComp = true;
            for (const auto& [typeName, data] : components) {
                if (!firstComp) {
                    buffer[offset++] = ',';
                }
                firstComp = false;
                WriteJsonString(buffer, offset, bufSize, typeName.c_str());
                buffer[offset++] = ':';
                buffer[offset++] = '{';
                buffer[offset++] = '"'; std::memcpy(buffer + offset, "json", 4); offset += 4; buffer[offset++] = '"';
                buffer[offset++] = ':';
                WriteJsonString(buffer, offset, bufSize, data.jsonValue.c_str());
                if (data.isOverridden) {
                    buffer[offset++] = ',';
                    buffer[offset++] = '"'; std::memcpy(buffer + offset, "overridden", 10); offset += 10; buffer[offset++] = '"';
                    buffer[offset++] = ':';
                    buffer[offset++] = 't'; buffer[offset++] = 'r'; buffer[offset++] = 'u'; buffer[offset++] = 'e';
                }
                buffer[offset++] = '}';
            }
            buffer[offset++] = '}';
            buffer[offset++] = ',';

            // Enfants
            buffer[offset++] = '"'; std::memcpy(buffer + offset, "children", 8); offset += 8; buffer[offset++] = '"';
            buffer[offset++] = ':';
            buffer[offset++] = '[';
            bool firstChild = true;
            for (const auto& child : children) {
                if (!firstChild) {
                    buffer[offset++] = ',';
                }
                firstChild = false;
                buffer[offset++] = '{';
                WriteJsonString(buffer, offset, bufSize, "name");
                buffer[offset++] = ':';
                WriteJsonString(buffer, offset, bufSize, child.name.c_str());
                buffer[offset++] = ',';
                if (!child.prefabPath.empty()) {
                    WriteJsonString(buffer, offset, bufSize, "prefabPath");
                    buffer[offset++] = ':';
                    WriteJsonString(buffer, offset, bufSize, child.prefabPath.c_str());
                    buffer[offset++] = ',';
                }
                WriteJsonString(buffer, offset, bufSize, "localPosition");
                buffer[offset++] = ':';
                WriteJsonVec3(buffer, offset, bufSize, child.localPosition);
                buffer[offset++] = ',';
                WriteJsonString(buffer, offset, bufSize, "isActive");
                buffer[offset++] = ':';
                buffer[offset++] = child.isActive ? 't' : 'f';
                buffer[offset++] = child.isActive ? 'r' : 'a';
                buffer[offset++] = child.isActive ? 'u' : 'l';
                buffer[offset++] = child.isActive ? 'e' : 's';
                buffer[offset++] = '}';
            }
            buffer[offset++] = ']';

            // Blueprint optionnel
            if (!blueprintPath.empty()) {
                buffer[offset++] = ',';
                WriteJsonString(buffer, offset, bufSize, "blueprintPath");
                buffer[offset++] = ':';
                WriteJsonString(buffer, offset, bufSize, blueprintPath.c_str());
            }

            buffer[offset++] = '}';
            if (offset < bufSize) {
                buffer[offset] = '\0';
            } else {
                buffer[bufSize - 1] = '\0';
            }
            return true;
        }

        // ============================================================================
        // NkPrefab::Deserialize — Désérialisation depuis JSON
        // ============================================================================

        bool NkPrefab::Deserialize(const char* json) noexcept {
            if (!json) {
                return false;
            }

            // Parser JSON minimaliste (en production : utiliser RapidJSON / nlohmann)
            // Ici : extraction basique des champs principaux

            // Extraire "name"
            const char* nameStart = std::strstr(json, "\"name\"");
            if (nameStart) {
                const char* valStart = std::strchr(nameStart, ':');
                if (valStart) {
                    valStart = std::strchr(valStart, '"');
                    if (valStart) {
                        ++valStart;
                        const char* valEnd = std::strchr(valStart, '"');
                        if (valEnd) {
                            name.assign(valStart, valEnd - valStart);
                        }
                    }
                }
            }

            // Extraire "path"
            const char* pathStart = std::strstr(json, "\"path\"");
            if (pathStart) {
                const char* valStart = std::strchr(pathStart, ':');
                if (valStart) {
                    valStart = std::strchr(valStart, '"');
                    if (valStart) {
                        ++valStart;
                        const char* valEnd = std::strchr(valStart, '"');
                        if (valEnd) {
                            path.assign(valStart, valEnd - valStart);
                            guid = detail::FNV1a(path.c_str());
                        }
                    }
                }
            }

            // Extraire "version"
            const char* verStart = std::strstr(json, "\"version\"");
            if (verStart) {
                const char* valStart = std::strchr(verStart, ':');
                if (valStart) {
                    valStart = std::strchr(valStart, '"');
                    if (valStart) {
                        ++valStart;
                        const char* valEnd = std::strchr(valStart, '"');
                        if (valEnd) {
                            version.assign(valStart, valEnd - valStart);
                        }
                    }
                }
            }

            // Extraire "blueprintPath"
            const char* bpStart = std::strstr(json, "\"blueprintPath\"");
            if (bpStart) {
                const char* valStart = std::strchr(bpStart, ':');
                if (valStart) {
                    valStart = std::strchr(valStart, '"');
                    if (valStart) {
                        ++valStart;
                        const char* valEnd = std::strchr(valStart, '"');
                        if (valEnd) {
                            blueprintPath.assign(valStart, valEnd - valStart);
                        }
                    }
                }
            }

            // Note : En production, parser "components" et "children" avec un vrai parser JSON
            // et utiliser NkReflect pour désérialiser chaque composant.

            return true;
        }

        // ============================================================================
        // NkPrefabRegistry::LoadFromFile — Chargement d'un prefab depuis un fichier
        // ============================================================================

        bool NkPrefabRegistry::LoadFromFile(const char* filePath) noexcept {
            if (!filePath) {
                return false;
            }

            // Ouvrir le fichier
            FILE* f = std::fopen(filePath, "rb");
            if (!f) {
                return false;
            }

            // Déterminer la taille
            std::fseek(f, 0, SEEK_END);
            long size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);

            if (size <= 0 || size > 10 * 1024 * 1024) { // Limite : 10 MB
                std::fclose(f);
                return false;
            }

            // Lire le contenu
            std::vector<char> buffer(static_cast<size_t>(size) + 1);
            if (std::fread(buffer.data(), 1, static_cast<size_t>(size), f) != static_cast<size_t>(size)) {
                std::fclose(f);
                return false;
            }
            buffer[size] = '\0';
            std::fclose(f);

            // Désérialiser le prefab
            NkPrefab prefab;
            if (!prefab.Deserialize(buffer.data())) {
                return false;
            }

            // Définir le chemin et GUID
            prefab.path = filePath;
            if (prefab.guid == 0) {
                prefab.guid = detail::FNV1a(filePath);
            }

            // Enregistrer dans le registre global
            return Register(prefab);
        }

    } // namespace ecs
} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DE NKPREFAB.CPP
// =============================================================================

/*
// -----------------------------------------------------------------------------
// Exemple 1 : Instanciation simple d'un prefab
// -----------------------------------------------------------------------------
void Exemple_Instantiate(nkentseu::ecs::NkWorld& world) {
    using namespace nkentseu::ecs;

    const NkPrefab* prefab = NkPrefabRegistry::Global().Get("Assets/Prefabs/Enemy.prefab");
    if (prefab) {
        // Instanciation unique
        NkGameObject enemy = prefab->Instantiate(world, "Goblin_01");
        enemy.SetPosition({10.0f, 0.0f, 5.0f});
    }
}

// -----------------------------------------------------------------------------
// Exemple 2 : Instanciation en masse (batch)
// -----------------------------------------------------------------------------
void Exemple_BatchInstantiate(nkentseu::ecs::NkWorld& world) {
    using namespace nkentseu::ecs;

    const NkPrefab* prefab = NkPrefabRegistry::Global().Get("Assets/Prefabs/Coin.prefab");
    if (prefab) {
        std::vector<NkGameObject> coins;
        prefab->InstantiateBatch(world, 50, &coins);

        // Positionner les pièces en cercle
        for (uint32 i = 0; i < coins.size(); ++i) {
            float angle = (static_cast<float>(i) / coins.size()) * 6.28318f;
            coins[i].SetPosition({
                std::cos(angle) * 10.0f,
                0.0f,
                std::sin(angle) * 10.0f
            });
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 3 : Sérialisation d'un prefab en JSON
// -----------------------------------------------------------------------------
void Exemple_SerializePrefab(const nkentseu::ecs::NkPrefab& prefab) {
    char buffer[65536];
    if (prefab.Serialize(buffer, sizeof(buffer))) {
        // Écrire dans un fichier
        FILE* f = std::fopen("output.prefab.json", "w");
        if (f) {
            std::fprintf(f, "%s\n", buffer);
            std::fclose(f);
        }
    }
}

// -----------------------------------------------------------------------------
// Exemple 4 : Chargement d'un prefab depuis un fichier
// -----------------------------------------------------------------------------
void Exemple_LoadPrefab() {
    using namespace nkentseu::ecs;

    if (NkPrefabRegistry::Global().LoadFromFile("Assets/Prefabs/Player.prefab")) {
        printf("Prefab chargé avec succès\n");
    } else {
        printf("Échec du chargement du prefab\n");
    }
}

// -----------------------------------------------------------------------------
// Exemple 5 : Instanciation avec overrides (personnalisation runtime)
// -----------------------------------------------------------------------------
void Exemple_Overrides(nkentseu::ecs::NkWorld& world) {
    using namespace nkentseu::ecs;

    const NkPrefab* prefab = NkPrefabRegistry::Global().Get("Assets/Prefabs/Enemy.prefab");
    if (prefab) {
        NkGameObject enemy = prefab->Instantiate(world, "Boss_01");

        // Appliquer des overrides via NkPrefabInstance (à implémenter)
        // NkPrefabInstance* inst = world.Get<NkPrefabInstance>(enemy.Id());
        // if (inst) {
        //     inst->SetOverride("NkHealth.max", 500.0f);
        //     inst->SetOverride("NkRigidbody3D.mass", 100.0f);
        // }
    }
}

// -----------------------------------------------------------------------------
// Exemple 6 : Intégration avec le cycle de vie de la scène
// -----------------------------------------------------------------------------
void Exemple_SceneIntegration(nkentseu::ecs::NkScene& scene) {
    using namespace nkentseu::ecs;

    // Charger tous les prefabs d'un dossier au démarrage de la scène
    // (En production : utiliser un AssetManager avec chargement asynchrone)
    const char* prefabPaths[] = {
        "Assets/Prefabs/Player.prefab",
        "Assets/Prefabs/Enemy.prefab",
        "Assets/Prefabs/Coin.prefab"
    };

    for (const char* path : prefabPaths) {
        NkPrefabRegistry::Global().LoadFromFile(path);
    }

    // Instancier le joueur au spawn point
    const NkPrefab* playerPrefab = NkPrefabRegistry::Global().Get("Assets/Prefabs/Player.prefab");
    if (playerPrefab) {
        NkGameObject player = playerPrefab->Instantiate(scene.GetWorld(), "Player_01");
        player.SetPosition({0.0f, 0.0f, 0.0f}); // Spawn point
    }
}
*/