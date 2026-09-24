module;
#include <utility>

module Artifact.Effect.Field;

import Memory.SharedPtr;

namespace Artifact {

    ArtifactAbstractField::ArtifactAbstractField(FieldType type, const UniString& name)
        : type_(type), name_(name)
    {
        properties_ = ArtifactCore::makeShared<PropertyGroup>();
        properties_->setName(name.toQString());
    }
}
