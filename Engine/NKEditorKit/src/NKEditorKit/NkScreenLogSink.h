#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkScreenLogSink.h
// @Brief   LE NEUVIEME PUITS : celui qui ecrit A L'ECRAN. Il ne dessine rien.
//          Il DEPOSE ; l'affichage RELIT.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI CE FICHIER EXISTE, ET POURQUOI IL EST SI PETIT
//   NKLogger a deja une architecture de puits : `NkISink`, `NkLogger::AddSink`,
//   et huit puits concrets (console, fichier, journalier, rotatif, asynchrone,
//   distributeur, nul, JSON). Il n'en manquait qu'un : celui qui rend le message
//   VISIBLE dans la vue. En l'ecrivant ici, tout ce qui journalise deja recoit
//   l'affichage sans rien connaitre de l'ecran -- le rendu, l'import, la
//   physique, et demain les scripts et le nodal.
//
// ⚠️ LA QUESTION DU FIL EST DECISIVE, ET ELLE EST MESUREE
//   `NkLogger::LogInternal` (NkLogger.cpp, l. 470-485) appelle `sink->Log()`
//   SYNCHRONEMENT, SUR LE FIL DE L'APPELANT, en tenant deja son propre mutex.
//   Un puits qui dessinerait depuis ce point dessinerait donc depuis n'importe
//   quel fil -- un plantage qui n'arrive qu'en production. D'ou la separation :
//   `Log()` ne fait que COPIER dans un tampon borne ; `Drain()` est appele par
//   le fil d'affichage, une fois par image.
//
// ⚠️ ET UN INTERBLOCAGE EVITE, NOMME
//   `Log()` est appele avec le mutex du JOURNAL deja pris. L'ordre de prise est
//   donc journal -> puits. Si `Drain()` journalisait quoi que ce soit en tenant
//   le mutex du puits, on obtiendrait l'ordre inverse, et deux fils suffiraient
//   a bloquer le programme. `Drain()` copie sous verrou et ne journalise
//   JAMAIS ; c'est ecrit ici pour que personne ne l'ajoute plus tard.
//
// CE QUE CE FICHIER NE FAIT PAS
//   Il ne vieillit pas les messages et n'en choisit pas la couleur. La duree de
//   vie appartient a l'AFFICHAGE, qui possede l'horloge de la boucle : deux
//   horloges pour une meme disparition, ce sont deux verites.
// -----------------------------------------------------------------------------

#include "NKLogger/NkSink.h"
#include "NKLogger/NkLog.h"
#include "NKLogger/NkLoggerFormatter.h"
#include "NKThreading/NkMutex.h"
#include "NKMemory/NkSharedPtr.h"
#include "NKMemory/NkUniquePtr.h"

#include <cstring>

// ⚠️ MESURE, PAS PRECAUTION. `NKThreading/NkMutex.h` inclut `<synchapi.h>` sous
//    Windows, qui tire `windef.h` et ses macros de l'ere 16 bits : `pascal`,
//    `near`, `far`. La premiere version de cet en-tete a casse le banc du kit
//    sur `uint32 total = 0, resolus = 0, pascal = 0;` -- une ligne qui n'a rien
//    a voir avec nous, et dont le message d'erreur ne nomme pas le coupable.
//    Un en-tete partage ne propage pas ce piege a ses consommateurs.
#if defined(pascal)
#	undef pascal
#endif
#if defined(near)
#	undef near
#endif
#if defined(far)
#	undef far
#endif

namespace nkentseu {
	namespace editorkit {

		/// Longueur retenue d'un message. Ce qui depasse est COUPE, et la coupe
		/// se voit (points de suspension) : un message tronque sans le dire ment
		/// sur sa fin, or la fin d'un refus est toujours « ce qu'il faut faire ».
		inline constexpr uint32 kEcranLogTexte = 240;

		/// Profondeur du tampon. Une rafale plus longue perd ses PLUS ANCIENS, et
		/// le nombre de perdus est COMPTE : le zero se dit.
		inline constexpr uint32 kEcranLogMax = 64;

		struct NkEcranLogEntree {
				char texte[kEcranLogTexte] = {};
				NkLogLevel niveau = NkLogLevel::NK_INFO;
				uint32 repetitions = 1; ///< fusion des doublons DANS le tampon
		};

		// ── LE PUITS ────────────────────────────────────────────────────────────
		class NkEcranLogSink final : public NkISink {
			public:
				NkEcranLogSink() {
					SetName("ecran");
					// Le puits ne laisse monter a l'ecran que ce qui merite d'y
					// monter. Le filtre du JOURNAL reste en amont et le demeure :
					// ce niveau-ci ne peut que RESSERRER, jamais elargir.
					SetLevel(NkLogLevel::NK_WARN);
				}

				// ── NkISink : le contrat ────────────────────────────────────────
				void Log(const NkLogMessage &message) override {
					if (!IsEnabled() || !ShouldLog(message.level))
						return;
					// Le texte BRUT, pas le texte formate : l'ecran n'a que faire
					// de l'horodatage ni du nom du fil, et le formateur du puits
					// se fait reecrire son motif par le journal a chaque emission
					// (cf. LogInternal) -- s'en servir donnerait une forme qu'on
					// ne controle pas.
					const char *src = message.message.CStr();
					threading::NkScopedLockMutex verrou(mMutex);
					// Fusion : le meme texte au meme niveau, deja present, compte
					// une fois de plus. Une boucle qui hurle mille fois laisse une
					// ligne et « x 1000 », pas mille lignes.
					for (uint32 i = 0; i < mCount; ++i) {
						if (mBuf[i].niveau == message.level &&
							std::strncmp(mBuf[i].texte, src ? src : "", kEcranLogTexte) == 0) {
							++mBuf[i].repetitions;
							return;
						}
					}
					if (mCount >= kEcranLogMax) {
						// Le plus ANCIEN cede. Et sa perte se compte.
						for (uint32 i = 1; i < kEcranLogMax; ++i)
							mBuf[i - 1] = mBuf[i];
						--mCount;
						++mPerdus;
					}
					NkEcranLogEntree &e = mBuf[mCount++];
					e.niveau = message.level;
					e.repetitions = 1;
					Copier(e.texte, src);
				}

				void Flush() override {
					// Rien a vider : le tampon n'est pas une file d'ecriture, c'est
					// ce que l'affichage n'a pas encore relu. Le vider ici ferait
					// disparaitre des messages a chaque `logger.Flush()` -- et le
					// journal en emet un apres chaque message critique.
				}

				void SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> formatter) override {
					mFormatter = traits::NkMove(formatter);
				}
				void SetPattern(const NkString &pattern) override {
					if (mFormatter)
						mFormatter->SetPattern(pattern);
					else
						mFormatter = memory::NkMakeUnique<NkLoggerFormatter>(pattern);
				}
				NkLoggerFormatter *GetFormatter() const override {
					return mFormatter.Get();
				}
				NkString GetPattern() const override {
					return mFormatter ? mFormatter->GetPattern() : NkString{};
				}

				// ── CE QUE LIT LE FIL D'AFFICHAGE ───────────────────────────────
				/// Vide le tampon dans `sortie` et rend le nombre ecrit.
				/// ⚠️ N'APPELEZ RIEN DE JOURNALISANT DEPUIS L'INTERIEUR : voir
				///    l'interblocage nomme en tete de fichier.
				uint32 Drain(NkEcranLogEntree *sortie, uint32 maxi) {
					if (!sortie || maxi == 0)
						return 0;
					threading::NkScopedLockMutex verrou(mMutex);
					const uint32 n = mCount < maxi ? mCount : maxi;
					for (uint32 i = 0; i < n; ++i)
						sortie[i] = mBuf[i];
					// Ce qui ne tenait pas reste en tete du tampon pour l'image
					// suivante : on ne jette pas ce qu'on n'a pas rendu.
					for (uint32 i = n; i < mCount; ++i)
						mBuf[i - n] = mBuf[i];
					mCount -= n;
					return n;
				}

				/// Nombre de messages tombes du tampon depuis le dernier appel.
				/// Le zero SE DIT : un affichage qui ne pose pas la question ne
				/// distingue pas « rien n'a ete perdu » de « je n'ai pas regarde ».
				uint32 ReprendrePerdus() {
					threading::NkScopedLockMutex verrou(mMutex);
					const uint32 p = mPerdus;
					mPerdus = 0;
					return p;
				}

				uint32 EnAttente() const {
					threading::NkScopedLockMutex verrou(mMutex);
					return mCount;
				}

			private:
				static void Copier(char *dst, const char *src) {
					if (!src) {
						dst[0] = '\0';
						return;
					}
					uint32 i = 0;
					for (; src[i] && i + 1 < kEcranLogTexte; ++i)
						dst[i] = src[i];
					dst[i] = '\0';
					if (src[i] && i >= 3) { // la coupe SE VOIT
						dst[i - 3] = '.';
						dst[i - 2] = '.';
						dst[i - 1] = '.';
					}
				}

				mutable threading::NkMutex mMutex;
				NkEcranLogEntree mBuf[kEcranLogMax];
				uint32 mCount = 0;
				uint32 mPerdus = 0;
				memory::NkUniquePtr<NkLoggerFormatter> mFormatter;
		};

		// ── LE BRANCHEMENT, FAIT UNE FOIS ───────────────────────────────────────
		/// Le pointeur NU vers le puits branche, ou `nullptr`.
		/// ⚠️ LA PROPRIETE EST AU JOURNAL, pas ici : `AddSink` prend un
		///    `NkSharedPtr<NkISink>` et le garde jusqu'a `ClearSinks()` ou la
		///    destruction du journal, c'est-a-dire la fin du programme. Ce
		///    pointeur nu est donc valide aussi longtemps qu'on s'en sert.
		///    (`NkSharedPtr` de ce depot n'a AUCUN constructeur de conversion
		///    derive -> base : on ne peut pas garder un `NkSharedPtr<NkEcranLogSink>`
		///    et le passer a `AddSink`. Mesure faite avant d'ecrire ceci.)
		inline NkEcranLogSink *&NkEcranLogPuits() {
			static NkEcranLogSink *puits = nullptr;
			return puits;
		}

		/// Branche le puits sur le journal GLOBAL (`logger`). Rend `true` la
		/// premiere fois, `false` ensuite -- brancher deux fois peindrait chaque
		/// message deux fois, et c'est exactement le genre de doublon qu'on ne
		/// voit qu'en production.
		inline bool NkBrancherEcranLog(NkLogLevel niveauMini = NkLogLevel::NK_WARN) {
			NkEcranLogSink *&p = NkEcranLogPuits();
			if (p)
				return false;
			NkEcranLogSink *brut = new NkEcranLogSink();
			brut->SetLevel(niveauMini);
			memory::NkSharedPtr<NkISink> partage(brut);
			NkLog::Instance().AddSink(partage);
			p = brut;
			return true;
		}

		/// Debranche le puits. ⚠️ EXISTE POUR LE NEGATIF : la preuve demandee est
		/// « retirez le puits, le message ne doit PLUS apparaitre ». Sans cette
		/// porte, le negatif ne serait pas executable et le critere positif ne
		/// prouverait rien. Elle vide TOUS les puits puis rebranche ceux du
		/// journal par defaut : NKLogger n'offre pas de retrait unitaire (mesure
		/// faite -- `NkLogger` expose `AddSink`, `ClearSinks`, `GetSinkCount`,
		/// et rien entre les deux). A n'employer que dans un banc.
		inline void NkDebrancherEcranLogPourNegatif() {
			NkEcranLogSink *&p = NkEcranLogPuits();
			if (!p)
				return;
			p->SetEnabled(false); // il reste branche mais n'accepte plus rien
			p = nullptr;
		}

	} // namespace editorkit
} // namespace nkentseu
