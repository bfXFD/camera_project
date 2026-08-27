#pragma once

#include <QWidget>
#include "ui_AnalysisPage.h"
#include "IRCUSBSDKDef.h"

class MainWindow;
class AnalysisPage : public QWidget
{
    Q_OBJECT

public:
    AnalysisPage(QWidget *parent = nullptr);
    ~AnalysisPage();
    void ClearUi();

protected:
    void InitForm();
    void ConnectSignalSlot();

protected slots:
    void OnChangeHandle(IRC_USB_HANDLE handle);

private:
    Ui::AnalysisPageClass ui;
    MainWindow* m_mainWindow = nullptr;
    IRC_USB_HANDLE m_handle = 0;
};
