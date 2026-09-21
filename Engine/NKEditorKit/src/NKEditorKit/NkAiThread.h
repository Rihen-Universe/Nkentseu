#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiThread.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LE CONTRAT DE DONNEES DU FIL DU PANNEAU IA — des blocs types, ce
//          qu'un porteur declare savoir produire, et le refus de tout le reste.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// LA DEMANDE QUI A PRODUIT CE FICHIER (Rodolf, 20/09/2026)
//   « que ce soit nk code ou nk3dmodeler ou nkuidesign ou n'importe quel de nos
//     app qui utilise une ia, je veux que le panneau soit exactement comme ceci,
//     donc meme design, meme emplacement dans sa pastille, meme comportement. »
//
//   Et la contrainte qui en decoule, qui n'est pas du gout mais de
//   l'architecture : « meme design » ne se tient pas par de la discipline, ca se
//   tient par du CODE COMMUN. Trois panneaux conformes aujourd'hui divergent en
//   un mois. Le fil vit donc ICI, dans le kit, et les applications le
//   REMPLISSENT -- elles ne le redessinent pas.
//
// ⚠️ CE FICHIER NE PEINT RIEN, ET C'EST VOULU.
//    Il ne connait ni `NkGuiDrawList`, ni fonte, ni rectangle. Le peintre vient
//    au lot suivant et lira ces donnees. Separer les deux a un cout (deux
//    fichiers au lieu d'un) et un gain qui le paie : le fil se MESURE SANS
//    FENETRE. Tout ce qui est ci-dessous est eprouve par un banc console.
//
// ═══════════════════════════════════════════════════════════════════════════
//  LA REGLE QUI GOUVERNE TOUT LE FICHIER, ET ELLE VIENT DE DEUX ENDROITS
// ═══════════════════════════════════════════════════════════════════════════
//   *On ne montre pas un element qui ne peut rien dire.*
//
//   Elle est arrivee ici par deux chemins qui ne se parlaient pas, ce qui est
//   la meilleure raison de la croire :
//
//   1. `NkBrushDesc.h` (19/09), sur les primitives de sculpture :
//      « AUCUNE PRIMITIVE N'EST DECLAREE ICI SANS ETRE IMPLEMENTEE. Le depot a
//        paye "108 widgets declares, 2 qui peignent" : une enumeration qui
//        annonce dix operations dont deux agissent est PIRE qu'une enumeration
//        de deux, parce qu'elle se lit comme un inventaire. »
//   2. `NKCode/Shell/NkAiPanel.h` (l. 5339), ecrit des jours plus tot, par un
//      autre chantier, sur les appels d'outil :
//      « Les evenements tool_use/tool_result ne sont PAS encore affiches (pas de
//        fausse carte d'outil qui ne ferait rien). »
//
//   Et Rodolf l'a vecue le matin du 20/09 : dix controles qui lui posaient des
//   questions qu'il ne s'etait jamais posees.
//
//   ⚠️ D'OU LA FORME DE CE CONTRAT, QUI EST SON SEUL POINT VRAIMENT NEUF :
//      un type de bloc EXISTE dans l'enumeration, mais un bloc de ce type ne
//      peut ENTRER dans un fil que si le porteur a DECLARE savoir le produire.
//      `Pousser` refuse les autres, AVEC LEUR MOTIF. Le contrat accueille donc
//      ce qui n'a pas encore de source -- la reflexion, par exemple -- sans que
//      ca puisse jamais apparaitre a l'ecran en decoration vide.
//
//      La difference avec « ne pas declarer du tout » est celle-ci : le jour ou
//      un dorsal emet vraiment de la reflexion, il pose un drapeau et rien
//      d'autre ne change. Le jour ou personne n'en emet, l'ecran n'en montre
//      pas. Les deux sont vrais en meme temps, ce qu'une enumeration seule ne
//      sait pas faire.
//
// ⚠️ CE QUE CE FICHIER NE TRANCHE PAS. Six elements de la capture n'ont aucun
//    support reel chez nous (compte d'agents, chronometre, historique,
//    reflexion, micro, selecteur de modele). Les dessiner ou non est une
//    decision de RODOLF, posee le 20/09 et non encore rendue. Ce fichier est
//    ecrit pour que les deux reponses restent possibles sans le rouvrir : rien
//    ici ne suppose qu'ils existent, rien ici ne les interdit.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES TYPES DE BLOC ───────────────────────────────────────────────────
		// APPEND-ONLY, pour la meme raison que `NkRole` : un type insere au milieu
		// decalerait les suivants, et un fil relu depuis un enregistrement lirait
		// les mauvais blocs.
		//
		// La liste vient de la capture du 20/09, relue bloc par bloc, PLUS deux
		// types qui ne s'y trouvent pas et que nos applications produisent
		// vraiment (`Refus`, `Effet`). On ajoute ce qu'on sait produire ; on
		// n'enleve pas ce que la capture montre au pretexte qu'on ne l'a pas.
		enum class NkAiBloc : uint8 {
			/// La demande de l'utilisateur. Encadree, en tete, JAMAIS repliee :
			/// c'est la question, et une question repliee n'est plus une question.
			Demande = 0,
			/// Une phrase de l'assistant. Peut porter du gras et des pastilles de
			/// code en ligne -- c'est de la mise en forme, elle ne peut pas mentir.
			Prose,
			/// Une etape d'outil : un nom, une phrase courte, une ENTREE et une
			/// SORTIE a chasse fixe. C'est le seul bloc a deux compartiments.
			Outil,
			/// Un REFUS NOMME, avec son motif. ⚠️ Ce n'est PAS un echec : une IA
			/// invente des verbes, c'est le cas normal, et l'afficher comme une
			/// panne apprendrait a l'utilisateur que l'outil est casse.
			Refus,
			/// Un ECHEC technique : le service est injoignable, le modele n'est pas
			/// installe, le processus est mort. Il porte le geste qui repare, pas
			/// seulement le symptome.
			Echec,
			/// L'EFFET MESURE d'une action sur le document (`faces 6 -> 384`), et
			/// son annulation. N'est pas dans la capture : vient de NK3DModeler, ou
			/// il est deja livre. C'est ce qui rend l'outil essayable sans risque.
			Effet,
			/// La REFLEXION du modele.
			/// ⚠️ AUCUN PRODUCTEUR AUJOURD'HUI, ET C'EST ECRIT ICI EXPRES. Le seul
			///    flux qui en emette vraiment est le NDJSON du CLI Claude, que
			///    NKCode lit deja sans l'afficher. NKConverse n'expose rien de tel,
			///    et le panneau du modeleur n'est meme pas encore une conversation.
			///    En fabriquer serait du theatre : personne ne pense derriere.
			///    Ce type existe pour que le jour venu rien ne soit a rouvrir ; en
			///    attendant, aucun porteur ne le declare, donc `Pousser` le refuse.
			///    CONDITION DE RETRAIT DE CET AVERTISSEMENT : le jour ou un dorsal
			///    pose `produitReflexion`, ces six lignes partent.
			Reflexion,

			Count
		};

		/// Cle STABLE du type, pour un fil enregistre et pour les journaux. Comme
		/// pour les roles de theme : l'enumeration est l'acces du CODE, le nom est
		/// l'acces du FICHIER, et les deux servent.
		inline const char *NkAiBlocNom(NkAiBloc t) {
			switch (t) {
				case NkAiBloc::Demande:	  return "demande";
				case NkAiBloc::Prose:	  return "prose";
				case NkAiBloc::Outil:	  return "outil";
				case NkAiBloc::Refus:	  return "refus";
				case NkAiBloc::Echec:	  return "echec";
				case NkAiBloc::Effet:	  return "effet";
				case NkAiBloc::Reflexion: return "reflexion";
				default:				  return "";
			}
		}

		// ── CE QUE LE PORTEUR DECLARE SAVOIR PRODUIRE ───────────────────────────
		// ⚠️ TOUT EST A `false` PAR DEFAUT, ET CE N'EST PAS UNE PRECAUTION.
		//    Un defaut permissif ferait qu'une application qui oublie de declarer
		//    obtiendrait tout -- c'est-a-dire que l'oubli passerait inapercu, ce
		//    qui est exactement le defaut qu'on cherche a rendre impossible. Ici
		//    l'oubli donne un fil vide avec son motif : il se voit en une seconde.
		struct NkAiCapacites {
				/// Le porteur sait montrer une demande et une reponse en prose.
				/// Aucune application n'en est depourvue ; c'est quand meme declare,
				/// parce qu'un contrat a exception est un contrat qu'on cesse de
				/// lire.
				bool produitProse = false;
				/// Le porteur sait rapporter l'entree ET la sortie d'un outil.
				/// NK3DModeler : oui, livre. NKCode : la source existe (NDJSON), le
				/// bloc pas encore. NKUIDesign : non.
				bool produitOutil = false;
				/// Le porteur distingue un refus nomme d'une panne.
				bool produitRefus = false;
				/// Le porteur rapporte un echec avec le geste qui repare.
				bool produitEchec = false;
				/// Le porteur MESURE l'effet d'une action sur son document, et sait
				/// l'annuler. NK3DModeler : oui. Les deux autres : non.
				bool produitEffet = false;
				/// ⚠️ Aucun porteur ne le pose aujourd'hui. Voir `NkAiBloc::Reflexion`.
				bool produitReflexion = false;

				/// Commodite : ce que produit un porteur qui n'a qu'une conversation
				/// en texte. C'est le socle commun aux trois applications.
				static NkAiCapacites Texte() {
					NkAiCapacites c;
					c.produitProse = true;
					c.produitEchec = true;
					return c;
				}

				/// Le type `t` peut-il entrer dans ce fil ?
				/// ⚠️ `Demande` n'est jamais soumise a declaration : elle vient de
				///    l'UTILISATEUR, pas du modele. Un porteur qui ne saurait pas
				///    afficher ce que l'utilisateur vient de taper n'est pas un
				///    porteur degrade, c'est un defaut.
				bool Accepte(NkAiBloc t) const {
					switch (t) {
						case NkAiBloc::Demande:	  return true;
						case NkAiBloc::Prose:	  return produitProse;
						case NkAiBloc::Outil:	  return produitOutil;
						case NkAiBloc::Refus:	  return produitRefus;
						case NkAiBloc::Echec:	  return produitEchec;
						case NkAiBloc::Effet:	  return produitEffet;
						case NkAiBloc::Reflexion: return produitReflexion;
						default:				  return false;
					}
				}
		};

		// ── UN BLOC ─────────────────────────────────────────────────────────────
		struct NkAiBlocDonnees {
				/// IDENTIFIANT STABLE, pose par `Pousser` et jamais reutilise.
				/// ⚠️ C'EST LUI QUI DESIGNE, JAMAIS LA POSITION. La fenetre
				///    glissante decale les indices sans que personne n'insere rien :
				///    un indice releve au clic peut designer un autre bloc a
				///    l'image suivante. 0 = pas encore entre dans un fil.
				uint32 id = 0;
				NkAiBloc type = NkAiBloc::Prose;
				/// Le nom de l'outil (`Bash`), ou vide. Court, il se lit en gras.
				NkString titre;
				/// La phrase courte a cote du titre, ou le corps de la prose.
				NkString texte;
				/// Compartiment d'ENTREE (`IN`), a chasse fixe. Vide hors `Outil`.
				NkString entree;
				/// Compartiment de SORTIE (`OUT`), a chasse fixe. Vide hors `Outil`.
				NkString sortie;
				/// Le MOTIF d'un `Refus` ou d'un `Echec`. ⚠️ Un refus sans motif est
				/// refuse a l'entree : « ca n'a pas marche » n'apprend rien, et c'est
				/// le defaut que le modeleur a paye le 20/09 au matin (Ollama eteint
				/// annonce comme un fichier manquant).
				NkString motif;
				/// `Effet` : ce que l'action a change, deja mis en forme par le
				/// porteur -- lui seul sait ce qu'il compte (faces, noeuds, lignes).
				NkString effet;
				/// Replie par defaut, sauf `Demande`. C'est la capture : le fil est
				/// une SUITE DE LIGNES, et on deplie ce qu'on veut lire.
				bool replie = true;
				// ── 21/09, Q8 (Rodolf : « pourquoi IN et OUT alors qu'on doit avoir
				//    Read, Write, Design, Wireframe, Esquisse… ») ──
				/// Les ETIQUETTES des deux compartiments, declarees par l'HOTE : vides =
				/// « IN » / « OUT », le vocabulaire de NKCode (des commandes). Le titre
				/// du bloc porte la NATURE de l'action (Lire, Design, Esquisse…).
				NkString etiquetteEntree;
				NkString etiquetteSortie;
				/// La sortie est un RESULTAT EN CLAIR (noeuds crees, faces changees) et
				/// non du code : police du texte, pas la chasse fixe.
				bool sortieEnClair = false;
				/// LA VIGNETTE DU RESULTAT, tracee : des rectangles normalises (0..1)
				/// dans le cadre de ce qui a ete pose. Vide = pas de vignette.
				struct Vignette {
						float32 x = 0.f, y = 0.f, w = 0.f, h = 0.f;
						uint8 genre = 0; ///< 0 cadre, 1 texte, 2 composant, 3 bouton
				};
				NkVector<Vignette> vignette;
				/// (Q8) LES IMAGES JOINTES a une demande : leurs chemins. Le fil les
				/// montre en vignettes dans la demande encadree.
				NkVector<NkString> images;
				float32 vignetteRapport = 0.75f; ///< hauteur / largeur du cadre pose
		};

		// ── LE FIL ──────────────────────────────────────────────────────────────
		// Fenetre glissante bornee : on perd le plus ancien, jamais le plus recent.
		// Le panneau du modeleur le fait deja ; la regle remonte ici pour que les
		// trois applications aient la meme, et non trois plafonds differents.
		class NkAiFil {
			public:
				/// Plafond par defaut. Un fil sans plafond finit par couter une
				/// image ; un plafond trop court efface le contexte sous les yeux.
				static const uint32 kPlafondDefaut = 200;

				void Declarer(const NkAiCapacites &c) {
					mCap = c;
				}
				const NkAiCapacites &Capacites() const {
					return mCap;
				}

				/// Pousse un bloc. Rend `true` s'il est entre.
				/// ⚠️ EN CAS DE REFUS, `pourquoi` DIT LAQUELLE DES DEUX RAISONS, et
				///    ce n'est pas du confort : « le porteur ne produit pas ce
				///    type » et « ce bloc est incomplet » demandent deux corrections
				///    opposees. Un booleen seul enverrait chercher la mauvaise.
				bool Pousser(const NkAiBlocDonnees &b, NkString &pourquoi) {
					if (!mCap.Accepte(b.type)) {
						pourquoi = NkString("le porteur ne declare pas produire de bloc « ");
						pourquoi.Append(NkAiBlocNom(b.type));
						pourquoi.Append(" » : il n'entre pas dans le fil plutot que d'y "
										"apparaitre vide");
						return false;
					}
					if (!Complet(b, pourquoi))
						return false;
					NkAiBlocDonnees c = b;
					// L'identifiant est pose ICI et nulle part ailleurs : un bloc
					// construit par l'appelant n'en a pas, et c'est l'entree dans le
					// fil qui le lui donne. `mProchainId` ne redescend jamais, meme
					// apres `Vider` -- un identifiant reutilise apres une nouvelle
					// conversation ferait basculer un bloc de l'ancienne.
					c.id = ++mProchainId;
					// La demande n'est JAMAIS repliee : c'est la question.
					if (c.type == NkAiBloc::Demande)
						c.replie = false;
					mBlocs.PushBack(c);
					// ⚠️ LE PLAFOND S'APPLIQUE APRES L'AJOUT, PAS AVANT. Applique
					//    avant, un fil plein REFUSERAIT le bloc neuf -- c'est-a-dire
					//    perdrait le plus RECENT, l'inverse exact de ce qu'une
					//    fenetre glissante doit faire.
					while (mBlocs.Size() > mPlafond)
						mBlocs.Erase(mBlocs.Begin());
					++mPousses;
					return true;
				}

				/// Combien de blocs le panneau n'a pas encore PEINTS.
				/// ⚠️ ELLE SE CONSOMME LA OU LES BLOCS SONT PEINTS, jamais au clic.
				///    Consommee au clic, elle marquerait « vu » un panneau que ce
				///    clic venait peut-etre de FERMER. La lecon est celle de
				///    `d31c127e9` (NK3DModeler, 20/09) et elle remonte ici telle
				///    quelle -- c'est le sens meme de mettre le fil en commun.
				uint32 NonVus() const {
					return mPousses > mVus ? mPousses - mVus : 0u;
				}
				void MarquerVus() {
					mVus = mPousses;
				}

				uint32 Taille() const {
					return (uint32)mBlocs.Size();
				}
				const NkAiBlocDonnees &At(uint32 i) const {
					return mBlocs[i];
				}
				// ⚠️ CORRECTIF DU 20/09 AU SOIR — J'AI LIVRE CETTE FAUTE LA VEILLE.
				//    `Basculer(uint32 i)` designait un bloc PAR SA POSITION. Et la
				//    fenetre glissante de ce meme fichier DECALE toutes les
				//    positions : des qu'un bloc entre dans un fil plein, `Erase` sort
				//    le plus ancien et l'indice 3 ne designe plus le meme bloc.
				//
				//    Le chemin exact du defaut : le peintre releve « l'utilisateur a
				//    clique le bloc 3 », une reponse arrive dans le meme intervalle,
				//    le fil glisse, et c'est un AUTRE bloc qui se deplie. Rien ne le
				//    dit -- l'utilisateur croit avoir mal vise.
				//
				//    C'est la faute que le depot a payee TROIS FOIS le 20/09 sur le
				//    chantier sculpture (pastille du rail, catalogue de brosses,
				//    brosse active), et une de plus sur `--demo=2`. *Un indice est
				//    vrai jusqu'a ce que quelqu'un insere quelque chose avant.* Ici
				//    PERSONNE N'INSERE : le fil se vide par le haut tout seul. C'est
				//    pire, parce qu'aucune relecture de code ne montre l'insertion
				//    coupable -- il n'y en a pas.
				//
				//    Chaque bloc porte donc un IDENTIFIANT STABLE, pose a l'entree et
				//    jamais reutilise. `At(i)` reste -- peindre PARCOURT le fil dans
				//    l'ordre, c'est legitime -- mais tout ce qui DESIGNE passe par le
				//    nom.
				void BasculerParId(uint32 id) {
					for (usize i = 0; i < mBlocs.Size(); ++i)
						if (mBlocs[i].id == id) {
							if (mBlocs[i].type != NkAiBloc::Demande)
								mBlocs[i].replie = !mBlocs[i].replie;
							return;
						}
					// Aucun bloc sous cet identifiant : il a quitte la fenetre. On ne
					// bascule RIEN plutot qu'un voisin -- un repli au hasard serait
					// pire qu'un geste sans effet, l'utilisateur croirait avoir vu.
				}
				/// Le bloc portant cet identifiant, MODIFIABLE, ou `nullptr` s'il a
				/// quitte la fenetre.
				/// ⚠️ IL EXISTE POUR UNE RAISON PRECISE : une mesure qui arrive APRES
				///    la pose du bloc. Le modeleur pousse son operation, puis lit les
				///    compteurs a l'image suivante -- lire tout de suite rendrait l'etat
				///    d'AVANT en le presentant comme celui d'apres.
				/// ⚠️ ET IL REND `nullptr` PLUTOT QU'UN VOISIN. C'est la meme politique
				///    que `BasculerParId` : perdre la mesure d'un bloc sorti de la
				///    fenetre vaut mieux que l'ecrire sur un autre -- un chiffre juste
				///    sur la mauvaise ligne est indetectable a l'oeil.
				NkAiBlocDonnees *MutableParId(uint32 id) {
					if (id == 0u)
						return nullptr;
					for (usize i = 0; i < mBlocs.Size(); ++i)
						if (mBlocs[i].id == id)
							return &mBlocs[i];
					return nullptr;
				}
				/// Rend `false` si l'identifiant a quitte la fenetre glissante. Le
				/// peintre s'en sert pour ne pas dessiner un survol sur un disparu.
				bool TrouverParId(uint32 id, uint32 &indexOut) const {
					for (usize i = 0; i < mBlocs.Size(); ++i)
						if (mBlocs[i].id == id) {
							indexOut = (uint32)i;
							return true;
						}
					return false;
				}
				/// ⚠️ `Vider` NE REMET PAS `NonVus()` A ZERO EN LE MENTANT : il
				///    aligne les deux compteurs. Une nouvelle conversation n'a rien
				///    a faire voir, donc rien n'est en attente -- et la pastille ne
				///    doit pas rester marquee pour des blocs qui n'existent plus.
				void Vider() {
					mBlocs.Clear();
					mVus = mPousses;
				}
				void PoserPlafond(uint32 p) {
					mPlafond = p < 1u ? 1u : p;
					while (mBlocs.Size() > mPlafond)
						mBlocs.Erase(mBlocs.Begin());
				}

			private:
				/// Un bloc doit porter ce que son type promet. Sinon il entrerait et
				/// se peindrait VIDE -- exactement la decoration qu'on refuse.
				static bool Complet(const NkAiBlocDonnees &b, NkString &pourquoi) {
					switch (b.type) {
						case NkAiBloc::Demande:
						case NkAiBloc::Prose:
						case NkAiBloc::Reflexion:
							if (b.texte.Length() == 0) {
								pourquoi = NkString("un bloc « ");
								pourquoi.Append(NkAiBlocNom(b.type));
								pourquoi.Append(" » sans texte n'a rien a dire");
								return false;
							}
							return true;
						case NkAiBloc::Outil:
							// Le TITRE est du, pas les deux compartiments : un outil
							// qui n'a pas encore rendu sa sortie est un etat REEL du
							// fil, et l'interdire empecherait de montrer l'attente.
							if (b.titre.Length() == 0) {
								pourquoi = NkString("un bloc « outil » sans nom d'outil "
													"ne se distingue d'aucun autre");
								return false;
							}
							return true;
						case NkAiBloc::Refus:
						case NkAiBloc::Echec:
							if (b.motif.Length() == 0) {
								pourquoi = NkString("un bloc « ");
								pourquoi.Append(NkAiBlocNom(b.type));
								pourquoi.Append(" » sans motif n'apprend rien : « ca n'a pas "
												"marche » envoie chercher au hasard");
								return false;
							}
							return true;
						case NkAiBloc::Effet:
							if (b.effet.Length() == 0) {
								pourquoi = NkString("un bloc « effet » sans mesure serait un "
													"compteur a zero, pire que son absence");
								return false;
							}
							return true;
						default:
							pourquoi = NkString("type de bloc inconnu");
							return false;
					}
				}

				NkVector<NkAiBlocDonnees> mBlocs;
				NkAiCapacites mCap;
				uint32 mPlafond = kPlafondDefaut;
				uint32 mProchainId = 0;
				uint32 mPousses = 0;
				uint32 mVus = 0;
		};

	} // namespace editorkit
} // namespace nkentseu
