module;

module Menu.Help;





namespace Artifact {

 class ArtifactHelpMenu::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 ArtifactHelpMenu::Impl::Impl()
 {

 }

 ArtifactHelpMenu::Impl::~Impl()
 {

 }

 ArtifactHelpMenu::ArtifactHelpMenu(QWidget* parent /*= nullptr*/):QMenu(parent),impl_(new Impl())
 {

 }

 ArtifactHelpMenu::~ArtifactHelpMenu()
 {
  delete impl_;
 }

};