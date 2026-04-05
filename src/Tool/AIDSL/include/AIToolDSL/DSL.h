module;
#include <string>
#include <memory>

export module AIToolDSL;

export namespace AIToolDSL {

// Simplified DSL interpreter for AI tools
class Interpreter {
public:
    struct Result {
        std::string output;  // JSON response or error
        bool success = false;
        std::string errorMessage;
    };

    Interpreter();
    ~Interpreter();

    // Parse DSL command string
    Result parse(const std::string& input);

    // Execute parsed commands (or parse and execute in one step)
    Result execute(const std::string& input);

    // Dry-run: show what would change
    Result dryRun(const std::string& input);

    // Query current state
    Result query(const std::string& queryExpression);

    // Undo/Redo
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;

    // Integration: set host application context
    void setActiveCompById(const std::string& compId);
    void setActiveCompByName(const std::string& compName);
    void setLayerLookup(std::function<std::vector<std::string>(const std::string& filter)> lookup);
    void setPropertyAccessor(std::function<std::string(const std::string& layerId, const std::string& prop)> getter);
    void setPropertyMutator(std::function<bool(const std::string& layerId, const std::string& prop, const std::string& value)> setter);
};

} // namespace AIToolDSL
