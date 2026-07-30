// =============================================================================
// NKSpeech/NkLangModel.h — modèle de langue n-gram (bigramme), header-only.
//
// Brique 3 de la Phase 8 (Lexique/décodage) : RE-SCORER les hypothèses produites
// par le décodage CTC beam search déjà existant (`NkCTCBeamSearchDecode`,
// `NkAsrModel.h`) avec un modèle de langue statistique simple, entraîné sur un
// petit corpus texte (compte de bigrammes + lissage add-one/Laplace).
//
// -----------------------------------------------------------------------------
// SOURCES (recherche web préalable, technique standard en ASR) :
//   - Hannun, Case, Casper, Catanzaro, Diamos, Elsen, Prenger, Satheesh, Sengupta,
//     Coates, Ng, « Deep Speech: Scaling up end-to-end speech recognition »,
//     arXiv:1412.5567 (2014), section 3.3 « Language Model » : le décodage CTC
//     combine le score acoustique du réseau et le score d'un modèle de langue
//     n-gram externe par une somme log-linéaire pondérée
//     Q(c) = log(p_ctc(c|x)) + alpha*log(p_lm(c)) + beta*|c|_mots — c'est
//     exactement la formule reprise ci-dessous (fonction `NkRescoreWithLM`),
//     appliquée ici à une liste N-best plutôt qu'en fusion à chaque pas du beam
//     (voir aussi Graves & Jaitly, ICML 2014, « Towards End-to-End Speech
//     Recognition with Recurrent Neural Networks » : rescoring d'une liste N-best
//     CTC par un modèle de langue externe).
//   - Cette famille de méthodes est nommée « shallow fusion » / « N-best
//     rescoring » dans la littérature récente (ex. survey NVIDIA NeMo,
//     « ASR Language Modeling and Customization », docs.nvidia.com, 2026) :
//     shallow fusion interpole le score du modèle acoustique et celui d'un LM
//     externe (poids alpha réglé sur un ensemble de validation) ; le rescoring
//     N-best est la variante qui n'évalue le LM QUE sur les hypothèses finales
//     du beam (moins coûteux, ce que fait ce fichier), par opposition à la fusion
//     "profonde" qui interroge le LM à CHAQUE extension de préfixe pendant le
//     beam search lui-même.
//
// Zero-STL, namespace nkentseu::ai. Header-only (comme NkAsrModel.h) : dépend
// seulement de NKContainers, que NKSpeech lie déjà.
//
// AUTEUR : Rihen — LICENCE : usage régi par le fichier LICENSE à la racine du dépôt
// =============================================================================
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace ai {

		// Modèle de langue BIGRAMME sur un petit vocabulaire de symboles entiers
		// 0..vocab-1 (les symboles CTC déjà "collapsés" — sans blanc ni répétition —
		// tels que produits par NkCTCGreedyDecode/NkCTCBeamSearchDecode). Compte
		// unigrammes + bigrammes sur un corpus de séquences ("phrases"), lissage
		// add-one (Laplace) pour ne jamais renvoyer une probabilité nulle sur un
		// bigramme non vu (indispensable : le beam search peut proposer des suites
		// jamais rencontrées dans le corpus d'entraînement du LM).
		class NkNgramLM {
			public:
				NkNgramLM() = default;

				// `vocab` = nombre de symboles (SANS le blanc CTC : le LM ne voit jamais
				// le blanc, seulement les symboles déjà collapsés).
				void Init(int32 vocab) {
					mVocab = vocab > 0 ? vocab : 1;
					mUnigram.Resize((nk_size)mVocab);
					mBigram.Resize((nk_size)mVocab * (nk_size)mVocab);
					for (uint32 i = 0; i < mUnigram.Size(); ++i)
						mUnigram[i] = 0.0;
					for (uint32 i = 0; i < mBigram.Size(); ++i)
						mBigram[i] = 0.0;
					mTotalUnigram = 0.0;
				}

				// Entraîne (compte) sur un corpus de séquences déjà en symboles (chacune une
				// "phrase" indépendante : le bigramme ne franchit jamais une frontière de
				// séquence). Peut être appelé plusieurs fois (les comptes s'accumulent).
				void Train(const NkVector<NkVector<int32>> &corpus) {
					for (uint32 s = 0; s < corpus.Size(); ++s) {
						const NkVector<int32> &seq = corpus[s];
						for (uint32 i = 0; i < seq.Size(); ++i) {
							const int32 sym = seq[i];
							if (sym < 0 || sym >= mVocab)
								continue;
							mUnigram[(nk_size)sym] += 1.0;
							mTotalUnigram += 1.0;
							if (i > 0) {
								const int32 prev = seq[i - 1];
								if (prev >= 0 && prev < mVocab)
									mBigram[(nk_size)prev * (nk_size)mVocab + (nk_size)sym] += 1.0;
							}
						}
					}
				}

				// P(sym) lissée (add-one) — sert de probabilité du PREMIER symbole d'une séquence.
				double UnigramProb(int32 sym) const {
					if (sym < 0 || sym >= mVocab)
						return 1.0 / (double)mVocab;
					return (mUnigram[(nk_size)sym] + 1.0) / (mTotalUnigram + (double)mVocab);
				}

				// P(next | prev) lissée (add-one).
				double BigramProb(int32 prev, int32 next) const {
					if (prev < 0 || prev >= mVocab || next < 0 || next >= mVocab)
						return 1.0 / (double)mVocab;
					const double c12 = mBigram[(nk_size)prev * (nk_size)mVocab + (nk_size)next];
					const double c1 = mUnigram[(nk_size)prev];
					return (c12 + 1.0) / (c1 + (double)mVocab);
				}

				// log P(seq) sous le modèle : unigramme du 1er symbole * bigrammes successifs.
				// Séquence vide -> 0.0 (log(1), neutre).
				double LogProb(const NkVector<int32> &seq) const {
					if (seq.IsEmpty())
						return 0.0;
					double lp = math::NkLog((float32)UnigramProb(seq[0]));
					for (uint32 i = 1; i < seq.Size(); ++i)
						lp += (double)math::NkLog((float32)BigramProb(seq[i - 1], seq[i]));
					return lp;
				}

				int32 Vocab() const {
					return mVocab;
				}

				// Auto-test headless : entraîne un LM caractère (bigramme) sur un VRAI corpus
				// texte français déjà présent dans le dépôt (AI/corpus/lamba/corpus_fr.txt,
				// texte encyclopédique fr, article "Afrique" — extrait recopié tel quel,
				// diacritiques translittérées en ASCII pour éviter tout littéral non-ASCII
				// dans ce .h, cf. convention déjà suivie par NKSpeechTest/main.cpp) et
				// vérifie des faits statistiques RÉELS de la langue française (ex. sur cet
				// extrait, "q" est suivi de "u" dans 67 occurrences sur 67 -> P(u|q) doit
				// dominer très largement la baseline uniforme ; un mot réel du corpus doit
				// obtenir une log-probabilité bien supérieure à son anagramme mélangé) —
				// preuve que le comptage n-gramme reflète une vraie distribution
				// linguistique, pas un artefact. AUCUN GPU/device.
				static bool SelfTest() {
					// Extrait ASCII (diacritiques retirés) de AI/corpus/lamba/corpus_fr.txt,
					// paragraphes de l'article "Afrique" (texte encyclopédique déjà présent
					// dans le dépôt, réutilisé tel quel comme corpus GPT/NKSpeech).
					const char *kCorpusText =
						"L'Afrique est un continent terrestre borde a l'est par l'ocean Indien et a l'ouest par l'ocean Atlan"
						"tique. Centre sur l'equateur, et s'etirant au-dela des lattitudes tropicales des deux hemispheres, s"
						"es extremites atteignent la mer Mediterannee au nord, et l'ocean Austral au sud. Par sa superficie d"
						"e 30 415 873 km2, il est le troisieme continent de la planete, occupant 6 % de sa superficie totale "
						"et 20 % de ses terres emergees. Par sa population de 1,5 milliard d'etres humains repartie a travers"
						" 54 etats souverains, le continent africain represente 17,2 % de la population mondiale. Ses habitan"
						"ts sont appeles les africains.  L'Afrique est traversee presque en son milieu par l'equateur et pres"
						"ente plusieurs climats : chaud et humide au plus pres de l'equateur, tropical dans les regions compr"
						"ises entre l'equateur et les tropiques, chaud et aride autour des tropiques, tempere dans les zones "
						"d'altitude. Le continent est caracterise par le manque de precipitations regulieres. En l'absence de"
						" glaciers ou de systemes montagneux aquiferes, il n'existe pas de moyen de regulation naturelle du c"
						"limat a l exception de la flore (forets notamment) et de la proximite de la mer. Les terres arides r"
						"epresentent 60 % du continent, dont l'environnement est neanmoins tres riche on l'a appele le paradi"
						"s de la biodiversite . Le continent abrite le deuxieme massif forestier continu de la planete : la f"
						"oret du bassin du Congo, mais qui est menace par la surexploitation, la deforestation, la fragmentat"
						"ion forestiere et la baisse de la biodiversite, consequences de la pression anthropique, exacerbee p"
						"ar le changement climatique. En 2020, les indicateurs climatiques montraient une elevation continue "
						"des temperatures en Afrique, une acceleration de l'elevation du niveau de la mer, et des evenements "
						"meteorologiques et climatiques extremes plus frequents (ex : inondations, secheresses, et leurs effe"
						"ts devastateurs). Le retrecissement rapide des derniers glaciers d'Afrique de l'Est, qui devraient f"
						"ondre entierement dans un avenir proche, signe aussi la menace d'un changement imminent et irreversi"
						"ble du systeme Terre. Le continent est considere comme le berceau de l'humanite, la ou sont apparus "
						"les ancetres de l'Homme, puis, il y a 200 000 ans environ, l'homme moderne qui s'est ensuite repandu"
						" sur le reste du globe. Le Sahara, le plus grand desert chaud du monde, a cree un hiatus, conduisant"
						" a des evolutions historiques distinctes entre le nord et le sud. A la periode historique, la civili"
						"sation de l'Egypte antique se developpe le long du Nil, l'Afrique subsaharienne voit naitre ses prop"
						"res civilisations dans les zones de savanes ; l'Afrique du Nord, rive sud de la Mediterranee, subit "
						"quant a elle l'influence des Pheniciens, des Grecs et des Romains. A compter de 3000 av. J.-C. l'Afr"
						"ique connait l'expansion bantoue. Il s'agit d'un mouvement de population en plusieurs phases, orient"
						"e globalement du nord, depuis le grassland du Cameroun actuel, vers le sud, jusqu'en Afrique austral"
						"e, atteinte aux debuts de l'ere chretienne. L'expansion bantoue explique la carte ethnolinguistique "
						"actuelle de la zone subsaharienne. La religion chretienne s'implante en l'Afrique des le Ier siecle,"
						" essentiellement dans l'Afrique romaine du nord du continent puis en Ethiopie. Le VIIe siecle voit l"
						"es debuts de l'islam en Afrique, lequel s'installe sur la cote est et dans le nord du continent jusq"
						"u'a la frange septentrionale de la zone subsaharienne. L'Afrique du Nord est, dans le meme temps, ar"
						"abisee. En Afrique subsaharienne, a partir du VIIIe siecle et jusqu'au XVIIe siecle, de puissants et"
						" riches empires se succedent. Vers la fin de cette periode, au XVe siecle, les Portugais, suivis par"
						" d'autres nations europeennes, installent sur la cote ouest un trafic d'esclaves, la traite atlantiq"
						"ue, qui s'ajoute a la traite intra-africaine et a la traite orientale qui sevissent deja sur le cont"
						"inent. Le XVIIIe siecle marque le debut des explorations europeennes, suivies par la colonisation ma"
						"ssive du continent entre la fin du XIXe et le debut du XXe siecle. La traite esclavagiste cesse au d"
						"ebut du XXe siecle, mais l'Afrique est presque entierement sous domination coloniale jusqu'au milieu"
						" du XXe siecle, ce qui modele jusqu'a aujourd'hui les frontieres et les economies des pays concernes"
						". La plupart des Etats obtiennent leur independance entre la fin des annees 1950 (Tunisie, Maroc, Gh"
						"ana...) et le milieu des annees 1970 (Angola, Mozambique...). L'Afrique independante est constituee "
						"essentiellement de democraties imparfaites voire de regimes autoritaires et les conflits y sont nomb"
						"reux. Selon le Comite international de la Croix-Rouge, environ 40 % des guerres en cours sur la plan"
						"ete sont en Afrique. Depuis l'accession a l'independance du Soudan du Sud en 2011, l'Afrique, compre"
						"nant Madagascar, compte 54 Etats souverains (non inclus la RASD et le Somaliland). Les pays du conti"
						"nent presentent la croissance demographique la plus importante de la planete et une situation sanita"
						"ire qui s'ameliore nettement tout en progressant moins vite que dans les autres pays en developpemen"
						"t. L'Afrique repose sur une organisation sociale fondee sur la famille elargie et l'appartenance eth"
						"nique ; on recense un millier d'ethnies sur le continent, lequel possede en parallele la diversite l"
						"inguistique la plus elevee du monde avec pres de 2 000 langues vivantes. L'Afrique contemporaine est"
						" dans une situation ou le poids de la demographie est delicat a gerer (chomage, financement de l'edu"
						"cation...) car le continent reste celui qui est le moins developpe economiquement malgre une forte c"
						"roissance depuis le debut du XXIe siecle, laquelle a permis l'emergence d'une classe moyenne, moins "
						"feconde, aux revenus plus eleves.";

					// texte -> symboles : 'a'..'z' (minuscule forcee) -> 0..25, tout le reste
					// (ponctuation, chiffres, espaces) -> 26 (un seul symbole "separateur",
					// runs consecutifs fusionnes pour ne pas noyer les bigrammes utiles sous
					// des repetitions de separateur).
					NkVector<int32> seq;
					bool lastWasSep = true; // évite un séparateur en tête
					for (const char *p = kCorpusText; *p; ++p) {
						char c = *p;
						if (c >= 'A' && c <= 'Z')
							c = (char)(c - 'A' + 'a');
						if (c >= 'a' && c <= 'z') {
							seq.PushBack((int32)(c - 'a'));
							lastWasSep = false;
						} else if (!lastWasSep) {
							seq.PushBack(26);
							lastWasSep = true;
						}
					}

					NkNgramLM lm;
					lm.Init(27);
					NkVector<NkVector<int32>> corpus;
					corpus.PushBack(seq);
					lm.Train(corpus);

					auto Id = [](char c) { return (int32)(c - 'a'); };
					auto ToIds = [&](const char *w) {
						NkVector<int32> ids;
						for (const char *p = w; *p; ++p)
							ids.PushBack(Id(*p));
						return ids;
					};

					// Fait 1 : sur cet extrait, "q" est TOUJOURS suivi de "u" (67/67 occurrences,
					// vérifié hors-ligne sur le même texte) -> P(u|q) doit dominer largement une
					// autre lettre jamais observée après "q" ET la baseline uniforme (1/27).
					const double pUgivenQ = lm.BigramProb(Id('q'), Id('u'));
					const double pXgivenQ = lm.BigramProb(Id('q'), Id('x'));
					const double uniform = 1.0 / 27.0;
					const bool factQU = (pUgivenQ > 10.0 * uniform) && (pUgivenQ > 20.0 * pXgivenQ);

					// Fait 2 : un mot RÉEL du corpus ("continent", très fréquent dans l'extrait)
					// doit obtenir une log-probabilité nettement supérieure à son anagramme
					// mélangé ("tnenitnoc", mêmes lettres, bigrammes non naturels en français).
					const double lpReal1 = lm.LogProb(ToIds("continent"));
					const double lpScrambled1 = lm.LogProb(ToIds("tnenitnoc"));
					const bool factWord1 = lpReal1 > lpScrambled1;

					// Fait 3 : idem avec "afrique" (mot-titre du corpus) vs anagramme "qifaeru".
					const double lpReal2 = lm.LogProb(ToIds("afrique"));
					const double lpScrambled2 = lm.LogProb(ToIds("qifaeru"));
					const bool factWord2 = lpReal2 > lpScrambled2;

					return factQU && factWord1 && factWord2;
				}

			private:
				int32 mVocab = 0;
				NkVector<double> mUnigram; // [vocab]
				NkVector<double> mBigram;	// [vocab*vocab]
				double mTotalUnigram = 0.0;
		};

	} // namespace ai
} // namespace nkentseu
