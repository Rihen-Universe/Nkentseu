// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// =============================================================================
// Noge/ECS/Systems/NkSceneComponentsSerialization.h
// =============================================================================
// LES COMPOSANTS QUI SAVENT S'ECRIRE ET SE RELIRE.
//
// `NkSceneSerializer` est une machine a registre : elle ne sait ecrire que les
// composants qu'on lui a presentes. MESURE du 2026-09-13 : le registre etait
// VIDE — `RegisterComponentSerializer` n'etait appele nulle part dans le depot,
// et `SerializeEntity` portait de toute facon un TODO qui n'ecrivait rien tout
// en rendant `true`. Une scene « sauvegardee » ne contenait donc que des
// identifiants d'entites, et le journal annoncait « Scene sauvegardee ».
//
// Ce fichier remplit le registre pour les composants d'une scene Noge :
//     NkName             le nom affiche dans l'Outliner
//     NkTransform        position, rotation (quaternion), echelle LOCALES
//     NkMeshComponent    le chemin du maillage et son etat visible
//     NkMaterialComponent  les chemins de materiau par slot
//
// POURQUOI ICI ET PAS DANS NOGEE : ces composants appartiennent a Noge, pas a
// l'editeur. Les enregistrer cote application aurait donne une scene lisible par
// Nogee seul — et deux hotes qui ecrivent deux formats finissent toujours par
// diverger. Ici, tout hote de Noge y gagne.
//
// APPEL EXPLICITE, ET C'EST DELIBERE : pas d'enregistrement par constructeur
// statique. L'ordre d'initialisation statique entre unites de compilation n'est
// pas garanti, et ce depot a deja paye un registre statique qui gardait un
// pointeur mort. On appelle cette fonction quand on veut, on sait quand elle
// passe, et elle est idempotente.
// =============================================================================

namespace nkentseu {
	namespace noge {

		// Enregistre les serialiseurs des composants de scene. Idempotente :
		// `RegisterComponentSerializer` remplace une entree de meme nom.
		void RegisterNogeSceneComponents() noexcept;

	} // namespace noge
} // namespace nkentseu
