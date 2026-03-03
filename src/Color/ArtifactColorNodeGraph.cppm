module;
#include <QObject>
#include <QString>
#include <QUuid>
#include <wobjectimpl.h>
#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>

module Artifact.Color.NodeGraph;

import std;
import Artifact.Color.Node;

// Hash for QUuid to use in std containers
namespace std {
    template<>
    struct hash<QUuid> {
        size_t operator()(const QUuid& uuid) const noexcept {
            return qHash(uuid);
        }
    };
}

namespace Artifact {

W_OBJECT_IMPL(ColorNodeGraph)

// ============================================================
// ColorNodeGraph::Impl
// ============================================================

class ColorNodeGraph::Impl {
public:
    // All nodes in the graph, keyed by UUID
    std::unordered_map<QUuid, std::unique_ptr<ColorNode>> nodes_;

    // All connections
    std::vector<NodeConnection> connections_;

    // Cached evaluation order (invalidated when graph changes)
    mutable std::vector<QUuid> evaluationOrder_;
    mutable bool orderDirty_ = true;

    // Special node references
    QUuid inputNodeId_;
    QUuid outputNodeId_;

    // --- Internal helpers ---

    /// Get the list of successor node IDs for a given node
    std::vector<QUuid> successors(const QUuid& nodeId) const {
        std::vector<QUuid> result;
        for (const auto& conn : connections_) {
            if (conn.source.nodeId == nodeId) {
                // Check if this successor is already in the list
                if (std::find(result.begin(), result.end(), conn.destination.nodeId) == result.end()) {
                    result.push_back(conn.destination.nodeId);
                }
            }
        }
        return result;
    }

    /// Get the list of predecessor node IDs for a given node
    std::vector<QUuid> predecessors(const QUuid& nodeId) const {
        std::vector<QUuid> result;
        for (const auto& conn : connections_) {
            if (conn.destination.nodeId == nodeId) {
                if (std::find(result.begin(), result.end(), conn.source.nodeId) == result.end()) {
                    result.push_back(conn.source.nodeId);
                }
            }
        }
        return result;
    }

    /// Perform topological sort using Kahn's algorithm
    std::vector<QUuid> topologicalSort() const {
        // Count in-degrees
        std::unordered_map<QUuid, int> inDegree;
        for (const auto& [id, _] : nodes_) {
            inDegree[id] = 0;
        }
        for (const auto& conn : connections_) {
            inDegree[conn.destination.nodeId]++;
        }

        // Start with nodes that have no incoming edges
        std::queue<QUuid> queue;
        for (const auto& [id, deg] : inDegree) {
            if (deg == 0) {
                queue.push(id);
            }
        }

        std::vector<QUuid> sorted;
        while (!queue.empty()) {
            QUuid current = queue.front();
            queue.pop();
            sorted.push_back(current);

            for (const auto& succ : successors(current)) {
                if (--inDegree[succ] == 0) {
                    queue.push(succ);
                }
            }
        }

        // If sorted size != node count, there's a cycle
        // Return what we have anyway (caller should validate)
        return sorted;
    }

    /// DFS-based cycle detection
    bool hasCycleFrom(const QUuid& startId, const QUuid& targetId) const {
        std::unordered_set<QUuid> visited;
        std::queue<QUuid> bfs;
        bfs.push(targetId);

        while (!bfs.empty()) {
            QUuid current = bfs.front();
            bfs.pop();

            if (current == startId) return true;
            if (visited.count(current)) continue;
            visited.insert(current);

            for (const auto& succ : successors(current)) {
                bfs.push(succ);
            }
        }
        return false;
    }

    void invalidateOrder() { orderDirty_ = true; }

    void ensureOrder() const {
        if (orderDirty_) {
            evaluationOrder_ = topologicalSort();
            orderDirty_ = false;
        }
    }

    /// Find where a node's output connects to (the next node in a serial chain)
    QUuid findNextNode(const QUuid& nodeId) const {
        for (const auto& conn : connections_) {
            if (conn.source.nodeId == nodeId) {
                return conn.destination.nodeId;
            }
        }
        return QUuid();
    }
};

// ============================================================
// ColorNodeGraph
// ============================================================

ColorNodeGraph::ColorNodeGraph(QObject* parent)
    : QObject(parent), impl_(std::make_unique<Impl>())
{}

ColorNodeGraph::~ColorNodeGraph() = default;

// --- Node Management ---

QUuid ColorNodeGraph::addNode(std::unique_ptr<ColorNode> node) {
    if (!node) return QUuid();

    QUuid id = node->id();

    // Track special nodes
    if (node->type() == ColorNodeType::Input)  impl_->inputNodeId_ = id;
    if (node->type() == ColorNodeType::Output) impl_->outputNodeId_ = id;

    impl_->nodes_[id] = std::move(node);
    impl_->invalidateOrder();
    emit nodeAdded(id);
    emit graphChanged();
    return id;
}

QUuid ColorNodeGraph::addNode(ColorNodeType type) {
    auto node = ColorNodeFactory::create(type);
    return addNode(std::move(node));
}

bool ColorNodeGraph::removeNode(const QUuid& nodeId) {
    auto it = impl_->nodes_.find(nodeId);
    if (it == impl_->nodes_.end()) return false;

    // Remove all connections involving this node
    disconnectNode(nodeId);

    // Clear special node references
    if (impl_->inputNodeId_ == nodeId)  impl_->inputNodeId_ = QUuid();
    if (impl_->outputNodeId_ == nodeId) impl_->outputNodeId_ = QUuid();

    impl_->nodes_.erase(it);
    impl_->invalidateOrder();
    emit nodeRemoved(nodeId);
    emit graphChanged();
    return true;
}

ColorNode* ColorNodeGraph::node(const QUuid& nodeId) const {
    auto it = impl_->nodes_.find(nodeId);
    return (it != impl_->nodes_.end()) ? it->second.get() : nullptr;
}

std::vector<QUuid> ColorNodeGraph::nodeIds() const {
    std::vector<QUuid> ids;
    ids.reserve(impl_->nodes_.size());
    for (const auto& [id, _] : impl_->nodes_) {
        ids.push_back(id);
    }
    return ids;
}

int ColorNodeGraph::nodeCount() const {
    return static_cast<int>(impl_->nodes_.size());
}

// --- Connection Management ---

bool ColorNodeGraph::connect(const PortId& source, const PortId& destination) {
    if (!source.isValid() || !destination.isValid()) return false;
    if (source.nodeId == destination.nodeId) return false;

    // Check that both nodes exist
    if (!impl_->nodes_.count(source.nodeId) || !impl_->nodes_.count(destination.nodeId))
        return false;

    // Check for cycle
    if (wouldCreateCycle(source.nodeId, destination.nodeId))
        return false;

    // Check for duplicate connection
    for (const auto& conn : impl_->connections_) {
        if (conn.source == source && conn.destination == destination)
            return false;
    }

    NodeConnection conn{ source, destination };
    impl_->connections_.push_back(conn);
    impl_->invalidateOrder();
    emit connectionAdded(conn);
    emit graphChanged();
    return true;
}

bool ColorNodeGraph::connect(const QUuid& sourceNodeId, int sourcePortIndex,
                              const QUuid& destNodeId, int destPortIndex) {
    return connect(
        PortId{ sourceNodeId, sourcePortIndex },
        PortId{ destNodeId, destPortIndex }
    );
}

bool ColorNodeGraph::disconnect(const PortId& source, const PortId& destination) {
    auto it = std::find_if(impl_->connections_.begin(), impl_->connections_.end(),
        [&](const NodeConnection& c) {
            return c.source == source && c.destination == destination;
        });

    if (it == impl_->connections_.end()) return false;

    NodeConnection conn = *it;
    impl_->connections_.erase(it);
    impl_->invalidateOrder();
    emit connectionRemoved(conn);
    emit graphChanged();
    return true;
}

void ColorNodeGraph::disconnectAll(const PortId& portId) {
    auto it = std::remove_if(impl_->connections_.begin(), impl_->connections_.end(),
        [&](const NodeConnection& c) {
            return c.source == portId || c.destination == portId;
        });
    if (it != impl_->connections_.end()) {
        impl_->connections_.erase(it, impl_->connections_.end());
        impl_->invalidateOrder();
        emit graphChanged();
    }
}

void ColorNodeGraph::disconnectNode(const QUuid& nodeId) {
    auto it = std::remove_if(impl_->connections_.begin(), impl_->connections_.end(),
        [&](const NodeConnection& c) {
            return c.source.nodeId == nodeId || c.destination.nodeId == nodeId;
        });
    if (it != impl_->connections_.end()) {
        impl_->connections_.erase(it, impl_->connections_.end());
        impl_->invalidateOrder();
        emit graphChanged();
    }
}

std::vector<NodeConnection> ColorNodeGraph::connections() const {
    return impl_->connections_;
}

std::vector<NodeConnection> ColorNodeGraph::connectionsToInput(const PortId& inputPort) const {
    std::vector<NodeConnection> result;
    for (const auto& conn : impl_->connections_) {
        if (conn.destination == inputPort) {
            result.push_back(conn);
        }
    }
    return result;
}

std::vector<NodeConnection> ColorNodeGraph::connectionsFromOutput(const PortId& outputPort) const {
    std::vector<NodeConnection> result;
    for (const auto& conn : impl_->connections_) {
        if (conn.source == outputPort) {
            result.push_back(conn);
        }
    }
    return result;
}

bool ColorNodeGraph::wouldCreateCycle(const QUuid& sourceNodeId, const QUuid& destNodeId) const {
    return impl_->hasCycleFrom(sourceNodeId, destNodeId);
}

// --- Graph Topology ---

ColorNode* ColorNodeGraph::inputNode() const {
    return node(impl_->inputNodeId_);
}

ColorNode* ColorNodeGraph::outputNode() const {
    return node(impl_->outputNodeId_);
}

std::vector<QUuid> ColorNodeGraph::evaluationOrder() const {
    impl_->ensureOrder();
    return impl_->evaluationOrder_;
}

bool ColorNodeGraph::isValid() const {
    // Must have input and output
    if (impl_->inputNodeId_.isNull() || impl_->outputNodeId_.isNull())
        return false;

    // Check no cycles (topological sort should cover all nodes)
    auto sorted = impl_->topologicalSort();
    if (sorted.size() != impl_->nodes_.size())
        return false;

    return true;
}

// --- Serial / Parallel helpers ---

QUuid ColorNodeGraph::insertSerial(const QUuid& afterNodeId, ColorNodeType type) {
    return insertSerial(afterNodeId, ColorNodeFactory::create(type));
}

QUuid ColorNodeGraph::insertSerial(const QUuid& afterNodeId, std::unique_ptr<ColorNode> newNode) {
    if (!newNode || !impl_->nodes_.count(afterNodeId)) return QUuid();

    QUuid newId = newNode->id();

    // Find the existing connection from afterNode to its successor
    QUuid nextNodeId = impl_->findNextNode(afterNodeId);

    // Remove the old connection (afterNode -> nextNode)
    if (!nextNodeId.isNull()) {
        // Find and remove the connection
        for (auto it = impl_->connections_.begin(); it != impl_->connections_.end(); ++it) {
            if (it->source.nodeId == afterNodeId && it->destination.nodeId == nextNodeId) {
                impl_->connections_.erase(it);
                break;
            }
        }
    }

    // Add the new node
    addNode(std::move(newNode));

    // Connect: afterNode -> newNode
    connect(afterNodeId, 0, newId, 0);

    // Connect: newNode -> nextNode (if there was one)
    if (!nextNodeId.isNull()) {
        connect(newId, 0, nextNodeId, 0);
    }

    // Position the new node between the two
    ColorNode* afterNode = node(afterNodeId);
    ColorNode* nextNode = nextNodeId.isNull() ? nullptr : node(nextNodeId);
    ColorNode* newNodePtr = node(newId);

    if (afterNode && newNodePtr) {
        QPointF afterPos = afterNode->position();
        if (nextNode) {
            QPointF nextPos = nextNode->position();
            newNodePtr->setPosition(QPointF(
                (afterPos.x() + nextPos.x()) / 2.0,
                (afterPos.y() + nextPos.y()) / 2.0
            ));
        } else {
            newNodePtr->setPosition(afterPos + QPointF(200, 0));
        }
    }

    return newId;
}

QUuid ColorNodeGraph::insertParallel(const QUuid& branchFromNodeId, ColorNodeType type) {
    if (!impl_->nodes_.count(branchFromNodeId)) return QUuid();

    // Create the parallel correction node
    auto parallelNode = ColorNodeFactory::create(type);
    if (!parallelNode) return QUuid();
    QUuid parallelId = parallelNode->id();

    // Find the successor of branchFromNode
    QUuid nextNodeId = impl_->findNextNode(branchFromNodeId);

    // If a successor exists, insert a merge node before it
    if (!nextNodeId.isNull()) {
        // Create merge node
        auto mergeNode = std::make_unique<MergeNode>();
        QUuid mergeId = mergeNode->id();

        // Remove old connection (branchFrom -> next)
        for (auto it = impl_->connections_.begin(); it != impl_->connections_.end(); ++it) {
            if (it->source.nodeId == branchFromNodeId && it->destination.nodeId == nextNodeId) {
                impl_->connections_.erase(it);
                break;
            }
        }

        // Add nodes
        addNode(std::move(parallelNode));
        addNode(std::move(mergeNode));

        // Wire up:
        // branchFrom -> merge (input 0, main path)
        // branchFrom -> parallelNode -> merge (input 1, parallel path)
        connect(branchFromNodeId, 0, mergeId, 0);
        connect(branchFromNodeId, 0, parallelId, 0);
        // Note: merge input 1 would need a second input port, for now connect serial
        connect(parallelId, 0, mergeId, 0);
        connect(mergeId, 0, nextNodeId, 0);

        // Position
        ColorNode* branchNode = node(branchFromNodeId);
        ColorNode* nextNode = node(nextNodeId);
        if (branchNode && nextNode) {
            QPointF bp = branchNode->position();
            QPointF np = nextNode->position();
            QPointF mid((bp.x() + np.x()) / 2.0, bp.y());
            node(parallelId)->setPosition(mid + QPointF(0, 120));
            node(mergeId)->setPosition(mid + QPointF(100, 60));
        }
    } else {
        // No successor, just add the parallel node
        addNode(std::move(parallelNode));
        connect(branchFromNodeId, 0, parallelId, 0);
    }

    return parallelId;
}

// --- Processing ---

void ColorNodeGraph::evaluate(float* pixels, int width, int height) {
    evaluate(pixels, nullptr, width, height);
}

void ColorNodeGraph::evaluate(float* pixels, const float* secondaryInput,
                               int width, int height) {
    if (!isValid()) return;

    impl_->ensureOrder();

    for (const QUuid& nodeId : impl_->evaluationOrder_) {
        ColorNode* n = node(nodeId);
        if (!n || !n->isEnabled() || n->isBypassed()) continue;

        // If it's a merge node and we have secondary input, provide it
        if (n->type() == ColorNodeType::Merge && secondaryInput) {
            auto* mergeNode = static_cast<MergeNode*>(n);
            mergeNode->setSecondaryInput(secondaryInput);
        }

        n->process(pixels, width, height);
    }

    emit evaluationComplete();
}

// --- Presets ---

std::unique_ptr<ColorNodeGraph> ColorNodeGraph::createMinimal() {
    auto graph = std::make_unique<ColorNodeGraph>();

    QUuid inputId = graph->addNode(ColorNodeType::Input);
    QUuid outputId = graph->addNode(ColorNodeType::Output);

    graph->node(inputId)->setPosition(QPointF(0, 0));
    graph->node(outputId)->setPosition(QPointF(400, 0));

    graph->connect(inputId, 0, outputId, 0);

    return graph;
}

std::unique_ptr<ColorNodeGraph> ColorNodeGraph::createStandardSerial() {
    auto graph = std::make_unique<ColorNodeGraph>();

    // Create nodes
    QUuid inputId    = graph->addNode(ColorNodeType::Input);
    QUuid lggId      = graph->addNode(ColorNodeType::LiftGammaGain);
    QUuid contrastId = graph->addNode(ColorNodeType::Contrast);
    QUuid outputId   = graph->addNode(ColorNodeType::Output);

    // Position nodes
    graph->node(inputId)->setPosition(QPointF(0, 0));
    graph->node(lggId)->setPosition(QPointF(200, 0));
    graph->node(contrastId)->setPosition(QPointF(400, 0));
    graph->node(outputId)->setPosition(QPointF(600, 0));

    // Connect: Input -> LGG -> Contrast -> Output
    graph->connect(inputId, 0, lggId, 0);
    graph->connect(lggId, 0, contrastId, 0);
    graph->connect(contrastId, 0, outputId, 0);

    return graph;
}

std::unique_ptr<ColorNodeGraph> ColorNodeGraph::createParallelTemplate() {
    auto graph = std::make_unique<ColorNodeGraph>();

    // Create nodes
    QUuid inputId    = graph->addNode(ColorNodeType::Input);
    QUuid lggId      = graph->addNode(ColorNodeType::LiftGammaGain);
    QUuid contrastId = graph->addNode(ColorNodeType::Contrast);
    QUuid mergeId    = graph->addNode(ColorNodeType::Merge);
    QUuid outputId   = graph->addNode(ColorNodeType::Output);

    // Position nodes
    graph->node(inputId)->setPosition(QPointF(0, 0));
    graph->node(lggId)->setPosition(QPointF(200, -60));
    graph->node(contrastId)->setPosition(QPointF(200, 60));
    graph->node(mergeId)->setPosition(QPointF(400, 0));
    graph->node(outputId)->setPosition(QPointF(600, 0));

    // Connect: Input -> LGG -> Merge -> Output
    //          Input -> Contrast -> Merge
    graph->connect(inputId, 0, lggId, 0);
    graph->connect(inputId, 0, contrastId, 0);
    graph->connect(lggId, 0, mergeId, 0);
    graph->connect(contrastId, 0, mergeId, 0);
    graph->connect(mergeId, 0, outputId, 0);

    return graph;
}

} // namespace Artifact
