//
// NkUnkenyModulesCpp.h
// =============================================================================
// Description :
//   Charger, et RECHARGER A CHAUD, une DLL de scripts C++ (document 01, § 5).
//   Reprend ce que Noge (NkScriptBridge.cpp) et ConquerorLab ont deja prouve :
//   copie par GENERATION avant chargement (Windows verrouille une DLL chargee),
//   la nouvelle chargee AVANT que l'ancienne ne parte, l'ancienne gardee si la
//   nouvelle echoue.
//
// Caracteristiques :
//   - Recharger : nouvelle copie « Scripts.genN.dll » -> chargee -> son point
//     d'entree (nk_unkeny_module_v1) -> enregistree dans NkScripts2D (les classes
//     disparues le sont dites) -> l'hote MIGRE ses instances (l'ancien code les
//     detruit, le nouveau les recree, l'etat prive passe par Sauver/Relire) ->
//     SEULEMENT ALORS l'ancienne copie est dechargee et effacee.
//   - Un echec (fichier absent, point d'entree introuvable, ABI majeure
//     differente) laisse l'ancienne generation ACTIVE : l'iteration ne casse pas
//     la session.
//   - Les copies d'une session tuee sont effacees au demarrage suivant
//     (NettoyerCopies).
//   - Windows (LoadLibrary) et POSIX (dlopen ; ⚪ non eprouve sur Linux ici).
//     Web, Android, iOS, HarmonyOS : rien ne se charge -- les scripts y sont
//     LIES EN STATIQUE dans le jeu construit (§ 5.6), par le meme registre.
//
// ⚠️ C'est le TROISIEME chargeur du depot (Noge, ConquerorLab, Unkeny). Le
//    document 01 demande son extraction en Kernel/System (NkModuleDynamique) ;
//    il est ecrit ici, autonome, pour descendre d'un bloc le jour ou un second
//    client le prendra.
//
// 📌 LES GREFFONS (R36, document 04-greffons.md) s'y brancheront SANS le
//    changer : un greffon est une DLL comme celle des scripts, construite de la
//    meme facon (l'editeur la compile et la recharge a chaud), qui exporte EN
//    PLUS son propre point d'entree (« nk_unkeny_greffon_v1 », sa table : outils
//    d'editeur, composants de jeu). Ce qu'il faudra :
//      1. un NkModulesCpp par greffon (une generation, une copie, comme ici) ;
//      2. Symbole("nk_unkeny_greffon_v1") pour lire SA table (le chargeur ne
//         l'interprete pas : il ne connait que des symboles) ;
//      3. ses classes de SCRIPT, s'il en a, passent par le meme registre
//         (EnregistrerModuleCpp) et la meme migration d'instances ;
//      4. ses noeuds Blueprint : des natifs de plus dans la table de la VM
//         (NkNatifsBp, ajoutes EN FIN, par nom) -- la VM ne change pas.
//    Recharger n'exige PAS de module de scripts : un greffon sans classe de
//    script exporte un module vide (0 classe), ou passe `scripts` nul... la
//    seconde forme est a ajouter avec le premier greffon.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENY_NKUNKENYMODULESCPP_H__
#define __NKENTSEU_UNKENY_NKUNKENYMODULESCPP_H__

#include "NKContainers/String/NkString.h"
#include "Unkeny/Script/NkUnkenyScriptABI.h"

namespace nkentseu {
	namespace unkeny {

		class NkScripts2D;
		class NkHoteScripts2D;

		/// Les modules C++ se chargent-ils dynamiquement sur CETTE plateforme ?
		bool NkModulesCppDynamiques() noexcept;

		class NkModulesCpp {
			public:
				NkModulesCpp() = default;
				~NkModulesCpp();
				NkModulesCpp(const NkModulesCpp &) = delete;
				NkModulesCpp &operator=(const NkModulesCpp &) = delete;

				/// Charge (ou RECHARGE) la DLL `chemin` : voir l'en-tete. `hote`
				/// (facultatif) migre ses instances avant le dechargement de
				/// l'ancienne generation. false : l'ancienne reste active, `erreur`
				/// dit pourquoi.
				bool Recharger(const char *chemin, NkScripts2D &scripts, NkHoteScripts2D *hote, NkString *erreur = nullptr);

				/// Decharge tout. ⚠️ L'hote doit avoir detruit ses instances
				/// (NkHoteScripts2D::Arreter) : leur code part avec la DLL.
				void Decharger();

				/// Le module charge, ou nul.
				const NkUnkModuleV1 *Module() const noexcept {
					return mModule;
				}
				uint32 Generation() const noexcept {
					return mGeneration;
				}
				/// La copie chargee (« .../Scripts.gen3.dll »).
				const NkString &Copie() const noexcept {
					return mCopie;
				}
				/// Un symbole quelconque de la generation chargee (le point d'entree
				/// d'un greffon, R36), ou nul.
				void *Symbole(const char *nom) const noexcept;

				/// Efface les copies « <nom>.genN.<ext> » laissees a cote de `chemin`
				/// par une session tuee. Rend le nombre efface.
				static uint32 NettoyerCopies(const char *chemin);

			private:
				void *mBibli = nullptr;
				const NkUnkModuleV1 *mModule = nullptr;
				NkString mCopie;
				uint32 mGeneration = 0u;
		};

	} // namespace unkeny
} // namespace nkentseu

#endif // __NKENTSEU_UNKENY_NKUNKENYMODULESCPP_H__
