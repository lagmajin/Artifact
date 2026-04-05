module;
#include <QDialog>
#include <wobjectdefs.h>

export module Artifact.Widgets.ColorSwatchDialog;

import Color.Float;

export namespace Artifact {

class ColorSwatchDialog : public QDialog {
    W_OBJECT(ColorSwatchDialog)
private:
    class Impl;
    Impl* impl_;
protected:
    void showEvent(QShowEvent* event) override;
public:
    explicit ColorSwatchDialog(QWidget* parent = nullptr);
    ~ColorSwatchDialog();

    ArtifactCore::FloatColor selectedColor() const;
    void setCurrentColor(const ArtifactCore::FloatColor& color);

public/*signals*/:
    void colorApplied(const ArtifactCore::FloatColor& color) W_SIGNAL(colorApplied, color);
};

} // namespace Artifact
