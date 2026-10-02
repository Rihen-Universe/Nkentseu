#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleJournal.h
// @Brief   LE TIROIR « JOURNAL » de la famille, a la maniere de l'Output Log
//          d'Unreal : les puces « Tout N / Avertissements N / Erreurs N », la
//          recherche, « Copier » et « Effacer », puis les lignes -- la plus
//          RECENTE en haut, les erreurs sur une bande rouge, les avertissements
//          sur une bande ambre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT : UnkenyEditor, OngletJournal (NkEditeurTiroir.cpp) et ses
// aides NkEditeurPucesNiveau / NkEditeurBandeNiveau (NkEditeurOngletJournal.cpp).
// Le NIVEAU d'une ligne est donne par l'application (elle seule sait ce qu'est
// une erreur chez elle).
// -----------------------------------------------------------------------------

#include "NKEditorKit/Famille/NkFamilleStyle.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace editorkit {

		enum class NkFamilleNiveau : uint8 { Info = 0, Succes, Avertissement, Erreur };

		struct NkFamilleJournal {
				NkVector<NkString> lignes;
				NkVector<uint8> niveaux; ///< NkFamilleNiveau, une par ligne
				int32 filtre = 0;		 ///< 0 tout, 1 avertissements, 2 erreurs
				char recherche[96] = {};
				bool rechercheFocus = false;
				float32 defil = 0.f;
				NkString retour;		 ///< « 3 ligne(s) copiée(s) », un instant
				float32 retourAge = 99.f;
				uint32 maximum = 400;

				void Ajouter(const char *texte, NkFamilleNiveau niveau = NkFamilleNiveau::Info);
				void Effacer();
		};

		void NkFamilleDessinerJournal(NkFamilleCtx &c, const nkgui::NkRect &zone, NkFamilleJournal &j, float32 dt);

	} // namespace editorkit
} // namespace nkentseu
