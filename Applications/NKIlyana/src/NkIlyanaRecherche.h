// =============================================================================
// NkIlyanaRecherche — retrouver un passage dans un corpus, pour qu'Ilyana LISE
// au lieu de DEVINER.
// -----------------------------------------------------------------------------
// LE PROBLÈME QU'ON ATTAQUE ICI. À 20 millions de paramètres, Ilyana ne peut pas
// retenir une bibliothèque : ce qu'elle absorbe d'un livre, c'est la langue — le
// vocabulaire, les tournures, la façon de mener un raisonnement. Les faits
// précis, elle les reconstruit de mémoire, et se trompe. Grossir le modèle
// repousse le problème sans le supprimer.
//
// L'autre voie est de lui donner de quoi OUVRIR le livre au moment de répondre.
// Une réponse fondée sur un passage retrouvé peut être citée, donc vérifiée, donc
// contredite. C'est ce qui fait reculer l'hallucination par construction plutôt
// que par la taille — et c'est à la portée d'un petit modèle.
//
// COMMENT. Un index inversé + BM25, écrit ici de bout en bout.
//
// POURQUOI BM25 ET PAS UN SIMPLE COMPTAGE DE MOTS. Trois défauts qu'un comptage
// naïf ne voit pas, et que BM25 corrige chacun par un terme précis :
//  - « de », « la », « et » apparaissent partout et ne distinguent rien. L'IDF
//    leur donne un poids qui tend vers zéro : un mot présent dans presque tous
//    les passages n'apporte aucune information sur celui qu'on cherche ;
//  - un mot répété vingt fois ne rend pas un passage vingt fois plus pertinent.
//    La saturation en tf/(tf+k1) fait que le deuxième emploi compte beaucoup, le
//    vingtième presque plus ;
//  - un long passage contient mécaniquement plus de mots, donc gagnerait toujours.
//    La normalisation par la longueur le ramène à égalité avec un passage court.
//
// CE QUI N'EST PAS FAIT, ET QUI SE VOIT. La recherche est LEXICALE : elle trouve
// les passages qui emploient les mots de la question. Demander « la capitale du
// Cameroun » ne trouvera pas un passage qui dit « Yaoundé est le siège des
// institutions » sans le mot « capitale ». Une recherche par le sens demanderait
// des vecteurs d'embedding — c'est la suite, pas le préalable.
// AUTEUR : Rihen — LICENCE : Propriétaire - usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#ifndef NK_ILYANA_RECHERCHE_H
#define NK_ILYANA_RECHERCHE_H

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// L'espace de noms est `ilyana` au niveau global, comme les autres briques de
// cette application (identité, valeurs, contrôle). Le placer dans `nkentseu`
// rendrait toute mention de `ilyana::` ambiguë sous le `using namespace nkentseu`
// du fichier principal.
namespace ilyana {
	using namespace nkentseu;

		// ---------------------------------------------------------------------
		// Découpage en mots. On garde les octets ≥ 0x80 comme des lettres : en
		// UTF-8, « é » s'écrit sur deux octets, et les traiter en séparateurs
		// couperait « mémoire » en « m », « moire » — trois mots pour un, dont
		// aucun ne se retrouverait à la recherche.
		// ---------------------------------------------------------------------
		inline bool EstLettre(unsigned char c) {
			if (c >= 0x80)
				return true;
			if (c >= 'a' && c <= 'z')
				return true;
			if (c >= 'A' && c <= 'Z')
				return true;
			if (c >= '0' && c <= '9')
				return true;
			return false;
		}

		// Replie une lettre latine accentuée sur sa lettre de base. Rend 0 si
		// l'octet suivant n'appartient pas à une séquence connue.
		//
		// POURQUOI C'EST INDISPENSABLE, ET PAS UN RAFFINEMENT. Mesuré : chercher
		// « Yaounde » dans un corpus qui écrit « Yaoundé » ne rendait AUCUN
		// passage. En UTF-8 le « é » s'écrit sur deux octets (0xC3 0xA9), donc
		// « Yaounde » et « Yaoundé » n'ont pas le même haché — ce sont deux mots
		// étrangers l'un à l'autre. Or personne ne tape les accents dans une
		// question. Sans ce repliement, une recherche en français échoue sur une
		// grande partie du vocabulaire, en silence et sans message d'erreur.
		inline char PlierAccent(unsigned char c1, unsigned char c2) {
			if (c1 == 0xC3) {
				if ((c2 >= 0x80 && c2 <= 0x85) || (c2 >= 0xA0 && c2 <= 0xA5))
					return 'a';
				if (c2 == 0x87 || c2 == 0xA7)
					return 'c';
				if ((c2 >= 0x88 && c2 <= 0x8B) || (c2 >= 0xA8 && c2 <= 0xAB))
					return 'e';
				if ((c2 >= 0x8C && c2 <= 0x8F) || (c2 >= 0xAC && c2 <= 0xAF))
					return 'i';
				if (c2 == 0x91 || c2 == 0xB1)
					return 'n';
				if ((c2 >= 0x92 && c2 <= 0x96) || (c2 >= 0xB2 && c2 <= 0xB6))
					return 'o';
				if ((c2 >= 0x99 && c2 <= 0x9C) || (c2 >= 0xB9 && c2 <= 0xBC))
					return 'u';
				if (c2 == 0x9D || c2 == 0xBD || c2 == 0xBF)
					return 'y';
			}
			return 0;
		}

		// Même repliement, mais pour un accent codé sur UN SEUL octet (Latin-1).
		//
		// POURQUOI CE SECOND CAS EXISTE. Le corpus est en UTF-8, mais la console
		// Windows livre les arguments dans son encodage hérité : « Yaoundé » tapé
		// au clavier arrive avec un seul octet 0xE9 là où le fichier en contient
		// deux. Sans ce repliement, celui qui tape SES ACCENTS ne trouve rien,
		// alors que celui qui les omet trouve — l'inverse de ce qu'on attend, et
		// d'autant plus déroutant que rien ne signale l'encodage en cause.
		inline char PlierLatin1(unsigned char c) {
			if (c >= 0xC0 && c <= 0xC5)
				return 'a';
			if (c == 0xC7)
				return 'c';
			if (c >= 0xC8 && c <= 0xCB)
				return 'e';
			if (c >= 0xCC && c <= 0xCF)
				return 'i';
			if (c == 0xD1)
				return 'n';
			if (c >= 0xD2 && c <= 0xD6)
				return 'o';
			if (c >= 0xD9 && c <= 0xDC)
				return 'u';
			if (c == 0xDD)
				return 'y';
			if (c >= 0xE0 && c <= 0xE5)
				return 'a';
			if (c == 0xE7)
				return 'c';
			if (c >= 0xE8 && c <= 0xEB)
				return 'e';
			if (c >= 0xEC && c <= 0xEF)
				return 'i';
			if (c == 0xF1)
				return 'n';
			if (c >= 0xF2 && c <= 0xF6)
				return 'o';
			if (c >= 0xF9 && c <= 0xFC)
				return 'u';
			if (c == 0xFD || c == 0xFF)
				return 'y';
			return 0;
		}

		// Vrai si l'octet ouvre une séquence UTF-8 valide. Ce test protège le
		// repliement Latin-1 : appliqué à l'aveugle, il transformerait le premier
		// octet d'une apostrophe typographique (0xE2 0x80 0x99) en « a », et
		// abîmerait le corpus au lieu de l'aider.
		inline bool DebutUtf8(unsigned char c1, unsigned char c2) {
			return c1 >= 0xC2 && c1 <= 0xF4 && c2 >= 0x80 && c2 <= 0xBF;
		}

		inline uint64 HachageMot(const char *p, nk_size n) {
			// FNV-1a 64 bits, sur le mot ramené à sa forme la plus dépouillée :
			// minuscules ASCII et lettres sans accent. « Cameroun », « cameroun »
			// et « CAMEROUN » deviennent le même mot ; « Yaoundé » et « Yaounde »
			// aussi. Ce repliement doit être fait ICI et nulle part ailleurs :
			// l'indexation et la recherche appellent la même fonction, elles ne
			// peuvent donc pas diverger — une normalisation dupliquée des deux
			// côtés finirait un jour par différer d'un cas, et la recherche
			// rendrait alors des résultats faux sans que rien ne le signale.
			uint64 h = 1469598103934665603ull;
			for (nk_size i = 0; i < n; ++i) {
				unsigned char c = (unsigned char)p[i];
				if (c >= 'A' && c <= 'Z')
					c = (unsigned char)(c - 'A' + 'a');
				else if (c >= 0x80) {
					const unsigned char suivant = (i + 1 < n) ? (unsigned char)p[i + 1] : 0;
					const char plie = PlierAccent(c, suivant);
					if (plie != 0) {
						++i; // la séquence accentuée occupe deux octets
						c = (unsigned char)plie;
					} else if (c == 0xC5 && suivant == 0x93) {
						// « œ » vaut « oe » : « cœur » doit répondre à « coeur ».
						++i;
						h ^= (uint64)'o';
						h *= 1099511628211ull;
						c = 'e';
					} else if (!DebutUtf8(c, suivant)) {
						// Pas de l'UTF-8 : très probablement du Latin-1 venu de
						// la console. On replie, ou on laisse tel quel si ce
						// n'est pas une lettre accentuée connue.
						const char l1 = PlierLatin1(c);
						if (l1 != 0)
							c = (unsigned char)l1;
					}
				}
				h ^= (uint64)c;
				h *= 1099511628211ull;
			}
			return h;
		}

		// ---------------------------------------------------------------------
		// Une occurrence : ce mot, dans ce passage. Le compte est agrégé après
		// tri — c'est bien moins cher que de tenir une table de hachage vivante
		// pendant l'indexation, et le tri sert de toute façon à la recherche.
		// ---------------------------------------------------------------------
		struct Occurrence {
			uint64 mot;
			uint32 passage;
			uint32 compte;
		};

		struct Passage {
			uint64 offset; // position dans le fichier — le texte n'est PAS gardé
			uint32 taille; // en octets
			uint32 nbMots;
		};

		// Tri par (mot, passage). Écrit ici plutôt qu'emprunté : trois lignes de
		// comparaison, et aucune dépendance à une politique de tri extérieure.
		inline void TrierOccurrences(Occurrence *a, int64 gauche, int64 droite) {
			while (gauche < droite) {
				const Occurrence pivot = a[(gauche + droite) / 2];
				int64 i = gauche;
				int64 j = droite;
				while (i <= j) {
					while (a[i].mot < pivot.mot || (a[i].mot == pivot.mot && a[i].passage < pivot.passage))
						++i;
					while (a[j].mot > pivot.mot || (a[j].mot == pivot.mot && a[j].passage > pivot.passage))
						--j;
					if (i <= j) {
						const Occurrence t = a[i];
						a[i] = a[j];
						a[j] = t;
						++i;
						--j;
					}
				}
				// On récurse sur le petit côté et on boucle sur le grand : la
				// pile reste en O(log n) même sur des données déjà ordonnées.
				if (j - gauche < droite - i) {
					if (gauche < j)
						TrierOccurrences(a, gauche, j);
					gauche = i;
				} else {
					if (i < droite)
						TrierOccurrences(a, i, droite);
					droite = j;
				}
			}
		}

		struct Resultat {
			uint32 passage;
			float score;
			NkString texte;
		};

		class NkIndex {
		public:
			// Construit l'index sur un fichier découpé en passages par les lignes
			// vides. `maxOctets` borne la lecture : sur un corpus de plusieurs
			// giga-octets, indexer tout demanderait plus de mémoire que la
			// machine n'en a, et il vaut mieux le dire que se faire tuer par le
			// système au milieu.
			bool Construire(const char *chemin, int64 maxOctets, int64 minPassage = 200) {
				mChemin = chemin;
				mPassages.Clear();
				mOccurrences.Clear();
				mTotalMots = 0;

				FILE *f = fopen(chemin, "rb");
				if (!f)
					return false;

				const nk_size tailleLot = 4u << 20;
				char *tampon = (char *)malloc(tailleLot);
				if (!tampon) {
					fclose(f);
					return false;
				}

				NkString courant;
				uint64 offsetCourant = 0;
				uint64 lu = 0;
				bool fini = false;

				// Lecture en flux : le fichier peut peser plus que la mémoire.
				// On accumule le passage en cours et on le clôt sur la ligne
				// vide, exactement comme le reste de la chaîne Ilyana.
				while (!fini) {
					const nk_size got = fread(tampon, 1, tailleLot, f);
					if (got == 0)
						break;
					for (nk_size i = 0; i < got; ++i) {
						const char c = tampon[i];
						if (c == '\r')
							continue;
						courant.Append(c);
						const nk_size n = courant.Size();
						if (n >= 2 && courant.Data()[n - 1] == '\n' && courant.Data()[n - 2] == '\n') {
							AjouterPassage(courant, offsetCourant, minPassage);
							offsetCourant = lu + (uint64)i + 1;
							courant.Clear();
						}
					}
					lu += (uint64)got;
					if (maxOctets > 0 && (int64)lu >= maxOctets)
						fini = true;
				}
				if (courant.Size() > 0)
					AjouterPassage(courant, offsetCourant, minPassage);

				free(tampon);
				fclose(f);

				if (mOccurrences.Size() == 0)
					return false;

				TrierOccurrences(mOccurrences.Data(), 0, (int64)mOccurrences.Size() - 1);
				Agreger();
				return mPassages.Size() > 0;
			}

			// Recherche BM25. Rend au plus `k` passages, du meilleur au moins bon.
			void Chercher(const NkString &question, int32 k, NkVector<Resultat> &out) const {
				out.Clear();
				if (mPassages.Size() == 0)
					return;

				NkVector<uint64> motsQuestion;
				DecouperMots(question.Data(), question.Size(), motsQuestion);
				if (motsQuestion.Size() == 0)
					return;

				const nk_size nP = mPassages.Size();
				NkVector<float> scores;
				scores.Resize(nP);
				for (nk_size i = 0; i < nP; ++i)
					scores[i] = 0.f;

				const double N = (double)nP;
				const double avgdl = (mTotalMots > 0) ? ((double)mTotalMots / N) : 1.0;
				const double k1 = 1.2;
				const double b = 0.75;

				for (nk_size q = 0; q < motsQuestion.Size(); ++q) {
					const uint64 mot = motsQuestion[q];
					// Le même mot répété dans la question ne doit pas compter
					// deux fois : la question n'est pas un document.
					bool dejaVu = false;
					for (nk_size r = 0; r < q; ++r)
						if (motsQuestion[r] == mot) {
							dejaVu = true;
							break;
						}
					if (dejaVu)
						continue;

					nk_size deb = 0;
					nk_size fin = 0;
					if (!Plage(mot, deb, fin))
						continue;

					const double df = (double)(fin - deb);
					const double idf = Log((N - df + 0.5) / (df + 0.5) + 1.0);
					for (nk_size i = deb; i < fin; ++i) {
						const Occurrence &o = mOccurrences[i];
						const double tf = (double)o.compte;
						const double dl = (double)mPassages[o.passage].nbMots;
						const double denom = tf + k1 * (1.0 - b + b * dl / avgdl);
						scores[o.passage] += (float)(idf * (tf * (k1 + 1.0)) / (denom > 0.0 ? denom : 1.0));
					}
				}

				// Sélection des k meilleurs par balayages successifs : pour un k
				// petit (3 à 10), c'est plus simple et plus rapide qu'un tri
				// complet de centaines de milliers de scores.
				for (int32 rang = 0; rang < k; ++rang) {
					nk_size best = nP;
					float bestScore = 0.f;
					for (nk_size i = 0; i < nP; ++i)
						if (scores[i] > bestScore) {
							bestScore = scores[i];
							best = i;
						}
					if (best == nP)
						break;
					Resultat r;
					r.passage = (uint32)best;
					r.score = bestScore;
					r.texte = LirePassage((uint32)best);
					out.PushBack(r);
					scores[best] = 0.f;
				}
			}

			nk_size NbPassages() const { return mPassages.Size(); }
			nk_size NbMotsDistincts() const { return mNbMotsDistincts; }
			int64 TotalMots() const { return mTotalMots; }

			// Relit le passage sur disque. L'index ne garde que des positions :
			// c'est ce qui permet d'indexer un corpus bien plus gros que la RAM.
			NkString LirePassage(uint32 id) const {
				NkString s;
				if (id >= mPassages.Size())
					return s;
				FILE *f = fopen(mChemin.CStr(), "rb");
				if (!f)
					return s;
				const Passage &p = mPassages[id];
				if (fseek(f, (long)p.offset, SEEK_SET) == 0) {
					char *buf = (char *)malloc(p.taille + 1);
					if (buf) {
						const nk_size got = fread(buf, 1, p.taille, f);
						s.Append(buf, got);
						free(buf);
					}
				}
				fclose(f);
				return s;
			}

		private:
			static double Log(double x) {
				// Logarithme népérien sans <cmath> : série sur atanh, qui
				// converge vite une fois x ramené dans [1, 2) par extraction des
				// puissances de deux.
				if (x <= 0.0)
					return 0.0;
				int e = 0;
				while (x >= 2.0) {
					x *= 0.5;
					++e;
				}
				while (x < 1.0) {
					x *= 2.0;
					--e;
				}
				const double z = (x - 1.0) / (x + 1.0);
				const double z2 = z * z;
				double terme = z;
				double somme = 0.0;
				for (int i = 0; i < 24; ++i) {
					somme += terme / (double)(2 * i + 1);
					terme *= z2;
				}
				return 2.0 * somme + (double)e * 0.6931471805599453;
			}

			static void DecouperMots(const char *p, nk_size n, NkVector<uint64> &out) {
				nk_size i = 0;
				while (i < n) {
					while (i < n && !EstLettre((unsigned char)p[i]))
						++i;
					const nk_size deb = i;
					while (i < n && EstLettre((unsigned char)p[i]))
						++i;
					if (i > deb)
						out.PushBack(HachageMot(p + deb, i - deb));
				}
			}

			void AjouterPassage(const NkString &texte, uint64 offset, int64 minPassage) {
				if ((int64)texte.Size() < minPassage)
					return;
				NkVector<uint64> mots;
				DecouperMots(texte.Data(), texte.Size(), mots);
				if (mots.Size() == 0)
					return;
				const uint32 id = (uint32)mPassages.Size();
				Passage p;
				p.offset = offset;
				p.taille = (uint32)texte.Size();
				p.nbMots = (uint32)mots.Size();
				mPassages.PushBack(p);
				mTotalMots += (int64)mots.Size();
				for (nk_size i = 0; i < mots.Size(); ++i) {
					Occurrence o;
					o.mot = mots[i];
					o.passage = id;
					o.compte = 1;
					mOccurrences.PushBack(o);
				}
			}

			// Après tri, les occurrences d'un même mot dans un même passage sont
			// contiguës : on les fond en une seule, dont le compte est le tf.
			void Agreger() {
				nk_size w = 0;
				mNbMotsDistincts = 0;
				for (nk_size r = 0; r < mOccurrences.Size();) {
					nk_size s = r + 1;
					while (s < mOccurrences.Size() && mOccurrences[s].mot == mOccurrences[r].mot &&
						   mOccurrences[s].passage == mOccurrences[r].passage)
						++s;
					Occurrence o = mOccurrences[r];
					o.compte = (uint32)(s - r);
					if (w == 0 || mOccurrences[w - 1].mot != o.mot)
						++mNbMotsDistincts;
					mOccurrences[w] = o;
					++w;
					r = s;
				}
				mOccurrences.Resize(w);
			}

			// Plage des occurrences d'un mot, par dichotomie sur le tableau trié.
			bool Plage(uint64 mot, nk_size &deb, nk_size &fin) const {
				nk_size lo = 0;
				nk_size hi = mOccurrences.Size();
				while (lo < hi) {
					const nk_size mid = lo + (hi - lo) / 2;
					if (mOccurrences[mid].mot < mot)
						lo = mid + 1;
					else
						hi = mid;
				}
				if (lo >= mOccurrences.Size() || mOccurrences[lo].mot != mot)
					return false;
				deb = lo;
				hi = mOccurrences.Size();
				while (lo < hi) {
					const nk_size mid = lo + (hi - lo) / 2;
					if (mOccurrences[mid].mot <= mot)
						lo = mid + 1;
					else
						hi = mid;
				}
				fin = lo;
				return true;
			}

			NkString mChemin;
			NkVector<Passage> mPassages;
			NkVector<Occurrence> mOccurrences;
			int64 mTotalMots = 0;
			nk_size mNbMotsDistincts = 0;
		};

} // namespace ilyana

#endif // NK_ILYANA_RECHERCHE_H
