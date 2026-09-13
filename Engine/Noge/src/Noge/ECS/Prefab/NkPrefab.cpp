// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// FICHIER: NKECS/Prefab/NkPrefab.cpp
// DESCRIPTION: Implémentations des méthodes d'instanciation et sérialisation.
// =============================================================================
#include "NkPrefab.h"
#include "NKCore/Text/NkSnprintf.h"
#include "NKECS/Reflect/NkReflect.h"
#include "Noge/ECS/Components/SceneComponent/NkSceneComponent.h" // NkSceneComponent
#include "NKContainers/String/NkFormat.h"						 // NkFormat
// Pont de (de)serialisation type-erase : ComponentMeta.serialize / .deserialize,
// branches par NkRegisterComponentReflection<T>() (Phase 4). C'est le chemin
// VIVANT ; celui de reflect::NkTypeInfo ne l'est pas — voir la note dans
// Instantiate().
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
	// NkPrefab vit dans nkentseu ; on importe ecs pour les types composants.
	using namespace ecs;

	// ============================================================================
	// 2026-09-13 — les trois ecrivains JSON maison ont ete RETIRES
	// ============================================================================
	// WriteJsonString / WriteJsonFloat / WriteJsonVec3 poussaient des octets a la
	// main. Deux d'entre eux n'honoraient PAS bufSize sur les noms de cles : un
	// prefab un peu gros debordait le tampon de l'appelant en silence. Serialize()
	// passe desormais par NkArchive + NkJSONWriter, qui echappent et bornent pour
	// nous, et plus personne n'appelait ces trois fonctions.

	// ============================================================================
	// NkPrefab::Instantiate — Création d'une instance dans le monde
	// ============================================================================

	NkEntityId NkPrefab::Instantiate(NkWorld &world, const char *instanceName) const noexcept {
		// 1. Création de l'entité racine
		NkEntityId rootId = world.CreateEntity();
		world.Add<NkName>(rootId, NkName(instanceName ? instanceName : name.CStr()));
		world.Add<NkTag>(rootId);
		world.Add<NkTransform>(rootId);
		world.Add<NkParent>(rootId);
		world.Add<NkChildren>(rootId);
		world.Add<NkBehaviourHost>(rootId);

		// 2. Application des composants définis dans le prefab
		//
		// 2026-09-13 — LE TODO A DISPARU AVEC SA CAUSE.
		//
		// L'ancien code cherchait le type dans `reflect::NkReflectRegistry` et
		// testait `info->deserialize`. Ce test était TOUJOURS FAUX : la macro
		// `NK_REFLECT_END` ne remplit jamais `NkTypeInfo::serialize` ni
		// `::deserialize` (elle ne pose que componentId/name/size/align/fields).
		// Le corps du `if` — celui qui portait le TODO — n'était donc même pas
		// atteint : ni malloc, ni désérialisation, ni attache. Deux trous
		// superposés, et c'est le premier qui masquait le second.
		//
		// Le chemin vivant est celui de la Phase 4 : les hooks type-erasés que
		// `NkRegisterComponentReflection<T>()` branche dans `ecs::ComponentMeta`.
		// Et l'attache manquante est désormais `NkWorld::AddRaw`.
		//
		// L'ordre compte : on attache D'ABORD (le composant est alors construit
		// par défaut dans son archétype), puis on le remplit EN PLACE. Aucun
		// malloc, donc aucune fuite possible dans le cas d'échec — le bug que
		// l'ancien commentaire décrivait ne peut plus exister.
		for (const auto &[typeName, data] : components) {
			// Nom écrit dans le fichier -> identifiant de composant.
			const NkComponentId cid = NkTypeRegistry::Global().FindIdByName(typeName.CStr());
			if (cid == kInvalidComponentId) {
				// Type absent du processus : personne n'a touché NkIdOf<T>() ni
				// NK_COMPONENT(T) dans ce binaire. On saute — jamais d'attache
				// à l'aveugle.
				continue;
			}

			const ComponentMeta *meta = NkTypeRegistry::Global().Get(cid);
			if (meta == nullptr) {
				continue;
			}

			// Attache type-erasée : c'est « la brique » (NKECS, 2026-09-13).
			if (!world.AddRaw(rootId, cid, nullptr)) {
				continue;
			}

			// Un tag n'a pas de slot : il est attaché, il n'y a rien à remplir.
			void *dst = world.GetRaw(rootId, cid);
			if (dst == nullptr || meta->deserialize == nullptr) {
				continue;
			}

			// JSON du prefab -> archive -> composant, en place.
			NkArchive ar;
			NkString parseErr;
			if (!NkJSONReader::ReadArchive(NkStringView(data.jsonValue.CStr()), ar, &parseErr)) {
				continue;
			}
			meta->deserialize(dst, ar);
		}

		// 3. Construction du GameObject
		NkGameObject go(rootId, &world);

		// 4. Instanciation récursive des enfants
		for (const auto &childData : children) {
			NkGameObject childGO;
			if (!childData.prefabPath.Empty()) {
				// Instanciation d'un prefab imbriqué
				const NkPrefab *nested = NkPrefabRegistry::Global().Get(childData.prefabPath.CStr());
				if (nested) {
					childGO = NkGameObject(nested->Instantiate(world, childData.name.CStr()), &world);
				}
			} else {
				// Création d'un nœud simple (entité + nom, wrappée en GameObject)
				NkEntityId e = world.CreateEntity();
				world.Add<NkName>(e, NkName(childData.name.CStr()));
				childGO = NkGameObject(e, &world);
			}

			if (childGO.IsValid()) {
				// Appliquer la transform locale
				if (auto t = childGO.GetComponent<NkSceneComponent>()) {
					t->SetLocalTransform(childData.localPosition, childData.localRotation, childData.localScale);
				}
				// Attacher à la racine
				go.AddChild(childGO);
				if (!childData.isActive) {
					childGO.SetActive(false);
				}
			}
		}

		// 5. Chargement du Blueprint optionnel
		if (!blueprintPath.Empty()) {
			// En production : charger et attacher le blueprint via NkBlueprintComponent
			// auto* bp = go.Add<NkBlueprintComponent>();
			// if (bp) bp->LoadFromFile(blueprintPath.CStr());
		}

		// 6. Activation finale (les entités sont créées inactives par défaut)
		go.SetActive(true);

		return rootId; // contrat header : Instantiate() retourne l'entité (bas niveau)
	}

	// ============================================================================
	// NkPrefab::SerializeComponent — LE CORPS QUI N'EXISTAIT NULLE PART
	// ============================================================================
	// Declaree (NkPrefab.h) et appelee (WithComponent<T>), cette fonction n'avait
	// AUCUN corps dans tout le depot. Rien ne rougissait, parce que rien
	// n'appelait WithComponent<T> : une fonction sans corps ne casse le lien que
	// le jour ou quelqu'un s'en sert. Le banc de ce chantier s'en sert.
	//
	// Symetrique exact du chemin de lecture d'Instantiate : hooks type-erases du
	// ComponentMeta -> NkArchive -> JSON.
	bool NkPrefab::SerializeComponent(void *data, NkComponentId cid, char *outJson, uint32 bufSize) noexcept {
		if (data == nullptr || outJson == nullptr || bufSize == 0u) {
			return false;
		}
		outJson[0] = '\0';

		const ComponentMeta *meta = NkTypeRegistry::Global().Get(cid);
		if (meta == nullptr || meta->serialize == nullptr) {
			// Le composant n'a pas de reflexion branchee : appeler
			// ecs::reflect::NkRegisterComponentReflection<T>() au demarrage.
			// On refuse plutot que d'ecrire un objet vide qui se relirait en
			// silence comme un composant a zero.
			return false;
		}

		NkArchive ar;
		meta->serialize(data, ar);

		NkString json;
		if (!NkJSONWriter::WriteArchive(ar, json, false)) {
			return false;
		}

		const nk_usize len = static_cast<nk_usize>(json.Size());
		if (len + 1u > static_cast<nk_usize>(bufSize)) {
			// Refus propre : une troncature silencieuse produirait un JSON
			// invalide que la relecture rejetterait bien plus loin, sans dire
			// pourquoi.
			return false;
		}

		std::memcpy(outJson, json.CStr(), len);
		outJson[len] = '\0';
		return true;
	}

	// ============================================================================
	// NkPrefab::InstantiateBatch — Création de multiples instances
	// ============================================================================

	void NkPrefab::InstantiateBatch(NkWorld &world, uint32 count, NkVector<NkEntityId> &out) const noexcept {
		out.Reserve(out.Size() + count);
		for (uint32 i = 0; i < count; ++i) {
			NkString instanceName = NkFormat("{0}_{1}", name, i);
			out.PushBack(Instantiate(world, instanceName.CStr()));
		}
	}

	// ============================================================================
	// NkPrefab::Serialize — Sérialisation en JSON
	// ============================================================================

	bool NkPrefab::Serialize(char *buffer, uint32 bufSize) const noexcept {
		if (!buffer || bufSize < 64) {
			return false;
		}
		buffer[0] = '\0';

		NkArchive ar;
		ar.SetString("name", NkStringView(name.CStr()));
		ar.SetString("path", NkStringView(path.CStr()));
		ar.SetString("version", NkStringView(version.CStr()));
		ar.SetUInt64("guid", guid);
		ar.SetUInt64("tagBits", tagBits);
		ar.SetUInt32("layer", layer);
		if (!blueprintPath.Empty()) {
			ar.SetString("blueprintPath", NkStringView(blueprintPath.CStr()));
		}

		// ── Composants ──────────────────────────────────────────────────
		NkArchive comps;
		for (const auto &[typeName, data] : components) {
			NkArchive one;
			one.SetString("json", NkStringView(data.jsonValue.CStr()));
			one.SetBool("overridden", data.isOverridden);
			comps.SetObject(NkStringView(typeName.CStr()), one);
		}
		ar.SetObject("components", comps);

		// ── Enfants ─────────────────────────────────────────────────────
		NkVector<NkArchive> kids;
		for (const auto &child : children) {
			NkArchive k;
			k.SetString("name", NkStringView(child.name.CStr()));
			if (!child.prefabPath.Empty()) {
				k.SetString("prefabPath", NkStringView(child.prefabPath.CStr()));
			}
			k.SetFloat32("px", child.localPosition.x);
			k.SetFloat32("py", child.localPosition.y);
			k.SetFloat32("pz", child.localPosition.z);
			k.SetFloat32("sx", child.localScale.x);
			k.SetFloat32("sy", child.localScale.y);
			k.SetFloat32("sz", child.localScale.z);
			k.SetBool("isActive", child.isActive);
			kids.PushBack(k);
		}
		if (!kids.Empty()) {
			ar.SetObjectArray("children", kids);
		}

		NkString json;
		if (!NkJSONWriter::WriteArchive(ar, json, false)) {
			return false;
		}

		const nk_usize len = static_cast<nk_usize>(json.Size());
		if (len + 1u > static_cast<nk_usize>(bufSize)) {
			// Refus propre plutot que troncature : l'ancienne version ecrivait
			// octet par octet SANS verifier bufSize sur les cles, et un prefab
			// un peu gros debordait le tampon de l'appelant en silence.
			return false;
		}
		std::memcpy(buffer, json.CStr(), len);
		buffer[len] = '\0';
		return true;
	}

	// ============================================================================
	// NkPrefab::Deserialize — Désérialisation depuis JSON
	// ============================================================================
	// 2026-09-13 — CE QUI MANQUAIT ICI ETAIT AUSSI GRAVE QUE LE TODO DE LA
	// LIGNE 101 : l'ancienne version extrayait name/path/version/blueprintPath
	// a coups de strstr et NE LISAIT NI LES COMPOSANTS NI LES ENFANTS. Un prefab
	// ecrit puis relu revenait donc VIDE de composants — l'aller-retour ne
	// pouvait pas marcher meme une fois l'attache reparee. Reecrit sur
	// NkArchive + NkJSONReader, comme la note de NkJsonSerialization.h le
	// demandait.

	bool NkPrefab::Deserialize(const char *json) noexcept {
		if (!json) {
			return false;
		}

		NkArchive ar;
		NkString parseErr;
		if (!NkJSONReader::ReadArchive(NkStringView(json), ar, &parseErr)) {
			return false;
		}

		ar.GetString("name", name);
		ar.GetString("path", path);
		ar.GetString("version", version);

		nk_uint64 g = 0u;
		if (ar.GetUInt64("guid", g) && g != 0u) {
			guid = g;
		} else if (!path.Empty()) {
			guid = ecs::detail::FNV1a(path.CStr());
		}

		nk_uint64 tb = 0u;
		if (ar.GetUInt64("tagBits", tb)) {
			tagBits = tb;
		}
		nk_uint32 ly = 0u;
		if (ar.GetUInt32("layer", ly)) {
			layer = ly;
		}
		ar.GetString("blueprintPath", blueprintPath);

		// ── Composants ──────────────────────────────────────────────────
		components.Clear();
		NkArchive comps;
		if (ar.GetObject("components", comps)) {
			const NkVector<NkArchiveEntry> &entries = comps.Entries();
			for (nk_usize i = 0; i < entries.Size(); ++i) {
				const NkString &typeName = entries[i].key;
				NkArchive one;
				if (!comps.GetObject(NkStringView(typeName.CStr()), one)) {
					continue;
				}
				NkString js;
				one.GetString("json", js);
				nk_bool overridden = false;
				one.GetBool("overridden", overridden);

				NkPrefabComponentData data(typeName.CStr(), js.CStr());
				data.isOverridden = (overridden != false);
				components[typeName] = data;
			}
		}

		// ── Enfants ─────────────────────────────────────────────────────
		children.Clear();
		NkVector<NkArchive> kids;
		if (ar.GetObjectArray("children", kids)) {
			for (nk_usize i = 0; i < kids.Size(); ++i) {
				NkArchive &k = kids[i];
				NkPrefabChild child;
				NkString n;
				k.GetString("name", n);
				child.name = n;
				k.GetString("prefabPath", child.prefabPath);
				k.GetFloat32("px", child.localPosition.x);
				k.GetFloat32("py", child.localPosition.y);
				k.GetFloat32("pz", child.localPosition.z);
				k.GetFloat32("sx", child.localScale.x);
				k.GetFloat32("sy", child.localScale.y);
				k.GetFloat32("sz", child.localScale.z);
				nk_bool active = true;
				if (k.GetBool("isActive", active)) {
					child.isActive = (active != false);
				}
				children.PushBack(child);
			}
		}

		return true;
	}

	// ============================================================================
	// NkPrefabRegistry::LoadFromFile — Chargement d'un prefab depuis un fichier
	// ============================================================================

	bool NkPrefabRegistry::LoadFromFile(const char *filePath) noexcept {
		if (!filePath) {
			return false;
		}

		// Ouvrir le fichier
		FILE *f = std::fopen(filePath, "rb");
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
			prefab.guid = ecs::detail::FNV1a(filePath);
		}

		// Enregistrer dans le registre global
		return Register(prefab);
	}

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