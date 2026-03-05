# NKMemory

NKMemory is now a single production-oriented implementation (no STL containers).

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

## Notes

- Legacy trees `mem1` and `mem2` were removed.
- Memory tracking stores file/line/function/tag metadata.
- `NkMemorySystem` can report leaks and collect GC objects.
