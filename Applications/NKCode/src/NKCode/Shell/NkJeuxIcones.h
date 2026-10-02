#pragma once
// =============================================================================
// NkJeuxIcones.h — LES JEUX D'ICONES, EXTENSIONS DE DONNEES de NKCode (01/10,
// choix de Rihen : « installables, facon extensions de VS Code »).
//
// UNE EXTENSION = UN DOSSIER <id>/ avec :
//   extension.cfg  le manifeste (cle = valeur, '#' commente) :
//                    id, nom, version, description, auteur
//                    contribue = jeuIcones     (le point de contribution)
//                    integre   = oui           (toujours installee : Pastilles)
//                    dossier   = ou sont les images (defaut : le dossier ;
//                                « @data/... » = le dossier data/ de NKCode)
//                    icons.cfg = la table extension -> icone (defaut ./icons.cfg)
//                    variantes = sombre, clair (sous-dossiers choisis par le theme)
//                    repli     = <id>          (un autre jeu, essaye avant Pastilles)
//                    rogner    = non           (garder les marges : grille respectee)
//                    mono      = A, B, ...     (icones BLANCHES a teinter par le theme)
//   icons.cfg      MEME format que data/icons.cfg (« .ext = NomIcone », plus « * »
//                  pour un fichier dont l'extension n'est pas listee)
//   <Nom>.svg / .png   une image par icone
//
// DEUX ENDROITS :
//   data/extensions/<id>/                 le CATALOGUE livre avec NKCode
//                                         (Pastilles y est INTEGRE ; Trait et Actuel
//                                         y attendent d'etre installes)
//   %APPDATA%\NKCode\extensions\<id>\     les extensions INSTALLEES (copiees depuis
//                                         le catalogue, ou deposees a la main)
//                                         (Linux/macOS : ~/.config/nkcode/extensions)
//
// L'ORDRE DE RECHERCHE d'une icone <Nom> (le premier trouve gagne) :
//   1. l'override utilisateur %APPDATA%\NKCode\icon\<Nom> (priorite GARDEE) ;
//   2. le jeu choisi (sa variante du theme, puis son dossier), puis ses replis ;
//   3. Pastilles, le jeu integre ;
//   4. les PNG de base (data/textures/icon) : toute l'interface y puise, une
//      icone absente d'un jeu n'est jamais un trou.
//
// ⚠️ CE FICHIER NE TOUCHE NI AU GPU NI A L'ECRAN : il lit des manifestes, copie
//    ou retire des dossiers et resout des chemins. C'est ce qui le rend
//    verifiable par le banc (NkBancApparences.h) sans fenetre.
// =============================================================================
#include "NKFileSystem/NkPath.h"
#include "NKFileSystem/NkFile.h"
#include "NKFileSystem/NkDirectory.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace nkcode {

		struct NkJeuIcones {
				NkString cle;			  ///< id (nom du dossier) : « trait »
				NkString titre, description, auteur, version;
				NkString dossier;		  ///< dossier de l'extension (barre finale)
				NkString dossierCatalogue; ///< sa source dans le catalogue (vide : deposee a la main)
				NkString images;		  ///< ou sont les images (barre finale)
				NkString iconsCfg;		  ///< chemin de sa table extension -> icone
				NkVector<NkString> variantes;
				NkString repli;			  ///< id du jeu de repli (vide : Pastilles)
				bool rogner = true;		  ///< rognage alpha au chargement
				bool integre = false;	  ///< toujours installee (Pastilles)
				bool installe = false;	  ///< utilisable (integree ou installee)
				NkVector<NkString> mono;  ///< icones blanches a teinter

				bool EstMono(const char *nom) const {
					for (usize i = 0; i < mono.Size(); ++i)
						if (mono[i] == NkString(nom))
							return true;
					return false;
				}
				bool AVariante(const char *v) const {
					for (usize i = 0; i < variantes.Size(); ++i)
						if (variantes[i] == NkString(v))
							return true;
					return false;
				}
		};

		/// Un maillon de la chaine de recherche : un dossier, et le jeu qui le porte
		/// (-1 = l'override utilisateur ou les PNG de base).
		struct NkMaillonIcones {
				NkString dossier; ///< barre finale
				int32 jeu = -1;
		};

		/// Ce qu'une resolution a trouve.
		struct NkIconeTrouvee {
				NkString chemin; ///< vide : introuvable partout
				int32 jeu = -1;	 ///< index du jeu qui l'a fournie (-1 : override ou base)
				bool rogner = true;
				bool mono = false;
		};

		inline NkString NkJeuxTrim(const NkString &s) {
			int32 a = 0, b = (int32)s.Length();
			const char *d = s.CStr();
			while (a < b && (d[a] == ' ' || d[a] == '\t'))
				++a;
			while (b > a && (d[b - 1] == ' ' || d[b - 1] == '\t' || d[b - 1] == '\r'))
				--b;
			return s.SubStr(a, b - a);
		}

		/// « a, b , c » -> {a, b, c}
		inline NkVector<NkString> NkJeuxListe(const NkString &v) {
			NkVector<NkString> out;
			NkString cur;
			for (const char *p = v.CStr();; ++p) {
				if (*p == ',' || *p == '\0') {
					const NkString t = NkJeuxTrim(cur);
					if (!t.Empty())
						out.PushBack(t);
					cur.Clear();
					if (*p == '\0')
						break;
				} else
					cur += *p;
			}
			return out;
		}

		/// Barre finale, separateurs '/'.
		inline NkString NkJeuxDossier(const NkString &s) {
			NkString r;
			for (usize i = 0; i < s.Size(); ++i)
				r += (s[i] == '\\') ? '/' : s[i];
			if (!r.Empty() && r[r.Size() - 1u] != '/')
				r += '/';
			return r;
		}

		class NkJeuxIcones {
			public:
				NkVector<NkJeuIcones> jeux; ///< catalogue + installees (une entree par id)
				NkString dossierBase;		 ///< data/textures/icon/ (repli final)
				NkString dossierOverride;	 ///< %APPDATA%/NKCode/icon/ (priorite)
				NkString dossierData;		 ///< data/ de NKCode (« @data/ »)
				NkString catalogue;			 ///< data/extensions/
				NkString installees;		 ///< %APPDATA%/NKCode/extensions/

				/// (Re)lit le catalogue puis les extensions installees. Une extension
				/// installee REMPLACE l'entree du catalogue de meme id.
				void Decouvrir() {
					jeux.Clear();
					Parcourir(catalogue, false);
					Parcourir(installees, true);
				}

				int32 Index(const char *cle) const {
					if (!cle)
						return -1;
					for (usize i = 0; i < jeux.Size(); ++i)
						if (jeux[i].cle == NkString(cle))
							return (int32)i;
					return -1;
				}

				const NkJeuIcones *Trouver(const char *cle) const {
					const int32 i = Index(cle);
					return i < 0 ? nullptr : &jeux[(usize)i];
				}

				/// L'id effectivement utilise : celui demande s'il est installe, sinon
				/// le jeu integre (Pastilles), sinon rien.
				NkString Effectif(const char *cle) const {
					const int32 i = Index(cle);
					if (i >= 0 && jeux[(usize)i].installe)
						return jeux[(usize)i].cle;
					for (usize k = 0; k < jeux.Size(); ++k)
						if (jeux[k].integre)
							return jeux[k].cle;
					return NkString();
				}

				/// Vrai si le jeu (ou un de ses replis) a des variantes de theme.
				bool DependDuTheme(const char *cle) const {
					const NkVector<int32> ch = Lignee(cle);
					for (usize k = 0; k < ch.Size(); ++k)
						if (!jeux[(usize)ch[k]].variantes.Empty())
							return true;
					return false;
				}

				/// La chaine de recherche du jeu `cle` pour la variante `variante`
				/// (« sombre » / « clair ») : override, le jeu et ses replis, le jeu
				/// integre, les PNG de base.
				NkVector<NkMaillonIcones> Chaine(const char *cle, const char *variante) const {
					NkVector<NkMaillonIcones> c;
					if (!dossierOverride.Empty())
						c.PushBack({dossierOverride, -1});
					const NkVector<int32> ch = Lignee(cle);
					for (usize k = 0; k < ch.Size(); ++k) {
						const NkJeuIcones &j = jeux[(usize)ch[k]];
						if (variante && *variante && j.AVariante(variante))
							c.PushBack({j.images + variante + "/", ch[k]});
						c.PushBack({j.images, ch[k]});
					}
					c.PushBack({dossierBase, -1});
					return c;
				}

				/// Ou est l'icone `nom` (sans extension) dans cette chaine : PNG puis SVG
				/// dans chaque maillon, dans l'ordre.
				NkIconeTrouvee Resoudre(const NkVector<NkMaillonIcones> &chaine, const char *nom) const {
					NkIconeTrouvee t;
					for (usize k = 0; k < chaine.Size(); ++k) {
						const NkMaillonIcones &m = chaine[k];
						if (m.dossier.Empty())
							continue;
						const char *ext[2] = {".png", ".svg"};
						for (const char *e : ext) {
							const NkString p = m.dossier + nom + e;
							if (NkFile::Exists(p.CStr())) {
								t.chemin = p;
								t.jeu = m.jeu;
								if (m.jeu >= 0) {
									t.rogner = jeux[(usize)m.jeu].rogner;
									t.mono = jeux[(usize)m.jeu].EstMono(nom);
								}
								return t;
							}
						}
					}
					return t;
				}

				/// Les tables extension -> icone a appliquer, DANS L'ORDRE (la derniere
				/// gagne) : du plus general (le jeu integre) au plus precis (le jeu).
				NkVector<NkString> TablesExtensions(const char *cle) const {
					const NkVector<int32> ch = Lignee(cle);
					NkVector<NkString> out;
					for (usize k = ch.Size(); k > 0; --k)
						if (!jeux[(usize)ch[k - 1]].iconsCfg.Empty())
							out.PushBack(jeux[(usize)ch[k - 1]].iconsCfg);
					return out;
				}

				/// INSTALLE l'extension `cle` du catalogue : copie de son dossier dans
				/// le dossier des extensions installees. Rend vrai si elle est installee.
				bool Installer(const char *cle) {
					const int32 i = Index(cle);
					if (i < 0)
						return false;
					NkJeuIcones &j = jeux[(usize)i];
					if (j.installe)
						return true;
					if (j.dossierCatalogue.Empty() || installees.Empty())
						return false;
					NkDirectory::CreateRecursive(installees.CStr());
					const NkString dst = installees + j.cle;
					NkDirectory::Copy(j.dossierCatalogue.CStr(), dst.CStr(), true, true);
					Decouvrir();
					const NkJeuIcones *n = Trouver(cle);
					return n && n->installe;
				}

				/// DESINSTALLE `cle` : retire son dossier des extensions installees. Une
				/// extension integree ne se desinstalle pas.
				bool Desinstaller(const char *cle) {
					const int32 i = Index(cle);
					if (i < 0 || jeux[(usize)i].integre)
						return false;
					const NkString dst = installees + jeux[(usize)i].cle;
					if (NkDirectory::Exists(dst.CStr()))
						NkDirectory::Delete(dst.CStr(), true);
					Decouvrir();
					const NkJeuIcones *n = Trouver(cle);
					return !n || !n->installe;
				}

			private:
				/// Le jeu, ses replis, puis le jeu integre (sans doublon, cycle borne).
				NkVector<int32> Lignee(const char *cle) const {
					NkVector<int32> ch;
					auto ajouter = [&](int32 i) {
						for (usize k = 0; k < ch.Size(); ++k)
							if (ch[k] == i)
								return false;
						ch.PushBack(i);
						return true;
					};
					int32 i = Index(Effectif(cle).CStr());
					for (int32 garde = 0; i >= 0 && garde < 8; ++garde) {
						if (!jeux[(usize)i].installe || !ajouter(i))
							break;
						i = Index(jeux[(usize)i].repli.CStr());
					}
					for (usize k = 0; k < jeux.Size(); ++k)
						if (jeux[k].integre)
							ajouter((int32)k);
					return ch;
				}

				NkString Resoudre(const NkString &base, const NkString &v) const {
					if (v.Empty())
						return base;
					if (v.StartsWith("@data/"))
						return dossierData + v.SubStr(6);
					if (NkPath(v.CStr()).IsAbsolute())
						return v;
					return base + v;
				}

				void Parcourir(const NkString &racine, bool utilisateur) {
					if (racine.Empty() || !NkDirectory::Exists(racine.CStr()))
						return;
					NkVector<NkString> sous = NkDirectory::GetDirectories(racine.CStr());
					for (usize s = 0; s < sous.Size(); ++s) {
						NkString chemin = sous[s];
						// GetDirectories peut rendre des NOMS ou des CHEMINS : on normalise.
						if (!NkPath(chemin.CStr()).IsAbsolute() && !NkDirectory::Exists(chemin.CStr()))
							chemin = racine + chemin;
						const NkString dossier = NkJeuxDossier(chemin);
						const NkString man = dossier + "extension.cfg";
						if (!NkFile::Exists(man.CStr()))
							continue;
						NkJeuIcones j;
						j.cle = NkPath(chemin.CStr()).GetFileName();
						j.dossier = dossier;
						j.images = dossier;
						if (!utilisateur)
							j.dossierCatalogue = dossier;
						const NkString cfg = dossier + "icons.cfg";
						if (NkFile::Exists(cfg.CStr()))
							j.iconsCfg = cfg;
						bool jeu = false;
						Lire(NkFile::ReadAllText(NkPath(man.CStr())), j, jeu);
						if (!jeu)
							continue; // une extension qui ne contribue pas de jeu d'icones
						if (j.titre.Empty())
							j.titre = j.cle;
						j.installe = utilisateur || j.integre;
						const int32 deja = Index(j.cle.CStr());
						if (deja >= 0) { // installee : remplace l'entree du catalogue
							j.dossierCatalogue = jeux[(usize)deja].dossierCatalogue;
							j.integre = j.integre || jeux[(usize)deja].integre;
							jeux[(usize)deja] = j;
						} else
							jeux.PushBack(j);
					}
				}

				void Lire(const NkString &texte, NkJeuIcones &j, bool &jeu) const {
					NkString ligne;
					for (const char *p = texte.CStr();; ++p) {
						if (*p == '\n' || *p == '\0') {
							const NkString l = NkJeuxTrim(ligne);
							ligne.Clear();
							if (!l.Empty() && l.CStr()[0] != '#') {
								int32 eq = -1;
								for (int32 k = 0; l.CStr()[k]; ++k)
									if (l.CStr()[k] == '=') {
										eq = k;
										break;
									}
								if (eq > 0) {
									const NkString k = NkJeuxTrim(l.SubStr(0, eq));
									const NkString v = NkJeuxTrim(l.SubStr(eq + 1, l.Length() - eq - 1));
									auto oui = [&]() {
										return !(v == NkString("non") || v == NkString("0") ||
												 v == NkString("false"));
									};
									if (k == NkString("id") && !v.Empty())
										j.cle = v;
									else if (k == NkString("nom") || k == NkString("titre"))
										j.titre = v;
									else if (k == NkString("description"))
										j.description = v;
									else if (k == NkString("auteur"))
										j.auteur = v;
									else if (k == NkString("version"))
										j.version = v;
									else if (k == NkString("contribue")) {
										const NkVector<NkString> c = NkJeuxListe(v);
										for (usize q = 0; q < c.Size(); ++q)
											if (c[q] == NkString("jeuIcones"))
												jeu = true;
									} else if (k == NkString("integre"))
										j.integre = oui();
									else if (k == NkString("dossier"))
										j.images = NkJeuxDossier(Resoudre(j.dossier, v));
									else if (k == NkString("icons.cfg"))
										j.iconsCfg = Resoudre(j.dossier, v);
									else if (k == NkString("variantes"))
										j.variantes = NkJeuxListe(v);
									else if (k == NkString("repli"))
										j.repli = v;
									else if (k == NkString("rogner"))
										j.rogner = oui();
									else if (k == NkString("mono"))
										j.mono = NkJeuxListe(v);
								}
							}
							if (*p == '\0')
								break;
						} else
							ligne += *p;
					}
				}
		};

		/// Le registre UNIQUE des jeux de NKCode (decouvert au chargement des icones).
		inline NkJeuxIcones &NkCodeJeuxIcones() {
			static NkJeuxIcones r;
			return r;
		}

	} // namespace nkcode
} // namespace nkentseu
