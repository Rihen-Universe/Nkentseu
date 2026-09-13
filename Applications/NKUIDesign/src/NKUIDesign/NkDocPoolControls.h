// =============================================================================
// NKUIDesign/NkDocPoolControls.h
// Les controles du pool de chaines du document (`--pool-controles`).
//
// Application console, comme `--roundtrip-controles` : ne touche ni au GPU ni a
// l'ecran, donc tourne sur une machine d'integration. Meme raison qu'ailleurs
// dans ce depot : un banc pose sous `tests/` ne s'execute jamais.
//
// Auteur : Rihen. License : Proprietary - All Rights Reserved.
// =============================================================================

#pragma once

#ifndef NKUIDESIGN_NKDOCPOOLCONTROLS_H
#define NKUIDESIGN_NKDOCPOOLCONTROLS_H

#include <cstdio>

#include "Document.h"
#include "Layout.h"
#include "NkDocStringPool.h"

namespace nkuidesign {
		namespace poolctl {

			static int s_pass = 0;
			static int s_fail = 0;

			inline void Check(bool ok, const char *what) {
				if (ok) {
					++s_pass;
				} else {
					++s_fail;
					printf("  FAIL %s\n", what);
				}
			}

			inline bool SameText(const char *a, const char *b) {
				if (!a || !b) {
					return false;
				}
				while (*a && *b) {
					if (*a != *b) {
						return false;
					}
					++a;
					++b;
				}
				return *a == *b;
			}

			// -----------------------------------------------------------------
			// P1 -- STABILITE DES ADRESSES : le controle qui compte vraiment
			// -----------------------------------------------------------------
			// Un pool naif (NkVector<NkString>) RELOGE ses elements en
			// grandissant, et tous les pointeurs deja distribues deviennent
			// pendouillants. Le document ne se corromprait pas a la premiere
			// metrique mais a la Nieme -- le pire des calendriers.
			//
			// On interne donc BEAUCOUP, en gardant les tout premiers pointeurs,
			// et on verifie qu'ils disent encore la verite a la fin.
			inline void P1_AdressesStables() {
				printf("[P1] Les adresses distribuees survivent a la croissance du pool\n");

				NkDocStringPool pool;

				const char *premier = pool.Intern("largeur_palette");
				const char *second = pool.Intern("gouttiere_ligne");

				// Assez pour forcer plusieurs reallocations du vecteur interne.
				for (int i = 0; i < 512; ++i) {
					char buf[32];
					snprintf(buf, sizeof(buf), "metrique_%d", i);
					const char *p = pool.Intern(buf);
					Check(SameText(p, buf), "une entree fraiche doit se relire");
				}

				// LE point : les deux premiers pointeurs, distribues avant des
				// centaines de reallocations, doivent etre intacts.
				Check(SameText(premier, "largeur_palette"), "P1 premier pointeur survit");
				Check(SameText(second, "gouttiere_ligne"), "P1 second pointeur survit");
				Check(pool.Size() == 514, "P1 compte des entrees possedees");
			}

			// -----------------------------------------------------------------
			// P2 -- IDENTITE : PAS de deduplication, et c'est un CONTRAT
			// -----------------------------------------------------------------
			// Dedupliquer ferait marcher la comparaison d'adresse comme test
			// d'egalite de nom -- mais seulement A L'INTERIEUR d'un document.
			// Deux documents, une copie, un import, et elle redevient fausse. On
			// refuse donc l'egalite d'adresse PARTOUT, pour que le premier qui
			// l'essaie echoue tout de suite plutot que trois mois plus tard.
			inline void P2_PasDeDeduplication() {
				printf("[P2] Deux fois le meme nom -> deux adresses DIFFERENTES (contrat)\n");

				NkDocStringPool pool;
				const char *a = pool.Intern("largeur_palette");
				const char *b = pool.Intern("largeur_palette");

				Check(a != b, "P2 l'egalite d'adresse ne doit JAMAIS marcher");
				Check(SameText(a, b), "P2 le CONTENU, lui, est bien le meme");
				Check(pool.Size() == 2, "P2 deux entrees, pas une");
			}

			// -----------------------------------------------------------------
			// P3 -- LE CAS LEGITIME : un document SANS metrique
			// -----------------------------------------------------------------
			// La contre-epreuve demandee. Le defaut du kit est la chaine vide ;
			// un document qui ne nomme aucune metrique est parfaitement valide et
			// ne doit RIEN couter. C'est le pendant exact du temoin de
			// non-regression pose sur la correction (c).
			inline void P3_SansMetriqueResteValide() {
				printf("[P3] Un document SANS metrique reste valide et ne coute rien\n");

				NkDocStringPool pool;
				const char *vide = pool.Intern("");
				const char *nul = pool.Intern(static_cast<const char *>(nullptr));

				Check(vide != nullptr, "P3 jamais nullptr");
				Check(nul != nullptr, "P3 jamais nullptr, meme depuis nullptr");
				Check(*vide == 0, "P3 la chaine vide reste vide");
				Check(pool.Size() == 0, "P3 aucune allocation pour du vide");
			}

			// -----------------------------------------------------------------
			// P4 -- LA PREUVE DE (b) : l'aller-retour du NOM
			// -----------------------------------------------------------------
			// C'est le geste exact de la deserialisation : le nom arrive dans une
			// NkString TEMPORAIRE lue de l'archive, on l'interne, la temporaire
			// meurt -- et le champ du document doit toujours dire le bon nom.
			// SANS pool, ce champ pointerait dans une chaine detruite.
			inline void P4_AllerRetourDuNom() {
				printf("[P4] Le nom survit a la mort de la source (le geste de la relecture)\n");

				NkDocStringPool pool;
				const char *champ = nullptr;

				{
					// Portee de la valeur lue depuis l'archive.
					NkString venuDuDisque("largeur_palette");
					champ = pool.Intern(venuDuDisque);
					Check(SameText(champ, "largeur_palette"), "P4 lu correctement");
				} // <- la source meurt ICI

				// Et on remue le tas pour qu'une memoire liberee ne puisse pas
				// passer pour intacte par simple chance.
				NkDocStringPool bruit;
				for (int i = 0; i < 64; ++i) {
					bruit.Intern("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
				}

				Check(SameText(champ, "largeur_palette"), "P4 LE NOM SURVIT A SA SOURCE");
			}

			// -----------------------------------------------------------------
			// P5 -- DUREE DE VIE : le pool possede, et il libere
			// -----------------------------------------------------------------
			inline void P5_DureeDeVie() {
				printf("[P5] Le pool possede ses chaines et les libere\n");

				NkDocStringPool pool;
				pool.Intern("a");
				pool.Intern("b");
				Check(pool.Size() == 2, "P5 deux entrees");

				pool.Clear();
				Check(pool.Size() == 0, "P5 Clear libere tout");

				// Reutilisable apres Clear : le pool n'est pas a usage unique.
				const char *apres = pool.Intern("c");
				Check(SameText(apres, "c"), "P5 utilisable apres Clear");
				Check(pool.Size() == 1, "P5 recompte depuis zero");
			}


			// =================================================================
			// D1..D5 -- LE POOL BRANCHE SUR `NkUIDocument`
			// =================================================================
			// P1..P5 prouvent le pool SEUL. Un pool juste mais non cable ne sert a
			// rien : ces cinq-la font passer un document REEL par les gestes ou un
			// nom de metrique change de proprietaire.
			//
			// ⚠️ ET C'EST LA LE POINT DE CE GROUPE. La preuve evidente etait
			//    l'aller-retour disque (D1). Elle ne voit NI la copie (D3) NI la
			//    greffe (D4) -- les deux seuls endroits ou un `const char*` traverse
			//    une frontiere de document. Meme motif que P4/P1 : la preuve la plus
			//    proche du besoin n'est pas celle qui attrape le plus.

			/// Remue le tas pour qu'une memoire liberee ne passe pas pour intacte par
			/// simple chance. Sans ca, un pointeur pendouillant lit souvent encore le
			/// bon texte, et le controle serait vert pour une mauvaise raison.
			inline void Pietiner() {
				NkDocStringPool bruit;
				for (int i = 0; i < 256; ++i) {
					bruit.Intern("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
				}
			}

			/// Compte les jetons ecrits apres `  <cle> = ` sur la `occurrence`-ieme ligne
			/// qui porte cette cle (0 = la premiere). Rend -1 si elle est absente.
			/// Sert a mesurer la FORME du fichier, pas son contenu.
			//
			// ⚠️ `occurrence` N'EST PAS DU ZELE. Chaque noeud ecrit sa ligne
			///    `hauteur` : la premiere est donc toujours celle de la RACINE. Un
			///    controle qui lirait la premiere mesurerait un noeud qu'il ne visait
			///    pas -- et il aurait eu raison de tomber, pour une mauvaise raison.
			inline int JetonsDeLaLigne(const char *texte, const char *cle, int occurrence) {
				const char *l = texte;
				while (*l) {
					const char *fin = l;
					while (*fin && *fin != '\n') {
						++fin;
					}
					const char *d = l;
					while (*d == ' ' || *d == '\t') {
						++d;
					}
					const char *k = cle;
					while (*k && d < fin && *d == *k) {
						++d;
						++k;
					}
					if (!*k && d < fin && *d == ' ') {
						while (d < fin && (*d == ' ' || *d == '=')) {
							++d;
						}
						int n = 0;
						while (d < fin) {
							while (d < fin && *d == ' ') {
								++d;
							}
							if (d >= fin) {
								break;
							}
							++n;
							while (d < fin && *d != ' ') {
								++d;
							}
						}
						if (occurrence == 0) {
							return n;
						}
						--occurrence;
					}
					l = (*fin) ? fin + 1 : fin;
				}
				return -1;
			}

			/// Dernier caractere de la `occurrence`-ieme ligne portant `cle`. Rend 0 si
			/// elle est absente.
			//
			// ⚠️ L'INVARIANT EST PORTE PAR LA LIGNE D'AXE, PAS PAR LE FICHIER, et
			///    c'est une mesure, pas une precaution. La premiere version de ce
			///    controle interdisait toute ligne finissant par une espace : elle est
			///    tombee sur le code SAIN. Cause : `Field` ecrit `  <cle> = <valeur>`,
			///    donc tout champ VIDE (`composant`, `origine`, `ancrage`...) finit
			///    deja par une espace. La regle generale etait fausse ; seule la ligne
			///    d'axe, qui finit toujours par un nombre ou par un nom, la porte.
			inline char DernierDeLaLigne(const char *texte, const char *cle, int occurrence) {
				const char *l = texte;
				while (*l) {
					const char *fin = l;
					while (*fin && *fin != '\n') {
						++fin;
					}
					const char *d = l;
					while (*d == ' ' || *d == '\t') {
						++d;
					}
					const char *k = cle;
					while (*k && d < fin && *d == *k) {
						++d;
						++k;
					}
					if (!*k && d < fin && *d == ' ') {
						if (occurrence == 0) {
							return (fin > l) ? *(fin - 1) : (char)0;
						}
						--occurrence;
					}
					l = (*fin) ? fin + 1 : fin;
				}
				return (char)0;
			}

			/// Un document a deux noeuds dont le second NOMME sa HAUTEUR.
			/// Rend l'index du noeud nomme (toujours 1).
			//
			// ⚠️ LA HAUTEUR, ET PAS LA LARGEUR : MESURE, PAS PREFERENCE. La racine
			///    d'un document neuf est en `Column`, donc la hauteur est l'axe
			///    PRINCIPAL et la largeur l'axe TRANSVERSE. Sur l'axe transverse,
			///    `crossAlign == Stretch` etire l'enfant et ECRASE sa taille declaree
			///    (`Layout.h`, l. 296) : une metrique nommee y serait parfaitement
			///    relue et parfaitement invisible. La premiere version de ce banc
			///    nommait la largeur ; elle est passee au rouge, et c'est comme ca
			///    que la regle ci-dessus s'est mesuree au lieu de se supposer.
			inline int32 BatirDocNomme(NkUIDocument &d, const char *nom, float32 valeur) {
				d.NewDocument("banc du pool", NkAuthor::Humain);
				const int32 n = d.AddChild(0, "", NkAuthor::Humain, "banc");
				d.SetMetric(nom, valeur);
				d.nodes[(uint32)n].height.mode = NkSizeMode::Fixed;
				// Volontairement FAUX : si le solveur rend 1, c'est que la metrique n'a
				// pas ete relue -- un nom conserve mais inerte serait passe inapercu.
				d.nodes[(uint32)n].height.value = 1.f;
				d.SetSizeMetric(n, false, nom);
				return n;
			}

			// -----------------------------------------------------------------
			// D1 -- LA PREUVE ATTENDUE : un document REEL fait l'aller-retour
			// -----------------------------------------------------------------
			// Le geste complet : on ecrit, la source meurt, on relit, LE TEXTE LU
			// MEURT AUSSI -- et le champ doit encore dire le nom. Puis le solveur du
			// kit doit rendre 240 et non le 1.f pose dans `value` : c'est ce qui
			// separe un nom RELU d'un nom simplement conserve.
			inline void D1_AllerRetourDuDocument() {
				printf("[D1] Un document REEL fait l'aller-retour avec ses metriques\n");

				NkString texte;
				int32 n = -1;
				{
					NkUIDocument source;
					n = BatirDocNomme(source, "largeur_palette", 240.f);
					source.Save(texte);
				} // la source meurt ICI, avec son pool

				Check(n == 1, "D1 le noeud nomme existe");

				NkUIDocument relu;
				bool ok = false;
				{
					// Le tampon lu du disque ne survit pas a la lecture, pas plus qu'un
					// fichier ferme ne survit a son `Load`.
					NkString tampon = texte;
					ok = relu.Load(tampon.Data());
				}
				Check(ok, "D1 le document se relit");
				Pietiner();

				Check(relu.NodeCount() == 2, "D1 deux noeuds relus");
				Check(SameText(relu.nodes[1].height.valueMetric, "largeur_palette"),
					  "D1 LE NOM SURVIT A LA MORT DU TEXTE LU");
				Check(relu.Metric("largeur_palette", -1.f) == 240.f, "D1 la metrique aussi");

				// Le solveur du kit, seul juge de « ca sert a quelque chose ».
				NkLayoutResult lay;
				NkPaintRect surface;
				surface.x = 0.f;
				surface.y = 0.f;
				surface.w = 1000.f;
				surface.h = 600.f;
				NkComputeLayout(relu, surface, lay);
				Check(lay.Has(1), "D1 le noeud est place");
				Check(lay.At(1).h == 240.f, "D1 LA METRIQUE PRIME SUR LE NOMBRE (240, pas 1)");

			// ⚠️ ET CE QUE TOUT CE QUI PRECEDE NE PEUT PAS VOIR. Tous les noms
				//    ci-dessus sont des LITTERAUX : ils sont statiques, donc ils
				//    survivent a tout, y compris a un `SetSizeMetric` qui garderait le
				//    pointeur recu au lieu de le copier. Mesure (mutation F) : en
				//    remplacant `pool.Intern(metricName)` par `metricName`, les 561
				//    controles restaient verts a une exception pres -- un COMPTE
				//    d'entrees, qui ne dit rien du danger.
				//
				//    Le seul appelant qui prouve quelque chose est celui dont le nom
				//    MEURT : c'est le cas reel de l'editeur, ou le nom vient d'un champ
				//    de saisie ou d'une ligne lue. Ci-dessous, la source est detruite
				//    avant meme qu'on relise le champ.
				//
			// ⚠️ ET LE NOM DOIT DEPASSER 23 CARACTERES (`NK_STRING_SSO_SIZE`).
				//    Deuxieme mesure, apres la premiere version de ce controle qui
				//    utilisait « gouttiere_ligne » (15 caracteres) : la mutation F y
				//    survivait encore. En dessous du seuil, `NkString` garde son texte
				//    INLINE, donc dans le cadre de pile -- que rien ne pietine ici. Le
				//    pointeur pendouillant lisait la bonne chaine et le banc etait vert.
				//    Au-dela du seuil, le texte est sur le TAS, et `Pietiner` le reprend.
				//    Un banc de duree de vie doit choisir ses donnees pour que la faute
				//    ait lieu, pas seulement pour qu'elle soit possible.
				NkUIDocument saisi;
				saisi.NewDocument("saisie", NkAuthor::Humain);
				const int32 cible = saisi.AddChild(0, "", NkAuthor::Humain, "banc");
				{
					NkString frappe("gouttiere_du_panneau_de_proprietes"); // 34 > 23 : sur le tas
					Check(saisi.SetSizeMetric(cible, false, frappe.Data()), "D1 la pose accepte");
				} // la chaine saisie meurt ICI
				Pietiner();
				Check(SameText(saisi.nodes[(uint32)cible].height.valueMetric,
							   "gouttiere_du_panneau_de_proprietes"),
					  "D1 UN NOM NON LITTERAL SURVIT A SA SOURCE");
				Check(saisi.SetSizeMetric(-1, false, "x") == false, "D1 un index invalide n'ecrit rien");
			}

			// -----------------------------------------------------------------
			// D2 -- COMPATIBILITE ASCENDANTE, mesuree et non supposee
			// -----------------------------------------------------------------
			// Le 5e jeton n'est ecrit que s'il existe. Un document qui ne nomme aucune
			// taille doit donc produire EXACTEMENT le texte d'avant, et un fichier
			// d'avant (4 jetons) doit se relire sans que rien ne change.
			//
			// ⚠️ C'est ce controle qui protege le corpus existant : avec un 5e jeton
			//    toujours ecrit, tous les fichiers du depot auraient change pour y
			//    poser un champ vide.
			inline void D2_CompatibiliteAscendante() {
				printf("[D2] Sans metrique nommee, le fichier ne bouge pas d'un octet\n");

				NkUIDocument sans;
				sans.NewDocument("banc du pool", NkAuthor::Humain);
				sans.AddChild(0, "", NkAuthor::Humain, "banc");
				NkString ecrit;
				sans.Save(ecrit);

				// La FORME de la ligne d'axe, pas son contenu : quatre jetons, comme
				// avant cette tranche. Occurrence 1 = le noeud fils (0 = la racine).
				Check(JetonsDeLaLigne(ecrit.Data(), "largeur", 1) == 4, "D2 quatre jetons sans metrique");
				Check(JetonsDeLaLigne(ecrit.Data(), "hauteur", 1) == 4, "D2 idem sur l'autre axe");

				NkUIDocument relu;
				Check(relu.Load(ecrit.Data()), "D2 un fichier a 4 jetons se relit");
				Check(relu.nodes[1].height.valueMetric != nullptr, "D2 jamais nul");
				Check(*relu.nodes[1].height.valueMetric == 0, "D2 pas de metrique inventee");
				Check(relu.pool.Size() == 0, "D2 un fichier sans nom ne coute aucune entree");

				NkString reecrit;
				relu.Save(reecrit);
				Check(SameText(ecrit.Data(), reecrit.Data()), "D2 OCTET POUR OCTET apres aller-retour");

			// ⚠️ ET VOICI CE QUE LA LIGNE CI-DESSUS NE PEUT PAS VOIR. Elle compare
				//    deux textes produits par le MEME ecrivain : si l'ecrivain change de
				//    format, les deux changent ensemble et la comparaison reste verte.
				//    Mesure (mutation E) : en ecrivant TOUJOURS le 5e jeton, meme vide,
				//    tous les fichiers gagnent une espace en fin de ligne d'axe -- et
				//    les 561 controles restaient verts. Il faut donc un invariant que
				//    l'ecrivain ne peut pas emporter avec lui : UNE LIGNE D'AXE NE SE
				//    TERMINE JAMAIS PAR UNE ESPACE.
				Check(DernierDeLaLigne(ecrit.Data(), "hauteur", 1) != ' ',
					  "D2 la ligne d'axe ne finit pas par une espace");
				Check(DernierDeLaLigne(ecrit.Data(), "largeur", 1) != ' ', "D2 idem sur l'autre axe");
				NkString avecNom;
				{
					NkUIDocument tmp;
					BatirDocNomme(tmp, "largeur_palette", 240.f);
					tmp.Save(avecNom);
				}
				Check(DernierDeLaLigne(avecNom.Data(), "hauteur", 1) != ' ',
					  "D2 idem quand l'axe est nomme");
				Check(DernierDeLaLigne(avecNom.Data(), "largeur", 1) != ' ',
					  "D2 idem sur l'axe non nomme du meme noeud");

			// ⚠️ LE VRAI TEMOIN DU PASSE : un fichier ECRIT A LA MAIN dans le format
				//    d'avant cette tranche. Il ne vient pas de notre ecrivain, donc il ne
				//    peut pas suivre ses changements. C'est lui qui atteste que les
				//    documents deja sur disque se relisent encore.
				const char *ancien = "nkuidoc 1\n"
									 "titre = format d avant\n"
									 "metrique largeur_palette = 240\n"
									 "noeud 0\n"
									 "  enfants = 1\n"
									 "  hauteur = expand 0 0 0\n"
									 "noeud 1\n"
									 "  enfants =\n"
									 "  hauteur = fixed 42 0 0\n";
				NkUIDocument vieux;
				Check(vieux.Load(ancien), "D2 UN FICHIER ECRIT AVANT SE RELIT ENCORE");
				Check(vieux.NodeCount() == 2, "D2 avec ses deux noeuds");
				Check(vieux.nodes[1].height.value == 42.f, "D2 et son nombre");
				Check(*vieux.nodes[1].height.valueMetric == 0, "D2 sans metrique inventee");
				Check(vieux.pool.Size() == 0, "D2 et sans une entree de pool");

				// Et le document NOMME doit lui aussi etre idempotent : un champ de
				// longueur variable ne doit pas deriver a chaque tour.
				NkUIDocument avec;
				BatirDocNomme(avec, "largeur_palette", 240.f);
				NkString t1;
				avec.Save(t1);
				Check(JetonsDeLaLigne(t1.Data(), "hauteur", 1) == 5, "D2 cinq jetons quand il y a un nom");
				// Et l'autre axe du MEME noeud reste a quatre : le jeton est par AXE, pas
				// par noeud ni par fichier.
				Check(JetonsDeLaLigne(t1.Data(), "largeur", 1) == 4, "D2 l'axe non nomme reste a quatre");
				NkUIDocument tour2;
				Check(tour2.Load(t1.Data()), "D2 le document nomme se relit");
				NkString t2;
				tour2.Save(t2);
				Check(SameText(t1.Data(), t2.Data()), "D2 idempotent avec le 5e jeton");
			}

			// -----------------------------------------------------------------
			// D3 -- CE QUE D1 NE PEUT PAS VOIR (1) : LA COPIE DE DOCUMENT
			// -----------------------------------------------------------------
			// ⚠️ `NkUIDocument` EST copiee par valeur -- cinq fois rien que dans
			//    `Probe.h`. Une copie membre a membre recopierait `valueMetric` tel
			//    quel, donc un pointeur dans le pool de la SOURCE. Tant que la source
			//    vit, tout est vert. On tue donc la source.
			inline void D3_LaCopieNePointePasChezLaSource() {
				printf("[D3] Une copie de document survit a la mort de son original\n");

				NkUIDocument copie;
				{
					NkUIDocument original;
					BatirDocNomme(original, "largeur_palette", 240.f);
					copie = original; // affectation par copie
					Check(SameText(copie.nodes[1].height.valueMetric, "largeur_palette"),
						  "D3 la copie porte le nom");
					// Vrai des maintenant : la copie ne partage pas l'adresse.
					Check(copie.nodes[1].height.valueMetric != original.nodes[1].height.valueMetric,
						  "D3 DEUX ADRESSES : la copie possede la sienne");
				} // l'original meurt ICI
				Pietiner();

				Check(SameText(copie.nodes[1].height.valueMetric, "largeur_palette"),
					  "D3 LE NOM SURVIT A LA MORT DE L'ORIGINAL");

				// L'autre forme, celle que `Probe.h` ecrit vraiment : construction.
				NkUIDocument construite;
				{
					NkUIDocument original;
					BatirDocNomme(original, "gouttiere_ligne", 12.f);
					NkUIDocument tampon(original);
					construite = tampon;
				}
				Pietiner();
				Check(SameText(construite.nodes[1].height.valueMetric, "gouttiere_ligne"),
					  "D3 idem par constructeur de copie");
			}

			// -----------------------------------------------------------------
			// D4 -- CE QUE D1 NE PEUT PAS VOIR (2) : LA GREFFE
			// -----------------------------------------------------------------
			// `GraftFrom` copie des noeuds d'un AUTRE document. C'est le second
			// franchissement de frontiere, et le plus discret : le document receveur
			// a bien un pool, il est simplement vide de ce nom-la.
			inline void D4_LaGreffeNePointePasChezLaSource() {
				printf("[D4] Un noeud greffe ne lit pas dans le pool du document source\n");

				NkUIDocument receveur;
				receveur.NewDocument("receveur", NkAuthor::Humain);
				int32 greffe = -1;
				{
					NkUIDocument donneur;
					const int32 n = BatirDocNomme(donneur, "largeur_palette", 240.f);
					greffe = receveur.GraftFrom(donneur, n, 0, false, NkAuthor::Humain, "greffe");
					Check(greffe > 0, "D4 la greffe a eu lieu");
					Check(receveur.nodes[(uint32)greffe].height.valueMetric !=
							  donneur.nodes[(uint32)n].height.valueMetric,
						  "D4 DEUX ADRESSES : le receveur possede la sienne");
				} // le donneur meurt ICI
				Pietiner();

				Check(SameText(receveur.nodes[(uint32)greffe].height.valueMetric, "largeur_palette"),
					  "D4 LE NOM SURVIT A LA MORT DU DONNEUR");

			// ⚠️ Et la greffe ne transporte PAS la valeur : les metriques sont au
				//    DOCUMENT. Un nom greffe sans sa metrique se resout au repli, et
				//    c'est le comportement voulu -- mais il faut le savoir, sinon on
				//    croit la greffe cassee. Ce n'est pas une dette du pool.
				Check(receveur.Metric("largeur_palette", -1.f) == -1.f,
					  "D4 (limite connue) la metrique elle-meme ne se greffe pas");
			}

			// -----------------------------------------------------------------
			// D5 -- LA CROISSANCE, a l'echelle du document
			// -----------------------------------------------------------------
			// P1 mesure la croissance du pool nu. Ici les DEUX tableaux grandissent en
			// meme temps -- `nodes` et le pool -- et c'est la conjonction qui n'avait
			// jamais ete mesuree : un `NkVector<NkUINode>` qui se reloge pendant
			// qu'on interne.
			// ⚠️ EN CHAINE, PAS EN EVENTAIL, ET C'EST UNE LIMITE MESUREE. La
			//    premiere version empilait 300 enfants sous la racine ; elle est
			//    tombee a la RELECTURE. Cause : `Load` lit une ligne entiere dans
			//    `val[256]`, or la ligne `enfants` d'un noeud a 300 fils depasse
			//    largement. La chaine (chaque noeud fils du precedent) donne les
			//    memes 300 noeuds et 300 noms avec une ligne `enfants` a un seul
			//    index. La limite est ANTERIEURE a cette tranche et sans rapport
			//    avec le pool -- elle est constatee juste en dessous plutot que
			//    contournee en silence.
			inline void D5_CroissanceConjointe() {
				printf("[D5] 300 noeuds nommes : les premiers pointeurs disent encore vrai\n");

				NkUIDocument d;
				d.NewDocument("croissance", NkAuthor::Humain);
				d.SetMetric("largeur_palette", 240.f);

				const char *premier = nullptr;
				int32 parent = 0;
				for (int i = 0; i < 300; ++i) {
					const int32 n = d.AddChild(parent, "", NkAuthor::Humain, "banc");
					d.SetSizeMetric(n, false, "largeur_palette");
					if (i == 0) {
						premier = d.nodes[(uint32)n].height.valueMetric;
					}
					parent = n;
				}

				Check(SameText(premier, "largeur_palette"), "D5 le tout premier pointeur survit");
				Check(SameText(d.nodes[1].height.valueMetric, "largeur_palette"),
					  "D5 et le noeud le dit toujours apres 300 relogements");
				Check(d.pool.Size() == 300, "D5 une entree par nom, pas de deduplication");

				// L'aller-retour a l'echelle : un fichier de 300 axes nommes est le cas
				// ou une troncature ou un decalage se verrait.
				NkString texte;
				d.Save(texte);
				NkUIDocument relu;
				Check(relu.Load(texte.Data()), "D5 300 axes nommes se relisent");
				Check(SameText(relu.nodes[300].height.valueMetric, "largeur_palette"),
					  "D5 y compris le dernier");
				Check(relu.pool.Size() == 300, "D5 le pool relu compte pareil");

			// ⚠️ LA LIMITE, CONSTATEE PLUTOT QUE CONTOURNEE. Le meme nombre de
				//    noeuds en EVENTAIL ne se relit pas : la ligne `enfants` deborde de
				//    `val[256]`. Le comportement est celui qui est documente dans
				//    `Document.h` -- REFUSER plutot que reconstruire a moitie -- et
				//    c'est ce que ce controle fige. S'il passe au vert un jour, c'est
				//    que la borne a bouge, et ce banc doit le dire.
				NkUIDocument eventail;
				eventail.NewDocument("eventail", NkAuthor::Humain);
				for (int i = 0; i < 300; ++i) {
					eventail.AddChild(0, "", NkAuthor::Humain, "banc");
				}
				NkString large;
				eventail.Save(large);
				NkUIDocument refus;
				Check(!refus.Load(large.Data()),
					  "D5 (limite anterieure) 300 freres : la relecture REFUSE, elle ne bricole pas");
			}

			inline int RunPoolControls() {
				setvbuf(stdout, nullptr, _IONBF, 0);
				printf("=========================================================\n");
				printf(" NkUIDesign -- controles du pool de chaines du document\n");
				printf("=========================================================\n\n");

				s_pass = 0;
				s_fail = 0;

				P1_AdressesStables();
				P2_PasDeDeduplication();
				P3_SansMetriqueResteValide();
				P4_AllerRetourDuNom();
				P5_DureeDeVie();

				D1_AllerRetourDuDocument();
				D2_CompatibiliteAscendante();
				D3_LaCopieNePointePasChezLaSource();
				D4_LaGreffeNePointePasChezLaSource();
				D5_CroissanceConjointe();

				const int total = s_pass + s_fail;
				printf("\n---------------------------------------------------------\n");
				printf(" CONTROLES POOL : %d / %d\n", s_pass, total);
				printf("---------------------------------------------------------\n");
				return (s_fail == 0) ? 0 : 1;
			}

	} // namespace poolctl
} // namespace nkuidesign

#endif // NKUIDESIGN_NKDOCPOOLCONTROLS_H

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================
