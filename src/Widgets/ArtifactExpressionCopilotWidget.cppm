module;
#include <utility>
#include <wobjectimpl.h>
#include <algorithm>
#include <cmath>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QClipboard>
#include <QDrag>
#include <QMimeData>
#include <QDropEvent>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QFrame>
#include <QFile>
#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSize>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QStringList>
#include <QVariant>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QSplitter>
#include <QPointF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <memory>

module Artifact.Widgets.ExpressionCopilotWidget;

import Script.Expression.Evaluator;
import Script.Expression.Parser;
import Script.Expression.Value;
import Core.ArtifactString;
import Widgets.Utils.CSS;

namespace Artifact {

namespace {

class ExpressionSyntaxHighlighter final : public QSyntaxHighlighter {
public:
    explicit ExpressionSyntaxHighlighter(QTextDocument* document)
        : QSyntaxHighlighter(document) {
        const auto mkFormat = [](const QColor& color, bool bold = false) {
            QTextCharFormat format;
            format.setForeground(color);
            format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
            return format;
        };

        keywordFormat_ = mkFormat(QColor(52, 152, 219), true);
        numberFormat_ = mkFormat(QColor(241, 196, 15));
        stringFormat_ = mkFormat(QColor(46, 204, 113));
        functionFormat_ = mkFormat(QColor(155, 89, 182), true);
        operatorFormat_ = mkFormat(QColor(230, 126, 34));
        commentFormat_ = mkFormat(QColor(149, 165, 166), false);
    }

protected:
    void highlightBlock(const QString& text) override {
        const auto applyMatches = [&](const QRegularExpression& rx, const QTextCharFormat& format) {
            auto it = rx.globalMatch(text);
            while (it.hasNext()) {
                const auto match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), format);
            }
        };

        static const QRegularExpression keywordRx(
            QStringLiteral(R"(\b(?:if|else|then|end|unless|and|or|not)\b)"));
        static const QRegularExpression numberRx(
            QStringLiteral(R"(\b\d+(?:\.\d+)?\b)"));
        static const QRegularExpression functionRx(
            QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*\\s*(?=\\()"));
        static const QRegularExpression operatorRx(
            QStringLiteral(R"([+\-*/^?:=<>&|!])"));
        static const QRegularExpression stringRx(
            QStringLiteral(R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')"));
        static const QRegularExpression commentRx(
            QStringLiteral(R"(#.*$|//.*$)"));

        applyMatches(stringRx, stringFormat_);
        applyMatches(keywordRx, keywordFormat_);
        applyMatches(numberRx, numberFormat_);
        applyMatches(functionRx, functionFormat_);
        applyMatches(operatorRx, operatorFormat_);
        applyMatches(commentRx, commentFormat_);
    }

private:
    QTextCharFormat keywordFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat stringFormat_;
    QTextCharFormat functionFormat_;
    QTextCharFormat operatorFormat_;
    QTextCharFormat commentFormat_;
};

QString posToLineColumn(const QString& text, int position)
{
    const int textSize = static_cast<int>(text.size());
    position = std::clamp(position, 0, textSize);
    int line = 1;
    int column = 1;
    for (int i = 0; i < position; ++i) {
        if (text.at(i) == QLatin1Char('\n')) {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return QStringLiteral("line %1, col %2").arg(line).arg(column);
}

ArtifactCore::ExpressionValue variantToExpressionValue(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return ArtifactCore::ExpressionValue();
    }

    switch (value.typeId()) {
    case QMetaType::Bool:
        return ArtifactCore::ExpressionValue(value.toBool() ? 1.0 : 0.0);
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
    case QMetaType::Double:
        return ArtifactCore::ExpressionValue(value.toDouble());
    case QMetaType::QString:
        return ArtifactCore::ExpressionValue(value.toString().toStdString());
    case QMetaType::QPointF: {
        const QPointF point = value.toPointF();
        return ArtifactCore::ExpressionValue(point.x(), point.y());
    }
    case QMetaType::QVector2D: {
        const QVector2D vec = value.value<QVector2D>();
        return ArtifactCore::ExpressionValue(vec.x(), vec.y());
    }
    case QMetaType::QVector3D: {
        const QVector3D vec = value.value<QVector3D>();
        return ArtifactCore::ExpressionValue(vec.x(), vec.y(), vec.z());
    }
    case QMetaType::QVector4D: {
        const QVector4D vec = value.value<QVector4D>();
        return ArtifactCore::ExpressionValue(vec.x(), vec.y(), vec.z(), vec.w());
    }
    case QMetaType::QColor: {
        const QColor color = value.value<QColor>();
        return ArtifactCore::ExpressionValue(color.redF(), color.greenF(),
                                             color.blueF(), color.alphaF());
    }
    case QMetaType::QVariantList: {
        const QVariantList list = value.toList();
        std::vector<ArtifactCore::ExpressionValue> items;
        items.reserve(list.size());
        for (const QVariant& item : list) {
            items.push_back(variantToExpressionValue(item));
        }
        return ArtifactCore::ExpressionValue(items);
    }
    case QMetaType::QVariantMap: {
        const auto map = value.toMap();
        std::map<std::string, ArtifactCore::ExpressionValue> object;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            object[it.key().toStdString()] = variantToExpressionValue(it.value());
        }
        return ArtifactCore::ExpressionValue(object);
    }
    default:
        break;
    }

    if (value.canConvert<double>()) {
        return ArtifactCore::ExpressionValue(value.toDouble());
    }
    return ArtifactCore::ExpressionValue(value.toString().toStdString());
}

ArtifactCore::ExpressionValue buildLayerObject(const QString& layerName, int layerIndex,
                                               const QString& compositionName,
                                               const QVariantMap& snapshot = {})
{
    std::map<std::string, ArtifactCore::ExpressionValue> object;
    object["name"] = ArtifactCore::ExpressionValue(layerName.toStdString());
    object["index"] = ArtifactCore::ExpressionValue(static_cast<double>(std::max(0, layerIndex)));
    object["comp"] = ArtifactCore::ExpressionValue(
        std::map<std::string, ArtifactCore::ExpressionValue>{
            {"name", ArtifactCore::ExpressionValue(compositionName.toStdString())}
        });
    const QVariantMap transform = snapshot.value(QStringLiteral("transform")).toMap();
    const QVariantMap position = transform.value(QStringLiteral("position")).toMap();
    const QVariantMap scale = transform.value(QStringLiteral("scale")).toMap();
    object["transform"] = ArtifactCore::ExpressionValue(
        std::map<std::string, ArtifactCore::ExpressionValue>{
            {"position", ArtifactCore::ExpressionValue(
                std::map<std::string, ArtifactCore::ExpressionValue>{
                    {"x", variantToExpressionValue(position.value(QStringLiteral("x")))},
                    {"y", variantToExpressionValue(position.value(QStringLiteral("y")))}})},
            {"scale", ArtifactCore::ExpressionValue(
                std::map<std::string, ArtifactCore::ExpressionValue>{
                    {"x", variantToExpressionValue(scale.value(QStringLiteral("x")))},
                    {"y", variantToExpressionValue(scale.value(QStringLiteral("y")))}})},
            {"rotation", variantToExpressionValue(transform.value(QStringLiteral("rotation")))},
            {"opacity", variantToExpressionValue(snapshot.value(QStringLiteral("opacity")))}
        });
    return ArtifactCore::ExpressionValue(object);
}

ArtifactCore::ExpressionValue buildCompositionObject(
    const QString& compositionName,
    const QSize& compositionSize,
    const QStringList& layerNames,
    const QVariantMap& layerSnapshots = {},
    const QVariantList& compositionMarkers = {})
{
    std::vector<ArtifactCore::ExpressionValue> layers;
    layers.reserve(layerNames.size());
    for (int i = 0; i < layerNames.size(); ++i) {
        layers.push_back(buildLayerObject(layerNames.at(i), i + 1, compositionName,
                                          layerSnapshots.value(layerNames.at(i)).toMap()));
    }

    std::map<std::string, ArtifactCore::ExpressionValue> object;
    object["name"] = ArtifactCore::ExpressionValue(compositionName.toStdString());
    object["width"] = ArtifactCore::ExpressionValue(static_cast<double>(compositionSize.width()));
    object["height"] = ArtifactCore::ExpressionValue(static_cast<double>(compositionSize.height()));
    object["numLayers"] = ArtifactCore::ExpressionValue(static_cast<double>(layerNames.size()));
    object["layers"] = ArtifactCore::ExpressionValue(layers);
    object["marker"] = ArtifactCore::ExpressionValue(
        std::map<std::string, ArtifactCore::ExpressionValue>{
            {"keys", variantToExpressionValue(compositionMarkers)}});
    return ArtifactCore::ExpressionValue(object);
}

} // namespace

W_OBJECT_IMPL(ArtifactExpressionCopilotWidget)

class ArtifactExpressionCopilotWidget::Impl {
public:
    struct SuggestionCandidate {
        QString display;
        QString insert;
        QString tooltip;
    };

    struct CompletionState {
        QString prefix;
        int replaceStart = 0;
        QList<SuggestionCandidate> candidates;
    };

    QLineEdit* promptInput = nullptr;
    QTextEdit* expressionEdit = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* hintLabel = nullptr;
    QWidget* suggestionPopup = nullptr;
    QListWidget* suggestionList = nullptr;
    QListWidget* referenceList = nullptr;
    QWidget* promptToolbar = nullptr;
    QWidget* referencePanel = nullptr;
    QLabel* editorTabLabel = nullptr;
    QLabel* propertyContextLabel = nullptr;
    QLabel* timeContextLabel = nullptr;
    QPushButton* generateBtn = nullptr;
    QPushButton* applyBtn = nullptr;
    QPushButton* revertBtn = nullptr;
    QPushButton* removeBtn = nullptr;
    QPushButton* saveSnippetBtn = nullptr;
    QPushButton* loadSnippetBtn = nullptr;
    QPushButton* copyBtn = nullptr;
    QPushButton* clearBtn = nullptr;
    QPushButton* wiggleBtn = nullptr;
    QPushButton* loopBtn = nullptr;
    QPushButton* driftBtn = nullptr;
    std::function<void(const QString& expression)> applyHandler;
    QTimer* validateTimer = nullptr;
    std::unique_ptr<ExpressionSyntaxHighlighter> highlighter;
    ArtifactCore::ExpressionParser parser;
    QString previewCompositionName;
    QSize previewCompositionSize;
    QStringList previewLayerNames;
    int previewLayerIndex = -1;
    QString previewLayerName;
    QString originalExpression;
    bool expressionValid = false;
    bool inlineMode = false;
    QVariant previewValue;
    QVariantMap previewLayerSnapshots;
    QVariantList previewCompositionMarkers;
    double previewTimeSeconds = 0.0;

    static QList<SuggestionCandidate> rootSuggestions()
    {
        return {
            { QStringLiteral("thisComp"), QStringLiteral("thisComp"), QStringLiteral("Composition context") },
            { QStringLiteral("thisLayer"), QStringLiteral("thisLayer"), QStringLiteral("Current layer context") },
            { QStringLiteral("time"), QStringLiteral("time"), QStringLiteral("Current time") },
            { QStringLiteral("value"), QStringLiteral("value"), QStringLiteral("Current property value") },
            { QStringLiteral("index"), QStringLiteral("index"), QStringLiteral("Layer index") },
            { QStringLiteral("linear"), QStringLiteral("linear"), QStringLiteral("Linear interpolation") },
            { QStringLiteral("ease"), QStringLiteral("ease"), QStringLiteral("Ease interpolation") },
            { QStringLiteral("easeIn"), QStringLiteral("easeIn"), QStringLiteral("Ease-in interpolation") },
            { QStringLiteral("easeOut"), QStringLiteral("easeOut"), QStringLiteral("Ease-out interpolation") },
            { QStringLiteral("length"), QStringLiteral("length"), QStringLiteral("Vector or string length") },
            { QStringLiteral("distance"), QStringLiteral("distance"), QStringLiteral("Distance between points") },
            { QStringLiteral("normalize"), QStringLiteral("normalize"), QStringLiteral("Normalize a vector") },
            { QStringLiteral("clamp"), QStringLiteral("clamp"), QStringLiteral("Clamp a value") },
            { QStringLiteral("random"), QStringLiteral("random"), QStringLiteral("Random value") },
            { QStringLiteral("wiggle"), QStringLiteral("wiggle"), QStringLiteral("Procedural motion") },
            { QStringLiteral("sin"), QStringLiteral("sin"), QStringLiteral("Sine function") },
            { QStringLiteral("cos"), QStringLiteral("cos"), QStringLiteral("Cosine function") },
            { QStringLiteral("tan"), QStringLiteral("tan"), QStringLiteral("Tangent function") },
            { QStringLiteral("degToRad"), QStringLiteral("degToRad"), QStringLiteral("Degrees to radians") },
            { QStringLiteral("radToDeg"), QStringLiteral("radToDeg"), QStringLiteral("Radians to degrees") }
        };
    }

    static QList<SuggestionCandidate> thisCompSuggestions()
    {
        return {
            { QStringLiteral("width"), QStringLiteral("thisComp.width"), QStringLiteral("Composition width") },
            { QStringLiteral("height"), QStringLiteral("thisComp.height"), QStringLiteral("Composition height") },
            { QStringLiteral("name"), QStringLiteral("thisComp.name"), QStringLiteral("Composition name") },
            { QStringLiteral("app_name"), QStringLiteral("thisComp.app_name"), QStringLiteral("Host application name") },
            { QStringLiteral("app_version"), QStringLiteral("thisComp.app_version"), QStringLiteral("Host application version") },
            { QStringLiteral("working_directory"), QStringLiteral("thisComp.working_directory"), QStringLiteral("Working directory") },
            { QStringLiteral("has_project"), QStringLiteral("thisComp.has_project"), QStringLiteral("Whether a project is loaded") },
            { QStringLiteral("has_composition"), QStringLiteral("thisComp.has_composition"), QStringLiteral("Whether a composition exists") },
            { QStringLiteral("selection_count"), QStringLiteral("thisComp.selection_count"), QStringLiteral("Selection count") },
            { QStringLiteral("numLayers"), QStringLiteral("thisComp.numLayers"), QStringLiteral("Number of available layers") },
            { QStringLiteral("layers"), QStringLiteral("thisComp.layers"), QStringLiteral("Layer catalog") },
            { QStringLiteral("layer(\"...\")"), QStringLiteral("thisComp.layer(\"...\")"), QStringLiteral("Lookup a layer by name") }
        };
    }

    static QList<SuggestionCandidate> thisLayerSuggestions()
    {
        return {
            { QStringLiteral("name"), QStringLiteral("thisLayer.name"), QStringLiteral("Layer name") },
            { QStringLiteral("index"), QStringLiteral("thisLayer.index"), QStringLiteral("Layer index") },
            { QStringLiteral("comp"), QStringLiteral("thisLayer.comp"), QStringLiteral("Owning composition") },
            { QStringLiteral("selection_count"), QStringLiteral("thisLayer.selection_count"), QStringLiteral("Selection count") }
        };
    }

    static QList<SuggestionCandidate> suggestionsForPrefix(const QString& prefix)
    {
        const QString lowerPrefix = prefix.toLower();
        if (lowerPrefix.startsWith(QStringLiteral("thiscomp."))) {
            return thisCompSuggestions();
        }
        if (lowerPrefix.startsWith(QStringLiteral("thislayer."))) {
            return thisLayerSuggestions();
        }
        return rootSuggestions();
    }

    static bool isLayerMemberContext(const QString& text)
    {
        static const QRegularExpression rx(
            QStringLiteral(R"((?:thisComp\.layer\s*\([^)]*\)|thisLayer)\s*$)"),
            QRegularExpression::CaseInsensitiveOption);
        return rx.match(text).hasMatch();
    }

    CompletionState completionState() const
    {
        CompletionState state;
        if (!expressionEdit) {
            return state;
        }

        const QString text = expressionEdit->toPlainText();
        const int pos = expressionEdit->textCursor().position();
        const QString rawPrefix = currentCompletionPrefix();

        state.candidates = rootSuggestions();
        state.prefix = rawPrefix;
        state.replaceStart = std::max(0, static_cast<int>(pos - rawPrefix.size()));

        if (rawPrefix.isEmpty()) {
            if (pos > 0 && text.at(pos - 1) == QLatin1Char('.')) {
                const QString anchor = text.left(pos - 1).trimmed();
                if (anchor.endsWith(QStringLiteral("thisComp"), Qt::CaseInsensitive)) {
                    state.candidates = thisCompSuggestions();
                } else if (anchor.endsWith(QStringLiteral("thisLayer"), Qt::CaseInsensitive) || isLayerMemberContext(anchor)) {
                    state.candidates = thisLayerSuggestions();
                } else {
                    state.candidates.clear();
                }
            }
            return state;
        }

        const int lastDot = rawPrefix.lastIndexOf(QLatin1Char('.'));
        if (lastDot >= 0) {
            const QString anchor = rawPrefix.left(lastDot);
            const QString suffix = rawPrefix.mid(lastDot + 1);
            state.prefix = suffix;
            state.replaceStart = std::max(0, static_cast<int>(pos - suffix.size()));
            const QString contextText = text.left(std::max(0, static_cast<int>(pos - rawPrefix.size())));

            if (anchor.endsWith(QStringLiteral("thisComp"), Qt::CaseInsensitive)) {
                state.candidates = thisCompSuggestions();
            } else if (anchor.endsWith(QStringLiteral("thisLayer"), Qt::CaseInsensitive)) {
                state.candidates = thisLayerSuggestions();
            } else if (isLayerMemberContext(contextText)) {
                state.candidates = thisLayerSuggestions();
            } else {
                state.candidates.clear();
            }
            return state;
        }

        return state;
    }

    void setHint(const QString& text, const QColor& color)
    {
        if (!hintLabel) {
            return;
        }
        hintLabel->setText(text);
        QPalette pal = hintLabel->palette();
        pal.setColor(QPalette::WindowText, color);
        hintLabel->setPalette(pal);
    }

    void setStatus(const QString& text, const QColor& color) {
        if (!statusLabel) {
            return;
        }
        statusLabel->setText(text);
        {
            QPalette pal = statusLabel->palette();
            pal.setColor(QPalette::WindowText, color);
            statusLabel->setPalette(pal);
        }
    }

    QString currentCompletionPrefix() const
    {
        if (!expressionEdit) {
            return {};
        }

        const QString text = expressionEdit->toPlainText();
        const int pos = expressionEdit->textCursor().position();
        int begin = pos;
        while (begin > 0) {
            const QChar ch = text.at(begin - 1);
            if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('.'))) {
                break;
            }
            --begin;
        }
        return text.mid(begin, pos - begin);
    }

    void hideSuggestions()
    {
        if (suggestionPopup) {
            suggestionPopup->hide();
        }
    }

    void showSuggestions(const QString&)
    {
        if (!expressionEdit || !suggestionPopup || !suggestionList) {
            return;
        }

        const CompletionState state = completionState();
        const QString text = expressionEdit->toPlainText();
        const int pos = expressionEdit->textCursor().position();
        const bool afterDot = pos > 0 && text.at(pos - 1) == QLatin1Char('.');
        if (state.prefix.isEmpty() && !afterDot) {
            hideSuggestions();
            return;
        }

        suggestionList->clear();
        int added = 0;
        for (const auto& candidate : state.candidates) {
            if (!state.prefix.isEmpty() && !candidate.insert.startsWith(state.prefix, Qt::CaseInsensitive)) {
                continue;
            }
            auto* item = new QListWidgetItem(candidate.display, suggestionList);
            item->setData(Qt::UserRole, candidate.insert);
            item->setToolTip(candidate.tooltip.isEmpty() ? candidate.insert : candidate.tooltip);
            ++added;
        }

        if (added == 0) {
            hideSuggestions();
            return;
        }

        suggestionList->setCurrentRow(0);
        suggestionList->setMinimumWidth(std::max(260, expressionEdit->width()));
        suggestionList->setMinimumHeight(std::min(180, 24 * added + 6));
        const QPoint popupPos = expressionEdit->mapToGlobal(QPoint(0, expressionEdit->height()));
        suggestionPopup->move(popupPos.x(), popupPos.y() + 4);
        suggestionPopup->resize(suggestionList->minimumWidth(), suggestionList->minimumHeight());
        suggestionPopup->show();
        suggestionPopup->raise();
    }

    bool acceptSuggestion()
    {
        if (!expressionEdit || !suggestionList || suggestionList->count() == 0) {
            return false;
        }

        QListWidgetItem* current = suggestionList->currentItem();
        if (!current) {
            current = suggestionList->item(0);
        }
        if (!current) {
            return false;
        }

        const QString chosen = current->data(Qt::UserRole).toString().isEmpty()
            ? current->text()
            : current->data(Qt::UserRole).toString();
        const CompletionState state = completionState();
        QTextCursor cursor = expressionEdit->textCursor();
        const int pos = cursor.position();
        cursor.setPosition(state.replaceStart);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
        cursor.insertText(chosen);
        expressionEdit->setTextCursor(cursor);
        hideSuggestions();
        return true;
    }

    bool handleSuggestionKey(QKeyEvent* keyEvent)
    {
        if (!suggestionPopup || !suggestionPopup->isVisible() || !suggestionList) {
            return false;
        }

        if (keyEvent->key() == Qt::Key_Up) {
            suggestionList->setCurrentRow(std::max(0, suggestionList->currentRow() - 1));
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            suggestionList->setCurrentRow(std::min(suggestionList->count() - 1, suggestionList->currentRow() + 1));
            return true;
        }
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            return acceptSuggestion();
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            hideSuggestions();
            return true;
        }
        return false;
    }

    void applyErrorSelection(int position, int length, const QString& text) {
        if (!expressionEdit) {
            return;
        }
        QList<QTextEdit::ExtraSelection> extras;
        if (position >= 0 && position < text.size()) {
            QTextCursor cursor(expressionEdit->document());
            cursor.setPosition(position);
            const int textSize = static_cast<int>(text.size());
            cursor.setPosition(std::min(position + std::max(1, length), textSize), QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(QColor(110, 20, 20, 70));
            selection.format.setUnderlineColor(QColor(255, 80, 80));
            selection.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            extras.push_back(selection);
        }
        expressionEdit->setExtraSelections(extras);
    }

    void validateExpression() {
        if (!expressionEdit) {
            return;
        }

        const QString text = expressionEdit->toPlainText();
        expressionValid = false;
        if (applyBtn) {
            applyBtn->setEnabled(false);
        }
        const std::string expr = text.toStdString();
        if (expr.empty()) {
            setStatus(QStringLiteral("Expression is empty"), QColor(148, 163, 184));
            setHint(QStringLiteral("Try: thisComp.width, thisLayer.name, or wiggle(3, 50)"), QColor(148, 163, 184));
            applyErrorSelection(-1, 0, text);
            return;
        }

        const auto ast = parser.parse(expr);
        if (ast) {
            ArtifactCore::ExpressionEvaluator evaluator;
            evaluator.setVariable("value", variantToExpressionValue(previewValue));
            evaluator.setVariable("time", ArtifactCore::ExpressionValue(previewTimeSeconds));
            if (previewLayerIndex >= 0) {
                // AE-style global layer index (1-based), matching thisLayer.index.
                evaluator.setVariable("index", ArtifactCore::ExpressionValue(static_cast<double>(previewLayerIndex + 1)));
            }
            if (!previewCompositionName.isEmpty() || !previewLayerName.isEmpty()) {
                evaluator.setVariable(
                    "thisComp",
                    buildCompositionObject(previewCompositionName,
                                           previewCompositionSize,
                                           previewLayerNames,
                                           previewLayerSnapshots,
                                           previewCompositionMarkers));
                evaluator.setVariable(
                    "thisLayer",
                    buildLayerObject(previewLayerName,
                                     previewLayerIndex >= 0 ? previewLayerIndex + 1 : 0,
                                     previewCompositionName,
                                     previewLayerSnapshots.value(previewLayerName).toMap()));
            }

            const auto runtime = evaluator.evaluate(expr);
            if (evaluator.hasError()) {
                setStatus(
                    QStringLiteral("Runtime error: %1")
                        .arg(QString::fromStdString(evaluator.getError())),
                    QColor(248, 113, 113));
            } else {
                expressionValid = true;
                setStatus(
                    QStringLiteral("Runtime OK: %1")
                        .arg(QString::fromStdString(
                                 ArtifactCore::toStdString(runtime.toString()))
                                 .left(96)),
                    QColor(74, 222, 128));
            }
            setHint(currentHintText(text), QColor(96, 165, 250));
            applyErrorSelection(-1, 0, text);
            if (applyBtn) {
                applyBtn->setEnabled(expressionValid);
            }
            return;
        }

        const QString error = QString::fromStdString(parser.getError());
        const int position = static_cast<int>(parser.getErrorPosition());
        const int length = static_cast<int>(std::max<std::size_t>(1, parser.getErrorLength()));
        const QString location = posToLineColumn(text, position);
        setStatus(QStringLiteral("%1 at %2").arg(error.isEmpty() ? QStringLiteral("Syntax error") : error, location),
                  QColor(248, 113, 113));
        setHint(currentHintText(text), QColor(248, 180, 0));
        applyErrorSelection(position, length, text);
    }

    QString currentHintText(const QString&) const
    {
        if (!expressionEdit) {
            return QStringLiteral("Hints: thisComp, thisLayer, linear, ease, wiggle");
        }

        const CompletionState state = completionState();
        QStringList matches;
        for (const auto& candidate : state.candidates) {
            if (state.prefix.isEmpty() || candidate.insert.startsWith(state.prefix, Qt::CaseInsensitive)) {
                matches.push_back(candidate.display);
            }
        }

        if (matches.isEmpty()) {
            return QStringLiteral("Hints: thisComp, thisLayer, linear, ease, wiggle");
        }

        const int maxCount = std::min(4, static_cast<int>(matches.size()));
        QStringList shown = matches.mid(0, maxCount);
        if (matches.size() > maxCount) {
            shown.push_back(QStringLiteral("..."));
        }
        return QStringLiteral("Hints: %1").arg(shown.join(QStringLiteral("   ")));
    }

    bool completeCurrentWord()
    {
        if (!expressionEdit) {
            return false;
        }

        QTextCursor cursor = expressionEdit->textCursor();
        const QString text = expressionEdit->toPlainText();
        const int pos = cursor.position();
        const CompletionState state = completionState();
        const bool afterDot = pos > 0 && text.at(pos - 1) == QLatin1Char('.');
        if (state.prefix.isEmpty() && !afterDot) {
            return false;
        }

        QString bestCandidate;
        for (const auto& candidate : state.candidates) {
            if (state.prefix.isEmpty() || candidate.insert.startsWith(state.prefix, Qt::CaseInsensitive)) {
                bestCandidate = candidate.insert;
                break;
            }
        }

        if (bestCandidate.isEmpty()) {
            return false;
        }

        const QString suffix = bestCandidate.mid(state.prefix.size());
        if (suffix.isEmpty()) {
            return false;
        }

        cursor.setPosition(state.replaceStart);
        cursor.setPosition(pos, QTextCursor::KeepAnchor);
        cursor.insertText(bestCandidate);
        expressionEdit->setTextCursor(cursor);
        return true;
    }

    void setupUi(QWidget* parent) {
        const auto& theme = ArtifactCore::currentDCCTheme();
        const QColor windowColor(theme.backgroundColor);
        const QColor panelColor(theme.secondaryBackgroundColor);
        const QColor toolbarColor(theme.buttonColor);
        const QColor editorColor(theme.trackBackgroundColor);
        const QColor borderColor(theme.borderColor);
        const QColor textColor(theme.textColor);
        const QColor mutedColor(theme.textMutedColor);
        const QColor accentColor(theme.accentColor);

        QPalette rootPalette = parent->palette();
        rootPalette.setColor(QPalette::Window, windowColor);
        rootPalette.setColor(QPalette::WindowText, textColor);
        rootPalette.setColor(QPalette::Base, editorColor);
        rootPalette.setColor(QPalette::AlternateBase, panelColor);
        rootPalette.setColor(QPalette::Text, textColor);
        rootPalette.setColor(QPalette::Button, toolbarColor);
        rootPalette.setColor(QPalette::ButtonText, textColor);
        rootPalette.setColor(QPalette::Highlight, accentColor);
        rootPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        rootPalette.setColor(QPalette::PlaceholderText, mutedColor);
        rootPalette.setColor(QPalette::Mid, borderColor);
        parent->setPalette(rootPalette);
        parent->setAutoFillBackground(true);

        const auto makeBar = [&](const QColor& fill) {
            auto* frame = new QFrame(parent);
            frame->setFrameShape(QFrame::StyledPanel);
            frame->setFrameShadow(QFrame::Plain);
            QPalette palette = rootPalette;
            palette.setColor(QPalette::Window, fill);
            frame->setPalette(palette);
            frame->setAutoFillBackground(true);
            return frame;
        };

        const auto setMuted = [&](QLabel* label) {
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, mutedColor);
            label->setPalette(palette);
        };

        const auto configureFlatAction = [&](QPushButton* button) {
            button->setFlat(true);
            button->setMinimumHeight(28);
            button->setMaximumHeight(30);
            button->setFocusPolicy(Qt::StrongFocus);
        };

        auto* layout = new QVBoxLayout(parent);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        auto* titleBar = makeBar(QColor(20, 26, 33));
        auto* headerLayout = new QHBoxLayout(titleBar);
        headerLayout->setContentsMargins(12, 7, 12, 7);
        headerLayout->setSpacing(8);
        auto* titleLabel = new QLabel(QStringLiteral("Expression Editor"), titleBar);
        {
            QFont font = titleLabel->font();
            font.setBold(true);
            font.setPointSize(11);
            titleLabel->setFont(font);
        }
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        auto* collapseHint = new QLabel(QString::fromUtf8("⌄"), titleBar);
        setMuted(collapseHint);
        headerLayout->addWidget(collapseHint);
        layout->addWidget(titleBar);

        auto* tabBar = makeBar(QColor(18, 24, 31));
        auto* tabLayout = new QHBoxLayout(tabBar);
        tabLayout->setContentsMargins(0, 0, 0, 0);
        tabLayout->setSpacing(0);
        auto* editorTab = makeBar(panelColor);
        editorTab->setParent(tabBar);
        editorTab->setMinimumWidth(230);
        editorTab->setMaximumWidth(320);
        auto* editorTabLayout = new QHBoxLayout(editorTab);
        editorTabLayout->setContentsMargins(12, 7, 10, 7);
        editorTabLayout->setSpacing(9);
        auto* tabIcon = new QLabel(QStringLiteral("ƒx"), editorTab);
        {
            QPalette palette = tabIcon->palette();
            palette.setColor(QPalette::WindowText, accentColor);
            tabIcon->setPalette(palette);
        }
        editorTabLabel = new QLabel(QStringLiteral("Position.expression"), editorTab);
        auto* tabClose = new QLabel(QString::fromUtf8("×"), editorTab);
        setMuted(tabClose);
        editorTabLayout->addWidget(tabIcon);
        editorTabLayout->addWidget(editorTabLabel, 1);
        editorTabLayout->addWidget(tabClose);
        tabLayout->addWidget(editorTab);
        tabLayout->addStretch();
        layout->addWidget(tabBar);

        auto* contextBar = makeBar(toolbarColor);
        auto* contextLayout = new QHBoxLayout(contextBar);
        contextLayout->setContentsMargins(12, 6, 12, 6);
        contextLayout->setSpacing(10);
        propertyContextLabel = new QLabel(QStringLiteral("Title / Position"), contextBar);
        auto* separator = new QFrame(contextBar);
        separator->setFrameShape(QFrame::VLine);
        separator->setFrameShadow(QFrame::Plain);
        timeContextLabel = new QLabel(QStringLiteral("Comp 1  ·  00:00:03:12"), contextBar);
        setMuted(timeContextLabel);
        contextLayout->addWidget(propertyContextLabel);
        contextLayout->addWidget(separator);
        contextLayout->addWidget(timeContextLabel);
        contextLayout->addStretch();
        layout->addWidget(contextBar);

        promptToolbar = makeBar(toolbarColor);
        auto* promptLayout = new QHBoxLayout(promptToolbar);
        promptLayout->setContentsMargins(12, 6, 12, 6);
        promptLayout->setSpacing(8);
        promptInput = new QLineEdit(promptToolbar);
        promptInput->setPlaceholderText(QStringLiteral("Describe the motion you want... (e.g. wiggle 3 times a second)"));
        promptInput->setMinimumHeight(30);
        generateBtn = new QPushButton(QStringLiteral("Generate"), promptToolbar);
        wiggleBtn = new QPushButton(QStringLiteral("Wiggle"), promptToolbar);
        loopBtn = new QPushButton(QStringLiteral("Loop"), promptToolbar);
        driftBtn = new QPushButton(QStringLiteral("Drift"), promptToolbar);
        configureFlatAction(wiggleBtn);
        configureFlatAction(loopBtn);
        configureFlatAction(driftBtn);
        generateBtn->setMinimumHeight(30);
        {
            QPalette palette = generateBtn->palette();
            palette.setColor(QPalette::Button, accentColor);
            palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
            generateBtn->setPalette(palette);
            generateBtn->setAutoFillBackground(true);
        }
        promptLayout->addWidget(promptInput, 1);
        promptLayout->addWidget(generateBtn);
        promptLayout->addWidget(wiggleBtn);
        promptLayout->addWidget(loopBtn);
        promptLayout->addWidget(driftBtn);
        layout->addWidget(promptToolbar);

        auto* workspaceSplitter = new QSplitter(Qt::Horizontal, parent);
        workspaceSplitter->setChildrenCollapsible(false);
        workspaceSplitter->setHandleWidth(1);

        expressionEdit = new QTextEdit(workspaceSplitter);
        expressionEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        expressionEdit->setPlaceholderText(QStringLiteral("Enter an expression here..."));
        expressionEdit->setLineWrapMode(QTextEdit::NoWrap);
        expressionEdit->setAcceptDrops(true);
        expressionEdit->setFrameShape(QFrame::NoFrame);
        expressionEdit->setTabStopDistance(expressionEdit->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4.0);
        QPalette editorPalette = rootPalette;
        editorPalette.setColor(QPalette::Base, editorColor);
        editorPalette.setColor(QPalette::Text, textColor);
        expressionEdit->setPalette(editorPalette);

        referencePanel = makeBar(panelColor);
        referencePanel->setParent(workspaceSplitter);
        referencePanel->setMinimumWidth(180);
        referencePanel->setMaximumWidth(320);
        auto* referenceLayout = new QVBoxLayout(referencePanel);
        referenceLayout->setContentsMargins(0, 0, 0, 0);
        referenceLayout->setSpacing(0);
        auto* referenceHeader = new QLabel(QStringLiteral("References"), referencePanel);
        referenceHeader->setContentsMargins(12, 8, 12, 8);
        referenceLayout->addWidget(referenceHeader);
        referenceList = new QListWidget(referencePanel);
        referenceList->setDragEnabled(true);
        referenceList->setSelectionMode(QAbstractItemView::SingleSelection);
        referenceList->setFrameShape(QFrame::NoFrame);
        referenceList->setAlternatingRowColors(false);
        referenceList->setSpacing(1);
        referenceList->setToolTip(QStringLiteral("Drag a reference into the expression editor"));
        QPalette referencePalette = rootPalette;
        referencePalette.setColor(QPalette::Base, panelColor);
        referencePalette.setColor(QPalette::AlternateBase, toolbarColor);
        referenceList->setPalette(referencePalette);
        referenceLayout->addWidget(referenceList, 1);

        workspaceSplitter->addWidget(expressionEdit);
        workspaceSplitter->addWidget(referencePanel);
        workspaceSplitter->setStretchFactor(0, 1);
        workspaceSplitter->setStretchFactor(1, 0);
        workspaceSplitter->setSizes({720, 240});
        layout->addWidget(workspaceSplitter, 1);

        auto* statusBar = makeBar(QColor(18, 24, 31));
        auto* statusLayout = new QHBoxLayout(statusBar);
        statusLayout->setContentsMargins(12, 5, 12, 5);
        statusLayout->setSpacing(12);
        statusLabel = new QLabel(QStringLiteral("Ready"), statusBar);
        statusLabel->setWordWrap(true);
        {
            QPalette pal = statusLabel->palette();
            pal.setColor(QPalette::WindowText, QColor(148, 163, 184));
            statusLabel->setPalette(pal);
        }
        hintLabel = new QLabel(QStringLiteral("Hints: thisComp, thisLayer, linear, ease, wiggle"), statusBar);
        hintLabel->setWordWrap(true);
        {
            QPalette pal = hintLabel->palette();
            pal.setColor(QPalette::WindowText, QColor(96, 165, 250));
            hintLabel->setPalette(pal);
        }
        statusLayout->addWidget(statusLabel, 1);
        statusLayout->addWidget(hintLabel);
        layout->addWidget(statusBar);

        suggestionPopup = new QWidget(parent, Qt::Popup | Qt::FramelessWindowHint);
        suggestionPopup->setObjectName(QStringLiteral("ExpressionSuggestionPopup"));
        {
            QPalette pal = suggestionPopup->palette();
            pal.setColor(QPalette::Window, QColor(17, 24, 39));
            pal.setColor(QPalette::WindowText, QColor(229, 231, 235));
            suggestionPopup->setPalette(pal);
            suggestionPopup->setAutoFillBackground(true);
        }
        auto* popupLayout = new QVBoxLayout(suggestionPopup);
        popupLayout->setContentsMargins(1, 1, 1, 1);
        popupLayout->setSpacing(0);
        suggestionList = new QListWidget(suggestionPopup);
        suggestionList->setFrameShape(QFrame::NoFrame);
        suggestionList->setSelectionMode(QAbstractItemView::SingleSelection);
        suggestionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        suggestionList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        suggestionList->setAlternatingRowColors(true);
        suggestionList->setUniformItemSizes(true);
        suggestionList->setSpacing(1);
        suggestionList->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        {
            QPalette pal = suggestionList->palette();
            pal.setColor(QPalette::Base, QColor(17, 24, 39));
            pal.setColor(QPalette::AlternateBase, QColor(30, 41, 59));
            pal.setColor(QPalette::Text, QColor(229, 231, 235));
            pal.setColor(QPalette::Highlight, QColor(59, 130, 246));
            pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
            suggestionList->setPalette(pal);
        }
        popupLayout->addWidget(suggestionList);

        auto* actionBar = makeBar(QColor(20, 26, 33));
        auto* btnLayout = new QHBoxLayout(actionBar);
        btnLayout->setContentsMargins(12, 7, 12, 7);
        btnLayout->setSpacing(8);
        applyBtn = new QPushButton(QStringLiteral("Apply"), actionBar);
        revertBtn = new QPushButton(QStringLiteral("Revert"));
        removeBtn = new QPushButton(QStringLiteral("Remove Expression"));
        copyBtn = new QPushButton(QStringLiteral("Copy"));
        clearBtn = new QPushButton(QStringLiteral("Clear"));
        saveSnippetBtn = new QPushButton(QStringLiteral("Save Snippet"));
        loadSnippetBtn = new QPushButton(QStringLiteral("Load Snippet"));
        configureFlatAction(saveSnippetBtn);
        configureFlatAction(loadSnippetBtn);
        configureFlatAction(copyBtn);
        configureFlatAction(clearBtn);
        btnLayout->addWidget(saveSnippetBtn);
        btnLayout->addWidget(loadSnippetBtn);
        btnLayout->addWidget(copyBtn);
        btnLayout->addWidget(clearBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(revertBtn);
        btnLayout->addWidget(removeBtn);
        btnLayout->addWidget(applyBtn);
        applyBtn->setMinimumWidth(100);
        {
            QPalette palette = applyBtn->palette();
            palette.setColor(QPalette::Button, accentColor);
            palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
            applyBtn->setPalette(palette);
            applyBtn->setAutoFillBackground(true);
        }
        layout->addWidget(actionBar);

        validateTimer = new QTimer(parent);
        validateTimer->setSingleShot(true);
        validateTimer->setInterval(140);
        highlighter = std::make_unique<ExpressionSyntaxHighlighter>(expressionEdit->document());
        expressionEdit->installEventFilter(parent);
        suggestionPopup->installEventFilter(parent);
        suggestionList->installEventFilter(parent);
    }
};

ArtifactExpressionCopilotWidget::ArtifactExpressionCopilotWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    impl_->setupUi(this);

    connect(impl_->wiggleBtn, &QPushButton::clicked, this, [this]() {
        impl_->promptInput->setText(QStringLiteral("wiggle 3 times a second"));
    });

    connect(impl_->referenceList, &QListWidget::itemPressed, this,
            [this](QListWidgetItem* item) {
        if (!item || !impl_->referenceList) return;
        auto* mime = new QMimeData();
        mime->setData(QStringLiteral("application/x-artifact-expression-reference").toUtf8(),
                      item->data(Qt::UserRole).toString().toUtf8());
        auto* drag = new QDrag(impl_->referenceList);
        drag->setMimeData(mime);
        drag->exec(Qt::CopyAction);
    });
    connect(impl_->loopBtn, &QPushButton::clicked, this, [this]() {
        impl_->promptInput->setText(QStringLiteral("make it loop"));
    });
    connect(impl_->driftBtn, &QPushButton::clicked, this, [this]() {
        impl_->promptInput->setText(QStringLiteral("slow drifting motion"));
    });

    connect(impl_->generateBtn, &QPushButton::clicked, this, [this]() {
        const QString prompt = impl_->promptInput->text().trimmed();
        if (prompt.isEmpty()) {
            return;
        }

        impl_->expressionEdit->setPlainText(QStringLiteral("Generating..."));
        QTimer::singleShot(600, this, [this, prompt]() {
            QString result;
            if (prompt.contains(QStringLiteral("wiggle"), Qt::CaseInsensitive)) {
                result = QStringLiteral("wiggle(3, 50)");
            } else if (prompt.contains(QStringLiteral("loop"), Qt::CaseInsensitive)) {
                result = QStringLiteral("loopOut(\"cycle\")");
            } else {
                result = QStringLiteral("value + [10, 0]");
            }
            impl_->expressionEdit->setPlainText(result);
            impl_->validateExpression();
        });
    });

    connect(impl_->copyBtn, &QPushButton::clicked, this, [this]() {
        const QString text = impl_->expressionEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            return;
        }
        QApplication::clipboard()->setText(text);
        impl_->setStatus(QStringLiteral("Copied to clipboard"), QColor(74, 222, 128));
    });

    connect(impl_->applyBtn, &QPushButton::clicked, this, [this]() {
        impl_->validateExpression();
        const QString text = impl_->expressionEdit->toPlainText().trimmed();
        if (impl_->expressionValid && !text.isEmpty()) {
            if (impl_->applyHandler) {
                impl_->applyHandler(text);
            }
            impl_->originalExpression = text;
            QApplication::clipboard()->setText(text);
        }
    });

    connect(impl_->revertBtn, &QPushButton::clicked, this, [this]() {
        impl_->expressionEdit->setPlainText(impl_->originalExpression);
        impl_->expressionEdit->moveCursor(QTextCursor::End);
        impl_->validateExpression();
        impl_->setStatus(QStringLiteral("Reverted to the saved expression"),
                         QColor(148, 163, 184));
    });

    connect(impl_->removeBtn, &QPushButton::clicked, this, [this]() {
        if (impl_->applyHandler) {
            impl_->applyHandler(QString());
        }
        impl_->originalExpression.clear();
        impl_->expressionEdit->clear();
        impl_->setStatus(QStringLiteral("Expression removed"),
                         QColor(74, 222, 128));
        impl_->applyErrorSelection(-1, 0, QString());
    });

    connect(impl_->saveSnippetBtn, &QPushButton::clicked, this, [this]() {
        const QString expression = impl_->expressionEdit->toPlainText().trimmed();
        if (expression.isEmpty()) {
            return;
        }
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + QStringLiteral("/expression-snippets");
        QDir().mkpath(dir);
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save Expression Snippet"), dir,
            QStringLiteral("Expression Snippet (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            impl_->setStatus(QStringLiteral("Could not save snippet"), QColor(248, 113, 113));
            return;
        }
        QJsonObject object;
        object[QStringLiteral("kind")] = QStringLiteral("artifact.expression-snippet");
        object[QStringLiteral("expression")] = expression;
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        impl_->setStatus(QStringLiteral("Snippet saved"), QColor(74, 222, 128));
    });

    connect(impl_->loadSnippetBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + QStringLiteral("/expression-snippets");
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Load Expression Snippet"), dir,
            QStringLiteral("Expression Snippet (*.json)"));
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            impl_->setStatus(QStringLiteral("Could not load snippet"), QColor(248, 113, 113));
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        const QString expression = document.object().value(QStringLiteral("expression"))
                                       .toString().trimmed();
        if (expression.isEmpty()) {
            impl_->setStatus(QStringLiteral("Snippet is empty or invalid"), QColor(248, 113, 113));
            return;
        }
        impl_->expressionEdit->setPlainText(expression);
        impl_->expressionEdit->moveCursor(QTextCursor::End);
        impl_->validateExpression();
        impl_->setStatus(QStringLiteral("Snippet loaded"), QColor(74, 222, 128));
    });

    connect(impl_->clearBtn, &QPushButton::clicked, this, [this]() {
        impl_->promptInput->clear();
        impl_->expressionEdit->clear();
        impl_->setStatus(QStringLiteral("Cleared"), QColor(148, 163, 184));
        impl_->applyErrorSelection(-1, 0, QString());
    });

    connect(impl_->expressionEdit, &QTextEdit::textChanged, this, [this]() {
        if (impl_->validateTimer) {
            impl_->validateTimer->start();
        }
        if (impl_ && impl_->expressionEdit) {
            impl_->showSuggestions(impl_->currentCompletionPrefix());
        }
    });

    connect(impl_->validateTimer, &QTimer::timeout, this, [this]() {
        impl_->validateExpression();
    });

    impl_->validateExpression();
}

ArtifactExpressionCopilotWidget::~ArtifactExpressionCopilotWidget() {
    delete impl_;
}

QString ArtifactExpressionCopilotWidget::expressionText() const {
    return impl_ && impl_->expressionEdit ? impl_->expressionEdit->toPlainText() : QString();
}

void ArtifactExpressionCopilotWidget::setPreviewContext(
    const QString& compositionName,
    const QSize& compositionSize,
    const QStringList& layerNames,
    int currentLayerIndex,
    const QString& layerName,
    const QVariant& propertyValue,
    const QVariantMap& layerSnapshots,
    const QVariantList& compositionMarkers,
    double timeSeconds) {
    if (!impl_) {
        return;
    }

    impl_->previewCompositionName = compositionName;
    impl_->previewCompositionSize = compositionSize;
    impl_->previewLayerNames = layerNames;
    impl_->previewLayerIndex = currentLayerIndex;
    impl_->previewLayerName = layerName;
    impl_->previewValue = propertyValue;
    impl_->previewLayerSnapshots = layerSnapshots;
    impl_->previewCompositionMarkers = compositionMarkers;
    impl_->previewTimeSeconds = timeSeconds;
    if (impl_->timeContextLabel) {
        const int totalFrames = std::max(0, static_cast<int>(std::round(timeSeconds * 30.0)));
        const int frames = totalFrames % 30;
        const int totalSeconds = totalFrames / 30;
        const int seconds = totalSeconds % 60;
        const int totalMinutes = totalSeconds / 60;
        const int minutes = totalMinutes % 60;
        const int hours = totalMinutes / 60;
        const QString composition = compositionName.trimmed().isEmpty()
            ? QStringLiteral("Composition")
            : compositionName.trimmed();
        impl_->timeContextLabel->setText(
            QStringLiteral("%1  ·  %2:%3:%4:%5")
                .arg(composition)
                .arg(hours, 2, 10, QLatin1Char('0'))
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'))
                .arg(frames, 2, 10, QLatin1Char('0')));
    }
    impl_->validateExpression();
}

void ArtifactExpressionCopilotWidget::clearPreviewContext() {
    if (!impl_) {
        return;
    }

    impl_->previewCompositionName.clear();
    impl_->previewCompositionSize = QSize();
    impl_->previewLayerNames.clear();
    impl_->previewLayerIndex = -1;
    impl_->previewLayerName.clear();
    impl_->previewValue = QVariant();
    impl_->previewLayerSnapshots.clear();
    impl_->previewCompositionMarkers.clear();
    impl_->previewTimeSeconds = 0.0;
    impl_->validateExpression();
}

bool ArtifactExpressionCopilotWidget::eventFilter(QObject* watched, QEvent* event) {
    if (!impl_ || !event) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == impl_->suggestionPopup || watched == impl_->suggestionList) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (impl_->handleSuggestionKey(keyEvent)) {
                return true;
            }
        }
        if (watched == impl_->suggestionList &&
            (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick)) {
            if (impl_->acceptSuggestion()) {
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    if (watched != impl_->expressionEdit) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            if (impl_->completeCurrentWord()) {
                impl_->validateExpression();
                return true;
            }
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            impl_->hideSuggestions();
            return true;
        }
    }

    if (event->type() == QEvent::Drop) {
        auto* drop = static_cast<QDropEvent*>(event);
        const QString mimeType = QStringLiteral("application/x-artifact-expression-reference");
        if (drop->mimeData()->hasFormat(mimeType)) {
            const QString reference = QString::fromUtf8(drop->mimeData()->data(mimeType)).trimmed();
            if (!reference.isEmpty()) {
                QTextCursor cursor = impl_->expressionEdit->textCursor();
                cursor.insertText(reference);
                impl_->expressionEdit->setTextCursor(cursor);
                impl_->validateExpression();
                drop->acceptProposedAction();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ArtifactExpressionCopilotWidget::setExpressionText(const QString& expression) {
    if (!impl_ || !impl_->expressionEdit) {
        return;
    }
    impl_->originalExpression = expression.trimmed();
    impl_->expressionEdit->setPlainText(expression);
    impl_->expressionEdit->moveCursor(QTextCursor::End);
    impl_->validateExpression();
    impl_->showSuggestions(impl_->currentCompletionPrefix());
}

void ArtifactExpressionCopilotWidget::setInlineMode(const bool inlineMode) {
    if (!impl_ || impl_->inlineMode == inlineMode) {
        return;
    }
    impl_->inlineMode = inlineMode;
    if (inlineMode) {
        setWindowFlags(Qt::Widget);
        setAttribute(Qt::WA_DeleteOnClose, false);
        if (impl_->promptInput) {
            impl_->promptToolbar->hide();
        }
        if (impl_->referencePanel) {
            impl_->referencePanel->hide();
        }
        if (impl_->generateBtn) {
            impl_->generateBtn->hide();
        }
        if (impl_->wiggleBtn) {
            impl_->wiggleBtn->hide();
        }
        if (impl_->loopBtn) {
            impl_->loopBtn->hide();
        }
        if (impl_->driftBtn) {
            impl_->driftBtn->hide();
        }
        if (impl_->saveSnippetBtn) {
            impl_->saveSnippetBtn->hide();
        }
        if (impl_->loadSnippetBtn) {
            impl_->loadSnippetBtn->hide();
        }
        adjustSize();
    }
}

void ArtifactExpressionCopilotWidget::setApplyHandler(std::function<void(const QString& expression)> handler) {
    if (!impl_) {
        return;
    }
    impl_->applyHandler = std::move(handler);
}

void ArtifactExpressionCopilotWidget::setReferenceItems(
    const QStringList& layerNames, const QString& propertyPath) {
    if (!impl_ || !impl_->referenceList) return;
    impl_->referenceList->clear();
    const QString normalizedProperty = propertyPath.trimmed();
    if (!normalizedProperty.isEmpty()) {
        QString propertyName = normalizedProperty.section(QLatin1Char('.'), -1);
        if (!propertyName.isEmpty()) {
            propertyName[0] = propertyName.at(0).toUpper();
        }
        if (impl_->editorTabLabel) {
            impl_->editorTabLabel->setText(
                QStringLiteral("%1.expression").arg(propertyName));
        }
        if (impl_->propertyContextLabel) {
            const QString layerName = impl_->previewLayerName.trimmed().isEmpty()
                ? QStringLiteral("Layer")
                : impl_->previewLayerName.trimmed();
            impl_->propertyContextLabel->setText(
                QStringLiteral("%1 / %2").arg(layerName, propertyName));
        }
        auto* propertyItem = new QListWidgetItem(
            QStringLiteral("Property: value (%1)").arg(normalizedProperty),
            impl_->referenceList);
        propertyItem->setData(Qt::UserRole, QStringLiteral("value"));
    }
    for (const QString& layerName : layerNames) {
        auto* item = new QListWidgetItem(layerName, impl_->referenceList);
        item->setData(Qt::UserRole,
                      QStringLiteral("thisComp.layer(\"%1\")").arg(layerName));
    }
}

QSize ArtifactExpressionCopilotWidget::sizeHint() const {
    return { 960, 620 };
}

} // namespace Artifact
