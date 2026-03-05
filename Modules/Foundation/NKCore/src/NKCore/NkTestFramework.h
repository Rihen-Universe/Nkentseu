// ============================================================
// FILE: NkTestFramework.h
// DESCRIPTION: Universal test framework for all Nkentseu modules
// AUTHOR: Rihen
// DATE: 2026-03-05
// ============================================================
// Lightweight, header-only test framework
// No external dependencies
// ============================================================

#pragma once

#ifndef NKENTSEU_CORE_NKTESTFRAMEWORK_H_INCLUDED
#define NKENTSEU_CORE_NKTESTFRAMEWORK_H_INCLUDED

#include <cstdio>
#include <cstdint>

namespace nkentseu {
    namespace test {

        // ============================================================
        // TEST CONTEXT
        // ============================================================

        struct TestContext {
            const char* module_name;
            const char* test_name;
            uint32_t tests_passed;
            uint32_t tests_failed;
            uint32_t assertions_executed;

            TestContext(const char* module)
                : module_name(module), test_name(nullptr),
                  tests_passed(0), tests_failed(0), assertions_executed(0) {}

            void PrintHeader() const {
                printf("\n==========================================\n");
                printf("  %s Test Suite\n", module_name);
                printf("==========================================\n\n");
            }

            void PrintSummary() const {
                uint32_t total = tests_passed + tests_failed;
                float percentage = total > 0 ? (100.0f * tests_passed) / total : 0.0f;
                printf("\n==========================================\n");
                printf("  Results: %u / %u tests passed\n", tests_passed, total);
                printf("  Success rate: %.1f%%\n", percentage);
                printf("  Total assertions: %u\n", assertions_executed);
                printf("==========================================\n\n");
            }
        };

        // ============================================================
        // ASSERTION MACROS
        // ============================================================

        #define NKTEST_ASSERT(ctx, condition, message) \
            do { \
                ctx.assertions_executed++; \
                if (!(condition)) { \
                    printf("  [FAIL] %s\n", (message)); \
                    return false; \
                } \
            } while(0)

        #define NKTEST_APPROX(ctx, actual, expected, epsilon, message) \
            do { \
                ctx.assertions_executed++; \
                auto diff = (actual) - (expected); \
                if (diff < 0) diff = -diff; \
                if (diff > (epsilon)) { \
                    printf("  [FAIL] %s (got %.10f, expected %.10f, diff %.2e)\n", \
                           (message), (actual), (expected), diff); \
                    return false; \
                } \
            } while(0)

        #define NKTEST_RUN(ctx, test_func, test_name) \
            do { \
                ctx.test_name = (test_name); \
                bool result = (test_func)(ctx); \
                if (result) { \
                    printf("[PASS] %s\n", (test_name)); \
                    ctx.tests_passed++; \
                } else { \
                    printf("[FAIL] %s\n", (test_name)); \
                    ctx.tests_failed++; \
                } \
            } while(0)

        // ============================================================
        // BENCHMARK MACROS
        // ============================================================

        #include <chrono>

        struct BenchmarkResult {
            const char* name;
            uint64_t iterations;
            uint64_t nanoseconds;
            double ops_per_sec;
        };

        #define NKBENCH_START(name) \
            auto __bench_name = (name); \
            uint64_t __bench_iterations = 0; \
            auto __bench_start = std::chrono::high_resolution_clock::now();

        #define NKBENCH_ITERATE(iterations) \
            __bench_iterations = (iterations);

        #define NKBENCH_END() \
            auto __bench_end = std::chrono::high_resolution_clock::now(); \
            auto __bench_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(__bench_end - __bench_start).count();

        #define NKBENCH_RESULT() \
            BenchmarkResult{ \
                __bench_name, \
                __bench_iterations, \
                __bench_ns, \
                (__bench_iterations * 1e9) / __bench_ns \
            }

        // ============================================================
        // EXCEPTION SAFETY TEST HELPER
        // ============================================================

        template<typename TestFunc>
        bool TestExceptionSafety(const char* test_name, TestFunc func) {
            try {
                func();
                return true;
            } catch (const std::exception& e) {
                printf("  [EXCEPTION] %s: %s\n", test_name, e.what());
                return false;
            } catch (...) {
                printf("  [UNKNOWN EXCEPTION] %s\n", test_name);
                return false;
            }
        }

        // ============================================================
        // MEMORY LEAK DETECTION (Basic)
        // ============================================================

        struct MemoryTracker {
            uint64_t allocations;
            uint64_t deallocations;
            uint64_t bytes_allocated;

            MemoryTracker() : allocations(0), deallocations(0), bytes_allocated(0) {}

            void PrintReport(const char* module) {
                printf("\nMemory Report (%s):\n", module);
                printf("  Allocations:  %lu\n", allocations);
                printf("  Deallocations: %lu\n", deallocations);
                printf("  Leaked allocs: %lu\n", allocations - deallocations);
                printf("  Total bytes:   %lu\n", bytes_allocated);
            }
        };

    } // namespace test
} // namespace nkentseu

#endif // NKENTSEU_CORE_NKTESTFRAMEWORK_H_INCLUDED

