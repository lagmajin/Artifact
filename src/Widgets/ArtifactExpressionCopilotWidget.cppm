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
#include <QAbstractTextDocumentLayout>
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSize>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QStringList>
#include <QTextBlock>
#include <QTextFormat>
#include <QVariant>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
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
import UI.ShortcutBindings;
import Widgets.Utils.CSS;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Service.Project;

namespace Artifact {

namespace {

constexpr int kBaseFontPointSize = 11;
constexpr int kMinFontPointSize = 7;
constexpr int kMaxFontPointSize = 32;

// Renders the line number gutter and the current-line highlight. Implemented as
// a QTextEdit subclass rather than a separate widget so the existing
// ExtraSelection based error underline keeps working unchanged.
class ExpressionTextEdit final : public QTextEdit {
public:
    explicit ExpressionTextEdit(QWidget* parent) : QTextEdit(parent) {}

    void setLineNumbersVisible(bool visible) {
        if (showLineNumbers_ == visible) {
            return;
        }
        showLineNumbers_ = visible;
        updateGutterWidth();
    }

    bool lineNumbersVisible() const { return showLineNumbers_; }

    void setCurrentLineColor(const QColor& color) {
        if (currentLineColor_ == color) {
            return;
        }
        currentLineColor_ = color;
        if (showCurrentLine_) {
            refreshCurrentLineHighlight();
        }
    }

    // Rebuilds the current-line band. Call after the caret moves so the band
    // follows it.
    void refreshCurrentLineHighlight() { rebuildExtraSelections(); }

    // ExtraSelections are owned by the caller for error underlines; the
    // current-line band is composed by this class and kept separate so the
    // two never overwrite each other.
    void setErrorSelections(const QList<QTextEdit::ExtraSelection>& selections) {
        errorSelections_ = selections;
        rebuildExtraSelections();
    }

    int lineNumberAreaWidth() const {
        if (!showLineNumbers_) {
            return 0;
        }
        int digits = 1;
        int max = std::max(1, blockCount());
        while (max >= 10) {
            max /= 10;
            ++digits;
        }
        return 10 + fontMetrics().horizontalAdvance(QLatin1Char('0')) * digits;
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QTextEdit::resizeEvent(event);
        updateGutterWidth();
    }

    void paintEvent(QPaintEvent* event) override {
        QTextEdit::paintEvent(event);
        // The gutter is painted last so it sits above the text column. The
        // current-line band is not painted here: it is a FullWidthSelection
        // ExtraSelection so that it cannot be overwritten by the error
        // underline that shares the same list.
        if (showLineNumbers_) {
            paintLineNumbers(event);
        }
    }

private:
    void updateGutterWidth() {
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    }

    void refreshCurrentLineHighlight() {
        rebuildExtraSelections();
    }

    void rebuildExtraSelections() {
        QList<QTextEdit::ExtraSelection> combined;

        // The band is skipped while the user has a real selection, otherwise it
        // would tint the whole selected region.
        if (showCurrentLine_ && !document()->isEmpty() && !textCursor().hasSelection()) {
            QTextEdit::ExtraSelection band;
            band.cursor = textCursor();
            band.cursor.clearSelection();
            band.format.setBackground(currentLineColor_);
            band.format.setProperty(QTextFormat::FullWidthSelection, true);
            combined.push_back(band);
        }

        combined.append(errorSelections_);
        setExtraSelections(combined);
    }

    void paintLineNumbers(QPaintEvent* event) {
        QPainter painter(viewport());
        const int areaWidth = lineNumberAreaWidth();
        painter.fillRect(0, 0, areaWidth, height(),
                         palette().color(QPalette::Window));

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = static_cast<int>(blockBoundingGeometry(block)
                                       .translated(contentOffset())
                                       .top());
        int bottom = top +
                     static_cast<int>(blockBoundingRect(block).height());

        painter.setPen(palette().color(QPalette::WindowText));
        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                // The active line is drawn bolder so the caret position is
                // readable without following the cursor.
                const bool active = blockNumber == textCursor().blockNumber();
                QFont blockFont = painter.font();
                blockFont.setBold(active);
                painter.setFont(blockFont);
                painter.drawText(0, top, areaWidth - 6, fontMetrics().height(),
                                 Qt::AlignRight, QString::number(blockNumber + 1));
            }
            block = block.next();
            top = bottom;
            bottom = top + static_cast<int>(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

    bool showLineNumbers_ = true;
    bool showCurrentLine_ = true;
    QColor currentLineColor_ = QColor(255, 255, 255, 10);
    QList<QTextEdit::ExtraSelection> errorSelections_;
};


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
    ExpressionTextEdit* expressionEdit = nullptr;
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
    QPushButton* formatBtn = nullptr;
    QWidget* problemsStrip = nullptr;
    QListWidget* problemsList = nullptr;
    QPushButton* problemsToggleBtn = nullptr;
    QWidget* findBar = nullptr;
    QLineEdit* findFindEdit = nullptr;
    QLineEdit* findReplaceEdit = nullptr;
    QLabel* findCountLabel = nullptr;
    QPushButton* findPrevBtn = nullptr;
    QPushButton* findNextBtn = nullptr;
    QPushButton* findReplaceBtn = nullptr;
    QPushButton* findReplaceAllBtn = nullptr;
    QPushButton* findCloseBtn = nullptr;
    std::function<void(const QString& expression)> applyHandler;
    QTimer* validateTimer = nullptr;
    std::unique_ptr<ExpressionSyntaxHighlighter> highlighter;
    ArtifactCore::ExpressionParser parser;
    // Evaluator owning the signature table the completion list is built from.
    // Kept alive for the widget's lifetime so completion never has to
    // construct an evaluator per keystroke.
    std::unique_ptr<ArtifactCore::ExpressionEvaluator> signatureSource;
    // Cached signature-derived completion candidates, refreshed only when the
    // table changes rather than on every textChanged.
    QList<SuggestionCandidate> cachedFunctionSuggestions;
    int editorFontPointSize = kBaseFontPointSize;
    bool problemsStripVisible = false;
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

    // Variables exposed by the evaluator context. These are not built-in
    // functions, so they stay hand-listed; every registered function is
    // merged in from the signature table by functionSuggestions().
    static QList<SuggestionCandidate> contextVariableSuggestions()
    {
        return {
            { QStringLiteral("thisComp"), QStringLiteral("thisComp"), QStringLiteral("Composition context") },
            { QStringLiteral("thisLayer"), QStringLiteral("thisLayer"), QStringLiteral("Current layer context") },
            { QStringLiteral("time"), QStringLiteral("time"), QStringLiteral("Current time in seconds") },
            { QStringLiteral("value"), QStringLiteral("value"), QStringLiteral("Current property value") },
            { QStringLiteral("index"), QStringLiteral("index"), QStringLiteral("1-based layer index") },
            { QStringLiteral("keyframes"), QStringLiteral("keyframes"), QStringLiteral("Keyframe catalog of this property") }
        };
    }

    static QString exprValueTypeName(ArtifactCore::ExprValueType type)
    {
        switch (type) {
        case ArtifactCore::ExprValueType::Number: return QStringLiteral("number");
        case ArtifactCore::ExprValueType::Vec2: return QStringLiteral("vec2");
        case ArtifactCore::ExprValueType::Vec3: return QStringLiteral("vec3");
        case ArtifactCore::ExprValueType::Vec4: return QStringLiteral("vec4");
        case ArtifactCore::ExprValueType::Array: return QStringLiteral("array");
        case ArtifactCore::ExprValueType::String: return QStringLiteral("string");
        case ArtifactCore::ExprValueType::Object: return QStringLiteral("object");
        case ArtifactCore::ExprValueType::Null: break;
        }
        return QStringLiteral("any");
    }

    // Renders "name(a, b?) : number" for the completion list and hover.
    static QString describeFunction(const ArtifactCore::ExpressionFunctionInfo& info)
    {
        QStringList parts;
        parts.reserve(static_cast<int>(info.params.size()));
        for (const auto& param : info.params) {
            QString text = QString::fromStdString(param.name);
            text += QLatin1String(": ");
            text += exprValueTypeName(param.type);
            if (param.variadic) {
                text += QStringLiteral("...");
            } else if (param.optional) {
                text += QStringLiteral("?");
            }
            parts.push_back(text);
        }
        return QStringLiteral("%1(%2) : %3")
            .arg(QString::fromStdString(info.name), parts.join(QStringLiteral(", ")),
                 exprValueTypeName(info.returnType));
    }

    // Reads the live signature table from the evaluator. Constructing the
    // candidate list is done once and cached, so the per-keystroke path only
    // filters an already-built QList.
    void refreshFunctionSuggestions()
    {
        cachedFunctionSuggestions.clear();
        if (!signatureSource) {
            return;
        }
        for (const auto& info : signatureSource->allFunctionInfos()) {
            const QString signature = describeFunction(info);
            SuggestionCandidate candidate;
            candidate.display = signature;
            candidate.insert = QString::fromStdString(info.name);
            candidate.tooltip = info.docText.empty()
                ? signature
                : QString::fromStdString(info.docText) + QStringLiteral("\n\n") + signature;
            cachedFunctionSuggestions.push_back(candidate);
        }
    }

    // Context variables first so thisComp/thisLayer rank above the long
    // function list, then every registered function.
    QList<SuggestionCandidate> rootSuggestions() const
    {
        QList<SuggestionCandidate> candidates = contextVariableSuggestions();
        candidates.append(cachedFunctionSuggestions);
        return candidates;
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
        // Routed through the subclass so the current-line band survives: it owns
        // the ExtraSelection list and re-adds its own entry.
        expressionEdit->setErrorSelections(extras);
    }

    // Replaces the Problems strip contents. Only the first diagnostic is shown
    // because the evaluator stops at the first failing node; see the plan's
    // "multi-error collection" follow-up.
    void setProblem(const QString& message, int position, int length, bool isError)
    {
        if (!problemsList) {
            return;
        }
        problemsList->clear();
        if (message.isEmpty()) {
            if (problemsToggleBtn) {
                problemsToggleBtn->setText(QStringLiteral("Problems"));
                problemsToggleBtn->setEnabled(false);
            }
            return;
        }

        const QString location =
            position >= 0 ? posToLineColumn(expressionEdit ? expressionEdit->toPlainText()
                                                           : QString(),
                                          position)
                          : QStringLiteral("expression");
        auto* item = new QListWidgetItem(QStringLiteral("%1  (%2)").arg(message, location),
                                         problemsList);
        item->setData(Qt::UserRole + 1, position);
        item->setData(Qt::UserRole + 2, length);
        item->setForeground(QColor(isError ? 248 : 234, isError ? 113 : 179,
                                   isError ? 113 : 8));
        item->setToolTip(item->text());

        if (problemsToggleBtn) {
            problemsToggleBtn->setText(QStringLiteral("Problems (%1)").arg(problemsList->count()));
            problemsToggleBtn->setEnabled(true);
        }
    }

    void clearProblem() { setProblem(QString(), -1, 0, true); }

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
            clearProblem();
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
                const QString message = QString::fromStdString(evaluator.getError());
                // The evaluator now reports the span of the node that failed,
                // so a runtime error gets a squiggle exactly like a syntax one.
                const std::size_t rawPosition = evaluator.getErrorPosition();
                const int position = rawPosition == std::string::npos
                    ? -1
                    : static_cast<int>(rawPosition);
                const int length = static_cast<int>(
                    std::max<std::size_t>(1, evaluator.getErrorLength()));
                setStatus(
                    position >= 0
                        ? QStringLiteral("%1 at %2").arg(
                              message, posToLineColumn(text, position))
                        : QStringLiteral("Runtime error: %1").arg(message),
                    QColor(248, 113, 113));
                applyErrorSelection(position, length, text);
                setProblem(message, position, length, true);
            } else {
                expressionValid = true;
                setStatus(
                    QStringLiteral("Runtime OK: %1")
                        .arg(QString::fromStdString(
                                 ArtifactCore::toStdString(runtime.toString()))
                                 .left(96)),
                    QColor(74, 222, 128));
                applyErrorSelection(-1, 0, text);
                clearProblem();
            }
            setHint(currentHintText(text), QColor(96, 165, 250));
            if (applyBtn) {
                applyBtn->setEnabled(expressionValid);
            }
            return;
        }

        const QString error = QString::fromStdString(parser.getError());
        const int position = static_cast<int>(parser.getErrorPosition());
        const int length = static_cast<int>(std::max<std::size_t>(1, parser.getErrorLength()));
        const QString message = error.isEmpty() ? QStringLiteral("Syntax error") : error;
        setStatus(QStringLiteral("%1 at %2").arg(message, posToLineColumn(text, position)),
                  QColor(248, 113, 113));
        setHint(currentHintText(text), QColor(248, 180, 0));
        applyErrorSelection(position, length, text);
        setProblem(message, position, length, true);
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

    // --- Editor chrome ----------------------------------------------------

    void applyEditorFont(int pointSize)
    {
        editorFontPointSize = std::clamp(pointSize, kMinFontPointSize, kMaxFontPointSize);
        if (!expressionEdit) {
            return;
        }
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPointSize(editorFontPointSize);
        font.setStyleHint(QFont::Monospace);
        expressionEdit->setFont(font);
        // Tab width must follow the font or indentation stops lining up.
        expressionEdit->setTabStopDistance(
            expressionEdit->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4.0);
    }

    void adjustEditorFont(int delta)
    {
        applyEditorFont(editorFontPointSize + delta);
        setStatus(QStringLiteral("Font size: %1 pt").arg(editorFontPointSize),
                  QColor(148, 163, 184));
    }

    void toggleLineNumbers()
    {
        if (!expressionEdit) {
            return;
        }
        expressionEdit->setLineNumbersVisible(!expressionEdit->lineNumbersVisible());
        setStatus(expressionEdit->lineNumbersVisible()
                      ? QStringLiteral("Line numbers on")
                      : QStringLiteral("Line numbers off"),
                  QColor(148, 163, 184));
    }

    void refreshCurrentLineHighlight()
    {
        if (expressionEdit) {
            expressionEdit->refreshCurrentLineHighlight();
        }
    }

    void toggleWordWrap()
    {
        if (!expressionEdit) {
            return;
        }
        const bool wrap = expressionEdit->lineWrapMode() == QTextEdit::NoWrap;
        expressionEdit->setLineWrapMode(wrap ? QTextEdit::WidgetWidth
                                             : QTextEdit::NoWrap);
        setStatus(wrap ? QStringLiteral("Word wrap on") : QStringLiteral("Word wrap off"),
                  QColor(148, 163, 184));
    }

    void toggleProblemsStrip()
    {
        if (!problemsStrip) {
            return;
        }
        problemsStripVisible = !problemsStripVisible;
        problemsStrip->setVisible(problemsStripVisible);
    }

    // Moves the caret to a problem's offset and focuses the editor.
    void gotoProblemOffset(int position)
    {
        if (!expressionEdit || position < 0) {
            return;
        }
        QTextCursor cursor = expressionEdit->textCursor();
        cursor.setPosition(std::min(position, static_cast<int>(expressionEdit->toPlainText().size())));
        expressionEdit->setTextCursor(cursor);
        expressionEdit->setFocus();
    }

    // --- Find / replace ---------------------------------------------------
    // Standard in-document search. Replaces Ctrl+F, which QTextEdit would
    // otherwise handle with its own (unstyled, non-localized) find bar.

    void showFindBar(bool withReplace)
    {
        if (!findBar) {
            return;
        }
        if (findBar->isVisible() && findReplaceEdit->isVisible() == withReplace) {
            // Already open in the requested mode: just focus the field.
            (withReplace ? findReplaceEdit : findFindEdit)->setFocus();
            return;
        }
        findBar->setVisible(true);
        findReplaceEdit->setVisible(withReplace);
        // Seed the query from the current selection, matching every editor.
        const QTextCursor cursor = expressionEdit ? expressionEdit->textCursor() : QTextCursor();
        if (cursor.hasSelection() && findFindEdit->text().isEmpty()) {
            findFindEdit->setText(cursor.selectedText().simplified());
        }
        findFindEdit->setFocus();
        findFindEdit->selectAll();
    }

    void hideFindBar()
    {
        if (findBar) {
            findBar->hide();
        }
        if (expressionEdit) {
            expressionEdit->setFocus();
        }
    }

    // Searches forward from the caret, wrapping when asked. Returns the number
    // of matches so the field can show "3 of 12".
    int findNext(bool wrap)
    {
        if (!expressionEdit || findFindEdit->text().isEmpty()) {
            return 0;
        }
        const QString needle = findFindEdit->text();
        if (expressionEdit->find(needle, wrap ? QTextDocument::FindBackward
                                              : QTextDocument::FindForward)) {
            highlightAllMatches(needle, expressionEdit->textCursor().position());
        }
        return countMatches(needle);
    }

    void findPrevious()
    {
        if (!expressionEdit || findFindEdit->text().isEmpty()) {
            return;
        }
        const QString needle = findFindEdit->text();
        if (expressionEdit->find(needle, QTextDocument::FindBackward)) {
            highlightAllMatches(needle, expressionEdit->textCursor().position());
        }
    }

    // Highlights the current match. Qt's QTextEdit::find already moved the
    // caret, so the match is re-expressed as a selection to keep it distinct
    // from the red error underline that shares the same list.
    void highlightAllMatches(const QString& needle, int currentPosition)
    {
        if (!expressionEdit || needle.isEmpty()) {
            return;
        }
        QTextEdit::ExtraSelection active;
        active.cursor = expressionEdit->textCursor();
        active.format.setBackground(QColor(96, 165, 250, 90));
        active.format.setUnderlineColor(QColor(96, 165, 250));
        active.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        expressionEdit->setErrorSelections({active});

        setStatus(QStringLiteral("%1 of %2 matches")
                      .arg(findMatchIndex(needle, currentPosition))
                      .arg(countMatches(needle)),
                  QColor(148, 163, 184));
    }

    int countMatches(const QString& needle) const
    {
        if (!expressionEdit || needle.isEmpty()) {
            return 0;
        }
        return expressionEdit->document()->find(needle).size();
    }

    // 1-based ordinal of the match containing the given offset.
    int findMatchIndex(const QString& needle, int position) const
    {
        if (!expressionEdit || needle.isEmpty()) {
            return 0;
        }
        const QList<QTextEdit::ExtraSelection> found = expressionEdit->document()->find(needle);
        int ordinal = 0;
        for (const auto& selection : found) {
            ++ordinal;
            if (position >= selection.cursor.selectionStart() &&
                position <= selection.cursor.selectionEnd()) {
                return ordinal;
            }
        }
        return ordinal > 0 ? ordinal : 1;
    }

    // Replaces the current match, or inserts when the caret sits on a match.
    void replaceCurrentMatch()
    {
        if (!expressionEdit) {
            return;
        }
        const QString needle = findFindEdit->text();
        const QString replacement = findReplaceEdit->text();
        if (needle.isEmpty()) {
            return;
        }
        QTextCursor cursor = expressionEdit->textCursor();
        if (cursor.hasSelection() && cursor.selectedText() == needle) {
            cursor.insertText(replacement);
        } else {
            if (!expressionEdit->find(needle)) {
                setStatus(QStringLiteral("No match for \"%1\"").arg(needle),
                          QColor(248, 180, 0));
                return;
            }
            cursor = expressionEdit->textCursor();
            cursor.insertText(replacement);
        }
        expressionEdit->setTextCursor(cursor);
        validateExpression();
        findNext(true);
    }

    void replaceAllMatches()
    {
        if (!expressionEdit) {
            return;
        }
        const QString needle = findFindEdit->text();
        const QString replacement = findReplaceEdit->text();
        if (needle.isEmpty()) {
            return;
        }
        // Rewriting from a plain-text copy avoids the cursor loop entirely and
        // is immune to a replacement that itself contains the needle.
        const int count = countMatches(needle);
        const QString updated = expressionEdit->toPlainText().replace(needle, replacement);
        expressionEdit->setPlainText(updated);
        validateExpression();
        setStatus(QStringLiteral("Replaced %1 occurrence(s)").arg(count),
                  QColor(74, 222, 128));
    }

    // --- Formatter --------------------------------------------------------
    // Purely lexical cleanup: normalizes quotes, collapses redundant spaces and
    // trims line padding. It never renames identifiers, and the result is
    // re-parsed before it is accepted so a formatting pass can never turn a
    // valid expression into an invalid one.
    static QString formatExpression(const QString& source)
    {
        QString out;
        out.reserve(source.size());

        const QStringList lines = source.split(QLatin1Char('\n'));
        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            if (lineIndex > 0) {
                out += QLatin1Char('\n');
            }

            const QString line = lines.at(lineIndex);
            QString formatted;
            formatted.reserve(line.size());

            bool inSingleQuote = false;
            bool inDoubleQuote = false;
            for (int i = 0; i < line.size(); ++i) {
                const QChar ch = line.at(i);

                // Keep string contents byte-for-byte; only the quote character
                // is normalized, and never while already inside a string.
                if (ch == QLatin1Char('\'') && !inDoubleQuote) {
                    inSingleQuote = !inSingleQuote;
                    formatted += QLatin1Char('"');
                    continue;
                }
                if (ch == QLatin1Char('"') && !inSingleQuote) {
                    inDoubleQuote = !inDoubleQuote;
                    formatted += QLatin1Char('"');
                    continue;
                }
                if (inSingleQuote || inDoubleQuote) {
                    formatted += ch;
                    continue;
                }

                if (ch == QLatin1Char('#') || (ch == QLatin1Char('/') && i + 1 < line.size() &&
                                               line.at(i + 1) == QLatin1Char('/'))) {
                    // Comment: copy the rest verbatim, trimmed on the right.
                    formatted += line.mid(i).trimmed();
                    i = line.size();
                    continue;
                }

                if (ch.isSpace()) {
                    // Collapse runs of whitespace, and drop it entirely after an
                    // opening bracket or a comma.
                    if (!formatted.isEmpty() && formatted.back().isSpace()) {
                        continue;
                    }
                    const QChar previous = formatted.isEmpty() ? QChar() : formatted.back();
                    const bool previousIsPunctuation =
                        previous == QLatin1Char('(') || previous == QLatin1Char('[') ||
                        previous == QLatin1Char(',');
                    if (previousIsPunctuation) {
                        continue;
                    }
                    formatted += QLatin1Char(' ');
                    continue;
                }

                formatted += ch;
            }

            out += formatted.trimmed();
        }

        return out.trimmed();
    }

    // Returns false (and reports why) when formatting would break the parse.
    bool applyFormatter()
    {
        if (!expressionEdit) {
            return false;
        }
        const QString original = expressionEdit->toPlainText();
        if (original.trimmed().isEmpty()) {
            setStatus(QStringLiteral("Nothing to format"), QColor(148, 163, 184));
            return false;
        }

        const QString formatted = formatExpression(original);
        if (formatted == original.trimmed()) {
            setStatus(QStringLiteral("Already formatted"), QColor(148, 163, 184));
            return true;
        }

        // Roll back rather than commit a change that stops parsing. The user's
        // own text is restored verbatim before returning.
        ArtifactCore::ExpressionParser probe;
        if (!probe.parse(formatted.toStdString())) {
            expressionEdit->setPlainText(original);
            expressionEdit->moveCursor(QTextCursor::End);
            setStatus(QStringLiteral("Formatting skipped: result would not parse"),
                      QColor(248, 180, 0));
            return false;
        }

        const int cursorPosition = expressionEdit->textCursor().position();
        expressionEdit->setPlainText(formatted);
        QTextCursor restored = expressionEdit->textCursor();
        restored.setPosition(std::min(cursorPosition, static_cast<int>(formatted.size())));
        expressionEdit->setTextCursor(restored);
        validateExpression();
        setStatus(QStringLiteral("Expression formatted"), QColor(74, 222, 128));
        return true;
    }

    // --- Pick Whip --------------------------------------------------------
    // Finds the thisComp.layer("Name") call whose argument string contains the
    // given offset. Returns an empty string when no such call exists.
    static QString layerReferenceAt(const QString& text, int offset,
                                    int* callStart, int* callLength)
    {
        static const QRegularExpression callRx(
            QStringLiteral(R"(thisComp\s*\.\s*layer\s*\(\s*(["'])([^"']*)\1\s*\))"),
            QRegularExpression::CaseInsensitiveOption);

        auto it = callRx.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            // The name is captured twice: the quote and the name itself.
            const int nameStart = match.capturedStart(2);
            const int nameEnd = nameStart + match.capturedLength(2);
            if (offset >= nameStart && offset <= nameEnd) {
                if (callStart) {
                    *callStart = match.capturedStart();
                }
                if (callLength) {
                    *callLength = match.capturedLength();
                }
                return match.captured(2);
            }
        }
        return QString();
    }

    // Selects the layer named in the thisComp.layer("...") call under the
    // cursor. Silently does nothing when the name does not resolve, since the
    // expression may legitimately reference a layer from another composition.
    bool pickWhipLayerAt(int offset)
    {
        if (!expressionEdit) {
            return false;
        }
        int callStart = 0;
        int callLength = 0;
        const QString layerName =
            layerReferenceAt(expressionEdit->toPlainText(), offset, &callStart, &callLength);
        if (layerName.isEmpty()) {
            return false;
        }

        auto* service = ArtifactProjectService::instance();
        if (!service) {
            return false;
        }
        // Scoped to the current composition on purpose: selectLayer rejects
        // ids from other compositions, so there is nothing to switch to.
        const auto composition = service->currentComposition().lock();
        if (!composition) {
            setStatus(QStringLiteral("No composition to pick from"), QColor(248, 180, 0));
            return false;
        }

        for (const auto& layer : composition->allLayer()) {
            if (layer && layer->layerName() == layerName) {
                service->selectLayer(layer->id());
                setStatus(QStringLiteral("Picked layer: %1").arg(layerName),
                          QColor(96, 165, 250));
                return true;
            }
        }

        setStatus(QStringLiteral("Layer not found: %1").arg(layerName),
                  QColor(248, 180, 0));
        return false;
    }

    // Signature help for the function call surrounding the caret, shown in the
    // hint label. Returns true when a signature was found.
    bool showSignatureHelpAt(int offset)
    {
        if (!expressionEdit || !signatureSource) {
            return false;
        }
        const QString text = expressionEdit->toPlainText();
        if (offset <= 0 || offset > text.size()) {
            return false;
        }

        // Walk back over an identifier to the character before the '('.
        int nameEnd = offset;
        while (nameEnd > 0) {
            const QChar ch = text.at(nameEnd - 1);
            if (!ch.isLetterOrNumber() && ch != QLatin1Char('_')) {
                break;
            }
            --nameEnd;
        }
        if (nameEnd == offset) {
            return false;
        }

        int openParen = nameEnd;
        while (openParen < text.size() && text.at(openParen).isSpace()) {
            ++openParen;
        }
        if (openParen >= text.size() || text.at(openParen) != QLatin1Char('(')) {
            return false;
        }

        const QString name = text.mid(nameEnd, offset - nameEnd);
        const ArtifactCore::ExpressionFunctionInfo* info =
            signatureSource->functionInfo(name.toStdString());
        if (!info) {
            return false;
        }

        // Count the commas before the caret to highlight the active argument.
        int argumentIndex = 0;
        for (int i = openParen + 1; i < offset && i < text.size(); ++i) {
            if (text.at(i) == QLatin1Char(',')) {
                ++argumentIndex;
            }
        }

        const QString signature = describeFunction(*info);
        setHint(QStringLiteral("%1   —   argument %2")
                    .arg(signature)
                    .arg(argumentIndex + 1),
                QColor(96, 165, 250));
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

        expressionEdit = new ExpressionTextEdit(workspaceSplitter);
        expressionEdit->setPlaceholderText(QStringLiteral("Enter an expression here..."));
        expressionEdit->setLineWrapMode(QTextEdit::NoWrap);
        expressionEdit->setAcceptDrops(true);
        expressionEdit->setFrameShape(QFrame::NoFrame);
        // A faint band marks the caret line; the gutter column reuses the panel
        // color so it reads as chrome rather than as editable text.
        expressionEdit->setCurrentLineColor(QColor(255, 255, 255, 10));
        QPalette editorPalette = rootPalette;
        editorPalette.setColor(QPalette::Base, editorColor);
        editorPalette.setColor(QPalette::Text, textColor);
        editorPalette.setColor(QPalette::Window, panelColor);
        expressionEdit->setPalette(editorPalette);
        applyEditorFont(kBaseFontPointSize);

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

        // Problems strip. Hidden until the first diagnostic appears, so the
        // default editor footprint matches what it was before.
        problemsStrip = makeBar(QColor(28, 20, 22));
        problemsStrip->setParent(parent);
        auto* problemsLayout = new QVBoxLayout(problemsStrip);
        problemsLayout->setContentsMargins(0, 0, 0, 0);
        problemsLayout->setSpacing(0);
        problemsList = new QListWidget(problemsStrip);
        problemsList->setFrameShape(QFrame::NoFrame);
        problemsList->setSelectionMode(QAbstractItemView::SingleSelection);
        problemsList->setMaximumHeight(64);
        problemsList->setToolTip(QStringLiteral("Click a problem to jump to it"));
        QPalette problemsPalette = rootPalette;
        problemsPalette.setColor(QPalette::Base, QColor(28, 20, 22));
        problemsPalette.setColor(QPalette::Text, textColor);
        problemsList->setPalette(problemsPalette);
        problemsLayout->addWidget(problemsList);
        problemsStrip->hide();
        layout->addWidget(problemsStrip);

        // Find / replace bar. Hidden until Ctrl+F or Ctrl+H, so it costs no
        // vertical space in the default layout.
        findBar = makeBar(QColor(24, 30, 38));
        findBar->setParent(parent);
        auto* findLayout = new QVBoxLayout(findBar);
        findLayout->setContentsMargins(8, 5, 8, 5);
        findLayout->setSpacing(5);

        auto* findRow = new QHBoxLayout();
        findRow->setContentsMargins(0, 0, 0, 0);
        findRow->setSpacing(6);
        findFindEdit = new QLineEdit(findBar);
        findFindEdit->setPlaceholderText(QStringLiteral("Find"));
        findFindEdit->setClearButtonEnabled(true);
        findCountLabel = new QLabel(QStringLiteral("0 of 0"), findBar);
        findCountLabel->setMinimumWidth(64);
        setMuted(findCountLabel);
        findPrevBtn = new QPushButton(QStringLiteral("Previous"), findBar);
        findNextBtn = new QPushButton(QStringLiteral("Next"), findBar);
        findCloseBtn = new QPushButton(QStringLiteral("Close"), findBar);
        configureFlatAction(findPrevBtn);
        configureFlatAction(findNextBtn);
        configureFlatAction(findCloseBtn);
        findRow->addWidget(findFindEdit, 1);
        findRow->addWidget(findCountLabel);
        findRow->addWidget(findPrevBtn);
        findRow->addWidget(findNextBtn);
        findRow->addWidget(findCloseBtn);
        findLayout->addLayout(findRow);

        auto* replaceRow = new QHBoxLayout();
        replaceRow->setContentsMargins(0, 0, 0, 0);
        replaceRow->setSpacing(6);
        findReplaceEdit = new QLineEdit(findBar);
        findReplaceEdit->setPlaceholderText(QStringLiteral("Replace with"));
        findReplaceEdit->setClearButtonEnabled(true);
        findReplaceBtn = new QPushButton(QStringLiteral("Replace"), findBar);
        findReplaceAllBtn = new QPushButton(QStringLiteral("Replace All"), findBar);
        configureFlatAction(findReplaceBtn);
        configureFlatAction(findReplaceAllBtn);
        replaceRow->addWidget(findReplaceEdit, 1);
        replaceRow->addWidget(findReplaceBtn);
        replaceRow->addWidget(findReplaceAllBtn);
        findLayout->addLayout(replaceRow);

        layout->addWidget(findBar);
        findBar->hide();

        auto* statusBar = makeBar(QColor(18, 24, 31));
        auto* statusLayout = new QHBoxLayout(statusBar);
        statusLayout->setContentsMargins(12, 5, 12, 5);
        statusLayout->setSpacing(12);
        problemsToggleBtn = new QPushButton(QStringLiteral("Problems"), statusBar);
        configureFlatAction(problemsToggleBtn);
        problemsToggleBtn->setEnabled(false);
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
        statusLayout->addWidget(problemsToggleBtn);
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
        formatBtn = new QPushButton(QStringLiteral("Format"));
        configureFlatAction(formatBtn);
        configureFlatAction(saveSnippetBtn);
        configureFlatAction(loadSnippetBtn);
        configureFlatAction(copyBtn);
        configureFlatAction(clearBtn);
        btnLayout->addWidget(saveSnippetBtn);
        btnLayout->addWidget(loadSnippetBtn);
        btnLayout->addWidget(copyBtn);
        btnLayout->addWidget(clearBtn);
        btnLayout->addWidget(formatBtn);
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
        // One evaluator lives for the widget's lifetime; its constructor
        // registers the standard functions together with their signatures, so
        // the completion list never has to build an evaluator per keystroke.
        signatureSource = std::make_unique<ArtifactCore::ExpressionEvaluator>();
        refreshFunctionSuggestions();
        expressionEdit->installEventFilter(parent);
        findFindEdit->installEventFilter(parent);
        findReplaceEdit->installEventFilter(parent);
        suggestionPopup->installEventFilter(parent);
        suggestionList->installEventFilter(parent);
    }
};

ArtifactExpressionCopilotWidget::ArtifactExpressionCopilotWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
    impl_->setupUi(this);

    connect(impl_->formatBtn, &QPushButton::clicked, this, [this]() {
        impl_->applyFormatter();
    });

    // --- Find bar wiring. Every control also works via the event filter, so
    // this only adds the mouse path for the same actions.
    const auto updateFindCount = [this]() {
        const int total = impl_->countMatches(impl_->findFindEdit->text());
        impl_->findCountLabel->setText(total == 0
                                            ? QStringLiteral("0 of 0")
                                            : QStringLiteral("%1 of %2").arg(impl_->findMatchIndex(
                                                  impl_->findFindEdit->text(),
                                                  impl_->expressionEdit->textCursor().position())).arg(total));
    };

    connect(impl_->findFindEdit, &QLineEdit::textChanged, this, [this, updateFindCount](const QString&) {
        if (impl_->findFindEdit->text().isEmpty()) {
            impl_->findCountLabel->setText(QStringLiteral("0 of 0"));
            // Clearing the query must also drop the match highlight.
            impl_->applyErrorSelection(-1, 0, impl_->expressionEdit->toPlainText());
            return;
        }
        impl_->findNext(true);
        updateFindCount();
    });

    connect(impl_->findFindEdit, &QLineEdit::returnPressed, this, [this, updateFindCount]() {
        impl_->findNext(true);
        updateFindCount();
    });

    connect(impl_->findNextBtn, &QPushButton::clicked, this, [this, updateFindCount]() {
        impl_->findNext(false);
        updateFindCount();
    });

    connect(impl_->findPrevBtn, &QPushButton::clicked, this, [this, updateFindCount]() {
        impl_->findPrevious();
        updateFindCount();
    });

    connect(impl_->findReplaceBtn, &QPushButton::clicked, this, [this]() {
        impl_->replaceCurrentMatch();
    });

    connect(impl_->findReplaceAllBtn, &QPushButton::clicked, this, [this]() {
        impl_->replaceAllMatches();
    });

    connect(impl_->findCloseBtn, &QPushButton::clicked, this, [this]() {
        // Drop the match highlight before handing focus back to the editor.
        impl_->applyErrorSelection(-1, 0, impl_->expressionEdit->toPlainText());
        impl_->hideFindBar();
    });

    // Enter in the replace field performs the replacement, matching every
    // editor's expected flow.
    connect(impl_->findReplaceEdit, &QLineEdit::returnPressed, this, [this]() {
        impl_->replaceCurrentMatch();
    });

    connect(impl_->problemsToggleBtn, &QPushButton::clicked, this, [this]() {
        // First press reveals the strip and selects the current problem, so the
        // user lands on the diagnostic instead of just seeing it appear.
        const bool wasVisible = impl_->problemsStripVisible;
        impl_->toggleProblemsStrip();
        if (!wasVisible && impl_->problemsList && impl_->problemsList->count() > 0) {
            impl_->problemsList->setCurrentRow(0);
        }
    });

    connect(impl_->problemsList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        impl_->gotoProblemOffset(item->data(Qt::UserRole + 1).toInt());
    });

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

    // Signature help follows the caret as it moves through a call's arguments,
    // and the current-line band has to be rebuilt whenever the block changes.
    connect(impl_->expressionEdit, &QTextEdit::cursorPositionChanged, this, [this]() {
        impl_->refreshCurrentLineHighlight();
        impl_->showSignatureHelpAt(impl_->expressionEdit->textCursor().position());
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

    if (watched == impl_->findFindEdit || watched == impl_->findReplaceEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            const auto& bindings = ArtifactCore::ShortcutBindings::instance();
            // Enter advances the match instead of inserting a newline, and
            // Escape returns to the editor. Both are resolved through the
            // binding system rather than hard-coded.
            if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFindNext)) {
                impl_->findNext(false);
                event->accept();
                return true;
            }
            if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFindPrevious)) {
                impl_->findPrevious();
                event->accept();
                return true;
            }
            if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFindClose)) {
                impl_->applyErrorSelection(
                    -1, 0, impl_->expressionEdit->toPlainText());
                impl_->hideFindBar();
                event->accept();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    if (watched != impl_->expressionEdit) {
        return QWidget::eventFilter(watched, event);
    }

    // Ctrl+click on a thisComp.layer("Name") reference selects that layer.
    // A plain click keeps the normal text-selection behavior.
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton &&
            (mouseEvent->modifiers() & Qt::ControlModifier)) {
            const int offset =
                impl_->expressionEdit->cursorForPosition(mouseEvent->position().toPoint())
                    .position();
            if (impl_->pickWhipLayerAt(offset)) {
                mouseEvent->accept();
                return true;
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);

        // Panel.ExpressionEditor owns these bindings. Resolved here, on the
        // focused editor, rather than as application-wide QAction shortcuts so
        // the timeline and project panels keep their same-key commands.
        const auto& bindings = ArtifactCore::ShortcutBindings::instance();
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionComplete)) {
            if (impl_->completeCurrentWord()) {
                impl_->validateExpression();
                event->accept();
                return true;
            }
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFontSizeIncrease)) {
            impl_->adjustEditorFont(1);
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFontSizeDecrease)) {
            impl_->adjustEditorFont(-1);
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFontSizeReset)) {
            impl_->applyEditorFont(kBaseFontPointSize);
            impl_->setStatus(QStringLiteral("Font size reset"), QColor(148, 163, 184));
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFind)) {
            impl_->showFindBar(false);
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionReplace)) {
            impl_->showFindBar(true);
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFindNext) &&
            impl_->findBar->isVisible()) {
            impl_->findNext(false);
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFindPrevious) &&
            impl_->findBar->isVisible()) {
            impl_->findPrevious();
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionFindClose) &&
            impl_->findBar->isVisible()) {
            // Drop the match highlight so the editor shows no stale underline.
            impl_->applyErrorSelection(-1, 0, impl_->expressionEdit->toPlainText());
            impl_->hideFindBar();
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionToggleLineNumbers)) {
            impl_->toggleLineNumbers();
            event->accept();
            return true;
        }
        if (bindings.matches(keyEvent, ArtifactCore::ShortcutId::ExpressionToggleWordWrap)) {
            impl_->toggleWordWrap();
            event->accept();
            return true;
        }
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
        // The strip is hidden by default and only revealed by its toggle, so
        // it contributes no height to the inline editor either way.
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
