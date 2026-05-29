// =============================================================================
// AfricaPlaces.cpp
// -----------------------------------------------------------------------------
// Data table compile-time : 54 pays UA + ~10 villes notables chacun. Tout en
// ASCII (sans accents) pour rester compatible avec le font atlas Pong qui
// ne charge pas les diacritiques. Les noms officiels en francais/anglais sont
// utilises (ex: "Cote d'Ivoire", "Sao Tome et Principe").
//
// Cette table pese ~5 KB de strings dans le binaire, accepte par l'user.
// =============================================================================

#include "Songoo/Net/AfricaPlaces.h"
#include "NKMath/NkRandom.h"
#include <cstdio>
#include <cstring>

namespace nkentseu
{
    namespace songoo
    {
        namespace africa
        {

            // ─────────────────────────────────────────────────────────────────
            // Tableaux de villes (un par pays). Les capitales d'abord, puis
            // les grandes villes par ordre approximatif d'importance.
            // ─────────────────────────────────────────────────────────────────

            static const char* const kAlgerie[] = {
                "Alger", "Oran", "Constantine", "Annaba", "Blida",
                "Batna", "Setif", "Tlemcen", "Bejaia", "Tizi Ouzou"
            };
            static const char* const kAngola[] = {
                "Luanda", "Huambo", "Lobito", "Benguela", "Kuito",
                "Lubango", "Malanje", "Namibe", "Soyo", "Cabinda"
            };
            static const char* const kBenin[] = {
                "Porto-Novo", "Cotonou", "Parakou", "Djougou", "Bohicon",
                "Abomey", "Natitingou", "Ouidah", "Lokossa", "Kandi"
            };
            static const char* const kBotswana[] = {
                "Gaborone", "Francistown", "Molepolole", "Maun", "Selibe Phikwe",
                "Serowe", "Kanye", "Mahalapye", "Mochudi", "Lobatse"
            };
            static const char* const kBurkinaFaso[] = {
                "Ouagadougou", "Bobo-Dioulasso", "Koudougou", "Banfora", "Ouahigouya",
                "Kaya", "Tenkodogo", "Fada N'Gourma", "Dedougou", "Houndé"
            };
            static const char* const kBurundi[] = {
                "Gitega", "Bujumbura", "Muyinga", "Ngozi", "Ruyigi",
                "Kayanza", "Cankuzo", "Rutana", "Bururi", "Makamba"
            };
            static const char* const kCameroun[] = {
                "Yaounde", "Douala", "Garoua", "Bamenda", "Maroua",
                "Bafoussam", "Ngaoundere", "Bertoua", "Ebolowa", "Kribi",
                "Dschang", "Limbe", "Buea"
            };
            static const char* const kCapVert[] = {
                "Praia", "Mindelo", "Santa Maria", "Espargos", "Assomada",
                "Tarrafal", "Sao Filipe", "Porto Novo", "Pedra Badejo", "Sal Rei"
            };
            static const char* const kCentrafrique[] = {
                "Bangui", "Bimbo", "Berberati", "Carnot", "Bambari",
                "Bouar", "Bossangoa", "Bria", "Kaga-Bandoro", "Sibut"
            };
            static const char* const kComores[] = {
                "Moroni", "Mutsamudu", "Fomboni", "Domoni", "Mitsamiouli",
                "Mbeni", "Sima", "Ouani", "Mirontsi", "Tsembehou"
            };
            static const char* const kCongo[] = {
                "Brazzaville", "Pointe-Noire", "Dolisie", "Nkayi", "Owando",
                "Ouesso", "Madingou", "Impfondo", "Sibiti", "Gamboma"
            };
            static const char* const kRDC[] = {
                "Kinshasa", "Lubumbashi", "Mbuji-Mayi", "Kananga", "Kisangani",
                "Bukavu", "Goma", "Likasi", "Kolwezi", "Tshikapa",
                "Matadi", "Mbandaka", "Boma"
            };
            static const char* const kCoteIvoire[] = {
                "Yamoussoukro", "Abidjan", "Bouake", "Daloa", "Korhogo",
                "San-Pedro", "Man", "Gagnoa", "Divo", "Anyama"
            };
            static const char* const kDjibouti[] = {
                "Djibouti", "Ali Sabieh", "Tadjourah", "Obock", "Dikhil",
                "Arta", "Holhol", "Loyada", "Yoboki", "As Eyla"
            };
            static const char* const kEgypte[] = {
                "Le Caire", "Alexandrie", "Gizeh", "Port-Said", "Suez",
                "Louxor", "Mansourah", "Tanta", "Asyout", "Ismailia",
                "Fayoum", "Zagazig", "Assouan", "Damiette"
            };
            static const char* const kErythree[] = {
                "Asmara", "Keren", "Massawa", "Assab", "Mendefera",
                "Barentu", "Adi Keyh", "Edd", "Ghinda", "Akordat"
            };
            static const char* const kEswatini[] = {
                "Mbabane", "Manzini", "Lobamba", "Nhlangano", "Siteki",
                "Pigg's Peak", "Big Bend", "Hluti", "Malkerns", "Bhunya"
            };
            static const char* const kEthiopie[] = {
                "Addis-Abeba", "Dire Dawa", "Mekele", "Gondar", "Adama",
                "Awasa", "Bahir Dar", "Dessie", "Jimma", "Jijiga",
                "Hararghe", "Harar", "Shashamane"
            };
            static const char* const kGabon[] = {
                "Libreville", "Port-Gentil", "Franceville", "Oyem", "Moanda",
                "Mouila", "Lambarene", "Tchibanga", "Koulamoutou", "Makokou"
            };
            static const char* const kGambie[] = {
                "Banjul", "Serekunda", "Brikama", "Bakau", "Farafenni",
                "Lamin", "Sukuta", "Basse", "Gunjur", "Soma"
            };
            static const char* const kGhana[] = {
                "Accra", "Kumasi", "Tamale", "Sekondi-Takoradi", "Sunyani",
                "Cape Coast", "Obuasi", "Tema", "Koforidua", "Ho",
                "Wa", "Bolgatanga"
            };
            static const char* const kGuinee[] = {
                "Conakry", "Nzerekore", "Kankan", "Kindia", "Labe",
                "Mamou", "Boke", "Faranah", "Siguiri", "Dabola"
            };
            static const char* const kGuineeBissau[] = {
                "Bissau", "Bafata", "Gabu", "Bissora", "Bolama",
                "Cacheu", "Bubaque", "Catio", "Mansoa", "Buba"
            };
            static const char* const kGuineeEqua[] = {
                "Malabo", "Bata", "Ebebiyin", "Aconibe", "Mongomo",
                "Luba", "Riaba", "Mbini", "Nsok", "Anisok"
            };
            static const char* const kKenya[] = {
                "Nairobi", "Mombasa", "Kisumu", "Nakuru", "Eldoret",
                "Thika", "Malindi", "Kitale", "Kakamega", "Garissa",
                "Machakos", "Meru"
            };
            static const char* const kLesotho[] = {
                "Maseru", "Teyateyaneng", "Hlotse", "Mafeteng", "Mohale's Hoek",
                "Maputsoe", "Quthing", "Qacha's Nek", "Butha-Buthe", "Thaba-Tseka"
            };
            static const char* const kLiberia[] = {
                "Monrovia", "Gbarnga", "Buchanan", "Kakata", "Voinjama",
                "Harper", "Zwedru", "Greenville", "Robertsport", "Tubmanburg"
            };
            static const char* const kLibye[] = {
                "Tripoli", "Benghazi", "Misratah", "Tarhuna", "Al-Khums",
                "Az-Zawiyah", "Zliten", "Tobruk", "Sabha", "Syrte"
            };
            static const char* const kMadagascar[] = {
                "Antananarivo", "Toamasina", "Antsirabe", "Fianarantsoa", "Mahajanga",
                "Toliara", "Antsiranana", "Ambovombe", "Sambava", "Manakara"
            };
            static const char* const kMalawi[] = {
                "Lilongwe", "Blantyre", "Mzuzu", "Zomba", "Karonga",
                "Kasungu", "Mangochi", "Salima", "Liwonde", "Nkhotakota"
            };
            static const char* const kMali[] = {
                "Bamako", "Sikasso", "Mopti", "Segou", "Koutiala",
                "Kayes", "Gao", "Tombouctou", "Kati", "San"
            };
            static const char* const kMaroc[] = {
                "Rabat", "Casablanca", "Marrakech", "Fes", "Tanger",
                "Agadir", "Meknes", "Oujda", "Kenitra", "Tetouan",
                "Sale", "Mohammedia", "El Jadida"
            };
            static const char* const kMaurice[] = {
                "Port-Louis", "Beau-Bassin", "Vacoas-Phoenix", "Curepipe", "Quatre-Bornes",
                "Triolet", "Goodlands", "Centre de Flacq", "Mahebourg", "Saint-Pierre"
            };
            static const char* const kMauritanie[] = {
                "Nouakchott", "Nouadhibou", "Kiffa", "Rosso", "Atar",
                "Zouerat", "Kaedi", "Nema", "Selibaby", "Akjoujt"
            };
            static const char* const kMozambique[] = {
                "Maputo", "Matola", "Beira", "Nampula", "Chimoio",
                "Nacala", "Quelimane", "Tete", "Pemba", "Lichinga",
                "Xai-Xai", "Inhambane"
            };
            static const char* const kNamibie[] = {
                "Windhoek", "Walvis Bay", "Swakopmund", "Oshakati", "Rundu",
                "Ondangwa", "Katima Mulilo", "Otjiwarongo", "Tsumeb", "Keetmanshoop"
            };
            static const char* const kNiger[] = {
                "Niamey", "Zinder", "Maradi", "Agadez", "Tahoua",
                "Dosso", "Diffa", "Birni N'Konni", "Tessaoua", "Gaya"
            };
            static const char* const kNigeria[] = {
                "Abuja", "Lagos", "Kano", "Ibadan", "Port Harcourt",
                "Benin City", "Kaduna", "Onitsha", "Maiduguri", "Aba",
                "Jos", "Ilorin", "Enugu", "Zaria", "Abeokuta"
            };
            static const char* const kOuganda[] = {
                "Kampala", "Gulu", "Lira", "Mbarara", "Jinja",
                "Bwizibwera", "Mbale", "Mukono", "Kasese", "Masaka"
            };
            static const char* const kRwanda[] = {
                "Kigali", "Butare", "Gitarama", "Ruhengeri", "Gisenyi",
                "Byumba", "Cyangugu", "Kibuye", "Kibungo", "Nyanza"
            };
            static const char* const kSahraoui[] = {
                "Laayoune", "Dakhla", "Smara", "Boujdour", "Bir Gandus",
                "Bir Lehlou", "Tifariti", "Mehaires", "Mijek", "Aousserd"
            };
            static const char* const kSaoTome[] = {
                "Sao Tome", "Santo Antonio", "Neves", "Santana", "Trindade",
                "Sao Joao dos Angolares", "Guadalupe", "Pantufo", "Almada", "Madalena"
            };
            static const char* const kSenegal[] = {
                "Dakar", "Touba", "Thies", "Rufisque", "Kaolack",
                "M'Bour", "Saint-Louis", "Ziguinchor", "Diourbel", "Tambacounda",
                "Louga", "Pikine"
            };
            static const char* const kSeychelles[] = {
                "Victoria", "Anse Boileau", "Beau Vallon", "Bel Ombre", "Cascade",
                "Glacis", "Grand Anse", "La Digue", "Port Glaud", "Takamaka"
            };
            static const char* const kSierraLeone[] = {
                "Freetown", "Bo", "Kenema", "Koidu", "Makeni",
                "Lunsar", "Port Loko", "Kabala", "Magburaka", "Waterloo"
            };
            static const char* const kSomalie[] = {
                "Mogadiscio", "Hargeisa", "Berbera", "Kismayo", "Merca",
                "Bossaso", "Gaalkacyo", "Baidoa", "Burao", "Garowe"
            };
            static const char* const kSoudan[] = {
                "Khartoum", "Omdurman", "Port-Soudan", "Kassala", "El-Obeid",
                "Nyala", "Wad Madani", "Dongola", "Ad-Damazin", "Sennar"
            };
            static const char* const kSoudanSud[] = {
                "Djouba", "Wau", "Malakal", "Yei", "Aweil",
                "Bor", "Yambio", "Rumbek", "Bentiu", "Torit"
            };
            static const char* const kAfriqueSud[] = {
                "Pretoria", "Johannesburg", "Le Cap", "Durban", "Port Elizabeth",
                "Bloemfontein", "East London", "Soweto", "Pietermaritzburg", "Polokwane",
                "Kimberley", "Nelspruit", "Rustenburg"
            };
            static const char* const kTanzanie[] = {
                "Dodoma", "Dar es-Salaam", "Mwanza", "Arusha", "Mbeya",
                "Morogoro", "Tanga", "Kahama", "Tabora", "Zanzibar",
                "Kigoma", "Moshi"
            };
            static const char* const kTchad[] = {
                "N'Djamena", "Moundou", "Sarh", "Abeche", "Kelo",
                "Koumra", "Pala", "Am Timan", "Bongor", "Mongo"
            };
            static const char* const kTogo[] = {
                "Lome", "Sokode", "Kara", "Atakpame", "Kpalime",
                "Dapaong", "Tsevie", "Aneho", "Mango", "Bassar"
            };
            static const char* const kTunisie[] = {
                "Tunis", "Sfax", "Sousse", "Kairouan", "Bizerte",
                "Gabes", "Ariana", "Gafsa", "Monastir", "Hammamet",
                "Mahdia", "Nabeul"
            };
            static const char* const kZambie[] = {
                "Lusaka", "Kitwe", "Ndola", "Kabwe", "Chingola",
                "Mufulira", "Livingstone", "Luanshya", "Kasama", "Chipata"
            };
            static const char* const kZimbabwe[] = {
                "Harare", "Bulawayo", "Chitungwiza", "Mutare", "Gweru",
                "Kwekwe", "Kadoma", "Masvingo", "Chinhoyi", "Marondera",
                "Norton", "Victoria Falls"
            };

            // ─────────────────────────────────────────────────────────────────
            // Macro pour declarer une entree compactement (NB_ELEMS calcule
            // au compile time la taille du tableau de villes).
            // ─────────────────────────────────────────────────────────────────
            #define NK_AFRICA_ENTRY(cname, table) \
                { cname, table, (int)(sizeof(table) / sizeof(table[0])) }

            // 54 pays UA + Sahara Occidental (RASD reconnue par l'UA).
            static const CountryEntry kAfricaPlaces[] = {
                NK_AFRICA_ENTRY("Afrique du Sud",          kAfriqueSud),
                NK_AFRICA_ENTRY("Algerie",                 kAlgerie),
                NK_AFRICA_ENTRY("Angola",                  kAngola),
                NK_AFRICA_ENTRY("Benin",                   kBenin),
                NK_AFRICA_ENTRY("Botswana",                kBotswana),
                NK_AFRICA_ENTRY("Burkina Faso",            kBurkinaFaso),
                NK_AFRICA_ENTRY("Burundi",                 kBurundi),
                NK_AFRICA_ENTRY("Cameroun",                kCameroun),
                NK_AFRICA_ENTRY("Cap-Vert",                kCapVert),
                NK_AFRICA_ENTRY("Centrafrique",            kCentrafrique),
                NK_AFRICA_ENTRY("Comores",                 kComores),
                NK_AFRICA_ENTRY("Congo",                   kCongo),
                NK_AFRICA_ENTRY("Cote d'Ivoire",           kCoteIvoire),
                NK_AFRICA_ENTRY("Djibouti",                kDjibouti),
                NK_AFRICA_ENTRY("Egypte",                  kEgypte),
                NK_AFRICA_ENTRY("Erythree",                kErythree),
                NK_AFRICA_ENTRY("Eswatini",                kEswatini),
                NK_AFRICA_ENTRY("Ethiopie",                kEthiopie),
                NK_AFRICA_ENTRY("Gabon",                   kGabon),
                NK_AFRICA_ENTRY("Gambie",                  kGambie),
                NK_AFRICA_ENTRY("Ghana",                   kGhana),
                NK_AFRICA_ENTRY("Guinee",                  kGuinee),
                NK_AFRICA_ENTRY("Guinee-Bissau",           kGuineeBissau),
                NK_AFRICA_ENTRY("Guinee Equatoriale",      kGuineeEqua),
                NK_AFRICA_ENTRY("Kenya",                   kKenya),
                NK_AFRICA_ENTRY("Lesotho",                 kLesotho),
                NK_AFRICA_ENTRY("Liberia",                 kLiberia),
                NK_AFRICA_ENTRY("Libye",                   kLibye),
                NK_AFRICA_ENTRY("Madagascar",              kMadagascar),
                NK_AFRICA_ENTRY("Malawi",                  kMalawi),
                NK_AFRICA_ENTRY("Mali",                    kMali),
                NK_AFRICA_ENTRY("Maroc",                   kMaroc),
                NK_AFRICA_ENTRY("Maurice",                 kMaurice),
                NK_AFRICA_ENTRY("Mauritanie",              kMauritanie),
                NK_AFRICA_ENTRY("Mozambique",              kMozambique),
                NK_AFRICA_ENTRY("Namibie",                 kNamibie),
                NK_AFRICA_ENTRY("Niger",                   kNiger),
                NK_AFRICA_ENTRY("Nigeria",                 kNigeria),
                NK_AFRICA_ENTRY("Ouganda",                 kOuganda),
                NK_AFRICA_ENTRY("RD Congo",                kRDC),
                NK_AFRICA_ENTRY("Rwanda",                  kRwanda),
                NK_AFRICA_ENTRY("Sahara Occidental",       kSahraoui),
                NK_AFRICA_ENTRY("Sao Tome et Principe",    kSaoTome),
                NK_AFRICA_ENTRY("Senegal",                 kSenegal),
                NK_AFRICA_ENTRY("Seychelles",              kSeychelles),
                NK_AFRICA_ENTRY("Sierra Leone",            kSierraLeone),
                NK_AFRICA_ENTRY("Somalie",                 kSomalie),
                NK_AFRICA_ENTRY("Soudan",                  kSoudan),
                NK_AFRICA_ENTRY("Soudan du Sud",           kSoudanSud),
                NK_AFRICA_ENTRY("Tanzanie",                kTanzanie),
                NK_AFRICA_ENTRY("Tchad",                   kTchad),
                NK_AFRICA_ENTRY("Togo",                    kTogo),
                NK_AFRICA_ENTRY("Tunisie",                 kTunisie),
                NK_AFRICA_ENTRY("Zambie",                  kZambie),
                NK_AFRICA_ENTRY("Zimbabwe",                kZimbabwe),
            };
            #undef NK_AFRICA_ENTRY

            // Tableau valide a compile-time.
            static constexpr int kAfricaPlacesCount =
                (int)(sizeof(kAfricaPlaces) / sizeof(kAfricaPlaces[0]));
            static_assert(kAfricaPlacesCount >= 54,
                          "La table doit contenir au moins les 54 pays UA");

            // ─────────────────────────────────────────────────────────────────
            // API publique
            // ─────────────────────────────────────────────────────────────────

            int CountryCount() noexcept
            {
                return kAfricaPlacesCount;
            }

            const CountryEntry& GetCountry(int idx) noexcept
            {
                // Clamp defensif : si idx hors bornes, on retourne le pays 0
                // pour eviter un acces UB. En pratique le caller utilise
                // toujours NkRandom modulo CountryCount() donc OK.
                if (idx < 0 || idx >= kAfricaPlacesCount)
                {
                    return kAfricaPlaces[0];
                }
                return kAfricaPlaces[idx];
            }

            int PickRandomPlace(char* outBuf, int bufSize) noexcept
            {
                if (outBuf == nullptr || bufSize < 2) return 0;
                // Tirage : pays uniforme, puis ville uniforme dans ce pays.
                // NkRandom::Instance() est une singleton seedee proprement (cf
                // [[feedback_nk_time_random]]) -> bonne distribution.
                math::NkRandom& rng = math::NkRandom::Instance();
                const int ci = (int)(rng.NextUInt32((uint32)kAfricaPlacesCount));
                const CountryEntry& e = kAfricaPlaces[ci];
                const int vi = (int)(rng.NextUInt32((uint32)e.cityCount));
                const char* city = e.cities[vi];
                // Format "Pays/Ville". std::snprintf garantit la null-terminaison.
                const int written = std::snprintf(outBuf, (size_t)bufSize,
                                                  "%s/%s", e.country, city);
                if (written < 0) return 0;
                // snprintf renvoie le nombre de caracteres qu'il aurait ecrit
                // si bufSize etait infini ; on clamp pour le caller.
                return (written >= bufSize) ? (bufSize - 1) : written;
            }

            bool PickRandomCountryCity(char* countryBuf, int countryBufSize,
                                       char* cityBuf, int cityBufSize) noexcept
            {
                if (countryBuf == nullptr || cityBuf == nullptr) return false;
                if (countryBufSize < 2 || cityBufSize < 2) return false;
                math::NkRandom& rng = math::NkRandom::Instance();
                const int ci = (int)(rng.NextUInt32((uint32)kAfricaPlacesCount));
                const CountryEntry& e = kAfricaPlaces[ci];
                const int vi = (int)(rng.NextUInt32((uint32)e.cityCount));
                std::snprintf(countryBuf, (size_t)countryBufSize, "%s", e.country);
                std::snprintf(cityBuf,    (size_t)cityBufSize,    "%s", e.cities[vi]);
                return true;
            }

            bool PickRandomCountryCityCode(char* countryBuf, int countryBufSize,
                                           char* cityBuf,    int cityBufSize,
                                           char* codeBuf,    int codeBufSize) noexcept
            {
                // Pays + ville comme la version courte.
                if (!PickRandomCountryCity(countryBuf, countryBufSize,
                                           cityBuf, cityBufSize))
                {
                    return false;
                }
                if (codeBuf == nullptr || codeBufSize < 10) return false;
                // Code 9 chiffres dans [0..999999999] formate avec zero-pad.
                // 1 milliard de combinaisons : collision quasi impossible meme
                // avec des dizaines de milliers de joueurs simultanes.
                math::NkRandom& rng = math::NkRandom::Instance();
                const uint32 code = rng.NextUInt32(1000000000u);
                std::snprintf(codeBuf, (size_t)codeBufSize, "%09u",
                              (unsigned)code);
                return true;
            }

        } // namespace africa
    }     // namespace songoo
} // namespace nkentseu
