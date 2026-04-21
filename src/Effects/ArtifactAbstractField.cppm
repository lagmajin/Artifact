module;
#include <utility>

module Artifact.Effect.Field;

import std;

namespace Artifact {

    ArtifactAbstractField::ArtifactAbstractField(FieldType type, const UniString& name)
        : type_(type), name_(name)
    {
        properties_ = std::make_shared<PropertyGroup>();
        properties_->setName(name.toQString());
    }
}
