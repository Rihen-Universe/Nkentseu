// =============================================================================
// NkModelerProject.cpp — lecture / ecriture du .nk3dm et des projets recents.
//
// JSON via NKSerialization (NkArchive + NkJSONWriter / NkJSONReader) : le depot
// a deja son modele de document et son ecrivain, en ecrire un second aurait
// diverge au premier caractere a echapper.
// =============================================================================

#include "NK3DModeler/Project/NkModelerProject.h"

#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFileSystem.h"
#include "NKSerialization/NkArchive.h"
#include "NKSerialization/JSON/NkJSONWriter.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKTime/NkDate.h"
#include "NKPlatform/NkEnv.h"

#include <cstdio> // snprintf : la date ISO est ecrite a la main (cf. Today())

namespace nkentseu {
	namespace nk3d {

		// ── OUTILS DE CHEMIN ────────────────────────────────────────────────────
		// Tout est ramene a '/' des l'entree. Windows accepte les deux, mais un
		// chemin ecrit dans un fichier avec des '\' ne se compare plus a celui que
		// NKFileSystem rend normalise -- et la couverture « relative » ne se
		// retrouvait plus dans sa racine.
		static NkString Norm(const char *p) {
			NkString out;
			if (!p)
				return out;
			for (const char *c = p; *c; ++c)
				out += (*c == '\\') ? '/' : *c;
			// Une barre finale ferait un double separateur a la concatenation.
			while (out.Size() > 1u && out[out.Size() - 1u] == '/')
				out = NkString(out.CStr(), (NkString::SizeType)(out.Size() - 1u));
			return out;
		}

		static NkString Join(const NkString &a, const char *b) {
			if (a.Empty())
				return NkString(b);
			NkString out = a;
			out += '/';
			out += b;
			return out;
		}

		static NkString DirOf(const NkString &file) {
			const NkString::SizeType s = file.RFind('/');
			if (s == NkString::npos)
				return NkString();
			return NkString(file.CStr(), s);
		}

		static NkString BaseNoExt(const NkString &file) {
			NkString::SizeType b = file.RFind('/');
			b = (b == NkString::npos) ? 0u : (b + 1u);
			NkString::SizeType d = file.RFind('.');
			if (d == NkString::npos || d < b)
				d = (NkString::SizeType)file.Size();
			return NkString(file.CStr() + b, (NkString::SizeType)(d - b));
		}

		// ── UN NOM D'AFFICHAGE PEUT-IL SERVIR DE NOM DE DOSSIER ? ───────────────
		// Les espaces et les accents passent ; ce qui suit ne passe pas, sur aucun
		// systeme. Un point ou un espace en fin de nom est refuse par Windows.
		static bool NameUsableAsFolder(const NkString &n) {
			if (n.Empty() || n.Size() > 100u)
				return false;
			for (NkString::SizeType i = 0u; i < n.Size(); ++i) {
				const char c = n[i];
				if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
					c == '<' || c == '>' || c == '|' || (unsigned char)c < 32u)
					return false;
			}
			const char last = n[n.Size() - 1u];
			return last != '.' && last != ' ';
		}

		// ── LE DOSSIER SUIT LE NOM (reparation au chargement) ───────────────────
		// Renomme le dossier du projet ET son .nk3dm d'apres le nom affiche, quand
		// les deux ont diverge. Chaque condition ci-dessous est un cas ou renommer
		// FERAIT DU MAL : on prefere une divergence visible a un projet perdu.
		static void ReconcileFolderWithName(NkProjectState &st, const NkString &file) {
			const NkString dirName = BaseNoExt(st.root);
			if (st.name == dirName || !NameUsableAsFolder(st.name))
				return;
			const NkString parent = DirOf(st.root);
			if (parent.Empty())
				return; // pas de dossier parent : rien a renommer dans quoi
			const NkString newDir = Join(parent, st.name.CStr());
			if (NkDirectory::Exists(newDir.CStr()) || NkFile::Exists(newDir.CStr()))
				return; // la place est prise -- ecraser un dossier voisin serait pire
			if (!NkDirectory::Move(st.root.CStr(), newDir.CStr()))
				return; // dossier verrouille, droits, disque : on garde tout en place
			// Le dossier a bouge : le .nk3dm l'a suivi, il reste a le renommer.
			NkString moved = Join(newDir, BaseNoExt(file).CStr());
			moved += '.';
			moved += kProjectExt;
			NkString target = Join(newDir, st.name.CStr());
			target += '.';
			target += kProjectExt;
			if (moved != target && !NkFile::Move(moved.CStr(), target.CStr()))
				target = moved; // le dossier porte le bon nom, le fichier garde le sien
			st.root = newDir;
			st.file = target;
		}

		// Chemin normalise AVEC l'extension du projet garantie. POINT DE PASSAGE
		// UNIQUE : « Enregistrer sous » et NkProjectRootFor doivent voir
		// EXACTEMENT le meme chemin. S'ils divergeaient, la scene serait capturee
		// relativement a un dossier different de celui ou elle s'ecrit, et tous
		// ses chemins relatifs pointeraient a cote -- une corruption silencieuse
		// qui ne se verrait qu'a la reouverture.
		static NkString NormWithExt(const char *file) {
			NkString f = Norm(file);
			if (f.Empty())
				return f;
			const NkString ext = NkPath(f.CStr()).GetExtension();
			if (!(ext == NkString(kProjectExt) || ext == NkString(".nk3dm"))) {
				f += '.';
				f += kProjectExt;
			}
			return f;
		}

		// Ecrit a la main plutot qu'avec une vue de chaine : la comparaison de
		// prefixe est le seul usage, et elle doit rester lisible sans aller
		// verifier quel constructeur de NkStringView prend une longueur.
		static bool StartsWith(const NkString &s, const NkString &prefix) {
			if (prefix.Size() > s.Size())
				return false;
			for (NkString::SizeType i = 0; i < prefix.Size(); ++i)
				if (s[i] != prefix[i])
					return false;
			return true;
		}

		static NkString Today() {
			const NkDate d = NkDate::GetCurrent();
			char buf[16];
			// Format ISO ecrit ici plutot que via ToString() : c'est CE format qui
			// est relu, il ne doit pas suivre un changement de presentation.
			snprintf(buf, sizeof(buf), "%04d-%02d-%02d", (int)d.GetYear(), (int)d.GetMonth(),
					 (int)d.GetDay());
			return NkString(buf);
		}

		// ── ECRITURE DU .nk3dm ──────────────────────────────────────────────────
		static bool WriteFile(const NkProjectState &st, const char *file, const NkArchive *sc,
							  NkString *err) {
			NkArchive doc;
			doc.SetInt32("version", kProjectFormatVersion);
			doc.SetString("application", "NK3DModeler");
			doc.SetString("nom", st.name.CStr());
			doc.SetString("creation", st.created.CStr());
			doc.SetString("modification", st.modified.CStr());
			// RELATIF a la racine : c'est la regle n.1 de CONVENTIONS_FICHIERS.md.
			doc.SetString("couverture", st.coverRel.CStr());

			// ── LA SECTION SCENE DIT CE QU'ELLE EST ─────────────────────────────
			// `serialisee` est pose ICI, jamais par le composeur : c'est le seul
			// endroit qui SAIT si une scene a vraiment ete fournie a l'ecriture.
			// Le laisser au composeur ferait qu'une ecriture sans scene pourrait,
			// un jour, oublier de le dire -- et un fichier qui ment sur ce qu'il
			// contient fait perdre du travail.
			//
			// `couvre` / `nonCouvert` enumerent ce qui est REELLEMENT restitue et
			// ce qui ne l'est pas. La liste de reference vit dans l'en-tete de
			// NkModelerScene.h, tenue a jour avec le code ; ce resume est la pour
			// qu'on puisse lire le fichier sans lire le code.
			{
				NkArchive scene;
				if (sc) {
					scene = *sc;
					scene.SetBool("serialisee", true);
					scene.SetString("couvre",
									"l'arborescence du navigateur de projet (dossiers et cartes), "
									"le CHEMIN RELATIF du fichier de chaque asset, et les onglets "
									"ouverts. Le CONTENU de chaque asset vit dans SON fichier "
									"(.nkscene, .nkmesh, .nkmat) -- ce fichier-ci n'en porte que "
									"des liens");
					scene.SetString("nonCouvert",
									"geometrie editee sommet par sommet, modificateurs, contenu "
									"des cartes de texture/graphe/dataset (elles n'ont pas encore "
									"de fichier), environnement de rendu et sortie");
				} else {
					scene.SetBool("serialisee", false);
					scene.SetString("note", "aucun contenu n'a ete fourni a l'ecriture");
				}
				// La cle reste « scene » pour que les projets deja ecrits restent
				// lisibles ; c'est `disposition` qui dit ce qu'elle contient (>= 3 :
				// l'arbre et des liens, plus la scene elle-meme). Renommer la cle
				// aurait rendu illisible tout projet anterieur, ce qui est
				// exactement ce qu'on refuse.
				doc.SetObject("scene", scene);
			}

			const NkString json = NkJSONWriter::WriteArchive(doc, true, 2);
			if (!NkFile::WriteAllText(file, json.CStr())) {
				if (err)
					*err = NkString("ecriture impossible : ") + file;
				return false;
			}
			return true;
		}

		bool NkProjectCreate(const char *parentDir, const char *name, NkProjectState &out,
							 NkString *err, const NkArchive *scene) {
			const NkString parent = Norm(parentDir);
			if (parent.Empty() || !name || !*name) {
				if (err)
					*err = "emplacement ou nom vide";
				return false;
			}
			const NkString dir = Join(parent, name);
			NkString file = dir;
			file += '/';
			file += name;
			file += '.';
			file += kProjectExt;

			// Un .nk3dm deja present = un projet deja la. On refuse plutot que
			// d'ecraser : c'est le genre d'accident qu'on ne pardonne pas.
			if (NkFile::Exists(file.CStr())) {
				if (err)
					*err = NkString("un projet existe deja : ") + file;
				return false;
			}
			if (!NkDirectory::Exists(dir.CStr()) && !NkDirectory::CreateRecursive(dir.CStr())) {
				if (err)
					*err = NkString("dossier impossible a creer : ") + dir;
				return false;
			}

			NkProjectState st;
			st.file = file;
			st.root = dir;
			st.name = name;
			st.created = Today();
			st.modified = st.created;
			if (!WriteFile(st, file.CStr(), scene, err))
				return false;
			st.open = true;
			st.neverSaved = false;
			out = st;
			return true;
		}

		bool NkProjectSave(NkProjectState &st, NkString *err, const NkArchive *scene) {
			if (st.file.Empty()) {
				if (err)
					*err = "aucun chemin : utiliser Enregistrer sous...";
				return false;
			}
			NkProjectPickCover(st);
			st.modified = Today();
			if (!WriteFile(st, st.file.CStr(), scene, err))
				return false;
			st.neverSaved = false;
			st.open = true;
			return true;
		}

		NkString NkProjectRootFor(const char *file) {
			return DirOf(NormWithExt(file));
		}

		bool NkProjectSaveAs(NkProjectState &st, const char *file, NkString *err,
							 const NkArchive *scene) {
			// L'extension est imposee : un projet doit s'identifier sans etre
			// ouvert -- c'est la raison d'etre de la table des extensions.
			NkString f = NormWithExt(file);
			if (f.Empty()) {
				if (err)
					*err = "chemin vide";
				return false;
			}
			const NkString dir = DirOf(f);
			if (!dir.Empty() && !NkDirectory::Exists(dir.CStr())
				&& !NkDirectory::CreateRecursive(dir.CStr())) {
				if (err)
					*err = NkString("dossier impossible a creer : ") + dir;
				return false;
			}
			st.file = f;
			st.root = dir;
			// La couverture etait relative a l'ANCIENNE racine : elle ne veut plus
			// rien dire ici. On la reconstruit depuis le nouveau dossier.
			st.coverRel.Clear();
			if (st.name.Empty())
				st.name = BaseNoExt(f);
			if (st.created.Empty())
				st.created = Today();
			return NkProjectSave(st, err, scene);
		}

		bool NkProjectLoad(const char *file, NkProjectState &out, NkString *err,
						   NkArchive *scene) {
			const NkString f = Norm(file);
			if (f.Empty() || !NkFile::Exists(f.CStr())) {
				if (err)
					*err = NkString("fichier introuvable : ") + f;
				return false;
			}
			const NkString text = NkFile::ReadAllText(f.CStr());
			NkArchive doc;
			NkString perr;
			if (!NkJSONReader::ReadArchive(NkStringView(text.CStr()), doc, &perr)) {
				if (err)
					*err = NkString("projet illisible : ") + perr;
				return false;
			}
			nk_int32 ver = 0;
			if (doc.GetInt32("version", ver) && ver > kProjectFormatVersion) {
				// On LIT quand meme : les champs inconnus sont ignores, et refuser
				// un fichier plus recent que soi rendrait le projet inaccessible
				// pour une nuance de format. Mais on le dit.
				if (err)
					*err = "projet ecrit par une version plus recente : lu au mieux";
			}
			NkProjectState st;
			st.file = f;
			st.root = DirOf(f);
			if (!doc.GetString("nom", st.name) || st.name.Empty())
				st.name = BaseNoExt(f);
			// ── LE DOSSIER SUIT LE NOM DU PROJET ────────────────────────────
			// `nom` est un nom d'AFFICHAGE libre, mais le laisser diverger du
			// dossier qui porte le projet trompe partout ou un chemin s'affiche :
			// le selecteur de fichiers ouvrait « AgentTest » pour un projet que
			// le launcher appelait « MonProjet » (Rihen, 12 aout). On repare donc
			// AU CHARGEMENT, comme Rihen l'a demande -- mais seulement quand c'est
			// SANS RISQUE. Un projet ne doit jamais devenir introuvable pour une
			// question de cosmetique : au moindre doute, on ne touche a rien.
			ReconcileFolderWithName(st, f);
			(void)doc.GetString("creation", st.created);
			(void)doc.GetString("modification", st.modified);
			(void)doc.GetString("couverture", st.coverRel);
			if (st.created.Empty())
				st.created = Today();
			if (st.modified.Empty())
				st.modified = st.created;
			st.open = true;
			st.neverSaved = false;
			// ── LA SECTION SCENE EST RENDUE A L'APPELANT ────────────────────
			// Elle ne l'etait PAS : le parametre existait, la documentation le
			// promettait, et le corps ne le touchait jamais. Consequence exacte,
			// constatee par Rihen : chaque reouverture repartait d'une scene
			// VIERGE (l'archive rendue etant vide, NkSceneRestore n'avait rien a
			// poser), et le premier enregistrement suivant ECRASAIT le fichier
			// avec cet etat vierge -- le travail disparaissait en deux temps,
			// sans qu'aucune erreur ne s'affiche.
			//
			// `serialisee` fait foi : une section qui se declare non serialisee
			// n'a rien a rendre, et la distinction « absente » / « vide » n'a pas
			// lieu d'etre ici -- l'appelant traite les deux pareil.
			if (scene) {
				scene->Clear();
				NkArchive sc;
				nk_bool ser = false;
				if (doc.GetObject("scene", sc) && sc.GetBool("serialisee", ser) && ser)
					*scene = sc;
			}
			out = st;
			return true;
		}

		// ── COUVERTURE : LA CAPTURE LA PLUS RECENTE DU PROJET ───────────────────
		// On ne fabrique RIEN : le rendu et la capture se declenchent par un
		// bouton. On se contente de retenir l'image la plus fraiche trouvee sous
		// la racine -- ce qui donne, en pratique, le dernier rendu produit.
		bool NkProjectPickCover(NkProjectState &st) {
			if (st.root.Empty() || !NkDirectory::Exists(st.root.CStr()))
				return false;
			static const char *const kPat[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp",
											   "*.tga", "*.qoi"};
			NkString best;
			nk_int64 bestT = 0;
			for (int32 p = 0; p < 6; ++p) {
				const NkVector<NkString> files =
					NkDirectory::GetFiles(st.root.CStr(), kPat[p], NkSearchOption::NK_ALL_DIRECTORIES);
				for (usize i = 0; i < files.Size(); ++i) {
					// GetEntries ne remplit PAS ModificationTime sous Windows (note
					// de NkDirectory.cpp) : on interroge le systeme de fichiers.
					const nk_int64 t = NkFileSystem::GetLastWriteTime(files[i].CStr());
					if (t > bestT || best.Empty()) {
						bestT = t;
						best = files[i];
					}
				}
			}
			if (best.Empty())
				return false;
			const NkString abs = Norm(best.CStr());
			NkString rel = abs;
			const NkString rootSlash = st.root + "/";
			if (StartsWith(abs, rootSlash))
				rel = NkString(abs.CStr() + rootSlash.Size(),
							   (NkString::SizeType)(abs.Size() - rootSlash.Size()));
			if (rel == st.coverRel)
				return false;
			st.coverRel = rel;
			return true;
		}

		NkString NkRecentEntry::CoverAbs() const {
			if (cover.Empty() || path.Empty())
				return NkString();
			const NkString dir = DirOf(path);
			if (dir.Empty())
				return cover;
			return Join(dir, cover.CStr());
		}

		// ── RECENTS : LE PATRON DE NKCODE ───────────────────────────────────────
		NkString NkRecentFilePath() {
			const char *home = env::GetEnvVar("USERPROFILE"); // API maison (NkEnv.h)
			if (!home || !*home)
				home = env::GetEnvVar("HOME");
			if (home && *home)
				return Norm(home) + "/.nk3dmodeler_recent.cfg";
			return NkString(".nk3dmodeler_recent.cfg");
		}

		// Decoupe « a|b|c|d ». Les champs manquants restent vides : un fichier
		// ecrit par une version anterieure (chemin seul) reste lisible.
		static void SplitLine(const NkString &line, NkRecentEntry &e) {
			const char *s = line.CStr();
			NkString parts[4];
			int32 k = 0;
			for (const char *c = s; *c && k < 4; ++c) {
				if (*c == '|') {
					++k;
					continue;
				}
				parts[k] += *c;
			}
			e.path = Norm(parts[0].CStr());
			e.name = parts[1];
			e.cover = parts[2];
			e.date = parts[3];
			if (e.name.Empty())
				e.name = BaseNoExt(e.path);
		}

		void NkRecentList::Load() {
			items.Clear();
			texDirty = true;
			const NkString txt = NkFile::ReadAllText(NkRecentFilePath().CStr());
			NkString line;
			auto flush = [&]() {
				if (line.Size() < 3u) {
					line.Clear();
					return;
				}
				const char tag = line[0];
				if ((tag != 'P' && tag != 'R') || line[1] != ' ') {
					line.Clear();
					return;
				}
				NkRecentEntry e;
				e.pinned = (tag == 'P');
				SplitLine(NkString(line.CStr() + 2, (NkString::SizeType)(line.Size() - 2u)), e);
				// UNE ENTREE QUI NE POINTE PLUS SUR RIEN N'EST PAS UN PROJET. Le
				// fichier a pu etre efface, deplace, ou renomme par la reparation
				// « le dossier suit le nom » -- dans tous les cas, l'afficher promet
				// au launcher un projet qui n'existe pas. On l'ecarte au chargement :
				// la liste se nettoie donc d'elle-meme au prochain enregistrement.
				if (!e.path.Empty() && NkFile::Exists(e.path.CStr()))
					items.PushBack(e);
				line.Clear();
			};
			for (const char *p = txt.CStr(); *p; ++p) {
				if (*p == '\n' || *p == '\r')
					flush();
				else
					line += *p;
			}
			flush();
		}

		void NkRecentList::Save() const {
			NkString out;
			// Les EPINGLES d'abord, comme NKCode : l'ordre du fichier EST l'ordre
			// d'affichage, on n'a donc rien a trier a la lecture.
			for (int32 pass = 0; pass < 2; ++pass) {
				for (usize i = 0; i < items.Size(); ++i) {
					const NkRecentEntry &e = items[i];
					if (e.pinned != (pass == 0))
						continue;
					out += e.pinned ? "P " : "R ";
					out += e.path;
					out += '|';
					out += e.name;
					out += '|';
					out += e.cover;
					out += '|';
					out += e.date;
					out += '\n';
				}
			}
			NkFile::WriteAllText(NkRecentFilePath().CStr(), out);
		}

		void NkRecentList::Touch(const NkProjectState &st) {
			if (st.file.Empty())
				return;
			NkRecentEntry e;
			e.path = st.file;
			e.name = st.name;
			e.cover = st.coverRel;
			e.date = st.modified;
			// Deja present : on garde son epingle, on le remonte.
			for (usize i = 0; i < items.Size(); ++i) {
				if (items[i].path == e.path) {
					e.pinned = items[i].pinned;
					items.Erase(items.Begin() + (isize)i);
					break;
				}
			}
			// En TETE : le plus recent d'abord. Les epingles sont replacees devant
			// a l'ecriture, donc l'ordre en memoire n'a pas a les distinguer.
			items.Insert(items.Begin(), e);
			texDirty = true;
			Save();
		}

		void NkRecentList::TogglePin(usize i) {
			if (i >= items.Size())
				return;
			items[i].pinned = !items[i].pinned;
			texDirty = true;
			Save();
		}

		void NkRecentList::Remove(usize i) {
			if (i >= items.Size())
				return;
			items.Erase(items.Begin() + (isize)i);
			texDirty = true;
			Save();
		}

	} // namespace nk3d
} // namespace nkentseu
