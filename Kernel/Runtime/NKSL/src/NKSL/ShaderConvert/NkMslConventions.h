// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// NkMslConventions.h — OU VONT LES RESSOURCES EN MSL (2026-09-30)
//
// UNE SEULE convention, partagee par ceux qui ECRIVENT le MSL (NKSL :
// NkShaderConverter::SpirvToMsl, et par lui le chemin NkSL -> MSL) et par celui
// qui le LIE (NKRHI : NkMetalDevice / NkMetalCommandBuffer). Avant elle, chacun
// avait la sienne : SPIRV-Cross numerotait les ressources dans l'ordre de
// decouverte, le generateur MSL natif de NKSL comptait les samplers a part, et
// le device liait « binding N -> index N » avec le tampon de sommets en 0, sur
// le tampon de la camera. Rien ne tombait au bon endroit.
//
// LA CONVENTION (Metal : 31 tampons, 128 textures sur macOS, 16 samplers) :
//   - ressource de descripteur (set S, binding N)  -> buffer(N) / texture(N) /
//     sampler(N), l'ensemble S est ignore : les shaders du moteur numerotent
//     leurs bindings sans doublon d'un ensemble a l'autre (comme pour OpenGL) ;
//   - sampler de binding >= kNkMslMaxSamplerSlots, ou sampler de comparaison
//     (sampler2DShadow...) de binding >= 16 : sampler CONSTEXPR dans le shader
//     (lineaire, bord ; comparaison <= pour l'ombre) -- Metal n'a que 16 cases ;
//   - tampons de sommets : liaison B -> buffer(kNkMslVertexBufferBase + B) ;
//   - push constants : buffer(kNkMslPushConstantBuffer).
//
// En-tete sans dependance, lisible en C++ comme en Objective-C++.
// =============================================================================

namespace nkentseu {

	// Premier index Metal des tampons de sommets (liaison 0 -> 26, 1 -> 27...).
	// Au-dessus de tout binding de tampon des shaders du moteur (le plus haut : 25).
	static constexpr unsigned int kNkMslVertexBufferBase = 26u;
	static constexpr unsigned int kNkMslMaxVertexBuffers = 4u;

	// Index Metal des push constants (setVertexBytes / setFragmentBytes).
	static constexpr unsigned int kNkMslPushConstantBuffer = 30u;

	// Samplers liables par etage (Metal) ; au-dela : sampler constexpr.
	static constexpr unsigned int kNkMslMaxSamplerSlots = 16u;

	// Textures liables par etage (macOS ; 31 sur les anciens iOS).
	static constexpr unsigned int kNkMslMaxTextureSlots = 128u;

} // namespace nkentseu
