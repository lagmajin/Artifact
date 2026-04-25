module;
#include <wobjectimpl.h>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QSizePolicy>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QSlider>
#include <QFrame>
#include <QFontMetrics>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QCheckBox>
#include <QPainter>
#include <QPaintEvent>
#include <QStyle>
#include <QIcon>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QDebug>
#include <QKeySequence>
#include <QStringList>

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

module Artifact.Widgets.PlaybackControlWidget;

import Utils;
import Icon.SvgToIcon;
import Frame.Range;
import Playback.State;
import Event.Bus;
import Artifact.Event.Types;
import Artifact.Composition.InOutPoints;
import Widgets.StyleSurface;
import Widgets.Utils.CSS;
import Artifact.Application.Manager;
import Artifact.Service.Playback;
import Artifact.Service.ActiveContext;
import Artifact.Composition.PlaybackController;

namespace {
class PlaybackTimecodeFrame final : public QFrame
{
public:
    explicit PlaybackTimecodeFrame(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setFrameShape(QFrame::StyledPanel);
        setFrameShadow(QFrame::Plain);
        setLineWidth(1);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setAttribute(Qt::WA_StyledBackground, true);
        setAutoFillBackground(false);
    }

    void setCurrentFrameText(const QString& text)
    {
        currentText_ = text;
        updateGeometry();
        update();
    }

    void setRangeTexts(const QString& inText, const QString& outText)
    {
        inText_ = inText;
        outText_ = outText;
        updateGeometry();
        update();
    }

    QSize sizeHint() const override
    {
        QFont currentFont = font();
        currentFont.setPointSize(13);
        currentFont.setWeight(QFont::DemiBold);
        QFont labelFont = font();
        labelFont.setPointSize(10);
        labelFont.setWeight(QFont::DemiBold);
        QFont valueFont = font();
        valueFont.setPointSize(11);
        valueFont.setWeight(QFont::DemiBold);

        const QFontMetrics currentMetrics(currentFont);
        const QFontMetrics labelMetrics(labelFont);
        const QFontMetrics valueMetrics(valueFont);
        const QString currentLine = currentText_.isEmpty()
            ? QStringLiteral("F0")
            : currentText_;
        const QString inLabel = QStringLiteral("In");
        const QString outLabel = QStringLiteral("Out");
        const QString inValue = inText_.isEmpty() ? QStringLiteral("--:--:--:--") : inText_;
        const QString outValue = outText_.isEmpty() ? QStringLiteral("--:--:--:--") : outText_;

        int width = currentMetrics.horizontalAdvance(currentLine);
        const int leftColumnWidth = std::max(labelMetrics.horizontalAdvance(inLabel),
                                             valueMetrics.horizontalAdvance(inValue));
        const int rightColumnWidth = std::max(labelMetrics.horizontalAdvance(outLabel),
                                              valueMetrics.horizontalAdvance(outValue));
        const int lowerWidth = leftColumnWidth + rightColumnWidth + 22;
        width = std::max(width, lowerWidth);

        const int height = currentMetrics.lineSpacing() +
                           labelMetrics.lineSpacing() + valueMetrics.lineSpacing() + 14;
        return QSize(width + 18, height + 12);
    }

    QSize minimumSizeHint() const override
    {
        return sizeHint();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        const auto& theme = ArtifactCore::currentDCCTheme();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(theme.secondaryBackgroundColor));
        painter.setPen(QPen(QColor(theme.borderColor), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        const QRect content = rect().adjusted(8, 6, -8, -6);
        QFont currentFont = font();
        currentFont.setPointSize(13);
        currentFont.setWeight(QFont::DemiBold);
        QFont labelFont = font();
        labelFont.setPointSize(10);
        labelFont.setWeight(QFont::DemiBold);
        QFont valueFont = font();
        valueFont.setPointSize(11);
        valueFont.setWeight(QFont::DemiBold);

        const QColor goldText(QColor(theme.accentColor));
        const QColor mutedGoldText(QColor(theme.textColor).darker(120));
        const QString currentLine = currentText_.isEmpty()
            ? QStringLiteral("F0")
            : currentText_;
        const QString inLabel = QStringLiteral("In");
        const QString outLabel = QStringLiteral("Out");
        const QString inValue = inText_.isEmpty() ? QStringLiteral("--:--:--:--") : inText_;
        const QString outValue = outText_.isEmpty() ? QStringLiteral("--:--:--:--") : outText_;

        painter.setFont(currentFont);
        painter.setPen(goldText);
        int y = content.top() + QFontMetrics(currentFont).ascent();
        painter.drawText(content.left(), y, currentLine);

        y += QFontMetrics(currentFont).descent() + 8;
        const int lowerTop = y;
        const int columnGap = 16;
        const int columnWidth = std::max(
            1, (content.width() - columnGap) / 2);
        const QRect leftRect(content.left(), lowerTop, columnWidth, content.height() - (lowerTop - content.top()));
        const QRect rightRect(content.left() + columnWidth + columnGap, lowerTop, columnWidth, content.height() - (lowerTop - content.top()));

        painter.setFont(labelFont);
        painter.setPen(mutedGoldText);
        const int labelHeight = QFontMetrics(labelFont).lineSpacing();
        const int valueHeight = QFontMetrics(valueFont).lineSpacing();

        painter.drawText(leftRect.adjusted(0, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop, inLabel);
        painter.setFont(valueFont);
        painter.setPen(goldText);
        painter.drawText(leftRect.adjusted(0, labelHeight - 1, 0, 0), Qt::AlignLeft | Qt::AlignTop, inValue);

        painter.setFont(labelFont);
        painter.setPen(mutedGoldText);
        painter.drawText(rightRect.adjusted(0, 0, 0, 0), Qt::AlignLeft | Qt::AlignTop, outLabel);
        painter.setFont(valueFont);
        painter.setPen(goldText);
        painter.drawText(rightRect.adjusted(0, labelHeight - 1, 0, 0), Qt::AlignLeft | Qt::AlignTop, outValue);
    }

private:
    QString currentText_;
    QString inText_;
    QString outText_;
};

void applyThemeTextPalette(QWidget* widget, const QColor& color, int shade = 100)
{
    if (!widget) {
        return;
    }
    QPalette pal = widget->palette();
    pal.setColor(QPalette::WindowText, color.darker(shade));
    pal.setColor(QPalette::Text, color.darker(shade));
    widget->setPalette(pal);
}

QIcon loadIconWithFallback(const QString& fileName)
{
  const QString resourcePath = ArtifactCore::resolveIconResourcePath(fileName);
  {
    const QIcon icon = ArtifactCore::svgToQIcon(resourcePath, QSize(24, 24));
    if (!icon.isNull()) {
      return icon;
    }
  }

  const QString filePath = ArtifactCore::resolveIconPath(fileName);
  {
    const QIcon fileIcon = ArtifactCore::svgToQIcon(filePath, QSize(24, 24));
    if (!fileIcon.isNull()) {
      return fileIcon;
    }
  }

  qWarning().noquote() << "[ArtifactPlaybackControlWidget] icon load failed:"
                       << "resource=" << resourcePath
                       << "exists=" << QFileInfo::exists(resourcePath)
                       << "file=" << filePath
                       << "exists=" << QFileInfo::exists(filePath);
  return QIcon();
}

QIcon loadIconWithFallback(const QStringList& fileNames)
{
  for (const auto& fileName : fileNames) {
    const QIcon icon = loadIconWithFallback(fileName);
    if (!icon.isNull()) {
      return icon;
    }
  }
  return QIcon();
}

QString formatFrameCount(qint64 frame)
{
    return QStringLiteral("F%1").arg(frame);
}

QString formatTimecode(qint64 frame, float fps)
{
    const int safeFps = std::max(1, static_cast<int>(std::lround(std::max(0.001f, fps))));
    const qint64 totalSeconds = frame / safeFps;
    const int ff = static_cast<int>(frame % safeFps);
    const int ss = static_cast<int>(totalSeconds % 60);
    const int mm = static_cast<int>((totalSeconds / 60) % 60);
    const int hh = static_cast<int>(totalSeconds / 3600);
    return QStringLiteral("%1:%2:%3:%4")
        .arg(hh, 2, 10, QChar('0'))
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));
}

QString formatSpeedLabel(float speed)
{
    return QStringLiteral("x%1").arg(speed >= 0.0f ? speed : -speed, 0, 'f', speed >= 1.0f ? 1 : 2);
}
}

namespace Artifact
{
using namespace ArtifactCore;

using PlaybackState = ::ArtifactCore::PlaybackState;

// ============================================================================
// ArtifactPlaybackControlWidget::Impl
// ============================================================================

class ArtifactPlaybackControlWidget::Impl {
public:
    ArtifactPlaybackControlWidget* owner_;
    
    // Buttons
    QToolButton* playButton_ = nullptr;
    QToolButton* pauseButton_ = nullptr;
    QToolButton* stopButton_ = nullptr;
    QToolButton* stepBackwardButton_ = nullptr;
    QToolButton* stepForwardButton_ = nullptr;
    QToolButton* seekStartButton_ = nullptr;
    QToolButton* seekEndButton_ = nullptr;
    QToolButton* seekPreviousButton_ = nullptr;
    QToolButton* seekNextButton_ = nullptr;
    QToolButton* loopButton_ = nullptr;
    QToolButton* inButton_ = nullptr;
    QToolButton* outButton_ = nullptr;
    QToolButton* clearInOutButton_ = nullptr;
    QLabel* inTimecodeLabel_ = nullptr;
    QLabel* outTimecodeLabel_ = nullptr;
    QToolButton* speedQuarterButton_ = nullptr;
    QToolButton* speedHalfButton_ = nullptr;
    QToolButton* speedOneButton_ = nullptr;
    QCheckBox* ramCacheCheckbox_ = nullptr;
    QComboBox* playbackRangeCombo_ = nullptr;
    QComboBox* playbackSkipCombo_ = nullptr;
    QSlider* scrubSlider_ = nullptr;
    class PlaybackTimecodeFrame* timecodeFrame_ = nullptr;
    
    // State
    bool isPlaying_ = false;
    bool isPaused_ = false;
    bool isStopped_ = true;
    bool isLooping_ = false;
    float playbackSpeed_ = 1.0f;
    ArtifactInOutPoints* inOutPoints_ = nullptr;
    ArtifactCore::EventBus eventBus_ = ArtifactCore::globalEventBus();
    std::vector<ArtifactCore::EventBus::Subscription> eventBusSubscriptions_;
    QElapsedTimer frameWidgetUpdateTimer_;
    qint64 lastDisplayedFrame_ = std::numeric_limits<qint64>::min();
    Impl(ArtifactPlaybackControlWidget* owner)
        : owner_(owner)
    {}
    
    ~Impl() = default;
    
    void setupUI()
    {
        auto* mainLayout = new QVBoxLayout(owner_);
        mainLayout->setSpacing(6);
        mainLayout->setContentsMargins(10, 8, 10, 8);
        
        auto* transportRow = new QHBoxLayout();
        transportRow->setSpacing(6);
        
        seekStartButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/seek_start.svg")
        }, "先頭へ (Home)", Qt::Key_Home);
        
        stepBackwardButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/skip_previous.svg")
        }, "1フレーム戻る (←)", Qt::Key_Left);

        playButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/play_arrow.svg")
        }, "再生/一時停止 (Space)", Qt::Key_Space);
        playButton_->setProperty("artifactPlayButton", true);
        playButton_->setFixedSize(53, 53); // about 15% smaller than before
        playButton_->setIconSize(QSize(32, 32));

        stopButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/stop.svg"),
            QStringLiteral("Material/stop.svg")
        }, "停止", 0);

        stepForwardButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/skip_next.svg")
        }, "1フレーム進む (→)", Qt::Key_Right);

        seekEndButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/seek_end.svg")
        }, "末尾へ (End)", Qt::Key_End);
        
        inButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/push_pin.svg")
        }, "In 点設定 (I)", Qt::Key_I);
        
        outButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/remove_circle.svg")
        }, "Out 点設定 (O)", Qt::Key_O);
        clearInOutButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/neutral/clear.svg"),
            QStringLiteral("Material/clear.svg")
        }, "In/Out クリア", 0);

        loopButton_ = createToolButton(QStringList{
            QStringLiteral("MaterialVS/colored/E3E3E3/loop.svg")
        }, "ループ再生 (L)", Qt::Key_L);
        loopButton_->setCheckable(true);
        timecodeFrame_ = new PlaybackTimecodeFrame(owner_);

        transportRow->addWidget(seekStartButton_);
        transportRow->addWidget(stepBackwardButton_);
        transportRow->addWidget(playButton_);
        transportRow->addWidget(stopButton_);
        transportRow->addWidget(stepForwardButton_);
        transportRow->addWidget(seekEndButton_);
        transportRow->addSpacing(8);
        
        auto* inWidget = new QWidget(owner_);
        auto* inLayout = new QVBoxLayout(inWidget);
        inLayout->setContentsMargins(0, 0, 0, 0);
        inLayout->setSpacing(2);
        inLayout->addWidget(inButton_);
        inTimecodeLabel_ = new QLabel(QStringLiteral("--:--:--:--"), owner_);
        {
            QFont labelFont = inTimecodeLabel_->font();
            labelFont.setPointSize(7);
            inTimecodeLabel_->setFont(labelFont);
            QPalette labelPal = inTimecodeLabel_->palette();
            labelPal.setColor(QPalette::WindowText, QColor(150, 150, 150));
            inTimecodeLabel_->setPalette(labelPal);
            inTimecodeLabel_->setAlignment(Qt::AlignCenter);
        }
        inLayout->addWidget(inTimecodeLabel_);
        
        auto* outWidget = new QWidget(owner_);
        auto* outLayout = new QVBoxLayout(outWidget);
        outLayout->setContentsMargins(0, 0, 0, 0);
        outLayout->setSpacing(2);
        outLayout->addWidget(outButton_);
        outTimecodeLabel_ = new QLabel(QStringLiteral("--:--:--:--"), owner_);
        {
            QFont labelFont = outTimecodeLabel_->font();
            labelFont.setPointSize(7);
            outTimecodeLabel_->setFont(labelFont);
            QPalette labelPal = outTimecodeLabel_->palette();
            labelPal.setColor(QPalette::WindowText, QColor(150, 150, 150));
            outTimecodeLabel_->setPalette(labelPal);
            outTimecodeLabel_->setAlignment(Qt::AlignCenter);
        }
        outLayout->addWidget(outTimecodeLabel_);
        
        transportRow->addWidget(inWidget);
        transportRow->addWidget(outWidget);
        transportRow->addWidget(clearInOutButton_);
        transportRow->addWidget(timecodeFrame_);
        transportRow->addSpacing(8);
        transportRow->addWidget(loopButton_);
        transportRow->addStretch();

        auto* speedLayout = new QHBoxLayout();
        speedLayout->setSpacing(4);
        auto* speedLabel = createLabel(QStringLiteral("速度:"), QStringLiteral("再生速度"));
        {
            QFont font = speedLabel->font();
            font.setPointSize(11);
            font.setWeight(QFont::DemiBold);
            speedLabel->setFont(font);
        }
        speedLayout->addWidget(speedLabel);
        speedQuarterButton_ = createTextToolButton(QStringLiteral("x0.25"), "再生速度 0.25x", true);
        speedHalfButton_ = createTextToolButton(QStringLiteral("x0.5"), "再生速度 0.5x", true);
        speedOneButton_ = createTextToolButton(QStringLiteral("x1.0"), "再生速度 1.0x", true);
        speedQuarterButton_->setProperty("artifactSpeedPresetButton", true);
        speedHalfButton_->setProperty("artifactSpeedPresetButton", true);
        speedOneButton_->setProperty("artifactSpeedPresetButton", true);
        {
            const auto& theme = ArtifactCore::currentDCCTheme();
            auto applySpeedPalette = [&theme](QToolButton* button) {
                if (!button) {
                    return;
                }
                QPalette pal = button->palette();
                const QColor accent(theme.accentColor);
                pal.setColor(QPalette::WindowText, accent);
                pal.setColor(QPalette::Text, accent);
                pal.setColor(QPalette::ButtonText, accent);
                button->setPalette(pal);
            };
            applySpeedPalette(speedQuarterButton_);
            applySpeedPalette(speedHalfButton_);
            applySpeedPalette(speedOneButton_);
        }
        speedLayout->addWidget(speedQuarterButton_);
        speedLayout->addWidget(speedHalfButton_);
        speedLayout->addWidget(speedOneButton_);
        transportRow->addLayout(speedLayout);

        mainLayout->addLayout(transportRow);

        auto* optionsRow = new QHBoxLayout();
        optionsRow->setContentsMargins(4, 0, 4, 0);
        optionsRow->setSpacing(12);

        ramCacheCheckbox_ = new QCheckBox(QStringLiteral("Ramキャッシュ作成してからプレビュー開始"), owner_);
        {
            QFont font = ramCacheCheckbox_->font();
            font.setPointSize(10);
            ramCacheCheckbox_->setFont(font);
            QPalette pal = ramCacheCheckbox_->palette();
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
            ramCacheCheckbox_->setPalette(pal);
        }
        optionsRow->addWidget(ramCacheCheckbox_);

        optionsRow->addSpacing(16);

        auto* rangeLabel = new QLabel(QStringLiteral("再生範囲:"), owner_);
        {
            QFont font = rangeLabel->font();
            font.setPointSize(10);
            rangeLabel->setFont(font);
            applyThemeTextPalette(rangeLabel, QColor(ArtifactCore::currentDCCTheme().textColor));
        }
        optionsRow->addWidget(rangeLabel);

        playbackRangeCombo_ = new QComboBox(owner_);
        playbackRangeCombo_->setFixedWidth(140);
        playbackRangeCombo_->addItem(QStringLiteral("コンポジション全体"), QVariant::fromValue(PlaybackRangeMode::All));
        playbackRangeCombo_->addItem(QStringLiteral("ワークエリア"), QVariant::fromValue(PlaybackRangeMode::WorkArea));
        playbackRangeCombo_->addItem(QStringLiteral("選択範囲"), QVariant::fromValue(PlaybackRangeMode::Selection));
        {
            QPalette pal = playbackRangeCombo_->palette();
            pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
            pal.setColor(QPalette::Button, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().selectionColor));
            pal.setColor(QPalette::HighlightedText, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
            playbackRangeCombo_->setPalette(pal);
            playbackRangeCombo_->setAutoFillBackground(true);
            if (auto* view = playbackRangeCombo_->view()) {
                view->setPalette(pal);
                if (auto* viewport = view->viewport()) {
                    viewport->setPalette(pal);
                }
            }
        }
        optionsRow->addWidget(playbackRangeCombo_);

        optionsRow->addStretch();
        mainLayout->addLayout(optionsRow);

        auto* skipRow = new QHBoxLayout();
        skipRow->setContentsMargins(4, 0, 4, 0);
        skipRow->setSpacing(12);

        auto* skipLabel = new QLabel(QStringLiteral("再生モード:"), owner_);
        {
            QFont font = skipLabel->font();
            font.setPointSize(10);
            skipLabel->setFont(font);
            applyThemeTextPalette(skipLabel, QColor(ArtifactCore::currentDCCTheme().textColor));
        }
        skipRow->addWidget(skipLabel);

        playbackSkipCombo_ = new QComboBox(owner_);
        playbackSkipCombo_->setFixedWidth(140);
        playbackSkipCombo_->addItem(QStringLiteral("全フレーム"), QVariant::fromValue(PlaybackSkipMode::None));
        playbackSkipCombo_->addItem(QStringLiteral("1/2 スキップ"), QVariant::fromValue(PlaybackSkipMode::Skip1));
        playbackSkipCombo_->addItem(QStringLiteral("1/4 スキップ"), QVariant::fromValue(PlaybackSkipMode::Skip3));
        {
            QPalette pal = playbackSkipCombo_->palette();
            pal.setColor(QPalette::Window, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::Base, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
            pal.setColor(QPalette::Button, QColor(ArtifactCore::currentDCCTheme().secondaryBackgroundColor));
            pal.setColor(QPalette::ButtonText, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::WindowText, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::Text, QColor(ArtifactCore::currentDCCTheme().textColor));
            pal.setColor(QPalette::Highlight, QColor(ArtifactCore::currentDCCTheme().selectionColor));
            pal.setColor(QPalette::HighlightedText, QColor(ArtifactCore::currentDCCTheme().backgroundColor));
            playbackSkipCombo_->setPalette(pal);
            playbackSkipCombo_->setAutoFillBackground(true);
            if (auto* view = playbackSkipCombo_->view()) {
                view->setPalette(pal);
                if (auto* viewport = view->viewport()) {
                    viewport->setPalette(pal);
                }
            }
        }
        skipRow->addWidget(playbackSkipCombo_);
        skipRow->addStretch();
        mainLayout->addLayout(skipRow);

        scrubSlider_ = new QSlider(Qt::Horizontal, owner_);
        scrubSlider_->setRange(0, 300);
        scrubSlider_->setTracking(true);
        scrubSlider_->setMinimumHeight(18);
        scrubSlider_->setToolTip(QStringLiteral("Current frame scrubber"));
        mainLayout->addWidget(scrubSlider_);

        connectSignals();
    }
    
    QToolButton* createToolButton(const QStringList& iconNames, const QString& tooltip, int shortcut)
    {
        auto* button = new ArtifactFramedToolButton();
        button->setIcon(loadIconWithFallback(iconNames));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(tooltip);
        button->setFixedSize(32, 32);
        applyThemeTextPalette(button, QColor(ArtifactCore::currentDCCTheme().textColor));
        
        if (shortcut != 0) {
            button->setShortcut(QKeySequence(shortcut));
        }
        
        return button;
    }

    ArtifactToneLabel* createToneLabel(const QString& text, const QString& tooltip, ArtifactTextTone tone)
    {
        auto* label = new ArtifactToneLabel(owner_);
        label->setText(text);
        label->setTone(tone);
        label->setToolTip(tooltip);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        return label;
    }

    QToolButton* createTextToolButton(const QString& text, const QString& tooltip, bool checkable)
    {
        auto* button = createToolButton(QStringList{}, tooltip, 0);
        button->setText(text);
        button->setCheckable(checkable);
        button->setFixedSize(48, 28);
        button->setIcon(QIcon());
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        QFont font = button->font();
        font.setPointSize(11);
        font.setWeight(QFont::DemiBold);
        button->setFont(font);
        applyThemeTextPalette(button, QColor(ArtifactCore::currentDCCTheme().textColor));
        return button;
    }

    QLabel* createLabel(const QString& text, const QString& tooltip)
    {
        auto* label = new QLabel(text, owner_);
        label->setToolTip(tooltip);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        QFont font = label->font();
        font.setPointSize(12);
        font.setWeight(QFont::DemiBold);
        label->setFont(font);
        applyThemeTextPalette(label, QColor(ArtifactCore::currentDCCTheme().textColor));
        return label;
    }

    void updateSpeedPresetButtons(float speed)
    {
        const auto setChecked = [](QToolButton* button, bool checked) {
            if (!button) return;
            button->blockSignals(true);
            button->setChecked(checked);
            button->blockSignals(false);
        };
        setChecked(speedQuarterButton_, std::abs(speed - 0.25f) < 0.001f);
        setChecked(speedHalfButton_, std::abs(speed - 0.5f) < 0.001f);
        setChecked(speedOneButton_, std::abs(speed - 1.0f) < 0.001f);
        playbackSpeed_ = speed;
        if (timecodeFrame_) {
            timecodeFrame_->setToolTip(QStringLiteral("Playback speed: %1").arg(formatSpeedLabel(speed)));
        }
    }

    void updateFrameWidgets()
    {
        if (!owner_) {
            return;
        }
        const auto* service = ArtifactPlaybackService::instance();
        const FrameRate fpsRate = service ? service->frameRate() : FrameRate(30.0f);
        const float fps = std::max(1.0f, fpsRate.framerate());
        const FrameRange range = service ? service->frameRange() : FrameRange(FramePosition(0), FramePosition(300));
        const FramePosition current = service ? service->currentFrame() : FramePosition(0);

        const qint64 startFrame = std::min(range.start(), range.end());
        const qint64 endFrame = std::max(range.start(), range.end());
        const qint64 clampedCurrent = std::clamp(current.framePosition(), startFrame, endFrame);

        if (scrubSlider_) {
            QSignalBlocker blocker(scrubSlider_);
            scrubSlider_->setRange(static_cast<int>(startFrame), static_cast<int>(std::max(startFrame, endFrame)));
            scrubSlider_->setValue(static_cast<int>(clampedCurrent));
        }

        if (timecodeFrame_) {
            QString inText = QStringLiteral("--:--:--:--");
            QString outText = QStringLiteral("--:--:--:--");
            QString currentText = QStringLiteral("00:00:00:00 / 00:00:00:00");
            if (inOutPoints_) {
                if (const auto inPoint = inOutPoints_->inPoint()) {
                    inText = formatTimecode(inPoint->framePosition(), fps);
                }
                if (const auto outPoint = inOutPoints_->outPoint()) {
                    outText = formatTimecode(outPoint->framePosition(), fps);
                }
            }
            currentText = formatFrameCount(clampedCurrent);
            timecodeFrame_->setCurrentFrameText(currentText);
            timecodeFrame_->setRangeTexts(inText, outText);
            
            if (inTimecodeLabel_) {
                inTimecodeLabel_->setText(inText);
            }
            if (outTimecodeLabel_) {
                outTimecodeLabel_->setText(outText);
            }
        }
    }

    void updateFrameWidgetsCoalesced(bool force = false)
    {
        const auto* service = ArtifactPlaybackService::instance();
        const FramePosition current = service ? service->currentFrame() : FramePosition(0);
        const bool playing = service && service->isPlaying();
        constexpr qint64 kPlaybackUiUpdateIntervalMs = 16;

        if (!force && current.framePosition() == lastDisplayedFrame_) {
            return;
        }
        if (!force && playing && frameWidgetUpdateTimer_.isValid() &&
            frameWidgetUpdateTimer_.elapsed() < kPlaybackUiUpdateIntervalMs) {
            return;
        }

        if (frameWidgetUpdateTimer_.isValid()) {
            frameWidgetUpdateTimer_.restart();
        } else {
            frameWidgetUpdateTimer_.start();
        }
        lastDisplayedFrame_ = current.framePosition();
        updateFrameWidgets();
    }

    void attachInOutPoints(ArtifactInOutPoints* points)
    {
        if (inOutPoints_ == points) {
            return;
        }
        inOutPoints_ = points;
        updateFrameWidgets();
    }
    
    void connectSignals()
    {
        // 再生制御
        QObject::connect(playButton_, &QToolButton::clicked, owner_, [this]() {
            handlePlayButtonClicked();
        });

        QObject::connect(stopButton_, &QToolButton::clicked, owner_, [this]() {
            handleStopButtonClicked();
        });
        
        // シーク操作
        QObject::connect(seekStartButton_, &QToolButton::clicked, owner_, [this]() {
            handleSeekStartClicked();
        });
        
        QObject::connect(seekEndButton_, &QToolButton::clicked, owner_, [this]() {
            handleSeekEndClicked();
        });
        
        // フレーム操作
        QObject::connect(stepBackwardButton_, &QToolButton::clicked, owner_, [this]() {
            handleStepBackwardClicked();
        });
        
        QObject::connect(stepForwardButton_, &QToolButton::clicked, owner_, [this]() {
            handleStepForwardClicked();
        });
        
        // オプション
        QObject::connect(loopButton_, &QToolButton::toggled, owner_, [this](bool checked) {
            handleLoopToggled(checked);
        });
        
        QObject::connect(inButton_, &QToolButton::clicked, owner_, [this]() {
            handleInButtonClicked();
        });
        
        QObject::connect(outButton_, &QToolButton::clicked, owner_, [this]() {
            handleOutButtonClicked();
        });

        QObject::connect(clearInOutButton_, &QToolButton::clicked, owner_, [this]() {
            handleClearInOutClicked();
        });

        QObject::connect(speedQuarterButton_, &QToolButton::clicked, owner_, [this]() {
            handleSpeedPresetClicked(0.25f);
        });
        QObject::connect(speedHalfButton_, &QToolButton::clicked, owner_, [this]() {
            handleSpeedPresetClicked(0.5f);
        });
        QObject::connect(speedOneButton_, &QToolButton::clicked, owner_, [this]() {
            handleSpeedPresetClicked(1.0f);
        });

        QObject::connect(ramCacheCheckbox_, &QCheckBox::toggled, owner_, [](bool checked) {
            if (auto* service = ArtifactPlaybackService::instance()) {
                service->setRamPreviewEnabled(checked);
            }
        });

        QObject::connect(playbackRangeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), owner_, [this](int index) {
            if (auto* service = ArtifactPlaybackService::instance()) {
                PlaybackRangeMode mode = playbackRangeCombo_->itemData(index).value<PlaybackRangeMode>();
                service->setPlaybackRangeMode(mode);
            }
        });

        QObject::connect(playbackSkipCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), owner_, [this](int index) {
            if (auto* service = ArtifactPlaybackService::instance()) {
                PlaybackSkipMode mode = playbackSkipCombo_->itemData(index).value<PlaybackSkipMode>();
                service->setPlaybackSkipMode(mode);
            }
        });

        QObject::connect(scrubSlider_, &QSlider::valueChanged, owner_, [this](int value) {
            if (auto* service = ArtifactPlaybackService::instance()) {
                service->goToFrame(FramePosition(value));
            }
            updateFrameWidgets();
        });
        
        // サービスからの状態更新は internal event bus に集約する
        eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackStateChangedEvent>(
            [this](const PlaybackStateChangedEvent& event) {
                updatePlaybackState(event.state);
            }));
        eventBusSubscriptions_.push_back(eventBus_.subscribe<FrameChangedEvent>(
            [this](const FrameChangedEvent&) {
                updateFrameWidgetsCoalesced(false);
            }));
        eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackFrameRangeChangedEvent>(
            [this](const PlaybackFrameRangeChangedEvent&) {
                updateFrameWidgetsCoalesced(true);
            }));
        eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackSpeedChangedEvent>(
            [this](const PlaybackSpeedChangedEvent& event) {
                updateSpeedPresetButtons(event.speed);
            }));
        eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackLoopingChangedEvent>(
            [this](const PlaybackLoopingChangedEvent& event) {
                isLooping_ = event.loop;
                if (loopButton_) {
                    loopButton_->blockSignals(true);
                    loopButton_->setChecked(event.loop);
                    loopButton_->blockSignals(false);
                }
            }));
        eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackCompositionChangedEvent>(
            [this](const PlaybackCompositionChangedEvent&) {
                syncFromService();
            }));
        eventBusSubscriptions_.push_back(eventBus_.subscribe<PlaybackInOutPointsChangedEvent>(
            [this](const PlaybackInOutPointsChangedEvent&) {
                updateFrameWidgetsCoalesced(true);
            }));

        if (auto* service = ArtifactPlaybackService::instance()) {
            QObject::connect(service, &ArtifactPlaybackService::ramPreviewStateChanged, owner_, [this](bool enabled, const FrameRange&) {
                if (ramCacheCheckbox_) {
                    QSignalBlocker blocker(ramCacheCheckbox_);
                    ramCacheCheckbox_->setChecked(enabled);
                }
            });
        }
    }
    
    void handlePlayButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            if (service->isPlaying()) {
                service->pause();
            } else {
                service->play();
            }
        }
    }
    
    void handlePauseButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->pause();
        }
    }
    
    void handleStopButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->stop();
        }
    }
    
    void handleSeekStartClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToStartFrame();
        }
    }
    
    void handleSeekEndClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToEndFrame();
        }
    }
    
    void handleSeekPreviousClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToPreviousMarker();
        }
    }
    
    void handleSeekNextClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToNextMarker();
        }
    }
    
    void handleStepBackwardClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToPreviousFrame();
        }
    }
    
    void handleStepForwardClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->goToNextFrame();
        }
    }
    
    void handleLoopToggled(bool enabled)
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->setLooping(enabled);
        }
        isLooping_ = enabled;
    }
    
    void handleInButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            if (auto* points = service->inOutPoints()) {
                points->setInPoint(service->currentFrame());
            }
        }
    }
    
    void handleOutButtonClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            if (auto* points = service->inOutPoints()) {
                points->setOutPoint(service->currentFrame());
            }
        }
    }
    
    void handleClearInOutClicked()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            if (auto* points = service->inOutPoints()) {
                points->clearAllPoints();
            }
        }
    }

    void handleSpeedPresetClicked(float speed)
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            service->setPlaybackSpeed(speed);
        }
        updateSpeedPresetButtons(speed);
    }
    
    void updatePlaybackState(PlaybackState state)
    {
        isPlaying_ = (state == PlaybackState::Playing);
        isPaused_ = (state == PlaybackState::Paused);
        isStopped_ = (state == PlaybackState::Stopped);
        
        // ボタンの状態と外観を更新
        if (playButton_) {
            playButton_->setChecked(isPlaying_);
            playButton_->setIcon(loadIconWithFallback(isPlaying_
                ? QStringList{QStringLiteral("MaterialVS/colored/E3E3E3/pause.svg"),
                              QStringLiteral("Material/pause.svg")}
                : QStringList{QStringLiteral("MaterialVS/colored/E3E3E3/play_arrow.svg"),
                              QStringLiteral("Material/play_arrow.svg")}));
            playButton_->setToolTip(isPlaying_ ? QStringLiteral("一時停止 (Space)") : QStringLiteral("再生 (Space)"));
        }
        if (pauseButton_) {
            pauseButton_->setEnabled(isPlaying_);
        }
        if (stopButton_) {
            stopButton_->setEnabled(isPlaying_ || isPaused_);
        }
        updateFrameWidgets();
    }
    
    void syncFromService()
    {
        if (auto* service = ArtifactPlaybackService::instance()) {
            updatePlaybackState(service->state());
            isLooping_ = service->isLooping();
            playbackSpeed_ = service->playbackSpeed();
            updateSpeedPresetButtons(playbackSpeed_);
            attachInOutPoints(service->inOutPoints());
            updateFrameWidgets();
            if (loopButton_) {
                loopButton_->setChecked(isLooping_);
            }
            if (scrubSlider_) {
                QSignalBlocker blocker(scrubSlider_);
                const FrameRange range = service->frameRange();
                scrubSlider_->setRange(static_cast<int>(std::min(range.start(), range.end())),
                                       static_cast<int>(std::max(range.start(), range.end())));
                scrubSlider_->setValue(static_cast<int>(service->currentFrame().framePosition()));
            }
            if (ramCacheCheckbox_) {
                QSignalBlocker blocker(ramCacheCheckbox_);
                ramCacheCheckbox_->setChecked(service->isRamPreviewEnabled());
            }
            if (playbackRangeCombo_) {
                QSignalBlocker blocker(playbackRangeCombo_);
                int index = playbackRangeCombo_->findData(QVariant::fromValue(service->playbackRangeMode()));
                if (index >= 0) {
                    playbackRangeCombo_->setCurrentIndex(index);
                }
            }
            if (playbackSkipCombo_) {
                QSignalBlocker blocker(playbackSkipCombo_);
                int index = playbackSkipCombo_->findData(QVariant::fromValue(service->playbackSkipMode()));
                if (index >= 0) {
                    playbackSkipCombo_->setCurrentIndex(index);
                }
            }
        }
    }
};

// ============================================================================
// ArtifactPlaybackControlWidget Implementation
// ============================================================================

W_OBJECT_IMPL(ArtifactPlaybackControlWidget)

ArtifactPlaybackControlWidget::ArtifactPlaybackControlWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setWindowTitle("Playback Control");
    setMinimumHeight(165);
    setAutoFillBackground(false);
    
    impl_->setupUI();
    
    impl_->syncFromService();
}

ArtifactPlaybackControlWidget::~ArtifactPlaybackControlWidget()
{
    delete impl_;
}

void ArtifactPlaybackControlWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    const auto& theme = ArtifactCore::currentDCCTheme();
    painter.fillRect(rect(), QColor(theme.backgroundColor));
}

void ArtifactPlaybackControlWidget::play()
{
    impl_->handlePlayButtonClicked();
}

void ArtifactPlaybackControlWidget::pause()
{
    impl_->handlePauseButtonClicked();
}

void ArtifactPlaybackControlWidget::stop()
{
    impl_->handleStopButtonClicked();
}

void ArtifactPlaybackControlWidget::togglePlayPause()
{
    if (impl_->isPlaying_ || impl_->isPaused_) {
        pause();
    } else {
        play();
    }
}

void ArtifactPlaybackControlWidget::seekStart()
{
    impl_->handleSeekStartClicked();
}

void ArtifactPlaybackControlWidget::seekEnd()
{
    impl_->handleSeekEndClicked();
}

void ArtifactPlaybackControlWidget::seekPrevious()
{
    impl_->handleSeekPreviousClicked();
}

void ArtifactPlaybackControlWidget::seekNext()
{
    impl_->handleSeekNextClicked();
}

void ArtifactPlaybackControlWidget::stepForward()
{
    impl_->handleStepForwardClicked();
}

void ArtifactPlaybackControlWidget::stepBackward()
{
    impl_->handleStepBackwardClicked();
}

void ArtifactPlaybackControlWidget::setLoopEnabled(bool enabled)
{
    if (impl_->loopButton_) {
        impl_->loopButton_->setChecked(enabled);
    }
    impl_->handleLoopToggled(enabled);
}

bool ArtifactPlaybackControlWidget::isLoopEnabled() const
{
    return impl_->isLooping_;
}

void ArtifactPlaybackControlWidget::setPlaybackSpeed(float speed)
{
    impl_->playbackSpeed_ = speed;
    if (auto* service = ArtifactPlaybackService::instance()) {
        service->setPlaybackSpeed(speed);
    }
}

float ArtifactPlaybackControlWidget::playbackSpeed() const
{
    return impl_->playbackSpeed_;
}

bool ArtifactPlaybackControlWidget::isPlaying() const
{
    return impl_->isPlaying_;
}

bool ArtifactPlaybackControlWidget::isPaused() const
{
    return impl_->isPaused_;
}

bool ArtifactPlaybackControlWidget::isStopped() const
{
    return impl_->isStopped_;
}

void ArtifactPlaybackControlWidget::setInPoint()
{
    impl_->handleInButtonClicked();
}

void ArtifactPlaybackControlWidget::setOutPoint()
{
    impl_->handleOutButtonClicked();
}

void ArtifactPlaybackControlWidget::clearInOutPoints()
{
    impl_->handleClearInOutClicked();
}

// ============================================================================
// ArtifactPlaybackInfoWidget::Impl
// ============================================================================

class ArtifactPlaybackInfoWidget::Impl {
public:
    ArtifactPlaybackInfoWidget* owner_;
    
    QLabel* frameLabel_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QLabel* droppedLabel_ = nullptr;
    
    int64_t currentFrame_ = 0;
    int64_t totalFrames_ = 300;
    float speed_ = 1.0f;
    int64_t droppedFrames_ = 0;
    
    Impl(ArtifactPlaybackInfoWidget* owner)
        : owner_(owner)
    {}
    
    void setupUI()
    {
        auto* layout = new QHBoxLayout(owner_);
        layout->setSpacing(16);
        layout->setContentsMargins(8, 4, 8, 4);
        
        // フレーム表示
        frameLabel_ = createLabel("0 / 300", "現在のフレーム / 総フレーム数");
        layout->addWidget(frameLabel_);
        layout->addSpacing(24);
        
        // 速度表示
        speedLabel_ = createLabel("1.00x", "再生速度");
        layout->addWidget(speedLabel_);
        
        // ドロップフレーム表示
        droppedLabel_ = createLabel("Dropped: 0", "ドロップフレーム数");
        applyThemeTextPalette(droppedLabel_, QColor(ArtifactCore::currentDCCTheme().textColor), 90);
        layout->addWidget(droppedLabel_);
        
        layout->addStretch();
    }
    
    QLabel* createLabel(const QString& text, const QString& tooltip)
    {
        auto* label = new QLabel(text);
        label->setToolTip(tooltip);
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        QFont font = label->font();
        font.setPointSize(12);
        label->setFont(font);
        applyThemeTextPalette(label, QColor(ArtifactCore::currentDCCTheme().textColor));
        return label;
    }
};

// ============================================================================
// ArtifactPlaybackInfoWidget Implementation
// ============================================================================

W_OBJECT_IMPL(ArtifactPlaybackInfoWidget)

ArtifactPlaybackInfoWidget::ArtifactPlaybackInfoWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setMinimumHeight(32);
    impl_->setupUI();
    setAutoFillBackground(false);
}

ArtifactPlaybackInfoWidget::~ArtifactPlaybackInfoWidget()
{
    delete impl_;
}

void ArtifactPlaybackInfoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    const auto& theme = ArtifactCore::currentDCCTheme();
    painter.fillRect(rect(), QColor(theme.secondaryBackgroundColor));
}

void ArtifactPlaybackInfoWidget::setCurrentFrame(int64_t frame)
{
    impl_->currentFrame_ = frame;
    
    if (impl_->frameLabel_) {
        impl_->frameLabel_->setText(QString("%1 / %2").arg(frame).arg(impl_->totalFrames_));
    }
    
    Q_EMIT frameChanged(frame);
}

void ArtifactPlaybackInfoWidget::setTotalFrames(int64_t frames)
{
    impl_->totalFrames_ = frames;
    setCurrentFrame(impl_->currentFrame_);
}

void ArtifactPlaybackInfoWidget::setFrameRate(float fps)
{
    Q_UNUSED(fps);
}

void ArtifactPlaybackInfoWidget::setPlaybackSpeed(float speed)
{
    impl_->speed_ = speed;
    if (impl_->speedLabel_) {
        QString speedText = speed >= 0 ? QString("%1x").arg(speed, 0, 'f', 2)
                                        : QString("%1x (REV)").arg(std::abs(speed), 0, 'f', 2);
        impl_->speedLabel_->setText(speedText);
    }
}

void ArtifactPlaybackInfoWidget::setDroppedFrames(int64_t count)
{
    impl_->droppedFrames_ = count;
    if (impl_->droppedLabel_) {
        impl_->droppedLabel_->setText(QString("Dropped: %1").arg(count));
    }
}

void ArtifactPlaybackInfoWidget::setEditable(bool editable)
{
    // 将来的にはスピンボックスなどで直接入力可能に
    Q_UNUSED(editable);
}

// ============================================================================
// ArtifactPlaybackSpeedWidget::Impl
// ============================================================================

class ArtifactPlaybackSpeedWidget::Impl {
public:
    ArtifactPlaybackSpeedWidget* owner_;
    
    QSlider* speedSlider_ = nullptr;
    QDoubleSpinBox* speedSpin_ = nullptr;
    QComboBox* presetCombo_ = nullptr;
    
    float currentSpeed_ = 1.0f;
    
    Impl(ArtifactPlaybackSpeedWidget* owner)
        : owner_(owner)
    {}
    
    void setupUI()
    {
        auto* layout = new QHBoxLayout(owner_);
        layout->setSpacing(8);
        layout->setContentsMargins(8, 4, 8, 4);
        
        // プリセットコンボボックス
        presetCombo_ = new QComboBox();
        presetCombo_->addItem("0.25x", 0.25);
        presetCombo_->addItem("0.5x", 0.5);
        presetCombo_->addItem("1.0x", 1.0);
        presetCombo_->addItem("2.0x", 2.0);
        presetCombo_->addItem("-1.0x (REV)", -1.0);
        presetCombo_->setCurrentIndex(2);  // 1.0x
        presetCombo_->setFixedWidth(80);
        layout->addWidget(new QLabel("Speed:"));
        layout->addWidget(presetCombo_);
        
        // スライダー
        speedSlider_ = new QSlider(Qt::Horizontal);
        speedSlider_->setRange(-20, 20);  // -2.0x to 2.0x
        speedSlider_->setValue(10);       // 1.0x
        speedSlider_->setFixedWidth(150);
        layout->addWidget(speedSlider_);
        
        // スピンボックス
        speedSpin_ = new QDoubleSpinBox();
        speedSpin_->setRange(-2.0, 2.0);
        speedSpin_->setSingleStep(0.25);
        speedSpin_->setValue(1.0);
        speedSpin_->setSuffix("x");
        speedSpin_->setFixedWidth(70);
        layout->addWidget(speedSpin_);
        
        // シグナル接続
        QObject::connect(presetCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            owner_, [this](int index) {
                float speed = presetCombo_->itemData(index).toFloat();
                setPlaybackSpeed(speed);
            });
        
        QObject::connect(speedSlider_, &QSlider::valueChanged,
            owner_, [this](int value) {
                float speed = value / 10.0f;
                if (speedSpin_) {
                    speedSpin_->blockSignals(true);
                    speedSpin_->setValue(speed);
                    speedSpin_->blockSignals(false);
                }
                setPlaybackSpeed(speed);
            });
        
        QObject::connect(speedSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            owner_, [this](double value) {
                if (speedSlider_) {
                    speedSlider_->blockSignals(true);
                    speedSlider_->setValue(static_cast<int>(value * 10));
                    speedSlider_->blockSignals(false);
                }
                setPlaybackSpeed(static_cast<float>(value));
            });
    }
    
    void setPlaybackSpeed(float speed)
    {
        currentSpeed_ = speed;
        Q_EMIT owner_->speedChanged(speed);
    }
};

// ============================================================================
// ArtifactPlaybackSpeedWidget Implementation
// ============================================================================

W_OBJECT_IMPL(ArtifactPlaybackSpeedWidget)

ArtifactPlaybackSpeedWidget::ArtifactPlaybackSpeedWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl(this))
{
    setMinimumHeight(32);
    impl_->setupUI();
    setAutoFillBackground(false);
}

ArtifactPlaybackSpeedWidget::~ArtifactPlaybackSpeedWidget()
{
    delete impl_;
}

void ArtifactPlaybackSpeedWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    const auto& theme = ArtifactCore::currentDCCTheme();
    painter.fillRect(rect(), QColor(theme.secondaryBackgroundColor));
}

float ArtifactPlaybackSpeedWidget::playbackSpeed() const
{
    return impl_->currentSpeed_;
}

void ArtifactPlaybackSpeedWidget::setPlaybackSpeed(float speed)
{
    impl_->setPlaybackSpeed(speed);
}

void ArtifactPlaybackSpeedWidget::setSpeedPreset(float speed)
{
    if (impl_->presetCombo_) {
        int index = impl_->presetCombo_->findData(speed);
        if (index >= 0) {
            impl_->presetCombo_->setCurrentIndex(index);
        }
    }
    setPlaybackSpeed(speed);
}

} // namespace Artifact

