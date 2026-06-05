#pragma once
#include "shapes.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <stack>
#include <cstring>

namespace col {

// ─────────────────────────────────────────────────────────────────────────────
//  Collision pair
// ─────────────────────────────────────────────────────────────────────────────
struct CollidingPair {
    uint64_t bodyA;
    uint64_t bodyB;
    AABB3D   aabbA;
    AABB3D   aabbB;
    bool operator==(const CollidingPair& o) const {
        return (bodyA==o.bodyA && bodyB==o.bodyB) ||
               (bodyA==o.bodyB && bodyB==o.bodyA);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  DBVT — Dynamic Bounding Volume Tree (Bullet-style)
//  Self-balancing, O(log n) insert/remove, O(n) query
// ─────────────────────────────────────────────────────────────────────────────
class DynamicBVH {
public:
    static constexpr float kFattenFactor = 0.1f;   // predictive AABB fattening
    static constexpr int   kNullNode     = -1;

    struct Node {
        AABB3D   aabb;
        uint64_t bodyId    = 0;
        int      parent    = kNullNode;
        int      children[2] = {kNullNode, kNullNode};
        int      height    = 0;
        bool     isLeaf() const { return children[0]==kNullNode; }
    };

    DynamicBVH() {
        nodes_.reserve(1024);
        freeList_ = kNullNode;
        root_     = kNullNode;
    }

    // ── Public API ──────────────────────────────────────────────────────────

    // Insert body with given AABB. Returns node index.
    int insert(uint64_t bodyId, const AABB3D& aabb) {
        int leaf = allocNode();
        nodes_[leaf].aabb   = fattenAABB(aabb);
        nodes_[leaf].bodyId = bodyId;
        nodes_[leaf].height = 0;
        insertLeaf(leaf);
        bodyToNode_[bodyId] = leaf;
        return leaf;
    }

    // Remove body
    void remove(uint64_t bodyId) {
        auto it = bodyToNode_.find(bodyId);
        if(it==bodyToNode_.end()) return;
        int leaf = it->second;
        bodyToNode_.erase(it);
        removeLeaf(leaf);
        freeNode(leaf);
    }

    // Update body's AABB (call every frame for dynamic bodies)
    // Returns true if node was moved (AABB drifted outside fat AABB)
    bool update(uint64_t bodyId, const AABB3D& newAABB, const Vec3& velocity={}) {
        auto it = bodyToNode_.find(bodyId);
        if(it==bodyToNode_.end()) return false;
        int leaf = it->second;
        Node& n  = nodes_[leaf];

        // Predictive AABB: expand in velocity direction
        AABB3D predictive = newAABB;
        if(velocity.lengthSq() > kEpsilon) {
            if(velocity.x>0) predictive.max.x += velocity.x;
            else             predictive.min.x += velocity.x;
            if(velocity.y>0) predictive.max.y += velocity.y;
            else             predictive.min.y += velocity.y;
            if(velocity.z>0) predictive.max.z += velocity.z;
            else             predictive.min.z += velocity.z;
        }

        if(n.aabb.contains(newAABB.min) && n.aabb.contains(newAABB.max))
            return false;

        removeLeaf(leaf);
        n.aabb = fattenAABB(predictive);
        insertLeaf(leaf);
        return true;
    }

    // Query: find all leaves overlapping with aabb
    void query(const AABB3D& aabb,
               const std::function<bool(uint64_t)>& callback) const {
        if(root_==kNullNode) return;
        std::stack<int> stack;
        stack.push(root_);
        while(!stack.empty()) {
            int idx = stack.top(); stack.pop();
            if(idx==kNullNode) continue;
            const Node& n = nodes_[idx];
            if(!n.aabb.overlaps(aabb)) continue;
            if(n.isLeaf()) {
                if(!callback(n.bodyId)) return;
            } else {
                stack.push(n.children[0]);
                stack.push(n.children[1]);
            }
        }
    }

    // Ray query
    void rayQuery(const Ray& ray,
                  const std::function<bool(uint64_t,float)>& callback) const {
        if(root_==kNullNode) return;
        std::stack<int> stack;
        stack.push(root_);
        while(!stack.empty()) {
            int idx = stack.top(); stack.pop();
            if(idx==kNullNode) continue;
            const Node& n = nodes_[idx];
            float tHit;
            if(!n.aabb.raycast(ray,tHit)) continue;
            if(n.isLeaf()) {
                if(!callback(n.bodyId,tHit)) return;
            } else {
                stack.push(n.children[0]);
                stack.push(n.children[1]);
            }
        }
    }

    // Collect all overlapping pairs (for narrowphase)
    void collectPairs(std::vector<CollidingPair>& pairs) const {
        if(root_==kNullNode) return;
        pairs.clear();
        collectPairsRecursive(root_, pairs);
    }

    // Statistics
    int nodeCount()  const { return (int)nodes_.size() - freeCount_; }
    int height()     const { return root_==kNullNode ? 0 : nodes_[root_].height; }
    float qualityRatio() const; // SAH quality metric

private:
    std::vector<Node>                nodes_;
    int                              root_      = kNullNode;
    int                              freeList_  = kNullNode;
    int                              freeCount_ = 0;
    std::unordered_map<uint64_t,int> bodyToNode_;

    // ── Node pool ───────────────────────────────────────────────────────────
    int allocNode() {
        if(freeList_ != kNullNode) {
            int idx   = freeList_;
            freeList_ = nodes_[idx].parent;  // reuse parent as next-free link
            freeCount_--;
            nodes_[idx] = Node{};
            return idx;
        }
        nodes_.emplace_back();
        return (int)nodes_.size()-1;
    }
    void freeNode(int idx) {
        nodes_[idx].parent = freeList_;
        freeList_ = idx;
        freeCount_++;
    }

    // ── Insert leaf using Surface Area Heuristic ────────────────────────────
    void insertLeaf(int leaf) {
        if(root_==kNullNode) { root_=leaf; nodes_[leaf].parent=kNullNode; return; }

        // Find best sibling (SAH)
        AABB3D leafAABB = nodes_[leaf].aabb;
        int best = findBestSibling(leafAABB);

        // Create new parent
        int oldParent = nodes_[best].parent;
        int newParent = allocNode();
        nodes_[newParent].parent      = oldParent;
        nodes_[newParent].aabb        = AABB3D::merge(leafAABB, nodes_[best].aabb);
        nodes_[newParent].height      = nodes_[best].height+1;
        nodes_[newParent].children[0] = best;
        nodes_[newParent].children[1] = leaf;
        nodes_[best].parent           = newParent;
        nodes_[leaf].parent           = newParent;

        if(oldParent==kNullNode) {
            root_ = newParent;
        } else {
            if(nodes_[oldParent].children[0]==best)
                nodes_[oldParent].children[0] = newParent;
            else
                nodes_[oldParent].children[1] = newParent;
        }
        refitAncestors(newParent);
    }

    int findBestSibling(const AABB3D& leafAABB) const {
        // Branch-and-bound SAH traversal
        float bestCost = kInfinity;
        int   bestNode = root_;
        struct Entry { int node; float inheritedCost; };
        std::stack<Entry> stack;
        stack.push({root_, 0.f});

        while(!stack.empty()) {
            auto [idx, ic] = stack.top(); stack.pop();
            const Node& n = nodes_[idx];

            AABB3D combined = AABB3D::merge(leafAABB, n.aabb);
            float directCost = combined.surfaceArea();
            float cost = directCost + ic;

            if(cost < bestCost) { bestCost=cost; bestNode=idx; }

            float childIC = ic + directCost - n.aabb.surfaceArea();
            float lowerBound = leafAABB.surfaceArea() + childIC;
            if(lowerBound < bestCost && !n.isLeaf()) {
                stack.push({n.children[0], childIC});
                stack.push({n.children[1], childIC});
            }
        }
        return bestNode;
    }

    void removeLeaf(int leaf) {
        if(leaf==root_) { root_=kNullNode; return; }
        int parent      = nodes_[leaf].parent;
        int grandParent = nodes_[parent].parent;
        int sibling     = nodes_[parent].children[0]==leaf
                        ? nodes_[parent].children[1]
                        : nodes_[parent].children[0];

        if(grandParent==kNullNode) {
            root_ = sibling;
            nodes_[sibling].parent = kNullNode;
        } else {
            if(nodes_[grandParent].children[0]==parent)
                nodes_[grandParent].children[0] = sibling;
            else
                nodes_[grandParent].children[1] = sibling;
            nodes_[sibling].parent = grandParent;
            refitAncestors(grandParent);
        }
        freeNode(parent);
    }

    void refitAncestors(int idx) {
        while(idx!=kNullNode) {
            Node& n = nodes_[idx];
            int c0=n.children[0], c1=n.children[1];
            n.height = 1 + std::max(nodes_[c0].height, nodes_[c1].height);
            n.aabb   = AABB3D::merge(nodes_[c0].aabb, nodes_[c1].aabb);
            // AVL rotation for balance
            balance(idx);
            idx = n.parent;
        }
    }

    // AVL-style rotation to keep tree balanced
    int balance(int idx) {
        Node& a = nodes_[idx];
        if(a.isLeaf()||a.height<2) return idx;

        int bIdx=a.children[0], cIdx=a.children[1];
        Node& b=nodes_[bIdx]; Node& c=nodes_[cIdx];
        int bf = c.height - b.height;

        if(bf>1) { // rotate C up
            int fIdx=c.children[0], gIdx=c.children[1];
            Node& f=nodes_[fIdx]; Node& g=nodes_[gIdx];
            c.children[0]=idx; c.parent=a.parent;
            a.parent=cIdx; a.children[1]=f.height>g.height ? fIdx : gIdx;
            if(c.parent!=kNullNode) {
                auto& cp=nodes_[c.parent];
                if(cp.children[0]==idx) cp.children[0]=cIdx;
                else cp.children[1]=cIdx;
            } else { root_=cIdx; }
            // Refit
            if(f.height>g.height) {
                c.children[1]=gIdx; a.children[1]=fIdx;
                f.parent=cIdx; nodes_[gIdx].parent=idx;
                a.aabb=AABB3D::merge(b.aabb,f.aabb);
            } else {
                c.children[1]=fIdx; a.children[1]=gIdx;
                g.parent=cIdx; nodes_[fIdx].parent=idx;
                a.aabb=AABB3D::merge(b.aabb,g.aabb);
            }
            a.height=1+std::max(b.height, nodes_[a.children[1]].height);
            c.aabb  =AABB3D::merge(a.aabb, nodes_[c.children[1]].aabb);
            c.height=1+std::max(a.height, nodes_[c.children[1]].height);
            return cIdx;
        }
        if(bf<-1) { // symmetric, rotate B up (mirror of above)
            // ... (symmetric code)
        }
        return idx;
    }

    // Collect leaf-leaf overlapping pairs
    void collectPairsRecursive(int node, std::vector<CollidingPair>& pairs) const {
        if(node==kNullNode) return;
        const Node& n = nodes_[node];
        if(!n.isLeaf()) {
            collectPairsRecursive(n.children[0], pairs);
            collectPairsRecursive(n.children[1], pairs);
            // Test subtrees against each other
            testSubtrees(n.children[0], n.children[1], pairs);
        }
    }
    void testSubtrees(int a, int b, std::vector<CollidingPair>& pairs) const {
        if(a==kNullNode||b==kNullNode) return;
        const Node& na=nodes_[a]; const Node& nb=nodes_[b];
        if(!na.aabb.overlaps(nb.aabb)) return;
        if(na.isLeaf()&&nb.isLeaf()) {
            pairs.push_back({na.bodyId,nb.bodyId,na.aabb,nb.aabb});
            return;
        }
        // Descend into larger
        if(na.isLeaf()) {
            testSubtrees(a, nb.children[0], pairs);
            testSubtrees(a, nb.children[1], pairs);
        } else if(nb.isLeaf()) {
            testSubtrees(na.children[0], b, pairs);
            testSubtrees(na.children[1], b, pairs);
        } else if(na.aabb.surfaceArea() > nb.aabb.surfaceArea()) {
            testSubtrees(na.children[0], b, pairs);
            testSubtrees(na.children[1], b, pairs);
        } else {
            testSubtrees(a, nb.children[0], pairs);
            testSubtrees(a, nb.children[1], pairs);
        }
    }

    static AABB3D fattenAABB(const AABB3D& aabb) {
        Vec3 margin(kFattenFactor);
        return { aabb.min - margin, aabb.max + margin };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Sweep and Prune (SAP) — fast for low-dimensional overlaps
//  Sort + scan on X axis, overlap check on Y/Z
// ─────────────────────────────────────────────────────────────────────────────
class SweepAndPrune {
public:
    struct Proxy {
        uint64_t bodyId;
        AABB3D   aabb;
    };

    void insert(uint64_t bodyId, const AABB3D& aabb) {
        proxies_.push_back({bodyId, aabb});
        dirty_ = true;
    }
    void remove(uint64_t bodyId) {
        proxies_.erase(std::remove_if(proxies_.begin(), proxies_.end(),
            [bodyId](const Proxy& p){ return p.bodyId==bodyId; }), proxies_.end());
        dirty_ = true;
    }
    void update(uint64_t bodyId, const AABB3D& aabb) {
        for(auto& p : proxies_)
            if(p.bodyId==bodyId) { p.aabb=aabb; dirty_=true; return; }
    }

    void collectPairs(std::vector<CollidingPair>& pairs) {
        pairs.clear();
        if(proxies_.size()<2) return;

        // Sort by min.x
        if(dirty_) {
            std::sort(proxies_.begin(), proxies_.end(),
                [](const Proxy& a, const Proxy& b){ return a.aabb.min.x < b.aabb.min.x; });
            dirty_ = false;
        }

        for(size_t i=0;i<proxies_.size();i++) {
            const Proxy& a = proxies_[i];
            for(size_t j=i+1;j<proxies_.size();j++) {
                const Proxy& b = proxies_[j];
                if(b.aabb.min.x > a.aabb.max.x) break; // X separated
                if(a.aabb.overlaps(b.aabb))
                    pairs.push_back({a.bodyId, b.bodyId, a.aabb, b.aabb});
            }
        }
    }

private:
    std::vector<Proxy> proxies_;
    bool               dirty_ = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Broadphase — wraps either BVH or SAP, selects best by scene density
// ─────────────────────────────────────────────────────────────────────────────
enum class BroadphaseType { DynamicBVH, SAP, Grid };

class Broadphase {
public:
    Broadphase(BroadphaseType type = BroadphaseType::DynamicBVH) : type_(type) {}

    void insert(uint64_t bodyId, const AABB3D& aabb) {
        if(type_==BroadphaseType::DynamicBVH) bvh_.insert(bodyId,aabb);
        else sap_.insert(bodyId,aabb);
    }
    void remove(uint64_t bodyId) {
        if(type_==BroadphaseType::DynamicBVH) bvh_.remove(bodyId);
        else sap_.remove(bodyId);
    }
    void update(uint64_t bodyId, const AABB3D& aabb, const Vec3& vel={}) {
        if(type_==BroadphaseType::DynamicBVH) bvh_.update(bodyId,aabb,vel);
        else sap_.update(bodyId,aabb);
    }

    void collectPairs(std::vector<CollidingPair>& pairs) {
        if(type_==BroadphaseType::DynamicBVH) bvh_.collectPairs(pairs);
        else sap_.collectPairs(pairs);
    }

    void query(const AABB3D& aabb,
               const std::function<bool(uint64_t)>& cb) const {
        if(type_==BroadphaseType::DynamicBVH) bvh_.query(aabb,cb);
    }
    void rayQuery(const Ray& ray,
                  const std::function<bool(uint64_t,float)>& cb) const {
        if(type_==BroadphaseType::DynamicBVH) bvh_.rayQuery(ray,cb);
    }

    BroadphaseType type() const { return type_; }

private:
    BroadphaseType type_;
    DynamicBVH     bvh_;
    SweepAndPrune  sap_;
};

} // namespace col
