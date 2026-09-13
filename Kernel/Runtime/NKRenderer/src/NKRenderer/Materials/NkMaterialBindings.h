#pragma once
// -----------------------------------------------------------------------------
// @File    NkMaterialBindings.h
// @Brief   Les numeros de binding du SET MATERIAU (set 2), en un seul endroit.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE — un defaut MESURE le 2026-08-22
//   En ajoutant le masque par texture, j'ai voulu verifier que le temoin
//   Demo4..Demo8 saurait attraper une erreur de binding. J'ai donc ecrit
//   volontairement le descripteur sur le binding 10, ABSENT du layout.
//
//   Resultat : **aucune erreur, aucun avertissement, aucune ligne de journal, et
//   les cinq signatures identiques.** Le moteur ecrit sans broncher un
//   descripteur sur un binding que son propre layout ne declare pas. La texture
//   n'arrive jamais au shader, et rien ne le dit.
//
//   C'est la forme exacte de la regle deja inscrite au CLAUDE.md parent — « un
//   parametre qui n'est pas honore est pire qu'un parametre absent ». Un
//   parametre absent casse ; un parametre ignore rend une image plausible.
//
//   Tant que le RHI ne saura pas refuser ce cas, la parade est ici : **UN SEUL
//   nombre, cite par les DEUX cotes**. Le C++ construit son layout et ecrit ses
//   descripteurs avec ces constantes ; le banc `NkMatGraphCheck` lit le SHADER
//   sur le disque et compare le binding qu'il y trouve a la constante. Les deux
//   chemins se valident l'un l'autre — changer un seul cote met le banc au
//   rouge, ce que le temoin d'images ne sait pas faire.
//
// AUCUNE DEPENDANCE, VOLONTAIREMENT. Ce fichier doit pouvoir etre inclus par un
//   banc console qui ne lie ni NKRenderer ni NKRHI. C'est ce qui rend la
//   verification croisee possible sans trainer un device.
// -----------------------------------------------------------------------------

namespace nkentseu {
	namespace renderer {

		// Set 2 = les ressources PAR INSTANCE de materiau. Le layout est UNIQUE et
		// partage par tous les archetypes : ajouter un binding ici le donne a
		// tout le monde, y compris aux gabarits qui n'en veulent pas.
		//
		// ⚠️ 0, 1, 2 sont pris par les sets globaux (camera, objet, lumieres) et
		// le binding 8 par l'UBO du materiau — d'ou le saut de 7 a 9. L'histoire
		// est dans NkMaterialSystem.cpp : un second Add(4, SAMPLER) ecrasait
		// l'Add(4, UBO) dans le tableau de descripteurs GL.
		enum : unsigned int {
			NK_MATBIND_ALBEDO = 3,
			NK_MATBIND_NORMAL_OR_MATCAP = 4,
			NK_MATBIND_ORM = 5,
			NK_MATBIND_EMISSIVE = 6,
			NK_MATBIND_HEIGHT = 7,
			NK_MATBIND_UBO = 8,
			// Carte de masque des couches (LayeredV1). Quatre canaux = quatre
			// masques dans une seule texture.
			NK_MATBIND_LAYER_MASK = 9,
		};

		// Nom du sampler tel qu'il est ECRIT dans le shader. Le banc le cherche
		// par ce nom : renommer d'un cote sans l'autre met le banc au rouge.
		static const char *const NK_MATBIND_LAYER_MASK_SAMPLER = "tMask";

		// ─────────────────────────────────────────────────────────────────────
		// LES SLOTS DE TEXTURE UTILISABLES PAR UN GRAPHE DE MATERIAU
		// ─────────────────────────────────────────────────────────────────────
		// ⚠️ AUCUN BINDING NOUVEAU. C'est le resultat d'une mesure, pas une
		// economie de paresse, et ca ecarte le risque le plus serieux du
		// chantier — toucher au layout PARTAGE par tous les archetypes.
		//
		// CE QUI A ETE MESURE le 2026-08-22, et qui change la reponse :
		//
		// 1. Le shader PBR declare **27 samplers en fragment** a lui seul (5 dans
		//    le set materiau, 22 dans le set global : IBL, atlas d'ombres, 12
		//    lumieres, voxels, LTC, matcap). La specification OpenGL ne garantit
		//    que **16** unites de texture par etage ; le moteur depasse donc deja
		//    le minimum garanti et s'appuie sur les 32 que donnent les pilotes
		//    reels. ⚠️ C'est une dette ANTERIEURE a ce chantier — je la nomme,
		//    je ne l'aggrave pas. Un budget calcule sur « 16 moins ce qui est
		//    pris » serait deja negatif : le chiffre n'aurait aucun sens.
		//
		// 2. Le depot REUTILISE DEJA le meme binding pour des usages differents
		//    selon le shader : le binding 3 porte `tAlbedo` en PBR et
		//    `tReflection` en sol miroir ; le binding 4 porte `tNormal`,
		//    `tMatcap`, `tReflectionBack` ou `tShadowRamp` selon l'archetype.
		//    **Le sens d'un binding est donc LOCAL AU SHADER**, et c'est une
		//    propriete etablie du depot, pas une invention.
		//
		// CONSEQUENCE : un materiau ENGENDRE n'a que faire de `tAlbedo` ou de
		// `tHeight` — ces slots sont morts pour lui. Il reutilise donc les six
		// emplacements que le layout declare deja, dans l'ordre. Le plafond n'est
		// pas un chiffre choisi : **c'est le nombre de slots existants**.
		//
		// Ce que ca evite, et c'est le point : pas de binding neuf, donc pas de
		// nouvelle occasion d'ecrire sur un binding que le layout ne declare pas
		// — la panne silencieuse mesuree le 22/08, qui ne produit ni erreur, ni
		// journal, ni difference d'image.
		static const unsigned int NK_MATBIND_GRAPH_SLOTS[] = {
			NK_MATBIND_ALBEDO,			 // 3
			NK_MATBIND_NORMAL_OR_MATCAP, // 4
			NK_MATBIND_ORM,				 // 5
			NK_MATBIND_EMISSIVE,		 // 6
			NK_MATBIND_HEIGHT,			 // 7
			NK_MATBIND_LAYER_MASK,		 // 9
		};

		// LE PLAFOND. Une seule source : le compilateur le lit ici, le banc le
		// lit ici, et le banc verifie en plus que le shader emis ne declare aucun
		// binding hors de cette table. S'il vivait a deux endroits, un
		// compilateur qui autorise 8 et un layout qui en declare 6 ecrirait sur
		// deux bindings inexistants, sans un mot.
		static const unsigned int NK_MATBIND_GRAPH_SLOT_COUNT = 6;

		// Le nom des samplers engendres. Numerotes, parce qu'un graphe ne sait
		// pas ce que sa Nieme texture represente — c'est l'auteur qui le sait, et
		// il l'a mis dans le nom du noeud, pas dans celui du slot.
		static const char *const NK_MATBIND_GRAPH_SAMPLER_PREFIX = "nkGraphTex";

		// ─────────────────────────────────────────────────────────────────────
		// LE BLOC DES PARAMETRES EXPOSES D'UN GRAPHE
		// ─────────────────────────────────────────────────────────────────────
		// Meme raisonnement que les slots de texture, et meme conclusion : AUCUN
		// binding neuf. Le binding 8 porte l'UBO du materiau pour les archetypes
		// ecrits a la main (`NkPBRParams`, `NkToonParams`...). Un materiau
		// ENGENDRE n'en a aucun usage — il n'a pas de struct figee — donc le slot
		// est libre pour lui, exactement comme `tAlbedo` l'etait.
		//
		// ⚠️ ET C'EST UN BLOC PAR INSTANCE, PAS PAR MATERIAU. Le cas qui compte
		// est « le meme materiau, deux objets, deux valeurs » : il exige que
		// chaque objet ait son propre contenu. Le cout est donc REEL et il est
		// nomme ici plutot que laisse a deduire : un tampon uniforme par objet
		// (ou des push constants pour les petits blocs), donc une ecriture et une
		// liaison de plus par objet dessine. Quelqu'un doit pouvoir contester ce
		// cout en le voyant ecrit, pas le decouvrir dans un profil.
		static const unsigned int NK_MATBIND_GRAPH_PARAMS = NK_MATBIND_UBO; // 8

		// Le nom du bloc dans le shader engendre. Le moteur ne s'en sert pas —
		// il ecrit a des DECALAGES — mais un humain qui lit le shader doit
		// reconnaitre d'ou viennent ces valeurs.
		static const char *const NK_MATBIND_GRAPH_PARAMS_BLOCK = "NkGraphParams";
		static const char *const NK_MATBIND_GRAPH_PARAMS_VAR = "nkParams";

	} // namespace renderer
} // namespace nkentseu
