#include "NkPlatformDetect.h"
#include <iostream>
#include <string>

// ============================================================
// Exemple : Utilisation du Système de Fenêtres
// ============================================================
// 
// Ce fichier montre comment utiliser le nouveau système
// NKENTSEU_WINDOWING_* pour détecter et utiliser différents
// systèmes de fenêtres (XCB, Xlib, Wayland).
//
// Compiler avec:
//   g++ -DNKENTSEU_FORCE_WINDOWING_XCB_ONLY example_windowing.cpp -o example_xcb
//   g++ -DNKENTSEU_FORCE_WINDOWING_XLIB_ONLY example_windowing.cpp -o example_xlib
//   g++ -DNKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY example_windowing.cpp -o example_wayland
//   g++ example_windowing.cpp -o example_auto  # Auto-détection

class WindowSystem {
public:
    WindowSystem() {
        detectWindowingSystem();
    }
    
    void displayInfo() const {
        std::cout << "=====================================" << std::endl;
        std::cout << "Windowing System Information" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        std::cout << "Detected System: " << system_name << std::endl;
        
        #ifdef NKENTSEU_WINDOWING_PREFERRED
        std::cout << "Preferred System: " << NKENTSEU_WINDOWING_PREFERRED << std::endl;
        #endif
        
        std::cout << "Capabilities:" << std::endl;
        
        #ifdef NKENTSEU_WINDOWING_XCB
        std::cout << "  ✓ XCB (X11 Core Protocol)" << std::endl;
        #else
        std::cout << "  ✗ XCB (X11 Core Protocol)" << std::endl;
        #endif
        
        #ifdef NKENTSEU_WINDOWING_XLIB
        std::cout << "  ✓ Xlib (X11 Legacy)" << std::endl;
        #else
        std::cout << "  ✗ Xlib (X11 Legacy)" << std::endl;
        #endif
        
        #ifdef NKENTSEU_WINDOWING_WAYLAND
        std::cout << "  ✓ Wayland" << std::endl;
        #else
        std::cout << "  ✗ Wayland" << std::endl;
        #endif
        
        #ifdef NKENTSEU_WINDOWING_X11
        std::cout << "  ✓ X11 (XCB or Xlib)" << std::endl;
        #else
        std::cout << "  ✗ X11 (XCB or Xlib)" << std::endl;
        #endif
        
        std::cout << "=====================================" << std::endl;
    }
    
    void initializeWindow() {
        std::cout << "\nInitializing window with: " << system_name << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // Initialization spécifique au système détecté
        NKENTSEU_XCB_ONLY({
            initializeXCB();
        });
        
        NKENTSEU_XLIB_ONLY({
            initializeXlib();
        });
        
        NKENTSEU_WAYLAND_ONLY({
            initializeWayland();
        });
        
        NKENTSEU_NOT_X11({
            std::cout << "Not using X11-based windowing" << std::endl;
        });
    }
    
    void cleanup() {
        std::cout << "\nCleaning up " << system_name << " resources..." << std::endl;
        
        NKENTSEU_XCB_ONLY({
            cleanupXCB();
        });
        
        NKENTSEU_XLIB_ONLY({
            cleanupXlib();
        });
        
        NKENTSEU_WAYLAND_ONLY({
            cleanupWayland();
        });
        
        std::cout << "Cleanup complete." << std::endl;
    }

private:
    std::string system_name;
    
    void detectWindowingSystem() {
        #if defined(NKENTSEU_WINDOWING_XCB)
        system_name = "XCB";
        #elif defined(NKENTSEU_WINDOWING_XLIB)
        system_name = "Xlib";
        #elif defined(NKENTSEU_WINDOWING_WAYLAND)
        system_name = "Wayland";
        #else
        system_name = "Unknown/None";
        #endif
    }
    
    void initializeXCB() {
        std::cout << "  • Connecting to X Server via XCB..." << std::endl;
        std::cout << "  • Modern X11 Core Protocol (XCB)" << std::endl;
        std::cout << "  • Better performance than Xlib" << std::endl;
        
        // Ici, on pourrait ajouter du vrai code XCB:
        // xcb_connection_t* conn = xcb_connect(NULL, NULL);
        // if (xcb_connection_has_error(conn))
        //     std::cerr << "Failed to connect to X server" << std::endl;
        // xcb_disconnect(conn);
    }
    
    void initializeXlib() {
        std::cout << "  • Connecting to X Server via Xlib..." << std::endl;
        std::cout << "  • Legacy X11 Client Library (Xlib)" << std::endl;
        std::cout << "  • Wider compatibility, older API" << std::endl;
        
        // Ici, on pourrait ajouter du vrai code Xlib:
        // Display* display = XOpenDisplay(NULL);
        // if (display == NULL)
        //     std::cerr << "Failed to open display" << std::endl;
        // XCloseDisplay(display);
    }
    
    void initializeWayland() {
        std::cout << "  • Connecting to Wayland Compositor..." << std::endl;
        std::cout << "  • Modern Display Protocol (Wayland)" << std::endl;
        std::cout << "  • Future-proof, simpler architecture" << std::endl;
        
        // Ici, on pourrait ajouter du vrai code Wayland:
        // struct wl_display* display = wl_display_connect(NULL);
        // if (display == NULL)
        //     std::cerr << "Failed to connect to Wayland" << std::endl;
        // wl_display_disconnect(display);
    }
    
    void cleanupXCB() {
        std::cout << "  • Disconnecting from X Server (XCB)" << std::endl;
    }
    
    void cleanupXlib() {
        std::cout << "  • Closing X11 Display (Xlib)" << std::endl;
    }
    
    void cleanupWayland() {
        std::cout << "  • Disconnecting from Wayland Compositor" << std::endl;
    }
};

// ============================================================
// Exemple d'Utilisation Conditionnelle
// ============================================================

void demonstrateConditionalCode() {
    std::cout << "\n" << std::endl;
    std::cout << "Conditional Code Execution Examples:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Code XCB uniquement
    NKENTSEU_XCB_ONLY({
        std::cout << "[XCB] This code runs only when XCB is enabled" << std::endl;
    });
    
    // Code Xlib uniquement
    NKENTSEU_XLIB_ONLY({
        std::cout << "[Xlib] This code runs only when Xlib is enabled" << std::endl;
    });
    
    // Code Wayland uniquement
    NKENTSEU_WAYLAND_ONLY({
        std::cout << "[Wayland] This code runs only when Wayland is enabled" << std::endl;
    });
    
    // Code X11 (XCB ou Xlib)
    NKENTSEU_X11_ONLY({
        std::cout << "[X11] This code runs for XCB or Xlib (but not Wayland)" << std::endl;
    });
    
    // Code si PAS XCB
    NKENTSEU_NOT_XCB({
        std::cout << "[Not XCB] This code runs for Xlib or Wayland" << std::endl;
    });
    
    // Code si PAS Wayland
    NKENTSEU_NOT_WAYLAND({
        std::cout << "[Not Wayland] This code runs for XCB or Xlib" << std::endl;
    });
}

// Main
int main() {
    try {
        // Créer une instance du système de fenêtres
        WindowSystem ws;
        
        // Afficher les informations
        ws.displayInfo();
        
        // Démonstrer le code conditionnel
        demonstrateConditionalCode();
        
        // Initialiser la fenêtre
        ws.initializeWindow();
        
        // Nettoyer
        ws.cleanup();
        
        std::cout << "\n✓ Example completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << std::endl;
        return 1;
    }
}

// ============================================================
// Sorties attendues selon la compilation:
// ============================================================
//
// # Avec: g++ example_windowing.cpp -o example_auto
// Detected: XCB (ou Xlib ou Wayland selon la disponibilité)
// 
// # Avec: g++ -DNKENTSEU_FORCE_WINDOWING_XCB_ONLY example_windowing.cpp
// Detected: XCB
// [XCB] This code runs only when XCB is enabled
// [X11] This code runs for XCB or Xlib (but not Wayland)
// [Not XCB] est ignoré
// 
// # Avec: g++ -DNKENTSEU_FORCE_WINDOWING_XLIB_ONLY example_windowing.cpp
// Detected: Xlib
// [Xlib] This code runs only when Xlib is enabled
// [X11] This code runs for XCB or Xlib (but not Wayland)
// [Not Wayland] This code runs for XCB or Xlib
// 
// # Avec: g++ -DNKENTSEU_FORCE_WINDOWING_WAYLAND_ONLY example_windowing.cpp
// Detected: Wayland
// [Wayland] This code runs only when Wayland is enabled
// [X11] est ignoré
// [Not XCB] This code runs for Xlib or Wayland
