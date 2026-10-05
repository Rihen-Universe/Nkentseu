//
// NkAssetMeta.cpp
// =============================================================================
// Description :
//   Le fichier de metadonnees « .nkmeta » (NkAssetMeta.h) : texte JSON a cles
//   ordonnees (NkArchive garde l'ordre d'insertion), lecture tolerante, GUID
//   unique, empreinte d'un fichier.
//
// Auteur   : TEUGUIA TADJUIDJE Rodolf Séderis (« Rihen »)
// Copyright: (c) 2022-2026 TEUGUIA TADJUIDJE Rodolf Séderis — Rihen Universe.
//            Tous droits réservés. Logiciel propriétaire : voir LICENSE.
//            Copie, reproduction, modification, redistribution et usage par une
//            IA interdits sans autorisation écrite.
// =============================================================================
#include "NKSerialization/Asset/NkAssetMeta.h"

#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKSerialization/JSON/NkJSONWriter.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace nkentseu {

	namespace {
		/// Le format et sa version, en tete du fichier.
		constexpr const char *NK_META_FORMAT = "nkentseu.meta";
		constexpr nk_uint32 NK_META_VERSION = 1u;

		nk_uint64 Melanger(nk_uint64 x) noexcept {
			// splitmix64 : chaque bit de l'entree touche chaque bit de la sortie.
			x += 0x9E3779B97F4A7C15ULL;
			x = (x ^ (x >> 30u)) * 0xBF58476D1CE4E5B9ULL;
			x = (x ^ (x >> 27u)) * 0x94D049BB133111EBULL;
			return x ^ (x >> 31u);
		}

		NkString Hex64(nk_uint64 v) {
			return NkString::Fmtf("%016llx", static_cast<unsigned long long>(v));
		}
		nk_uint64 LireHex64(const NkString &s) {
			nk_uint64 v = 0u;
			for (nk_size i = 0; i < s.Length(); ++i) {
				const char c = s.CStr()[i];
				nk_uint64 d = 0u;
				if (c >= '0' && c <= '9') {
					d = static_cast<nk_uint64>(c - '0');
				} else if (c >= 'a' && c <= 'f') {
					d = static_cast<nk_uint64>(c - 'a' + 10);
				} else if (c >= 'A' && c <= 'F') {
					d = static_cast<nk_uint64>(c - 'A' + 10);
				} else {
					break;
				}
				v = (v << 4u) | d;
			}
			return v;
		}

		/// La nature d'un fichier d'apres son extension (NkAssetTypeFromExtension),
		/// « File » pour le reste (une image, un son, un source...).
		NkString NatureDe(const char *file) {
			const char *point = file != nullptr ? std::strrchr(file, '.') : nullptr;
			const char *barre = file != nullptr ? std::strrchr(file, '/') : nullptr;
			if (point == nullptr || (barre != nullptr && point < barre)) {
				return NkString("File");
			}
			const NkAssetType t = NkAssetTypeFromExtension(point);
			if (t != NkAssetType::Unknown) {
				return NkString(NkAssetTypeName(t));
			}
			struct E {
					const char *ext;
					const char *nature;
			};
			static const E connues[] = {{".png", "Image"},	   {".jpg", "Image"},	  {".jpeg", "Image"}, {".bmp", "Image"},
										{".tga", "Image"},	   {".wav", "Sound"},	  {".ogg", "Sound"},  {".mp3", "Sound"},
										{".ttf", "Font"},	   {".otf", "Font"},	  {".cpp", "SourceCpp"}, {".h", "SourceCpp"},
										{".hpp", "SourceCpp"}, {".json", "Data"},	  {".txt", "Text"}};
			for (const E &e : connues) {
				const char *a = point;
				const char *b = e.ext;
				bool egal = true;
				for (; *a != '\0' && *b != '\0'; ++a, ++b) {
					const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a - 'A' + 'a') : *a;
					egal = egal && ca == *b;
				}
				if (egal && *a == '\0' && *b == '\0') {
					return NkString(e.nature);
				}
			}
			return NkString("File");
		}
	} // namespace

	NkString NkAssetMeta::Setting(const char *key, const char *fallback) const {
		for (nk_size i = 0; key != nullptr && i < importSettings.Size(); ++i) {
			if (importSettings[i].key == key) {
				return importSettings[i].value;
			}
		}
		return NkString(fallback != nullptr ? fallback : "");
	}

	void NkAssetMeta::SetSetting(const char *key, const char *value) {
		if (key == nullptr) {
			return;
		}
		for (nk_size i = 0; i < importSettings.Size(); ++i) {
			if (importSettings[i].key == key) {
				importSettings[i].value = value != nullptr ? value : "";
				return;
			}
		}
		NkAssetMetaSetting s;
		s.key = key;
		s.value = value != nullptr ? value : "";
		importSettings.PushBack(s);
	}

	NkString NkAssetMetaIO::PathFor(const char *file) {
		return NkString(file != nullptr ? file : "") + EXTENSION;
	}

	bool NkAssetMetaIO::IsMetaPath(const char *path) {
		if (path == nullptr) {
			return false;
		}
		const nk_size n = std::strlen(path);
		const nk_size e = std::strlen(EXTENSION);
		if (n <= e) {
			return false;
		}
		for (nk_size i = 0; i < e; ++i) {
			const char c = path[n - e + i];
			const char b = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
			if (b != EXTENSION[i]) {
				return false;
			}
		}
		return true;
	}

	nk_uint64 NkAssetMetaIO::Fingerprint(const char *file) {
		if (file == nullptr || !NkFile::Exists(file)) {
			return 0u;
		}
		const nk_uint64 taille = static_cast<nk_uint64>(NkFile::GetFileSize(file));
		const nk_uint64 date = static_cast<nk_uint64>(NkFileSystem::GetLastWriteTime(file));
		// FNV-1a 64 sur les 16 octets (taille, date).
		nk_uint64 h = 14695981039346656037ULL;
		const nk_uint64 mots[2] = {taille, date};
		for (nk_uint64 m : mots) {
			for (int k = 0; k < 8; ++k) {
				h = (h ^ ((m >> (8 * k)) & 0xFFu)) * 1099511628211ULL;
			}
		}
		return h != 0u ? h : 1u;
	}

	NkAssetId NkAssetMetaIO::GenerateUnique() {
		static nk_uint64 compteur = 0u;
		++compteur;
		const nk_uint64 haute = static_cast<nk_uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
		const nk_uint64 murale = static_cast<nk_uint64>(std::chrono::system_clock::now().time_since_epoch().count());
#if defined(_WIN32)
		const nk_uint64 processus = static_cast<nk_uint64>(_getpid());
#else
		const nk_uint64 processus = static_cast<nk_uint64>(getpid());
#endif
		const nk_uint64 adresse = static_cast<nk_uint64>(reinterpret_cast<nk_size>(&compteur));
		NkAssetId id;
		id.lo = Melanger(haute ^ Melanger(compteur) ^ (processus << 32u));
		id.hi = Melanger(murale ^ Melanger(adresse) ^ Melanger(id.lo) ^ compteur);
		if (id.lo == 0u && id.hi == 0u) {
			id.lo = 1u;
		}
		return id;
	}

	NkString NkAssetMetaIO::ToText(const NkAssetMeta &meta) {
		NkArchive a;
		a.SetString("format", NK_META_FORMAT);
		a.SetUInt32("version", NK_META_VERSION);
		a.SetString("guid", meta.guid.ToString().View());
		a.SetString("nature", meta.nature.View());
		NkVector<NkArchive> reglages;
		for (nk_size i = 0; i < meta.importSettings.Size(); ++i) {
			NkArchive r;
			r.SetString("cle", meta.importSettings[i].key.View());
			r.SetString("valeur", meta.importSettings[i].value.View());
			reglages.PushBack(r);
		}
		a.SetObjectArray("import", reglages);
		NkArchive script;
		NkVector<NkArchive> classes;
		for (nk_size i = 0; i < meta.scriptClasses.Size(); ++i) {
			NkArchive c;
			c.SetString("nom", meta.scriptClasses[i].View());
			classes.PushBack(c);
		}
		script.SetObjectArray("classes", classes);
		NkVector<NkArchive> champs;
		for (nk_size i = 0; i < meta.scriptFields.Size(); ++i) {
			NkArchive c;
			c.SetString("classe", meta.scriptFields[i].className.View());
			c.SetString("nom", meta.scriptFields[i].name.View());
			c.SetString("type", meta.scriptFields[i].type.View());
			champs.PushBack(c);
		}
		script.SetObjectArray("champs", champs);
		a.SetObject("script", script);
		a.SetString("empreinte", Hex64(meta.sourceFingerprint).View());
		a.SetString("vignette", Hex64(meta.thumbnailFingerprint).View());
		a.SetString("chemin", meta.lastPath.View());
		NkString texte;
		NkJSONWriter::WriteArchive(a, texte, true, 2);
		texte += "\n";
		return texte;
	}

	bool NkAssetMetaIO::FromText(const char *text, NkAssetMeta &meta, NkString *error) {
		meta = NkAssetMeta();
		NkArchive a;
		NkString err;
		if (text == nullptr || !NkJSONReader::ReadArchive(NkStringView(text), a, &err)) {
			if (error != nullptr) {
				*error = err.Empty() ? NkString("JSON illisible") : err;
			}
			return false;
		}
		NkString format;
		(void)a.GetString("format", format);
		if (!(format == NK_META_FORMAT)) {
			if (error != nullptr) {
				*error = NkString("ce n'est pas un .nkmeta (format « ") + format + " »)";
			}
			return false;
		}
		NkString guid;
		(void)a.GetString("guid", guid);
		meta.guid = NkAssetId::FromString(guid.View());
		if (!meta.guid.IsValid()) {
			if (error != nullptr) {
				*error = NkString("GUID illisible : ") + guid;
			}
			return false;
		}
		(void)a.GetString("nature", meta.nature);
		NkVector<NkArchive> reglages;
		if (a.GetObjectArray("import", reglages)) {
			for (nk_size i = 0; i < reglages.Size(); ++i) {
				NkAssetMetaSetting s;
				(void)reglages[i].GetString("cle", s.key);
				(void)reglages[i].GetString("valeur", s.value);
				meta.importSettings.PushBack(s);
			}
		}
		NkArchive script;
		if (a.GetObject("script", script)) {
			NkVector<NkArchive> classes;
			if (script.GetObjectArray("classes", classes)) {
				for (nk_size i = 0; i < classes.Size(); ++i) {
					NkString nom;
					(void)classes[i].GetString("nom", nom);
					meta.scriptClasses.PushBack(nom);
				}
			}
			NkVector<NkArchive> champs;
			if (script.GetObjectArray("champs", champs)) {
				for (nk_size i = 0; i < champs.Size(); ++i) {
					NkAssetMetaField f;
					(void)champs[i].GetString("classe", f.className);
					(void)champs[i].GetString("nom", f.name);
					(void)champs[i].GetString("type", f.type);
					meta.scriptFields.PushBack(f);
				}
			}
		}
		NkString empreinte, vignette;
		(void)a.GetString("empreinte", empreinte);
		(void)a.GetString("vignette", vignette);
		meta.sourceFingerprint = LireHex64(empreinte);
		meta.thumbnailFingerprint = LireHex64(vignette);
		(void)a.GetString("chemin", meta.lastPath);
		return true;
	}

	bool NkAssetMetaIO::Read(const char *metaPath, NkAssetMeta &meta, NkString *error) {
		if (metaPath == nullptr || !NkFile::Exists(metaPath)) {
			if (error != nullptr) {
				*error = NkString("absent");
			}
			return false;
		}
		return FromText(NkFile::ReadAllText(metaPath).CStr(), meta, error);
	}

	bool NkAssetMetaIO::Write(const char *metaPath, const NkAssetMeta &meta) {
		if (metaPath == nullptr) {
			return false;
		}
		const NkString texte = ToText(meta);
		if (NkFile::Exists(metaPath) && NkFile::ReadAllText(metaPath) == texte) {
			return true;
		}
		return NkFile::WriteAllText(metaPath, texte.CStr());
	}

	bool NkAssetMetaIO::Ensure(const char *file, NkAssetMeta &meta, bool *created) {
		if (created != nullptr) {
			*created = false;
		}
		if (file == nullptr || IsMetaPath(file)) {
			return false;
		}
		const NkString chemin = PathFor(file);
		if (Read(chemin.CStr(), meta)) {
			return true;
		}
		meta = NkAssetMeta();
		meta.guid = GenerateUnique();
		meta.nature = NatureDe(file);
		meta.sourceFingerprint = Fingerprint(file);
		if (created != nullptr) {
			*created = true;
		}
		return Write(chemin.CStr(), meta);
	}

	bool NkAssetMetaIO::Reidentify(const char *metaPath) {
		NkAssetMeta meta;
		if (!Read(metaPath, meta)) {
			return false;
		}
		meta.guid = GenerateUnique();
		return Write(metaPath, meta);
	}

} // namespace nkentseu
