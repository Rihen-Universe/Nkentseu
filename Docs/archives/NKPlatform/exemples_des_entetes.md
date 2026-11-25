<!-- AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis, Rihen -->
# Les exemples qui étaient en commentaire dans les en-têtes de NKPlatform

Déplacés le 25/11/2026 (règle « en-têtes courts »), tels quels, sans les relire ni les compiler : ils ne sont
PAS une documentation vérifiée. Chacun est à réécrire en exemple compilé dans `Docs/exemples/Foundation/NKPlatform/`
(sans bibliothèque standard), puis à retirer d'ici. Le site ne lit pas ce dossier.

## NkArchDetect.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKARCHDETECT.H
// =============================================================================
// Ce fichier fournit des macros pour détecter l'architecture CPU à la compilation
// et exécuter du code conditionnel optimisé pour chaque plateforme.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Affichage d'informations d'architecture au runtime
// -----------------------------------------------------------------------------
/*
	#include "NkArchDetect.h"
	#include <iostream>

	void PrintArchInfo() {
		std::cout << "Architecture : " << NKENTSEU_ARCH_NAME << "\n";
		std::cout << "Version      : " << NKENTSEU_ARCH_VERSION << "\n";
		std::cout << "Bitness      : " << NKENTSEU_WORD_BITS << " bits\n";
		std::cout << "Endianness   : "
			#ifdef NKENTSEU_ARCH_LITTLE_ENDIAN
				  << "Little-endian\n";
			#elif defined(NKENTSEU_ARCH_BIG_ENDIAN)
				  << "Big-endian\n";
			#else
				  << "Unknown\n";
			#endif

		// Extensions SIMD disponibles
		#if defined(NKENTSEU_CPU_HAS_AVX2)
			std::cout << "SIMD         : AVX2 disponible\n";
		#elif defined(NKENTSEU_CPU_HAS_AVX)
			std::cout << "SIMD         : AVX disponible\n";
		#elif defined(NKENTSEU_CPU_HAS_SSE2)
			std::cout << "SIMD         : SSE2 disponible\n";
		#elif defined(NKENTSEU_CPU_HAS_NEON)
			std::cout << "SIMD         : NEON disponible\n";
		#else
			std::cout << "SIMD         : Aucun support SIMD détecté\n";
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Sélection de chemin de code optimisé par architecture
// -----------------------------------------------------------------------------
/*
	#include "NkArchDetect.h"

	// Fonction de multiplication de vecteurs avec fallback portable
	void VectorMultiply(float* result, const float* a, const float* b, size_t count) {
		NKENTSEU_AVX2_ONLY({
			// Chemin optimisé AVX2 : traitement par blocs de 8 floats
			for (size_t i = 0; i < count; i += 8) {
				__m256 va = _mm256_loadu_ps(&a[i]);
				__m256 vb = _mm256_loadu_ps(&b[i]);
				__m256 vr = _mm256_mul_ps(va, vb);
				_mm256_storeu_ps(&result[i], vr);
			}
			return;
		});

		NKENTSEU_NEON_ONLY({
			// Chemin optimisé NEON : traitement par blocs de 4 floats
			for (size_t i = 0; i < count; i += 4) {
				float32x4_t va = vld1q_f32(&a[i]);
				float32x4_t vb = vld1q_f32(&b[i]);
				float32x4_t vr = vmulq_f32(va, vb);
				vst1q_f32(&result[i], vr);
			}
			return;
		});

		// Fallback portable pour architectures sans SIMD
		for (size_t i = 0; i < count; ++i) {
			result[i] = a[i] * b[i];
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Allocation mémoire alignée selon l'architecture
// -----------------------------------------------------------------------------
/*
	#include "NkArchDetect.h"
	#include <cstdlib>

	// Alloue un buffer aligné sur la ligne de cache pour éviter le false sharing
	void* AllocateCacheAligned(size_t size) {
		#if defined(_MSC_VER)
			return _aligned_malloc(size, NKENTSEU_CACHE_LINE_SIZE);
		#elif defined(__GNUC__) || defined(__clang__)
			void* ptr = nullptr;
			posix_memalign(&ptr, NKENTSEU_CACHE_LINE_SIZE, size);
			return ptr;
		#else
			// Fallback non-aligné (moins performant mais portable)
			return malloc(size);
		#endif
	}

	void FreeCacheAligned(void* ptr) {
		#if defined(_MSC_VER)
			_aligned_free(ptr);
		#else
			free(ptr);
		#endif
	}

	// Utilisation :
	// float* data = static_cast<float*>(AllocateCacheAligned(1024 * sizeof(float)));
	// ... utilisation ...
	// FreeCacheAligned(data);
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Code conditionnel par famille d'architecture
// -----------------------------------------------------------------------------
/*
	#include "NkArchDetect.h"

	void InitializePlatformOptimizations() {
		NKENTSEU_INTEL_ONLY({
			// Optimisations spécifiques x86/x86_64
			EnableFastStringOps();
			ConfigurePrefetchHints();
		});

		NKENTSEU_ARM_FAMILY_ONLY({
			// Optimisations spécifiques ARM
			ConfigureNEONRegisters();
			EnableHardwareCrypto();
		});

		NKENTSEU_64BIT_ONLY({
			// Code utilisant des pointeurs 64-bit ou registres larges
			UseWideRegisters();
		});

		NKENTSEU_LITTLE_ENDIAN_ONLY({
			// Optimisations dépendant de l'ordre des octets
			UseNativeByteOrder();
		});
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Structure alignée pour performance cache/SIMD
// -----------------------------------------------------------------------------
/*
	#include "NkArchDetect.h"

	// Structure de particule alignée sur 32 octets pour AVX/NEON
	struct NKENTSEU_ALIGN_32 Particle {
		float position[3];    // 12 octets
		float velocity[3];    // 12 octets
		float mass;           // 4 octets
		uint32_t flags;       // 4 octets
		// Total : 32 octets exactement = alignement parfait
	};

	// Tableau de particules aligné sur ligne de cache
	NKENTSEU_ALIGN_CACHE static Particle g_ParticleBuffer[4096];

	// Fonction de mise à jour utilisant l'alignement pour SIMD
	NKENTSEU_FORCE_INLINE void UpdateParticles(Particle* particles, size_t count) {
		NKENTSEU_AVX_ONLY({
			// Traitement AVX : 8 particules par itération grâce à l'alignement
			for (size_t i = 0; i < count; i += 8) {
				// ... code SIMD optimisé ...
			}
		});
	}
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Détection de fonctionnalités CPU à la compilation
// -----------------------------------------------------------------------------
/*
	#include "NkArchDetect.h"

	// Sélection de l'implémentation de chiffrement selon les extensions disponibles
	void EncryptBlock(uint8_t* output, const uint8_t* input, const uint8_t* key) {
		NKENTSEU_AES_ONLY({
			// Utiliser les instructions AES-NI matérielles (très rapide)
			EncryptWithAESNI(output, input, key);
		});

		NKENTSEU_NOT_AES({
			// Fallback logiciel (plus lent mais portable)
			EncryptWithSoftwareAES(output, input, key);
		});
	}

	// Note : Pour une détection à l'exécution (CPUID, etc.), utiliser
	// une bibliothèque comme cpu_features ou implémenter votre propre détecteur.
	// Les macros de ce fichier ne détectent que ce qui est disponible À LA COMPILATION.
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec NkPlatformDetect.h pour code multi-plateforme
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"  // Détection OS
	#include "NkArchDetect.h"      // Détection architecture

	void CrossPlatformInit() {
		// Combinaison OS + Architecture pour un ciblage précis
		NKENTSEU_WINDOWS_ONLY({
			NKENTSEU_X86_64_ONLY({
				// Windows sur x86_64 : utiliser les APIs spécifiques
				InitializeWindowsX64();
			});
		});

		NKENTSEU_PLATFORM_LINUX({
			NKENTSEU_ARM64_ONLY({
				// Linux sur ARM64 : optimisations serveur mobile
				InitializeLinuxARM64();
			});
		});

		NKENTSEU_PLATFORM_CONSOLE({
			NKENTSEU_CPU_HAS_NEON({
				// Console avec support NEON : activer les optimisations SIMD
				EnableConsoleNEONOptimizations();
			});
		});
	}
*/
```

## NkCGXDetect.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKCGXDETECT.H
// =============================================================================
// Ce fichier fournit la détection compile-time des capacités graphiques et compute.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sélection conditionnelle du backend graphique
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCGXDetect.h"
	#include "NKCore/NkFoundationLog.h"

	class GraphicsManager {
	public:
		static GraphicsManager& Instance() {
			static GraphicsManager instance;
			return instance;
		}

		bool Initialize() {
			// Logging de la configuration détectée
			NK_FOUNDATION_LOG_INFO(
				"Graphics config: API=%d, Version=%d.%d",
				NKENTSEU_GFX_ACTIVE,
				(NKENTSEU_GFX_VERSION >> 16) & 0xFFFF,
				NKENTSEU_GFX_VERSION & 0xFFFF
			);

			// Dispatch vers l'initialisation spécifique
			#if NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_VULKAN
				return InitVulkan();
			#elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_METAL
				return InitMetal();
			#elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_DIRECTX
				return InitDirectX();
			#elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_OPENGL
				return InitOpenGL();
			#else
				NK_FOUNDATION_LOG_WARNING("Falling back to software renderer");
				return InitSoftware();
			#endif
		}

	private:
		// Déclarations conditionnelles selon disponibilité
		NKENTSEU_VULKAN_ONLY(
			bool InitVulkan();
		)
		NKENTSEU_METAL_ONLY(
			bool InitMetal();
		)
		NKENTSEU_D3D12_ONLY(
			bool InitDirectX();
		)
		NKENTSEU_OPENGL_ONLY(
			bool InitOpenGL();
		)
		bool InitSoftware();  // Toujours disponible
	};
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des énumérations typées
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCGXDetect.h"
	#include <string>

	// Conversion runtime d'une enum vers string (complément aux templates constexpr)
	std::string GraphicsApiToString(nkentseu::platform::graphics::NkGraphicsApi api) {
		using namespace nkentseu::platform::graphics;

		switch (api) {
			case NkGraphicsApi::NK_GFX_API_VULKAN:  return "Vulkan";
			case NkGraphicsApi::NK_GFX_API_METAL:   return "Metal";
			case NkGraphicsApi::NK_GFX_API_DX12:   return "Direct3D 12";
			case NkGraphicsApi::NK_GFX_API_DX11:   return "Direct3D 11";
			case NkGraphicsApi::NK_GFX_API_OPENGL:  return "OpenGL";
			case NkGraphicsApi::NK_GFX_API_OPENGLES:return "OpenGL ES";
			case NkGraphicsApi::NK_GFX_API_WEBGL2:  return "WebGL 2";
			case NkGraphicsApi::NK_GFX_API_SOFTWARE:return "Software";
			default: return "Unknown";
		}
	}

	// Vérification de compatibilité GPU
	bool IsGPUSupported(
		nkentseu::platform::graphics::NkGPUVendor vendor,
		nkentseu::platform::graphics::NkGPUType type
	) {
		using namespace nkentseu::platform::graphics;

		// Refuser les GPU software pour les applications exigeantes
		if (type == NkGPUType::NK_SOFTWARE) {
			return false;
		}

		// Workaround pour anciens drivers Intel sur Windows
		#if defined(NKENTSEU_PLATFORM_WINDOWS)
			if (vendor == NkGPUVendor::NK_INTEL && type == NkGPUType::NK_INTEGRATED) {
				return CheckIntelDriverVersion();  // Fonction personnalisée
			}
		#endif

		return true;  // Par défaut, accepter
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Configuration compute pour accélération parallèle
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCGXDetect.h"

	// Wrapper unifié pour kernels de calcul
	class ComputeKernel {
	public:
		virtual ~ComputeKernel() = default;
		virtual void Execute(void* inputData, void* outputData, size_t elementCount) = 0;
	};

	// Factory conditionnelle selon disponibilité
	ComputeKernel* CreateImageFilterKernel() {
		#if defined(NKENTSEU_COMPUTE_CUDA_AVAILABLE)
			return new CudaImageFilterKernel();  // Implémentation CUDA
		#elif defined(NKENTSEU_COMPUTE_OPENCL_AVAILABLE)
			return new OpenCLImageFilterKernel();  // Implémentation OpenCL
		#elif defined(NKENTSEU_COMPUTE_SYCL_AVAILABLE)
			return new SYCLImageFilterKernel();  // Implémentation SYCL
		#else
			// Fallback CPU multithreaded via NKCore threading
			return new CPUImageFilterKernel();
		#endif
	}

	// Exécution avec fallback automatique
	void ProcessImage(void* pixels, size_t width, size_t height) {
		auto kernel = CreateImageFilterKernel();
		if (kernel) {
			kernel->Execute(pixels, pixels, width * height);
			delete kernel;
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Détection des capacités à l'exécution
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCGXDetect.h"
	#include "NKPlatform/NkPlatformQuery.h"  // Pour QueryGPUInfo()

	struct GraphicsCapabilities {
		nkentseu::platform::graphics::NkGraphicsApi api;
		nkentseu::platform::graphics::NkGPUVendor vendor;
		nkentseu::platform::graphics::NkGPUType type;
		uint32_t apiVersion;
		size_t videoMemoryMB;
		bool supportsRayTracing;
		bool supportsVariableRateShading;
	};

	GraphicsCapabilities DetectGraphicsCaps() {
		using namespace nkentseu::platform::graphics;

		GraphicsCapabilities caps{};

		// API compile-time (détection header)
		#if NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_VULKAN
			caps.api = NkGraphicsApi::NK_GFX_API_VULKAN;
			caps.apiVersion = NKENTSEU_GFX_VERSION;
		#elif NKENTSEU_GFX_ACTIVE == NKENTSEU_GFX_METAL
			caps.api = NkGraphicsApi::NK_GFX_API_METAL;
			caps.apiVersion = NKENTSEU_GFX_VERSION;
		// ... autres APIs
		#else
			caps.api = NkGraphicsApi::NK_GFX_API_SOFTWARE;
			caps.apiVersion = NKENTSEU_GFX_VERSION;
		#endif

		// Query hardware info via NKPlatform (implémentation spécifique)
		auto gpuInfo = nkentseu::platform::QueryGPUInfo();
		caps.vendor = static_cast<NkGPUVendor>(gpuInfo.vendorId);
		caps.type = static_cast<NkGPUType>(gpuInfo.deviceType);
		caps.videoMemoryMB = gpuInfo.dedicatedVideoMemoryMB;

		// Détection des features avancées (runtime)
		#if defined(NKENTSEU_GRAPHICS_VULKAN_AVAILABLE)
			if (caps.api == NkGraphicsApi::NK_GFX_API_VULKAN) {
				caps.supportsRayTracing = CheckVulkanRayTracingSupport();
				caps.supportsVariableRateShading = CheckVulkanVRSSupport();
			}
		#endif

		return caps;
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Macros conditionnelles pour code cross-platform
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCGXDetect.h"

	// Déclaration de fonction avec implémentations conditionnelles
	void CreateSwapchain(void* windowHandle) {
		NKENTSEU_VULKAN_ONLY(
			// Vulkan : vkCreateSwapchainKHR
			VkSwapchainCreateInfoKHR createInfo{};
			createInfo.surface = GetVulkanSurface(windowHandle);
			// ... configuration ...
			vkCreateSwapchainKHR(vkDevice, &createInfo, nullptr, &swapchain);
		)

		NKENTSEU_METAL_ONLY(
			// Metal : CAMetalLayer
			id<CAMetalLayer> metalLayer = (__bridge id<CAMetalLayer>)windowHandle;
			metalLayer.device = GetMetalDevice();
			metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		)

		NKENTSEU_D3D12_ONLY(
			// Direct3D 12 : IDXGISwapChain3
			DXGI_SWAP_CHAIN_DESC1 swapDesc{};
			swapDesc.BufferCount = 3;
			swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			// ... configuration ...
			dxgiFactory->CreateSwapChainForHwnd(
				d3dCommandQueue, windowHandle, &swapDesc, nullptr, nullptr, &swapChain
			);
		)

		NKENTSEU_OPENGL_ONLY(
			// OpenGL : contexte géré par la fenêtre (GLFW/SDL/etc.)
			// Pas de création explicite de swapchain
		)
	}

	// Fonction utilitaire avec fallback
	void SetVSyncEnabled(bool enabled) {
		NKENTSEU_VULKAN_ONLY(
			SetVulkanVSync(enabled);  // Via VK_KHR_present_wait ou mailbox mode
		)
		NKENTSEU_METAL_ONLY(
			GetMetalDrawableQueue()->setVSyncEnabled(enabled);
		)
		NKENTSEU_D3D12_ONLY(
			dxgiSwapChain->SetSyncInterval(enabled ? 1 : 0);
		)
		NKENTSEU_OPENGL_ONLY(
			#if defined(NKENTSEU_PLATFORM_WINDOWS)
				wglSwapIntervalEXT(enabled ? 1 : 0);
			#elif defined(NKENTSEU_PLATFORM_LINUX)
				glXSwapIntervalEXT(display, drawable, enabled ? 1 : 0);
			#endif
		)
		NKENTSEU_NOT_VULKAN(
		NKENTSEU_NOT_METAL(
		NKENTSEU_NOT_D3D12(
		NKENTSEU_NOT_OPENGL(
			// Fallback software : pas de VSync supporté
			NK_UNUSED(enabled);
		))))
	}
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Configuration via variables d'environnement
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCGXDetect.h"
	#include "NKPlatform/NkEnvironment.h"  // Pour GetEnvVar()

	// Sélection de l'API via variable d'environnement NK_GFX_BACKEND
	nkentseu::platform::graphics::NkGraphicsApi SelectGraphicsAPI() {
		using namespace nkentseu::platform::graphics;

		const char* envBackend = nkentseu::platform::GetEnvVar("NK_GFX_BACKEND");

		if (envBackend) {
			// Override manuel par l'utilisateur
			#if defined(NKENTSEU_GRAPHICS_VULKAN_AVAILABLE)
				if (strcmp(envBackend, "vulkan") == 0) return NkGraphicsApi::NK_GFX_API_VULKAN;
			#endif
			#if defined(NKENTSEU_GRAPHICS_METAL_AVAILABLE)
				if (strcmp(envBackend, "metal") == 0) return NkGraphicsApi::NK_GFX_API_METAL;
			#endif
			#if defined(NKENTSEU_GRAPHICS_D3D12_AVAILABLE)
				if (strcmp(envBackend, "d3d12") == 0) return NkGraphicsApi::NK_GFX_API_DX12;
			#endif
			#if defined(NKENTSEU_GRAPHICS_OPENGL_AVAILABLE)
				if (strcmp(envBackend, "opengl") == 0) return NkGraphicsApi::NK_GFX_API_OPENGL;
			#endif

			NK_LOG_WARNING("Unknown graphics backend '%s' in NK_GFX_BACKEND", envBackend);
		}

		// Fallback vers l'API par défaut détectée
		return NKENTSEU_GRAPHICS_DEFAULT;
	}
*/

```

## NkCompilerDetect.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKCOMPILERDETECT.H
// =============================================================================
// Ce fichier fournit des macros pour détecter le compilateur et les fonctionnalités
// C++ disponibles, permettant d'écrire du code portable et optimisé.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Code conditionnel par compilateur
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCompilerDetect.h"

	void PlatformSpecificCode() {
		#ifdef NKENTSEU_COMPILER_MSVC
			// Code spécifique à MSVC
			__assume(ptr != nullptr);  // Hint d'optimisation MSVC

		#elif defined(NKENTSEU_COMPILER_GCC)
			// Code spécifique à GCC
			if (__builtin_expect(ptr != nullptr, 1)) {
				ProcessData(ptr);
			}

		#elif defined(NKENTSEU_COMPILER_CLANG)
			// Code spécifique à Clang
			#if NKENTSEU_COMPILER_VERSION >= 140000  // Clang 14+
				UseNewClangAPI();
			#endif
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des macros de convenance C++
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCompilerDetect.h"

	class ResourceManager {
	public:
		// Constructeur avec noexcept portable
		ResourceManager() NKENTSEU_NOEXCEPT {
			// Initialisation sans risque d'exception
		}

		// Méthode virtuelle avec override portable
		virtual void Load() NKENTSEU_OVERRIDE {
			// Implémentation
		}

		// Méthode finale empêchant la redéfinition
		virtual void Finalize() NKENTSEU_FINAL {
			// Dernière étape, non redéfinissable
		}

		// Fonction dont le retour ne doit pas être ignoré
		NKENTSEU_NODISCARD_MSG("Vérifier le code de retour pour les erreurs")
		int Initialize() {
			return 0;  // 0 = succès, autre = code d'erreur
		}

		// Paramètre potentiellement inutilisé sans warning
		void Callback(int NKENTSEU_MAYBE_UNUSED userData) {
			// userData peut être ignoré dans certains cas
		}
	};
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Gestion portable des warnings
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCompilerDetect.h"

	// Fonction avec paramètre unused intentionnel
	NKENTSEU_DISABLE_WARNING_PUSH

	#ifdef NKENTSEU_COMPILER_MSVC
		NKENTSEU_DISABLE_WARNING(4100)  // unreferenced formal parameter
	#elif defined(NKENTSEU_COMPILER_GCC) || defined(NKENTSEU_COMPILER_CLANG)
		NKENTSEU_DISABLE_WARNING("-Wunused-parameter")
	#endif

	void CallbackHandler(int eventId, void* NKENTSEU_RESTRICT context) {
		// Seul eventId est utilisé, context est requis par l'API mais ignoré
		ProcessEvent(eventId);
	}

	NKENTSEU_DISABLE_WARNING_POP
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Structures binaires avec packing portable
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCompilerDetect.h"
	#include <cstdint>

	NKENTSEU_PACK_BEGIN
	struct NKENTSEU_PACKED NetworkPacket {
		uint16_t magic;      // 2 octets
		uint16_t type;       // 2 octets
		uint32_t length;     // 4 octets
		uint8_t  flags;      // 1 octet
		// Total : 9 octets, pas de padding
	};
	NKENTSEU_PACK_END

	// Vérification à la compilation
	static_assert(sizeof(NetworkPacket) == 9, "NetworkPacket must be tightly packed");

	void SendPacket(const NetworkPacket* packet) {
		// Envoi binaire direct sans sérialisation
		WriteToSocket(packet, sizeof(NetworkPacket));
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Détection de fonctionnalités à la compilation
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCompilerDetect.h"

	template<typename T>
	class OptimizedContainer {
	public:
		#if defined(NKENTSEU_HAS_CONCEPTS)
			// Version C++20 avec concepts pour une meilleure erreur de compilation
			template<typename U>
			requires std::copyable<U> && std::destructible<U>
			void Add(U&& item) {
				// Implémentation optimisée avec concepts
			}
		#else
			// Fallback pour C++11/14/17 sans concepts
			template<typename U>
			void Add(U&& item) {
				// Implémentation générique avec SFINAE si nécessaire
			}
		#endif

		// Méthode constexpr si supporté
		NKENTSEU_CONSTEXPR size_t Capacity() const {
			return m_capacity;
		}

	private:
		size_t m_capacity = 0;
	};
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec NkPlatformDetect.h et NkArchDetect.h
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformDetect.h"   // Détection OS
	#include "NKPlatform/NkArchDetect.h"       // Détection architecture
	#include "NKPlatform/NkCompilerDetect.h"   // Détection compilateur

	void CrossPlatformOptimization() {
		// Combinaison OS + Architecture + Compilateur pour un ciblage précis

		NKENTSEU_PLATFORM_WINDOWS({
			NKENTSEU_ARCH_X86_64({
				NKENTSEU_COMPILER_MSVC({
					// Windows x64 avec MSVC : utiliser les intrinsics MSVC
					UseMSVCIntrinsics();
				});

				NKENTSEU_COMPILER_CLANG({
					// Windows x64 avec Clang : utiliser les intrinsics compatibles
					UseClangIntrinsics();
				});
			});
		});

		NKENTSEU_PLATFORM_LINUX({
			NKENTSEU_ARCH_ARM64({
				NKENTSEU_COMPILER_GCC({
					// Linux ARM64 avec GCC : optimisations NEON + GCC
					EnableNEONGCCOptimizations();
				});
			});
		});

		// Code utilisant les fonctionnalités C++ modernes si disponibles
		#if defined(NKENTSEU_HAS_IF_CONSTEXPR)
			if constexpr (std::is_pointer_v<T>) {
				HandlePointerCase();
			} else {
				HandleValueCase();
			}
		#else
			// Fallback sans if constexpr
			if (std::is_pointer<T>::value) {
				HandlePointerCase();
			} else {
				HandleValueCase();
			}
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Macro utilitaire pour le logging portable
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCompilerDetect.h"
	#include <cstdio>

	// Macro de logging avec informations de compilation
	#define NK_LOG(level, msg, ...) \
		do { \
			fprintf(stderr, "[%s:%d] [%s] " msg "\n", \
				NKENTSEU_FILE_NAME, \
				NKENTSEU_LINE_NUMBER, \
				level, \
				##__VA_ARGS__); \
		} while(0)

	// Utilisation :
	void Initialize() {
		NK_LOG("INFO", "Initialisation en cours...");

		#ifdef NKENTSEU_COMPILER_DEBUG
			NK_LOG("DEBUG", "Compilateur: %d", NKENTSEU_COMPILER_VERSION);
			NK_LOG("DEBUG", "C++ Standard: %d", NKENTSEU_CPP_VERSION);
		#endif

		if (!CheckPrerequisites()) {
			NK_LOG("ERROR", "Prérequis non satisfaits");
			return;
		}

		NK_LOG("INFO", "Initialisation terminée");
	}
*/
```

## NkCPUFeatures.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKCPUFEATURES.H
// =============================================================================
// Ce fichier fournit une API complète pour détecter les fonctionnalités CPU
// à l'exécution et adapter le code en conséquence.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Sélection de chemin de code optimisé par SIMD
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCPUFeatures.h"

	void ProcessData(float* data, size_t count) {
		const auto& cpu = nkentseu::platform::CPUFeatures::Get();

		// Sélection du meilleur chemin disponible
		if (cpu.simd.hasAVX512F) {
			ProcessDataAVX512(data, count);
		} else if (cpu.simd.hasAVX2) {
			ProcessDataAVX2(data, count);
		} else if (cpu.simd.hasSSE42) {
			ProcessDataSSE42(data, count);
		} else {
			ProcessDataScalar(data, count);
		}
	}

	// Alternative avec les macros de convenance Nk* :
	void ProcessDataSimple(float* data, size_t count) {
		if (nkentseu::platform::NkHasAVX2()) {
			ProcessDataAVX2(data, count);
		} else if (nkentseu::platform::NkHasSSE2()) {
			ProcessDataSSE2(data, count);
		} else {
			ProcessDataScalar(data, count);
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Configuration multi-threading basée sur la topologie
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCPUFeatures.h"
	#include <thread>
	#include <vector>

	void LaunchParallelWork() {
		const auto& cpu = nkentseu::platform::CPUFeatures::Get();

		// Déterminer le nombre optimal de threads
		int32_t threadCount = cpu.topology.numPhysicalCores;

		// Ajuster si Hyper-Threading est activé
		if (cpu.topology.hasHyperThreading) {
			// Pour les charges de travail CPU-bound, privilégier les cœurs physiques
			// Pour les charges I/O-bound, on peut utiliser plus de threads
			if (IsIOBoundWorkload()) {
				threadCount = cpu.topology.numLogicalCores;
			}
		}

		// Lancer les threads
		std::vector<std::thread> workers;
		workers.reserve(static_cast<size_t>(threadCount));

		for (int32_t i = 0; i < threadCount; ++i) {
			workers.emplace_back(WorkerFunction, i);
		}

		// Attendre la complétion
		for (auto& t : workers) {
			t.join();
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Optimisation basée sur la taille de cache
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCPUFeatures.h"

	void ProcessLargeArray(const float* input, float* output, size_t elementCount) {
		const auto& cpu = nkentseu::platform::CPUFeatures::Get();

		// Taille de ligne de cache pour l'alignement
		const size_t cacheLine = static_cast<size_t>(cpu.cache.lineSize);

		// Taille de bloc pour le tiling (adapter à L1/L2)
		const size_t blockSize = static_cast<size_t>(cpu.cache.l2Size) * 1024 / 4;

		// Allocation alignée sur ligne de cache
		float* alignedOutput = static_cast<float*>(
			aligned_alloc(cacheLine, elementCount * sizeof(float))
		);

		// Traitement par blocs pour optimiser l'utilisation du cache
		for (size_t blockStart = 0; blockStart < elementCount; blockStart += blockSize) {
			size_t blockEnd = (blockStart + blockSize < elementCount)
				? blockStart + blockSize
				: elementCount;

			ProcessBlock(input + blockStart, alignedOutput + blockStart, blockEnd - blockStart);
		}

		// Copie finale si nécessaire
		memcpy(output, alignedOutput, elementCount * sizeof(float));
		free(alignedOutput);
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Logging des informations CPU au démarrage
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCPUFeatures.h"
	#include "NKPlatform/NkFoundationLog.h"

	void LogSystemInfo() {
		const auto& cpu = nkentseu::platform::CPUFeatures::Get();

		NK_FOUNDATION_LOG_INFO("=== CPU Information ===");
		NK_FOUNDATION_LOG_INFO("Vendor: %s", cpu.vendor);
		NK_FOUNDATION_LOG_INFO("Brand: %s", cpu.brand);
		NK_FOUNDATION_LOG_INFO("Family/Model/Stepping: %d/%d/%d",
			cpu.family, cpu.model, cpu.stepping);
		NK_FOUNDATION_LOG_INFO("Frequency: %d MHz (base) / %d MHz (max)",
			cpu.baseFrequency, cpu.maxFrequency);

		NK_FOUNDATION_LOG_INFO("\n=== Topology ===");
		NK_FOUNDATION_LOG_INFO("Physical cores: %d", cpu.topology.numPhysicalCores);
		NK_FOUNDATION_LOG_INFO("Logical cores: %d", cpu.topology.numLogicalCores);
		NK_FOUNDATION_LOG_INFO("Hyper-Threading: %s",
			cpu.topology.hasHyperThreading ? "Yes" : "No");

		NK_FOUNDATION_LOG_INFO("\n=== Cache ===");
		NK_FOUNDATION_LOG_INFO("Line size: %d bytes", cpu.cache.lineSize);
		NK_FOUNDATION_LOG_INFO("L1 Data: %d KB, L1 Inst: %d KB",
			cpu.cache.l1DataSize, cpu.cache.l1InstructionSize);
		NK_FOUNDATION_LOG_INFO("L2: %d KB, L3: %d KB",
			cpu.cache.l2Size, cpu.cache.l3Size);

		NK_FOUNDATION_LOG_INFO("\n=== SIMD Features ===");
		if (cpu.simd.hasSSE2) NK_FOUNDATION_PRINT("  SSE2");
		if (cpu.simd.hasSSE42) NK_FOUNDATION_PRINT("  SSE4.2");
		if (cpu.simd.hasAVX) NK_FOUNDATION_PRINT("  AVX");
		if (cpu.simd.hasAVX2) NK_FOUNDATION_PRINT("  AVX2");
		if (cpu.simd.hasAVX512F) NK_FOUNDATION_PRINT("  AVX-512");
		if (cpu.simd.hasNEON) NK_FOUNDATION_PRINT("  NEON");
		if (cpu.simd.hasSVE) NK_FOUNDATION_PRINT("  SVE");

		NK_FOUNDATION_LOG_INFO("\n=== Extended Features ===");
		if (cpu.extended.hasAES) NK_FOUNDATION_PRINT("  AES-NI");
		if (cpu.extended.hasSHA) NK_FOUNDATION_PRINT("  SHA");
		if (cpu.extended.hasRDRAND) NK_FOUNDATION_PRINT("  RDRAND");
		if (cpu.extended.hasBMI2) NK_FOUNDATION_PRINT("  BMI2");
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Dispatch runtime avec fallback portable
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCPUFeatures.h"

	// Déclaration des implémentations spécialisées
	void Compute_Scalar(const float* a, const float* b, float* out, size_t n);
	void Compute_SSE2(const float* a, const float* b, float* out, size_t n);
	void Compute_AVX2(const float* a, const float* b, float* out, size_t n);
	void Compute_AVX512(const float* a, const float* b, float* out, size_t n);

	// Pointeur de fonction pour le dispatch
	using ComputeFunc = void(*)(const float*, const float*, float*, size_t);

	// Initialisation du dispatch au démarrage
	ComputeFunc g_ComputeImpl = nullptr;

	void InitializeComputeDispatch() {
		const auto& cpu = nkentseu::platform::CPUFeatures::Get();

		// Sélection de la meilleure implémentation disponible
		if (cpu.simd.hasAVX512F) {
			g_ComputeImpl = Compute_AVX512;
		} else if (cpu.simd.hasAVX2) {
			g_ComputeImpl = Compute_AVX2;
		} else if (cpu.simd.hasSSE2) {
			g_ComputeImpl = Compute_SSE2;
		} else {
			g_ComputeImpl = Compute_Scalar;
		}
	}

	// Fonction publique : appelle l'implémentation optimale
	void Compute(const float* a, const float* b, float* out, size_t n) {
		// g_ComputeImpl est initialisé au démarrage, donc safe à appeler
		g_ComputeImpl(a, b, out, n);
	}
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Vérification de fonctionnalités pour activation conditionnelle
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkCPUFeatures.h"

	class CryptoEngine {
	public:
		CryptoEngine() {
			const auto& cpu = nkentseu::platform::CPUFeatures::Get();

			// Activer les accélérations matérielles si disponibles
			if (cpu.extended.hasAES) {
				m_useAESNI = true;
				NK_FOUNDATION_LOG_DEBUG("AES-NI enabled");
			} else {
				m_useAESNI = false;
				NK_FOUNDATION_LOG_WARN("AES-NI not available, using software fallback");
			}

			if (cpu.extended.hasSHA) {
				m_useSHAExtensions = true;
			}

			// RDRAND pour la génération de clés
			if (cpu.extended.hasRDRAND) {
				m_useHardwareRNG = true;
			}
		}

		void EncryptBlock(uint8_t* output, const uint8_t* input, const uint8_t* key) {
			if (m_useAESNI) {
				EncryptWithAESNI(output, input, key);
			} else {
				EncryptWithSoftwareAES(output, input, key);
			}
		}

	private:
		bool m_useAESNI = false;
		bool m_useSHAExtensions = false;
		bool m_useHardwareRNG = false;
	};
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
	#include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
	#include "NKPlatform/NkCPUFeatures.h"      // Détection CPU runtime

	void PlatformOptimizedInit() {
		const auto& cpu = nkentseu::platform::CPUFeatures::Get();

		// Combinaison plateforme + architecture + fonctionnalités CPU
		#if defined(NKENTSEU_ARCH_X86_64)
			if (cpu.simd.hasAVX2) {
				InitializeAVX2Optimizations();
			}
		#elif defined(NKENTSEU_ARCH_ARM64)
			if (cpu.simd.hasNEON) {
				InitializeNEONOptimizations();
			}
			if (cpu.simd.hasSVE) {
				InitializeSVEOptimizations();
			}
		#endif

		// Ajustement basé sur la topologie
		SetThreadAffinity(cpu.topology.numPhysicalCores);

		// Configuration du cache-aware allocator
		ConfigureCacheAwareAllocator(
			static_cast<size_t>(cpu.cache.lineSize),
			static_cast<size_t>(cpu.cache.l3Size) * 1024
		);
	}
*/
```

## NkEndianness.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKENDIANNESS.H
// =============================================================================
// Ce fichier fournit des utilitaires pour la détection d'endianness et la conversion
// d'ordre des octets, essentiels pour le parsing de formats binaires et la communication réseau.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Détection d'endianness et logging
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEndianness.h"
	#include "NKPlatform/NkFoundationLog.h"

	void LogEndiannessInfo() {
		constexpr auto compileEndianness = nkentseu::platform::NkGetCompileTimeEndianness();
		const auto runtimeEndianness = nkentseu::platform::NkGetRuntimeEndianness();

		NK_FOUNDATION_LOG_INFO("Compile-time endianness: %d", static_cast<int>(compileEndianness));
		NK_FOUNDATION_LOG_INFO("Runtime endianness: %d", static_cast<int>(runtimeEndianness));

		if (nkentseu::platform::NkIsLittleEndian()) {
			NK_FOUNDATION_LOG_INFO("System is little-endian (x86/ARM typical)");
		} else if (nkentseu::platform::NkIsBigEndian()) {
			NK_FOUNDATION_LOG_INFO("System is big-endian (network order, some embedded)");
		} else {
			NK_FOUNDATION_LOG_WARN("Endianness unknown - using fallback conversions");
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Conversion réseau (big-endian) pour protocole TCP/IP
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEndianness.h"
	#include <cstdint>

	struct NetworkHeader {
		uint16_t magic;      // 0xABCD en big-endian
		uint16_t length;     // Longueur du payload en big-endian
		uint32_t timestamp;  // Timestamp en big-endian
	};

	void SendPacket(int socket, const void* payload, size_t payloadSize) {
		NetworkHeader header;
		header.magic = nkentseu::platform::HostToNetwork16(0xABCD);
		header.length = nkentseu::platform::HostToNetwork16(static_cast<uint16_t>(payloadSize));
		header.timestamp = nkentseu::platform::HostToNetwork32(GetCurrentTimestamp());

		// Envoi : header + payload
		send(socket, &header, sizeof(header), 0);
		send(socket, payload, payloadSize, 0);
	}

	NetworkHeader ReceivePacket(int socket) {
		NetworkHeader header;
		recv(socket, &header, sizeof(header), MSG_WAITALL);

		// Conversion retour vers host order
		header.magic = nkentseu::platform::NetworkToHost16(header.magic);
		header.length = nkentseu::platform::NetworkToHost16(header.length);
		header.timestamp = nkentseu::platform::NetworkToHost32(header.timestamp);

		return header;
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Parsing de fichier binaire avec valeurs little-endian
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEndianness.h"
	#include <fstream>

	struct FileHeader {
		uint32_t magic;      // "NKFF" = 0x46464B4E en little-endian
		uint16_t version;
		uint32_t dataSize;
	};

	bool LoadCustomFile(const char* filename) {
		std::ifstream file(filename, std::ios::binary);
		if (!file) {
			return false;
		}

		// Lecture du header depuis une adresse potentiellement non-alignée
		FileHeader header;
		file.read(reinterpret_cast<char*>(&header), sizeof(header));

		// Conversion depuis little-endian (format du fichier)
		const uint32_t magic = nkentseu::platform::ReadLE32(&header.magic);
		const uint16_t version = nkentseu::platform::ReadLE16(&header.version);
		const uint32_t dataSize = nkentseu::platform::ReadLE32(&header.dataSize);

		if (magic != 0x46464B4E) {  // "NKFF"
			return false;  // Mauvais format de fichier
		}

		// Lecture du payload...
		return true;
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Conversion générique via templates
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEndianness.h"

	template <typename T>
	void SerializeValue(std::vector<uint8_t>& buffer, T value, bool littleEndian) {
		if (littleEndian) {
			value = nkentseu::platform::ToLittleEndian(value);
		} else {
			value = nkentseu::platform::ToBigEndian(value);
		}

		const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
		buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
	}

	template <typename T>
	T DeserializeValue(const uint8_t* data, bool littleEndian) {
		T value;
		::memcpy(&value, data, sizeof(T));

		if (littleEndian) {
			return nkentseu::platform::FromLittleEndian(value);
		} else {
			return nkentseu::platform::FromBigEndian(value);
		}
	}

	// Utilisation :
	// std::vector<uint8_t> buffer;
	// SerializeValue(buffer, uint32_t(0x12345678), true);  // little-endian
	// auto val = DeserializeValue<uint32_t>(buffer.data(), true);
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Conversion de buffer entier pour sérialisation
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEndianness.h"
	#include <vector>

	void PrepareNetworkBuffer(std::vector<uint32_t>& values) {
		// Convertir tout le buffer vers big-endian (ordre réseau)
		nkentseu::platform::BufferToBigEndian(values.data(), values.size());
	}

	void ProcessReceivedBuffer(std::vector<uint32_t>& values) {
		// Convertir depuis big-endian vers host order après réception
		nkentseu::platform::BufferFromBigEndian(values.data(), values.size());
	}

	// Pour les types flottants (via union dans ByteSwap) :
	void SerializeFloatArray(std::vector<uint8_t>& out, const float* values, size_t count) {
		out.reserve(count * sizeof(float));

		for (size_t i = 0; i < count; ++i) {
			// ByteSwap supporte float via union interne (taille 4 octets)
			const uint32_t swapped = nkentseu::platform::ByteSwap(*reinterpret_cast<const uint32_t*>(&values[i]));
			const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&swapped);
			out.insert(out.end(), bytes, bytes + sizeof(float));
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Accès non-aligné pour parsing de formats binaires
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEndianness.h"

	// Parser un en-tête de format binaire avec champs non-alignés
	struct PackedHeader {
		uint8_t type;      // offset 0
		uint32_t id;       // offset 1 (non-aligné sur 4!)
		uint16_t flags;    // offset 5 (non-aligné sur 2)
	} NKENTSEU_PACKED;     // Pas de padding entre les membres

	bool ParsePackedHeader(const uint8_t* data, PackedHeader& out) {
		// Lecture safe depuis adresses non-alignées
		out.type = data[0];
		out.id = nkentseu::platform::ReadLE32(data + 1);      // little-endian, non-aligné
		out.flags = nkentseu::platform::ReadLE16(data + 5);   // little-endian, non-aligné

		return true;
	}

	// Alternative : copier dans une structure alignée puis convertir
	bool ParsePackedHeaderSafe(const uint8_t* data, PackedHeader& out) {
		// Copie byte-à-byte pour éviter l'aliasing
		::memcpy(&out, data, sizeof(PackedHeader));

		// Conversion des champs multi-octets
		out.id = nkentseu::platform::FromLittleEndian(out.id);
		out.flags = nkentseu::platform::FromLittleEndian(out.flags);

		return true;
	}
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
	#include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
	#include "NKPlatform/NkEndianness.h"       // Conversions d'endianness

	void PlatformAwareSerialization(uint8_t* buffer, size_t offset, uint32_t value) {
		// Adaptation selon la plateforme cible du format binaire

		#if defined(NKENTSEU_PLATFORM_WINDOWS) || defined(NKENTSEU_PLATFORM_LINUX)
			// Ces plateformes sont typiquement little-endian
			// Si le format cible est big-endian (réseau), convertir
			nkentseu::platform::WriteBE32(buffer + offset, value);
		#elif defined(NKENTSEU_PLATFORM_UNKNOWN)
			// Plateforme inconnue : détection runtime + conversion conditionnelle
			if (nkentseu::platform::NkIsLittleEndian()) {
				nkentseu::platform::WriteBE32(buffer + offset, value);
			} else {
				nkentseu::platform::WriteUnaligned32(buffer + offset, value);
			}
		#else
			// Fallback générique : toujours convertir vers big-endian
			nkentseu::platform::WriteBE32(buffer + offset, value);
		#endif
	}

	// Optimisation compile-time : si l'endianness est connue, le compiler
	// éliminera la branche inutile grâce aux constexpr et NK_LITTLE_ENDIAN
*/
```

## NkEnv.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKENV.H
// =============================================================================
// Ce fichier fournit une API portable pour manipuler les variables d'environnement
// sans dépendance à la STL, adaptée aux environnements embarqués et systèmes critiques.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Lecture et écriture basique d'une variable d'environnement
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEnv.h"
	#include <cstdio>

	int main() {
		using namespace nkentseu::env;

		// Définir une variable temporaire
		NkEnvString varName("MY_APP_VAR");
		NkEnvString varValue("Hello from NkEnv");
		NkSet(varName, varValue);

		// Lire la variable
		NkEnvString result;
		NkGet(varName, result);
		std::printf("MY_APP_VAR = %s\n", result.CStr());

		// Vérifier l'existence
		if (NkExists(varName)) {
			std::printf("Variable exists!\n");
		}

		// Supprimer la variable
		NkUnset(varName);
		return 0;
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Modification du PATH pour ajouter des répertoires d'exécution
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEnv.h"

	void SetupApplicationPath() {
		using namespace nkentseu::env;

		// Ajouter un répertoire personnalisé au début du PATH (priorité haute)
		NkPrependToPath(NkEnvString("/opt/myapp/bin"));

		// Ajouter un autre répertoire à la fin (priorité basse)
		NkAppendToPath(NkEnvString("/usr/local/myapp/tools"));

		// Vérifier le résultat
		NkEnvString pathValue;
		NkGet(NkEnvString("PATH"), pathValue);
		// pathValue contient maintenant le PATH mis à jour
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Lister toutes les variables d'environnement (debug)
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEnv.h"
	#include <cstdio>

	void DumpEnvironment() {
		using namespace nkentseu::env;

		NkEnvUMap<NkEnvString, NkEnvString> allVars = NkGetAll();

		std::printf("=== Environment Variables (%u total) ===\n", allVars.Size());
		for (NkSize i = 0; i < allVars.Size(); ++i) {
			const auto& pair = allVars.Data()[i];
			std::printf("%s = %s\n", pair.key.CStr(), pair.value.CStr());
		}
		std::printf("=======================================\n");
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion conditionnelle basée sur une variable d'environnement
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEnv.h"

	bool InitializeWithConfig() {
		using namespace nkentseu::env;

		NkEnvString configPath;
		NkGet(NkEnvString("MYAPP_CONFIG"), configPath);

		if (configPath.Empty()) {
			// Fallback vers un chemin par défaut
			configPath = NkEnvString("/etc/myapp/config.ini");
		}

		// Charger la configuration depuis configPath.CStr()
		return LoadConfigFile(configPath.CStr());
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation des conteneurs génériques sans STL
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEnv.h"

	void ProcessEnvVariables() {
		using namespace nkentseu::env;

		// Créer un vecteur de chaînes
		NkEnvVector<NkEnvString> searchPaths;
		searchPaths.PushBack(NkEnvString("/usr/bin"));
		searchPaths.PushBack(NkEnvString("/usr/local/bin"));
		searchPaths.PushBack(NkEnvString("~/bin"));

		// Itérer avec les itérateurs style C
		for (const NkEnvString* it = searchPaths.Begin(); it != searchPaths.End(); ++it) {
			std::printf("Searching in: %s\n", it->CStr());
		}

		// Créer une map pour un cache de valeurs
		NkEnvUMap<NkEnvString, NkEnvString> cache;
		cache.Set(NkEnvString("last_result"), NkEnvString("success"));

		const NkEnvString* cached = cache.Find(NkEnvString("last_result"));
		if (cached) {
			std::printf("Cached: %s\n", cached->CStr());
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Concaténation et manipulation de chaînes NkEnvString
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkEnv.h"

	NkEnvString BuildFullPath(const char* base, const char* subpath) {
		using namespace nkentseu::env;

		NkEnvString result(base);
		if (result.CStr()[result.Size() - 1] != '/' &&
			result.CStr()[result.Size() - 1] != '\\') {
			result = result + NkEnvString('/');
		}
		result = result + NkEnvString(subpath);
		return result;
	}

	// Utilisation :
	// NkEnvString fullPath = BuildFullPath("/home/user", "documents/file.txt");
	// => "/home/user/documents/file.txt"
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
	#include "NKPlatform/NkEnv.h"              // Gestion d'environnement

	void PlatformAwareInit() {
		using namespace nkentseu::env;

		#if defined(NKENTSEU_PLATFORM_WINDOWS)
			// Sur Windows, utiliser des chemins avec backslashes
			NkPrependToPath(NkEnvString("C:\\MyApp\\bin"));
		#elif defined(NKENTSEU_PLATFORM_LINUX) || defined(NKENTSEU_PLATFORM_MACOS)
			// Sur Unix-like, utiliser des chemins POSIX
			NkPrependToPath(NkEnvString("/opt/myapp/bin"));
		#endif

		// Vérifier une variable de debug multiplateforme
		NkEnvString debugFlag;
		NkGet(NkEnvString("MYAPP_DEBUG"), debugFlag);
		if (debugFlag == NkEnvString("1") || debugFlag == NkEnvString("true")) {
			EnableDebugMode();
		}
	}
*/
```

## NkFoundationLog.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKFOUNDATIONLOG.H
// =============================================================================
// Ce fichier fournit un système de logging minimaliste et portable pour la
// couche Foundation, avec support multi-plateforme et extensibilité.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Logging basique avec les macros de niveau
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkFoundationLog.h"

	void InitializeSubsystem() {
		NK_FOUNDATION_LOG_INFO("Initialisation du sous-système...");

		if (!LoadConfiguration()) {
			NK_FOUNDATION_LOG_ERROR("Échec de chargement de la configuration");
			return;
		}

		NK_FOUNDATION_LOG_DEBUG("Configuration chargée avec succès");

		#if NK_FOUNDATION_LOG_LEVEL >= NK_FOUNDATION_LOG_LEVEL_TRACE
			NK_FOUNDATION_LOG_TRACE("Détails: configPath=%s", GetConfigPath());
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Logging de valeurs avec formatage automatique
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkFoundationLog.h"

	void ProcessData(int userId, float score, bool isValid) {
		// Formatage automatique des types primitifs
		NK_FOUNDATION_LOG_INFO_VALUE("userId", userId);
		NK_FOUNDATION_LOG_INFO_VALUE("score", score);
		NK_FOUNDATION_LOG_INFO_VALUE("isValid", isValid);

		// Formatage de pointeurs (adresse mémoire)
		void* buffer = AllocateBuffer(1024);
		NK_FOUNDATION_LOG_DEBUG_VALUE("buffer", buffer);

		// Formatage de chaînes
		const char* status = "OK";
		NK_FOUNDATION_LOG_INFO_VALUE("status", status);
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Extension du formatage pour un type personnalisé (ADL)
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkFoundationLog.h"

	// Type personnalisé dans votre namespace
	namespace myapp {
		struct Vector3 {
			float x, y, z;
		};

		// Fonction d'extension pour le formatage (même namespace que Vector3)
		// NOTE: Le nom doit être NKFoundationToString (sans préfixe)
		int NKFoundationToString(const Vector3& v, char* out, size_t outSize) {
			return snprintf(out, outSize, "(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
		}
	}

	// Utilisation : le formateur trouve automatiquement NKFoundationToString via ADL
	void LogPlayerPosition(const myapp::Vector3& position) {
		NK_FOUNDATION_LOG_INFO_VALUE("position", position);
		// Sortie : [INF] file.cpp:42 position=(12.34, 56.78, 90.12)
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Configuration d'un sink de log personnalisé
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkFoundationLog.h"
	#include <fstream>

	// Sink personnalisé pour écrire dans un fichier
	void FileLogSink(const char* level, const char* file, int line, const char* message) {
		static std::ofstream logFile("app.log", std::ios::app);
		if (logFile.is_open()) {
			logFile << "[" << level << "] " << file << ":" << line << " " << message << "\n";
			logFile.flush();
		}
	}

	// Sink personnalisé pour envoyer vers un serveur de logs
	void NetworkLogSink(const char* level, const char* file, int line, const char* message) {
		// Pseudo-code : envoyer via HTTP, UDP, etc.
		// LogServer::Send(level, file, line, message);
	}

	void SetupLogging() {
		// Utiliser le sink fichier en mode debug
		#if defined(_DEBUG)
			nkentseu::platform::NkFoundationSetLogSink(FileLogSink);
		#else
			// En production, utiliser le sink réseau ou le défaut (stderr)
			// nkentseu::platform::NkFoundationSetLogSink(NetworkLogSink);
		#endif
	}

	// Pour restaurer le backend par défaut :
	// nkentseu::platform::NkFoundationSetLogSink(nullptr);
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Contrôle du niveau de log à la compilation
// -----------------------------------------------------------------------------
/*
	// Avant d'inclure NkFoundationLog.h, redéfinir le niveau souhaité :

	// Mode très verbeux pour le debug intensif
	#define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_TRACE
	#include "NKPlatform/NkFoundationLog.h"

	// Ou mode minimal pour la production
	// #define NK_FOUNDATION_LOG_LEVEL NK_FOUNDATION_LOG_LEVEL_ERROR
	// #include "NKPlatform/NkFoundationLog.h"

	void VerboseFunction() {
		// Ces logs seront compilés uniquement si le niveau le permet
		NK_FOUNDATION_LOG_TRACE("Entrée dans VerboseFunction");

		for (int i = 0; i < 100; ++i) {
			NK_FOUNDATION_LOG_TRACE_VALUE("iteration", i);
			// ... traitement ...
		}

		NK_FOUNDATION_LOG_TRACE("Sortie de VerboseFunction");
	}

	// En mode ERROR uniquement, tout le code ci-dessus est éliminé à la compilation
	// (pas d'évaluation des arguments, pas de surcoût runtime)
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec les autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
	#include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
	#include "NKPlatform/NkCompilerDetect.h"   // NKENTSEU_COMPILER_*
	#include "NKPlatform/NkFoundationLog.h"    // Logging

	void StartupDiagnostics() {
		// Log d'informations de plateforme
		NK_FOUNDATION_LOG_INFO("Plateforme: %s (%s)",
			NKENTSEU_PLATFORM_NAME,
			NKENTSEU_PLATFORM_VERSION);

		NK_FOUNDATION_LOG_INFO("Architecture: %s (%d-bit)",
			NKENTSEU_ARCH_NAME,
			NKENTSEU_WORD_BITS);

		NK_FOUNDATION_LOG_INFO("Compilateur: version %d",
			NKENTSEU_COMPILER_VERSION);

		// Log conditionnel par plateforme
		#ifdef NKENTSEU_PLATFORM_ANDROID
			NK_FOUNDATION_LOG_DEBUG("Backend Android logcat activé");
		#endif

		// Log avec formatage de pointeur
		void* mainAddr = reinterpret_cast<void*>(&StartupDiagnostics);
		NK_FOUNDATION_LOG_DEBUG_VALUE("mainAddress", mainAddr);
	}
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Logging thread-safe avec contexte
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkFoundationLog.h"
	#include <thread>
	#include <sstream>

	// Helper pour préfixer les logs avec l'ID de thread
	std::string ThreadedLogPrefix(const char* level) {
		std::ostringstream oss;
		oss << "[" << std::this_thread::get_id() << "] " << level;
		return oss.str();
	}

	void WorkerThread(int workerId) {
		NK_FOUNDATION_LOG_INFO("Thread %d démarré", workerId);

		// Simulation de travail avec logs
		for (int i = 0; i < 5; ++i) {
			NK_FOUNDATION_LOG_DEBUG_VALUE("workerId", workerId);
			NK_FOUNDATION_LOG_DEBUG_VALUE("progress", i * 20);
			// ... traitement ...
		}

		NK_FOUNDATION_LOG_INFO("Thread %d terminé", workerId);
	}

	void LaunchWorkers() {
		std::thread t1(WorkerThread, 1);
		std::thread t2(WorkerThread, 2);

		t1.join();
		t2.join();

		NK_FOUNDATION_LOG_INFO("Tous les workers terminés");
	}

	// Note : Les appels à fprintf/__android_log_print sont généralement thread-safe,
	// mais l'ordre des messages entre threads n'est pas garanti.
	// Pour un logging strictement ordonné, utiliser un sink personnalisé avec mutex.
*/
```

## NKPlatform.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORM.H
// =============================================================================
// Ce fichier est le point d'entrée unique pour le module NKPlatform.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Inclusion minimale pour interrogation plateforme
// -----------------------------------------------------------------------------
/*
	// Fichier : main.cpp
	#include "NKPlatform/NKPlatform.h"  // Seul include nécessaire

	int main() {
		// Accès aux informations système
		const auto* info = nkentseu::platform::NkGetPlatformInfo();

		printf("Platform: %s %s\n", info->osName, info->osVersion);
		printf("Architecture: %s\n", info->archName);
		printf("Compiler: %s %s\n", info->compilerName, info->compilerVersion);

		return 0;
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation des utilitaires système portables
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NKPlatform.h"

	void PortableFileOperation() {
		// Chemin de fichier portable (gère / et \ automatiquement)
		nkentseu::platform::NkPath configPath =
			nkentseu::platform::NkPath::Join({"config", "settings.json"});

		// Vérification d'existence portable
		if (nkentseu::platform::NkFile::Exists(configPath)) {
			// Lecture portable avec gestion d'erreur unifiée
			auto result = nkentseu::platform::NkFile::ReadAllText(configPath);
			if (result.IsSuccess()) {
				ProcessConfig(result.GetValue());
			} else {
				NK_FOUNDATION_LOG_ERROR("Failed to read config: %s",
					nkentseu::platform::NkGetErrorString(result.GetError()));
			}
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Threading portable avec synchronisation
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NKPlatform.h"
	#include <atomic>

	class WorkerPool {
	public:
		WorkerPool(size_t threadCount) {
			for (size_t i = 0; i < threadCount; ++i) {
				m_threads.emplace_back([this]() {
					WorkerThread();
				});
			}
		}

		~WorkerPool() {
			m_stop.store(true);
			m_condition.notify_all();
			for (auto& t : m_threads) {
				if (t.joinable()) {
					t.join();
				}
			}
		}

		void SubmitTask(std::function<void()> task) {
			{
				nkentseu::platform::NkScopedLock lock(m_mutex);
				m_tasks.push(std::move(task));
			}
			m_condition.notify_one();
		}

	private:
		void WorkerThread() {
			while (!m_stop.load()) {
				std::function<void()> task;
				{
					nkentseu::platform::NkScopedLock lock(m_mutex);
					m_condition.wait(lock, [this]() {
						return m_stop.load() || !m_tasks.empty();
					});
					if (!m_tasks.empty()) {
						task = std::move(m_tasks.front());
						m_tasks.pop();
					}
				}
				if (task) {
					task();
				}
			}
		}

		std::vector<nkentseu::platform::NkThread> m_threads;
		std::queue<std::function<void()>> m_tasks;
		nkentseu::platform::NkMutex m_mutex;
		nkentseu::platform::NkConditionVariable m_condition;
		std::atomic<bool> m_stop{false};
	};
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Logging portable avec contexte plateforme
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NKPlatform.h"

	void InitializeApplication() {
		// Logging avec niveau configurable via NKENTSEU_LOG_LEVEL
		NK_FOUNDATION_LOG_INFO("Application starting...");

		// Logging avec localisation source automatique
		auto loc = NkCurrentSourceLocation;
		NK_FOUNDATION_LOG_DEBUG("Init at %s:%d in %s",
			loc.FileName(), loc.Line(), loc.FunctionName());

		// Logging d'erreur avec code plateforme
		auto result = nkentseu::platform::NkNetworkInit();
		if (!result.IsSuccess()) {
			NK_FOUNDATION_LOG_ERROR("Network init failed: %s (code: %d)",
				nkentseu::platform::NkGetErrorString(result.GetError()),
				static_cast<int>(result.GetError()));
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Détection runtime des capacités pour optimisation
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NKPlatform.h"

	// Dispatcher de fonction optimisée selon les capacités CPU
	typedef void (*ComputeFn)(float*, const float*, size_t);

	static ComputeFn sComputeImpl = nullptr;

	void Compute_Scalar(float* out, const float* in, size_t count);
	void Compute_SSE4(float* out, const float* in, size_t count);
	void Compute_AVX2(float* out, const float* in, size_t count);

	void InitializeComputeDispatcher() {
		const auto* info = nkentseu::platform::NkGetPlatformInfo();

		// Sélection par ordre de préférence (plus optimisé en premier)
		if (info->hasAVX2) {
			sComputeImpl = Compute_AVX2;
		} else if (info->hasSSE4_2) {
			sComputeImpl = Compute_SSE4;
		} else {
			sComputeImpl = Compute_Scalar;  // Fallback universel
		}
	}

	void Compute(float* out, const float* in, size_t count) {
		if (!sComputeImpl) {
			InitializeComputeDispatcher();  // Initialisation lazy thread-safe
		}
		sComputeImpl(out, in, count);
	}
*/
```

## NkPlatformConfig.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORMCONFIG.H
// =============================================================================
// Ce fichier fournit des macros et structures pour accéder aux informations
// de configuration de plateforme, tant à la compilation qu'à l'exécution.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration conditionnelle basée sur les macros
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformConfig.h"

	void ConfigureApplication() {
		// Ajustement basé sur le mode de build
		#if NKENTSEU_DEBUG_BUILD
			EnableVerboseLogging();
			DisableOptimizations();
		#elif NKENTSEU_RELEASE_BUILD
			EnablePerformanceMode();
		#endif

		// Ajustement basé sur la plateforme
		#if defined(NKENTSEU_PLATFORM_WINDOWS)
			UseWindowsSpecificAPIs();
		#elif defined(NKENTSEU_PLATFORM_LINUX)
			UseLinuxSpecificAPIs();
		#endif

		// Ajustement basé sur l'architecture
		#if NKENTSEU_ARCH_64BIT
			Enable64BitOptimizations();
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Accès aux informations via PlatformConfig
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformConfig.h"
	#include <cstdio>

	void PrintBuildInfo() {
		const auto& config = nkentseu::platform::GetPlatformConfig();

		std::printf("Platform : %s\n", config.platformName);
		std::printf("Arch     : %s\n", config.archName);
		std::printf("Compiler : %s v%d\n", config.compilerName, config.compilerVersion);
		std::printf("Build    : %s\n", config.isDebugBuild ? "Debug" : "Release");
		std::printf("64-bit   : %s\n", config.is64Bit ? "Yes" : "No");
		std::printf("Endian   : %s\n", config.isLittleEndian ? "Little" : "Big");

		std::printf("\nFeatures:\n");
		if (config.hasUnicode)    std::printf("  Unicode\n");
		if (config.hasThreading)  std::printf("  Threading\n");
		if (config.hasFilesystem) std::printf("  Filesystem (C++17)\n");
		if (config.hasNetwork)    std::printf("  Network\n");

		std::printf("\nLimits:\n");
		std::printf("  Max path length : %d\n", config.maxPathLength);
		std::printf("  Cache line size : %d bytes\n", config.cacheLineSize);
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Sélection de code optimisé via PlatformCapabilities
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformConfig.h"

	void InitializeComputeEngine() {
		const auto& caps = nkentseu::platform::GetPlatformCapabilities();

		// Sélection du meilleur chemin SIMD disponible
		if (caps.hasAVX2) {
			InitializeAVX2Backend();
		} else if (caps.hasAVX) {
			InitializeAVXBackend();
		} else if (caps.hasSSE2) {
			InitializeSSE2Backend();
		} else if (caps.hasNEON) {
			InitializeNEONBackend();
		} else {
			InitializeScalarBackend();
		}

		// Ajustement basé sur la mémoire disponible
		if (caps.totalPhysicalMemory >= 16ULL * 1024 * 1024 * 1024) {
			EnableLargeMemoryMode();
		} else if (caps.totalPhysicalMemory >= 4ULL * 1024 * 1024 * 1024) {
			EnableStandardMemoryMode();
		} else {
			EnableLowMemoryMode();
		}
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion portable des chemins de fichiers
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformConfig.h"
	#include <string>

	std::string BuildConfigPath(const char* filename) {
		// Utiliser le séparateur de chemin de la plateforme cible
		std::string path = "config";
		path += NKENTSEU_PATH_SEPARATOR_STR;
		path += filename;
		return path;
	}

	// Vérifier que le chemin ne dépasse pas la limite de la plateforme
	bool IsValidPath(const char* path) {
		return (path != nullptr) &&
			   (std::strlen(path) < static_cast<size_t>(NKENTSEU_MAX_PATH));
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation des macros d'alignement pour performance
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformConfig.h"

	// Structure alignée sur ligne de cache pour éviter le false sharing
	struct NKENTSEU_CACHE_ALIGNED ThreadCounter {
		alignas(NKENTSEU_CACHE_LINE_SIZE) int64_t counter;
	};

	// Buffer aligné pour opérations SIMD
	NKENTSEU_SIMD_ALIGNED float simdBuffer[32];  // 32 floats = 128 bytes (AVX)

	void ProcessVectorized(float* data, size_t count) {
		// Utiliser le buffer aligné pour des opérations SIMD safe
		#if defined(__AVX__)
			// AVX nécessite un alignement 32 bytes
			ProcessWithAVX(data, count, simdBuffer);
		#elif defined(__SSE2__)
			// SSE2 nécessite un alignement 16 bytes
			ProcessWithSSE2(data, count, simdBuffer);
		#else
			// Fallback scalaire
			ProcessScalar(data, count);
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec d'autres modules NKPlatform
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformDetect.h"   // NKENTSEU_PLATFORM_*
	#include "NKPlatform/NkArchDetect.h"       // NKENTSEU_ARCH_*
	#include "NKPlatform/NkPlatformConfig.h"   // Configuration

	void PlatformAwareInitialization() {
		const auto& config = nkentseu::platform::GetPlatformConfig();
		const auto& caps = nkentseu::platform::GetPlatformCapabilities();

		// Logging des informations de démarrage
		NK_FOUNDATION_LOG_INFO("Starting on %s/%s with %s",
			config.platformName,
			config.archName,
			config.compilerName);

		// Configuration basée sur les capacités détectées
		if (caps.hasDisplay) {
			InitializeGraphics(caps.primaryScreenWidth, caps.primaryScreenHeight);
		} else {
			InitializeHeadlessMode();
		}

		// Ajustement du nombre de threads de travail
		int workerThreads = caps.processorCount;
		if (config.isDebugBuild) {
			workerThreads = 1;  // Simplifier le debug
		}
		InitializeThreadPool(workerThreads);
	}
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Feature detection pour activation conditionnelle
// -----------------------------------------------------------------------------
/*
	#include "NKPlatform/NkPlatformConfig.h"

	class FeatureManager {
	public:
		FeatureManager() {
			const auto& config = nkentseu::platform::GetPlatformConfig();

			// Activer les fonctionnalités selon ce qui est disponible
			if (config.hasFilesystem) {
				m_useStdFilesystem = true;
			} else {
				m_useStdFilesystem = false;
				NK_FOUNDATION_LOG_WARN("Filesystem not available, using fallback");
			}

			if (config.hasNetwork) {
				InitializeNetworkStack();
			}

			if (config.hasThreading) {
				InitializeThreadingPrimitives();
			}
		}

		bool UseStdFilesystem() const {
			return m_useStdFilesystem;
		}

	private:
		bool m_useStdFilesystem = false;
	};
*/
```

## NkPlatformDetect.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORMDETECT.H
// =============================================================================
// Ce fichier fournit des macros pour détecter la plateforme d'exécution et
// exécuter du code conditionnel de manière portable et maintenable.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Obtenir le nom de la plateforme et sa version
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"
	#include <iostream>

	void PrintPlatformInfo() {
		std::cout << "Plateforme : " << NKENTSEU_PLATFORM_NAME << "\n";
		std::cout << "Version    : " << NKENTSEU_PLATFORM_VERSION << "\n";

		#ifdef NKENTSEU_PLATFORM_WINDOWS
			std::cout << "Système : Windows\n";
		#elif defined(NKENTSEU_PLATFORM_LINUX)
			std::cout << "Système : Linux\n";
		#elif defined(NKENTSEU_PLATFORM_MACOS)
			std::cout << "Système : macOS\n";
		#elif defined(NKENTSEU_PLATFORM_ANDROID)
			std::cout << "Système : Android\n";
		#elif defined(NKENTSEU_PLATFORM_IOS)
			std::cout << "Système : iOS\n";
		#else
			std::cout << "Système : inconnu\n";
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Exécution de code spécifique à une plateforme (macros _ONLY)
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"

	void PlatformSpecificInit() {
		// Code exécuté uniquement sur Windows
		NKENTSEU_WINDOWS_ONLY({
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			// Initialisation spécifique à Windows : COM, registry, etc.
		});

		// Code exécuté uniquement sur Linux
		NKENTSEU_LINUX_ONLY({
			// Initialisation spécifique à Linux : signals, epoll, etc.
		});

		// Code exécuté uniquement sur PlayStation 5
		NKENTSEU_PS5_ONLY({
			sceUserServiceInitialize(nullptr);
			// Initialisation PS5 : orbis, libSce, etc.
		});

		// Code exécuté partout SAUF sur Android
		NKENTSEU_NOT_ANDROID({
			// Code qui n'est pas compatible avec Android
			EnableDesktopFeatures();
		});
	}
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation des catégories (Desktop, Mobile, Console)
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"

	void ConfigureGraphics() {
		// Sur PC/Mac/Linux desktop : qualité graphique maximale
		NKENTSEU_DESKTOP_ONLY({
			SetShadowQuality(ShadowQuality::High);
			SetAntiAliasing(AntiAliasingMode::x8);
			EnableRayTracing(true);
		});

		// Sur mobile : optimisation batterie et performance
		NKENTSEU_MOBILE_ONLY({
			SetPowerSaving(true);
			SetAntiAliasing(AntiAliasingMode::x2);
			SetTextureResolution(TextureRes::Medium);
		});

		// Sur console : optimisations spécifiques au hardware
		NKENTSEU_CONSOLE_ONLY({
			UseConsoleGraphicsAPI();
			EnablePlatformSpecificOptimizations();
		});
	}
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Forcer un backend de fenêtrage sur Linux
// -----------------------------------------------------------------------------
/*
	// Avant d'inclure le fichier, définir la macro de force pour sélectionner le backend
	#define NKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY
	#include "NkPlatformDetect.h"

	void CreateWindow() {
		#ifdef NKENTSEU_WINDOWING_WAYLAND
			// Utiliser l'API Wayland native
			struct wl_display* display = wl_display_connect(nullptr);
			if (display == nullptr) {
				// Fallback ou gestion d'erreur
			}
		#elif defined(NKENTSEU_WINDOWING_X11)
			// Utiliser l'API X11 (Xlib ou XCB selon la détection)
			Display* disp = XOpenDisplay(nullptr);
			if (disp == nullptr) {
				// Fallback ou gestion d'erreur
			}
		#endif
	}
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Code portable avec conditionnement par catégorie
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"
	#include <string>
	#include <cstdlib>

	class FileSystem {
	public:
		std::string GetAppDataPath() {
			// Sur Windows : utilisation de %APPDATA%
			NKENTSEU_WINDOWS_ONLY({
				const char* appData = std::getenv("APPDATA");
				if (appData != nullptr) {
					return std::string(appData) + "/MyApp/";
				}
				return "./";
			});

			// Sur macOS : ~/Library/Application Support/
			NKENTSEU_MACOS_ONLY({
				const char* home = std::getenv("HOME");
				if (home != nullptr) {
					return std::string(home) + "/Library/Application Support/MyApp/";
				}
				return "./";
			});

			// Sur Linux : ~/.config/ (convention XDG)
			NKENTSEU_LINUX_ONLY({
				const char* home = std::getenv("HOME");
				if (home != nullptr) {
					return std::string(home) + "/.config/myapp/";
				}
				return "./";
			});

			// Plateforme inconnue ou non gérée : dossier courant
			return "./";
		}
	};
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Gestion des APIs spécifiques avec fallback
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"

	void InitializeAudio() {
		// Initialisation audio avec fallback par plateforme
		NKENTSEU_WINDOWS_ONLY({
			// Windows : utiliser WASAPI ou XAudio2
			InitializeWASAPI();
		});

		NKENTSEU_MACOS_ONLY({
			// macOS : utiliser CoreAudio
			InitializeCoreAudio();
		});

		NKENTSEU_LINUX_ONLY({
			// Linux : utiliser PulseAudio ou ALSA
			#if defined(NKENTSEU_WINDOWING_WAYLAND)
				InitializePipeWire();
			#else
				InitializePulseAudio();
			#endif
		});

		NKENTSEU_PLATFORM_CONSOLE({
			// Consoles : utiliser l'API audio propriétaire
			InitializeConsoleAudio();
		});

		// Fallback générique si aucune plateforme spécifique n'est détectée
		NKENTSEU_NOT_WINDOWS({
			NKENTSEU_NOT_MACOS({
				NKENTSEU_NOT_LINUX({
					InitializeGenericAudio();
				});
			});
		});
	}
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Compilation conditionnelle de fonctionnalités
// -----------------------------------------------------------------------------
/*
	#include "NkPlatformDetect.h"

	class Renderer {
	public:
		void SetupAdvancedFeatures() {
			// Ray tracing uniquement sur desktop puissant et PS5
			#if defined(NKENTSEU_PLATFORM_DESKTOP) || defined(NKENTSEU_PLATFORM_PS5)
				EnableRayTracing();
				EnableDLSS();
			#endif

			// VRR/Adaptive Sync sur plateformes supportées
			#if defined(NKENTSEU_PLATFORM_WINDOWS) || \
				defined(NKENTSEU_PLATFORM_XBOX_SERIES) || \
				defined(NKENTSEU_PLATFORM_PS5)
				EnableVariableRefreshRate();
			#endif

			// HDR sur toutes les plateformes modernes
			#if !defined(NKENTSEU_PLATFORM_UNKNOWN)
				EnableHDRSupport();
			#endif
		}
	};
*/
```

## NkPlatformExport.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION
// =============================================================================
/*
	// Exemple 1 : Compilation de NKPlatform en DLL (CMake)
	// target_compile_definitions(nkplatform PRIVATE NKENTSEU_PLATFORM_BUILD_SHARED_LIB)

	// Exemple 2 : Utilisation statique de NKPlatform
	// target_compile_definitions(monapp PRIVATE NKENTSEU_PLATFORM_STATIC_LIB)

	// Exemple 3 : Utilisation par défaut (DLL import)
	// Aucun define nécessaire, NKENTSEU_PLATFORM_API devient dllimport

	// Exemple 4 : Déclaration dans un header public
	#include "NKPlatform/NkPlatformExport.h"
	NKENTSEU_CLASS_EXPORT class PlatformManager {
	public:
		NKENTSEU_PLATFORM_API void Initialize();
	};
*/
```

## NkPlatformInline.h

```text
// =============================================================================
// EXEMPLES D'UTILISATION DE NKPLATFORMINLINE.H
// =============================================================================

// -----------------------------------------------------------------------------
// Exemple 1 : Fonction inline standard dans un en-tête
// -----------------------------------------------------------------------------
/*
    // math_utils.h
    #include "NkPlatformInline.h"

    NKENTSEU_INLINE float Clamp(float value, float min, float max) {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    // Utilisation : la fonction peut être inlinée à chaque site d'appel
    float result = Clamp(userInput, 0.0f, 1.0f);
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Fonction critique avec force_inline
// -----------------------------------------------------------------------------
/*
    // performance_critical.h
    #include "NkPlatformInline.h"

    // Fonction appelée des millions de fois par frame : forcer l'inlining
    NKENTSEU_FORCE_INLINE float FastNormalize(float x, float y) {
        // Approximation rapide de 1/sqrt pour éviter la division
        float invSq = 1.0f / (x * x + y * y + 1e-8f);
        return __builtin_sqrtf(invSq);
    }

    // Dans la boucle de rendu :
    for (int i = 0; i < particleCount; ++i) {
        float norm = FastNormalize(particles[i].vx, particles[i].vy);
        // ... utilisation de norm
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Fonction de gestion d'erreur avec no_inline
// -----------------------------------------------------------------------------
/*
    // error_handling.h
    #include "NkPlatformInline.h"

    // Fonction rarement appelée : éviter d'encombrer le cache d'instructions
    NKENTSEU_NO_INLINE void LogAndAbort(const char* file, int line, const char* msg) {
        fprintf(stderr, "[%s:%d] FATAL: %s\n", file, line, msg);
        abort();
    }

    // Macro utilitaire pour les assertions
    #define NK_ASSERT(cond) \
        do { \
            if (NKENTSEU_UNLIKELY(!(cond))) { \
                LogAndAbort(__FILE__, __LINE__, #cond); \
            } \
        } while(0)
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Combinaison avec NKENTSEU_PLATFORM_API pour DLL
// -----------------------------------------------------------------------------
/*
    // public_api.h
    #include "NkPlatformExport.h"
    #include "NkPlatformInline.h"

    // Fonction publique inline : exportée mais potentiellement inlinée
    NKENTSEU_API_INLINE int GetLibraryVersion() {
        return 0x010203; // Version 1.2.3 encodée en hex
    }

    // Fonction publique force_inline : pour les getters critiques
    NKENTSEU_API_FORCE_INLINE float* GetVertexData(NkMesh* mesh) {
        return mesh->vertices;
    }

    // Fonction publique no_inline : pour le debugging symbolique
    NKENTSEU_API_NO_INLINE void DebugPrintState(NkContext* ctx);
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Optimisation de branche avec LIKELY/UNLIKELY
// -----------------------------------------------------------------------------
/*
    #include "NkPlatformInline.h"

    NKENTSEU_FORCE_INLINE void ProcessPacket(Packet* pkt) {
        // Le header valide est le cas normal : optimiser pour ce chemin
        if (NKENTSEU_LIKELY(pkt->header.magic == PACKET_MAGIC)) {
            DecodePayload(pkt);
            DispatchToHandler(pkt);
        } else {
            // Chemin d'erreur rare : placé loin du hot path
            NKENTSEU_UNLIKELY(pkt->header.magic == PACKET_MAGIC_LEGACY);
            HandleLegacyPacket(pkt);
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Fonctions pures/const pour optimisation du compilateur
// -----------------------------------------------------------------------------
/*
    #include "NkPlatformInline.h"

    // Fonction pure : peut être éliminée si le résultat n'est pas utilisé
    NKENTSEU_PURE_FUNCTION NKENTSEU_INLINE int ComputeChecksum(const void* data, size_t len) {
        const uint8_t* bytes = (const uint8_t*)data;
        uint32_t sum = 0;
        for (size_t i = 0; i < len; ++i) {
            sum += bytes[i];
        }
        return (int)(sum & 0xFFFF);
    }

    // Fonction const : dépend uniquement des paramètres, pas d'état global
    NKENTSEU_CONST_FUNCTION NKENTSEU_INLINE float Lerp(float a, float b, float t) {
        return a + t * (b - a);
    }

    // Le compilateur peut optimiser :
    // - Appels redondants éliminés (CSE)
    // - Réordonnancement agressif des instructions
    // - Vectorisation automatique si possible
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Combinaison complète dans une classe publique
// -----------------------------------------------------------------------------
/*
    // vector3.h
    #include "NkPlatformExport.h"
    #include "NkPlatformInline.h"

    NKENTSEU_EXTERN_C_BEGIN

    struct NKENTSEU_ALIGN(16) NKENTSEU_CLASS_EXPORT Vector3 {
        float x, y, z, w;

        // Constructeur inline pour performance
        NKENTSEU_FORCE_INLINE Vector3(float x_, float y_, float z_)
            : x(x_), y(y_), z(z_), w(0.0f) {}

        // Getter force_inline : accès fréquent, pas de surcoût d'appel
        NKENTSEU_FORCE_INLINE float LengthSquared() const {
            return x * x + y * y + z * z;
        }

        // Méthode publique exportée mais pas inlinée : debugging facilité
        NKENTSEU_API_NO_INLINE void Normalize();

        // Opérateur avec hint de branche
        NKENTSEU_FORCE_INLINE bool IsNearlyZero(float epsilon) const {
            return NKENTSEU_LIKELY(LengthSquared() < epsilon * epsilon);
        }
    };

    // Fonction utilitaire pure pour optimisation
    NKENTSEU_PURE_FUNCTION NKENTSEU_INLINE float Dot(
        const Vector3& a,
        const Vector3& b
    ) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    NKENTSEU_EXTERN_C_END
*/
```

## NkX11Clean.h

Le commentaire d'origine (le pourquoi), déplacé hors de l'en-tête le 25/11/2026 :

```text
// POURQUOI CE FICHIER EXISTE
//
// Les en-tetes X11 definissent des MACROS portant des noms de mots courants :
// None, Bool, Status, Success, Always... Ce ne sont pas des identifiants mais
// des `#define`, donc ils remplacent aveuglement le texte partout ou il
// apparait ENSUITE — y compris dans des enumerations parfaitement legitimes.
//
// Symptome rencontre : `NkGuiTypes.h` declare `None = 0` dans plusieurs
// enumerations. Des qu'un fichier incluait X11 avant NKGui, le preprocesseur
// transformait `None = 0` en `0L = 0`, et le compilateur repondait « expected
// identifier » sur des lignes parfaitement correctes. Des dizaines d'erreurs,
// toutes a cote de la vraie cause.
//
// CE QU'ON NE FAIT PAS, ET POURQUOI
//
// On ne retire PAS les constantes d'EVENEMENTS (KeyPress, Expose,
// ConfigureNotify...). Le code de fenetrage X11 en a besoin : les supprimer
// deplacerait simplement la casse vers NkXLibWindow.cpp et consorts. Seuls
// sont retires les noms qui entrent en conflit avec du C++ ordinaire.
//
// USAGE : inclure ce fichier APRES les en-tetes X11, dans les fichiers dont le
// contenu est ensuite mele a du code moteur (contexte graphique, RHI). Les
// fichiers purement X11 ne doivent PAS l'inclure.
//
```

## NKPlatform.h (bloc @defgroup)

Remplacé le 25/11/2026 : sa note disait « seul fichier à inclure » (faux : NkEnv.h et NkCPUFeatures.h s'incluent à part), et son exemple appelait NkGetPlatformInfo et NkHasSIMDFeature, qui n'existent pas. Le texte d'origine :

```text
/**
 * @defgroup PlatformModule Module NKPlatform
 * @brief Module de détection et d'abstraction plateforme multi-système
 * @ingroup FrameworkModules
 *
 * NKPlatform fournit une interface unifiée pour :
 *   - Détecter l'OS, l'architecture CPU, le compilateur au runtime
 *   - Accéder aux capacités système (mémoire, cache, SIMD, threading)
 *   - Abstraire les APIs système spécifiques (Win32, POSIX, etc.)
 *   - Fournir des utilitaires portables (alignement, endianness, etc.)
 *
 * @note
 *   - Ce fichier est le seul à inclure pour utiliser le module NKPlatform
 *   - Tous les autres headers du module sont inclus indirectement
 *   - L'ordre d'inclusion est géré automatiquement pour éviter les dépendances circulaires
 *
 * @example
 * @code
 * // Inclusion unique du module Platform
 * #include "NKPlatform/NKPlatform.h"
 *
 * void PrintSystemInfo() {
 *     const auto* info = nkentseu::platform::NkGetPlatformInfo();
 *
 *     printf("Running on %s %s\n", info->osName, info->osVersion);
 *     printf("CPU: %s (%u cores)\n", info->archName, info->cpuCoreCount);
 *
 *     if (nkentseu::platform::NkHasSIMDFeature("AVX2")) {
 *         printf("AVX2 instructions available\n");
 *     }
 * }
 * @endcode
 */
```
