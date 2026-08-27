#pragma once

#include <QWidget>
#include "ui_ImagePage.h"
#include "IRCUSBSDKDef.h"

class MainWindow;
class ImagePage : public QWidget
{
    Q_OBJECT

public:
    ImagePage(QWidget *parent = nullptr);
    ~ImagePage();
    void ClearUi();

protected:
    void InitForm();
    void ConnectSignalSlot();
    void InitImageParms(int level);

protected slots:
    void OnChangeHandle(IRC_USB_HANDLE handle);
    void OnVideoCallBack(bool flag);
    void OnPalleteGetBtnClicked();
    void OnPalleteSetBtnClicked();
    void OnShutterBtnClicked();
    void OnSnapBtnClicked();
    void OnElectronicMagnificationBtnClicked();
    void OnSaveImageDataBtnClicked();
    void OnImageEnhancementGetBtnClicked();
    void OnImageEnhancementSetBtnClicked();
    void OnDetailEnhancementGetBtnClicked();
    void OnDetailEnhancementSetBtnClicked();
    void OnSpatialDenoisingGetBtnClicked();
    void OnSpatialDenoisingSetBtnClicked();
    void OnContrastGetBtnClicked();
    void OnContrastSetBtnClicked();
    void OnBrightnessGetBtnClicked();
    void OnBrightnessSetBtnClicked();
    void OnTimeDenosingGetBtnClicked();
    void OnTimeDenosingSetBtnClicked();
    void OnDynamicGetBtnClicked();
    void OnDynamicSetBtnClicked();
signals:
    void ChangeMagnificationFlag(bool magnificationFlag);
private:
    Ui::ImagePageClass ui;
    MainWindow* m_mainWindow = nullptr;
    IRC_USB_HANDLE m_handle = 0;
};
