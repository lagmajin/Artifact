module;
#include <utility>
#include <wobjectimpl.h>
#include <algorithm>
#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QClipboard>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QFrame>
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QStringList>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

module Artifact.Widgets.ExpressionCopilotWidget;

import Script.Expression.Parser;

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
    QPushButton* generateBtn = nullptr;
    QPushButton* applyBtn = nullptr;
    QPushButton* copyBtn = nullptr;
    QPushButton* clearBtn = nullptr;
    QPushButton* wiggleBtn = nullptr;
    QPushButton* loopBtn = nullptr;
    QPushButton* driftBtn = nullptr;
    std::function<void(const QString& expression)> applyHandler;
    QTimer* validateTimer = nullptr;
    std::unique_ptr<ExpressionSyntaxHighlighter> highlighter;
    ArtifactCore::ExpressionParser parser;

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
        const std::string expr = text.toStdString();
        if (expr.empty()) {
            setStatus(QStringLiteral("Expression is empty"), QColor(148, 163, 184));
            setHint(QStringLiteral("Try: thisComp.width, thisLayer.name, or wiggle(3, 50)"), QColor(148, 163, 184));
            applyErrorSelection(-1, 0, text);
            return;
        }

        const auto ast = parser.parse(expr);
        if (ast) {
            setStatus(QStringLiteral("Syntax OK"), QColor(74, 222, 128));
            setHint(currentHintText(text), QColor(96, 165, 250));
            applyErrorSelection(-1, 0, text);
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
        auto* layout = new QVBoxLayout(parent);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        auto* headerLayout = new QHBoxLayout();
        auto* iconLabel = new QLabel(QString::fromUtf8("fx"));
        auto* titleLabel = new QLabel(QStringLiteral("Expression Copilot"));
        {
            QFont font = titleLabel->font();
            font.setBold(true);
            font.setPointSize(14);
            titleLabel->setFont(font);
        }
        headerLayout->addWidget(iconLabel);
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        layout->addLayout(headerLayout);

        promptInput = new QLineEdit();
        promptInput->setPlaceholderText(QStringLiteral("Describe the motion you want... (e.g. wiggle 3 times a second)"));
        layout->addWidget(promptInput);

        auto* examplesLayout = new QHBoxLayout();
        wiggleBtn = new QPushButton(QStringLiteral("Wiggle"));
        loopBtn = new QPushButton(QStringLiteral("Loop"));
        driftBtn = new QPushButton(QStringLiteral("Drift"));
        examplesLayout->addWidget(wiggleBtn);
        examplesLayout->addWidget(loopBtn);
        examplesLayout->addWidget(driftBtn);
        examplesLayout->addStretch();
        layout->addLayout(examplesLayout);

        expressionEdit = new QTextEdit();
        expressionEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        expressionEdit->setPlaceholderText(QStringLiteral("Enter an expression here..."));
        expressionEdit->setLineWrapMode(QTextEdit::NoWrap);
        expressionEdit->setTabStopDistance(expressionEdit->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4.0);
        layout->addWidget(expressionEdit);

        statusLabel = new QLabel(QStringLiteral("Ready"));
        statusLabel->setWordWrap(true);
        {
            QPalette pal = statusLabel->palette();
            pal.setColor(QPalette::WindowText, QColor(148, 163, 184));
            statusLabel->setPalette(pal);
        }
        layout->addWidget(statusLabel);

        hintLabel = new QLabel(QStringLiteral("Hints: thisComp, thisLayer, linear, ease, wiggle"));
        hintLabel->setWordWrap(true);
        {
            QPalette pal = hintLabel->palette();
            pal.setColor(QPalette::WindowText, QColor(96, 165, 250));
            hintLabel->setPalette(pal);
        }
        layout->addWidget(hintLabel);

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

        auto* btnLayout = new QHBoxLayout();
        generateBtn = new QPushButton(QStringLiteral("Generate"));
        applyBtn = new QPushButton(QStringLiteral("Apply"));
        copyBtn = new QPushButton(QStringLiteral("Copy"));
        clearBtn = new QPushButton(QStringLiteral("Clear"));
        btnLayout->addStretch();
        btnLayout->addWidget(generateBtn);
        btnLayout->addWidget(applyBtn);
        btnLayout->addWidget(copyBtn);
        btnLayout->addWidget(clearBtn);
        layout->addLayout(btnLayout);

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
        if (!text.isEmpty()) {
            if (impl_->applyHandler) {
                impl_->applyHandler(text);
            }
            QApplication::clipboard()->setText(text);
        }
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

    return QWidget::eventFilter(watched, event);
}

void ArtifactExpressionCopilotWidget::setExpressionText(const QString& expression) {
    if (!impl_ || !impl_->expressionEdit) {
        return;
    }
    impl_->expressionEdit->setPlainText(expression);
    impl_->expressionEdit->moveCursor(QTextCursor::End);
    impl_->validateExpression();
    impl_->showSuggestions(impl_->currentCompletionPrefix());
}

void ArtifactExpressionCopilotWidget::setApplyHandler(std::function<void(const QString& expression)> handler) {
    if (!impl_) {
        return;
    }
    impl_->applyHandler = std::move(handler);
}

QSize ArtifactExpressionCopilotWidget::sizeHint() const {
    return { 520, 360 };
}

} // namespace Artifact
