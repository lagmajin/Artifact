module;
#include <utility>
#include <wobjectimpl.h>
#include <algorithm>
#include <QApplication>
#include <QColor>
#include <QClipboard>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
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
            QStringLiteral(R"(--[^\n]*$)"));

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
    QLineEdit* promptInput = nullptr;
    QTextEdit* expressionEdit = nullptr;
    QLabel* statusLabel = nullptr;
    QPushButton* generateBtn = nullptr;
    QPushButton* applyBtn = nullptr;
    QPushButton* copyBtn = nullptr;
    QPushButton* clearBtn = nullptr;
    QPushButton* wiggleBtn = nullptr;
    QPushButton* loopBtn = nullptr;
    QPushButton* driftBtn = nullptr;
    QTimer* validateTimer = nullptr;
    std::unique_ptr<ExpressionSyntaxHighlighter> highlighter;
    ArtifactCore::ExpressionParser parser;

    void setStatus(const QString& text, const QColor& color) {
        if (!statusLabel) {
            return;
        }
        statusLabel->setText(text);
        statusLabel->setStyleSheet(QStringLiteral("color: rgb(%1, %2, %3);")
                                       .arg(color.red())
                                       .arg(color.green())
                                       .arg(color.blue()));
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
            applyErrorSelection(-1, 0, text);
            return;
        }

        const auto ast = parser.parse(expr);
        if (ast) {
            setStatus(QStringLiteral("Syntax OK"), QColor(74, 222, 128));
            applyErrorSelection(-1, 0, text);
            return;
        }

        const QString error = QString::fromStdString(parser.getError());
        const int position = static_cast<int>(parser.getErrorPosition());
        const int length = static_cast<int>(std::max<std::size_t>(1, parser.getErrorLength()));
        const QString location = posToLineColumn(text, position);
        setStatus(QStringLiteral("%1 at %2").arg(error.isEmpty() ? QStringLiteral("Syntax error") : error, location),
                  QColor(248, 113, 113));
        applyErrorSelection(position, length, text);
    }

    void setupUi(QWidget* parent) {
        auto* layout = new QVBoxLayout(parent);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);

        auto* headerLayout = new QHBoxLayout();
        auto* iconLabel = new QLabel(QString::fromUtf8("fx"));
        auto* titleLabel = new QLabel(QStringLiteral("Expression Copilot"));
        titleLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
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
        statusLabel->setStyleSheet(QStringLiteral("color: rgb(148, 163, 184);"));
        layout->addWidget(statusLabel);

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
    });

    connect(impl_->validateTimer, &QTimer::timeout, this, [this]() {
        impl_->validateExpression();
    });

    impl_->validateExpression();
}

ArtifactExpressionCopilotWidget::~ArtifactExpressionCopilotWidget() {
    delete impl_;
}

void ArtifactExpressionCopilotWidget::setExpressionText(const QString& expression) {
    if (!impl_ || !impl_->expressionEdit) {
        return;
    }
    impl_->expressionEdit->setPlainText(expression);
    impl_->validateExpression();
}

QString ArtifactExpressionCopilotWidget::expressionText() const {
    return impl_ && impl_->expressionEdit ? impl_->expressionEdit->toPlainText() : QString();
}

QSize ArtifactExpressionCopilotWidget::sizeHint() const {
    return { 520, 360 };
}

} // namespace Artifact
