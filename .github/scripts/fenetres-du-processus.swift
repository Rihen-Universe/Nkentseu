// fenetres-du-processus.swift — les fenetres qu'un processus a REELLEMENT a l'ecran
// =============================================================================
// Usage : swift fenetres-du-processus.swift <pid>
//
// POURQUOI : « le processus tourne encore » ne prouve pas qu'une fenetre existe
// (une boucle d'evenements peut tourner sans NSWindow). CoreGraphics liste les
// fenetres du serveur de fenetres avec le PID de leur proprietaire : si la
// liste est vide, la fenetre n'a pas ete creee, quoi que dise le journal.
//
// LIMITE : depuis macOS 10.15, le TITRE d'une fenetre exige l'autorisation
// « Enregistrement de l'ecran », que le runner n'a peut-etre pas ; la taille et
// le PID restent lisibles sans elle. Sortie 0 si au moins une fenetre, 1 sinon.
// =============================================================================

import CoreGraphics
import Foundation

guard CommandLine.arguments.count > 1, let pid = Int(CommandLine.arguments[1]) else {
    print("usage : fenetres-du-processus <pid>")
    exit(2)
}

let options: CGWindowListOption = [.optionAll]
guard let liste = CGWindowListCopyWindowInfo(options, kCGNullWindowID) as? [[String: Any]] else {
    print("CGWindowListCopyWindowInfo : aucune liste")
    exit(1)
}

var trouvees = 0
for fenetre in liste {
    guard let proprietaire = fenetre[kCGWindowOwnerPID as String] as? Int, proprietaire == pid else {
        continue
    }
    let titre = fenetre[kCGWindowName as String] as? String ?? "(titre illisible)"
    let bornes = fenetre[kCGWindowBounds as String] as? [String: Any] ?? [:]
    let largeur = bornes["Width"] as? Double ?? 0
    let hauteur = bornes["Height"] as? Double ?? 0
    let calque = fenetre[kCGWindowLayer as String] as? Int ?? -1
    let visible = fenetre[kCGWindowIsOnscreen as String] as? Bool ?? false
    // Le calque 0 est celui des fenetres d'application ; la barre de menus et
    // les elements systeme vivent ailleurs et ne comptent pas.
    if calque == 0 && largeur > 1 && hauteur > 1 {
        trouvees += 1
    }
    print("fenetre : « \(titre) » \(Int(largeur))x\(Int(hauteur)) calque=\(calque) a_l_ecran=\(visible)")
}

print("fenetres d'application du processus \(pid) : \(trouvees)")
exit(trouvees > 0 ? 0 : 1)
