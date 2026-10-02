// -----------------------------------------------------------------------------
// FICHIER: Unkeny/Maillage/NkUnkenyMaillageFichier.cpp
// DESCRIPTION: L'asset .nkmesh2d (voir NkUnkenyMaillageFichier.h).
//
// AUTEUR: Rihen
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "Unkeny/Maillage/NkUnkenyMaillageFichier.h"

#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NKMemory.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/NkArchive.h"
#include "Unkeny/Maillage/NkUnkenyMaillagePhysique.h"
#include "Unkeny/Scene/NkUnkenySauvegardeInterne.h"

#include <cstring>

namespace nkentseu {
	namespace unkeny {

		bool NkMaillage2DVersJSON(const NkMaillage2D &m, NkString &json, const NkRessourcesScene &r) {
			// Une COPIE sans etat de jeu ni source : l'asset ne dit pas d'ou il vient.
			NkMaillage2D *pc = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (pc == nullptr) {
				return false;
			}
			*pc = m;
			NkMaillageOublierPhysique(*pc);
			std::memset(pc->source, 0, sizeof(pc->source));
			uint32 n = 0;
			const NkChampSauve *champs = NkChampsMaillage2D(n);
			NkArchive corps;
			interne::EcrireComposant(corps, champs, n, reinterpret_cast<const uint8 *>(pc), r);
			memory::NkGetDefaultAllocator().Delete(pc);
			NkArchive a;
			a.SetString(NkStringView("format"), NkStringView(NK_MAILLAGE2D_FORMAT));
			a.SetInt32(NkStringView("version"), NK_MAILLAGE2D_VERSION);
			a.SetObject(NkStringView("maillage"), corps);
			return NkJSONWriter::WriteArchive(a, json, true, 1);
		}

		bool NkMaillage2DDepuisJSON(NkMaillage2D &m, const char *json, NkScene &scene, const NkRessourcesScene &r,
									NkString *erreur) {
			auto Faux = [erreur](const char *quoi) {
				if (erreur != nullptr) {
					*erreur = NkString(quoi);
				}
				return false;
			};
			if (json == nullptr) {
				return Faux("JSON vide");
			}
			NkArchive a;
			NkString err;
			if (!NkJSONReader::ReadArchive(NkStringView(json), a, &err)) {
				return Faux("JSON illisible");
			}
			NkString format;
			if (!a.GetString(NkStringView("format"), format) || std::strcmp(format.CStr(), NK_MAILLAGE2D_FORMAT) != 0) {
				// « L'en-tete est la verite » : un autre format ne se lit pas comme un maillage.
				return Faux("ce fichier n'est pas un maillage 2D (format)");
			}
			int32 version = 0;
			(void)a.GetInt32(NkStringView("version"), version);
			if (version > NK_MAILLAGE2D_VERSION) {
				return Faux("maillage 2D d'une version plus recente");
			}
			NkArchive corps;
			if (!a.GetObject(NkStringView("maillage"), corps)) {
				return Faux("pas d'objet \"maillage\"");
			}
			NkMaillage2D *pl = memory::NkGetDefaultAllocator().New<NkMaillage2D>();
			if (pl == nullptr) {
				return Faux("memoire");
			}
			uint32 n = 0;
			const NkChampSauve *champs = NkChampsMaillage2D(n);
			interne::LireComposant(corps, champs, n, reinterpret_cast<uint8 *>(pl), scene, r);
			const char *pourquoi = nullptr;
			const bool ok = NkMaillageCoherent2D(*pl, &pourquoi);
			if (ok) {
				m = *pl;
			}
			memory::NkGetDefaultAllocator().Delete(pl);
			return ok ? true : Faux(pourquoi != nullptr ? pourquoi : "maillage incoherent");
		}

		bool NkSauverMaillage2D(const NkMaillage2D &m, const char *chemin, const NkRessourcesScene &r) {
			NkString json;
			if (chemin == nullptr || !NkMaillage2DVersJSON(m, json, r)) {
				return false;
			}
			if (!NkFile::WriteAllText(chemin, json.CStr())) {
				logger.Warn("[unkeny] ecriture du maillage impossible : {0}", chemin);
				return false;
			}
			return true;
		}

		bool NkChargerMaillage2D(NkMaillage2D &m, const char *chemin, NkScene &scene, const NkRessourcesScene &r,
								 NkString *erreur) {
			if (chemin == nullptr) {
				return false;
			}
			const NkString json = NkFile::ReadAllText(chemin);
			if (json.Empty()) {
				if (erreur != nullptr) {
					*erreur = NkString("fichier absent ou vide : ") + chemin;
				}
				return false;
			}
			return NkMaillage2DDepuisJSON(m, json.CStr(), scene, r, erreur);
		}

	} // namespace unkeny
} // namespace nkentseu
