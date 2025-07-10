module;
#include <QWidget>
export module ApplicationSettingDialog;

export namespace ArtifactCore {

 class LabelColorSettingWidget :public QWidget {
 private:
  //LabelColorSettingWidgetPrivate* const	pWidget_;
 public:
  explicit LabelColorSettingWidget(const QString& labelname, const QColor& color, QWidget* parent = NULL);
  ~LabelColorSettingWidget();
 };


 class MemoryAndCpuSettingPage :public QWidget {
  //Q_OBJECT
 private:

 protected:
 public:
  explicit MemoryAndCpuSettingPage(QWidget* parent = nullptr);
  virtual ~MemoryAndCpuSettingPage();
  int cpuCount() const;
  void resetSetting();

 };








};