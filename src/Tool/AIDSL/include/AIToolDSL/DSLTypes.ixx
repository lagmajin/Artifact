module;
#include <utility>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <cstdint>
#include <unordered_map>

export module AIToolDSL.Types;

export namespace AIToolDSL {

// Forward declarations
struct CommandNode;
struct QueryNode;
struct ExprNode;
struct Action {
    virtual ~Action() = default;
};
struct TransactionAction : Action {
    std::string name;
    std::vector<std::unique_ptr<Action>> actions;
};

// Utility types
using LayerID = std::string;  // "#L1234"
using CompID = std::string;   // "#C9"
using FrameTime = int64_t;    // frame number
using PropertyPath = std::string;  // "layer.opacity", "transform.position.x"

// Value variant (supports all property types)
export using Value = std::variant<
    bool,
    int64_t,
    double,
    std::string,
    std::vector<double>  // vec2, vec3, vec4
>;

// Frame expression: 12f or 12
struct FrameExpr {
    std::variant<int64_t, std::string> value;  // raw number or "12f"
    FrameTime resolve(const std::unordered_map<std::string, FrameTime>& context) const;
};

// Binary operator for filters
enum class BinOp {
    Eq, Ne, Gt, Lt, Ge, Le, Matches  // ~= for regex
};

// Expression tree for filter conditions
struct ExprNode {
    virtual ~ExprNode() = default;
    virtual bool evaluate(const std::unordered_map<std::string, Value>& props) const = 0;
};

struct PropertyRef : ExprNode {
    PropertyPath path;
    explicit PropertyRef(PropertyPath p) : path(std::move(p)) {}
    bool evaluate(const std::unordered_map<std::string, Value>& props) const override;
};

struct Literal : ExprNode {
    Value value;
    explicit Literal(Value v) : value(std::move(v)) {}
    bool evaluate(const std::unordered_map<std::string, Value>& props) const override;
};

struct BinaryExpr : ExprNode {
    std::unique_ptr<ExprNode> lhs;
    BinOp op;
    std::unique_ptr<ExprNode> rhs;
    bool evaluate(const std::unordered_map<std::string, Value>& props) const override;
};

// Command AST nodes
struct CommandNode {
    virtual ~CommandNode() = default;
    virtual std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const = 0;
    std::string sourceLine;  // for error reporting
};

struct UseCompCommand : CommandNode {
    std::string compName;  // "Main" or "#C9"
    CompID resolvedCompId;
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct SelectLayersCommand : CommandNode {
    std::unique_ptr<ExprNode> filter;
    std::vector<LayerID> resolvedLayerIds;  // filled at compile time
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct SetPropertyCommand : CommandNode {
    enum class Target { Selected, All, Specific };
    Target target;
    std::optional<LayerID> specificLayerId;  // if target == Specific
    PropertyPath property;
    Value value;
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct AddKeyCommand : CommandNode {
    enum class Target { Selected, All, Specific };
    Target target;
    std::optional<LayerID> specificLayerId;
    FrameExpr frame;
    PropertyPath property;
    Value value;
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct RenameCommand : CommandNode {
    enum class Target { Selected, All, Specific };
    Target target;
    std::optional<LayerID> specificLayerId;
    std::string templateStr;  // e.g., "title_{index}"
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct DeleteCommand : CommandNode {
    enum class Target { Selected, All, Specific };
    Target target;
    std::optional<LayerID> specificLayerId;
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct GroupCommand : CommandNode {
    enum class Target { Selected, All, Specific };
    Target target;
    std::optional<LayerID> specificLayerId;
    std::string groupName;
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct TransactionCommand : CommandNode {
    std::string name;
    std::vector<std::unique_ptr<CommandNode>> body;
    std::unique_ptr<Action> compile(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

// Query AST nodes
struct QueryNode {
    virtual ~QueryNode() = default;
    virtual std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const = 0;  // returns JSON string
};

struct QuerySelectedLayers : QueryNode {
    std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct QueryActiveComp : QueryNode {
    std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct QueryCompSize : QueryNode {
    CompID compId;
    std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct QueryFindLayers : QueryNode {
    std::unique_ptr<ExprNode> filter;
    std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct QueryDescribeLayer : QueryNode {
    LayerID layerId;
    std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

struct QueryListProperties : QueryNode {
    LayerID layerId;
    std::string execute(
        const std::unordered_map<std::string, CompID>& compMap,
        const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
    ) const override;
};

// DSL top-level container
struct DSLScript {
    std::optional<UseCompCommand> useComp;
    std::vector<std::unique_ptr<CommandNode>> commands;
    std::vector<std::unique_ptr<QueryNode>> queries;
    std::string parseError;
    bool hasError = false;
};

// Parse result
export struct ParseResult {
    DSLScript script;
    std::string error;
    bool success() const { return error.empty() && !script.hasError; }
};

// DSL interface
export class AIDSLInterpreter {
public:
    AIDSLInterpreter();
    ~AIDSLInterpreter();

    // Parse DSL text
    ParseResult parse(const std::string& input);

    // Dry-run: return what would change without executing
    std::string dryRun(const DSLScript& script) const;

    // Execute commands
    std::string execute(const DSLScript& script);

    // Execute a single query
    std::string executeQuery(const QueryNode& query);

    // Undo support
    bool undo();
    bool canUndo() const;

    // Set current composition context (from host app)
    void setActiveComp(const CompID& compId);
    void setActiveCompByName(const std::string& compName);

    // Lookup helpers (host app integration)
    void setLayerLookup(const std::unordered_map<std::string, std::vector<LayerID>>& lookup);
    void setCompLookup(const std::unordered_map<std::string, CompID>& lookup);

private:
    // Parser implementation
    ParseResult parseImpl(const std::string& input);

    // Target resolution
    LayerID resolveLayerRef(const std::string& ref, const std::unordered_map<std::string, std::vector<LayerID>>& layerMap) const;
    CompID resolveCompRef(const std::string& ref) const;

    // Compilation
    std::unique_ptr<TransactionAction> compileTransaction(const TransactionCommand& cmd) const;

    // Execution state
    std::unordered_map<std::string, CompID> compNameToId_;
    std::unordered_map<std::string, std::vector<LayerID>> layerNameToIds_;
    CompID activeCompId_;
    mutable std::vector<std::unique_ptr<Action>> undoStack_;
    mutable std::vector<std::unique_ptr<Action>> redoStack_;
};

} // namespace AIToolDSL
