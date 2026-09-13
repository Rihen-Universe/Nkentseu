#pragma once
// -----------------------------------------------------------------------------
// @File    NkNodeGraph.inl
// @Brief   Implantation du coeur de graphe. Incluse par NkNodeGraph.h — le module
//          reste EN-TETE PUR tant qu'aucun consommateur ne le lie.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

namespace nkentseu {
	namespace graph {

		namespace detail {
			inline bool GraphStrEq(const NkString &a, const char *b) {
				const char *p = a.CStr();
				if (!p || !b)
					return false;
				while (*p && *b) {
					if (*p != *b)
						return false;
					++p;
					++b;
				}
				return *p == *b;
			}
		} // namespace detail

		inline const char *NkSocketFamilyName(NkSocketFamily f) {
			return f == NkSocketFamily::Exec ? "execution" : "donnee";
		}

		inline const char *NkLinkErrorName(NkLinkError e) {
			switch (e) {
				case NkLinkError::Ok:
					return "ok";
				case NkLinkError::UnknownNode:
					return "noeud-inconnu";
				case NkLinkError::UnknownSocket:
					return "socket-inconnu";
				case NkLinkError::SameNode:
					return "meme-noeud";
				case NkLinkError::DirectionMismatch:
					return "sens-invalide";
				case NkLinkError::TypeMismatch:
					return "type-incompatible";
				case NkLinkError::FamilyMismatch:
					return "familles-incompatibles";
				case NkLinkError::ExecOutputAlreadyBound:
					return "sortie-execution-deja-reliee";
				case NkLinkError::WouldCycle:
					return "cycle";
			}
			return "?";
		}

		inline int32 NkNode::FindSocket(const char *name, NkSocketDir dir) const {
			for (uint32 i = 0; i < (uint32)sockets.Size(); ++i)
				if (sockets[i].dir == dir && detail::GraphStrEq(sockets[i].name, name))
					return (int32)i;
			return -1;
		}

		// ── TYPES ───────────────────────────────────────────────────────────────
		// ── L'EMPREINTE DE STRUCTURE ─────────────────────────────────────────
		//
		// ⚠️ FNV-1a EN LARGEUR FIXE, ET C'EST TOUT L'INTERET. Le depot porte deja
		// un FNV-1a -- `NkHash<NkString>` dans NKContainers -- et le reutiliser
		// ici serait FAUX : il travaille en `usize`, donc 8 octets sur une
		// machine 64 bits et 4 sur une 32 bits, avec des constantes differentes.
		// Une empreinte ECRITE DANS UN FICHIER ne peut pas dependre de la
		// machine qui l'ecrit : un graphe sauve en 64 bits serait refuse au
		// chargement en 32 bits, pour une divergence qui n'existe pas.
		//
		// 📌 Regle 5 par un bout inhabituel : la convention EXISTE en dessous, et
		// il faut quand meme ne pas la prendre -- parce que mon usage
		// (persistance) a une exigence que le sien (table de hachage en memoire)
		// n'a pas. Aller voir ce qui existe ne veut pas dire s'en servir ; ca
		// veut dire savoir POURQUOI on s'en ecarte.
		namespace detail {
			// ⚠️ ECRITURE DE NOMBRE LOCALE, ET LA RAISON EST UN ORDRE D'INCLUSION.
			// `detail::PutU32` existe -- dans NkNodeGraphIO.inl, qui est inclus
			// APRES ce fichier. S'en servir ici compilerait ou non selon l'ordre
			// des inclusions chez l'appelant, ce qui est la pire forme de
			// dependance : elle marche jusqu'au jour ou quelqu'un reordonne.
			inline void NombreDansTexte(NkString &s, uint32 v) {
				char b[16];
				uint32 n = 0;
				if (v == 0)
					b[n++] = '0';
				while (v > 0 && n < 15) {
					b[n++] = (char)('0' + (v % 10));
					v /= 10;
				}
				for (uint32 i = 0; i < n; ++i)
					s.Append(b[n - 1 - i]);
			}

			inline void EmpreinteAvale(uint64 &h, const char *s) {
				if (!s)
					return;
				for (const char *p = s; *p; ++p) {
					h ^= (uint64)(uint8)(*p);
					h *= 1099511628211ULL; // FNV-1a 64, largeur FIXE
				}
			}

			// La forme canonique : le genre, puis chaque membre DANS L'ORDRE.
			//
			// ⚠️ L'ORDRE FAIT PARTIE DE L'EMPREINTE, et ce n'est pas un detail :
			// les valeurs d'une enumeration sont POSITIONNELLES. Permuter deux
			// enumerateurs ne renomme pas, ca change ce que valent les donnees
			// deja sauvees. Une empreinte insensible a l'ordre laisserait passer
			// exactement la corruption la plus silencieuse.
			//
			// Les separateurs ne sont pas decoratifs : sans eux, {"ab","c"} et
			// {"a","bc"} auraient la meme empreinte.
			inline uint64 EmpreinteDeStructure(NkTypeKind kind, const NkTypeMember *m, uint32 n) {
				uint64 h = 14695981039346656037ULL; // FNV-1a 64, decalage initial
				char g[2] = {(char)('0' + (int)kind), 0};
				EmpreinteAvale(h, g);
				EmpreinteAvale(h, "|");
				for (uint32 i = 0; i < n; ++i) {
					EmpreinteAvale(h, m[i].name.CStr());
					EmpreinteAvale(h, ":");
					EmpreinteAvale(h, m[i].type.CStr());
					EmpreinteAvale(h, ";");
				}
				return h;
			}
			// ⚠️ « LES DEUX NE CORRESPONDENT PAS » NE SUFFIT PAS. Un refus qui ne
			// dit pas QUOI force a ouvrir deux fichiers et a les comparer a la
			// main -- et sur une enumeration de trente entrees, personne ne le
			// fait correctement. On nomme donc la PREMIERE divergence, dans
			// l'ordre ou quelqu'un la chercherait : le genre, puis le nombre de
			// membres, puis le premier membre qui differe.
			//
			// On s'arrete au PREMIER ecart plutot que de tout lister : au-dela,
			// les differences suivantes sont souvent des consequences du
			// decalage, et une liste de vingt lignes se lit moins bien qu'une.
			inline NkString DecrisDivergence(const NkVector<NkTypeMember> &a, NkTypeKind ka,
											 const NkTypeMember *b, uint32 nb, NkTypeKind kb) {
				NkString q;
				if (ka != kb) {
					q.Append("le genre differe : ");
					q.Append(NkTypeKindName(ka));
					q.Append(" ici, ");
					q.Append(NkTypeKindName(kb));
					q.Append(" la");
					return q;
				}
				const uint32 na = (uint32)a.Size();
				const uint32 n = na < nb ? na : nb;
				for (uint32 i = 0; i < n; ++i) {
					if (!(a[i].name == b[i].name)) {
						q.Append("le membre ");
						NombreDansTexte(q, i);
						q.Append(" s'appelle « ");
						q.Append(a[i].name);
						q.Append(" » ici et « ");
						q.Append(b[i].name);
						q.Append(" » la");
						return q;
					}
					if (!(a[i].type == b[i].type)) {
						q.Append("le membre « ");
						q.Append(a[i].name);
						q.Append(" » porte le type « ");
						q.Append(a[i].type.Size() ? a[i].type : NkString("(sans objet)"));
						q.Append(" » ici et « ");
						q.Append(b[i].type.Size() ? b[i].type : NkString("(sans objet)"));
						q.Append(" » la");
						return q;
					}
				}
				if (na != nb) {
					// Le cas `Rihen::Difficulte` : meme debut, un membre en plus.
					q.Append(na < nb ? "il manque " : "il y a en trop ");
					NombreDansTexte(q, na < nb ? (nb - na) : (na - nb));
					q.Append(" membre(s) -- ");
					NombreDansTexte(q, na);
					q.Append(" ici, ");
					NombreDansTexte(q, nb);
					q.Append(" la");
					const NkTypeMember *sup = (na < nb) ? &b[na] : &a[na < nb ? 0 : nb];
					q.Append(" ; le premier en plus est « ");
					q.Append(sup->name);
					q.Append(" »");
					return q;
				}
				// Meme genre, memes membres, empreintes differentes : impossible
				// par construction. On le dit au lieu de rendre un message vide.
				return NkString("les empreintes different alors que genre et membres concordent -- "
								"incoherence interne du registre, a signaler");
			}

		} // namespace detail

		inline const char *NkTypeKindName(NkTypeKind k) {
			switch (k) {
				case NkTypeKind::Leaf:
					return "feuille";
				case NkTypeKind::Enum:
					return "enumeration";
				case NkTypeKind::Struct:
					return "structure";
				case NkTypeKind::Union:
					return "union";
			}
			return "?";
		}

		inline bool NkNodeGraph::NomQualifieValide(const char *n) {
			if (!n || !*n)
				return false;
			// Un segment : lettre ou souligne, puis lettres/chiffres/souligne.
			// Les segments se separent par `::`. Ni segment vide, ni `::` final.
			const char *p = n;
			for (;;) {
				const bool debutOk = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_';
				if (!debutOk)
					return false;
				++p;
				while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
					++p;
				if (*p == 0)
					return true;
				if (p[0] != ':' || p[1] != ':')
					return false;
				p += 2;
			}
		}

		inline void NkNodeGraph::SepareNomQualifie(const char *n, NkString *outEspace, NkString *outSimple) {
			if (outEspace)
				*outEspace = NkString("");
			if (outSimple)
				*outSimple = NkString(n ? n : "");
			if (!n)
				return;
			int32 dernier = -1;
			for (int32 i = 0; n[i]; ++i)
				if (n[i] == ':' && n[i + 1] == ':')
					dernier = i;
			if (dernier < 0)
				return;
			NkString esp, simple;
			for (int32 i = 0; i < dernier; ++i)
				esp.Append(n[i]);
			for (const char *p = n + dernier + 2; *p; ++p)
				simple.Append(*p);
			if (outEspace)
				*outEspace = esp;
			if (outSimple)
				*outSimple = simple;
		}

		inline NkTypeId NkNodeGraph::RegisterType(const char *name) {
			const NkTypeId existing = FindType(name);
			if (existing != NK_TYPE_INVALID)
				return existing; // idempotent : deux consommateurs peuvent declarer le meme
			if (mTypeNames.Empty())
				mTypeNames.PushBack(NkString("")); // l'index 0 reste « invalide »
			while (mTypeDefs.Size() < mTypeNames.Size())
				mTypeDefs.PushBack(TypeDef());
			mTypeNames.PushBack(NkString(name ? name : ""));
			mTypeDefs.PushBack(TypeDef()); // feuille : pas d'empreinte, pas zero
			return (NkTypeId)(mTypeNames.Size() - 1);
		}

		inline NkTypeId NkNodeGraph::RegisterCompositeType(const char *name, NkTypeKind kind,
														   const NkTypeMember *members, uint32 count,
														   NkString *outErreur) {
			if (outErreur)
				*outErreur = NkString("");
			auto refuse = [&](const NkString &q) {
				if (outErreur)
					*outErreur = q;
				return NK_TYPE_INVALID;
			};
			if (!name || !*name)
				return refuse(NkString("type composite sans nom"));
			if (kind == NkTypeKind::Leaf)
				return refuse(NkString("un type composite ne peut pas etre declare « feuille » : une feuille "
									   "n'a pas de membres, et son nom EST sa definition"));

			const uint64 emp = detail::EmpreinteDeStructure(kind, members, count);
			const NkTypeId existant = FindType(name);
			if (existant != NK_TYPE_INVALID) {
				// ⚠️ IDEMPOTENT SI IDENTIQUE, REFUS NOMME SINON. Ecraser ferait
				// dependre le sens du graphe de l'ORDRE d'enregistrement -- deux
				// consommateurs, deux definitions, et le dernier gagne en
				// silence. C'est le cas `Rihen::Difficulte` de la decision.
				const TypeDef &d = mTypeDefs[existant];
				if (d.aEmpreinte && d.empreinte == emp)
					return existant;
				NkString q("le type « ");
				q.Append(name);
				q.Append(" » est deja declare avec une AUTRE definition -- ");
				q.Append(detail::DecrisDivergence(d.members, d.kind, members, count, kind));
				return refuse(q);
			}

			if (mTypeNames.Empty())
				mTypeNames.PushBack(NkString(""));
			while (mTypeDefs.Size() < mTypeNames.Size())
				mTypeDefs.PushBack(TypeDef());
			mTypeNames.PushBack(NkString(name));
			TypeDef d;
			d.kind = kind;
			for (uint32 i = 0; i < count; ++i)
				d.members.PushBack(members[i]);
			d.empreinte = emp;
			d.aEmpreinte = true;
			mTypeDefs.PushBack(d);
			return (NkTypeId)(mTypeNames.Size() - 1);
		}

		inline NkTypeKind NkNodeGraph::TypeKind(NkTypeId t) const {
			return t < (NkTypeId)mTypeDefs.Size() ? mTypeDefs[t].kind : NkTypeKind::Leaf;
		}

		inline bool NkNodeGraph::TypeFingerprint(NkTypeId t, uint64 *out) const {
			if (t >= (NkTypeId)mTypeDefs.Size() || !mTypeDefs[t].aEmpreinte)
				return false; // FEUILLE : pas d'empreinte. Pas une empreinte nulle.
			if (out)
				*out = mTypeDefs[t].empreinte;
			return true;
		}

		inline uint32 NkNodeGraph::TypeMemberCount(NkTypeId t) const {
			return t < (NkTypeId)mTypeDefs.Size() ? (uint32)mTypeDefs[t].members.Size() : 0;
		}

		inline const NkTypeMember *NkNodeGraph::TypeMemberAt(NkTypeId t, uint32 i) const {
			if (t >= (NkTypeId)mTypeDefs.Size() || i >= (uint32)mTypeDefs[t].members.Size())
				return nullptr;
			return &mTypeDefs[t].members[i];
		}

		inline NkTypeId NkNodeGraph::FindType(const char *name) const {
			for (uint32 i = 1; i < (uint32)mTypeNames.Size(); ++i)
				if (detail::GraphStrEq(mTypeNames[i], name))
					return (NkTypeId)i;
			return NK_TYPE_INVALID;
		}

		inline const NkString *NkNodeGraph::TypeName(NkTypeId t) const {
			return (t != NK_TYPE_INVALID && t < (NkTypeId)mTypeNames.Size()) ? &mTypeNames[t] : nullptr;
		}

		inline void NkNodeGraph::AllowConversion(NkTypeId from, NkTypeId to) {
			if (from == NK_TYPE_INVALID || to == NK_TYPE_INVALID || from == to)
				return;
			const uint64 k = ((uint64)from << 32) | (uint64)to;
			for (uint32 i = 0; i < (uint32)mConversions.Size(); ++i)
				if (mConversions[i] == k)
					return;
			mConversions.PushBack(k);
		}

		inline bool NkNodeGraph::Accepts(NkTypeId socketType, NkTypeId valueType) const {
			if (socketType == valueType)
				return true;
			// La conversion est DIRIGEE : autoriser flottant -> vecteur n'autorise pas
			// vecteur -> flottant. Une conversion symetrique perdrait de l'information
			// dans un sens sans le dire.
			const uint64 k = ((uint64)valueType << 32) | (uint64)socketType;
			for (uint32 i = 0; i < (uint32)mConversions.Size(); ++i)
				if (mConversions[i] == k)
					return true;
			return false;
		}

		// ── NOEUDS ──────────────────────────────────────────────────────────────
		inline NkNodeId NkNodeGraph::AddNode(const char *type, const char *label) {
			NkNode n;
			n.id = mNextNode++;
			n.type = NkString(type ? type : "");
			n.label = NkString(label ? label : (type ? type : ""));
			n.alive = true;
			mNodes.PushBack(n);
			return n.id;
		}

		inline NkNode *NkNodeGraph::Find(NkNodeId id) {
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive && mNodes[i].id == id)
					return &mNodes[i];
			return nullptr;
		}

		inline const NkNode *NkNodeGraph::Find(NkNodeId id) const {
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive && mNodes[i].id == id)
					return &mNodes[i];
			return nullptr;
		}

		inline uint32 NkNodeGraph::NodeCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive)
					n++;
			return n;
		}

		inline bool NkNodeGraph::AddSocket(NkNodeId id, const char *name, NkTypeId type, NkSocketDir dir,
										   NkSocketFamily family) {
			NkNode *n = Find(id);
			if (!n || !name || type == NK_TYPE_INVALID)
				return false;
			if (n->FindSocket(name, dir) >= 0)
				return false; // deux sockets homonymes dans la meme direction : ambigu
			NkSocket s;
			s.name = NkString(name);
			s.type = type;
			s.dir = dir;
			s.family = family;
			n->sockets.PushBack(s);
			return true;
		}

		inline bool NkNodeGraph::RemoveNode(NkNodeId id) {
			NkNode *n = Find(id);
			if (!n)
				return false;
			n->alive = false;
			// Les liens incidents meurent AVEC le noeud. Un lien pendant ferait
			// paraitre le graphe valide alors qu'il s'evaluerait faux — exactement le
			// genre de defaut qui ne se voit qu'au resultat.
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && (mLinks[i].fromNode == id || mLinks[i].toNode == id))
					mLinks[i].alive = false;
			return true;
		}

		// ── CONNEXIONS ──────────────────────────────────────────────────────────
		inline uint32 NkNodeGraph::LinkCount() const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive)
					n++;
			return n;
		}

		inline const NkLink *NkNodeGraph::LinkAt(uint32 idx) const {
			uint32 n = 0;
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				if (!mLinks[i].alive)
					continue;
				if (n++ == idx)
					return &mLinks[i];
			}
			return nullptr;
		}

		inline const NkLink *NkNodeGraph::IncomingOf(NkNodeId id, int32 socketIndex) const {
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].toNode == id && mLinks[i].toSocket == socketIndex)
					return &mLinks[i];
			return nullptr;
		}

		// ✅ LA FAMILLE D'UN LIEN -- ET ELLE NE COMMANDE PLUS L'ACYCLICITE.
		//
		// ⚠️ CE FICHIER A PORTE LE CONTRAIRE PENDANT UNE JOURNEE, et la trace
		// reste ici parce qu'elle explique le code qu'on lit. Le 23/08 au matin,
		// ce parcours et `TopoSort` ne suivaient QUE la famille `Data`, pour
		// qu'un rebouclage d'execution devienne exprimable. ❌ RETIRE le 23/08
		// au soir par Rodolf : « l'acyclicite reste UNIVERSELLE ».
		//
		//     UN CYCLE VIT A L'INTERIEUR D'UN NOEUD, JAMAIS DANS LE GRAPHE.
		//
		// Trois raisons, et la premiere est la seule qui compte vraiment :
		//   1. une regle SANS EXCEPTION est une regle que les outils n'ont pas a
		//      interroger. Le jour ou l'acyclicite depend de la famille du lien,
		//      TOUT ce qui parcourt un graphe doit savoir dans quelle famille il
		//      se trouve -- et ce chantier vient de passer trois jours sur des
		//      defauts causes par des choses qui NE SAVAIENT PAS a quelle
		//      famille elles appartenaient ;
		//   2. le tri topologique reste valide sur le graphe ENTIER. Autoriser un
		//      cycle QUELQUE PART, c'est perdre l'ordre defini PARTOUT ;
		//   3. tous les cas connus sont couverts sans fil qui revienne : boucle
		//      -> noeud de boucle ; machine a etats -> noeud de machine a etats.
		//      Le `For Loop` d'Unreal a une sortie « corps » et une sortie
		//      « termine », et LE CORPS NE REVIENT JAMAIS AU NOEUD PAR UN FIL :
		//      le noeud itere lui-meme. Meme regle, chez le moteur le plus
		//      employe du metier.
		//
		// 📌 CE QUE CETTE FONCTION SERT DONC ENCORE : la VUE. Un fil d'execution
		// ne se dessine pas comme un fil de donnee, et le canevas (couche 2) doit
		// pouvoir le demander sans relire les prises lui-meme. Elle n'a plus
		// aucun appelant DANS le coeur, et c'est voulu : le coeur ne consulte
		// plus la famille pour decider d'un cycle.
		inline NkSocketFamily NkNodeGraph::LinkFamily(const NkLink &l) const {
			// On lit la prise SOURCE. Les deux extremites s'accordent forcement --
			// `Connect` refuse le croisement -- donc l'une des deux suffit, et
			// choisir la source rend la reponse stable meme si la cible a ete
			// retiree entre-temps.
			const NkNode *n = Find(l.fromNode);
			if (!n || l.fromSocket < 0 || l.fromSocket >= (int32)n->sockets.Size())
				return NkSocketFamily::Data;
			return n->sockets[(uint32)l.fromSocket].family;
		}

		inline bool NkNodeGraph::WouldCreateCycle(NkNodeId from, NkNodeId to) const {
			// Existe-t-il DEJA un chemin de `to` vers `from` ? Si oui, ajouter
			// from -> to fermerait la boucle. Parcours en profondeur avec garde.
			if (from == to)
				return true;
			NkVector<NkNodeId> stack;
			NkVector<NkNodeId> seen;
			stack.PushBack(to);
			uint32 guard = 0;
			while (!stack.Empty() && guard++ < 100000u) {
				const NkNodeId cur = stack[(uint32)stack.Size() - 1];
				stack.PopBack();
				if (cur == from)
					return true;
				bool already = false;
				for (uint32 i = 0; i < (uint32)seen.Size(); ++i)
					if (seen[i] == cur) {
						already = true;
						break;
					}
				if (already)
					continue;
				seen.PushBack(cur);
				// ⚠️ TOUS LES LIENS VIVANTS, SANS REGARDER LEUR FAMILLE. C'est
				// l'acyclicite universelle, et c'est la seule ligne qui la porte.
				for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
					if (mLinks[i].alive && mLinks[i].fromNode == cur)
						stack.PushBack(mLinks[i].toNode);
			}
			return false;
		}

		inline NkLinkError NkNodeGraph::Connect(NkNodeId from, const char *fromSocket, NkNodeId to,
												const char *toSocket, NkLinkId *outId) {
			if (outId)
				*outId = 0;
			NkNode *a = Find(from);
			NkNode *b = Find(to);
			if (!a || !b)
				return NkLinkError::UnknownNode;
			if (from == to)
				return NkLinkError::SameNode;
			const int32 si = a->FindSocket(fromSocket, NkSocketDir::Output);
			const int32 di = b->FindSocket(toSocket, NkSocketDir::Input);
			// Un socket cherche dans la MAUVAISE direction est introuvable ; on
			// distingue tout de meme les deux cas, parce que « ce socket n'existe
			// pas » et « vous branchez une entree sur une entree » ne se corrigent
			// pas de la meme facon.
			if (si < 0 || di < 0) {
				const bool aHasIn = a->FindSocket(fromSocket, NkSocketDir::Input) >= 0;
				const bool bHasOut = b->FindSocket(toSocket, NkSocketDir::Output) >= 0;
				if (aHasIn || bHasOut)
					return NkLinkError::DirectionMismatch;
				return NkLinkError::UnknownSocket;
			}
			// ── LA FAMILLE SE COMPARE **AVANT** LE TYPE ──────────────────────
			// ⚠️ L'ORDRE EST LA MOITIE DE LA REGLE. Deux prises de familles
			// differentes portent tres souvent le MEME type -- dans le banc,
			// « apres » (exec) et « valeur » (donnee) sont toutes deux `reel`.
			// Comparer les types d'abord rendrait `Ok` sur un croisement, ou,
			// si les types differaient, rendrait `TypeMismatch` : l'auteur
			// chercherait une conversion, et il n'y en a pas a trouver.
			const NkSocketFamily fa = a->sockets[(uint32)si].family;
			const NkSocketFamily fb = b->sockets[(uint32)di].family;
			if (fa != fb)
				return NkLinkError::FamilyMismatch;

			// ⚠️ UN FIL D'EXECUTION NE TRANSPORTE RIEN, donc son type ne veut rien
			// dire et on ne le compare pas. Le comparer imposerait aux auteurs de
			// donner le meme type bidon a toutes les prises d'execution du
			// catalogue -- une contrainte inventee, que rien ne justifierait.
			if (fa == NkSocketFamily::Data &&
				!Accepts(b->sockets[(uint32)di].type, a->sockets[(uint32)si].type))
				return NkLinkError::TypeMismatch;

			// ── ACYCLICITE : UNIVERSELLE, SANS EXCEPTION ──────────────────
			// 🔴 AUCUNE CONDITION DE FAMILLE ICI. Cette ligne a porte
			// `fa == NkSocketFamily::Data &&` pendant une journee ; le voir
			// revenir, c'est voir revenir l'exception. Pourquoi elle est partie :
			// voir le pave devant `LinkFamily`, plus haut dans ce fichier.
			//
			// 📌 ET LA DEFENSE REDONDANTE A DISPARU AVEC L'EXCEPTION -- c'est
			// le gain qu'on n'attendait pas, et il vaut d'etre ecrit.
			//
			// L'exemption s'ecrivait a DEUX endroits : ce `fa == Data &&`, et le
			// filtre de famille dans le parcours de `WouldCreateCycle`. DEUX
			// encodages de la MEME regle, donc chacun invisible a une mutation a
			// un seul defaut : M36 retirait le premier et SURVIVAIT, parce que le
			// second refusait toujours de voir les liens d'execution. Il avait
			// fallu un COUPLE (M38) pour mesurer ce que chacun achetait.
			//
			// Les deux sont partis ensemble. Il reste UNE garde et UNE
			// implementation -- pas deux defenses -- et M36 comme M38 rougissent
			// desormais SEULES. Une regle sans exception se mesure aussi plus
			// simplement que la meme regle avec une exception ecrite deux fois.
			if (WouldCreateCycle(from, to))
				return NkLinkError::WouldCycle;

			// ── ARITE DE SORTIE ──────────────────────────────────────────────
			// DONNEE : autant de liens qu'on veut, une valeur se lit partout.
			// EXECUTION : UN SEUL -- une instruction n'a qu'une suite. Deux
			// suites seraient un branchement, et un branchement est un NOEUD, pas
			// un cablage ; l'accepter en silence rendrait l'ordre d'execution
			// dependant de l'ordre d'insertion des liens.
			if (fa == NkSocketFamily::Exec)
				for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
					if (mLinks[i].alive && mLinks[i].fromNode == from && mLinks[i].fromSocket == si)
						return NkLinkError::ExecOutputAlreadyBound;

			// ── ARITE D'ENTREE ───────────────────────────────────────────────
			// DONNEE : une seule source, l'ancienne est remplacee -- inchange.
			// EXECUTION : PLUSIEURS sources tiennent. Dix chemins peuvent mener
			// au meme noeud, et remplacer serait perdre neuf branches sans un mot.
			if (fb == NkSocketFamily::Data)
				for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
					if (mLinks[i].alive && mLinks[i].toNode == to && mLinks[i].toSocket == di)
						mLinks[i].alive = false;

			NkLink l;
			l.id = mNextLink++;
			l.fromNode = from;
			l.fromSocket = si;
			l.toNode = to;
			l.toSocket = di;
			l.alive = true;
			mLinks.PushBack(l);
			if (outId)
				*outId = l.id;
			return NkLinkError::Ok;
		}

		// ── QUALIFIER UN LIEN ───────────────────────────────────────────────────
		// ⚠️ AUCUN CODE NEUF DE STOCKAGE DE VALEUR : on reutilise `NkGraphProp`,
		// celui des noeuds. La specification du § 20.3 supposait qu'il fallait
		// l'inventer (« il n'existe AUCUNE valeur dans NKGraph, pour rien ») --
		// il existait deja, et servait les proprietes de noeud et les defauts de
		// prise depuis le debut. Un second mecanisme aurait diverge du premier au
		// premier changement de format.
		inline NkLink *NkNodeGraph::TrouveLien(NkLinkId id) {
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].id == id)
					return &mLinks[i];
			return nullptr;
		}

		inline const NkLink *NkNodeGraph::TrouveLien(NkLinkId id) const {
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].id == id)
					return &mLinks[i];
			return nullptr;
		}

		inline bool NkNodeGraph::SetLinkProp(NkLinkId id, const char *name, const NkGraphValue &v) {
			NkLink *l = TrouveLien(id);
			if (!l || !name || !*name)
				return false;
			for (uint32 i = 0; i < (uint32)l->props.Size(); ++i)
				if (detail::GraphStrEq(l->props[i].name, name)) {
					l->props[i].value = v; // REMPLACE : deux homonymes rendraient
					return true;		   // la lecture dependante de l'insertion
				}
			NkGraphProp p;
			p.name = NkString(name);
			p.value = v;
			l->props.PushBack(p);
			return true;
		}

		inline const NkGraphValue *NkNodeGraph::FindLinkProp(NkLinkId id, const char *name) const {
			const NkLink *l = TrouveLien(id);
			if (!l || !name)
				return nullptr;
			for (uint32 i = 0; i < (uint32)l->props.Size(); ++i)
				if (detail::GraphStrEq(l->props[i].name, name))
					return &l->props[i].value;
			return nullptr;
		}

		inline bool NkNodeGraph::RemoveLinkProp(NkLinkId id, const char *name) {
			NkLink *l = TrouveLien(id);
			if (!l || !name)
				return false;
			for (uint32 i = 0; i < (uint32)l->props.Size(); ++i)
				if (detail::GraphStrEq(l->props[i].name, name)) {
					for (uint32 k = i + 1; k < (uint32)l->props.Size(); ++k)
						l->props[k - 1] = l->props[k];
					l->props.PopBack();
					return true;
				}
			return false;
		}

		inline uint32 NkNodeGraph::LinkPropCount(NkLinkId id) const {
			const NkLink *l = TrouveLien(id);
			return l ? (uint32)l->props.Size() : 0;
		}

		inline bool NkNodeGraph::SetLinkSubgraph(NkLinkId id, const char *nomGraphe) {
			NkLink *l = TrouveLien(id);
			if (!l)
				return false;
			l->subgraph = NkString(nomGraphe ? nomGraphe : "");
			return true;
		}

		inline const NkString *NkNodeGraph::LinkSubgraph(NkLinkId id) const {
			const NkLink *l = TrouveLien(id);
			// ⚠️ `nullptr` = LE LIEN N'EXISTE PAS ; chaine vide = il existe et n'a
			// pas de condition. Deux etats, deux reponses -- regle 3.
			return l ? &l->subgraph : nullptr;
		}

		inline bool NkNodeGraph::Disconnect(NkLinkId id) {
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i)
				if (mLinks[i].alive && mLinks[i].id == id) {
					mLinks[i].alive = false;
					return true;
				}
			return false;
		}

		// ── ORDRE D'EVALUATION ──────────────────────────────────────────────────
		// ⚠️ COMPTE **TOUS** LES LIENS VIVANTS, quelle que soit leur famille --
		// meme regle que `WouldCreateCycle`, et c'est la raison n°2 : le tri
		// topologique doit rester valide sur le graphe ENTIER.
		//
		// 📌 UN FIL D'EXECUTION N'EST PAS UNE DEPENDANCE DE VALEUR -- il dit
		// « puis », pas « a besoin de » -- mais « puis » EST UN ORDRE, et le
		// compter ne peut pas faire echouer le tri : le graphe est acyclique
		// toutes familles confondues, donc il se trie. Ce que ca change : l'ordre
		// rendu respecte AUSSI la succession d'execution, ce qui est un ordre
		// PLUS contraint, jamais un ordre faux.
		inline bool NkNodeGraph::TopoSort(NkVector<NkNodeId> &out) const {
			out.Clear();
			NkVector<NkNodeId> ids;
			NkVector<uint32> indeg;
			NkVector<uint8> done;
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i)
				if (mNodes[i].alive) {
					ids.PushBack(mNodes[i].id);
					indeg.PushBack(0);
					done.PushBack(0);
				}
			auto indexOf = [&](NkNodeId id) -> int32 {
				for (uint32 i = 0; i < (uint32)ids.Size(); ++i)
					if (ids[i] == id)
						return (int32)i;
				return -1;
			};
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				if (!mLinks[i].alive)
					continue;
				const int32 t = indexOf(mLinks[i].toNode);
				if (t >= 0)
					indeg[(uint32)t]++;
			}
			const uint32 total = (uint32)ids.Size();
			uint32 emitted = 0;
			uint32 guard = 0;
			// Passes successives : on emet ce qui n'a plus de dependance. Si une passe
			// n'emet RIEN alors qu'il reste des noeuds, c'est un cycle — on le DIT au
			// lieu de rendre un ordre arbitraire.
			while (emitted < total && guard++ <= total + 1) {
				const uint32 before = emitted;
				for (uint32 i = 0; i < total; ++i) {
					if (done[i] || indeg[i] != 0)
						continue;
					out.PushBack(ids[i]);
					done[i] = 1;
					emitted++;
					for (uint32 k = 0; k < (uint32)mLinks.Size(); ++k) {
						if (!mLinks[k].alive || mLinks[k].fromNode != ids[i])
							continue;
						const int32 t = indexOf(mLinks[k].toNode);
						if (t >= 0 && indeg[(uint32)t] > 0)
							indeg[(uint32)t]--;
					}
				}
				if (emitted == before)
					break;
			}
			if (emitted != total) {
				out.Clear();
				return false;
			}
			return true;
		}

		inline bool NkNodeGraph::HasCycle() const {
			NkVector<NkNodeId> tmp;
			return !TopoSort(tmp);
		}

		// ── VALEURS ─────────────────────────────────────────────────────────────
		inline bool NkGraphValue::Equals(const NkGraphValue &o) const {
			if (type != o.type)
				return false;
			if (numbers.Size() != o.numbers.Size())
				return false;
			for (uint32 i = 0; i < (uint32)numbers.Size(); ++i)
				if (numbers[i] != o.numbers[i])
					return false;
			return text == o.text;
		}

		inline NkGraphValue NkValueReal(NkTypeId t, float32 v) {
			NkGraphValue g;
			g.type = t;
			g.numbers.PushBack(v);
			return g;
		}

		inline NkGraphValue NkValueVec(NkTypeId t, const float32 *v, uint32 n) {
			NkGraphValue g;
			g.type = t;
			for (uint32 i = 0; i < n && v; ++i)
				g.numbers.PushBack(v[i]);
			return g;
		}

		inline NkGraphValue NkValueText(NkTypeId t, const char *s) {
			NkGraphValue g;
			g.type = t;
			g.text = NkString(s ? s : "");
			return g;
		}

		inline bool NkNodeGraph::SetSocketDefault(NkNodeId id, const char *socket, NkSocketDir dir,
												  const NkGraphValue &v) {
			NkNode *n = Find(id);
			if (!n)
				return false;
			const int32 i = n->FindSocket(socket, dir);
			if (i < 0)
				return false;
			n->sockets[(uint32)i].defaultValue = v;
			return true;
		}

		inline const NkGraphValue *NkNodeGraph::SocketDefault(NkNodeId id, const char *socket,
															  NkSocketDir dir) const {
			const NkNode *n = Find(id);
			if (!n)
				return nullptr;
			const int32 i = n->FindSocket(socket, dir);
			if (i < 0)
				return nullptr;
			return &n->sockets[(uint32)i].defaultValue;
		}

		inline bool NkNodeGraph::SetProp(NkNodeId id, const char *name, const NkGraphValue &v) {
			NkNode *n = Find(id);
			if (!n || !name || !*name)
				return false;
			for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
				if (detail::GraphStrEq(n->props[i].name, name)) {
					n->props[i].value = v; // remplace, comme une entree remplace sa source
					return true;
				}
			NkGraphProp pr;
			pr.name = NkString(name);
			pr.value = v;
			n->props.PushBack(pr);
			return true;
		}

		inline const NkGraphValue *NkNodeGraph::FindProp(NkNodeId id, const char *name) const {
			const NkNode *n = Find(id);
			if (!n)
				return nullptr;
			for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
				if (detail::GraphStrEq(n->props[i].name, name))
					return &n->props[i].value;
			return nullptr;
		}

		inline bool NkNodeGraph::RemoveProp(NkNodeId id, const char *name) {
			NkNode *n = Find(id);
			if (!n)
				return false;
			for (uint32 i = 0; i < (uint32)n->props.Size(); ++i)
				if (detail::GraphStrEq(n->props[i].name, name)) {
					// Retrait par decalage : l'ORDRE des proprietes est ce qui rend
					// l'aller-retour de fichier reproductible. Un retrait par
					// permutation avec la derniere ferait varier le texte ecrit
					// sans que rien n'ait change pour l'utilisateur.
					for (uint32 k = i + 1; k < (uint32)n->props.Size(); ++k)
						n->props[k - 1] = n->props[k];
					n->props.PopBack();
					return true;
				}
			return false;
		}

		inline uint32 NkNodeGraph::PropCount(NkNodeId id) const {
			const NkNode *n = Find(id);
			return n ? (uint32)n->props.Size() : 0u;
		}

		// ── VALIDATION ──────────────────────────────────────────────────────────
		inline const char *NkGraphIssueName(NkGraphIssue i) {
			switch (i) {
				case NkGraphIssue::Ok:
					return "ok";
				case NkGraphIssue::LinkUnknownNode:
					return "lien-noeud-inconnu";
				case NkGraphIssue::LinkSocketOutOfRange:
					return "lien-socket-hors-bornes";
				case NkGraphIssue::LinkDirection:
					return "lien-sens-invalide";
				case NkGraphIssue::LinkTypeMismatch:
					return "lien-type-incompatible";
				case NkGraphIssue::LinkDuplicateTarget:
					return "lien-entree-doublee";
				case NkGraphIssue::Cycle:
					return "cycle";
				case NkGraphIssue::SocketUnknownType:
					return "socket-type-inconnu";
				case NkGraphIssue::DefaultTypeMismatch:
					return "defaut-type-different";
				case NkGraphIssue::PropUnknownType:
					return "propriete-type-inconnu";
			}
			return "?";
		}

		inline uint32 NkNodeGraph::Validate(NkVector<NkGraphDiag> &out) const {
			out.Clear();
			auto add = [&](NkGraphIssue is, NkNodeId n, NkLinkId l, const NkString &d) {
				NkGraphDiag g;
				g.issue = is;
				g.node = n;
				g.link = l;
				g.detail = d;
				out.PushBack(g);
			};
			auto typeKnown = [&](NkTypeId t) -> bool {
				// L'index 0 est reserve « invalide » : un type a 0 n'est pas
				// « inconnu », il est ABSENT, et c'est un autre defaut. On borne
				// donc par le haut ET par le bas.
				return t != NK_TYPE_INVALID && t < (NkTypeId)mTypeNames.Size();
			};

			// ── les prises, leurs types, leurs defauts ──────────────────────
			for (uint32 i = 0; i < (uint32)mNodes.Size(); ++i) {
				const NkNode &n = mNodes[i];
				if (!n.alive)
					continue;
				for (uint32 k = 0; k < (uint32)n.sockets.Size(); ++k) {
					const NkSocket &sk = n.sockets[k];
					if (!typeKnown(sk.type))
						add(NkGraphIssue::SocketUnknownType, n.id, 0, sk.name);
					// ⚠️ Le defaut porte SON type, et il doit etre celui de la
					// prise. Sans ce controle, un fichier peut poser un defaut de
					// type « shader » sur une prise « couleur » : le compilateur
					// lirait une valeur du mauvais genre et rendrait quelque chose
					// de plausible, ce qui est pire qu'une erreur.
					if (sk.defaultValue.IsSet() && sk.defaultValue.type != sk.type)
						add(NkGraphIssue::DefaultTypeMismatch, n.id, 0, sk.name);
				}
				for (uint32 k = 0; k < (uint32)n.props.Size(); ++k)
					if (n.props[k].value.IsSet() && !typeKnown(n.props[k].value.type))
						add(NkGraphIssue::PropUnknownType, n.id, 0, n.props[k].name);
			}

			// ── les liens ───────────────────────────────────────────────────
			for (uint32 i = 0; i < (uint32)mLinks.Size(); ++i) {
				const NkLink &l = mLinks[i];
				if (!l.alive)
					continue;
				const NkNode *a = Find(l.fromNode);
				const NkNode *b = Find(l.toNode);
				if (!a || !b) {
					add(NkGraphIssue::LinkUnknownNode, !a ? l.fromNode : l.toNode, l.id, NkString(""));
					continue;
				}
				if (l.fromSocket < 0 || (uint32)l.fromSocket >= (uint32)a->sockets.Size() || l.toSocket < 0 ||
					(uint32)l.toSocket >= (uint32)b->sockets.Size()) {
					add(NkGraphIssue::LinkSocketOutOfRange, l.toNode, l.id, NkString(""));
					continue;
				}
				const NkSocket &sa = a->sockets[(uint32)l.fromSocket];
				const NkSocket &sb = b->sockets[(uint32)l.toSocket];
				if (sa.dir != NkSocketDir::Output || sb.dir != NkSocketDir::Input) {
					add(NkGraphIssue::LinkDirection, l.toNode, l.id, NkString(""));
					continue;
				}
				if (!Accepts(sb.type, sa.type))
					add(NkGraphIssue::LinkTypeMismatch, l.toNode, l.id, sb.name);

				// Une entree n'accepte qu'UNE source. `Connect` le garantit ; un
				// fichier, non. Sans ce controle le graphe s'evaluerait avec celle
				// des deux que l'ordre d'iteration a rencontree en premier.
				for (uint32 k = i + 1; k < (uint32)mLinks.Size(); ++k)
					if (mLinks[k].alive && mLinks[k].toNode == l.toNode && mLinks[k].toSocket == l.toSocket) {
						add(NkGraphIssue::LinkDuplicateTarget, l.toNode, mLinks[k].id, sb.name);
						break;
					}
			}

			if (HasCycle())
				add(NkGraphIssue::Cycle, NK_NODE_INVALID, 0, NkString(""));

			return (uint32)out.Size();
		}

		inline void NkNodeGraph::Clear() {
			mNodes.Clear();
			mLinks.Clear();
			mConversions.Clear();
			mTypeNames.Clear();
			mNextNode = 1;
			mNextLink = 1;
		}

	} // namespace graph
} // namespace nkentseu
