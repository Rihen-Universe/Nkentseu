#pragma once
// =============================================================================
// NkcTextAudit — le registre des libelles qui ne tiennent pas dans leur cadre.
//
// POURQUOI CE FICHIER EXISTE
// --------------------------
// « Ca deborde » ne se compare pas. Une largeur mesuree contre une largeur
// disponible, si. Ce registre note, a chaque image, tout libelle dont la
// largeur RENDUE depasse la largeur du cadre ou on le pose. Le panneau Sortie
// en fait une liste. Le defaut devient alors un nombre : 0 debordement, ou 7.
//
// CE QU'IL COUTE
// --------------
// Rien quand tout tient : on ne note QUE le cas de depassement, et la mesure
// (`MeasureWidth`) etait de toute facon deja faite par le code de dessin pour
// centrer le texte. Aucune allocation : tableau fixe, chaines copiees dans des
// tampons. Quand le tableau est plein, on compte les debordements suivants sans
// les detailler — mieux vaut une liste tronquee mais honnete sur son total.
//
// POURQUOI IL RESSERVIRA
// ----------------------
// Le francais est en moyenne 27 % plus long que l'anglais (mesure sur la table
// de `NkcLang.h` : 1115 caracteres contre 878, jusqu'a 2,8x sur un libelle isole
// — « Interrompre » / « Stop »). Le jour ou une langue est reellement branchee,
// chaque libelle change de largeur. Ce registre est le controle qui dira, sans
// relire l'atelier ecran par ecran, lesquels ne tiennent plus.
// =============================================================================

#include "NKCore/NkTypes.h"

#include <cstdio>
#include <cstring>

namespace nkentseu {
	namespace conqueror {

		inline constexpr uint32 kAuditTextMax  = 96;
		inline constexpr uint32 kAuditCapacity = 64;

		struct NkcTextOverflow {
				char	text[kAuditTextMax];
				float32 wanted;	 // largeur rendue du libelle, en pixels
				float32 avail;	 // largeur du cadre qui devait le contenir
				uint32	hits;	 // nombre d'images ou le cas s'est presente
		};

		/// Registre unique. Volontairement un singleton : le dessin traverse une
		/// dizaine de panneaux qui n'ont aucune raison de se passer un pointeur
		/// de diagnostic de main en main.
		class NkcTextAudit {
			public:
				static NkcTextAudit &Get() noexcept {
					static NkcTextAudit a;
					return a;
				}

				/// Appele par le code de dessin quand `wanted > avail`.
				void Note(const char *s, float32 wanted, float32 avail) noexcept {
					if (!s || !*s) return;
					++mTotal;
					for (uint32 i = 0; i < mCount; ++i) {
						if (std::strncmp(mItems[i].text, s, kAuditTextMax - 1) == 0) {
							++mItems[i].hits;
							// On garde le pire cas vu : c'est celui qui se voit.
							if (wanted - avail > mItems[i].wanted - mItems[i].avail) {
								mItems[i].wanted = wanted;
								mItems[i].avail	 = avail;
							}
							return;
						}
					}
					if (mCount >= kAuditCapacity) {
						++mDropped;
						return;
					}
					NkcTextOverflow &e = mItems[mCount++];
					std::snprintf(e.text, kAuditTextMax, "%s", s);
					e.wanted = wanted;
					e.avail	 = avail;
					e.hits	 = 1;
				}

				uint32				   Count() const noexcept { return mCount; }
				uint32				   Dropped() const noexcept { return mDropped; }
				uint64				   Total() const noexcept { return mTotal; }
				const NkcTextOverflow &Item(uint32 i) const noexcept { return mItems[i]; }

				/// Remet a zero. Utile avant une passe de verification pour ne pas
				/// lire des debordements corriges depuis.
				void Clear() noexcept {
					mCount	 = 0;
					mDropped = 0;
					mTotal	 = 0;
				}

			private:
				NkcTextOverflow mItems[kAuditCapacity] = {};
				uint32			mCount				   = 0;
				uint32			mDropped			   = 0;
				uint64			mTotal				   = 0;
		};

		inline void NkcNoteOverflow(const char *s, float32 wanted, float32 avail) noexcept {
			NkcTextAudit::Get().Note(s, wanted, avail);
		}

	} // namespace conqueror
} // namespace nkentseu
