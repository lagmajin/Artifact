module;
#include <QVector>
#include <QMatrix4x4>
#include <QString>

export module Generator.Clone;

export namespace Artifact
{


 class CloneGenerator
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  CloneGenerator();
  virtual ~CloneGenerator();

  void setCount(int count);
  int count() const;

  void setSpacing(float spacing);
  float spacing() const;

  void setPrototypeName(const QString& name);
  QString prototypeName() const;

  virtual QVector<QMatrix4x4> generateTransforms() const;
 };

};
