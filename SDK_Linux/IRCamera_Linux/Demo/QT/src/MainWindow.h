#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"
#include "ImagePage.h"
#include "TempPage.h"
#include "AnalysisPage.h"
#include "GeneralPage.h"
#include "IRCUSBSDK.h"
#include "IRCUSBSDKDef.h"
#include <QMutex>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void StopCallback();
    void SetTempShowFlag(bool flag);
    void SetSavaOneTemp(bool flag, QString path);
    void SetSavaOneFrame(bool flag, QString path);
    void SetSnap(bool flag, QString path);

protected:
    void InitForm();
    void InitPages();
    void RefreshPagesUi();
    void ConnectSignalSlot();
    void HandleDevSearchInfo(IRC_USB_DEV_INFO* searchInfo);

    static void ExceptionCallback(IRC_USB_HANDLE handle, int exceptionType, void* userData);
    static void DevSearchCallback(IRC_USB_DEV_INFO* searchInfo, void* userData);
    static void FrameCallback(IRC_USB_HANDLE handle, IRC_USB_VIDEO_INFO_CB* videoInfo, void* userData);
    static void TempCallback(IRC_USB_HANDLE handle, IRC_USB_TEMP_INFO_CB* tempInfo, void* userData);

protected slots:
    void OnDevSearched(const QString& devName);
    void OnException(int type);
    void OnUpdateFrame();
    void OnUpdateTemp(bool flag);
    void OnSearchBtnClicked();
    void OnConnectDevBtnClicked();
    void OnPreviewBtnClicked();
    void OnPullTempBtnClicked();
    void OnChangeMagnificationFlag(bool magnificationFlag);


signals:
    void DevSearched(const QString& devName);
    void ChangeHandle(IRC_USB_HANDLE handle);
    void Exception(int type);
    void UpdateFrame();
    void UpdateTemp(bool flag);
    void TempCallBack(bool flag);
    void VideoCallBack(bool flag);

private:
    Ui::MainWindowClass ui;
    IRC_USB_HANDLE m_handle = 0;
    ImagePage* m_imagePage = nullptr;
    TempPage* m_tempPage = nullptr;
    AnalysisPage* m_analysisPage = nullptr;
    GeneralPage* m_generalPage = nullptr;
    QImage m_img;
    QMutex m_mutex;
    QMutex m_tempMutex;
    int m_width;
    int m_height;
    bool m_openTempShow = false;
    bool m_saveOneTemp = false;
    QString m_oneTempPath;
    bool m_saveOneFrame = false;
    QString m_oneFramePath;
    bool m_sanp = false;
    QString m_snapPath;

};
