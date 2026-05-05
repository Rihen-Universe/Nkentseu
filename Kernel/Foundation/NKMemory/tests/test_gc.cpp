#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "NKMemory/NkGc.h"

using namespace nkentseu::memory;

namespace {

struct GcNode final : NkGcObject {
    NkGcObject* child = nullptr;
    static int destroyedCount;

    ~GcNode() override { ++destroyedCount; }

    void Trace(NkGcTracer& tracer) override {
        tracer.Mark(child);
    }
};

int GcNode::destroyedCount = 0;

} // namespace

TEST_CASE(NKMemoryGC, RootReachabilityAndCollection) {
    NkGarbageCollector gc;
    gc.Collect();
    GcNode::destroyedCount = 0;

    GcNode* rootObject = gc.New<GcNode>();
    GcNode* childObject = gc.New<GcNode>();
    ASSERT_NOT_NULL(rootObject);
    ASSERT_NOT_NULL(childObject);

    rootObject->child = childObject;
    NkGcObject* rootSlot = rootObject;
    NkGcRoot root(&rootSlot);
    gc.AddRoot(&root);

    gc.Collect();
    ASSERT_EQUAL(0, GcNode::destroyedCount);
    ASSERT_TRUE(gc.ObjectCount() >= 2u);

    rootSlot = nullptr;
    gc.RemoveRoot(&root);
    gc.Collect();

    ASSERT_TRUE(GcNode::destroyedCount >= 2);
    ASSERT_EQUAL(0, static_cast<int>(gc.ObjectCount()));
}

TEST_CASE(NKMemoryGC, MultipleCollectorsAreIndependent) {
    NkGarbageCollector gcA;
    NkGarbageCollector gcB;

    gcA.Collect();
    gcB.Collect();
    GcNode::destroyedCount = 0;

    GcNode* a = gcA.New<GcNode>();
    GcNode* b = gcB.New<GcNode>();
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);

    ASSERT_EQUAL(1, static_cast<int>(gcA.ObjectCount()));
    ASSERT_EQUAL(1, static_cast<int>(gcB.ObjectCount()));

    gcA.Delete(a);
    ASSERT_EQUAL(0, static_cast<int>(gcA.ObjectCount()));
    ASSERT_EQUAL(1, static_cast<int>(gcB.ObjectCount()));

    gcB.Collect();
    ASSERT_EQUAL(0, static_cast<int>(gcB.ObjectCount()));
    ASSERT_TRUE(GcNode::destroyedCount >= 2);
}
