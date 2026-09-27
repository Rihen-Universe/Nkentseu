#pragma once
// -----------------------------------------------------------------------------
// @File    NkGuiRappels.h
// @Brief   La table `nom -> appelable` par-dessus le point d'entrée des `Callback`.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  LA DEMANDE
// =============================================================================
//  Rodolf, le 27/09/2026 : « est-ce que le système pour brancher une fonction ou
//  méthode ou lambda sur les événements callback et autres de `.nkgui` est déjà
//  en place ? »
//
//  La réponse mesurée était : le FIL existe, le BRANCHEMENT non.
//
//    using NkGuiCallbackFn = void (*)(const NkGuiAppelCallback &, void *);
//
//  C'est un pointeur de fonction C nu, et il n'y en a **qu'un** pour tout le
//  document. Trois conséquences, toutes vérifiables :
//
//    1. pas de table par nom : l'application reçoit TOUS les appels et écrit
//       elle-même la chaîne de `if (appel.nom == "sauver")` ;
//    2. une lambda CAPTURANTE ne s'y branche pas — il n'existe aucune conversion
//       vers un pointeur de fonction ;
//    3. une MÉTHODE non plus : il faut passer `this` dans le `void *` et écrire
//       un trampoline statique, à la main, dans chaque application.
//
//  ⚠️ ET LA PREUVE DU BESOIN EST DANS LE BANC LUI-MÊME. `NKGuiInteractTest`
//     écrit SIX fois de suite les deux mêmes lignes :
//
//         s.exe.eval.rappel     = &SurCallback;   // le trampoline
//         s.exe.eval.rappelUser = &rec;           // le `this` déguisé
//
//     Six copies d'un même geste, c'est six occasions de le faire différemment.
//
// =============================================================================
//  CE QUI N'EST PAS RÉÉCRIT, ET C'EST LE POINT
// =============================================================================
//  ⚠️ CETTE TABLE NE REMPLACE PAS `NkGuiCallbackFn`, ELLE LE CONSOMME. Elle EST
//     un `NkGuiCallbackFn` (`Pont`) qui prend la table dans son `void *`. Le
//     mécanisme du dessous reste le seul mécanisme : il n'y a pas deux chemins
//     par lesquels un `Callback` peut atteindre l'application, donc pas deux
//     vérités à tenir d'accord. Une application qui préfère le pointeur nu
//     continue de marcher sans rien changer.
//
//  ⚠️ ET L'APPELABLE N'EST PAS ÉCRIT ICI. `NkFunction<R(Args...)>`
//     (NKContainers/Functional) existe depuis le 10/06/2025, sans STL, et il
//     prend déjà les quatre formes que Rodolf nomme : fonction libre, lambda
//     capturante, méthode non-const, méthode const. En écrire un second aurait
//     été le deuxième délégué du dépôt, et le jour où ils divergent personne ne
//     sait lequel est le bon.
//
// =============================================================================
//  UNE SEULE PORTE, ET PAS DE COMMODITÉ SANS ARGUMENT
// =============================================================================
//  La tentation était d'offrir aussi `Brancher(nom, NkFunction<void()>)` pour
//  les rappels qui ignorent leurs arguments — c'est-à-dire la majorité.
//
//  🔴 DEUX SURCHARGES SUR UN MÊME NOM AURAIENT ÉTÉ UNE AMBIGUÏTÉ, PAS UN
//     CONFORT. Une lambda ne se convertit que vers UNE des deux signatures, et
//     le compilateur doit essayer les deux pour le savoir : le message d'erreur
//     du jour où ça ne marche pas parle de résolution de surcharge, pas du
//     rappel qu'on voulait brancher. Le paramètre s'ignore en trois caractères
//     (`[&](const NkGuiAppelCallback &) { … }`) ; l'ambiguïté, non.
//
// =============================================================================
//  LE SILENCE EST COMPTÉ
// =============================================================================
//  ⚠️ UN `Callback "sauver"` SANS DESTINATAIRE NE DOIT PAS DISPARAÎTRE. Ce dépôt
//     a passé la journée du 27/09 à débusquer SEPT abandons silencieux du même
//     genre (l'infobulle, la couleur illisible, `visible`, les enfants d'une
//     instance, `Checkbox.value`, huit rôles du schéma, `size` en flux). Un nom
//     qu'on n'a pas su servir est donc COMPTÉ (`sansDestinataire`) et le dernier
//     est NOMMÉ (`dernierSansDestinataire`) : sans le nom, le compteur dit qu'il
//     manque quelque chose sans dire quoi, et on le cherche à la main.
//
//  ⚠️ ET « AJOUTÉ » N'EST PAS « REMPLACÉ ». Brancher deux fois le même nom est
//     légitime (un panneau qui reprend la main), mais un total qui confond les
//     deux ne voit pas un remplacement — leçon payée deux fois en deux jours par
//     deux compteurs saturés (`contenu` à 53 239, `ComptePixelsPeints` à 2 063).
//     D'où deux chiffres : `branches` et `remplaces`.
// -----------------------------------------------------------------------------

#ifndef NK_GUI_DOC_NKGUIRAPPELS_H
#define NK_GUI_DOC_NKGUIRAPPELS_H

#include "NKContainers/Functional/NkFunction.h"
#include "NKGui/Doc/NkGuiInteraction.h"

namespace nkentseu {
	namespace nkgui {

		/// La table `nom -> appelable` que l'hôte pose devant l'évaluateur.
		///
		/// ⚠️ ELLE APPARTIENT À L'HÔTE, ET L'ÉVALUATEUR N'EN GARDE QU'UN POINTEUR
		///    (`rappelUser`). Elle doit donc vivre AUSSI LONGTEMPS que les images
		///    qui l'appellent : une table locale à une fonction qui rend la main
		///    est un défaut, pas une commodité. Même leçon que le registre des
		///    jetons et que les tables d'actions et de zones.
		///
		/// Emploi :
		///
		///     NkGuiRappels rappels;                        // membre de l'application
		///     rappels.Brancher("sauver", [&](const NkGuiAppelCallback &) { Sauver(); });
		///     rappels.Brancher("ouvrir", NkGuiRappels::Rappel(this, &App::SurOuvrir));
		///     rappels.BrancherSur(exe.eval);               // une ligne, pas deux
		class NkGuiRappels {
			public:
				/// La signature d'un rappel. L'appel porte son nom, le comportement
				/// d'où il part et jusqu'à quatre arguments déjà évalués.
				using Rappel = NkFunction<void(const NkGuiAppelCallback &)>;

				/// Capacité fixe : ce noyau est zero-STL, et une application
				/// d'édition déclare quelques dizaines d'actions, pas des milliers.
				/// Un dépassement se REFUSE et se compte — il ne s'écrase pas.
				static const uint32 kMax = 96u;

				// ── CE QUE LA TABLE A FAIT, ET CE QU'ELLE N'A PAS SU FAIRE ──────
				uint32 branches = 0u;	///< noms neufs acceptés
				uint32 remplaces = 0u;	///< noms déjà présents, ré-branchés
				uint32 refuses = 0u;	///< la table était pleine
				uint32 servis = 0u;		///< appels remis à un destinataire
				/// ⚠️ LE CHIFFRE QUI COMPTE VRAIMENT. Un `Callback` dont personne
				///    n'a pris la charge est le défaut que cette classe existe pour
				///    rendre visible.
				uint32 sansDestinataire = 0u;
				NkString dernierSansDestinataire;

				void Vider() noexcept {
					for (uint32 i = 0; i < mNb; ++i) {
						mNoms[i] = NkString();
						mRappels[i] = nullptr;
					}
					mNb = 0u;
				}

				uint32 Nombre() const noexcept {
					return mNb;
				}

				/// Remet à zéro les compteurs d'APPEL, sans toucher aux
				/// branchements. À appeler par image, comme
				/// `NkGuiExecution::ReinitialiserCompteurs`.
				///
				/// ⚠️ ELLE NE TOUCHE PAS À `branches`/`remplaces`/`refuses` : ceux-là
				///    décrivent la table, pas l'image. Les remettre à zéro chaque
				///    image aurait effacé la trace d'un branchement REFUSÉ, qui est
				///    justement ce qu'on veut pouvoir lire après coup.
				void ReinitialiserCompteursAppel() noexcept {
					servis = 0u;
					sansDestinataire = 0u;
					dernierSansDestinataire = NkString();
				}

				/// Branche `f` sur le nom `nom`. Rend faux si la table est pleine —
				/// et le compte, plutôt que d'écraser un voisin en silence.
				bool Brancher(NkStringView nom, Rappel f) noexcept {
					const int32 i = Index(nom);
					if (i >= 0) {
						mRappels[(uint32)i] = traits::NkMove(f);
						++remplaces;
						return true;
					}
					if (mNb >= kMax) {
						++refuses;
						return false;
					}
					mNoms[mNb] = NkString(nom);
					mRappels[mNb] = traits::NkMove(f);
					++mNb;
					++branches;
					return true;
				}

				/// Retire le branchement de `nom`. Rend faux s'il n'y en avait pas —
				/// « rien à retirer » et « retiré » sont deux réponses distinctes.
				bool Debrancher(NkStringView nom) noexcept {
					const int32 i = Index(nom);
					if (i < 0)
						return false;
					const uint32 k = (uint32)i;
					const uint32 dernier = mNb - 1u;
					if (k != dernier) {
						mNoms[k] = mNoms[dernier];
						mRappels[k] = traits::NkMove(mRappels[dernier]);
					}
					mNoms[dernier] = NkString();
					mRappels[dernier] = nullptr;
					--mNb;
					return true;
				}

				bool EstBranche(NkStringView nom) const noexcept {
					return Index(nom) >= 0;
				}

				/// Sert un appel. Rend faux quand personne n'a pris la charge — et
				/// l'a déjà COMPTÉ et NOMMÉ avant de rendre.
				bool Servir(const NkGuiAppelCallback &a) noexcept {
					const int32 i = Index(NkStringView(a.nom.CStr()));
					if (i < 0 || mRappels[(uint32)i] == nullptr) {
						++sansDestinataire;
						dernierSansDestinataire = a.nom;
						return false;
					}
					mRappels[(uint32)i](a);
					++servis;
					return true;
				}

				// ── LE PONT VERS LE MÉCANISME NU ────────────────────────────────
				/// ⚠️ C'EST ELLE QUI FAIT QU'IL N'Y A QU'UN SEUL MÉCANISME. La table
				///    n'est pas un second chemin vers l'application : elle est un
				///    `NkGuiCallbackFn` ordinaire, qui se reçoit lui-même dans le
				///    `void *` que l'évaluateur transporte déjà.
				static void Pont(const NkGuiAppelCallback &a, void *user) noexcept {
					NkGuiRappels *t = (NkGuiRappels *)user;
					if (t)
						(void)t->Servir(a);
				}

				/// Pose le pont et la table sur un évaluateur. Une ligne au lieu des
				/// deux que le banc écrivait six fois.
				void BrancherSur(NkGuiEvaluateur &eval) noexcept {
					eval.rappel = &NkGuiRappels::Pont;
					eval.rappelUser = this;
				}

				/// Le nom branché à l'indice `i` — pour un relevé, une sonde, un
				/// panneau qui liste les actions disponibles.
				const NkString &NomA(uint32 i) const noexcept {
					return mNoms[i];
				}

			private:
				int32 Index(NkStringView nom) const noexcept {
					for (uint32 i = 0; i < mNb; ++i)
						if (MemeNom(NkStringView(mNoms[i].CStr()), nom))
							return (int32)i;
					return -1;
				}

				/// ⚠️ COMPARAISON D'OCTETS, PAS DE `strcmp`. Un `NkStringView` n'est
				///    pas terminé par zéro : le lire avec une fonction de la libc
				///    dépasserait la tranche. C'est la même comparaison que celle de
				///    la table des jetons, écrite pour la même raison.
				static bool MemeNom(NkStringView a, NkStringView b) noexcept {
					if (a.Size() != b.Size())
						return false;
					for (usize i = 0; i < a.Size(); ++i)
						if (a.Data()[i] != b.Data()[i])
							return false;
					return true;
				}

				NkString mNoms[kMax];
				Rappel mRappels[kMax];
				uint32 mNb = 0u;
		};

	} // namespace nkgui
} // namespace nkentseu

#endif // NK_GUI_DOC_NKGUIRAPPELS_H

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
