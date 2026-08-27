#include "MainWindow.h"
#include "MsgBox.h"
#include "opencv2/opencv.hpp"

unsigned short m_tempData[1280 * 1024];

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    InitForm();
    ConnectSignalSlot();
}

MainWindow::~MainWindow()
{
    if (tr("DisconnectDevice") == ui.connectDevBtn->text())
    {
        OnConnectDevBtnClicked();
    }
}

void MainWindow::StopCallback()
{
    if (tr("StopPreview") == ui.previewBtn->text())
    {
        IRC_USB_StopPreview(m_handle);
        ui.previewBtn->setText(tr("StartPreview"));
        emit VideoCallBack(false);
    }

    if (tr("StopPullTemp") == ui.pullTempBtn->text())
    {
        IRC_USB_StopPullTemp(m_handle);
        ui.pullTempBtn->setText(tr("StartPullTemp"));
        emit TempCallBack(false);
    }
}

void MainWindow::SetTempShowFlag(bool flag)
{
    m_openTempShow = flag;
}

void MainWindow::SetSavaOneTemp(bool flag, QString path)
{
    m_saveOneTemp = flag;
    m_oneTempPath = path;
}

void MainWindow::SetSavaOneFrame(bool flag, QString path)
{
    m_saveOneFrame = flag;
    m_oneFramePath = path;
}

void MainWindow::SetSnap(bool flag, QString path)
{
    m_sanp = flag;
    m_snapPath = path;
}

void MainWindow::InitForm()
{
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setFixedSize(width(), height());
    ui.tabWidget->setEnabled(false);
    ui.devNameComboBox->setEditable(false);
    ui.searthBtn->setEnabled(true);
    ui.pullTempBtn->setEnabled(false);
    ui.previewBtn->setEnabled(false);
    InitPages();
}

void MainWindow::InitPages()
{
    m_imagePage = new ImagePage(this);
    m_tempPage = new TempPage(this);
    //m_analysisPage = new AnalysisPage(this);
    m_generalPage = new GeneralPage(this);
    ui.tabWidget->addTab(m_imagePage, tr("Image"));
    ui.tabWidget->addTab(m_tempPage, tr("Temp"));
    //ui.tabWidget->addTab(m_analysisPage, tr("Analysis"));
    ui.tabWidget->addTab(m_generalPage, tr("General"));
    ui.tabWidget->setCurrentIndex(0);
}

void MainWindow::RefreshPagesUi()
{
    m_imagePage->ClearUi();
    m_tempPage->ClearUi();
    m_analysisPage->ClearUi();
    m_generalPage->ClearUi();
    ui.videoWidget->ClearUi();

}

void MainWindow::ConnectSignalSlot()
{
    connect(this, SIGNAL(DevSearched(const QString&)), this, SLOT(OnDevSearched(const QString&)));
    connect(this, SIGNAL(UpdateFrame()), this, SLOT(OnUpdateFrame()));
    connect(this, SIGNAL(UpdateTemp(bool)), this, SLOT(OnUpdateTemp(bool)));
    connect(ui.searthBtn, SIGNAL(clicked()), this, SLOT(OnSearchBtnClicked()));
    connect(this, SIGNAL(Exception(int)), this, SLOT(OnException(int)));
    connect(ui.connectDevBtn, SIGNAL(clicked()), this, SLOT(OnConnectDevBtnClicked()));
    connect(ui.previewBtn, SIGNAL(clicked()), this, SLOT(OnPreviewBtnClicked()));
    connect(ui.pullTempBtn, SIGNAL(clicked()), this, SLOT(OnPullTempBtnClicked()));
    connect(m_imagePage, SIGNAL(ChangeMagnificationFlag(bool)), this, SLOT(OnChangeMagnificationFlag(bool)));
}

void MainWindow::HandleDevSearchInfo(IRC_USB_DEV_INFO* searchInfo)
{
    if (searchInfo != nullptr)
    {
        QString devName = searchInfo->devName;
        QString portName = searchInfo->portName;
#ifndef _WIN32
        if(!portName.contains("dev"))
        {
            portName = "";
        }
#endif // _WIN32
        emit DevSearched(devName + "-" + portName);
    }
}

void MainWindow::ExceptionCallback(IRC_USB_HANDLE handle, int exceptionType, void* userData)
{
    qDebug() << "exception" << exceptionType;
    MainWindow* thisPtr = static_cast<MainWindow*>(userData);
    thisPtr->Exception(exceptionType);
}

void MainWindow::DevSearchCallback(IRC_USB_DEV_INFO* searchInfo, void* userData)
{
    MainWindow* thisPtr = static_cast<MainWindow*>(userData);
    thisPtr->HandleDevSearchInfo(searchInfo);
}

void MainWindow::FrameCallback(IRC_USB_HANDLE handle, IRC_USB_VIDEO_INFO_CB* videoInfo, void* userData)
{
    MainWindow* page = static_cast<MainWindow*>(userData);
    QMutexLocker locker(&page->m_mutex);

    if (page->m_saveOneFrame)
    {
        QFile file(page->m_oneFramePath);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(videoInfo->dataBuf, videoInfo->width * videoInfo->height * 2);
            file.close();
        }
        page->m_saveOneFrame = false;
    }

    cv::Mat mat = cv::Mat(videoInfo->height, videoInfo->width, CV_8UC2, videoInfo->dataBuf);
    if (mat.empty())
    {
        return;
    }
    if (IRC_NET_FRAME_FMT_YUYV == videoInfo->frameFmt)
    {
        cv::cvtColor(mat, mat, cv::COLOR_YUV2BGR_YUYV);
    }
    else if (IRC_NET_FRAME_FMT_UYVY == videoInfo->frameFmt)
    {
        cv::cvtColor(mat, mat, cv::COLOR_YUV2BGR_UYVY);
    }
    else if (IRC_NET_FRAME_FMT_RAW == videoInfo->frameFmt)
    {
        return;
    }
    else
    {
        return;
    }
    page->m_img = QImage(mat.data, mat.cols, mat.rows, mat.cols * 3, QImage::Format_RGB888).rgbSwapped();
    page->UpdateFrame();
    if (page->m_sanp)
    {
        page->m_img.save(page->m_snapPath);
        page->m_sanp = false;
    }
}

void MainWindow::TempCallback(IRC_USB_HANDLE handle, IRC_USB_TEMP_INFO_CB* tempInfo, void* userData)
{
    MainWindow* page = static_cast<MainWindow*>(userData);
    QMutexLocker locker(&page->m_tempMutex);
    page->m_width = tempInfo->width;
    page->m_height = tempInfo->height;
    memcpy(&m_tempData[0], tempInfo->dataBuf, tempInfo->width * tempInfo->height * 2);
    if (page->m_openTempShow)
    {
        page->UpdateTemp(true);
    }
    else
    {
        page->UpdateTemp(false);
    }
    if (page->m_saveOneTemp)
    {
        if (".raw" == page->m_oneTempPath.right(4))
        {
            QFile file(page->m_oneTempPath);
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(tempInfo->dataBuf, tempInfo->width * tempInfo->height * 2);
                file.close();
            }
        }
        else
        {
            QFile file(page->m_oneTempPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                QTextStream textStream(&file);
                short tempValue = 0;
                int width = tempInfo->width;
                int height = tempInfo->height;
                for(int y = 0; y < height; y++)
                {
                    for(int x = 0; x< width; x++)
                    {
                        int index = y*width + x;
                        memcpy(&tempValue, &tempInfo->dataBuf[2*index], 2);
                        textStream << QString("%1").arg(QString::number(tempValue));
                        if(x < width - 1)
                        {
                            textStream << QString(", ");
                        }
                    }
                    textStream << "\n";
                }
            }
        }
        page->m_saveOneTemp = false;
    }
}

void MainWindow::OnDevSearched(const QString& devName)
{
    ui.devNameComboBox->addItem(devName);
}

void MainWindow::OnException(int type)
{
    switch (type)
    {
    case IRC_USB_EXCEPTION_DEV_OFFLINE:
        QMessageBox::critical(this, tr("Exception"), QString::number(type) + tr(":Dev offline."));
        break;
    case IRC_USB_EXCEPTION_PREVIEW_OFFLINE:
        QMessageBox::critical(this, tr("Exception"), QString::number(type) + tr(":Preview offline."));
        break;
    case IRC_USB_EXCEPTION_TEMP_OFFLINE:
        QMessageBox::critical(this, tr("Exception"), QString::number(type) + tr(":Temp offline."));
        break;
    default:
        break;
    }
}

void MainWindow::OnUpdateFrame()
{
    ui.videoWidget->SetImg(m_img);
}

void MainWindow::OnUpdateTemp(bool flag)
{
    int unit = m_tempPage->GetTempUnit();
    ui.videoWidget->SetTempData(m_tempData, m_width, m_height, flag, unit);
}

void MainWindow::OnSearchBtnClicked()
{
    ui.devNameComboBox->clear();
    int err = IRC_USB_SearchDev(DevSearchCallback, this);
    QString temp = tr("Search Dev");
    if (IRC_USB_ERROR_OK != err)
    {
        MSG_BOX.ShowErrorMessage(temp, err);
    }
}

void MainWindow::OnConnectDevBtnClicked()
{
    QString temp;
    if (tr("ConnectDevice") == ui.connectDevBtn->text())
    {
        int id = ui.devNameComboBox->currentIndex();
        if (id < 0)
        {
            temp = tr("Please search device first.");
            MSG_BOX.ShowErrorMessage(temp, -1);
            return;
        }
        int err = IRC_USB_OpenDev(id, &m_handle);
        if (IRC_USB_ERROR_OK != err)
        {
            temp = tr("ConnectDevice");
            MSG_BOX.ShowErrorMessage(temp, err);
            return;
        }
        err = IRC_USB_SetExceptionCallback(m_handle, ExceptionCallback, this);
        temp = tr("SetExceptionCallback");
        if (IRC_USB_ERROR_OK != err)
        {
            MSG_BOX.ShowErrorMessage(temp, err);
            return;
        }
        ui.tabWidget->setEnabled(true);
        ui.searthBtn->setEnabled(false);
        ui.pullTempBtn->setEnabled(true);
        ui.previewBtn->setEnabled(true);
        ui.connectDevBtn->setText(tr("DisconnectDevice"));
    }
    else
    {
        StopCallback();
        int err = IRC_USB_CloseDev(m_handle);
        if (IRC_USB_ERROR_OK != err)
        {
            temp = tr("DisconnectDevice");
            MSG_BOX.ShowErrorMessage(temp, err);
            return;
        }
        ui.tabWidget->setEnabled(false);
        ui.searthBtn->setEnabled(true);
        ui.pullTempBtn->setEnabled(false);
        ui.previewBtn->setEnabled(false);
        RefreshPagesUi();
        ui.connectDevBtn->setText(tr("ConnectDevice"));
    }
    emit ChangeHandle(m_handle);
}

void MainWindow::OnPreviewBtnClicked()
{
    if (tr("StartPreview") == ui.previewBtn->text())
    {
        int err = IRC_USB_StartPreview(m_handle, FrameCallback, this);
        QString temp = tr("StartPreview");
        if (IRC_USB_ERROR_OK != err)
        {
            MSG_BOX.ShowErrorMessage(temp, err);
            return;
        }
        ui.previewBtn->setText(tr("StopPreview"));
        emit VideoCallBack(true);
    }
    else
    {
        int err = IRC_USB_StopPreview(m_handle);
        QString temp = tr("StopPreview");
        if (IRC_USB_ERROR_OK != err)
        {
            MSG_BOX.ShowErrorMessage(temp, err);
            return;

        }
        ui.previewBtn->setText(tr("StartPreview"));
        emit VideoCallBack(false);

    }
}

void MainWindow::OnPullTempBtnClicked()
{
    if (tr("StartPullTemp") == ui.pullTempBtn->text())
    {
        int err = IRC_USB_StartPullTemp(m_handle, TempCallback, this);
        QString temp = tr("StartPullTemp");
        if (IRC_USB_ERROR_OK != err)
        {
            MSG_BOX.ShowErrorMessage(temp, err);
            return;
        }
        ui.pullTempBtn->setText(tr("StopPullTemp"));
        emit TempCallBack(true);
    }
    else
    {
        int err = IRC_USB_StopPullTemp(m_handle);
        QString temp = tr("StopPullTemp");
        if (IRC_USB_ERROR_OK != err)
        {
            MSG_BOX.ShowErrorMessage(temp, err);
            return;
        }
        ui.pullTempBtn->setText(tr("StartPullTemp"));
        emit TempCallBack(false);
    }
}

void MainWindow::OnChangeMagnificationFlag(bool magnificationFlag)
{
    ui.videoWidget->SetMagnificationFlag(magnificationFlag);
}
