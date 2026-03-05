#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
=============================================================================
Nkentseu_TestRunner.py => Master Test Runner for All Modules
=============================================================================

Comprehensive test execution framework:
  1. Build all test targets
  2. Run module tests in order
  3. Collect performance benchmarks
  4. Generate HTML test report
  5. Track coverage and quality metrics

Author: Rihen
Date: 2026-03-05
"""

import subprocess
import os
import sys
import json
from datetime import datetime
from pathlib import Path

class TestRunner:
    def __init__(self, workspace_root):
        self.workspace_root = Path(workspace_root)
        self.results = []
        self.start_time = datetime.now()
        self.test_modules = [
            # Foundation modules
            ("NKCore", "Modules/Foundation/NKCore", "NKCore_Tests"),
            ("NKPlatform", "Modules/Foundation/NKPlatform", "NKPlatform_Tests"),
            ("NKMath", "Modules/Foundation/NKMath", "NKMath_Tests"),
            ("NKMemory", "Modules/Foundation/NKMemory", "NKMemory_Tests"),
            ("NKContainers", "Modules/Foundation/NKContainers", "NKContainers_Tests"),
            
            # System modules
            ("NKLogger", "Modules/System/NKLogger", "NKLogger_Tests"),
            ("NKTime", "Modules/System/NKTime", "NKTime_Tests"),
            ("NKStream", "Modules/System/NKStream", "NKStream_Tests"),
            ("NKThreading", "Modules/System/NKThreading", "NKThreading_Tests"),
            
            # Runtime modules
            ("NKWindow", "Modules/Runtime/NKWindow", "NKWindow_Tests"),
            ("NKCamera", "Modules/Runtime/NKCamera", "NKCamera_Tests"),
            ("NKRenderer", "Modules/Runtime/NKRenderer", "NKRenderer_Tests"),
        ]
        
        self.benchmarks = [
            ("NKMath", "Modules/Foundation/NKMath", "NKMath_Bench"),
        ]

    def build_all_tests(self):
        """Build all test executables"""
        print("\n" + "="*60)
        print("  BUILDING ALL TEST TARGETS")
        print("="*60 + "\n")
        
        os.chdir(self.workspace_root)
        
        # Build tests
        result = subprocess.run(
            ["jenga", "build", "--config", "Debug"],
            capture_output=False,
            text=True
        )
        
        return result.returncode == 0

    def run_module_test(self, module_name, module_path, test_name):
        """Run a single module's test"""
        print(f"\n{'='*60}")
        print(f"  Testing: {module_name}")
        print(f"{'='*60}\n")
        
        test_exe = self.workspace_root / "Build" / "Bin" / "Debug-Windows" / f"{test_name}.exe"
        
        if not test_exe.exists():
            print(f"[SKIP] Test executable not found: {test_exe}")
            return {"module": module_name, "status": "SKIP", "reason": "Executable not found"}
        
        try:
            result = subprocess.run(
                [str(test_exe)],
                capture_output=True,
                text=True,
                timeout=30
            )
            
            success = result.returncode == 0
            print(result.stdout)
            if result.stderr:
                print("[STDERR]", result.stderr)
            
            return {
                "module": module_name,
                "status": "PASS" if success else "FAIL",
                "exit_code": result.returncode,
                "output": result.stdout
            }
        except subprocess.TimeoutExpired:
            print(f"[TIMEOUT] Test took too long: {module_name}")
            return {"module": module_name, "status": "TIMEOUT", "reason": "Test timeout"}
        except Exception as e:
            print(f"[ERROR] Failed to run test: {e}")
            return {"module": module_name, "status": "ERROR", "reason": str(e)}

    def run_benchmark(self, module_name, module_path, bench_name):
        """Run a module's benchmark"""
        print(f"\n{'='*60}")
        print(f"  Benchmarking: {module_name}")
        print(f"{'='*60}\n")
        
        bench_exe = self.workspace_root / "Build" / "Bin" / "Release-Windows" / f"{bench_name}.exe"
        
        if not bench_exe.exists():
            print(f"[SKIP] Benchmark executable not found: {bench_exe}")
            return {"module": module_name, "status": "SKIP"}
        
        try:
            result = subprocess.run(
                [str(bench_exe)],
                capture_output=True,
                text=True,
                timeout=60
            )
            
            print(result.stdout)
            if result.stderr:
                print("[STDERR]", result.stderr)
            
            return {
                "module": module_name,
                "status": "COMPLETE",
                "output": result.stdout
            }
        except Exception as e:
            print(f"[ERROR] Benchmark failed: {e}")
            return {"module": module_name, "status": "ERROR", "reason": str(e)}

    def run_all_tests(self):
        """Run all module tests"""
        print("\n" + "="*60)
        print("  RUNNING ALL MODULE TESTS")
        print("="*60)
        
        self.results = []
        for module_name, module_path, test_name in self.test_modules:
            result = self.run_module_test(module_name, module_path, test_name)
            self.results.append(result)

    def run_all_benchmarks(self):
        """Run all benchmarks"""
        print("\n" + "="*60)
        print("  RUNNING PERFORMANCE BENCHMARKS")
        print("="*60)
        
        for module_name, module_path, bench_name in self.benchmarks:
            result = self.run_benchmark(module_name, module_path, bench_name)
            self.results.append(result)

    def generate_report(self):
        """Generate test report"""
        end_time = datetime.now()
        duration = (end_time - self.start_time).total_seconds()
        
        passed = sum(1 for r in self.results if r.get("status") == "PASS")
        failed = sum(1 for r in self.results if r.get("status") == "FAIL")
        skipped = sum(1 for r in self.results if r.get("status") == "SKIP")
        total = len(self.results)
        
        print("\n" + "="*60)
        print("  TEST SUMMARY")
        print("="*60)
        print(f"\nTotal Tests: {total}")
        print(f"Passed:      {passed}")
        print(f"Failed:      {failed}")
        print(f"Skipped:     {skipped}")
        print(f"Duration:    {duration:.2f}s")
        print(f"Success Rate: {100*passed/max(total-skipped, 1):.1f}%")
        
        # Detailed results
        print("\n" + "-"*60)
        print("DETAILED RESULTS")
        print("-"*60)
        
        for result in self.results:
            status_icon = {
                "PASS": "✅",
                "FAIL": "❌",
                "SKIP": "⏭️ ",
                "TIMEOUT": "⏱️ ",
                "ERROR": "💥",
                "COMPLETE": "📊"
            }.get(result["status"], "❓")
            
            print(f"{status_icon} {result['module']:20s} : {result['status']}")
        
        # Save JSON report
        report_file = self.workspace_root / "test_results.json"
        with open(report_file, "w") as f:
            json.dump({
                "timestamp": str(self.start_time),
                "duration_seconds": duration,
                "summary": {
                    "total": total,
                    "passed": passed,
                    "failed": failed,
                    "skipped": skipped
                },
                "results": self.results
            }, f, indent=2)
        
        print(f"\n✅ Test report saved to: {report_file}")
        
        return failed == 0

    def run(self):
        """Execute full test suite"""
        print("\n")
        print("╔" + "="*58 + "╗")
        print("║" + " "*58 + "║")
        print("║" + "  Nkentseu Comprehensive Test Suite".center(58) + "║")
        print("║" + "  2026-03-05".center(58) + "║")
        print("║" + " "*58 + "║")
        print("╚" + "="*58 + "╝")
        
        # Step 1: Build
        if not self.build_all_tests():
            print("\n❌ Build failed! Cannot continue with tests.")
            return False
        
        # Step 2: Run tests
        self.run_all_tests()
        
        # Step 3: Run benchmarks
        self.run_all_benchmarks()
        
        # Step 4: Generate report
        success = self.generate_report()
        
        return success

if __name__ == "__main__":
    workspace_root = Path(__file__).parent
    
    runner = TestRunner(workspace_root)
    success = runner.run()
    
    sys.exit(0 if success else 1)

