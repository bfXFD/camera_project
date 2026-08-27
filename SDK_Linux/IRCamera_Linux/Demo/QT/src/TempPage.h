#pragma once

#include <QWidget>
#include "ui_TempPage.h"
#include "IRCUSBSDKDef.h"

class MainWindow;
class TempPage : public QWidget
{
    Q_OBJECT

public:
    TempPage(QWidget *parent = nullptr);
    ~TempPage();
    void ClearUi();
    int GetTempUnit() { return m_tempUnit; }

protected:
    void InitForm();
    void ConnectSignalSlot();
    void RefreshEnvParam(IRC_USB_ENV_PARAM envParam);

protected slots:
    void OnChangeHandle(IRC_USB_HANDLE handle);
    void OnTempCallBack(bool flag);
    void OnTempUnitGetBtnClicked();
    void OnTempUnitSetBtnClicked();
    void OnGainGetBtnClicked();
    void OnGainSetBtnClicked();
    void OnOpenTempShowBtnClicked();
    void OnSaveTempDataBtnClicked();
    void OnEnvParamGetBtnClicked();
    void OnEnvParnSetBtnClicked();

private:
    Ui::TempPageClass ui;
    MainWindow* m_mainWindow = nullptr;
    IRC_USB_HANDLE m_handle = 0;
    int m_tempUnit = IRC_USB_TEMP_CENTIGRADE;
};
