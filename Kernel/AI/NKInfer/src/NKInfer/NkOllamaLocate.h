#pragma once
// =============================================================================
// NkOllamaLocate.h — retrouver un modèle GGUF par son NOM plutôt que par son
// empreinte.
//
// POURQUOI
// --------
// Un blob Ollama se nomme d'après son empreinte SHA-256 :
//   ~/.ollama/models/blobs/sha256-2bada8a7450677000f678be90653b85d364de7db25e…
// Personne ne tape cela de mémoire, et une erreur d'un seul caractère donne un
// « fichier introuvable » qu'on met dix minutes à comprendre. Pire : plusieurs
// modèles voisins cohabitent dans ce dossier, et se tromper de blob donne un
// modèle qui répond n'importe quoi — l'erreur est alors cherchée dans le code
// alors qu'elle est dans le chemin.
//
// Ollama tient pourtant déjà la correspondance nom -> empreinte, dans ses
// manifestes :
//   ~/.ollama/models/manifests/registry.ollama.ai/library/<nom>/<tag>
// un JSON dont la couche « application/vnd.ollama.image.model » porte le digest
// du modèle. On la lit, plutôt que de demander à l'utilisateur de la recopier.
//
// Aucune dépendance à Ollama lui-même : on lit des fichiers, rien de plus. Si
// Ollama n'est pas installé, les fonctions renvoient faux et l'appelant garde
// son chemin explicite.
//
// Zéro STL. Namespace ai::infer.
// =============================================================================

#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKCore/NkTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nkentseu {
	namespace ai {
		namespace infer {

			/// Racine des données Ollama, chemins à la UNIX (séparateurs « / »).
			/// Vide si le profil utilisateur est introuvable.
			inline NkString NkOllamaRoot() {
				const char *home = getenv("USERPROFILE"); // Windows
				if (!home || !*home)
					home = getenv("HOME"); // Linux, macOS
				if (!home || !*home)
					return NkString();
				NkString r;
				for (const char *c = home; *c; ++c)
					r += (*c == '\\') ? '/' : *c;
				r += "/.ollama/models";
				return r;
			}

			/// Dernier segment d'un chemin. Les fonctions d'énumération rendent des
			/// chemins complets ; c'est le NOM qui nous intéresse.
			inline NkString LastSegment(const NkString &path) {
				NkString::SizeType s = path.RFind('/');
				const NkString::SizeType b = path.RFind('\\');
				if (s == NkString::npos || (b != NkString::npos && b > s))
					s = b;
				return (s == NkString::npos) ? path : NkString(path.CStr() + s + 1);
			}

			/// Liste les modèles installés, sous la forme « nom:tag ».
			/// L'arborescence des manifestes EST la liste : un dossier par nom, un
			/// fichier par tag. Pas d'index à tenir à jour, donc rien à
			/// désynchroniser.
			inline bool NkOllamaListModels(NkVector<NkString> &out) {
				const NkString root = NkOllamaRoot();
				if (root.Empty())
					return false;
				const NkString base = root + NkString("/manifests/registry.ollama.ai/library");
				if (!NkDirectory::Exists(base.CStr()))
					return false;
				// GetDirectories/GetFiles rendent des CHEMINS complets : on ne garde
				// que le dernier segment, qui est le nom du modèle ou celui du tag.
				const NkVector<NkString> names = NkDirectory::GetDirectories(base.CStr());
				for (uint32 i = 0; i < names.Size(); ++i) {
					const NkString modelName = LastSegment(names[i]);
					const NkString dir = base + NkString("/") + modelName;
					const NkVector<NkString> tags = NkDirectory::GetFiles(dir.CStr());
					for (uint32 t = 0; t < tags.Size(); ++t)
						out.PushBack(modelName + NkString(":") + LastSegment(tags[t]));
				}
				return out.Size() > 0;
			}

			/// « qwen2.5:7b-instruct » -> chemin ABSOLU du blob GGUF.
			/// Sans « : », le tag « latest » est supposé — c'est la convention
			/// d'Ollama, la reprendre évite une surprise.
			/// Renvoie une chaîne vide si le modèle n'est pas installé.
			inline NkString NkOllamaResolve(const char *nameTag, NkString *err = nullptr) {
				const NkString root = NkOllamaRoot();
				if (root.Empty()) {
					if (err)
						*err = "profil utilisateur introuvable (ni USERPROFILE ni HOME)";
					return NkString();
				}
				NkString name(nameTag ? nameTag : ""), tag("latest");
				const NkString::SizeType colon = name.RFind(':');
				if (colon != NkString::npos) {
					tag = NkString(name.CStr() + colon + 1);
					name = NkString(name.CStr(), colon);
				}
				const NkString manifest =
					root + NkString("/manifests/registry.ollama.ai/library/") + name + NkString("/") + tag;
				if (!NkFile::Exists(manifest.CStr())) {
					if (err)
						*err = NkString("modèle non installé : ") + NkString(nameTag ? nameTag : "");
					return NkString();
				}

				NkFile f(manifest.CStr(), NkFileMode::NK_READ_BINARY);
				if (!f.IsOpen()) {
					if (err)
						*err = NkString("manifeste illisible : ") + manifest;
					return NkString();
				}
				const uint64 size = (uint64)f.Size();
				NkVector<char> buf;
				buf.Resize((NkVector<char>::SizeType)(size + 1));
				const usize got = f.Read(buf.Data(), (usize)size);
				buf[(NkVector<char>::SizeType)got] = 0;

				// On cherche la couche du MODÈLE, pas la première venue : le
				// manifeste décrit aussi le gabarit de prompt, la licence et les
				// paramètres. Prendre le premier digest donnerait un fichier de
				// quelques octets.
				const char *k = std::strstr(buf.Data(), "application/vnd.ollama.image.model");
				if (!k) {
					if (err)
						*err = "aucune couche de modèle dans le manifeste";
					return NkString();
				}
				const char *d = std::strstr(k, "\"digest\":\"");
				if (!d) {
					if (err)
						*err = "manifeste sans digest exploitable";
					return NkString();
				}
				d += 10;
				const char *e = std::strchr(d, '"');
				if (!e) {
					if (err)
						*err = "manifeste tronqué";
					return NkString();
				}
				NkString digest(d, (NkString::SizeType)(e - d));
				// Le manifeste écrit « sha256:… », le fichier s'appelle « sha256-… ».
				for (NkString::SizeType i = 0; i < digest.Size(); ++i)
					if (digest[i] == ':')
						digest[i] = '-';

				const NkString blob = root + NkString("/blobs/") + digest;
				if (!NkFile::Exists(blob.CStr())) {
					if (err)
						*err = NkString("le manifeste désigne un blob absent : ") + blob;
					return NkString();
				}
				return blob;
			}

			/// Affiche les modèles installés — pour un message d'aide utile plutôt
			/// qu'un « fichier introuvable » sec.
			inline void NkOllamaPrintModels() {
				NkVector<NkString> models;
				if (!NkOllamaListModels(models)) {
					printf("  (aucun modèle Ollama trouvé sous %s)\n", NkOllamaRoot().CStr());
					return;
				}
				printf("  Modèles installés :\n");
				for (uint32 i = 0; i < models.Size(); ++i)
					printf("    %s\n", models[i].CStr());
			}

		} // namespace infer
	} // namespace ai
} // namespace nkentseu
