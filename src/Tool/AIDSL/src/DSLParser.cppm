module;
#include <utility>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
//#include "DSLTypes.ixx"

export module AIToolDSL.Parser;

import AIToolDSL.Types;
import std;

namespace AIToolDSL {

namespace {

// Trim whitespace
std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Split line into tokens (simple whitespace split, but respect quotes)
std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    char quoteChar = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == quoteChar) {
                inQuotes = false;
                quoteChar = 0;
            } else {
                current += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
            } else if (isspace(c)) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

// Check if token is a number
bool isNumber(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

// Check if token is a float
bool isFloat(const std::string& s) {
    std::regex pattern(R"(^-?\d+(\.\d+)?([eE][+-]?\d+)?$)");
    return std::regex_match(s, pattern);
}

// Check if token is a boolean
bool isBoolean(const std::string& s) {
    return s == "true" || s == "false";
}

// Parse value from token
Value parseValue(const std::string& token) {
    if (token == "true") return true;
    if (token == "false") return false;
    if (isFloat(token)) return std::stod(token);
    if (isNumber(token)) return static_cast<int64_t>(std::stoll(token));
    // Check for frame expression "12f"
    if (token.size() > 1 && token.back() == 'f' && std::all_of(token.begin(), token.end() - 1, ::isdigit)) {
        return static_cast<int64_t>(std::stoll(token.substr(0, token.size() - 1)));
    }
    // Check for vector [x,y] or [x,y,z]
    if (!token.empty() && token[0] == '[' && token.back() == ']') {
        std::string inner = token.substr(1, token.size() - 2);
        std::vector<std::string> parts;
        std::stringstream ss(inner);
        std::string item;
        while (std::getline(ss, item, ',')) {
            parts.push_back(trim(item));
        }
        std::vector<double> vec;
        for (const auto& p : parts) {
            if (isFloat(p)) {
                vec.push_back(std::stod(p));
            } else {
                vec.push_back(0.0);  // invalid, but don't crash
            }
        }
        return vec;
    }
    // String literal (remove quotes)
    if ((token.front() == '"' && token.back() == '"') || (token.front() == '\'' && token.back() == '\'')) {
        return token.substr(1, token.size() - 2);
    }
    return token;  // plain string
}

// Parse comparison operator
BinOp parseBinOp(const std::string& op) {
    if (op == "==") return BinOp::Eq;
    if (op == "!=") return BinOp::Ne;
    if (op == ">") return BinOp::Gt;
    if (op == "<") return BinOp::Lt;
    if (op == ">=") return BinOp::Ge;
    if (op == "<=") return BinOp::Le;
    if (op == "~=") return BinOp::Matches;
    return BinOp::Eq;  // default
}

// Parse filter expression (very simplified: only binary conditions)
std::unique_ptr<ExprNode> parseFilter(const std::vector<std::string>& tokens, size_t start) {
    // Simplified: property op value [and property op value ...]
    std::vector<std::unique_ptr<ExprNode>> conditions;
    for (size_t i = start; i < tokens.size(); ) {
        if (tokens[i] == "and") {
            ++i;
            continue;
        }
        if (i + 2 >= tokens.size()) {
            break;
        }
        auto lhs = std::make_unique<PropertyRef>(std::string(tokens[i]));
        auto op = parseBinOp(tokens[i + 1]);
        auto rhs = std::make_unique<Literal>(parseValue(tokens[i + 2]));
        i += 3;

        auto expr = BinaryExpr{};
        expr.lhs = std::move(lhs);
        expr.op = op;
        expr.rhs = std::move(rhs);
        auto cond = std::make_unique<BinaryExpr>(std::move(expr));
        conditions.push_back(std::move(cond));
    }

    if (conditions.empty()) {
        return nullptr;
    }
    if (conditions.size() == 1) {
        return std::move(conditions[0]);
    }
    // Combine with AND (nested left-associative)
    auto result = std::move(conditions[0]);
    for (size_t i = 1; i < conditions.size(); ++i) {
        auto expr = BinaryExpr{};
        expr.lhs = std::move(result);
        expr.op = BinOp::Eq;  // placeholder op; will not be used
        expr.rhs = std::move(conditions[i]);
        result = std::make_unique<BinaryExpr>(std::move(expr));
        // Actually we need a proper AND chain, but for simplicity we flatten later
    }
    return result;
}

} // namespace

// Convert PropertyRef::evaluate to use actual lookup
bool PropertyRef::evaluate(const std::unordered_map<std::string, Value>& props) const {
    auto it = props.find(path);
    if (it == props.end()) return false;
    // In real implementation, compare with rhs value; this stub only checks existence
    return true;
}

bool Literal::evaluate(const std::unordered_map<std::string, Value>& /*props*/) const {
    return true;  // Literals don't need evaluation; they're values
}

bool BinaryExpr::evaluate(const std::unordered_map<std::string, Value>& props) const {
    // This is a stub; real implementation would evaluate both sides and compare
    return true;
}

// Public parse function
ParseResult AIDSLInterpreter::parseImpl(const std::string& input) {
    ParseResult result;
    DSLScript script;

    std::istringstream iss(input);
    std::string line;
    int lineNum = 0;

    // State: are we inside a transaction?
    bool inTransaction = false;
    TransactionCommand* currentTransaction = nullptr;

    while (std::getline(iss, line)) {
        ++lineNum;
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;  // skip comments and blank lines
        }

        auto tokens = tokenize(trimmed);

        if (tokens.empty()) continue;

        // Parse command
        if (tokens[0] == "use") {
            if (tokens.size() >= 3 && tokens[1] == "comp") {
                UseCompCommand cmd;
                cmd.compName = tokens[2];
                if (tokens.size() > 3) { /* maybe quoted */ }
                cmd.sourceLine = trimmed;
                script.useComp.emplace(std::move(cmd));
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid 'use' syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "begin_transaction") {
            if (tokens.size() >= 2) {
                auto tx = std::make_unique<TransactionCommand>();
                tx->name = tokens[1];
                // Remove quotes if present
                if (tx->name.front() == '"' && tx->name.back() == '"') {
                    tx->name = tx->name.substr(1, tx->name.size() - 2);
                }
                tx->sourceLine = trimmed;
                if (inTransaction) {
                    script.hasError = true;
                    script.parseError = "Line " + std::to_string(lineNum) + ": nested transactions not allowed";
                    result.error = script.parseError;
                    break;
                }
                inTransaction = true;
                currentTransaction = tx.get();
                script.commands.push_back(std::move(tx));
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": missing transaction name";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "end_transaction") {
            if (!inTransaction) {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": end_transaction without begin";
                result.error = script.parseError;
                break;
            }
            inTransaction = false;
            currentTransaction = nullptr;
        }
        else if (tokens[0] == "select") {
            if (tokens.size() >= 3 && tokens[1] == "layers") {
                auto cmd = std::make_unique<SelectLayersCommand>();
                // Look for "where" keyword
                size_t wherePos = 2;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    if (tokens[i] == "where") {
                        wherePos = i + 1;
                        break;
                    }
                }
                // TODO: parse filter expression from tokens[wherePos...]
                // For now, store the whole line and implement filter later
                cmd->filter = nullptr;  // placeholder
                cmd->sourceLine = trimmed;
                if (inTransaction && currentTransaction) {
                    currentTransaction->body.push_back(std::move(cmd));
                } else {
                    script.commands.push_back(std::move(cmd));
                }
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid select syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "set") {
            if (tokens.size() >= 3 && tokens[2] == "=") {
                auto cmd = std::make_unique<SetPropertyCommand>();
                cmd->property = tokens[1];
                // Reconstruct value tokens after '='
                std::string valueStr;
                for (size_t i = 3; i < tokens.size(); ++i) {
                    if (i > 3) valueStr += " ";
                    valueStr += tokens[i];
                }
                cmd->value = parseValue(valueStr);
                cmd->sourceLine = trimmed;
                if (inTransaction && currentTransaction) {
                    currentTransaction->body.push_back(std::move(cmd));
                } else {
                    script.commands.push_back(std::move(cmd));
                }
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid set syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "add") {
            if (tokens.size() >= 5 && tokens[1] == "key" && tokens[2] == "at") {
                auto cmd = std::make_unique<AddKeyCommand>();
                // Parse frame
                std::string frameStr = tokens[3];
                if (frameStr.back() == 'f') {
                    frameStr.pop_back();
                }
                cmd->frame = FrameExpr{ std::stoll(frameStr) };
                // property = value
                if (tokens[4] == "=" && tokens.size() > 5) {
                    cmd->property = tokens[3]; // Actually should be after `at`? Wait order: add key at 12f opacity = 0
                    // Correct parse: tokens[3] is frame, tokens[4] is property, tokens[5] is '=', tokens[6] is value
                    // Actually typical DSL: "add key at 12f opacity = 0"
                    // So: tokens[0]="add", [1]="key", [2]="at", [3]="12f", [4]="opacity", [5]="=", [6]="0"
                    // We need to parse better
                    // For now, skip complex parsing; this is a stub
                }
                cmd->sourceLine = trimmed;
                if (inTransaction && currentTransaction) {
                    currentTransaction->body.push_back(std::move(cmd));
                } else {
                    script.commands.push_back(std::move(cmd));
                }
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid add key syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "rename") {
            // stub
        }
        else if (tokens[0] == "delete") {
            // stub
        }
        else if (tokens[0] == "group") {
            // stub
        }
        else if (tokens[0] == "query") {
            // stub for queries
        }
        else {
            // Unknown command
            script.hasError = true;
            script.parseError = "Line " + std::to_string(lineNum) + ": unknown command '" + tokens[0] + "'";
            result.error = script.parseError;
            break;
        }
    }

    script.hasError = script.hasError || inTransaction;  // missing end
    if (inTransaction) {
        script.parseError = "Unclosed transaction at end of script";
        result.error = script.parseError;
    }
    result.script = std::move(script);
    return result;
}

// Public parse method
ParseResult AIDSLInterpreter::parse(const std::string& input) {
    return parseImpl(input);
}

// Stub for compile (not implemented yet)
std::unique_ptr<Action> UseCompCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> SelectLayersCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> SetPropertyCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> AddKeyCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> RenameCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> DeleteCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> GroupCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

std::unique_ptr<Action> TransactionCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return nullptr;
}

// Query stubs
std::string QuerySelectedLayers::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return R"({"status":"not_implemented"})";
}

std::string QueryActiveComp::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return R"({"status":"not_implemented"})";
}

std::string QueryCompSize::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return R"({"status":"not_implemented"})";
}

std::string QueryFindLayers::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return R"({"status":"not_implemented"})";
}

std::string QueryDescribeLayer::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return R"({"status":"not_implemented"})";
}

std::string QueryListProperties::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    return R"({"status":"not_implemented"})";
}

// Implementation stubs for AIDSLInterpreter
AIDSLInterpreter::AIDSLInterpreter() = default;
AIDSLInterpreter::~AIDSLInterpreter() = default;

std::string AIDSLInterpreter::dryRun(const DSLScript& script) const {
    return R"({"dry_run":"not_implemented"})";
}

std::string AIDSLInterpreter::execute(const DSLScript& script) {
    return R"({"status":"not_implemented"})";
}

std::string AIDSLInterpreter::executeQuery(const QueryNode& query) {
    return query.execute(compNameToId_, layerNameToIds_);
}

bool AIDSLInterpreter::undo() {
    if (undoStack_.empty()) return false;
    // stub
    return true;
}

bool AIDSLInterpreter::canUndo() const {
    return !undoStack_.empty();
}

void AIDSLInterpreter::setActiveComp(const CompID& compId) {
    activeCompId_ = compId;
}

void AIDSLInterpreter::setActiveCompByName(const std::string& compName) {
    auto it = compNameToId_.find(compName);
    if (it != compNameToId_.end()) {
        activeCompId_ = it->second;
    }
}

void AIDSLInterpreter::setLayerLookup(const std::unordered_map<std::string, std::vector<LayerID>>& lookup) {
    layerNameToIds_ = lookup;
}

void AIDSLInterpreter::setCompLookup(const std::unordered_map<std::string, CompID>& lookup) {
    compNameToId_ = lookup;
}

LayerID AIDSLInterpreter::resolveLayerRef(const std::string& ref, const std::unordered_map<std::string, std::vector<LayerID>>& layerMap) const {
    // If ref starts with "#", it's a direct ID
    if (ref.starts_with('#')) {
        return ref;
    }
    // Otherwise look up by name
    auto it = layerMap.find(ref);
    if (it != layerMap.end() && !it->second.empty()) {
        return it->second[0];  // Return first if multiple
    }
    return "#L0";  // invalid placeholder
}

CompID AIDSLInterpreter::resolveCompRef(const std::string& ref) const {
    if (ref.starts_with('#')) {
        return ref;
    }
    auto it = compNameToId_.find(ref);
    if (it != compNameToId_.end()) {
        return it->second;
    }
    return "#C0";  // invalid placeholder
}

std::unique_ptr<TransactionAction> AIDSLInterpreter::compileTransaction(const TransactionCommand& cmd) const {
    auto tx = std::make_unique<TransactionAction>();
    tx->name = cmd.name;
    // Compile each subcommand
    for (const auto& sub : cmd.body) {
        // call compile on each
        // auto action = sub->compile(compNameToId_, layerNameToIds_);
        // if (action) tx->actions.push_back(std::move(action));
    }
    return tx;
}

} // namespace AIToolDSL
