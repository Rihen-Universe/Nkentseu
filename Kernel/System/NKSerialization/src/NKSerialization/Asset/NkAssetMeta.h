//
// NkAssetMeta.h
// =============================================================================
// Description :
//   LE FICHIER DE METADONNEES qui accompagne chaque fichier d'un projet :
//   « <fichier>.nkmeta », a cote de lui (CONVENTIONS_FICHIERS.md § 7, decision
//   proposee le 2026-10-05). Il porte ce qui doit SURVIVRE a un deplacement, un
//   renommage ou une copie du fichier : son identite (un GUID stable), sa
//   nature, ses reglages d'import, pour un script sa classe et le cache de ses
//   champs exposes, et l'empreinte du fichier (vignettes, import a refaire).
//
// Caracteristiques :
//   - TEXTE (JSON, cles dans un ordre fixe) : il se lit, se compare et se
//     fusionne sous git, comme le .meta d'Unity -- il fait partie du projet.
//   - Le GUID ne change JAMAIS pour un fichier deplace ou renomme (le
//     navigateur deplace le compagnon avec lui) ; une COPIE recoit un GUID neuf
//     (Reidentify) : deux fichiers ne partagent jamais une identite.
//   - Scenes, prefabs et Blueprints gardent le CHEMIN et le GUID de ce qu'ils
//     citent : un chemin perime se repare par le GUID a l'ouverture.
//   - Un champ inconnu d'un lecteur ancien est ignore ; un champ absent prend
//     sa valeur par defaut (version 1).
//
// Algorithmes implementes :
//   - GenerateUnique : 128 bits melanges (splitmix64) depuis l'horloge haute
//     resolution, l'horloge murale, le processus, une adresse et un compteur --
//     NkAssetId::Generate, lui, repart de la meme graine a chaque lancement.
//   - Empreinte d'un fichier : FNV-1a 64 de sa taille et de sa date.
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis (« Rihen »)
// Copyright: (c) 2022-2026 TEUGUIA TADJUIDJE Rodolf Séderis — Rihen Universe.
//            Tous droits réservés. Logiciel propriétaire : voir LICENSE.
//            Copie, reproduction, modification, redistribution et usage par une
//            IA interdits sans autorisation écrite.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_SERIALIZATION_ASSET_NKASSETMETA_H__
#define __NKENTSEU_SERIALIZATION_ASSET_NKASSETMETA_H__

#include "NKSerialization/Asset/NkAssetMetadata.h"

namespace nkentseu {

	/// Un champ EXPOSE d'une classe de script (le cache du .nkmeta d'un source).
	struct NkAssetMetaField {
			NkString className; ///< « Jeu::Porte »
			NkString name;		///< « vitesse »
			NkString type;		///< « float », « int », « bool », « Vec2 »...
	};

	/// Un reglage d'import : une cle, une valeur en texte.
	struct NkAssetMetaSetting {
			NkString key;
			NkString value;
	};

	/// Le contenu d'un « .nkmeta ».
	struct NkAssetMeta {
			/// L'IDENTITE du fichier : stable a travers deplacements et renommages.
			NkAssetId guid;
			/// La nature (NkAssetTypeName, ou « File » pour un fichier quelconque).
			NkString nature;
			NkVector<NkAssetMetaSetting> importSettings;
			/// (scripts) Les classes que le fichier declare, et leurs champs exposes.
			NkVector<NkString> scriptClasses;
			NkVector<NkAssetMetaField> scriptFields;
			/// L'empreinte du fichier quand ce .nkmeta a ete mis a jour (taille,
			/// date) : une vignette ou un import plus ancien est a refaire.
			nk_uint64 sourceFingerprint = 0u;
			/// L'empreinte du fichier pour laquelle la vignette en cache a ete
			/// faite (0 : aucune vignette).
			nk_uint64 thumbnailFingerprint = 0u;
			/// Le chemin (relatif au projet) ou ce compagnon a ete vu pour la derniere
			/// fois. Deux fichiers au meme GUID (une copie faite HORS de l'editeur) :
			/// celui qui est a CETTE place garde l'identite, l'autre en recoit une neuve.
			NkString lastPath;

			/// La valeur d'un reglage d'import, ou `fallback`.
			NkString Setting(const char *key, const char *fallback = "") const;
			void SetSetting(const char *key, const char *value);
	};

	class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkAssetMetaIO {
		public:
			/// L'extension du compagnon, point compris.
			static constexpr const char *EXTENSION = ".nkmeta";
			/// Le compagnon de `file` : « Contenu/Porte.png » -> « Contenu/Porte.png.nkmeta ».
			static NkString PathFor(const char *file);
			/// `path` est-il un compagnon (« ....nkmeta ») ? Les listes de fichiers le cachent.
			static bool IsMetaPath(const char *path);
			/// L'empreinte de `file` (taille et date), 0 s'il n'existe pas.
			static nk_uint64 Fingerprint(const char *file);

			static NkString ToText(const NkAssetMeta &meta);
			static bool FromText(const char *text, NkAssetMeta &meta, NkString *error = nullptr);
			static bool Read(const char *metaPath, NkAssetMeta &meta, NkString *error = nullptr);
			/// Ecrit le compagnon (seulement si son texte change : sa date ne bouge pas
			/// pour rien). false : ecriture impossible.
			static bool Write(const char *metaPath, const NkAssetMeta &meta);
			/// Le compagnon de `file`, CREE (GUID neuf, nature deduite de
			/// l'extension, empreinte) s'il manque ou s'il est illisible. `created` :
			/// vrai si un GUID neuf a ete donne. false : ni lu ni ecrit.
			static bool Ensure(const char *file, NkAssetMeta &meta, bool *created = nullptr);
			/// Une IDENTITE NEUVE pour le compagnon `metaPath` (celui d'une COPIE) :
			/// GUID neuf, le reste garde. false : illisible ou non ecrit.
			static bool Reidentify(const char *metaPath);
			/// Un GUID que personne d'autre n'a (ni dans cette session, ni ailleurs).
			static NkAssetId GenerateUnique();
	};

} // namespace nkentseu

#endif // __NKENTSEU_SERIALIZATION_ASSET_NKASSETMETA_H__
