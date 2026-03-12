# NKMemory

NKMemory provides a production-oriented core plus advanced allocators/profiler/tracker that are compiled and actively hardened.

## Main Components

- `NkAllocator.h/.cpp`
  - `NkAllocator` interface
  - `NkMallocAllocator` default backend
  - `NkLinearAllocator` frame/transient allocator
- `NkUtils.h/.cpp`
  - memory helpers (`NkMemCopy`, `NkMemMove`, `NkMemSet`, alignment)
- `NkUniquePtr.h`
  - unique ownership smart pointer
- `NkSharedPtr.h`
  - shared/weak ownership smart pointers (atomic ref-count)
- `NkIntrusivePtr.h`
  - intrusive ref-counted pointer
- `NkGc.h/.cpp`
  - simple mark/sweep collector with explicit roots
- `NkMemory.h/.cpp`
  - tracked allocation system, leak dump, stats, typed `New/Delete`
- `NkPoolAllocator.h/.cpp`
  - fixed-size and variable-size pool allocators
- `NkMultiLevelAllocator.h/.cpp`
  - tiered allocator dispatching tiny/small/medium/large allocations
- `NkProfiler.h/.cpp`
  - runtime allocation/free/realloc hooks and global stats
- `NkTracker.h/.cpp`
  - O(1) allocation tracker and leak reporting

## Notes

- Legacy trees `mem1` and `mem2` were removed.
- Memory tracking stores file/line/function/tag metadata and is indexed in O(1).
- `NkMemorySystem` can report leaks, profile allocations, and collect GC objects.
