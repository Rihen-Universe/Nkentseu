/**
 * @File    NkPAConfig.cpp
 * @Brief   Lecture/écriture de la config NKPA (fichier texte éditable).
 */

#include "NkPAConfig.h"

#include "NKFileSystem/NkFile.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
	namespace nkpa {

		const char *NkPAConfig::ApiName(NkGraphicsApi api) {
			switch (api) {
				case NkGraphicsApi::NK_GFX_API_OPENGL:
					return "opengl";
				case NkGraphicsApi::NK_GFX_API_VULKAN:
					return "vulkan";
				case NkGraphicsApi::NK_GFX_API_DX11:
					return "dx11";
				case NkGraphicsApi::NK_GFX_API_DX12:
					return "dx12";
				case NkGraphicsApi::NK_GFX_API_SOFTWARE:
					return "software";
				default:
					return "opengl";
			}
		}

		bool NkPAConfig::ParseApi(const NkString &name, NkGraphicsApi &out) {
			// Comparaison insensible à la casse via minuscule.
			NkString n = name;
			n.Trim();
			n = n.ToLower();
			if (n == "opengl" || n == "gl") {
				out = NkGraphicsApi::NK_GFX_API_OPENGL;
				return true;
			}
			if (n == "vulkan" || n == "vk") {
				out = NkGraphicsApi::NK_GFX_API_VULKAN;
				return true;
			}
			if (n == "dx11" || n == "d3d11") {
				out = NkGraphicsApi::NK_GFX_API_DX11;
				return true;
			}
			if (n == "dx12" || n == "d3d12") {
				out = NkGraphicsApi::NK_GFX_API_DX12;
				return true;
			}
			if (n == "software" || n == "sw" || n == "cpu") {
				out = NkGraphicsApi::NK_GFX_API_SOFTWARE;
				return true;
			}
			return false;
		}

		NkPAConfig NkPAConfig::Load(const NkString &path) {
			NkPAConfig cfg;
			if (!NkFile::Exists(path.CStr()))
				return cfg; // défauts

			NkString content = NkFile::ReadAllText(path.CStr());
			// Parse ligne par ligne : "clef = valeur", '#' = commentaire.
			usize i = 0;
			const usize len = content.Size();
			while (i < len) {
				usize lineEnd = i;
				while (lineEnd < len && content[lineEnd] != '\n' && content[lineEnd] != '\r')
					++lineEnd;
				NkString line = content.SubStr(i, lineEnd - i);
				i = lineEnd + 1;

				line.Trim();
				if (line.Empty() || line[0] == '#')
					continue;

				const usize eq = line.Find('=');
				if (eq == NkString::npos)
					continue;
				NkString key = line.SubStr(0, eq);
				NkString val = line.SubStr(eq + 1, line.Size() - eq - 1);
				key.Trim();
				val.Trim();

				if (key.ToLower() == "backend") {
					NkGraphicsApi api;
					if (ParseApi(val, api))
						cfg.backend = api;
					else
						logger.Warn("[NKPA-Config] backend inconnu '{0}' → défaut", val.CStr());
				}
			}
			return cfg;
		}

		bool NkPAConfig::Save(const NkString &path) const {
			NkString out;
			out += "# NKPA — configuration (editable a la main ou via l'interface)\n";
			out += "# backend = opengl | vulkan | dx11 | dx12 | software\n";
			out += "backend = ";
			out += ApiName(backend);
			out += "\n";
			const bool ok = NkFile::WriteAllText(path.CStr(), out.CStr());
			if (!ok)
				logger.Error("[NKPA-Config] echec ecriture '{0}'", path.CStr());
			return ok;
		}

	} // namespace nkpa
} // namespace nkentseu
