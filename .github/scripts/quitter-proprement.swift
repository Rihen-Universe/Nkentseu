// quitter-proprement.swift — demander a une application de QUITTER, comme Cmd+Q
// =============================================================================
// Usage : swift quitter-proprement.swift <pid>
//
// POURQUOI : tuer le processus (SIGTERM) prouve qu'il demarre, pas qu'il sait
// s'arreter. NSRunningApplication.terminate() envoie l'evenement Apple « quit »,
// exactement celui de Cmd+Q ou du menu « Quitter » : l'application doit alors
// fermer ses fenetres, sortir de sa boucle et rendre son code. C'est le chemin
// que suivra l'etudiant, et le seul qui passe par applicationShouldTerminate:.
//
// Aucune entree souris ni clavier n'est simulee : c'est un message a UNE
// application, adresse par son PID.
//
// Sortie 0 si la demande est partie, 1 sinon.
// =============================================================================

import AppKit

guard CommandLine.arguments.count > 1, let pid = Int32(CommandLine.arguments[1]) else {
    print("usage : quitter-proprement <pid>")
    exit(2)
}

guard let application = NSRunningApplication(processIdentifier: pid) else {
    print("aucune application AppKit pour le pid \(pid)")
    exit(1)
}

let envoye = application.terminate()
print("demande de fermeture (quit) envoyee a \(application.localizedName ?? "?") : \(envoye)")
exit(envoye ? 0 : 1)
