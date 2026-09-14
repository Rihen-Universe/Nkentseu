#pragma once
// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NKAnima/Clip/NkClipRegistry.h — rendre `clipHandle` honnête.
// -----------------------------------------------------------------------------
// POURQUOI CE FICHIER EXISTE.
//
// `NkClipOnTrack::clipHandle` (Noge/Sequencer/NkSequencer.h) est un `nk_uint64`
// dont le commentaire annonce « Handle du NkAnimationClip ». Mesure du
// 2026-09-13, sur tout `Kernel/` et `Engine/` : **aucune fonction, nulle part, ne
// transforme ce nombre en `NkAnimationClip *`**. Chaque application construit ses
// clips et les tient elle-même (NkAnimaEditor, NkAnimaTest, NkLocomotionDemo,
// Sandbox). Le même motif attend ailleurs sans être résolu non plus :
// `Noge/Anim/NkLocomotion.h:156` prend un `clipHandle`,
// `Noge/ECS/Components/Audio/NkAudioComponents.h:14` en déclare un.
//
// Une poignée qui ne se résout pas est une promesse écrite et jamais tenue.
//
// ── CE QUE CE REGISTRE EST, ET SURTOUT CE QU'IL N'EST PAS ────────────────────
//
// **Il DÉSIGNE, il ne POSSÈDE PAS.** Les applications continuent de tenir leurs
// clips et de décider quand ils meurent. On ne trouvera donc ici ni allocation,
// ni destruction, ni compteur de références, ni `delete` : le registre ne gère
// aucune durée de vie. C'est ce qui le distingue de `NkPool`
// (NKContainers/CacheFriendly), qui lui possède sa mémoire et ne convenait pas.
//
// **Un handle invalide se résout en RIEN, et le dit.** Jamais en un clip par
// défaut, jamais en « le premier de la liste ». Un repli silencieux sur un objet
// de substitution a déjà coûté une journée d'enquête à ce dépôt : on cherche
// pourquoi l'animation est fausse, alors que le problème est qu'elle n'a jamais
// été trouvée.
//
// **Un numéro n'est JAMAIS réutilisé.** Le compteur est monotone et ne redescend
// pas quand on retire une entrée. Sans cela, un handle retiré finirait par
// résoudre le clip d'un VOISIN — c'est-à-dire le pire des échecs possibles :
// silencieux, plausible, et faux.
//
// **Le handle 0 n'est jamais valide.** C'est la valeur par défaut de
// `NkClipOnTrack::clipHandle` ; une piste qu'on n'a pas remplie ne doit pas
// tomber par hasard sur le premier clip enregistré.
//
// ── LE CONTRAT QUI COMPTE POUR LA SÉRIALISATION ──────────────────────────────
//
// Un handle est un nombre : il s'écrit dans un fichier, un pointeur non. C'est la
// raison pour laquelle cette forme a été retenue plutôt que des pointeurs directs
// dans le séquenceur — le format `.nkseq` livré le même jour serait mort à la
// naissance.
//
// ⚠️ **Dans un processus neuf, le registre est vide.** Les handles ne sont donc
// stables d'une exécution à l'autre que si les clips sont **réenregistrés dans le
// même ordre**. C'est exactement le contrat que `NkEntityId` impose déjà au monde
// ECS (une entité créée en premier dans un monde neuf reprend le même
// identifiant), et il se vérifie de la même façon : en comparant, pas en
// espérant.
// =============================================================================

#include "NKAnima/Clip/NkAnimation.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace anim {

		class NkClipRegistry {
			public:
				static constexpr nk_uint64 kInvalide = 0ull;

				// Enregistre un clip et rend sa poignée. Le registre NE PREND PAS
				// la propriété : `clip` doit rester vivant tant qu'on veut le
				// résoudre. Rend `kInvalide` si `clip` est nul.
				//
				// Enregistrer DEUX FOIS le même pointeur rend DEUX poignées
				// différentes, volontairement : fusionner en silence ferait qu'un
				// retrait invaliderait une poignée que l'appelant croit à lui.
				nk_uint64 Register(const NkAnimationClip *clip) noexcept {
					mRefus = "";
					if (clip == nullptr) {
						mRefus = "clip nul : rien a designer";
						return kInvalide;
					}
					Entree e;
					e.handle = mProchain++;
					e.clip = clip;
					mEntrees.PushBack(static_cast<Entree &&>(e));
					return e.handle;
				}

				// Résout une poignée. Rend `nullptr` si elle est inconnue, retirée
				// ou nulle — JAMAIS un clip de substitution. La raison est lisible
				// par `DernierRefus()`.
				const NkAnimationClip *Resolve(nk_uint64 handle) const noexcept {
					mRefus = "";
					if (handle == kInvalide) {
						mRefus = "poignee 0 : jamais valide (valeur par defaut d'une piste vide)";
						return nullptr;
					}
					for (uint32 i = 0; i < (uint32)mEntrees.Size(); ++i)
						if (mEntrees[i].handle == handle)
							return mEntrees[i].clip;
					// Distinguer « jamais enregistrée » de « retirée » aide à
					// diagnostiquer : la première accuse l'appelant, la seconde
					// accuse l'ordre des opérations.
					mRefus = (handle < mProchain) ? "poignee retiree du registre"
												  : "poignee jamais enregistree";
					return nullptr;
				}

				// Retire une entrée. Le numéro n'est PAS recyclé : toute résolution
				// ultérieure de cette poignée échouera, et n'ira jamais chercher un
				// voisin.
				bool Unregister(nk_uint64 handle) noexcept {
					mRefus = "";
					for (uint32 i = 0; i < (uint32)mEntrees.Size(); ++i) {
						if (mEntrees[i].handle == handle) {
							mEntrees.Erase(mEntrees.Begin() + i);
							return true;
						}
					}
					mRefus = "poignee inconnue : rien a retirer";
					return false;
				}

				// Vide les désignations. Ne détruit AUCUN clip — le registre n'en a
				// jamais possédé un seul. Le compteur ne redescend pas.
				void Clear() noexcept {
					mEntrees.Clear();
					mRefus = "";
				}

				[[nodiscard]] uint32 Count() const noexcept {
					return (uint32)mEntrees.Size();
				}

				[[nodiscard]] bool Contains(nk_uint64 handle) const noexcept {
					if (handle == kInvalide)
						return false;
					for (uint32 i = 0; i < (uint32)mEntrees.Size(); ++i)
						if (mEntrees[i].handle == handle)
							return true;
					return false;
				}

				// Raison du dernier refus, en clair. Jamais nulle.
				[[nodiscard]] const char *DernierRefus() const noexcept {
					return mRefus;
				}

			private:
				struct Entree {
						nk_uint64 handle = kInvalide;
						const NkAnimationClip *clip = nullptr;
				};

				NkVector<Entree> mEntrees;
				// Monotone, et il commence à 1 : le 0 reste l'absence.
				nk_uint64 mProchain = 1ull;
				// `mutable` parce que `Resolve` est `const` et doit pourtant
				// pouvoir nommer son refus. L'alternative — rendre `Resolve` non
				// const — obligerait tous les appelants à tenir un registre
				// modifiable pour une simple lecture.
				mutable const char *mRefus = "";
		};

	} // namespace anim
} // namespace nkentseu
