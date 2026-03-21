#!/usr/bin/env python3
"""
CodeCollector Pro - Collection, Analyse et Reconstruction de code
Usage: python CodeCollectorPro.py [MODE] [OPTIONS]
"""

import os
import sys
import argparse
import json
import ast
import tokenize
from pathlib import Path
from typing import List, Set, Dict, Any, Optional
from dataclasses import dataclass

@dataclass
class CodeAnalysis:
    """Résultats d'analyse de code"""
    TotalFiles: int
    TotalLines: int
    TotalFunctions: int
    TotalClasses: int
    FileTypes: Dict[str, int]
    ComplexFiles: List[str]
    LargeFiles: List[str]
    AnalysisErrors: List[str]

class CodeCollectorPro:
    def __init__(self, BaseDirectory: str = "."):
        self._BaseDirectory = Path(BaseDirectory).resolve()
        self._IncludedExtensions: Set[str] = set()
        self._ExcludedExtensions: Set[str] = set()
        self._IncludedPaths: Set[Path] = set()
        self._ExcludedPaths: Set[Path] = set()
        self._IncludedFiles: Set[Path] = set()
        self._ExcludedFiles: Set[Path] = set()
        self._OutputFile = "code_collection.txt"
        self._MetadataFile = "code_metadata.json"
        self._Verbose = False
        
    # Méthodes de configuration (existant)
    def SetIncludedExtensions(self, Extensions: List[str]) -> None:
        self._IncludedExtensions = {ext.lower().lstrip('.') for ext in Extensions}
        
    def SetExcludedExtensions(self, Extensions: List[str]) -> None:
        self._ExcludedExtensions = {ext.lower().lstrip('.') for ext in Extensions}
        
    def SetIncludedPaths(self, Paths: List[str]) -> None:
        self._IncludedPaths = {self._BaseDirectory / path for path in Paths}
        
    def SetExcludedPaths(self, Paths: List[str]) -> None:
        self._ExcludedPaths = {self._BaseDirectory / path for path in Paths}
        
    def SetIncludedFiles(self, Files: List[str]) -> None:
        self._IncludedFiles = {self._BaseDirectory / file for file in Files}
        
    def SetExcludedFiles(self, Files: List[str]) -> None:
        self._ExcludedFiles = {self._BaseDirectory / file for file in Files}
        
    def SetOutputFile(self, OutputFile: str) -> None:
        self._OutputFile = OutputFile
        
    def SetVerbose(self, Verbose: bool) -> None:
        self._Verbose = Verbose
        
    def _Log(self, Message: str) -> None:
        if self._Verbose:
            print(f"[INFO] {Message}")

    # Méthodes existantes de collection
    def _ShouldIncludeFile(self, FilePath: Path) -> bool:
        if self._IncludedFiles and FilePath in self._IncludedFiles:
            return True
        if FilePath in self._ExcludedFiles:
            return False
            
        Extension = FilePath.suffix.lower().lstrip('.')
        
        if self._IncludedExtensions and Extension not in self._IncludedExtensions:
            return False
        if Extension in self._ExcludedExtensions:
            return False
            
        for ExcludedPath in self._ExcludedPaths:
            if FilePath.is_relative_to(ExcludedPath):
                return False
                
        if self._IncludedPaths:
            for IncludedPath in self._IncludedPaths:
                if FilePath.is_relative_to(IncludedPath):
                    return True
            return False
            
        return True
        
    def _GetFileStructure(self) -> Dict[Path, List[Path]]:
        Structure: Dict[Path, List[Path]] = {}
        
        if self._IncludedFiles:
            for FilePath in self._IncludedFiles:
                if FilePath.exists() and FilePath.is_file():
                    ParentDir = FilePath.parent
                    if ParentDir not in Structure:
                        Structure[ParentDir] = []
                    Structure[ParentDir].append(FilePath)
        
        for Root, Dirs, Files in os.walk(self._BaseDirectory):
            RootPath = Path(Root)
            Dirs[:] = [d for d in Dirs if not any((RootPath / d) == excluded or (RootPath / d).is_relative_to(excluded) for excluded in self._ExcludedPaths)]
            
            FilePaths: List[Path] = []
            for File in Files:
                FilePath = RootPath / File
                if self._IncludedFiles:
                    if FilePath in self._IncludedFiles:
                        FilePaths.append(FilePath)
                elif self._ShouldIncludeFile(FilePath):
                    FilePaths.append(FilePath)
                    
            if FilePaths:
                Structure[RootPath] = FilePaths
                
        return Structure

    # 🔄 NOUVEAU : Méthodes de Reconstruction
    def RestoreFromCollection(self, CollectionFile: str, TargetDirectory: str = "restored_project") -> bool:
        """Reconstruit la structure de projet à partir d'un fichier de collection"""
        try:
            RestorePath = Path(TargetDirectory)
            RestorePath.mkdir(parents=True, exist_ok=True)
            
            self._Log(f"Restauration vers: {RestorePath}")
            
            with open(CollectionFile, 'r', encoding='utf-8') as f:
                Content = f.read()
                
            # Parser le fichier de collection
            CurrentFile = None
            FileContent = []
            FilesRestored = 0
            
            for Line in Content.split('\n'):
                if Line.startswith('[FICHIER:'):
                    # Sauvegarder le fichier précédent
                    if CurrentFile and FileContent:
                        self._SaveRestoredFile(RestorePath / CurrentFile, '\n'.join(FileContent))
                        FilesRestored += 1
                    
                    # Nouveau fichier
                    FilePath = Line[9:-1].strip()  # Extraire le chemin du fichier
                    CurrentFile = Path(FilePath)
                    FileContent = []
                    
                elif Line.startswith('=' * 50) and CurrentFile:
                    # Fin du contenu du fichier
                    if FileContent and CurrentFile:
                        self._SaveRestoredFile(RestorePath / CurrentFile, '\n'.join(FileContent))
                        FilesRestored += 1
                    CurrentFile = None
                    FileContent = []
                    
                elif CurrentFile is not None and not Line.startswith('=' * 50):
                    FileContent.append(Line)
            
            self._Log(f"Fichiers restaurés: {FilesRestored}")
            return True
            
        except Exception as Error:
            print(f"❌ Erreur lors de la restauration: {Error}")
            return False
            
    def _SaveRestoredFile(self, FilePath: Path, Content: str) -> None:
        """Sauvegarde un fichier restauré"""
        FilePath.parent.mkdir(parents=True, exist_ok=True)
        with open(FilePath, 'w', encoding='utf-8') as f:
            f.write(Content.strip())
        self._Log(f"Fichier restauré: {FilePath}")

    # 📊 NOUVEAU : Méthodes d'Analyse
    def AnalyzeCode(self) -> CodeAnalysis:
        """Analyse la complexité et la structure du code"""
        Structure = self._GetFileStructure()
        Analysis = CodeAnalysis(
            TotalFiles=0,
            TotalLines=0,
            TotalFunctions=0,
            TotalClasses=0,
            FileTypes={},
            ComplexFiles=[],
            LargeFiles=[],
            AnalysisErrors=[]
        )
        
        for Directory, Files in Structure.items():
            for FilePath in Files:
                try:
                    FileStats = self._AnalyzeSingleFile(FilePath)
                    Analysis.TotalFiles += 1
                    Analysis.TotalLines += FileStats['lines']
                    Analysis.TotalFunctions += FileStats['functions']
                    Analysis.TotalClasses += FileStats['classes']
                    
                    # Statistiques par type de fichier
                    Extension = FilePath.suffix.lower()
                    Analysis.FileTypes[Extension] = Analysis.FileTypes.get(Extension, 0) + 1
                    
                    # Fichiers complexes/lourds
                    if FileStats['functions'] > 10:
                        Analysis.ComplexFiles.append(str(FilePath.relative_to(self._BaseDirectory)))
                    if FileStats['lines'] > 500:
                        Analysis.LargeFiles.append(str(FilePath.relative_to(self._BaseDirectory)))
                        
                except Exception as Error:
                    ErrorMsg = f"{FilePath.relative_to(self._BaseDirectory)}: {Error}"
                    Analysis.AnalysisErrors.append(ErrorMsg)
                    
        return Analysis
        
    def _AnalyzeSingleFile(self, FilePath: Path) -> Dict[str, int]:
        """Analyse un fichier individuel"""
        try:
            with open(FilePath, 'r', encoding='utf-8') as f:
                Content = f.read()
                
            Lines = Content.count('\n') + 1
            Functions = Content.count('def ')
            Classes = Content.count('class ')
            
            # Analyse basique pour JavaScript/TypeScript
            if FilePath.suffix.lower() in ['.js', '.jsx', '.ts', '.tsx']:
                Functions = Content.count('function ') + Content.count('=>') // 2
                Classes = Content.count('class ')
                
            return {
                'lines': Lines,
                'functions': Functions,
                'classes': Classes
            }
            
        except Exception as Error:
            return {'lines': 0, 'functions': 0, 'classes': 0}

    def GenerateReport(self, Analysis: CodeAnalysis) -> str:
        """Génère un rapport d'analyse"""
        Report = [
            "📊 RAPPORT D'ANALYSE DE CODE",
            "=" * 50,
            f"📁 Fichiers analysés: {Analysis.TotalFiles}",
            f"📏 Lignes de code: {Analysis.TotalLines}",
            f"🔧 Fonctions/Méthodes: {Analysis.TotalFunctions}",
            f"🏗️ Classes/Composants: {Analysis.TotalClasses}",
            "",
            "📈 Répartition par type:"
        ]
        
        for Ext, Count in sorted(Analysis.FileTypes.items()):
            Report.append(f"  {Ext or 'sans extension'}: {Count} fichiers")
            
        if Analysis.ComplexFiles:
            Report.extend(["", "⚠️ Fichiers complexes (plus de 10 fonctions):"])
            for File in Analysis.ComplexFiles[:5]:  # Top 5 seulement
                Report.append(f"  • {File}")
                
        if Analysis.LargeFiles:
            Report.extend(["", "📦 Fichiers volumineux (plus de 500 lignes):"])
            for File in Analysis.LargeFiles[:5]:
                Report.append(f"  • {File}")
                
        if Analysis.AnalysisErrors:
            Report.extend(["", "❌ Erreurs d'analyse:"])
            for Error in Analysis.AnalysisErrors[:3]:
                Report.append(f"  • {Error}")
                
        return '\n'.join(Report)

    # Méthode principale de collection (améliorée)
    def CollectCode(self) -> bool:
        """Collecte le code avec métadonnées"""
        try:
            self._Log("Début de la collection...")
            
            if not self._BaseDirectory.exists():
                print(f"❌ Le répertoire {self._BaseDirectory} n'existe pas")
                return False
                
            Structure = self._GetFileStructure()
            
            if not Structure:
                print("Aucun fichier trouvé avec les critères spécifiés")
                return False
                
            # Générer les métadonnées pour la reconstruction
            Metadata = {
                "base_directory": str(self._BaseDirectory),
                "included_extensions": list(self._IncludedExtensions),
                "excluded_extensions": list(self._ExcludedExtensions),
                "included_paths": [str(p.relative_to(self._BaseDirectory)) for p in self._IncludedPaths],
                "excluded_paths": [str(p.relative_to(self._BaseDirectory)) for p in self._ExcludedPaths],
                "included_files": [str(p.relative_to(self._BaseDirectory)) for p in self._IncludedFiles],
                "excluded_files": [str(p.relative_to(self._BaseDirectory)) for p in self._ExcludedFiles],
                "structure": {str(k.relative_to(self._BaseDirectory)): [str(f.relative_to(self._BaseDirectory)) for f in v] 
                             for k, v in Structure.items()},
                "timestamp": str(os.path.getctime(__file__))
            }
            
            with open(self._OutputFile, 'w', encoding='utf-8') as Output:
                # En-tête avec métadonnées
                Output.write("ANALYSE DE CODE - COLLECTION AVEC MÉTADONNÉES\n")
                Output.write(f"Répertoire de base: {self._BaseDirectory}\n")
                Output.write("MÉTADONNÉES POUR RECONSTRUCTION:\n")
                Output.write(json.dumps(Metadata, indent=2))
                Output.write("\n" + "=" * 80 + "\n\n")
                
                # Contenu des fichiers
                for Directory, Files in sorted(Structure.items()):
                    RelativeDir = Directory.relative_to(self._BaseDirectory)
                    Output.write(f"\n📁 DOSSIER: {RelativeDir}\n")
                    Output.write("─" * 50 + "\n")
                    
                    for FilePath in sorted(Files):
                        self._Log(f"Traitement: {FilePath}")
                        self._WriteFileContent(FilePath, Output)
                        
            # Sauvegarder les métadonnées séparément
            with open(self._MetadataFile, 'w', encoding='utf-8') as f:
                json.dump(Metadata, f, indent=2)
                
            # Générer un rapport d'analyse
            Analysis = self.AnalyzeCode()
            Report = self.GenerateReport(Analysis)
            
            print("📊 Rapport d'analyse:")
            print(Report)
            
            self._Log(f"Collection terminée: {self._OutputFile}")
            return True
            
        except Exception as Error:
            print(f"Erreur lors de la collection: {Error}")
            return False
            
    def _WriteFileContent(self, FilePath: Path, OutputHandle) -> None:
        try:
            with open(FilePath, 'r', encoding='utf-8', errors='ignore') as f:
                Content = f.read().strip()
                
            if Content:
                RelativePath = FilePath.relative_to(self._BaseDirectory)
                OutputHandle.write(f"\n[FICHIER: {RelativePath}]\n")
                OutputHandle.write("=" * 50 + "\n")
                OutputHandle.write(Content)
                OutputHandle.write("\n" + "=" * 50 + "\n\n")
                
        except Exception as Error:
            self._Log(f"Erreur lecture {FilePath}: {Error}")

class AdvancedArgumentParser:
    def __init__(self):
        self.Parser = argparse.ArgumentParser(
            description="CodeCollector Pro - Collection, Analyse et Reconstruction",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Exemples d'utilisation:

  # Collection avec analyse
  python CodeCollectorPro.py collect -i py js -x node_modules -v

  # Reconstruction d'un projet
  python CodeCollectorPro.py restore code_collection.txt -t mon_projet_restaure

  # Analyse seule
  python CodeCollectorPro.py analyze -i py js

  # Statistiques détaillées
  python CodeCollectorPro.py stats -i py js
            """
        )
        self._SetupArguments()
        
    def _SetupArguments(self) -> None:
        Subparsers = self.Parser.add_subparsers(dest='command', help='Commandes disponibles')
        
        # Parser pour la collection
        CollectParser = Subparsers.add_parser('collect', help='Collecter le code')
        self._AddCollectionArguments(CollectParser)
        
        # Parser pour la restauration
        RestoreParser = Subparsers.add_parser('restore', help='Restaurer un projet')
        RestoreParser.add_argument('input_file', help='Fichier de collection à restaurer')
        RestoreParser.add_argument('-t', '--target', default='restored_project', 
                                 help='Dossier cible pour la restauration')
        
        # Parser pour l'analyse
        AnalyzeParser = Subparsers.add_parser('analyze', help='Analyser le code')
        self._AddCollectionArguments(AnalyzeParser)
        
        # Parser pour les statistiques
        StatsParser = Subparsers.add_parser('stats', help='Afficher les statistiques')
        self._AddCollectionArguments(StatsParser)
        
    def _AddCollectionArguments(self, Parser):
        Parser.add_argument('-d', '--directory', default='.', help='Répertoire de base')
        Parser.add_argument('-i', '--include-ext', nargs='+', help='Extensions à inclure')
        Parser.add_argument('-e', '--exclude-ext', nargs='+', default=[], help='Extensions à exclure')
        Parser.add_argument('-x', '--exclude-paths', nargs='+', default=[], help='Chemins à exclure')
        Parser.add_argument('-ip', '--include-paths', nargs='+', default=[], help='Chemins à inclure')
        Parser.add_argument('-if', '--include-files', nargs='+', default=[], help='Fichiers à inclure')
        Parser.add_argument('-ef', '--exclude-files', nargs='+', default=[], help='Fichiers à exclure')
        Parser.add_argument('-o', '--output', default='code_collection.txt', help='Fichier de sortie')
        Parser.add_argument('-v', '--verbose', action='store_true', help='Mode verbeux')
        
    def ParseArguments(self):
        return self.Parser.parse_args()

def Main():
    Parser = AdvancedArgumentParser()
    Args = Parser.ParseArguments()
    
    if not Args.command:
        Parser.Parser.print_help()
        return
    
    if Args.command == 'collect':
        Collector = CodeCollectorPro(Args.directory)
        _ConfigureCollector(Collector, Args)
        Success = Collector.CollectCode()
        
    elif Args.command == 'restore':
        Collector = CodeCollectorPro()
        Success = Collector.RestoreFromCollection(Args.input_file, Args.target)
        if Success:
            print(f"✅ Projet restauré dans: {Args.target}")
        
    elif Args.command == 'analyze':
        Collector = CodeCollectorPro(Args.directory)
        _ConfigureCollector(Collector, Args)
        Analysis = Collector.AnalyzeCode()
        Report = Collector.GenerateReport(Analysis)
        print(Report)
        
    elif Args.command == 'stats':
        Collector = CodeCollectorPro(Args.directory)
        _ConfigureCollector(Collector, Args)
        Structure = Collector._GetFileStructure()
        TotalFiles = sum(len(files) for files in Structure.values())
        TotalDirs = len(Structure)
        print(f"📊 Statistiques:")
        print(f"   Dossiers: {TotalDirs}")
        print(f"   Fichiers: {TotalFiles}")

def _ConfigureCollector(Collector: CodeCollectorPro, Args):
    if Args.include_ext:
        Collector.SetIncludedExtensions(Args.include_ext)
    if Args.exclude_ext:
        Collector.SetExcludedExtensions(Args.exclude_ext)
    if Args.exclude_paths:
        Collector.SetExcludedPaths(Args.exclude_paths)
    if Args.include_paths:
        Collector.SetIncludedPaths(Args.include_paths)
    if Args.include_files:
        Collector.SetIncludedFiles(Args.include_files)
    if Args.exclude_files:
        Collector.SetExcludedFiles(Args.exclude_files)
    Collector.SetOutputFile(Args.output)
    Collector.SetVerbose(Args.verbose)

if __name__ == "__main__":
    Main()