#include "Nova.h"

#include <iostream>
#include <thread>

class GameObject {
    public:
        GameObject(int id) : id(id) {}
        void Update() { /* ... */ }
        int GetId() const { return id; }
    private:
        int id;
};

void BasicUsage() {

    // Allocation normale
    GameObject* obj = Memorys.New<GameObject>("Main Player", 123);
    
    // Utilisation de l'objet
    obj->Update();
    
    // Libération explicite
    int id = obj->GetId();
    logger.Info("Libération de l'objet : {0}", id);
    Memorys.Delete(obj);
    logger.Info("Objet libéré : {0}", id);
}

void ArrayAllocation() {
    try {
        // Allocation d'un tableau de 10 entiers
        int* numbers = Memorys.NewArray<int>("Level Data", 10, 0);
        
        Memorys.Delete(numbers);
    }
    catch (...) {
        // Le destructeur des éléments et la libération sont gérés automatiquement
    }
}

void ThreadTask(int p) {
    for (int i = 0; i < 10; ++i) {
        auto* data = Memorys.New<std::string>("Thread Data", "temp");
        // Opérations thread-safe
        Memorys.Delete(data);
    }

    NK_UNUSED p;
}

void ConcurrentUsage() {
    std::thread t1(ThreadTask, 0);
    std::thread t2(ThreadTask, 1);
    
    t1.join();
    t2.join();
    
    // Aucune fuite malgré l'accès concurrent
}

class Base {
    public:
    virtual ~Base() = default;
};

class Derived : public Base {
    char* buffer;
public:
    Derived() { buffer = Memorys.NewArray<char>("Derived Buffer", 512); }
    ~Derived() {
        Memorys.Delete(buffer);
    }
};

void PolymorphicUsage() {
    Base* obj = Memorys.New<Derived>("Polymorphic Object");
    Memorys.Delete(obj); // Appel correct au destructeur dérivé
}

class Player {
    public:
        Player(Player&&) = default;

        Player(std::string name, int level) 
            : m_name(std::move(name)), m_level(level) {
            logger.Info("Création Player: {0}", m_name);
        }
    
        void LevelUp() {
            m_level++;
            logger.Debug("{0} level up -> {1}", m_name, m_level);
        }
    
        ~Player() {
            logger.Warning("Destruction Player: {0}", m_name);
        }
    
    private:
        std::string m_name;
        int m_level;
};

void TestUniquePlayer() {
    auto player = Memorys.MakeUnique<Player>("Aragorn", "Aragorn", 1);
    player->LevelUp();
    // Transfert de propriété
    auto newOwner = std::move(player);
}

class PlasmaGun {
    int m_ammo;
    int m_maxAmmo;
    public:
        PlasmaGun(int maxAmmo) 
            : m_ammo(maxAmmo), m_maxAmmo(maxAmmo) {
                logger.Info("Arme chargée: {0}", m_maxAmmo);
        }
    
        bool Fire() {
            if(m_ammo == 0) return false;
            m_ammo--;
            logger.Debug("Tir! Restant: {0}", (m_ammo*100)/m_maxAmmo);
            return true;
        }
    
        ~PlasmaGun() {
            logger.Errors("Arme surchauffe!");
        }
};

void CombatSystem(nkentseu::SharedPtr<PlasmaGun> gun) {
    gun->Fire();
}

void TestSharedWeapon() {
    auto gun = Memorys.MakeShared<PlasmaGun>("PlasmaGun", 100);
    CombatSystem(gun); // Partage sécurisé
    gun->Fire();
}

class TextureResource {
    public:
        explicit TextureResource(std::string path) 
            : m_path(std::move(path)) {
                logger.Info("Texture chargée: {0}", m_path);
        }
    
        void Bind() const {
            logger.Debug("Texture active: {0}", m_path);
        }
    
        ~TextureResource() {
            logger.Warning("Libération texture: {0}", m_path);
        }
    
    private:
        std::string m_path;
};

nkentseu::UniquePtr<TextureResource> LoadTexture() {
    return Memorys.MakeUnique<TextureResource>("metal_plate", "metal_plate.dds");
}

void TestTextureTransfer() {
    auto texture = LoadTexture();
    texture->Bind();
}

struct Particle {
    float x, y;
    float velocity;
    
    void Update() {
        x += velocity;
        logger.Debug("Particule [{0}, {1}]", x, y);
    }
};

void TestParticleStorm() {
    auto particles = Memorys.MakeUniqueArray<Particle>("Feu dartifice", 500);
    
    for(int i = 0; i < 500; i++) {
        particles[i].x = 0;
        particles[i].y = 0;
        particles[i].velocity = rand() % 100 * 0.1f;
    }
}

class LevelData {
    public:
        explicit LevelData(int size) : m_data(Memorys.MakeUniqueArray<int>("Sauvegarde", size)){
            if(size > 1000) throw std::runtime_error("Taille invalide");
        }
    
        ~LevelData() {
            logger.Info("Niveau déchargé");
        }
    
    private:
        nkentseu::UniquePtr<int> m_data;
};

void TestLevelLoading() {
    try {
        auto level = Memorys.MakeShared<LevelData>("LevelData", 1500); // Lance une exception
    }
    catch(const std::exception& e) {
        logger.Errors("Erreur niveau: {0}", e.what());
    }
}

class Enemy {
    public:
        Enemy(){}
        virtual ~Enemy() = default;
        virtual void Attack() = 0;
};
    
class Goblin : public Enemy {
    public:
        Goblin() : Enemy() {}
        void Attack() override {
            logger.Debug("Goblin attaque avec une massue!");
        }
        
        ~Goblin() {
            logger.Info("Goblin meurt");
        }
};

void TestPolymorphicEnemy() {
    // Conversion implicite sécurisée
    nkentseu::SharedPtr<Enemy> enemy = Memorys.MakeShared<Goblin>("Goblin");
    enemy->Attack();

    // Affectation entre types compatibles
    nkentseu::SharedPtr<Goblin> goblin = Memorys.MakeShared<Goblin>("Goblin Elite");
    nkentseu::SharedPtr<Enemy> genericEnemy;
    genericEnemy = goblin; // Affectation polymorphe
}

int main() {
    // Initialisation du système
    logger.Trace("[Initialisation] Demarrage du systeme de journalisation (Logger)...");
    logger.Trace("[Initialisation] Mise en place du gestionnaire memoire...\n\n");
    Memorys.Initialize();

    
    //nkentseu::Vector2fTest vector2Test;
    nkentseu::StringTest stringTest;
    
    // nkentseu::StringUtilsTest stringUtilTest;
    // nkentseu::StringConvertTest stringConverterTest;
    nkentseu::int32 result = nkentseu::RunTest();

    
    logger.Trace("[Nettoyage] Liberation des cibles du logger...");
    logger.ClearTargetsAndFlush();

    logger.Trace("[Fermeture] Fin normale de l'execution du programme.");
    logger.Trace("[Fermeture] Destruction du systeme de journalisation...");
    logger.Shutdown();

    logger.Trace("[Nettoyage] Arret du systeme de memoire...\n");
    Memorys.Shutdown();
    
    return result;
}