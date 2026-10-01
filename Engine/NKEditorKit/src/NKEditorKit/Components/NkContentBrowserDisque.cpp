// -----------------------------------------------------------------------------
// @File    NkContentBrowserDisque.cpp
// @Brief   Les gestes du navigateur de contenu sur le disque (voir l'en-tete).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NKEditorKit/Components/NkContentBrowserDisque.h"

#include "NKFileSystem/NkDirectory.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkPath.h"

namespace nkentseu {
	namespace editorkit {

		namespace {

			/// La barre oblique inverse, ecrite par son code (un chemin se compare
			/// aux deux separateurs).
			constexpr char ANTISLASH = 92;

			bool Sep(char c) {
				return c == '/' || c == ANTISLASH;
			}

			char Bas(char c) {
				return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			bool Existe(const NkString &p) {
				return NkFile::Exists(p.CStr()) || NkDirectory::Exists(p.CStr());
			}

			/// Un chemin ABSOLU ? (« C:... », « /... », « \\... »)
			bool EstAbsolu(const char *p) {
				if (!p || !p[0])
					return false;
				if (Sep(p[0]))
					return true;
				return ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':';
			}

			/// Le chemin rendu absolu (repertoire courant), puis normalise.
			NkString Absolu(const char *p) {
				if (EstAbsolu(p))
					return NkDisqueNormaliser(p);
				NkString base = NkDirectory::GetCurrentDirectory().ToString();
				base.Append('/');
				base.Append(p ? p : "");
				return NkDisqueNormaliser(base.CStr());
			}

			/// `a/b`, sans doubler le separateur.
			NkString Joindre(const NkString &a, const NkString &b) {
				NkString r = a;
				if (!r.Empty() && !Sep(r.CStr()[r.Length() - 1u]))
					r.Append('/');
				r.Append(b);
				return r;
			}

			void Dire(NkString *raison, const char *texte) {
				if (raison)
					*raison = NkString(texte);
			}

			/// La source est-elle un geste legal : sous la racine, existante, et pas
			/// la racine elle-meme ?
			bool SourceLegale(const char *racine, const char *source, NkString *raison) {
				if (!source || !source[0] || !NkDisqueSous(racine, source)) {
					Dire(raison, "hors du contenu du projet");
					return false;
				}
				if (NkDisqueSous(source, racine)) {
					Dire(raison, "la racine du contenu ne se deplace pas");
					return false;
				}
				if (!Existe(NkString(source))) {
					Dire(raison, "introuvable sur le disque");
					return false;
				}
				return true;
			}

			/// Copie recursive, sans jamais ecraser (le nom de tete est deja libre).
			/// Les entrees cachees (« .xxx ») ne voyagent pas : ce sont des
			/// metadonnees d'outil, pas du contenu.
			bool CopierArbre(const NkString &src, const NkString &dst, NkDisqueRapport *r,
							 bool (*accepte)(void *, const char *), void *user, uint32 profondeur) {
				if (profondeur > 64u)
					return false; // un lien circulaire ne doit pas faire tourner sans fin
				if (NkDirectory::Exists(src.CStr())) {
					if (!NkDirectory::Exists(dst.CStr()) && !NkDirectory::CreateRecursive(dst.CStr())) {
						if (r)
							++r->echecs;
						return false;
					}
					if (r)
						++r->dossiers;
					bool ok = true;
					NkVector<NkDirectoryEntry> ent = NkDirectory::GetEntries(src.CStr());
					for (uint32 i = 0; i < ent.Size(); ++i) {
						const NkDirectoryEntry &e = ent[i];
						if (e.IsHidden || e.Name.Empty() || e.Name.CStr()[0] == '.')
							continue;
						if (!e.IsDirectory && !e.IsFile)
							continue;
						const NkString s = Joindre(src, e.Name);
						const NkString d = NkDisqueCheminLibre(dst.CStr(), e.Name.CStr());
						if (d.Empty()) {
							if (r)
								++r->echecs;
							ok = false;
							continue;
						}
						ok = CopierArbre(s, d, r, accepte, user, profondeur + 1u) && ok;
					}
					return ok;
				}
				if (!NkFile::Exists(src.CStr())) {
					if (r)
						++r->echecs;
					return false;
				}
				if (accepte && !accepte(user, src.CStr())) {
					if (r) {
						++r->refuses;
						if (r->premierRefus.Empty())
							r->premierRefus = NkDisqueNom(src.CStr());
					}
					return true; // un refus n'est pas un echec
				}
				if (!NkFile::Copy(src.CStr(), dst.CStr(), false)) {
					if (r)
						++r->echecs;
					return false;
				}
				if (r)
					++r->fichiers;
				return true;
			}

			uint32 LireHex(const char *s) {
				uint32 v = 0;
				for (; s && *s; ++s) {
					const char c = *s;
					uint32 d = 0;
					if (c >= '0' && c <= '9')
						d = (uint32)(c - '0');
					else if (c >= 'a' && c <= 'f')
						d = (uint32)(c - 'a' + 10);
					else if (c >= 'A' && c <= 'F')
						d = (uint32)(c - 'A' + 10);
					else
						break;
					v = (v << 4) | d;
				}
				return v;
			}

			/// `rel` est-il `cle` ou sous `cle` ?
			bool SousCle(const NkString &cle, const NkString &rel) {
				const usize n = cle.Length();
				if (rel.Length() < n)
					return false;
				for (usize i = 0; i < n; ++i)
					if (Bas(cle.CStr()[i]) != Bas(rel.CStr()[i]))
						return false;
				return rel.Length() == n || n == 0 || rel.CStr()[n] == '/';
			}

			NkString Rebaser(const NkString &rel, const NkString &ancien, const NkString &nouveau) {
				NkString r = nouveau;
				r.Append(rel.CStr() + ancien.Length());
				return r;
			}

		} // namespace

		// ── LES CHEMINS ─────────────────────────────────────────────────────────
		NkString NkDisqueNormaliser(const char *chemin) {
			if (!chemin)
				return NkString();
			// Prefixe conserve tel quel : « C: », ou le « / » d'un chemin absolu.
			NkString tete;
			const char *p = chemin;
			if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':') {
				tete.Append(p[0]);
				tete.Append(':');
				p += 2;
			}
			const bool absolu = Sep(*p);
			// Les segments, « .. » resolus contre la pile.
			NkVector<NkString> pile;
			while (*p) {
				while (Sep(*p))
					++p;
				const char *f = p;
				while (*f && !Sep(*f))
					++f;
				const usize n = (usize)(f - p);
				if (n == 1 && p[0] == '.') {
					// rien
				} else if (n == 2 && p[0] == '.' && p[1] == '.') {
					if (!pile.Empty() && !(pile[pile.Size() - 1u] == NkString("..")))
						pile.PopBack();
					else if (!absolu && tete.Empty())
						pile.PushBack(NkString("..")); // un relatif peut remonter
				} else if (n > 0) {
					pile.PushBack(NkString(p, n));
				}
				p = f;
			}
			NkString r = tete;
			if (absolu)
				r.Append('/');
			for (uint32 i = 0; i < pile.Size(); ++i) {
				if (i > 0)
					r.Append('/');
				r.Append(pile[i]);
			}
			return r;
		}

		bool NkDisqueSous(const char *racine, const char *chemin) {
			if (!racine || !chemin || !racine[0] || !chemin[0])
				return false;
			const NkString a = Absolu(racine);
			const NkString b = Absolu(chemin);
			const usize n = a.Length();
			if (b.Length() < n)
				return false;
			for (usize i = 0; i < n; ++i)
				if (Bas(a.CStr()[i]) != Bas(b.CStr()[i]))
					return false;
			// « Contenu2 » n'est pas sous « Contenu » : la frontiere est un separateur.
			return b.Length() == n || b.CStr()[n] == '/' || (n > 0 && a.CStr()[n - 1u] == '/');
		}

		NkString NkDisqueNom(const char *chemin) {
			if (!chemin)
				return NkString();
			const NkString n = NkDisqueNormaliser(chemin);
			const char *d = n.CStr();
			for (const char *q = n.CStr(); *q; ++q)
				if (Sep(*q))
					d = q + 1;
			return NkString(d);
		}

		NkString NkDisqueParent(const char *chemin) {
			if (!chemin)
				return NkString();
			const NkString n = NkDisqueNormaliser(chemin);
			usize coupe = 0;
			bool trouve = false;
			for (usize i = 0; i < n.Length(); ++i)
				if (Sep(n.CStr()[i])) {
					coupe = i;
					trouve = true;
				}
			if (!trouve)
				return NkString();
			if (coupe == 0)
				return NkString("/");
			return NkString(n.CStr(), coupe);
		}

		bool NkDisqueNomValide(const char *nom) {
			if (!nom || !nom[0])
				return false;
			usize n = 0;
			for (const char *q = nom; *q; ++q, ++n) {
				const char c = *q;
				if (Sep(c) || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
					return false;
				if ((unsigned char)c < 32u)
					return false;
			}
			if ((n == 1 && nom[0] == '.') || (n == 2 && nom[0] == '.' && nom[1] == '.'))
				return false;
			const char fin = nom[n - 1u];
			return fin != ' ' && fin != '.';
		}

		NkString NkDisqueCheminLibre(const char *dossier, const char *nom) {
			const NkString d(dossier ? dossier : "");
			const NkString nm(nom ? nom : "");
			NkString chemin = Joindre(d, nm);
			if (!Existe(chemin))
				return chemin;
			// Le PIED et l'extension : « caisse.png » -> « caisse » + « png ». Un nom
			// qui commence par un point n'a pas d'extension (« .gitkeep »).
			const char *ext = nullptr;
			for (const char *q = nm.CStr(); *q; ++q)
				if (*q == '.' && q != nm.CStr())
					ext = q;
			const usize lPied = ext ? (usize)(ext - nm.CStr()) : nm.Length();
			const NkString pied(nm.CStr(), lPied);
			for (int32 k = 2; k < 10000; ++k) {
				NkString n = pied;
				n.Append(NkString::Format("_%d", k).CStr());
				if (ext)
					n.Append(ext);
				chemin = Joindre(d, n);
				if (!Existe(chemin))
					return chemin;
			}
			return NkString();
		}

		// ── LES GESTES ──────────────────────────────────────────────────────────
		NkString NkDisqueNouveauDossier(const char *racine, const char *dossier, const char *nomVoulu,
										NkString *raison) {
			if (!dossier || !NkDisqueSous(racine, dossier)) {
				Dire(raison, "hors du contenu du projet");
				return NkString();
			}
			const char *nom = (nomVoulu && nomVoulu[0]) ? nomVoulu : "Nouveau dossier";
			if (!NkDisqueNomValide(nom)) {
				Dire(raison, "nom de dossier invalide");
				return NkString();
			}
			NkDirectory::CreateRecursive(dossier);
			const NkString chemin = NkDisqueCheminLibre(dossier, nom);
			if (chemin.Empty() || !NkDirectory::CreateRecursive(chemin.CStr())) {
				Dire(raison, "creation impossible");
				return NkString();
			}
			return chemin;
		}

		NkString NkDisqueCopier(const char *racine, const char *source, const char *dossierCible, NkString *raison) {
			if (!SourceLegale(racine, source, raison))
				return NkString();
			if (!dossierCible || !NkDisqueSous(racine, dossierCible) || !NkDirectory::Exists(dossierCible)) {
				Dire(raison, "la destination n'est pas un dossier du contenu");
				return NkString();
			}
			// Un dossier dans lui-meme : la copie se nourrirait d'elle-meme.
			if (NkDirectory::Exists(source) && NkDisqueSous(source, dossierCible)) {
				Dire(raison, "un dossier ne se copie pas dans lui-meme");
				return NkString();
			}
			const NkString dst = NkDisqueCheminLibre(dossierCible, NkDisqueNom(source).CStr());
			if (dst.Empty() || !CopierArbre(NkString(source), dst, nullptr, nullptr, nullptr, 0u)) {
				Dire(raison, "copie impossible");
				return dst.Empty() || !Existe(dst) ? NkString() : dst;
			}
			return dst;
		}

		NkString NkDisqueDeplacer(const char *racine, const char *source, const char *dossierCible, NkString *raison) {
			if (!SourceLegale(racine, source, raison))
				return NkString();
			if (!dossierCible || !NkDisqueSous(racine, dossierCible) || !NkDirectory::Exists(dossierCible)) {
				Dire(raison, "la destination n'est pas un dossier du contenu");
				return NkString();
			}
			if (NkDirectory::Exists(source) && NkDisqueSous(source, dossierCible)) {
				Dire(raison, "un dossier ne se deplace pas dans lui-meme");
				return NkString();
			}
			// Deja la : rien a faire, et surtout pas « caisse_2.png ».
			const NkString parent = NkDisqueParent(source);
			if (NkDisqueSous(parent.CStr(), dossierCible) && NkDisqueSous(dossierCible, parent.CStr()))
				return NkString(source);
			const NkString dst = NkDisqueCheminLibre(dossierCible, NkDisqueNom(source).CStr());
			if (dst.Empty()) {
				Dire(raison, "aucun nom libre");
				return NkString();
			}
			const bool dossier = NkDirectory::Exists(source);
			const bool ok = dossier ? NkDirectory::Move(source, dst.CStr()) : NkFile::Move(source, dst.CStr());
			if (!ok) {
				// Le repli : copier puis effacer -- seulement si la copie est COMPLETE.
				if (CopierArbre(NkString(source), dst, nullptr, nullptr, nullptr, 0u)) {
					if (dossier)
						NkDirectory::Delete(source, true);
					else
						NkFile::Delete(source);
					return dst;
				}
				Dire(raison, "deplacement impossible");
				return NkString();
			}
			return dst;
		}

		NkString NkDisqueDupliquer(const char *racine, const char *source, NkString *raison) {
			if (!SourceLegale(racine, source, raison))
				return NkString();
			const NkString parent = NkDisqueParent(source);
			const NkString dst = NkDisqueCheminLibre(parent.CStr(), NkDisqueNom(source).CStr());
			if (dst.Empty() || !CopierArbre(NkString(source), dst, nullptr, nullptr, nullptr, 0u)) {
				Dire(raison, "duplication impossible");
				return NkString();
			}
			return dst;
		}

		NkString NkDisqueRenommer(const char *racine, const char *source, const char *nouveauNom, NkString *raison) {
			if (!SourceLegale(racine, source, raison))
				return NkString();
			if (!NkDisqueNomValide(nouveauNom)) {
				Dire(raison, "nom invalide (ni / \\ : * ? \" < > |)");
				return NkString();
			}
			const NkString parent = NkDisqueParent(source);
			const NkString dst = Joindre(parent, NkString(nouveauNom));
			if (NkDisqueNormaliser(source) == NkDisqueNormaliser(dst.CStr()))
				return NkString(source); // meme nom : rien a faire
			// ⚠️ « Caisse.png » -> « caisse.png » : sous Windows c'est LE MEME fichier.
			//    Le refuser comme « nom pris » serait faux ; on passe par un nom
			//    intermediaire, que l'OS accepte.
			const bool memeSaufCasse = NkDisqueSous(source, dst.CStr()) && NkDisqueSous(dst.CStr(), source);
			if (!memeSaufCasse && Existe(dst)) {
				Dire(raison, "ce nom est deja pris");
				return NkString();
			}
			const bool dossier = NkDirectory::Exists(source);
			bool ok;
			if (memeSaufCasse) {
				const NkString tmp = NkDisqueCheminLibre(parent.CStr(), "~nkrenommage");
				ok = (dossier ? NkDirectory::Move(source, tmp.CStr()) : NkFile::Move(source, tmp.CStr())) &&
					 (dossier ? NkDirectory::Move(tmp.CStr(), dst.CStr()) : NkFile::Move(tmp.CStr(), dst.CStr()));
			} else {
				ok = dossier ? NkDirectory::Move(source, dst.CStr()) : NkFile::Move(source, dst.CStr());
			}
			if (!ok) {
				Dire(raison, "renommage impossible");
				return NkString();
			}
			return dst;
		}

		bool NkDisqueSupprimer(const char *racine, const char *chemin, bool corbeille, NkString *raison) {
			if (!SourceLegale(racine, chemin, raison))
				return false;
			const bool dossier = NkDirectory::Exists(chemin);
			bool ok;
			if (corbeille)
				ok = dossier ? NkDirectory::MoveToTrash(chemin) : NkFile::MoveToTrash(chemin);
			else
				ok = dossier ? NkDirectory::Delete(chemin, true) : NkFile::Delete(chemin);
			if (!ok)
				Dire(raison, corbeille ? "la corbeille a refuse" : "suppression impossible");
			return ok;
		}

		NkDisqueRapport NkDisqueImporter(const char *racine, const char *dossierCible, const NkVector<NkString> &sources,
										 bool (*accepte)(void *, const char *), void *user) {
			NkDisqueRapport r;
			if (!dossierCible || !NkDisqueSous(racine, dossierCible)) {
				r.echecs = (uint32)sources.Size();
				return r;
			}
			NkDirectory::CreateRecursive(dossierCible);
			for (uint32 i = 0; i < sources.Size(); ++i) {
				const NkString src = NkDisqueNormaliser(sources[i].CStr());
				if (!Existe(src)) {
					++r.echecs;
					continue;
				}
				// Importer le contenu DANS lui-meme (deposer « Contenu » sur
				// « Contenu/Textures ») : la copie se nourrirait d'elle-meme.
				if (NkDirectory::Exists(src.CStr()) && NkDisqueSous(src.CStr(), dossierCible)) {
					++r.refuses;
					if (r.premierRefus.Empty())
						r.premierRefus = NkDisqueNom(src.CStr());
					continue;
				}
				// Un FICHIER refuse ne cree rien ; un DOSSIER est cree meme vide.
				if (!NkDirectory::Exists(src.CStr()) && accepte && !accepte(user, src.CStr())) {
					++r.refuses;
					if (r.premierRefus.Empty())
						r.premierRefus = NkDisqueNom(src.CStr());
					continue;
				}
				const NkString dst = NkDisqueCheminLibre(dossierCible, NkDisqueNom(src.CStr()).CStr());
				if (dst.Empty()) {
					++r.echecs;
					continue;
				}
				CopierArbre(src, dst, &r, accepte, user, 0u);
				if (Existe(dst))
					r.crees.PushBack(dst);
			}
			return r;
		}

		// ── LA MEMOIRE ──────────────────────────────────────────────────────────
		uint32 NkDisqueMeta::Couleur(const NkString &rel) const {
			for (uint32 i = 0; i < couleursCles.Size(); ++i)
				if (couleursCles[i] == rel)
					return couleurs[i];
			return 0u;
		}

		void NkDisqueMeta::PoserCouleur(const NkString &rel, uint32 rgba) {
			for (uint32 i = 0; i < couleursCles.Size(); ++i) {
				if (couleursCles[i] == rel) {
					if (rgba == 0u) {
						couleursCles.RemoveAt(i);
						couleurs.RemoveAt(i);
					} else {
						couleurs[i] = rgba;
					}
					return;
				}
			}
			if (rgba != 0u) {
				couleursCles.PushBack(rel);
				couleurs.PushBack(rgba);
			}
		}

		bool NkDisqueMeta::EstFavori(const NkString &rel) const {
			for (uint32 i = 0; i < favoris.Size(); ++i)
				if (favoris[i] == rel)
					return true;
			return false;
		}

		void NkDisqueMeta::BasculerFavori(const NkString &rel) {
			for (uint32 i = 0; i < favoris.Size(); ++i) {
				if (favoris[i] == rel) {
					favoris.RemoveAt(i);
					return;
				}
			}
			favoris.PushBack(rel);
		}

		void NkDisqueMeta::Renommer(const NkString &ancien, const NkString &nouveau) {
			for (uint32 i = 0; i < couleursCles.Size(); ++i)
				if (SousCle(ancien, couleursCles[i]))
					couleursCles[i] = Rebaser(couleursCles[i], ancien, nouveau);
			for (uint32 i = 0; i < favoris.Size(); ++i)
				if (SousCle(ancien, favoris[i]))
					favoris[i] = Rebaser(favoris[i], ancien, nouveau);
			for (uint32 c = 0; c < collections.Size(); ++c)
				for (uint32 i = 0; i < collections[c].elements.Size(); ++i)
					if (SousCle(ancien, collections[c].elements[i]))
						collections[c].elements[i] = Rebaser(collections[c].elements[i], ancien, nouveau);
		}

		void NkDisqueMeta::Oublier(const NkString &rel) {
			for (uint32 i = (uint32)couleursCles.Size(); i > 0; --i)
				if (SousCle(rel, couleursCles[i - 1u])) {
					couleursCles.RemoveAt(i - 1u);
					couleurs.RemoveAt(i - 1u);
				}
			for (uint32 i = (uint32)favoris.Size(); i > 0; --i)
				if (SousCle(rel, favoris[i - 1u]))
					favoris.RemoveAt(i - 1u);
			for (uint32 c = 0; c < collections.Size(); ++c)
				for (uint32 i = (uint32)collections[c].elements.Size(); i > 0; --i)
					if (SousCle(rel, collections[c].elements[i - 1u]))
						collections[c].elements.RemoveAt(i - 1u);
		}

		bool NkDisqueMetaLire(const char *racine, NkDisqueMeta &meta) {
			meta = NkDisqueMeta();
			const NkString chemin = Joindre(NkString(racine ? racine : ""), NkString(NK_DISQUE_META));
			if (!NkFile::Exists(chemin.CStr()))
				return true;
			const NkString texte = NkFile::ReadAllText(chemin.CStr());
			// Une ligne = des champs separes par des TABULATIONS (un nom de dossier
			// peut porter des espaces, pas de tabulation).
			const char *p = texte.CStr();
			while (*p) {
				const char *f = p;
				while (*f && *f != '\n')
					++f;
				NkVector<NkString> champs;
				const char *c = p;
				while (c < f) {
					const char *d = c;
					while (d < f && *d != '\t' && *d != '\r')
						++d;
					champs.PushBack(NkString(c, (usize)(d - c)));
					c = (d < f && *d == '\t') ? d + 1 : f;
				}
				if (champs.Size() >= 3u && champs[0] == NkString("couleur")) {
					meta.couleursCles.PushBack(champs[1]);
					meta.couleurs.PushBack(LireHex(champs[2].CStr()));
				} else if (champs.Size() >= 2u && champs[0] == NkString("favori")) {
					meta.favoris.PushBack(champs[1]);
				} else if (champs.Size() >= 3u && champs[0] == NkString("collection")) {
					NkDisqueCollection col;
					col.nom = champs[1];
					col.couleur = LireHex(champs[2].CStr());
					meta.collections.PushBack(col);
				} else if (champs.Size() >= 3u && champs[0] == NkString("element")) {
					for (uint32 k = 0; k < meta.collections.Size(); ++k)
						if (meta.collections[k].nom == champs[1])
							meta.collections[k].elements.PushBack(champs[2]);
				}
				p = *f ? f + 1 : f;
			}
			return true;
		}

		bool NkDisqueMetaEcrire(const char *racine, const NkDisqueMeta &meta) {
			if (!racine || !racine[0])
				return false;
			NkDirectory::CreateRecursive(racine);
			NkString t("# NkNavigateur 1 -- couleurs de dossiers, favoris, collections\n");
			for (uint32 i = 0; i < meta.couleursCles.Size(); ++i)
				t.Append(NkString::Format("couleur\t%s\t%08X\n", meta.couleursCles[i].CStr(), meta.couleurs[i]).CStr());
			for (uint32 i = 0; i < meta.favoris.Size(); ++i)
				t.Append(NkString::Format("favori\t%s\n", meta.favoris[i].CStr()).CStr());
			for (uint32 c = 0; c < meta.collections.Size(); ++c) {
				const NkDisqueCollection &col = meta.collections[c];
				t.Append(NkString::Format("collection\t%s\t%08X\n", col.nom.CStr(), col.couleur).CStr());
				for (uint32 i = 0; i < col.elements.Size(); ++i)
					t.Append(NkString::Format("element\t%s\t%s\n", col.nom.CStr(), col.elements[i].CStr()).CStr());
			}
			const NkString chemin = Joindre(NkString(racine), NkString(NK_DISQUE_META));
			return NkFile::WriteAllText(chemin.CStr(), t.CStr());
		}

	} // namespace editorkit
} // namespace nkentseu
