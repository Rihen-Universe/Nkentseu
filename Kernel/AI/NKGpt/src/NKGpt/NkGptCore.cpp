// =============================================================================
// NKGpt/NkGptCore.cpp — implémentation des briques réutilisables (voir NkGptCore.h)
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#include "NKGpt/NkGptCore.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKLogger/NkLog.h" // logger (status) ; FILE* conservé pour l'I/O binaire du checkpoint

#include <cstdio>
#if defined(_WIN32)
#include <io.h> // _commit : forcer l'ecriture physique d'un checkpoint
#else
#include <unistd.h> // fsync
#endif

namespace nkentseu {
	namespace ai {
		namespace gpt {

			// ---- Lecture d'un fichier entier en NkString (FILE* C, comme NKInfer) ---------
			static NkString ReadFileAll(const char *path) {
				FILE *f = fopen(path, "rb");
				NkString s;
				if (!f)
					return s;
				char buf[65536];
				size_t n;
				while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
					s.Append(buf, (NkString::SizeType)n);
				fclose(f);
				return s;
			}

			NkString LoadCorpus(const char *path, nk_size maxChars) {
				NkString all = ReadFileAll(path);
				NkString::SizeType start = 0;
				NkString::SizeType m = all.Find("*** START OF");
				if (m != NkString::npos) {
					NkString::SizeType nl = all.Find("\n", m);
					if (nl != NkString::npos)
						start = nl + 1;
				}
				NkString::SizeType end = all.Find("*** END OF");
				NkString::SizeType count = (end != NkString::npos && end > start) ? (end - start) : NkString::npos;
				NkString body = all.SubStr(start, count);
				if (body.Size() > (NkString::SizeType)maxChars)
					body = body.SubStr(0, (NkString::SizeType)maxChars);
				return body;
			}

			NkString LangOf(const NkString &path) {
				const char *p = path.Data();
				int64 n = (int64)path.Size();
				int64 base = 0;
				for (int64 i = 0; i < n; ++i)
					if (p[i] == '/' || p[i] == '\\')
						base = i + 1;
				for (int64 i = base; i < n; ++i) {
					if (p[i] == '_') {
						int64 len = i - base;
						if (len >= 1 && len <= 4)
							return path.SubStr((NkString::SizeType)base, (NkString::SizeType)len);
						break;
					}
				}
				return NkString("??");
			}

			void LoadCorpusByLang(const NkString &dir, nk_size totalCap, NkVector<NkString> &langs,
								  NkVector<NkString> &texts) {
				NkVector<NkString> files =
					NkDirectory::GetFiles(dir.CStr(), "*.txt", NkSearchOption::NK_TOP_DIRECTORY_ONLY);
				for (int64 i = 1; i < (int64)files.Size(); ++i)
					for (int64 j = i; j > 0; --j) {
						if (!(files[(nk_size)j] < files[(nk_size)(j - 1)]))
							break;
						NkString tmp = files[(nk_size)j];
						files[(nk_size)j] = files[(nk_size)(j - 1)];
						files[(nk_size)(j - 1)] = tmp;
					}
				if (files.Size() == 0)
					return;

				NkVector<NkVector<int64>> byLang;
				for (int64 fi = 0; fi < (int64)files.Size(); ++fi) {
					NkString lg = LangOf(files[(nk_size)fi]);
					int64 idx = -1;
					for (int64 k = 0; k < (int64)langs.Size(); ++k)
						if (langs[(nk_size)k] == lg) {
							idx = k;
							break;
						}
					if (idx < 0) {
						langs.PushBack(lg);
						byLang.PushBack(NkVector<int64>());
						texts.PushBack(NkString());
						idx = (int64)langs.Size() - 1;
					}
					byLang[(nk_size)idx].PushBack(fi);
				}

				const nk_size perLang = totalCap / (nk_size)langs.Size();
				for (int64 li = 0; li < (int64)langs.Size(); ++li) {
					const nk_size perFile = perLang / (nk_size)byLang[(nk_size)li].Size();
					for (int64 bi = 0; bi < (int64)byLang[(nk_size)li].Size(); ++bi) {
						const NkString &path = files[(nk_size)byLang[(nk_size)li][(nk_size)bi]];
						NkString body = LoadCorpus(path.CStr(), perFile);
						if (body.Size() < 200)
							continue;
						texts[(nk_size)li].Append(body);
						texts[(nk_size)li].Append("\n\n");
						logger.Info("  + [{0}] {1} {2} car.", langs[(nk_size)li].CStr(), path.CStr(),
									(unsigned long long)body.Size());
					}
					logger.Info("    => langue {0} : {1} car. (cible/langue {2})", langs[(nk_size)li].CStr(),
								(unsigned long long)texts[(nk_size)li].Size(), (unsigned long long)perLang);
				}
			}

			// ================= I64Map / BPE ================================================
			// Déplacés/généralisés dans NKData (data::NkI64Map / data::NkBpe / data::TrainBpe,
			// cf NKData/NkTokenizer.cpp) : aucune logique ci-dessous n'était spécifique GPT.
			// `gpt::I64Map`/`gpt::Bpe`/`gpt::TrainBpe` restent utilisables via les alias de
			// NkGptCore.h (compatibilité) mais l'implémentation vit désormais dans NKData.

			// ================= Checkpoint « NKGP » (v3 poids · v4 + état optimiseur) =======
			static int64 ShapeNumel(const NkShape &sh) {
				int64 n = 1;
				for (uint32 i = 0; i < sh.Size(); ++i)
					n *= sh[i];
				return n;
			}

			// Écrit un tenseur (rang + dims + floats) après passage sur CPU contigu.
			static bool WriteTensor(FILE *f, const NkTensor &src) {
				NkTensor v = src.ToCPU().Contiguous();
				const NkShape &sh = v.Shape();
				uint32 rank = sh.Size();
				bool ok = fwrite(&rank, sizeof(uint32), 1, f) == 1;
				for (uint32 dd = 0; ok && dd < rank; ++dd) {
					int64 dim = sh[dd];
					ok = fwrite(&dim, sizeof(int64), 1, f) == 1;
				}
				int64 numel = ShapeNumel(sh);
				const float *p = v.DataAs<float>();
				ok = ok && (numel == 0 || fwrite(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
				return ok;
			}

			// Lit un tenseur (rang + dims + floats) dans un NkTensor CPU.
			static bool ReadTensor(FILE *f, NkTensor &out) {
				uint32 rank = 0;
				if (fread(&rank, sizeof(uint32), 1, f) != 1 || rank > 8)
					return false;
				NkShape shape;
				for (uint32 dd = 0; dd < rank; ++dd) {
					int64 dim = 0;
					if (fread(&dim, sizeof(int64), 1, f) != 1)
						return false;
					shape.PushBack(dim);
				}
				int64 numel = ShapeNumel(shape);
				out = NkTensor::Zeros(shape);
				float *p = out.DataAs<float>();
				return numel == 0 || fread(p, sizeof(float), (size_t)numel, f) == (size_t)numel;
			}

			// Saute un tenseur (rang + dims + floats) sans le charger.
			static bool SkipTensor(FILE *f) {
				uint32 rank = 0;
				if (fread(&rank, sizeof(uint32), 1, f) != 1 || rank > 8)
					return false;
				int64 numel = 1;
				for (uint32 dd = 0; dd < rank; ++dd) {
					int64 dim = 0;
					if (fread(&dim, sizeof(int64), 1, f) != 1)
						return false;
					numel *= dim;
				}
				return fseek(f, (long)(numel * (int64)sizeof(float)), SEEK_CUR) == 0;
			}

			// Saute l'en-tête (magic + ver + dims + fusions + langues) et renvoie la version lue.
			// Positionne le curseur juste avant `count` (nombre de tenseurs de poids). 0 si erreur.
			static uint32 SkipHeader(FILE *f) {
				char magic[4];
				uint32 ver = 0;
				int32 hdr[5] = {0};
				if (fread(magic, 1, 4, f) != 4 || fread(&ver, sizeof(uint32), 1, f) != 1 ||
					fread(hdr, sizeof(int32), 5, f) != 5)
					return 0;
				int32 nMerges = 0;
				if (fread(&nMerges, sizeof(int32), 1, f) != 1 || nMerges < 0)
					return 0;
				if (nMerges > 0 && fseek(f, (long)nMerges * 2 * (long)sizeof(int32), SEEK_CUR) != 0)
					return 0;
				int32 nLang = 0;
				if (fread(&nLang, sizeof(int32), 1, f) != 1 || nLang < 0 || nLang > 64)
					return 0;
				for (int32 i = 0; i < nLang; ++i) {
					uint8 ln = 0;
					if (fread(&ln, 1, 1, f) != 1 || (ln != 0 && fseek(f, (long)ln, SEEK_CUR) != 0))
						return 0;
				}
				// v6 : deux octets d'architecture. Un fichier anterieur n'en a pas, et
				// sauter deux octets qui n'existent pas decalerait TOUT le reste de la
				// lecture -- d'ou le branchement sur la version, ici comme partout.
				if (ver >= 6u && fseek(f, 2, SEEK_CUR) != 0)
					return 0;
				return ver;
			}

			// -------------------------------------------------------------------------
			// ÉCRITURE SÛRE D'UN CHECKPOINT — pourquoi ce détour au lieu d'un fopen(w).
			//
			// Un checkpoint pèse des centaines de Mo et représente des JOURS de calcul.
			// Ouvrir directement le fichier de destination en écriture le TRONQUE avant
			// d'avoir écrit le premier octet : une coupure de courant pendant les
			// quelques secondes d'écriture détruit alors le nouveau checkpoint ET
			// l'ancien d'un seul coup. Cette machine a déjà coupé deux fois.
			//
			// On écrit donc dans un fichier temporaire, et on ne touche à la
			// destination qu'une fois l'écriture RÉUSSIE, par des renommages — des
			// opérations de métadonnées, quasi instantanées :
			//     <path>.tmp  --(écriture complète)-->  puis  <path> -> <path>.prev
			//                                            puis  <path>.tmp -> <path>
			// Il reste une fenêtre de quelques millisecondes entre les deux renommages
			// où `<path>` n'existe pas ; `<path>.prev` est alors intact et suffit à
			// reprendre. On passe d'un risque de tout perdre à un risque de perdre le
			// dernier intervalle de checkpoint.
			// -------------------------------------------------------------------------
			//
			// ROTATION A TROIS (2026-08-17, avant la campagne longue) : <path>,
			// <path>.prev, <path>.prev2. Deux exemplaires ne suffisent pas : si la
			// coupure survient pendant le renommage ET que .prev est lui-meme le
			// produit d'une ecriture douteuse, il ne reste rien. Trois exemplaires
			// separes par deux intervalles complets, c'est ce que demande la memoire
			// du projet (« un seul point de reprise n'est pas une sauvegarde »).
			//
			// DURABILITE : fflush() ne pousse que les tampons de la bibliotheque C
			// vers le systeme ; une coupure de COURANT peut encore perdre ce que le
			// systeme n'a pas ecrit sur le disque, et NTFS remplace alors la fin du
			// fichier par des zeros. On force donc l'ecriture physique (_commit /
			// fsync) AVANT le renommage : un checkpoint renomme est un checkpoint
			// sur le disque, pas en memoire.
			// -------------------------------------------------------------------------
			bool SaveCheckpoint(const char *path, const GptMeta &m, const NkVector<NkVar> &params,
								const NkVector<NkTensor> *optM, const NkVector<NkTensor> *optV, int64 step,
								uint64 rng) {
				NkString cheminTmp(path);
				cheminTmp.Append(".tmp");
				NkString cheminPrev(path);
				cheminPrev.Append(".prev");
				NkString cheminPrev2(path);
				cheminPrev2.Append(".prev2");

				FILE *f = fopen(cheminTmp.CStr(), "wb");
				if (!f)
					return false;
				const char magic[4] = {'N', 'K', 'G', 'P'};
				// v5 = v4 + queue {rng} : l'etat du flux aleatoire d'echantillonnage des
				// lots. Sans lui, une reprise repart du meme etat initial du flux et
				// RE-TIRE les memes fenetres que le debut de la course precedente.
				// v6 = v5 + deux octets d'ARCHITECTURE apres les langues. Le fichier dit
				// desormais lui-meme s'il faut construire un NkLlamaLM et si les tables
				// sont liees ; l'inference n'a plus a le deviner (cf. NkGptCore.h).
				uint32 ver = 6u;
				bool ok = fwrite(magic, 1, 4, f) == 4 && fwrite(&ver, sizeof(uint32), 1, f) == 1;
				int32 hdr[5] = {m.V, m.d, m.H, m.L, m.T};
				ok = ok && fwrite(hdr, sizeof(int32), 5, f) == 5;
				int32 nMerges = (int32)m.merges.Size();
				ok = ok && fwrite(&nMerges, sizeof(int32), 1, f) == 1;
				for (int32 i = 0; ok && i < nMerges; ++i) {
					int32 ab[2] = {m.merges[(nk_size)i].a, m.merges[(nk_size)i].b};
					ok = fwrite(ab, sizeof(int32), 2, f) == 2;
				}
				int32 nLang = (int32)m.langs.Size();
				ok = ok && fwrite(&nLang, sizeof(int32), 1, f) == 1;
				for (int32 i = 0; ok && i < nLang; ++i) {
					uint8 ln = (uint8)m.langs[(nk_size)i].Size();
					ok = ok && fwrite(&ln, 1, 1, f) == 1 &&
						 (ln == 0 || fwrite(m.langs[(nk_size)i].CStr(), 1, ln, f) == ln);
				}
				// Architecture (v6). Deux octets, ecrits meme quand ils valent 0 : un
				// champ present et faux se distingue d'un champ absent, et c'est
				// exactement la distinction qui manquait.
				{
					uint8 arch[2] = {(uint8)(m.architectureLlama ? 1 : 0), (uint8)(m.weightTying ? 1 : 0)};
					ok = ok && fwrite(arch, 1, 2, f) == 2;
				}
				uint32 count = params.Size();
				ok = ok && fwrite(&count, sizeof(uint32), 1, f) == 1;
				for (uint32 i = 0; ok && i < params.Size(); ++i)
					ok = ok && WriteTensor(f, params[i].Value());

				// Bloc optimiseur (v4) : hasOpt, puis {step, moments m/v} si présent.
				const bool hasOpt = optM != nullptr && optV != nullptr && optM->Size() == params.Size() &&
									 optV->Size() == params.Size();
				uint32 optFlag = hasOpt ? 1u : 0u;
				ok = ok && fwrite(&optFlag, sizeof(uint32), 1, f) == 1;
				if (ok && hasOpt) {
					ok = fwrite(&step, sizeof(int64), 1, f) == 1;
					for (uint32 i = 0; ok && i < optM->Size(); ++i)
						ok = ok && WriteTensor(f, (*optM)[i]);
					for (uint32 i = 0; ok && i < optV->Size(); ++i)
						ok = ok && WriteTensor(f, (*optV)[i]);
				}
				// Queue v5 : etat du flux aleatoire. Ecrit meme sans etat optimiseur ;
				// un lecteur v4 s'arrete avant et ne le voit pas.
				ok = ok && fwrite(&rng, sizeof(uint64), 1, f) == 1;

				// Vider les tampons AVANT de considérer l'écriture comme réussie : sans
				// cela, `ok` serait vrai alors que des centaines de Mo dorment encore en
				// mémoire, et on renommerait un fichier incomplet par-dessus le bon.
				ok = ok && (fflush(f) == 0);
				// ... puis forcer le disque (voir en-tete : une coupure de courant ne
				// respecte pas les tampons du systeme).
#if defined(_WIN32)
				ok = ok && (_commit(_fileno(f)) == 0);
#else
				ok = ok && (fsync(fileno(f)) == 0);
#endif
				fclose(f);

				if (!ok) {
					remove(cheminTmp.CStr()); // ne pas laisser trainer un fichier tronque
					logger.Info("Checkpoint NON ecrit : l'ecriture a echoue, l'ancien est INTACT ({0}).", path);
					return false;
				}

				// L'ecriture a reussi : on peut enfin toucher a la destination.
				// Rotation : prev -> prev2, path -> prev, tmp -> path.
				remove(cheminPrev2.CStr());					 // `rename` echoue si la cible existe
				rename(cheminPrev.CStr(), cheminPrev2.CStr()); // sans effet si .prev n'existe pas
				rename(path, cheminPrev.CStr());			 // sans effet si `path` n'existe pas encore
				if (rename(cheminTmp.CStr(), path) != 0) {
					// Cas rare (fichier verrouille) : remettre l'ancien en place plutot
					// que de laisser l'utilisateur sans aucun checkpoint.
					rename(cheminPrev.CStr(), path);
					rename(cheminPrev2.CStr(), cheminPrev.CStr());
					logger.Info("Checkpoint : renommage impossible vers {0}, l'ancien a ete remis en place.", path);
					return false;
				}
				return true;
			}

			// Un checkpoint est VALIDE s'il se lit jusqu'au bout : entete, fusions,
			// langues, tous les poids et — s'il en a — tout l'etat optimiseur. Un
			// fichier de la bonne taille rempli de zeros (NTFS apres coupure) ou coupe
			// net (mort du processus) echoue ici, et non trois heures plus tard.
			bool VerifierCheckpoint(const char *path, int64 *stepOut) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				char magic[4];
				uint32 ver = 0;
				bool ok = fread(magic, 1, 4, f) == 4 && magic[0] == 'N' && magic[1] == 'K' && magic[2] == 'G' &&
						  magic[3] == 'P' && fread(&ver, sizeof(uint32), 1, f) == 1 && (ver >= 3u && ver <= 6u);
				fclose(f);
				if (!ok)
					return false;
				f = fopen(path, "rb");
				if (!f)
					return false;
				ok = SkipHeader(f) == ver;
				uint32 count = 0;
				ok = ok && fread(&count, sizeof(uint32), 1, f) == 1 && count > 0 && count < 100000u;
				for (uint32 i = 0; ok && i < count; ++i)
					ok = SkipTensor(f);
				int64 step = 0;
				if (ok && ver >= 4u) {
					uint32 optFlag = 0;
					ok = fread(&optFlag, sizeof(uint32), 1, f) == 1;
					if (ok && optFlag != 0u) {
						ok = fread(&step, sizeof(int64), 1, f) == 1;
						for (uint32 i = 0; ok && i < 2 * count; ++i)
							ok = SkipTensor(f);
					}
				}
				if (ok && ver >= 5u) {
					uint64 rng = 0;
					ok = fread(&rng, sizeof(uint64), 1, f) == 1;
				}
				fclose(f);
				if (ok && stepOut)
					*stepOut = step;
				return ok;
			}

			// Choisit, parmi <path>, <path>.prev et <path>.prev2, le PLUS RECENT qui
			// soit valide. Rend le chemin retenu dans `retenu` (vide si aucun).
			bool ChoisirCheckpointValide(const char *path, NkString &retenu, int64 *stepOut) {
				const char *suffixes[3] = {"", ".prev", ".prev2"};
				for (int i = 0; i < 3; ++i) {
					NkString c(path);
					c.Append(suffixes[i]);
					int64 st = 0;
					if (VerifierCheckpoint(c.CStr(), &st)) {
						retenu = c;
						if (stepOut)
							*stepOut = st;
						if (i > 0)
							logger.Info("Checkpoint {0} illisible ou tronque : REPRISE SUR {1} (pas global {2}).", path,
										c.CStr(), (long long)st);
						return true;
					}
				}
				retenu = NkString();
				return false;
			}

			bool LoadCheckpointMeta(const char *path, GptMeta &m) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				char magic[4];
				uint32 ver = 0;
				int32 hdr[5] = {0};
				bool ok = fread(magic, 1, 4, f) == 4 && magic[0] == 'N' && magic[1] == 'K' && magic[2] == 'G' &&
						  magic[3] == 'P' && fread(&ver, sizeof(uint32), 1, f) == 1 && (ver >= 3u && ver <= 6u) &&
						  fread(hdr, sizeof(int32), 5, f) == 5;
				if (ok) {
					m.V = hdr[0];
					m.d = hdr[1];
					m.H = hdr[2];
					m.L = hdr[3];
					m.T = hdr[4];
				}
				if (ok) {
					int32 nMerges = 0;
					ok = fread(&nMerges, sizeof(int32), 1, f) == 1 && nMerges >= 0 && nMerges <= 200000;
					for (int32 i = 0; ok && i < nMerges; ++i) {
						int32 ab[2];
						ok = fread(ab, sizeof(int32), 2, f) == 2;
						if (ok) {
							NkMerge mg;
							mg.a = ab[0];
							mg.b = ab[1];
							m.merges.PushBack(mg);
						}
					}
				}
				if (ok) {
					int32 nLang = 0;
					ok = fread(&nLang, sizeof(int32), 1, f) == 1 && nLang >= 0 && nLang <= 64;
					for (int32 i = 0; ok && i < nLang; ++i) {
						uint8 ln = 0;
						char buf[256];
						ok = fread(&ln, 1, 1, f) == 1 && (ln == 0 || fread(buf, 1, ln, f) == ln);
						if (ok)
							m.langs.PushBack(NkString(buf, (NkString::SizeType)ln));
					}
				}
				// Architecture : v6 seulement. Sur un fichier anterieur, `architectureConnue`
				// reste false et l'appelant garde ce que la ligne de commande demande --
				// c'est la retro-compatibilite, et elle ne coute qu'un `if`.
				if (ok && ver >= 6u) {
					uint8 arch[2] = {0, 0};
					ok = fread(arch, 1, 2, f) == 2;
					if (ok) {
						m.architectureLlama = arch[0] != 0;
						m.weightTying = arch[1] != 0;
						m.architectureConnue = true;
					}
				}
				fclose(f);
				return ok;
			}

			bool LoadCheckpointWeights(const char *path, NkVector<NkVar> &params) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				char magic[4];
				uint32 ver = 0;
				int32 hdr[5] = {0};
				bool ok = fread(magic, 1, 4, f) == 4 && fread(&ver, sizeof(uint32), 1, f) == 1 &&
						  fread(hdr, sizeof(int32), 5, f) == 5;
				if (ok) {
					int32 nMerges = 0;
					ok = fread(&nMerges, sizeof(int32), 1, f) == 1 && nMerges >= 0;
					if (ok && nMerges > 0)
						ok = fseek(f, (long)nMerges * 2 * (long)sizeof(int32), SEEK_CUR) == 0;
				}
				if (ok) {
					int32 nLang = 0;
					ok = fread(&nLang, sizeof(int32), 1, f) == 1 && nLang >= 0 && nLang <= 64;
					for (int32 i = 0; ok && i < nLang; ++i) {
						uint8 ln = 0;
						ok = fread(&ln, 1, 1, f) == 1 && (ln == 0 || fseek(f, (long)ln, SEEK_CUR) == 0);
					}
				}
				if (ok && ver >= 6u) // deux octets d'architecture (v6) ; absents avant
					ok = fseek(f, 2, SEEK_CUR) == 0;
				uint32 count = 0;
				ok = ok && fread(&count, sizeof(uint32), 1, f) == 1 && count == params.Size();
				for (uint32 i = 0; ok && i < params.Size(); ++i) {
					uint32 rank = 0;
					ok = ok && fread(&rank, sizeof(uint32), 1, f) == 1;
					NkShape shape;
					for (uint32 dd = 0; ok && dd < rank; ++dd) {
						int64 dim = 0;
						ok = ok && fread(&dim, sizeof(int64), 1, f) == 1;
						shape.PushBack(dim);
					}
					int64 numel = ok ? ShapeNumel(shape) : 0;
					NkTensor t = NkTensor::Zeros(shape);
					float *p = t.DataAs<float>();
					ok = ok && (numel == 0 || fread(p, sizeof(float), (size_t)numel, f) == (size_t)numel);
					if (ok)
						params[i].SetValue(t);
				}
				fclose(f);
				return ok;
			}

			bool LoadCheckpointOptState(const char *path, NkVector<NkTensor> &optM, NkVector<NkTensor> &optV,
									   int64 &step, uint64 *rng) {
				FILE *f = fopen(path, "rb");
				if (!f)
					return false;
				uint32 ver = SkipHeader(f);
				if (ver < 4u) { // v3 : pas de bloc optimiseur
					fclose(f);
					return false;
				}
				uint32 count = 0;
				bool ok = fread(&count, sizeof(uint32), 1, f) == 1;
				for (uint32 i = 0; ok && i < count; ++i) // saute les poids
					ok = SkipTensor(f);
				uint32 optFlag = 0;
				ok = ok && fread(&optFlag, sizeof(uint32), 1, f) == 1;
				if (!ok || optFlag == 0u) { // fichier v4 sans état optimiseur
					fclose(f);
					return false;
				}
				ok = fread(&step, sizeof(int64), 1, f) == 1;
				optM.Clear();
				optV.Clear();
				for (uint32 i = 0; ok && i < count; ++i) {
					NkTensor t;
					ok = ReadTensor(f, t);
					if (ok)
						optM.PushBack(t);
				}
				for (uint32 i = 0; ok && i < count; ++i) {
					NkTensor t;
					ok = ReadTensor(f, t);
					if (ok)
						optV.PushBack(t);
				}
				// Queue v5 : etat du flux aleatoire. Absent en v4 : `rng` n'est pas touche,
				// l'appelant sait qu'il repart d'un etat par defaut et le dit.
				if (ok && ver >= 5u && rng) {
					uint64 r = 0;
					if (fread(&r, sizeof(uint64), 1, f) == 1)
						*rng = r;
				}
				fclose(f);
				return ok && optM.Size() == count && optV.Size() == count;
			}

		} // namespace gpt
	} // namespace ai
} // namespace nkentseu
