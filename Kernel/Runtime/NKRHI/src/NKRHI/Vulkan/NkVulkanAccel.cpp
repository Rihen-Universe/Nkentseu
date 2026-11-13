// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkVulkanAccel.cpp — (13/11/2026, plan RTX etape 1) LES STRUCTURES D'ACCELERATION du lancer de rayons materiel
// (VK_KHR_acceleration_structure) : BLAS de triangles, TLAS d'instances, construites sur la carte et attendues.
// Actives seulement si NkVulkanDesc.rayTracing est demande ET que la carte les a (CreateLogicalDevice) ; sinon
// chaque appel repond comme NkIDevice : « non pris en charge » (poignee nulle, taille et adresse 0).
// Le plan : D:\Projets\2026\Nkentseu\Tests\rendu_realiste\PLAN_RTX_NKSL.md (etape 1).
// =============================================================================
#ifdef NK_RHI_VK_ENABLED
#include "NkVulkanDevice.h"
#include "NKLogger/NkLog.h"
#include "NKPlatform/NkEnv.h"

#define NK_VKA_LOG(...) logger_src.Infof("[NkRHI_VK] " __VA_ARGS__)
#define NK_VKA_ERR(...) logger_src.Errorf("[NkRHI_VK][ERR] " __VA_ARGS__)

namespace nkentseu {

	namespace {
		bool MemeNom(const char *a, const char *b) {
			if (!a || !b)
				return false;
			while (*a && *a == *b) {
				++a;
				++b;
			}
			return *a == *b;
		}
	} // namespace

	// les trois extensions et une carte en Vulkan 1.2 au moins
	bool NkVulkanDevice::CarteAExtensionsRT() const {
		if (mPhysicalDevice == VK_NULL_HANDLE)
			return false;
		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
		if (props.apiVersion < VK_API_VERSION_1_2)
			return false;
		uint32 n = 0;
		vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &n, nullptr);
		NkVector<VkExtensionProperties> exts(n);
		vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &n, exts.Data());
		const char *voulues[3] = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME,
								  VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
		for (uint32 k = 0; k < 3; ++k) {
			bool vue = false;
			for (uint32 e = 0; e < n && !vue; ++e)
				vue = MemeNom(exts[e].extensionName, voulues[k]);
			if (!vue)
				return false;
		}
		return true;
	}

	bool NkVulkanDevice::ChargerFonctionsRT() {
		mVkCreateAS = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(mDevice, "vkCreateAccelerationStructureKHR");
		mVkDestroyAS = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(mDevice, "vkDestroyAccelerationStructureKHR");
		mVkTaillesAS = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(mDevice, "vkGetAccelerationStructureBuildSizesKHR");
		mVkCmdConstruireAS =
			(PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(mDevice, "vkCmdBuildAccelerationStructuresKHR");
		mVkAdresseAS = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(
			mDevice, "vkGetAccelerationStructureDeviceAddressKHR");
		mVkAdresseTampon = (PFN_vkGetBufferDeviceAddress)vkGetDeviceProcAddr(mDevice, "vkGetBufferDeviceAddress");
		// l'alignement des tampons de travail (scratch) de la construction
		VkPhysicalDeviceAccelerationStructurePropertiesKHR pas{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
		VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		p2.pNext = &pas;
		auto pProps2 = (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(mInstance, "vkGetPhysicalDeviceProperties2");
		if (pProps2) {
			pProps2(mPhysicalDevice, &p2);
			if (pas.minAccelerationStructureScratchOffsetAlignment > 0)
				mRtAlignScratch = pas.minAccelerationStructureScratchOffsetAlignment;
		}
		return mVkCreateAS && mVkDestroyAS && mVkTaillesAS && mVkCmdConstruireAS && mVkAdresseAS && mVkAdresseTampon;
	}

	// un tampon dont la carte connait l'ADRESSE (bufferDeviceAddress) ; `visible` : memoire ecrite par le processeur
	bool NkVulkanDevice::CreerTamponRT(uint64 taille, VkBufferUsageFlags usage, bool visible, VkBuffer &b, VkDeviceMemory &m,
									   void **map) {
		b = VK_NULL_HANDLE;
		m = VK_NULL_HANDLE;
		VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bci.size = taille > 0 ? taille : 16u;
		bci.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(mDevice, &bci, nullptr, &b) != VK_SUCCESS)
			return false;
		VkMemoryRequirements req{};
		vkGetBufferMemoryRequirements(mDevice, b, &req);
		VkMemoryAllocateFlagsInfo fl{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
		fl.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
		VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		mai.pNext = &fl;
		mai.allocationSize = req.size;
		mai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, visible
																	? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
																	: VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (vkAllocateMemory(mDevice, &mai, nullptr, &m) != VK_SUCCESS) {
			vkDestroyBuffer(mDevice, b, nullptr);
			b = VK_NULL_HANDLE;
			return false;
		}
		vkBindBufferMemory(mDevice, b, m, 0);
		if (map) {
			*map = nullptr;
			if (visible)
				vkMapMemory(mDevice, m, 0, req.size, 0, map);
		}
		return true;
	}

	void NkVulkanDevice::DetruireTamponRT(VkBuffer &b, VkDeviceMemory &m) {
		if (b != VK_NULL_HANDLE)
			vkDestroyBuffer(mDevice, b, nullptr);
		if (m != VK_NULL_HANDLE)
			vkFreeMemory(mDevice, m, nullptr);
		b = VK_NULL_HANDLE;
		m = VK_NULL_HANDLE;
	}

	uint64 NkVulkanDevice::AdresseTamponRT(VkBuffer b) const {
		VkBufferDeviceAddressInfo ai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
		ai.buffer = b;
		return (uint64)mVkAdresseTampon(mDevice, &ai);
	}

	// la construction commune : tailles, stockage, structure, tampon de travail, commande unique ATTENDUE
	NkAccelStructHandle NkVulkanDevice::ConstruireAS(bool haut, const VkAccelerationStructureGeometryKHR &geo, uint32 primitives,
													 const char *nom) {
		VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
		bi.type = haut ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		bi.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		bi.geometryCount = 1;
		bi.pGeometries = &geo;
		VkAccelerationStructureBuildSizesInfoKHR tailles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
		mVkTaillesAS(mDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, &primitives, &tailles);
		if (tailles.accelerationStructureSize == 0) {
			NK_VKA_ERR("RTX : taille de structure nulle (%s)\n", nom ? nom : "?");
			return NkAccelStructHandle::Null();
		}
		NkVkAccel a;
		a.haut = haut;
		a.taille = (uint64)tailles.accelerationStructureSize;
		if (!CreerTamponRT(a.taille, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, false, a.buffer, a.memory, nullptr)) {
			NK_VKA_ERR("RTX : stockage de la structure refuse (%llu octets)\n", (unsigned long long)a.taille);
			return NkAccelStructHandle::Null();
		}
		VkAccelerationStructureCreateInfoKHR ci{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		ci.buffer = a.buffer;
		ci.size = tailles.accelerationStructureSize;
		ci.type = bi.type;
		if (mVkCreateAS(mDevice, &ci, nullptr, &a.as) != VK_SUCCESS) {
			DetruireTamponRT(a.buffer, a.memory);
			NK_VKA_ERR("RTX : vkCreateAccelerationStructureKHR refuse\n");
			return NkAccelStructHandle::Null();
		}
		// le tampon de travail, aligne a la main (l'adresse, pas seulement le decalage)
		VkBuffer travail = VK_NULL_HANDLE;
		VkDeviceMemory memTravail = VK_NULL_HANDLE;
		const uint64 al = mRtAlignScratch;
		if (!CreerTamponRT((uint64)tailles.buildScratchSize + al, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, travail, memTravail,
						   nullptr)) {
			mVkDestroyAS(mDevice, a.as, nullptr);
			DetruireTamponRT(a.buffer, a.memory);
			NK_VKA_ERR("RTX : tampon de travail refuse (%llu octets)\n", (unsigned long long)tailles.buildScratchSize);
			return NkAccelStructHandle::Null();
		}
		const uint64 adrTravail = (AdresseTamponRT(travail) + al - 1u) / al * al;
		bi.dstAccelerationStructure = a.as;
		bi.scratchData.deviceAddress = adrTravail;
		VkAccelerationStructureBuildRangeInfoKHR plage{};
		plage.primitiveCount = primitives;
		const VkAccelerationStructureBuildRangeInfoKHR *pPlage = &plage;
		VkCommandBuffer cmd = BeginOneShot();
		mVkCmdConstruireAS(cmd, 1, &bi, &pPlage);
		EndOneShot(cmd); // soumet et ATTEND la file : la structure est prete au retour
		DetruireTamponRT(travail, memTravail);
		VkAccelerationStructureDeviceAddressInfoKHR ai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
		ai.accelerationStructure = a.as;
		a.adresse = (uint64)mVkAdresseAS(mDevice, &ai);
		const uint64 id = NextId();
		mAccels[id] = a;
		NK_VKA_LOG("RTX : %s construite (%s) : %u primitive(s), %llu octets, travail %llu octets\n", haut ? "TLAS" : "BLAS",
				   nom ? nom : "sans nom", (unsigned)primitives, (unsigned long long)a.taille,
				   (unsigned long long)tailles.buildScratchSize);
		return NkAccelStructHandle{id};
	}

	NkAccelStructHandle NkVulkanDevice::CreateBottomLevelAS(const NkBlasDesc &desc) {
		if (!mRtActif || !desc.positions || desc.vertexCount == 0 || !desc.indices || desc.indexCount < 3 ||
			desc.strideOctets < 12u)
			return NkAccelStructHandle::Null();
		threading::NkScopedLockMutex lock(mMutex);
		// les entrees de la construction, copiees en memoire visible (positions serrees a 12 octets)
		const uint64 tailleV = (uint64)desc.vertexCount * 12u, tailleI = (uint64)desc.indexCount * 4u;
		VkBuffer bv = VK_NULL_HANDLE, bix = VK_NULL_HANDLE;
		VkDeviceMemory mv = VK_NULL_HANDLE, mi = VK_NULL_HANDLE;
		void *pv = nullptr;
		void *pi = nullptr;
		const VkBufferUsageFlags entree = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		if (!CreerTamponRT(tailleV, entree, true, bv, mv, &pv) || !CreerTamponRT(tailleI, entree, true, bix, mi, &pi) || !pv ||
			!pi) {
			DetruireTamponRT(bv, mv);
			DetruireTamponRT(bix, mi);
			NK_VKA_ERR("RTX : tampons d'entree de la BLAS refuses\n");
			return NkAccelStructHandle::Null();
		}
		float32 *dv = (float32 *)pv;
		const uint8 *src = (const uint8 *)desc.positions;
		for (uint32 v = 0; v < desc.vertexCount; ++v) {
			const float32 *p = (const float32 *)(src + (uint64)v * desc.strideOctets);
			dv[v * 3u] = p[0];
			dv[v * 3u + 1u] = p[1];
			dv[v * 3u + 2u] = p[2];
		}
		uint32 *di = (uint32 *)pi;
		for (uint32 i = 0; i < desc.indexCount; ++i)
			di[i] = desc.indices[i];
		VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geo.flags = desc.opaque ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0u;
		VkAccelerationStructureGeometryTrianglesDataKHR &tri = geo.geometry.triangles;
		tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		tri.vertexData.deviceAddress = AdresseTamponRT(bv);
		tri.vertexStride = 12u;
		tri.maxVertex = desc.vertexCount - 1u;
		tri.indexType = VK_INDEX_TYPE_UINT32;
		tri.indexData.deviceAddress = AdresseTamponRT(bix);
		const NkAccelStructHandle h = ConstruireAS(false, geo, desc.indexCount / 3u, desc.debugName);
		DetruireTamponRT(bv, mv);
		DetruireTamponRT(bix, mi);
		return h;
	}

	NkAccelStructHandle NkVulkanDevice::CreateTopLevelAS(const NkTlasDesc &desc) {
		if (!mRtActif || !desc.instances || desc.instanceCount == 0)
			return NkAccelStructHandle::Null();
		threading::NkScopedLockMutex lock(mMutex);
		// NK_RTX_MUTATION=1 : la contre-epreuve du banc -- la transformation est recopiee TRANSPOSEE (colonnes pour lignes)
		static const bool mutation = env::GetEnvVar("NK_RTX_MUTATION") != nullptr;
		VkBuffer bi = VK_NULL_HANDLE;
		VkDeviceMemory mi = VK_NULL_HANDLE;
		void *pi = nullptr;
		if (!CreerTamponRT((uint64)desc.instanceCount * sizeof(VkAccelerationStructureInstanceKHR),
						   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, true, bi, mi, &pi) ||
			!pi) {
			DetruireTamponRT(bi, mi);
			NK_VKA_ERR("RTX : tampon des instances refuse\n");
			return NkAccelStructHandle::Null();
		}
		VkAccelerationStructureInstanceKHR *inst = (VkAccelerationStructureInstanceKHR *)pi;
		for (uint32 k = 0; k < desc.instanceCount; ++k) {
			const NkTlasInstance &s = desc.instances[k];
			auto bit = mAccels.Find(s.blas.id);
			if (!bit || bit->haut) {
				DetruireTamponRT(bi, mi);
				NK_VKA_ERR("RTX : instance %u sans BLAS valide\n", (unsigned)k);
				return NkAccelStructHandle::Null();
			}
			VkAccelerationStructureInstanceKHR v{};
			for (uint32 r = 0; r < 3; ++r)
				for (uint32 c = 0; c < 4; ++c)
					v.transform.matrix[r][c] = (mutation && c < 3) ? s.transform[c * 4 + r] : s.transform[r * 4 + c];
			v.instanceCustomIndex = s.instanceId & 0xFFFFFFu;
			v.mask = s.mask;
			v.instanceShaderBindingTableRecordOffset = 0;
			v.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			v.accelerationStructureReference = bit->adresse;
			inst[k] = v;
		}
		VkAccelerationStructureGeometryKHR geo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		geo.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		geo.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		geo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		geo.geometry.instances.arrayOfPointers = VK_FALSE;
		geo.geometry.instances.data.deviceAddress = AdresseTamponRT(bi);
		const NkAccelStructHandle h = ConstruireAS(true, geo, desc.instanceCount, desc.debugName);
		DetruireTamponRT(bi, mi);
		return h;
	}

	void NkVulkanDevice::DestroyAccelStruct(NkAccelStructHandle &handle) {
		if (handle.IsValid() && mRtActif) {
			threading::NkScopedLockMutex lock(mMutex);
			auto it = mAccels.Find(handle.id);
			if (it) {
				vkDeviceWaitIdle(mDevice); // une commande en vol peut encore la lire
				mVkDestroyAS(mDevice, it->as, nullptr);
				DetruireTamponRT(it->buffer, it->memory);
				mAccels.Erase(handle.id);
			}
		}
		handle = NkAccelStructHandle::Null();
	}

	uint64 NkVulkanDevice::GetAccelStructSize(NkAccelStructHandle handle) const {
		threading::NkScopedLockMutex lock(mMutex);
		auto it = mAccels.Find(handle.id);
		return it ? it->taille : 0u;
	}

	uint64 NkVulkanDevice::GetAccelStructDeviceAddress(NkAccelStructHandle handle) const {
		threading::NkScopedLockMutex lock(mMutex);
		auto it = mAccels.Find(handle.id);
		return it ? it->adresse : 0u;
	}

	// a l'arret du dispositif (mMutex non pris : Shutdown)
	void NkVulkanDevice::DetruireAccels() {
		if (!mRtActif)
			return;
		for (auto &[id, a] : mAccels) {
			(void)id;
			if (a.as != VK_NULL_HANDLE)
				mVkDestroyAS(mDevice, a.as, nullptr);
			DetruireTamponRT(a.buffer, a.memory);
		}
		mAccels.Clear();
	}

} // namespace nkentseu

#endif // NK_RHI_VK_ENABLED
