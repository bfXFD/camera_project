#pragma once

#include <QWidget>
#include "ui_GeneralPage.h"
#include "IRCUSBSDKDef.h"

class MainWindow;
class Worker : public QObject
{
    Q_OBJECT

public:
    Worker() {}
protected:
    bool SendDataLength(IRC_USB_HANDLE handle, unsigned char cmd, unsigned int dataLength);
    bool SendVerification(IRC_USB_HANDLE handle, unsigned char cmd, unsigned int dataLength);

public slots:
    void doWork(IRC_USB_HANDLE handle, int type, const QString& filePath);

signals:
    void Finished();
    void SendProgress(int current, int total);
    void SendMessage(QString msg);
};

class GeneralPage : public QWidget
{
    Q_OBJECT

public:
    GeneralPage(QWidget *parent = nullptr);
    ~GeneralPage();
    void ClearUi();

protected:
    void InitForm();
    void ConnectSignalSlot();

protected slots:
    void OnChangeHandle(IRC_USB_HANDLE handle);
    void OnFpaWidthBtnClicked();
    void OnFpaHeightBtnClicked();
    void OnFpaTempBtnClicked();
    void OnCameraTempBtnClicked();
    void OnSoftUpdateBtnClicked();
    void OnFirmUpdateBtnClicked();
    void OnParameterUpdateBtnClicked();
    void OnSerialSendBtnClicked();
    void OnUpdateProgress(int current, int total);
    void OnMessage(QString msg);

private:
    Ui::GeneralPageClass ui;
    MainWindow* m_mainWindow = nullptr;
    IRC_USB_HANDLE m_handle = 0;
};
