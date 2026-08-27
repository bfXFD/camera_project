#include "ImagePage.h"
#include "MainWindow.h"
#include "MsgBox.h"
#include <QFile>
#include <QFileDialog>

ImagePage::ImagePage(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    m_mainWindow = (MainWindow*)parent;
    InitForm();
    ConnectSignalSlot();
}

ImagePage::~ImagePage()
{}

void ImagePage::ClearUi()
{
    ui.palleteCombo->setCurrentIndex(-1);
    ui.imageEnhancementCombo->setCurrentIndex(-1);
    ui.snapBtn->setEnabled(false);
    ui.electronicMagnificationBtn->setEnabled(false);
    ui.saveImageDataButton->setEnabled(false);
    ui.detailEnhancementEdit->clear();
    ui.spatialDenoisingEdit->clear();
    ui.contrastEdit->clear();
    ui.brightnessEdit->clear();
    ui.timeDenosingEdit->clear();
    ui.dynamicEdit->clear();
    emit ChangeMagnificationFlag(false);
    ui.electronicMagnificationBtn->setText(tr("Open"));
}

void ImagePage::InitForm()
{
    ClearUi();
}

void ImagePage::ConnectSignalSlot()
{
    connect(m_mainWindow, SIGNAL(ChangeHandle(IRC_USB_HANDLE)), this, SLOT(OnChangeHandle(IRC_USB_HANDLE)));
    connect(m_mainWindow, SIGNAL(VideoCallBack(bool)), this, SLOT(OnVideoCallBack(bool)));
    connect(ui.palleteGetBtn, SIGNAL(clicked()), this, SLOT(OnPalleteGetBtnClicked()));
    connect(ui.palleteSetBtn, SIGNAL(clicked()), this, SLOT(OnPalleteSetBtnClicked()));
    connect(ui.shutterBtn, SIGNAL(clicked()), this, SLOT(OnShutterBtnClicked()));
    connect(ui.snapBtn, SIGNAL(clicked()), this, SLOT(OnSnapBtnClicked()));
    connect(ui.electronicMagnificationBtn, SIGNAL(clicked()), this, SLOT(OnElectronicMagnificationBtnClicked()));
    connect(ui.saveImageDataButton, SIGNAL(clicked()), this, SLOT(OnSaveImageDataBtnClicked()));
    connect(ui.imageEnhancementGetBtn, SIGNAL(clicked()), this, SLOT(OnImageEnhancementGetBtnClicked()));
    connect(ui.imageEnhancementSetBtn, SIGNAL(clicked()), this, SLOT(OnImageEnhancementSetBtnClicked()));
    connect(ui.detailEnhancementGetBtn, SIGNAL(clicked()), this, SLOT(OnDetailEnhancementGetBtnClicked()));
    connect(ui.detailEnhancementSetBtn, SIGNAL(clicked()), this, SLOT(OnDetailEnhancementSetBtnClicked()));
    connect(ui.spatialDenoisingGetBtn, SIGNAL(clicked()), this, SLOT(OnSpatialDenoisingGetBtnClicked()));
    connect(ui.spatialDenoisingSetBtn, SIGNAL(clicked()), this, SLOT(OnSpatialDenoisingSetBtnClicked()));
    connect(ui.spatialDenoisingGetBtn, SIGNAL(clicked()), this, SLOT(OnSpatialDenoisingGetBtnClicked()));
    connect(ui.spatialDenoisingSetBtn, SIGNAL(clicked()), this, SLOT(OnSpatialDenoisingSetBtnClicked()));
    connect(ui.contrastGetBtn, SIGNAL(clicked()), this, SLOT(OnContrastGetBtnClicked()));
    connect(ui.contrastSetBtn, SIGNAL(clicked()), this, SLOT(OnContrastSetBtnClicked()));
    connect(ui.brightnessGetBtn, SIGNAL(clicked()), this, SLOT(OnBrightnessGetBtnClicked()));
    connect(ui.brightnessSetBtn, SIGNAL(clicked()), this, SLOT(OnBrightnessSetBtnClicked()));
    connect(ui.timeDenosingGetBtn, SIGNAL(clicked()), this, SLOT(OnTimeDenosingGetBtnClicked()));
    connect(ui.timeDenosingSetBtn, SIGNAL(clicked()), this, SLOT(OnTimeDenosingSetBtnClicked()));
    connect(ui.dynamicGetBtn, SIGNAL(clicked()), this, SLOT(OnDynamicGetBtnClicked()));
    connect(ui.dynamicSetBtn, SIGNAL(clicked()), this, SLOT(OnDynamicSetBtnClicked()));
}

void ImagePage::InitImageParms(int level)
{
    if (level != 0)
    {
        ui.detailEnhancementEdit->setEnabled(false);
        ui.detailEnhancementGetBtn->setEnabled(false);
        ui.detailEnhancementSetBtn->setEnabled(false);
        ui.spatialDenoisingEdit->setEnabled(false);
        ui.spatialDenoisingGetBtn->setEnabled(false);
        ui.spatialDenoisingSetBtn->setEnabled(false);
    }
    else
    {
        ui.detailEnhancementEdit->setEnabled(true);
        ui.detailEnhancementGetBtn->setEnabled(true);
        ui.detailEnhancementSetBtn->setEnabled(true);
        ui.spatialDenoisingEdit->setEnabled(true);
        ui.spatialDenoisingGetBtn->setEnabled(true);
        ui.spatialDenoisingSetBtn->setEnabled(true);
    }
    OnDetailEnhancementGetBtnClicked();
    OnSpatialDenoisingGetBtnClicked();
    OnContrastGetBtnClicked();
    OnBrightnessGetBtnClicked();
    OnTimeDenosingGetBtnClicked();
    OnDynamicGetBtnClicked();
}

void ImagePage::OnVideoCallBack(bool flag)
{
    ui.snapBtn->setEnabled(flag);
    ui.electronicMagnificationBtn->setEnabled(flag);
    ui.saveImageDataButton->setEnabled(flag);
}

void ImagePage::OnPalleteGetBtnClicked()
{
    QString temp;
    int pallete;
    int err = IRC_USB_GetPalleteType(m_handle, &pallete);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get pallete type");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.palleteCombo->setCurrentIndex(pallete);
}

void ImagePage::OnPalleteSetBtnClicked()
{
    QString temp;
    int pallete = ui.palleteCombo->currentIndex();
    int err = IRC_USB_SetPalleteType(m_handle, pallete);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set pallete type");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnShutterBtnClicked()
{
    QString temp;
    int err = IRC_USB_CorrectShutter(m_handle);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Correction");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnSnapBtnClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Snapshot"), "", "*.jpg");
    if (fileName.isEmpty())
    {
        return;
    }
#ifdef _WIN32
 fileName = fileName;
#else
 fileName.append(".jpg");
#endif // _WIN32

    m_mainWindow->SetSnap(true, fileName);
}

void ImagePage::OnElectronicMagnificationBtnClicked()
{
    if (tr("Open") == ui.electronicMagnificationBtn->text())
    {
        emit ChangeMagnificationFlag(true);
        ui.electronicMagnificationBtn->setText(tr("Close"));
    }
    else
    {
        emit ChangeMagnificationFlag(false);
        ui.electronicMagnificationBtn->setText(tr("Open"));
    }
   
}

void ImagePage::OnSaveImageDataBtnClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), QDir::currentPath() + "/", "RAW File(*.raw)");
    if (fileName.isEmpty())
    {
        return;
    }
    if (!fileName.endsWith(".raw", Qt::CaseInsensitive))
    {
        fileName += ".raw";
    }
    m_mainWindow->SetSavaOneFrame(true, fileName);
}

void ImagePage::OnImageEnhancementGetBtnClicked()
{
    QString temp;
    int level;
    int err = IRC_USB_GetImageLevel(m_handle, &level);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get imgae enhancement");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.imageEnhancementCombo->setCurrentIndex(level);
    InitImageParms(level);
}

void ImagePage::OnImageEnhancementSetBtnClicked()
{
    QString temp;
    int level = ui.imageEnhancementCombo->currentIndex();
    int err = IRC_USB_SetImageLevel(m_handle, level);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set imgae enhancemen");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    InitImageParms(level);
}

void ImagePage::OnDetailEnhancementGetBtnClicked()
{
    QString temp;
    int value;
    int err = IRC_USB_GetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_DETAIL , &value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get detail enhancement");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.detailEnhancementEdit->setText(QString::number(value));
}

void ImagePage::OnDetailEnhancementSetBtnClicked()
{
    QString temp;
    int value = ui.detailEnhancementEdit->text().toInt();
    int err = IRC_USB_SetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_DETAIL, value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set detail enhancement");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnSpatialDenoisingGetBtnClicked()
{
    QString temp;
    int value;
    int err = IRC_USB_GetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_SPATIAL_DENOISING, &value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get spatial denoisong");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.spatialDenoisingEdit->setText(QString::number(value));
}

void ImagePage::OnSpatialDenoisingSetBtnClicked()
{
    QString temp;
    int value = ui.spatialDenoisingEdit->text().toInt();
    int err = IRC_USB_SetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_SPATIAL_DENOISING, value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set spatial denoisong");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnContrastGetBtnClicked()
{
    QString temp;
    int value;
    int err = IRC_USB_GetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_CONTRAST, &value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get contrast");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.contrastEdit->setText(QString::number(value));
}

void ImagePage::OnContrastSetBtnClicked()
{
    QString temp;
    int value = ui.contrastEdit->text().toInt();
    int err = IRC_USB_SetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_CONTRAST, value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set contrast");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnBrightnessGetBtnClicked()
{
    QString temp;
    int value;
    int err = IRC_USB_GetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_BRIGHTNESS, &value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get brightness");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.brightnessEdit->setText(QString::number(value));
}

void ImagePage::OnBrightnessSetBtnClicked()
{
    QString temp;
    int value = ui.brightnessEdit->text().toInt();
    int err = IRC_USB_SetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_BRIGHTNESS, value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set brightness");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnTimeDenosingGetBtnClicked()
{
    QString temp;
    int value;
    int err = IRC_USB_GetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_TIME_DENOISING, &value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get time denoising");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.timeDenosingEdit->setText(QString::number(value));
}

void ImagePage::OnTimeDenosingSetBtnClicked()
{
    QString temp;
    int value = ui.timeDenosingEdit->text().toInt();
    int err = IRC_USB_SetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_TIME_DENOISING, value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set time denoising");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnDynamicGetBtnClicked()
{
    QString temp;
    int value;
    int err = IRC_USB_GetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_DYNAMIC_RANGE, &value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get dynamic range");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.dynamicEdit->setText(QString::number(value));
}

void ImagePage::OnDynamicSetBtnClicked()
{
    QString temp;
    int value = ui.dynamicEdit->text().toInt();
    int err = IRC_USB_SetImageParam(m_handle, IRC_USB_IMAGE_PARAM_TYPE_DYNAMIC_RANGE, value);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set dynamic range");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void ImagePage::OnChangeHandle(IRC_USB_HANDLE handle)
{
    m_handle = handle;
}
