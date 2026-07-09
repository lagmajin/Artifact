module;
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QString>
#include <QChar>
#include <cmath>
export module Artifact.Widgets.RelativeSpinBox;

export namespace Artifact {

class ArtifactRelativeDoubleSpinBox : public QDoubleSpinBox {
public:
  using QDoubleSpinBox::QDoubleSpinBox;

  QLineEdit *scrubLineEdit() const { return lineEdit(); }

  void setDefaultValue(double v) { defaultValue_ = v; }
  double defaultValue() const { return defaultValue_; }

protected:
  void wheelEvent(QWheelEvent *event) override {
    event->ignore();
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton &&
        (event->modifiers() & Qt::AltModifier)) {
      setValue(defaultValue_);
      event->accept();
      return;
    }
    QDoubleSpinBox::mousePressEvent(event);
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

private:
  double defaultValue_ = 0.0;
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
      Q_UNUSED(pos);
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
      int delta = text.mid(1).toInt(&ok);
      if (!ok)
        return value();
      if (first == '+')
        return value() + delta;
      if (first == '-')
        return value() - delta;
      if (first == '*')
        return value() * delta;
      if (first == '/')
        return (delta != 0) ? value() / delta : value();
    }
    return QSpinBox::valueFromText(text);
  }
};

} // namespace Artifact
