module;
#include <wobjectimpl.h>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
module Artifact.Widget.Dialog.RenderOutputSetting;


namespace Artifact
{
	
 class ArtifactRenderOutputSettingDialog::Impl
 {
 private:
 	
 public:
  Impl();
  ~Impl();
  QLineEdit* outputPathEdit = nullptr;
  QPushButton* browseButton = nullptr;
  QComboBox* formatCombo = nullptr;
  QComboBox* resolutionCombo = nullptr;
  QSpinBox* widthSpin = nullptr;
  QSpinBox* heightSpin = nullptr;
  QComboBox* fpsCombo = nullptr;
  QDialogButtonBox* buttonBox = nullptr;
  
  void handleBrowseClicked(ArtifactRenderOutputSettingDialog* dialog);
 };

 ArtifactRenderOutputSettingDialog::Impl::Impl()
 {

 }

 ArtifactRenderOutputSettingDialog::Impl::~Impl()
 {

 }

 void ArtifactRenderOutputSettingDialog::Impl::handleBrowseClicked(ArtifactRenderOutputSettingDialog* dialog)
 {
     QString filePath = QFileDialog::getSaveFileName(
         dialog,
         "Select Output File",
         "",
         "Video Files (*.mp4 *.mov *.avi);;All Files (*.*)"
     );
     if (!filePath.isEmpty()) {
         outputPathEdit->setText(filePath);
     }
 }
	
	W_OBJECT_IMPL(ArtifactRenderOutputSettingDialog)
	
 ArtifactRenderOutputSettingDialog::ArtifactRenderOutputSettingDialog(QWidget* parent /*= nullptr*/):QDialog(parent),impl_(new Impl())
 {
    setWindowTitle("Render Output Settings");
    setMinimumWidth(500);
    
    auto mainLayout = new QVBoxLayout(this);
    auto formLayout = new QFormLayout();
    
    // Output path
    impl_->outputPathEdit = new QLineEdit();
    impl_->browseButton = new QPushButton("Browse...");
    
    auto pathLayout = new QHBoxLayout();
    pathLayout->addWidget(impl_->outputPathEdit);
    pathLayout->addWidget(impl_->browseButton);
    
    formLayout->addRow("Output To:", pathLayout);

    // Format selection
    impl_->formatCombo = new QComboBox();
    impl_->formatCombo->addItem("MP4 (H.264)");
    impl_->formatCombo->addItem("MOV");
    impl_->formatCombo->addItem("AVI");
    impl_->formatCombo->addItem("Image Sequence (PNG)");
    formLayout->addRow("Format:", impl_->formatCombo);

    // Resolution presets + custom width/height
    impl_->resolutionCombo = new QComboBox();
    impl_->resolutionCombo->addItem("1920 x 1080");
    impl_->resolutionCombo->addItem("1280 x 720");
    impl_->resolutionCombo->addItem("Custom");

    impl_->widthSpin = new QSpinBox();
    impl_->widthSpin->setRange(1, 16384);
    impl_->heightSpin = new QSpinBox();
    impl_->heightSpin->setRange(1, 16384);
    impl_->widthSpin->setValue(1920);
    impl_->heightSpin->setValue(1080);
    impl_->widthSpin->setEnabled(false);
    impl_->heightSpin->setEnabled(false);

    auto resLayout = new QHBoxLayout();
    resLayout->addWidget(impl_->resolutionCombo);
    resLayout->addWidget(new QLabel("W:"));
    resLayout->addWidget(impl_->widthSpin);
    resLayout->addWidget(new QLabel("H:"));
    resLayout->addWidget(impl_->heightSpin);
    formLayout->addRow("Resolution:", resLayout);

    // Frame rate selection
    impl_->fpsCombo = new QComboBox();
    impl_->fpsCombo->addItem("23.976");
    impl_->fpsCombo->addItem("24");
    impl_->fpsCombo->addItem("25");
    impl_->fpsCombo->addItem("29.97");
    impl_->fpsCombo->addItem("30");
    impl_->fpsCombo->addItem("60");
    impl_->fpsCombo->setCurrentIndex(3); // default 29.97
    formLayout->addRow("Frame Rate:", impl_->fpsCombo);
    
    // OK/Cancel buttons
    impl_->buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(impl_->buttonBox);
    
    setLayout(mainLayout);
    
    // Connections
    QObject::connect(impl_->browseButton, &QPushButton::clicked, [this]() {
        impl_->handleBrowseClicked(this);
    });

    QObject::connect(impl_->resolutionCombo, &QComboBox::currentTextChanged, [this](const QString& text){
        if (text.contains("1920")) {
            impl_->widthSpin->setValue(1920);
            impl_->heightSpin->setValue(1080);
            impl_->widthSpin->setEnabled(false);
            impl_->heightSpin->setEnabled(false);
        } else if (text.contains("1280")) {
            impl_->widthSpin->setValue(1280);
            impl_->heightSpin->setValue(720);
            impl_->widthSpin->setEnabled(false);
            impl_->heightSpin->setEnabled(false);
        } else {
            impl_->widthSpin->setEnabled(true);
            impl_->heightSpin->setEnabled(true);
        }
    });
    
    QObject::connect(impl_->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(impl_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
 }

 ArtifactRenderOutputSettingDialog::~ArtifactRenderOutputSettingDialog()
 {
  delete impl_;
 }

};
