module;


module Generator.Clone;


namespace Artifact
{
 class CloneGenerator::Impl
 {
 private:

 public:
  Impl();
  ~Impl();


 };

 CloneGenerator::Impl::Impl()
 {

 }

 CloneGenerator::Impl::~Impl()
 {

 }

 CloneGenerator::CloneGenerator() :impl_(new Impl())
 {

 }

 CloneGenerator::~CloneGenerator()
 {
    delete impl_;
 }


};