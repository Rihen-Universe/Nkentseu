// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
// NkGuiComposants.inl — le développement, et les trois refus qu'il sait dire.
// =============================================================================

namespace nkentseu {
	namespace nkgui {

		namespace detail {

			/// Cherche une définition par son nom. Rend nullptr si le nom n'est pas
			/// un composant — ce n'est PAS une faute : c'est probablement un rôle
			/// du vocabulaire, ou un rôle inconnu que le monteur comptera.
			inline const NkGCDefinition *NkGCTrouver(const NkVector<NkGCDefinition> &defs,
													 NkStringView nom) noexcept {
				for (uint32 i = 0; i < (uint32)defs.Size(); ++i)
					if (NkGCMotEgal(nom, defs[i].nom.CStr()))
						return &defs[i];
				return nullptr;
			}

			/// Vrai si `nom` figure dans la pile d'instanciation en cours.
			inline bool NkGCDansLaPile(const NkVector<NkString> &pile, NkStringView nom) noexcept {
				for (uint32 i = 0; i < (uint32)pile.Size(); ++i)
					if (NkGCMotEgal(nom, pile[i].CStr()))
						return true;
				return false;
			}

			/// Les blocs `behavior` d'une définition, copiés et réécrits pour une
			/// instance. Ils sont rendus à l'appelant, qui les posera au niveau du
			/// document : un `behavior` n'est pas un widget, il ne vit pas dans
			/// l'arbre des widgets.
			inline void NkGCComportementsDe(const NkArchive &def, const NkString &instance,
											const NkGCRenommage &ren,
											NkVector<NkArchiveNode> &sortie,
											NkGuiRapportComposants &rap) noexcept {
				const NkArchiveNode *corps = NkGCCorps(def);
				if (!corps)
					return;
				for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
					const NkArchiveNode &n = corps->array[i];
					if (!n.IsObject() || !n.object)
						continue;
					const NkStringView t = NkGuiArchive::TypeOf(*n.object);
					// `animation` et `callback` voyagent aussi : le format les
					// porte, et les jeter ici les ferait disparaître du document
					// développé alors que l'auteur les a écrits. Le monteur les
					// compte sans les jouer -- c'est SON affaire, pas la nôtre.
					const bool estSection = NkGCMotEgal(t, "behavior") || NkGCMotEgal(t, "animation")
											|| NkGCMotEgal(t, "callback");
					if (!estSection)
						continue;

					NkArchiveNode copie = n; // copie profonde
					NkArchive &bloc = *copie.object;

					// L'identifiant du bloc suit le même renommage que les widgets :
					// un `behavior "bouton"` devient `behavior "monInstance"` quand
					// `bouton` est la racine, ou `behavior "monInstance.case"` sinon.
					const NkStringView id = NkGuiArchive::IdOf(bloc);
					if (id.Size() > 0u) {
						NkString ancien(id);
						bool trouve = false;
						for (uint32 k = 0; k < (uint32)ren.avant.Size() && !trouve; ++k) {
							if (ancien.Compare(ren.avant[k].CStr()) == 0) {
								NkGuiArchive::SetToken(bloc, NkStringView(NkGuiArchive::KeyId()),
													   NkStringView(ren.apres[k].CStr()));
								trouve = true;
							}
						}
						if (!trouve) {
							// Le bloc nomme quelque chose que le composant ne déclare
							// pas : on préfixe quand même, sinon deux instances
							// partageraient le même `behavior`.
							NkString neuf = instance;
							neuf += ".";
							neuf += ancien;
							NkGuiArchive::SetToken(bloc, NkStringView(NkGuiArchive::KeyId()),
												   NkStringView(neuf.CStr()));
						}
					}

					rap.referencesReecrites += NkGCReecrireCorps(bloc, ren);
					sortie.PushBack(copie);
					++rap.comportementsCopies;
				}
			}

			/// Développe un sous-arbre de widgets. Rend le nombre d'instances
			/// développées. `aPoser` reçoit les sections (`behavior`...) que les
			/// instances apportent et qui doivent remonter au document.
			inline void NkGCDevelopperCorps(NkArchiveNode &corps,
											const NkVector<NkGCDefinition> &defs,
											NkVector<NkString> &pile,
											NkVector<NkArchiveNode> &aPoser,
											NkGuiRapportComposants &rap) noexcept {
				if (!corps.IsArray())
					return;
				NkVector<NkArchiveNode> sortie;
				for (uint32 i = 0; i < (uint32)corps.array.Size(); ++i) {
					NkArchiveNode &n = corps.array[i];
					if (!n.IsObject() || !n.object) {
						sortie.PushBack(n); // tranche brute : telle quelle
						continue;
					}
					NkArchive &bloc = *n.object;
					const NkStringView type = NkGuiArchive::TypeOf(bloc);
					const NkGCDefinition *def = NkGCTrouver(defs, type);

					if (!def) {
						// Pas un composant : on descend dans ses enfants, puis on
						// le garde tel quel.
						NkArchiveNode *sousCorps = NkGCCorpsMut(bloc);
						if (sousCorps)
							NkGCDevelopperCorps(*sousCorps, defs, pile, aPoser, rap);
						sortie.PushBack(n);
						continue;
					}

					// ── C'EST UNE INSTANCE ──────────────────────────────────
					// ⚠️ LA RÉCURSION EST REFUSÉE, PAS LIMITÉE. Un composant qui
					//    s'instancie lui-même est une faute d'écriture. Une
					//    profondeur maximale en ferait un document à moitié
					//    développé -- un défaut silencieux.
					if (NkGCDansLaPile(pile, type)) {
						++rap.recursions;
						NkString r(type);
						r += " : s'instancie lui-meme (refuse)";
						rap.refuses.PushBack(r);
						continue; // l'instance disparaît, et le compteur le dit
					}

					const NkArchiveNode *defCorps = NkGCCorps(def->corps);
					const NkArchiveNode *widgetsDef = nullptr;
					if (defCorps) {
						for (uint32 k = 0; k < (uint32)defCorps->array.Size(); ++k) {
							const NkArchiveNode &s = defCorps->array[k];
							if (s.IsObject() && s.object
								&& NkGCMotEgal(NkGuiArchive::TypeOf(*s.object), "widgets")) {
								widgetsDef = &s;
								break;
							}
						}
					}

					// Compter les racines : il en faut EXACTEMENT une.
					uint32 racines = 0;
					const NkArchiveNode *wCorps =
						(widgetsDef && widgetsDef->object) ? NkGCCorps(*widgetsDef->object) : nullptr;
					if (wCorps)
						for (uint32 k = 0; k < (uint32)wCorps->array.Size(); ++k)
							if (wCorps->array[k].IsObject() && wCorps->array[k].object)
								++racines;

					if (racines != 1u) {
						++rap.racinesMultiples;
						NkString r(type);
						r += (racines == 0u) ? " : aucune racine (refuse)"
											 : " : plusieurs racines (refuse)";
						rap.refuses.PushBack(r);
						continue;
					}

					// La racine, copiée.
					NkArchiveNode racine;
					for (uint32 k = 0; k < (uint32)wCorps->array.Size(); ++k) {
						if (wCorps->array[k].IsObject() && wCorps->array[k].object) {
							racine = wCorps->array[k];
							break;
						}
					}

					const NkStringView idInstance = NkGuiArchive::IdOf(bloc);
					NkString instance(idInstance);
					NkGCRenommage ren;

					// La racine PREND l'identifiant de l'instance.
					{
						NkArchive &r = *racine.object;
						const NkStringView idRacine = NkGuiArchive::IdOf(r);
						if (idRacine.Size() > 0u)
							ren.Ajouter(NkString(idRacine), instance);
						NkGuiArchive::SetToken(r, NkStringView(NkGuiArchive::KeyId()),
											   NkStringView(instance.CStr()));
						// Ses descendants sont préfixés.
						NkGCPrefixerIds(racine, instance, ren);
						// Les attributs de l'instance l'emportent.
						rap.attributsSurcharges += NkGCSurcharger(r, bloc);
						// Les références des `behavior` internes suivent.
						NkGCReecrireCorps(r, ren);
					}

					// Les sections que le composant apporte.
					NkGCComportementsDe(def->corps, instance, ren, aPoser, rap);

					// Un composant peut en contenir d'autres : on développe la
					// racine à son tour, sous protection de récursion.
					pile.PushBack(NkString(type));
					{
						NkArchiveNode *sousCorps = NkGCCorpsMut(*racine.object);
						if (sousCorps)
							NkGCDevelopperCorps(*sousCorps, defs, pile, aPoser, rap);
					}
					pile.PopBack();

					sortie.PushBack(racine);
					++rap.instances;
				}
				corps.array = sortie;
			}

		} // namespace detail

		inline bool NkGuiDevelopperComposants(NkArchive &doc, NkGuiRapportComposants &rap) noexcept {
			using namespace detail;

			NkArchiveNode *corps = NkGCCorpsMut(doc);
			if (!corps)
				return true; // un document sans corps n'a rien à développer

			// ── 1. RECENSER LES DÉFINITIONS ────────────────────────────────
			NkVector<NkGCDefinition> defs;
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				const NkArchiveNode &n = corps->array[i];
				if (!n.IsObject() || !n.object)
					continue;
				if (!NkGCMotEgal(NkGuiArchive::TypeOf(*n.object), "component"))
					continue;
				NkGCDefinition d;
				d.nom = NkString(NkGuiArchive::IdOf(*n.object));
				d.corps = *n.object;
				if (d.nom.Size() == 0u) {
					++rap.racinesMultiples; // un composant sans nom ne s'instancie pas
					rap.refuses.PushBack(NkString("component sans nom (refuse)"));
					continue;
				}
				rap.noms.PushBack(d.nom);
				defs.PushBack(d);
				++rap.definitions;
			}

			if (defs.Size() == 0u)
				return true; // rien à faire, et c'est le cas courant

			// ── 2. DÉVELOPPER LES SECTIONS `widgets` ───────────────────────
			NkVector<NkString> pile;
			NkVector<NkArchiveNode> aPoser;
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				NkArchiveNode &n = corps->array[i];
				if (!n.IsObject() || !n.object)
					continue;
				if (!NkGCMotEgal(NkGuiArchive::TypeOf(*n.object), "widgets"))
					continue;
				NkArchiveNode *wCorps = NkGCCorpsMut(*n.object);
				if (wCorps)
					NkGCDevelopperCorps(*wCorps, defs, pile, aPoser, rap);
			}

			// ── 3. POSER CE QUE LES INSTANCES ONT APPORTÉ, ET RETIRER LES
			//       DÉFINITIONS ─────────────────────────────────────────────
			// ⚠️ LES DÉFINITIONS SORTENT DU DOCUMENT DÉVELOPPÉ. Les laisser ferait
			//    compter au monteur des widgets qui ne sont montés nulle part --
			//    un `widgets` de définition n'est pas une vue, c'est un patron.
			NkVector<NkArchiveNode> final;
			for (uint32 i = 0; i < (uint32)corps->array.Size(); ++i) {
				const NkArchiveNode &n = corps->array[i];
				if (n.IsObject() && n.object
					&& NkGCMotEgal(NkGuiArchive::TypeOf(*n.object), "component"))
					continue;
				final.PushBack(n);
			}
			for (uint32 i = 0; i < (uint32)aPoser.Size(); ++i)
				final.PushBack(aPoser[i]);
			corps->array = final;

			return rap.Propre();
		}

	} // namespace nkgui
} // namespace nkentseu
