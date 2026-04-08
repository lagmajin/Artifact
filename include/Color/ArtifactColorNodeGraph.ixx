module;
#include <memory>
#include <wobjectdefs.h>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QObject>
#include <QString>
#include <QUuid>
export module Artifact.Color.NodeGraph;




import Artifact.Color.Node;
W_REGISTER_ARGTYPE(Artifact::PortId)
W_REGISTER_ARGTYPE(Artifact::NodeConnection)

export namespace Artifact {

// ============================================================
// ColorNodeGraph — manages a directed acyclic graph of ColorNodes
// ============================================================

/// Evaluation order & graph topology manager
class ColorNodeGraph : public QObject {
    W_OBJECT(ColorNodeGraph)
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    explicit ColorNodeGraph(QObject* parent = nullptr);
    ~ColorNodeGraph();

    // --- Node Management ---

    /// Add a node to the graph. Graph takes ownership.
    /// Returns the node's UUID.
    QUuid addNode(std::unique_ptr<ColorNode> node);

    /// Add node by type (uses factory internally)
    QUuid addNode(ColorNodeType type);

    /// Remove a node and all its connections
    bool removeNode(const QUuid& nodeId);

    /// Get node by ID (nullptr if not found)
    ColorNode* node(const QUuid& nodeId) const;

    /// Get all node IDs
    std::vector<QUuid> nodeIds() const;

    /// Number of nodes in the graph
    int nodeCount() const;

    // --- Connection Management ---

    /// Connect an output port to an input port
    /// Returns true if connection was successful
    bool connect(const PortId& source, const PortId& destination);

    /// Connect by node IDs and port indices (convenience)
    bool connect(const QUuid& sourceNodeId, int sourcePortIndex,
                 const QUuid& destNodeId, int destPortIndex);

    /// Disconnect a specific connection
    bool disconnect(const PortId& source, const PortId& destination);

    /// Disconnect all connections to/from a port
    void disconnectAll(const PortId& portId);

    /// Disconnect all connections for a node
    void disconnectNode(const QUuid& nodeId);

    /// Get all connections in the graph
    std::vector<NodeConnection> connections() const;

    /// Get connections to a specific input port
    std::vector<NodeConnection> connectionsToInput(const PortId& inputPort) const;

    /// Get connections from a specific output port
    std::vector<NodeConnection> connectionsFromOutput(const PortId& outputPort) const;

    /// Check if a connection would create a cycle
    bool wouldCreateCycle(const QUuid& sourceNodeId, const QUuid& destNodeId) const;

    // --- Graph Topology ---

    /// Get the input node (source of the pipeline)
    ColorNode* inputNode() const;

    /// Get the output node (end of the pipeline)
    ColorNode* outputNode() const;

    /// Get evaluation order (topologically sorted)
    std::vector<QUuid> evaluationOrder() const;

    /// Check if the graph is valid (has input/output, no cycles, all connected)
    bool isValid() const;

    // --- Serial / Parallel node helpers (DaVinci-style) ---

    /// Insert a node in serial after the given node
    /// Automatically reconnects the chain: prevNode -> newNode -> nextNode
    QUuid insertSerial(const QUuid& afterNodeId, ColorNodeType type);

    /// Insert a node in serial after the given node (with existing node)
    QUuid insertSerial(const QUuid& afterNodeId, std::unique_ptr<ColorNode> node);

    /// Create a parallel branch from a node
    /// Inserts a merge node and connects both paths
    QUuid insertParallel(const QUuid& branchFromNodeId, ColorNodeType type);

    // --- Processing ---

    /// Evaluate the entire graph on a pixel buffer
    /// Processes nodes in topological order
    void evaluate(float* pixels, int width, int height);

    /// Evaluate with a secondary input (for merge nodes, etc.)
    void evaluate(float* pixels, const float* secondaryInput,
                  int width, int height);

    // --- Presets / Templates ---

    /// Create a minimal graph (Input -> Output)
    static std::unique_ptr<ColorNodeGraph> createMinimal();

    /// Create a standard DaVinci-style serial graph
    /// Input -> LiftGammaGain -> Contrast -> Curves -> Output
    static std::unique_ptr<ColorNodeGraph> createStandardSerial();

    /// Create a parallel graph with two correction branches
    static std::unique_ptr<ColorNodeGraph> createParallelTemplate();

public:
    void nodeAdded(const QUuid& nodeId)                 W_SIGNAL(nodeAdded, nodeId);
    void nodeRemoved(const QUuid& nodeId)               W_SIGNAL(nodeRemoved, nodeId);
    void connectionAdded(const NodeConnection& conn)    W_SIGNAL(connectionAdded, conn);
    void connectionRemoved(const NodeConnection& conn)  W_SIGNAL(connectionRemoved, conn);
    void graphChanged()                                 W_SIGNAL(graphChanged);
    void evaluationComplete()                           W_SIGNAL(evaluationComplete);
};

} // namespace Artifact
