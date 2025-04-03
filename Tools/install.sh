#!/bin/bash
# @File install.sh
# @Description Installation automatisée de la toolchain Clang/LLVM multi-plateforme
# @Usage sudo ./install.sh
# @Examples:
#   ./install.sh  - Installation automatique selon l'OS
# @Author [Votre Nom]
# @Date [2025-01-20]
# @License Rihen

set -e

##############################################################################
# Fonctions d'Installation
##############################################################################

# @Function install_linux
# @Description Configuration spécifique pour les systèmes Debian/Ubuntu
# @CriticalOperation - Requiert les privilèges root
install_linux() {
    # @Section Gestion des Verrous APT
    echo "🔒 Vérification des verrous apt..."
    while sudo fuser /var/lib/dpkg/lock-frontend >/dev/null 2>&1 ; do
        echo "⚠️  Verrou apt détecté, attente 30s..."
        sleep 30
    done

    # @Section Mise à Jour Système
    echo "🔧 Mise à jour des paquets..."
    sudo apt-get -o DPkg::Lock::Timeout=60 update

    # @Section Installation LLVM
    echo "📦 Installation de Clang/LLVM 18..."
    wget https://apt.llvm.org/llvm.sh
    chmod +x llvm.sh
    sudo ./llvm.sh 18

    # @Section Dépendances Principales
    echo "📦 Installation des dépendances système..."
    sudo apt-get install -y \
        clang-18 lldb-18 lld-18 clangd-18 \
        libc++-18-dev libc++abi-18-dev \
        llvm clang lld libunwind-18-dev \
        cmake ninja-build g++-12 libstdc++-12-dev \
        libicu-dev

    echo "✅ Toolchain Linux configurée!"
}

# @Function install_macos
# @Description Configuration pour les systèmes macOS
# @Note: Requiert Homebrew pour les installations
install_macos() {
    # @Section Xcode Tools
    if ! xcode-select -p &>/dev/null; then
        echo "🛠️ Installation des Xcode Command Line Tools..."
        xcode-select --install
        until xcode-select -p &>/dev/null; do sleep 5; done
    fi

    # @Section Homebrew
    if ! command -v brew &>/dev/null; then
        echo "🍺 Installation de Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
        eval "$(/opt/homebrew/bin/brew shellenv)"
    fi

    # @Section Installation LLVM
    echo "📦 Installation des outils de compilation..."
    brew install llvm cmake ninja

    # @Section Configuration Liens Symboliques
    local BREW_LLVM_PATH="$(brew --prefix llvm)"
    sudo ln -sfn "${BREW_LLVM_PATH}/bin/clang" /usr/local/bin/clang
    sudo ln -sfn "${BREW_LLVM_PATH}/bin/clang++" /usr/local/bin/clang++
    
    echo "✅ Toolchain macOS configurée!"
}

##############################################################################
# Logique Principale
##############################################################################

# @Section Détection OS
case "$(uname -s)" in
    Linux*)  install_linux ;;
    Darwin*) install_macos ;;
    *)       echo "❌ OS non supporté" >&2; exit 1 ;;
esac

##############################################################################
# Vérification Finale
##############################################################################

# @Section Validation Installation
echo "🔍 Vérification des versions..."
clang --version
ld.lld --version 2>/dev/null || echo "⚠️  LLD non détecté"

echo "🎉 Installation réussite! Toolchain prête à l'emploi"


# Ce document, ainsi que toutes les informations qu'il contient, est protégé par la licence Rihen.
# Toute utilisation, reproduction ou diffusion, sous quelque forme que ce soit, requiert une autorisation préalable de Rihen.
# © Rihen 2025 - Tous droits réservés.