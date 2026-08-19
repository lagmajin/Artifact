module;
#include <QByteArray>
//#include "DSLTypes.ixx"

export module AIToolDSL.Parser;

import AIToolDSL.Types;
import std;

namespace AIToolDSL {

namespace {

class StringBuilder {
public:
    StringBuilder& operator<<(const std::string& value) {
        text_ += value;
        return *this;
    }

    StringBuilder& operator<<(const int value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const unsigned int value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const long value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const unsigned long value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const long long value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const unsigned long long value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const float value) { text_ += QByteArray::number(value).toStdString(); return *this; }
    StringBuilder& operator<<(const double value) { text_ += QByteArray::number(value).toStdString(); return *this; }

    StringBuilder& operator<<(const char* value) {
        text_ += value;
        return *this;
    }

    StringBuilder& operator<<(const char value) {
        text_ += value;
        return *this;
    }

    [[nodiscard]] std::string str() const { return text_; }

private:
    std::string text_;
};

std::string escapeJson(const std::string& input)
{
    StringBuilder out;
    for (const char c : input) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                constexpr char hex[] = "0123456789ABCDEF";
                const auto value = static_cast<unsigned char>(c);
                out << "\\u" << '0' << '0'
                    << hex[(value >> 4) & 0x0F] << hex[value & 0x0F];
            } else {
                out << c;
            }
        }
    }
    return out.str();
}

std::string jsonString(const std::string& value)
{
    return "\"" + escapeJson(value) + "\"";
}

std::string jsonBool(const bool value)
{
    return value ? "true" : "false";
}

std::string jsonArray(const std::vector<std::string>& values)
{
    StringBuilder out;
    out << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << jsonString(values[i]);
    }
    out << ']';
    return out.str();
}

std::string summarizeScript(const DSLScript& script, const std::string& mode)
{
    StringBuilder out;
    out << '{'
        << "\"mode\":" << jsonString(mode) << ','
        << "\"hasError\":" << jsonBool(script.hasError) << ','
        << "\"parseError\":" << jsonString(script.parseError) << ','
        << "\"hasUseComp\":" << jsonBool(script.useComp.has_value()) << ','
        << "\"commandCount\":" << script.commands.size() << ','
        << "\"queryCount\":" << script.queries.size();
    if (script.useComp.has_value()) {
        out << ",\"useComp\":" << jsonString(script.useComp->compName);
    }
    out << '}';
    return out.str();
}

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
            } else if (std::isspace(static_cast<unsigned char>(c))) {
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
    if (s.empty()) return false;
    std::size_t start = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    return start < s.size() &&
           std::all_of(s.begin() + static_cast<std::ptrdiff_t>(start), s.end(),
                       [](const char c) { return std::isdigit(static_cast<unsigned char>(c)); });
}

// Check if token is a float
bool isFloat(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    std::size_t i = 0;
    if (s[i] == '+' || s[i] == '-') {
        ++i;
    }
    bool hasDigits = false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        hasDigits = true;
        ++i;
    }
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            hasDigits = true;
            ++i;
        }
    }
    if (!hasDigits) {
        return false;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        bool hasExponentDigits = false;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            hasExponentDigits = true;
            ++i;
        }
        if (!hasExponentDigits) {
            return false;
        }
    }
    return i == s.size();
}

// Check if token is a boolean
bool isBoolean(const std::string& s) {
    return s == "true" || s == "false";
}

// Parse value from token
Value parseValue(const std::string& token) {
    if (token.empty()) return std::string();
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
        std::size_t partStart = 0;
        while (partStart <= inner.size()) {
            const auto partEnd = inner.find(',', partStart);
            parts.push_back(trim(inner.substr(
                partStart,
                partEnd == std::string::npos ? std::string::npos : partEnd - partStart)));
            if (partEnd == std::string::npos) break;
            partStart = partEnd + 1;
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
    std::vector<BinOp> joins;
    BinOp nextJoin = BinOp::And;
    for (size_t i = start; i < tokens.size(); ) {
        if (tokens[i] == "and" || tokens[i] == "or") {
            nextJoin = tokens[i] == "or" ? BinOp::Or : BinOp::And;
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
        if (conditions.size() > 1) {
            joins.push_back(nextJoin);
        }
        nextJoin = BinOp::And;
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
        expr.op = joins[i - 1];
        expr.rhs = std::move(conditions[i]);
        result = std::make_unique<BinaryExpr>(std::move(expr));
        // Actually we need a proper AND chain, but for simplicity we flatten later
    }
    return result;
}

} // namespace

FrameTime FrameExpr::resolve(
    const std::unordered_map<std::string, FrameTime>& context) const
{
    if (const auto* frame = std::get_if<int64_t>(&value)) {
        return *frame;
    }
    const auto& expression = std::get<std::string>(value);
    if (const auto it = context.find(expression); it != context.end()) {
        return it->second;
    }
    try {
        std::size_t consumed = 0;
        const auto resolved = std::stoll(expression, &consumed);
        return consumed == expression.size() ? resolved : 0;
    } catch (const std::exception&) {
        return 0;
    }
}

bool PropertyRef::evaluate(const std::unordered_map<std::string, Value>& props) const {
    auto it = props.find(path);
    if (it == props.end()) return false;
    if (const auto* boolean = std::get_if<bool>(&it->second)) return *boolean;
    if (const auto* integer = std::get_if<int64_t>(&it->second)) return *integer != 0;
    if (const auto* real = std::get_if<double>(&it->second)) {
        return std::isfinite(*real) && *real != 0.0;
    }
    if (const auto* text = std::get_if<std::string>(&it->second)) return !text->empty();
    return false;
}

bool Literal::evaluate(const std::unordered_map<std::string, Value>& /*props*/) const {
    return true;  // Literals don't need evaluation; they're values
}

namespace {

std::optional<Value> expressionValue(
    const ExprNode* expression,
    const std::unordered_map<std::string, Value>& props)
{
    if (const auto* property = dynamic_cast<const PropertyRef*>(expression)) {
        const auto it = props.find(property->path);
        return it == props.end() ? std::nullopt
                                 : std::optional<Value>(it->second);
    }
    if (const auto* literal = dynamic_cast<const Literal*>(expression)) {
        return literal->value;
    }
    if (const auto* binary = dynamic_cast<const BinaryExpr*>(expression)) {
        return Value(binary->evaluate(props));
    }
    return std::nullopt;
}

bool numericValue(const Value& value, double& out)
{
    if (const auto* integer = std::get_if<int64_t>(&value)) {
        out = static_cast<double>(*integer);
        return true;
    }
    if (const auto* real = std::get_if<double>(&value)) {
        out = *real;
        return true;
    }
    return false;
}

bool equalValue(const Value& lhs, const Value& rhs)
{
    double leftNumber = 0.0;
    double rightNumber = 0.0;
    if (numericValue(lhs, leftNumber) && numericValue(rhs, rightNumber)) {
        return leftNumber == rightNumber;
    }
    return lhs == rhs;
}

} // namespace

bool BinaryExpr::evaluate(const std::unordered_map<std::string, Value>& props) const {
    if (op == BinOp::And) {
        return lhs && lhs->evaluate(props) && rhs && rhs->evaluate(props);
    }
    if (op == BinOp::Or) {
        return (lhs && lhs->evaluate(props)) || (rhs && rhs->evaluate(props));
    }
    const auto lhsValue = expressionValue(lhs.get(), props);
    const auto rhsValue = expressionValue(rhs.get(), props);
    if (!lhsValue || !rhsValue) {
        return false;
    }

    switch (op) {
    case BinOp::Eq:
        return equalValue(*lhsValue, *rhsValue);
    case BinOp::Ne:
        return !equalValue(*lhsValue, *rhsValue);
    case BinOp::Gt:
    case BinOp::Lt:
    case BinOp::Ge:
    case BinOp::Le: {
        double leftNumber = 0.0;
        double rightNumber = 0.0;
        if (numericValue(*lhsValue, leftNumber) && numericValue(*rhsValue, rightNumber)) {
            if (op == BinOp::Gt) return leftNumber > rightNumber;
            if (op == BinOp::Lt) return leftNumber < rightNumber;
            if (op == BinOp::Ge) return leftNumber >= rightNumber;
            return leftNumber <= rightNumber;
        }
        const auto* leftString = std::get_if<std::string>(&*lhsValue);
        const auto* rightString = std::get_if<std::string>(&*rhsValue);
        if (!leftString || !rightString) return false;
        if (op == BinOp::Gt) return *leftString > *rightString;
        if (op == BinOp::Lt) return *leftString < *rightString;
        if (op == BinOp::Ge) return *leftString >= *rightString;
        return *leftString <= *rightString;
    }
    case BinOp::Matches: {
        const auto* leftString = std::get_if<std::string>(&*lhsValue);
        const auto* rightString = std::get_if<std::string>(&*rhsValue);
        if (!leftString || !rightString) return false;
        try {
            return std::regex_search(*leftString, std::regex(*rightString));
        } catch (const std::regex_error&) {
            return false;
        }
    }
    }
    return false;
}

// Public parse function
ParseResult AIDSLInterpreter::parseImpl(const std::string& input) {
    ParseResult result;
    DSLScript script;

    std::string line;
    int lineNum = 0;

    // State: are we inside a transaction?
    bool inTransaction = false;
    TransactionCommand* currentTransaction = nullptr;

    std::size_t lineStart = 0;
    while (lineStart <= input.size()) {
        const auto lineEnd = input.find('\n', lineStart);
        line = input.substr(
            lineStart,
            lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (lineEnd == std::string::npos) lineStart = input.size() + 1;
        else lineStart = lineEnd + 1;
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
                if (wherePos < tokens.size()) {
                    cmd->filter = parseFilter(tokens, wherePos);
                }
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
            if (tokens.size() >= 7 && tokens[1] == "key" && tokens[2] == "at") {
                auto cmd = std::make_unique<AddKeyCommand>();
                // Parse frame
                std::string frameStr = tokens[3];
                if (frameStr.back() == 'f') {
                    frameStr.pop_back();
                }
                cmd->frame = FrameExpr{ std::stoll(frameStr) };
                // Typical DSL: "add key at 12f opacity = 0".
                if (tokens[4].empty() || tokens[5] != "=") {
                    script.hasError = true;
                    script.parseError = "Line " + std::to_string(lineNum) + ": invalid add key assignment";
                    result.error = script.parseError;
                    break;
                }
                cmd->property = tokens[4];
                std::string valueStr;
                for (size_t i = 6; i < tokens.size(); ++i) {
                    if (i > 6) valueStr += " ";
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
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid add key syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "rename") {
            if (tokens.size() >= 4 && tokens[1] == "selected" && tokens[2] == "with") {
                auto cmd = std::make_unique<RenameCommand>();
                cmd->target = RenameCommand::Target::Selected;
                cmd->templateStr = tokens[3];
                cmd->sourceLine = trimmed;
                if (inTransaction && currentTransaction) {
                    currentTransaction->body.push_back(std::move(cmd));
                } else {
                    script.commands.push_back(std::move(cmd));
                }
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid rename syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "delete") {
            if (tokens.size() >= 2 && tokens[1] == "selected") {
                auto cmd = std::make_unique<DeleteCommand>();
                cmd->target = DeleteCommand::Target::Selected;
                cmd->sourceLine = trimmed;
                if (inTransaction && currentTransaction) {
                    currentTransaction->body.push_back(std::move(cmd));
                } else {
                    script.commands.push_back(std::move(cmd));
                }
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid delete syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "group") {
            if (tokens.size() >= 4 && tokens[1] == "layers" && tokens[2] == "into") {
                auto cmd = std::make_unique<GroupCommand>();
                cmd->target = GroupCommand::Target::Selected;
                cmd->groupName = tokens[3];
                cmd->sourceLine = trimmed;
                if (inTransaction && currentTransaction) {
                    currentTransaction->body.push_back(std::move(cmd));
                } else {
                    script.commands.push_back(std::move(cmd));
                }
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid group syntax";
                result.error = script.parseError;
                break;
            }
        }
        else if (tokens[0] == "query") {
            if (tokens.size() == 2 && tokens[1] == "selected_layers") {
                script.queries.push_back(std::make_unique<QuerySelectedLayers>());
            } else if (tokens.size() == 2 && tokens[1] == "active_comp") {
                script.queries.push_back(std::make_unique<QueryActiveComp>());
            } else if (tokens.size() >= 2 && tokens[1] == "comp_size") {
                auto query = std::make_unique<QueryCompSize>();
                query->compId = tokens.size() >= 3 ? tokens[2] : std::string();
                script.queries.push_back(std::move(query));
            } else if (tokens.size() >= 4 && tokens[1] == "list" &&
                       tokens[2] == "properties" && tokens[3] == "of" &&
                       tokens.size() >= 5 && tokens[4] == "selected") {
                // The current query model represents a single layer; keep an
                // empty id so the executor can report the available lookup.
                auto query = std::make_unique<QueryListProperties>();
                query->layerId.clear();
                script.queries.push_back(std::move(query));
            } else if (tokens.size() >= 4 && tokens[1] == "find" &&
                       tokens[2] == "layers" && tokens[3] == "where") {
                auto query = std::make_unique<QueryFindLayers>();
                if (tokens.size() > 4) {
                    query->filter = parseFilter(tokens, 4);
                }
                script.queries.push_back(std::move(query));
            } else if (tokens.size() >= 3 && tokens[1] == "describe" &&
                       tokens[2] == "layer") {
                auto query = std::make_unique<QueryDescribeLayer>();
                query->layerId = tokens.size() >= 4 ? tokens[3] : std::string();
                script.queries.push_back(std::move(query));
            } else {
                script.hasError = true;
                script.parseError = "Line " + std::to_string(lineNum) + ": invalid query syntax";
                result.error = script.parseError;
                break;
            }
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

// Command compilation and host-action preparation
std::unique_ptr<Action> UseCompCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)layerMap;
    CompID resolved = resolvedCompId;
    if (resolved.empty() && compName.starts_with('#')) {
        resolved = compName;
    }
    if (resolved.empty()) {
        const auto it = compMap.find(compName);
        if (it == compMap.end()) {
            return nullptr;
        }
        resolved = it->second;
    }
    auto action = std::make_unique<CommandAction>();
    action->kind = "use_comp";
    action->argument = resolved;
    return action;
}

std::unique_ptr<Action> SelectLayersCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)compMap;
    auto action = std::make_unique<CommandAction>();
    action->kind = "select_layers";
    resolvedLayerIds.clear();
    for (const auto& [name, layerIds] : layerMap) {
        for (const auto& layerId : layerIds) {
            if (!filter) {
                resolvedLayerIds.push_back(layerId);
                continue;
            }
            std::unordered_map<std::string, Value> props;
            props.emplace("id", layerId);
            props.emplace("name", name);
            props.emplace("layer.id", layerId);
            props.emplace("layer.name", name);
            if (filter->evaluate(props)) {
                resolvedLayerIds.push_back(layerId);
            }
        }
    }
    action->valueText = filter ? "filtered" : "all";
    StringBuilder resolved;
    for (std::size_t i = 0; i < resolvedLayerIds.size(); ++i) {
        if (i > 0) resolved << ',';
        resolved << resolvedLayerIds[i];
    }
    action->argument = resolved.str();
    return action;
}

std::unique_ptr<Action> SetPropertyCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)compMap;
    if (property.empty()) {
        return nullptr;
    }
    if (target == Target::Specific &&
        (!specificLayerId || specificLayerId->empty())) {
        return nullptr;
    }
    if (target == Target::Specific) {
        bool known = false;
        for (const auto& [name, layerIds] : layerMap) {
            (void)name;
            if (std::find(layerIds.begin(), layerIds.end(), *specificLayerId) != layerIds.end()) {
                known = true;
                break;
            }
        }
        if (!known) return nullptr;
    }
    auto action = std::make_unique<CommandAction>();
    action->kind = "set_property";
    action->argument = property;
    action->targetText = target == Target::Selected ? "selected"
                       : target == Target::All ? "all"
                       : *specificLayerId;
    action->valueText = std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) {
            return item ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return item;
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            StringBuilder text;
            text << '[';
            for (std::size_t i = 0; i < item.size(); ++i) {
                if (i > 0) text << ',';
                text << item[i];
            }
            text << ']';
            return text.str();
        } else {
            return std::to_string(item);
        }
    }, value);
    return action;
}

std::unique_ptr<Action> AddKeyCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)compMap;
    if (target == Target::Specific &&
        (!specificLayerId || specificLayerId->empty())) {
        return nullptr;
    }
    if (target == Target::Specific) {
        bool known = false;
        for (const auto& [name, layerIds] : layerMap) {
            (void)name;
            if (std::find(layerIds.begin(), layerIds.end(), *specificLayerId) != layerIds.end()) {
                known = true;
                break;
            }
        }
        if (!known) return nullptr;
    }
    if (property.empty()) {
        return nullptr;
    }
    auto action = std::make_unique<CommandAction>();
    action->kind = "add_key";
    action->argument = property;
    action->targetText = target == Target::Selected ? "selected"
                       : target == Target::All ? "all"
                       : *specificLayerId;
    if (const auto* frame = std::get_if<int64_t>(&this->frame.value)) {
        action->frameText = std::to_string(*frame);
    } else {
        action->frameText = std::get<std::string>(this->frame.value);
    }
    action->valueText = std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) {
            return item ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            return item;
        } else if constexpr (std::is_same_v<T, std::vector<double>>) {
            StringBuilder text;
            text << '[';
            for (std::size_t i = 0; i < item.size(); ++i) {
                if (i > 0) text << ',';
                text << item[i];
            }
            text << ']';
            return text.str();
        } else {
            return std::to_string(item);
        }
    }, value);
    return action;
}

std::unique_ptr<Action> RenameCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)compMap;
    if (templateStr.empty()) {
        return nullptr;
    }
    if (target == RenameCommand::Target::Specific) {
        if (!specificLayerId || specificLayerId->empty()) return nullptr;
        bool known = false;
        for (const auto& [name, layerIds] : layerMap) {
            (void)name;
            if (std::find(layerIds.begin(), layerIds.end(), *specificLayerId) != layerIds.end()) {
                known = true;
                break;
            }
        }
        if (!known) return nullptr;
    }
    auto action = std::make_unique<CommandAction>();
    action->kind = "rename";
    action->argument = templateStr;
    action->targetText = target == RenameCommand::Target::Selected ? "selected"
                       : target == RenameCommand::Target::All ? "all"
                       : *specificLayerId;
    return action;
}

std::unique_ptr<Action> DeleteCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)compMap;
    if (target == DeleteCommand::Target::Specific) {
        if (!specificLayerId || specificLayerId->empty()) return nullptr;
        bool known = false;
        for (const auto& [name, layerIds] : layerMap) {
            (void)name;
            if (std::find(layerIds.begin(), layerIds.end(), *specificLayerId) != layerIds.end()) {
                known = true;
                break;
            }
        }
        if (!known) return nullptr;
    }
    auto action = std::make_unique<CommandAction>();
    action->kind = "delete";
    action->targetText = target == DeleteCommand::Target::Selected ? "selected"
                       : target == DeleteCommand::Target::All ? "all"
                       : *specificLayerId;
    return action;
}

std::unique_ptr<Action> GroupCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    (void)compMap;
    if (groupName.empty()) {
        return nullptr;
    }
    if (target == GroupCommand::Target::Specific) {
        if (!specificLayerId || specificLayerId->empty()) return nullptr;
        bool known = false;
        for (const auto& [name, layerIds] : layerMap) {
            (void)name;
            if (std::find(layerIds.begin(), layerIds.end(), *specificLayerId) != layerIds.end()) {
                known = true;
                break;
            }
        }
        if (!known) return nullptr;
    }
    auto action = std::make_unique<CommandAction>();
    action->kind = "group";
    action->argument = groupName;
    action->targetText = target == GroupCommand::Target::Selected ? "selected"
                       : target == GroupCommand::Target::All ? "all"
                       : *specificLayerId;
    return action;
}

std::unique_ptr<Action> TransactionCommand::compile(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    auto transaction = std::make_unique<TransactionAction>();
    transaction->name = name;
    transaction->actions.reserve(body.size());
    for (const auto& command : body) {
        if (!command) {
            return nullptr;
        }
        auto action = command->compile(compMap, layerMap);
        if (!action) {
            return nullptr;
        }
        transaction->actions.push_back(std::move(action));
    }
    return transaction;
}

// Query execution against the currently supplied lookup context
std::string QuerySelectedLayers::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    StringBuilder out;
    out << "{\"status\":\"unavailable\",\"selectedLayerIds\":[]"
        << ",\"availableCompCount\":" << compMap.size()
        << ",\"availableLayerGroupCount\":" << layerMap.size()
        << ",\"reason\":\"selection state is not part of the query context\"}";
    return out.str();
}

std::string QueryActiveComp::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    StringBuilder out;
    out << "{\"status\":\"unavailable\",\"activeCompId\":null"
        << ",\"availableCompCount\":" << compMap.size()
        << ",\"availableLayerGroupCount\":" << layerMap.size()
        << ",\"reason\":\"active composition is not part of the query context\"}";
    return out.str();
}

std::string QueryCompSize::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    const CompID requestedId = this->compId;
    bool known = false;
    for (const auto& [name, knownCompId] : compMap) {
        (void)name;
        if (knownCompId == requestedId) {
            known = true;
            break;
        }
    }
    StringBuilder out;
    out << "{\"status\":\"unavailable\",\"compId\":" << jsonString(requestedId)
        << ",\"knownComp\":" << jsonBool(known)
        << ",\"availableLayerGroupCount\":" << layerMap.size()
        << ",\"reason\":\"composition dimensions are not part of the query context\"}";
    return out.str();
}

std::string QueryFindLayers::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    std::vector<std::string> ids;
    for (const auto& [name, layerIds] : layerMap) {
        for (const auto& layerId : layerIds) {
            if (!filter) {
                ids.push_back(layerId);
                continue;
            }

            // The lookup does not expose full host properties yet, but it does
            // provide stable layer-group names and IDs. Make those available
            // to the filter instead of silently returning every layer.
            std::unordered_map<std::string, Value> props;
            props.emplace("id", layerId);
            props.emplace("name", name);
            props.emplace("layer.id", layerId);
            props.emplace("layer.name", name);
            if (filter->evaluate(props)) {
                ids.push_back(layerId);
            }
        }
    }
    StringBuilder out;
    out << "{\"status\":\"ok\",\"matchedLayerIds\":" << jsonArray(ids)
        << ",\"availableCompCount\":" << compMap.size()
        << ",\"filterScope\":\"layer id and lookup name\"}";
    return out.str();
}

std::string QueryDescribeLayer::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    bool known = false;
    std::string groupName;
    for (const auto& [name, layerIds] : layerMap) {
        if (std::find(layerIds.begin(), layerIds.end(), layerId) != layerIds.end()) {
            known = true;
            groupName = name;
            break;
        }
    }
    StringBuilder out;
    out << "{\"status\":\"ok\",\"layerId\":" << jsonString(layerId)
        << ",\"groupName\":" << jsonString(groupName)
        << ",\"knownLayer\":" << jsonBool(known)
        << ",\"availableCompCount\":" << compMap.size() << "}";
    return out.str();
}

std::string QueryListProperties::execute(
    const std::unordered_map<std::string, CompID>& compMap,
    const std::unordered_map<std::string, std::vector<LayerID>>& layerMap
) const {
    bool known = false;
    std::string groupName;
    for (const auto& [name, layerIds] : layerMap) {
        if (std::find(layerIds.begin(), layerIds.end(), layerId) != layerIds.end()) {
            known = true;
            groupName = name;
            break;
        }
    }
    StringBuilder out;
    out << "{\"status\":\"ok\",\"layerId\":" << jsonString(layerId)
        << ",\"properties\":[\"id\",\"name\",\"layer.id\",\"layer.name\"]"
        << ",\"propertyValues\":{\"id\":" << jsonString(layerId)
        << ",\"name\":" << jsonString(groupName)
        << ",\"layer.id\":" << jsonString(layerId)
        << ",\"layer.name\":" << jsonString(groupName) << "}"
        << ",\"knownLayer\":" << jsonBool(known)
        << "}";
    return out.str();
}

AIDSLInterpreter::AIDSLInterpreter() = default;
AIDSLInterpreter::~AIDSLInterpreter() = default;

std::string AIDSLInterpreter::dryRun(const DSLScript& script) const {
    std::size_t compiledActionCount = 0;
    bool compileFailed = false;
    for (const auto& command : script.commands) {
        if (!command || !command->compile(compNameToId_, layerNameToIds_)) {
            compileFailed = true;
        } else {
            ++compiledActionCount;
        }
    }
    StringBuilder out;
    out << '{'
        << "\"mode\":\"dry_run\","
        << "\"script\":" << summarizeScript(script, "dry_run") << ','
        << "\"compiledActionCount\":" << compiledActionCount << ','
        << "\"compileFailed\":" << jsonBool(compileFailed) << ','
        << "\"status\":" << jsonString(compileFailed ? "error" : "ok") << ','
        << "\"knownCompCount\":" << compNameToId_.size() << ','
        << "\"knownLayerGroupCount\":" << layerNameToIds_.size()
        << '}';
    return out.str();
}

std::string AIDSLInterpreter::execute(const DSLScript& script) {
    std::vector<std::string> queryResults;
    queryResults.reserve(script.queries.size());
    for (const auto& query : script.queries) {
        if (query) {
            queryResults.push_back(executeQuery(*query));
        }
    }

    std::size_t compiledActionCount = 0;
    bool compileFailed = false;
    bool executionFailed = false;
    const auto historyStart = undoStack_.size();
    const auto rollback = [&]() {
        if (!actionUndoExecutor_) return false;
        while (undoStack_.size() > historyStart) {
            auto action = std::move(undoStack_.back());
            undoStack_.pop_back();
            if (!actionUndoExecutor_(*action)) return false;
        }
        return true;
    };
    for (const auto& command : script.commands) {
        if (!command) {
            compileFailed = true;
            executionFailed = true;
            rollback();
            break;
        }
        auto action = command->compile(compNameToId_, layerNameToIds_);
        if (!action) {
            compileFailed = true;
            executionFailed = true;
            rollback();
            break;
        }
        if (!actionExecutor_ || !actionExecutor_(*action)) {
            executionFailed = true;
            rollback();
            break;
        }
        redoStack_.clear();
        undoStack_.push_back(std::move(action));
        ++compiledActionCount;
    }

    StringBuilder out;
    out << '{'
        << "\"mode\":\"execute\","
        << "\"script\":" << summarizeScript(script, "execute") << ','
        << "\"queryResults\":" << jsonArray(queryResults) << ','
        << "\"compiledActionCount\":" << compiledActionCount << ','
        << "\"compileFailed\":" << jsonBool(compileFailed) << ','
        << "\"executionFailed\":" << jsonBool(executionFailed) << ','
        << "\"status\":" << jsonString((compileFailed || executionFailed) ? "error" : "ok") << ','
        << "\"knownCompCount\":" << compNameToId_.size() << ','
        << "\"knownLayerGroupCount\":" << layerNameToIds_.size() << ','
        << "\"executorAttached\":" << jsonBool(static_cast<bool>(actionExecutor_))
        << '}';
    return out.str();
}

std::string AIDSLInterpreter::executeQuery(const QueryNode& query) {
    if (const auto* activeQuery = dynamic_cast<const QueryActiveComp*>(&query)) {
        (void)activeQuery;
        std::string active = activeCompId_.empty()
                                 ? std::string()
                                 : activeCompId_;
        StringBuilder out;
        out << "{\"status\":\"ok\",\"activeCompId\":" << jsonString(active)
            << ",\"availableCompCount\":" << compNameToId_.size()
            << ",\"availableLayerGroupCount\":" << layerNameToIds_.size() << "}";
        return out.str();
    }
    return query.execute(compNameToId_, layerNameToIds_);
}

bool AIDSLInterpreter::undo() {
    if (undoStack_.empty() || !actionUndoExecutor_) return false;
    auto action = std::move(undoStack_.back());
    undoStack_.pop_back();
    if (!actionUndoExecutor_(*action)) {
        undoStack_.push_back(std::move(action));
        return false;
    }
    redoStack_.push_back(std::move(action));
    return true;
}

bool AIDSLInterpreter::redo() {
    if (redoStack_.empty() || !actionExecutor_) return false;
    auto action = std::move(redoStack_.back());
    redoStack_.pop_back();
    if (!actionExecutor_(*action)) {
        redoStack_.push_back(std::move(action));
        return false;
    }
    undoStack_.push_back(std::move(action));
    return true;
}

bool AIDSLInterpreter::canUndo() const {
    return !undoStack_.empty() && static_cast<bool>(actionUndoExecutor_);
}

bool AIDSLInterpreter::canRedo() const {
    return !redoStack_.empty() && static_cast<bool>(actionExecutor_);
}

void AIDSLInterpreter::setActionUndoExecutor(
    std::function<bool(const Action&)> executor) {
    actionUndoExecutor_ = std::move(executor);
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
    return {};  // unresolved reference
}

CompID AIDSLInterpreter::resolveCompRef(const std::string& ref) const {
    if (ref.starts_with('#')) {
        return ref;
    }
    auto it = compNameToId_.find(ref);
    if (it != compNameToId_.end()) {
        return it->second;
    }
    return {};  // unresolved reference
}

std::unique_ptr<TransactionAction> AIDSLInterpreter::compileTransaction(const TransactionCommand& cmd) const {
    auto tx = std::make_unique<TransactionAction>();
    tx->name = cmd.name;
    // Compile each subcommand
    for (const auto& sub : cmd.body) {
        if (!sub) {
            return nullptr;
        }
        auto action = sub->compile(compNameToId_, layerNameToIds_);
        if (!action) {
            return nullptr;
        }
        tx->actions.push_back(std::move(action));
    }
    return tx;
}

void AIDSLInterpreter::setActionExecutor(
    std::function<bool(const Action&)> executor) {
    actionExecutor_ = std::move(executor);
}

} // namespace AIToolDSL
