//
// NkUnkenyLivraison.h
// =============================================================================
// Description :
//   CUIRE un jeu (ecrire ce que la construction emporte) et le RELIRE (ce que
//   fait le joueur autonome au demarrage). Les deux moities vivent ensemble :
//   un format ecrit ici et lu ailleurs diverge au premier champ ajoute.
//
// Caracteristiques :
//   - Le dossier cuit (le « assets/ » du jeu) :
//         jeu.json                 le SOMMAIRE (voir plus bas)
//         scene.nkscene            la scene, telle que l'editeur l'enregistre
//         textures/<nom>.nktex     chaque texture que la scene NOMME
//         sons/<nom>.nksnd         chaque son demande
//     Les extensions sont celles de CONVENTIONS_FICHIERS.md § 2, tirees de
//     NkAssetExtensionFor : la table n'est jamais recopiee.
//   - Une texture cuite est un actif ordinaire (NkAssetIO) dont le payload est
//     celui du four de NKImage (NkTextureOven), en RGBA brut : NKCanvas
//     televerse du RGBA, et une compression par blocs serait a decompresser a
//     chaque lancement.
//   - Un son cuit est un actif Sound dont le payload est du PCM float32 mono
//     petit-boutiste (NkSons2D::Creer ne prend que du mono).
//   - A la relecture, chaque texture est enregistree SOUS SON NOM D'ORIGINE
//     (« unkeny/sim/caisse ») avant la scene : NkChargerScene la trouve par
//     NkTextures2D::Trouver et ne va pas la chercher sur le disque.
//
// ⚠️ LE SOMMAIRE, `jeu.json` — UN NOM PROVISOIRE, DIT COMME TEL
//   « Aucun nom n'est grave avant le premier octet ecrit »
//   (CONVENTIONS_FICHIERS.md § 1). Ce fichier est un detail INTERNE du jeu
//   construit (l'utilisateur ne l'ouvre ni ne le depose nulle part) : il prend
//   l'extension generique `.json` et le champ "format" = "unkeny.jeu", plutot
//   qu'une extension Nk a soi. Le jour ou il merite un nom, c'est Rihen qui le
//   donne ; le champ "format" suffit a le reconnaitre d'ici la.
//
// ⚠️ UNE RESSOURCE ABSENTE SE NOMME (temoin l2)
//   Le joueur ne montre jamais un ecran noir : ce qui manque est rendu dans
//   NkJeuCharge::manquantes, par son NOM et par son FICHIER, et le joueur
//   l'affiche. Un fichier illisible compte comme absent.
//
// ⚠️ L'EMPREINTE (temoin l1)
//   FNV-1a 64 de la forme JSON de la scene -- celle-la meme qu'ecrit
//   l'enregistrement. La cuisson la calcule sur la scene de l'editeur et
//   l'ecrit au sommaire ; le joueur la recalcule sur la scene RELUE, juste
//   apres le chargement, avant toute trame. Egales : le jeu joue la scene que
//   l'editeur montrait.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYLIVRAISON_H__
#define __NKENTSEU_UNKENY_NKUNKENYLIVRAISON_H__

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringView.h"
#include "Unkeny/Scene/NkUnkenyScene.h"

namespace nkentseu {
	namespace unkeny {

		class NkTextures2D;
		class NkSons2D;

		/// Le nom du sommaire, a la racine du dossier cuit.
		constexpr const char *NK_LIVRAISON_SOMMAIRE = "jeu.json";
		/// La version du sommaire ecrit. Une lecture accepte toute version <=.
		constexpr int32 NK_LIVRAISON_VERSION = 1;

		// --- L'empreinte -----------------------------------------------------
		/// FNV-1a 64 d'un texte.
		uint64 NkEmpreinteTexte(const char *texte, usize longueur) noexcept;
		/// L'empreinte de la scene : celle de sa forme JSON (NkSauverSceneJSON),
		/// camera comprise. 0 si la scene ne se serialise pas.
		uint64 NkEmpreinteScene(NkScene &scene, const NkTextures2D *textures);

		/// Les noms de texture qu'une scene JSON NOMME (sprites), sans doublon,
		/// dans l'ordre de premiere apparition. false si le JSON est illisible.
		bool NkTexturesNommees(NkStringView json, NkVector<NkString> &noms, NkString *erreur = nullptr);

		// --- Cuire -----------------------------------------------------------
		/// Un son a emporter : son nom (celui qu'utilisera NkSons2D::Trouver)
		/// et ses echantillons mono.
		struct NkSonACuire {
				NkString nom;
				NkVector<float32> mono;
				int32 frequence = 44100;
		};

		struct NkDemandeCuisson {
				/// Ou ecrire : le dossier des donnees du jeu (cree au besoin).
				NkString dossier;
				NkString nomJeu;
				/// La taille, en pixels, du viseur ou la scene etait regardee. Le
				/// joueur s'en sert pour montrer LE MEME MORCEAU DE MONDE dans une
				/// fenetre d'une autre taille. 0 = garder le zoom tel quel.
				float32 vueLargeur = 0.f;
				float32 vueHauteur = 0.f;
				NkVector<NkSonACuire> sons;
		};

		struct NkRapportCuisson {
				/// Chaque fichier ecrit, relatif au dossier.
				NkVector<NkString> fichiers;
				/// Ce qui n'a pas pu etre cuit (une texture sans pixels...).
				NkVector<NkString> erreurs;
				uint64 empreinte = 0u;
				uint32 textures = 0u;
				uint32 sons = 0u;
		};

		/// Ecrit le jeu dans `demande.dossier` : la scene, ses textures, les
		/// sons demandes, le sommaire. false si le sommaire ou la scene n'a pas
		/// pu etre ecrit ; une ressource ratee est dans `rapport.erreurs` et
		/// n'empeche pas le reste.
		/// ⚠️ Les pixels viennent de NkTextures2D::Pixels : une texture chargee
		///    avec `garderPixels = false` ne peut pas etre cuite (et le dit).
		bool NkCuireJeu(NkScene &scene, const NkTextures2D &textures, const NkDemandeCuisson &demande,
						NkRapportCuisson &rapport);

		// --- Relire ----------------------------------------------------------
		struct NkJeuCharge {
				NkString nom;
				NkString scene;	 ///< le fichier de scene, relatif au dossier
				uint64 empreinteAttendue = 0u; ///< celle du sommaire
				uint64 empreinteLue = 0u;	   ///< celle de la scene relue
				float32 vueLargeur = 0.f;
				float32 vueHauteur = 0.f;
				uint32 textures = 0u; ///< textures relues
				uint32 sons = 0u;	  ///< sons relus
				/// Chaque ressource absente ou illisible, NOMMEE (temoin l2).
				NkVector<NkString> manquantes;
				/// Ce qui empeche de jouer : sommaire ou scene introuvable. Vide =
				/// la scene est chargee (meme s'il manque une texture).
				NkString erreur;

				bool Jouable() const noexcept {
					return erreur.Empty();
				}
				bool EmpreinteIdentique() const noexcept {
					return empreinteAttendue != 0u && empreinteAttendue == empreinteLue;
				}
		};

		/// Relit un jeu cuit. `dossier` : le prefixe des chemins, barre finale
		/// comprise (« assets/ » sur ordinateur, « » sur Android ou tout est
		/// lu dans l'APK). `sons` peut etre nul : les sons du sommaire ne sont
		/// alors pas lus (`sortie.sons` reste a 0), sans que ce soit une absence.
		/// Rend Jouable().
		bool NkChargerJeu(const char *dossier, NkScene &scene, NkTextures2D &textures, NkSons2D *sons, NkJeuCharge &sortie);

		/// Le nom du jeu (« Gelée »), lu au sommaire SANS rien charger d'autre :
		/// la fenetre doit porter son titre des sa creation, avant que la scene
		/// ne soit lue. Vide si le sommaire est absent ou illisible.
		NkString NkNomDuJeu(const char *dossier);

		/// Un nom de ressource (« unkeny/sim/caisse ») en nom de fichier sur :
		/// tout ce qui n'est pas lettre, chiffre, point, tiret ou souligne
		/// devient « _ ».
		NkString NkNomDeFichierSur(const char *nom);

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYLIVRAISON_H__
