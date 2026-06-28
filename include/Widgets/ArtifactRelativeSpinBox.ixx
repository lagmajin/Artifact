module;
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QWheelEvent>
#include <QString>
#include <QChar>
#include <cmath>
export module Artifact.Widgets.RelativeSpinBox;

// 接頭辞計算付きスピンボックス（AE 風）。
// +10 / -5 / *2 / /3 のような入力を Enter 確定で現在値に対する相対演算として処理する。
// それ以外の入力は通常の QDoubleSpinBox / QSpinBox と同じ。
//
// 元々 ArtifactPropertyEditor.cppm のファイルローカルクラスだったが、
// Dialog 系などプロパティエディタ以外の数値入力欄でも接頭辞計算を共有するため
// このモジュールに切り出した。

export namespace Artifact {

class ArtifactRelativeDoubleSpinBox : public QDoubleSpinBox {
public:
  using QDoubleSpinBox::QDoubleSpinBox;

  QLineEdit *scrubLineEdit() const { return lineEdit(); }

protected:
  void wheelEvent(QWheelEvent *event) override {
    event->ignore();
  }

  QValidator::State validate(QString &input, int &pos) const override {
    if (input.isEmpty())
      return QValidator::Intermediate;
    const QChar first = input.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      // Allow relative operators at the start
      QString rest = input.mid(1);
      if (rest.isEmpty())
        return QValidator::Intermediate;
      return QValidator::Acceptable;
    }
    return QDoubleSpinBox::validate(input, pos);
  }

  double valueFromText(const QString &text) const override {
    if (text.isEmpty())
      return value();
    const QChar first = text.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      bool ok = false;
      double delta = text.mid(1).toDouble(&ok);
      if (!ok)
        return value();
      if (first == '+')
        return value() + delta;
      if (first == '-')
        return value() - delta;
      if (first == '*')
        return value() * delta;
      if (first == '/')
        return (std::abs(delta) > 1e-9) ? value() / delta : value();
    }
    return QDoubleSpinBox::valueFromText(text);
  }
};

class ArtifactRelativeSpinBox : public QSpinBox {
public:
  using QSpinBox::QSpinBox;

  QLineEdit *scrubLineEdit() const { return lineEdit(); }

protected:
  void wheelEvent(QWheelEvent *event) override {
    event->ignore();
  }

  QValidator::State validate(QString &input, int &pos) const override {
    if (input.isEmpty())
      return QValidator::Intermediate;
    const QChar first = input.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      QString rest = input.mid(1);
      if (rest.isEmpty())
        return QValidator::Intermediate;
      return QValidator::Acceptable;
    }
    return QSpinBox::validate(input, pos);
  }

  int valueFromText(const QString &text) const override {
    if (text.isEmpty())
      return value();
    const QChar first = text.at(0);
    if (first == '+' || first == '-' || first == '*' || first == '/') {
      bool ok = false;
      double delta = text.mid(1).toDouble(&ok);
      if (!ok)
        return value();
      if (first == '+')
        return value() + static_cast<int>(delta);
      if (first == '-')
        return value() - static_cast<int>(delta);
      if (first == '*')
        return static_cast<int>(value() * delta);
      if (first == '/')
        return (std::abs(delta) > 1e-9) ? static_cast<int>(value() / delta)
                                        : value();
    }
    return QSpinBox::valueFromText(text);
  }
};

} // namespace Artifact
