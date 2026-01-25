module;
#include <QVector>
#include <QMatrix4x4>
#include <QString>

module Generator.Clone;

import std;

namespace Artifact
{
 class CloneGenerator::Impl
 {
 public:
  int count_ = 1;
  float spacing_ = 100.0f;
  QString prototypeName_;
 };

 CloneGenerator::CloneGenerator() :impl_(new Impl())
 {

 }

 CloneGenerator::~CloneGenerator()
 {
  delete impl_;
 }

 void CloneGenerator::setCount(int count)
 {
  if (count < 1) count = 1;
  impl_->count_ = count;
 }

 int CloneGenerator::count() const
 {
  return impl_->count_;
 }

 void CloneGenerator::setSpacing(float spacing)
 {
  impl_->spacing_ = spacing;
 }

 float CloneGenerator::spacing() const
 {
  return impl_->spacing_;
 }

 void CloneGenerator::setPrototypeName(const QString& name)
 {
  impl_->prototypeName_ = name;
 }

 QString CloneGenerator::prototypeName() const
 {
  return impl_->prototypeName_;
 }

 QVector<QMatrix4x4> CloneGenerator::generateTransforms() const
 {
  QVector<QMatrix4x4> transforms;
  transforms.reserve(impl_->count_);
  for (int i = 0; i < impl_->count_; ++i) {
   QMatrix4x4 m;
   m.translate(i * impl_->spacing_, 0.0f, 0.0f);
   transforms.push_back(m);
  }
  return transforms;
 }

};
