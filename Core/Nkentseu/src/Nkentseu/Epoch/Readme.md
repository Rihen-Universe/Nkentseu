# 📅 Module Epoch - NKENTSEU Framework  

**Gestion temporelle avancée** avec mesures haute précision, dates grégoriennes et calculs de durées pour applications C++ modernes.  

![Chronométrage Haute Précision](https://via.placeholder.com/800x200.png?text=Chrono|Dates|Heures|Durées|Nanosecondes)  

---

## 🕒 Composants Clés  

### ⏱️ Chronomètre Haute Précision (`Chrono.h`)  
Mesure des intervalles temporels avec précision nanoseconde.  

**Fonctionnalités principales :**  
```cpp  
nkentseu::Chrono chrono;  
// ... code à mesurer ...  
auto elapsed = chrono.Elapsed(); // Obtient le temps écoulé  

std::cout << "Durée : " << elapsed.ToString(); // Formatage automatique  
```  

**Sortie :**  
```text  
Durée : 123.456 ms  
```  

---

### 📅 Gestion de Dates (`Date.h`)  
Manipulation de dates grégoriennes avec validation complète.  

**Utilisation typique :**  
```cpp  
// Création et validation  
nkentseu::Date date(2025, 12, 31); // Lance une exception si invalide  

// Opérations courantes  
date.SetMonth(2); // Validation automatique du 29 février  
std::cout << date.ToString(); // Format ISO 8601 : "2025-02-28"  
```  

---

### 🕰️ Temps Journalier (`Time.h`)  
Représentation précise du temps quotidien avec gestion des nanosecondes.  

**Exemple complet :**  
```cpp  
// Création avec précision nanoseconde  
nkentseu::Time meetingTime(14, 30, 0, 500, 250000); // 14:30:00.500.250000  

// Opérations temporelles  
auto duration = nkentseu::Time(18, 0, 0) - meetingTime;  
double totalHours = static_cast<double>(duration);  
```  

---

### ⏳ Calcul de Durées (`TimeSpan.h`)  
Représente des intervalles temporels signés avec composants détaillés.  

**Opérations possibles :**  
```cpp  
TimeSpan ts1 = TimeSpan::FromDays(2) + TimeSpan::FromHours(3);  
TimeSpan ts2 = ts1 * 1.5; // Multiplication par un scalaire  

std::cout << ts2.ToString(); // "2j 7h 30m 0s"  
```  

---

## 🚀 Fonctionnalités  

### Classe `Time`  
- Représentation temporelle avec **précision nanoseconde**  
- Opérations arithmétiques sécurisées  
- Conversion multi-fuseaux horaires  
- Formatage ISO 8601 étendu (`HH:MM:SS.sss.nnn`)  
- Validation automatique des valeurs  

### Classe `Date`  
- Dates grégoriennes **1601-30827**  
- Gestion des années bissextiles  
- Calculs de jours/mois/années  
- Localisation française intégrée  
- Validation de l'intégrité des dates  

### Classe `Chrono`  
- Chronométrage haute résolution (nanoseconde)  
- Mesure de durée avec auto-formatage  
- Réinitialisation atomique  
- Compatibilité multi-plateforme  

---

## 📦 Inclusion  

```cpp  
// Fichiers nécessaires  
#include "Nkentseu/Epoch/Time.h"  
#include "Nkentseu/Epoch/Date.h"  
#include "Nkentseu/Epoch/Chrono.h"  

// Espace de noms  
using namespace nkentseu;  
```  

**Dépendances :**  
- C++17 ou supérieur  
- `<chrono>`, `<ctime>`, `<iomanip>`  

---

## 🛠 Utilisation Basique  

### Manipulation du temps  
```cpp  
Time now = Time::GetCurrent();  
Time meeting(14, 30, 0); // 14h30  

// Opérations temporelles  
meeting += Time(0, 15, 0); // +15 minutes  
double totalSec = static_cast<double>(meeting);  
```  

### Gestion de dates  
```cpp  
Date today = Date::GetCurrent();  
Date projectDeadline(2025, 12, 31);  

// Calculs avancés  
if(Date::IsLeapYear(2024)) {  
    int days = Date::DaysInMonth(2024, 2); // 29  
}  
```  

### Benchmarks  
```cpp  
Chrono timer;  
// ... code à mesurer ...  
ElapsedTime results = timer.Reset();  

cout << "Durée : " << results.ToString(); // "1.534 ms"  
```  

---

## 💡 Exemples Complets  

### Mesure de Performance  
```cpp  
#include <Nkentseu/Epoch/Chrono.h>  

void BenchmarkFunction() {  
    nkentseu::Chrono chrono;  

    // Code à profiler  
    ProcessData();  

    auto result = chrono.Elapsed();  
    std::cout << "Temps écoulé : " << result.ToString() << "\n";  
}  
```  

---

## 📦 Intégration avec Premake  
Ajoutez cette configuration à votre `premake5.lua` :  
```lua   
project "MonProjet"  
    dependson { "Nkentseu" }  
    links { "Nkentseu" }  
```  

---

## 🔗 Dépendances  
- **Nkentseu-Platform** : Pour la détection de plateforme  
- **LibICU** (Linux) : Conversion de locales pour les noms de mois  
- **C++17** : Requis pour les fonctionnalités chrono avancées  

---

## 🎯 Bonnes Pratiques  
1. **Validation Temporelle** : Utilisez toujours les types Date/Time plutôt que des entiers bruts  
2. **Mesures de Performance** : Préférez `Chrono` aux solutions maison pour la précision multiplateforme  
3. **Calculs de Durées** : Utilisez `TimeSpan` pour les opérations arithmétiques complexes  

---

## 📜 Licence
```text
Copyright 2025 Rihen  
Sous licence GNU GENERAL PUBLIC LICENSE Version 3  
Utilisation commerciale soumise à autorisation écrite  
Contributions sous licence Rihen  
```

---

## 📬 Support & Communauté

**Équipe Technique**  
[rihen.universe@gmail.com](rihen.universe@gmail.com)  
📞 (+237) 693 761 773 (24/7/365)  

**Ressources :**
- [Documentation Officielle](https://rihen.com/docs/nkentseu)
- [Forum Communautaire](https://forum.rihen.com/nkentseu)
- [Dépôt GitHub](https://github.com/rihen/nkentseu)
