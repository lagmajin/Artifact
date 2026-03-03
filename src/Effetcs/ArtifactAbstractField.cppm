module;
module Artifact.Effect.Field;

namespace Artifact {

    ArtifactAbstractField::ArtifactAbstractField(FieldType type, const UniString& name)
        : type_(type), name_(name)
    {
        properties_ = std::make_shared<PropertyGroup>();
        properties_->setName(name.toQString());
    }
}
