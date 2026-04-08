module;
#include <utility>

#include <QWidget>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringListModel>
#include <wobjectdefs.h>
export module Artifact.Widgets.FontPicker;

import Font.FreeFont;
import Font.Descriptor;
import Event.Bus;

namespace Artifact {

export class FontPickerWidget : public QWidget {
    W_OBJECT(FontPickerWidget)
public:
    explicit FontPickerWidget(QWidget* parent = nullptr);
    virtual ~FontPickerWidget();

    void setCurrentFont(const QString& family);
    QString currentFont() const;

    // 信号
    void fontChanged(const QString& family) W_SIGNAL(fontChanged, family);

private:
    void setupUi();
    void updateFontList();

    QComboBox* fontCombo_ = nullptr;
    ArtifactCore::EventBus::Subscription fontChangeSubscription_;
};

} // namespace Artifact
