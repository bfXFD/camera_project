#include "GeneralPage.h"
#include "MainWindow.h"
#include "MsgBox.h"
#include <QThread>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>

unsigned char RAY_AAL_CKSUM(unsigned char* buf, int len)
{
    unsigned char cksum = 0;
    int i;

    for (i = 0; i < len; i++)
    {
        cksum += buf[i];
    }

    return cksum;
}

unsigned int RAY_UINT_CKSUM(unsigned char* buf, int len)
{
    unsigned int cksum = 0;
    int i;

    for (i = 0; i < len; i++)
    {
        cksum += buf[i];
    }

    return cksum;
}

GeneralPage::GeneralPage(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    m_mainWindow = (MainWindow*)parent;
    InitForm();
    ConnectSignalSlot();
}

GeneralPage::~GeneralPage()
{}

void GeneralPage::ClearUi()
{
    ui.fpaWidthEdit->clear();
    ui.fpaHeightEdit->clear();
    ui.fpaTempEdit->clear();
    ui.cameraTempEdit->clear();
    ui.receiveEdit->clear();
    ui.sendEdit->clear();
    ui.timeOutEdit->setText("1000");
    ui.processLabel->clear();
}

void GeneralPage::InitForm()
{
    QRegExp rx("^[1-9]\\d*$");
    QRegExpValidator* validator = new QRegExpValidator(rx, ui.timeOutEdit);
    ui.timeOutEdit->setValidator(validator);
    ui.timeOutEdit->setText("1000");
}

void GeneralPage::ConnectSignalSlot()
{
    connect(m_mainWindow, SIGNAL(ChangeHandle(IRC_USB_HANDLE)), this, SLOT(OnChangeHandle(IRC_USB_HANDLE)));
    connect(ui.fpaWidthBtn, SIGNAL(clicked()), this, SLOT(OnFpaWidthBtnClicked()));
    connect(ui.fpaHeightBtn, SIGNAL(clicked()), this, SLOT(OnFpaHeightBtnClicked()));
    connect(ui.fpaTempBtn, SIGNAL(clicked()), this, SLOT(OnFpaTempBtnClicked()));
    connect(ui.cameraTempBtn, SIGNAL(clicked()), this, SLOT(OnCameraTempBtnClicked()));
    connect(ui.softUpdateBtn, SIGNAL(clicked()), this, SLOT(OnSoftUpdateBtnClicked()));
    connect(ui.firmUpdateBtn, SIGNAL(clicked()), this, SLOT(OnFirmUpdateBtnClicked()));
    connect(ui.parameterUpdateBtn, SIGNAL(clicked()), this, SLOT(OnParameterUpdateBtnClicked()));
    connect(ui.serialSendBtn, SIGNAL(clicked()), this, SLOT(OnSerialSendBtnClicked()));
}

void GeneralPage::OnFpaWidthBtnClicked()
{
    QString temp;
    int width;
    int height;
    int err = IRC_USB_GetFPASize(m_handle, &width, &height);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get FPA width");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.fpaWidthEdit->setText(QString::number(width));
}

void GeneralPage::OnFpaHeightBtnClicked()
{
    QString temp;
    int width;
    int height;
    int err = IRC_USB_GetFPASize(m_handle, &width, &height);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get FPA height");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.fpaHeightEdit->setText(QString::number(height));
}

void GeneralPage::OnFpaTempBtnClicked()
{
    QString temp;
    float temperature;
    int err = IRC_USB_GetFPATemp(m_handle, &temperature);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get FPA temp");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.fpaTempEdit->setText(QString::number(temperature));
}

void GeneralPage::OnCameraTempBtnClicked()
{
    QString temp;
    float temperature;
    int err = IRC_USB_GetCameraTemp(m_handle, &temperature);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get camera temp");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.cameraTempEdit->setText(QString::number(temperature));
}

void GeneralPage::OnSoftUpdateBtnClicked()
{
    m_mainWindow -> StopCallback();
    QString fileName = QFileDialog::getOpenFileName(
        nullptr,
        "Choose File",
        QDir::homePath(), // ��ʼĿ¼
        "BIN Files (*.bin);;DAT Files (*.dat)"
    );
    IRC_USB_HANDLE handle = m_handle;
    if (fileName.isEmpty())
    {
        return;
    }
    QThread* thread = new QThread;
    Worker* worker = new Worker;
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, [worker, handle, fileName]() mutable {
        worker->doWork(handle, 0, fileName);
        });
    connect(worker, &Worker::SendProgress, this, &GeneralPage::OnUpdateProgress);
    connect(worker, &Worker::SendMessage, this, &GeneralPage::OnMessage);
    connect(worker, &Worker::Finished, thread, &QThread::quit);
    connect(worker, &Worker::Finished, worker, &Worker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void GeneralPage::OnFirmUpdateBtnClicked()
{
    m_mainWindow->StopCallback();
    QString fileName = QFileDialog::getOpenFileName(
        nullptr,
        "Choose File",
        QDir::homePath(), // ��ʼĿ¼
        "BIN Files (*.bin);;DAT Files (*.dat)"
    );
    IRC_USB_HANDLE handle = m_handle;
    if (fileName.isEmpty())
    {
        return;
    }
    QThread* thread = new QThread;
    Worker* worker = new Worker;
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, [worker, handle, fileName]() mutable {
        worker->doWork(handle, 1, fileName);
        });
    connect(worker, &Worker::SendProgress, this, &GeneralPage::OnUpdateProgress);
    connect(worker, &Worker::SendMessage, this, &GeneralPage::OnMessage);
    connect(worker, &Worker::Finished, thread, &QThread::quit);
    connect(worker, &Worker::Finished, worker, &Worker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void GeneralPage::OnParameterUpdateBtnClicked()
{
    m_mainWindow->StopCallback();
    QString fileName = QFileDialog::getOpenFileName(
        nullptr,
        "Choose File",
        QDir::homePath(), // ��ʼĿ¼
        "BIN Files (*.bin);;DAT Files (*.dat)"
    );
    IRC_USB_HANDLE handle = m_handle;
    if (fileName.isEmpty())
    {
        return;
    }
    QThread* thread = new QThread;
    Worker* worker = new Worker;
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, [worker, handle, fileName]() mutable {
        worker->doWork(handle, 2, fileName);
        });
    connect(worker, &Worker::SendProgress, this, &GeneralPage::OnUpdateProgress);
    connect(worker, &Worker::SendMessage, this, &GeneralPage::OnMessage);
    connect(worker, &Worker::Finished, thread, &QThread::quit);
    connect(worker, &Worker::Finished, worker, &Worker::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void GeneralPage::OnSerialSendBtnClicked()
{
    QString temp;
    QString cmd = ui.sendEdit->text();
    QByteArray hexCmd = QByteArray::fromHex(cmd.toLatin1());
    int err = IRC_USB_WritePort(m_handle, hexCmd.data(), hexCmd.size());
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Send cmd");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    QThread::msleep(100);
    char recvBuf[512];
    int outSize = 0;
    QString timeout = ui.timeOutEdit->text();
    err = IRC_USB_ReadPort(m_handle, recvBuf, 512, &outSize, timeout.toInt());
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Read cmd");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    QByteArray receivedData(recvBuf, outSize);
    QString hexString = receivedData.toHex(' ').toUpper();
    ui.receiveEdit->setText(hexString);
}

void GeneralPage::OnUpdateProgress(int current, int total)
{
    if (1 == total && 1 == current)
    {
        ui.processLabel->setText("Update completed");
    }
    else if (0 == total && 0 == current)
    {
        ui.processLabel->setText("Updating, please do not operate, verification in progress");
    }
    else
    {
        ui.processLabel->setText("Updating, please do not operate, " + QString::number(current) + "/" + QString::number(total));
    }
}

void GeneralPage::OnMessage(QString msg)
{
    MSG_BOX.ShowErrorMessage(msg, IRC_USB_ERROR_FAILED);
}


void GeneralPage::OnChangeHandle(IRC_USB_HANDLE handle)
{
    m_handle = handle;
}

bool Worker::SendDataLength(IRC_USB_HANDLE handle, unsigned char cmd, unsigned int dataLength)
{
    unsigned char szCmd[0x0D] =
    {
        0xAA, 0x09,0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEB, 0xAA
    };
    szCmd[3] = cmd;
    szCmd[5] = dataLength & 0xFF;
    szCmd[6] = (dataLength >> 8) & 0xFF;
    szCmd[7] = (dataLength >> 16) & 0xFF;
    szCmd[8] = (dataLength >> 24) & 0xFF;
    szCmd[10] = RAY_AAL_CKSUM(szCmd, 10);
    if (IRC_USB_WritePort(handle, (char*)szCmd, sizeof(szCmd)) != IRC_USB_ERROR_OK)
    {
        return false;
    }
    QThread::msleep(100);
    int recvCnt = 0;
    unsigned char recvBuf[512] = { 0 };
    if (IRC_USB_ReadPort(handle, (char*)recvBuf, 512, &recvCnt, 10000) != IRC_USB_ERROR_OK)
    {
        return false;
    }
    if (!(recvCnt > 3 &&
        recvBuf[0] == 0x55 &&
        recvBuf[2] == cmd  &&
        recvBuf[4] == 0x01))
    {
        return false;
    }
    return true;
}

bool Worker::SendVerification(IRC_USB_HANDLE handle, unsigned char cmd, unsigned int dataLength)
{
    emit SendProgress(0, 0);
    unsigned char szCmd[0x0C] =
    {
        0xAA, 0x08,0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEB, 0xAA
    };
    szCmd[3] = cmd;
    szCmd[5] = dataLength & 0xFF;
    szCmd[6] = (dataLength >> 8) & 0xFF;
    szCmd[7] = (dataLength >> 16) & 0xFF;
    szCmd[8] = (dataLength >> 24) & 0xFF;
    szCmd[9] = RAY_AAL_CKSUM(szCmd, 9);
    if (IRC_USB_WritePort(handle, (char*)szCmd, sizeof(szCmd)) != IRC_USB_ERROR_OK)
    {
        return false;
    }
    //Receive Data
    int recvCnt = 0;
    unsigned char recvBuf[512] = { 0 };
    if (IRC_USB_ReadPort(handle, (char*)recvBuf, 512, &recvCnt, 1000*60*5) != IRC_USB_ERROR_OK)
    {
        return false;
    }
    if (!(recvCnt > 3 &&
        recvBuf[0] == 0x55 &&
        recvBuf[2] == cmd  &&
        recvBuf[4] == 0x01))
    {
        return false;
    }
    return true;
}

void Worker::doWork(IRC_USB_HANDLE handle, int type, const QString& filePath)
{
    QFile file(filePath);
    QString errorStr;
    if (!file.open(QIODevice::ReadOnly))
    {
        errorStr = tr("Open file");
        emit SendMessage(errorStr);
        emit Finished();
        return;
    }
    QFileInfo fileInfo(file);
    unsigned long int fileLen = fileInfo.size();
    unsigned char cmd = 0x00;
    if (0 == type)
    {
        cmd = 0xbb;
    }
    else if (1 == type)
    {
        cmd = 0x8f;
    }
    else if (2 == type)
    {
        cmd = 0x81;
    }
    //������������
    if (!SendDataLength(handle, cmd, fileLen))
    {
        errorStr = tr("Send data length");
        emit SendMessage(errorStr);
        file.close();
        emit Finished();
        return;
    }

    //ѭ��д������
    unsigned int ckSum = 0;
    unsigned int process = 0;
    while (!file.atEnd())
    {
        QByteArray buffer = file.read(512);
        if (buffer.isEmpty()) {
            errorStr = tr("Read file");
            emit SendMessage(errorStr);
            file.close();
            emit Finished();
            return;
        }
        if (IRC_USB_WritePort(handle, (char*)(buffer.data()), buffer.size()) != IRC_USB_ERROR_OK)
        {
            errorStr = tr("Write file");
            emit SendMessage(errorStr);
            file.close();
            emit Finished();
            return;
        }
        process += buffer.size();
        ckSum += RAY_UINT_CKSUM((unsigned char*)buffer.data(), buffer.size());
        emit SendProgress(process, fileLen);
        QThread::msleep(50);
    }

    //���������Ƿ�д��ɹ�
    unsigned char recvBuf[512] = { 0 };
    int outSize = 0;
    if (IRC_USB_ReadPort(handle, (char*)recvBuf, 512, &outSize, 1000) != IRC_USB_ERROR_OK)
    {
        errorStr = tr("Write file");
        emit SendMessage(errorStr);
        file.close();
        emit Finished();
        return;
    }
    if (!(outSize > 3 &&
        recvBuf[0] == 0x55 &&
        recvBuf[2] == cmd  &&
        recvBuf[4] == 0x01))
    {
        errorStr = tr("Write file");
        emit SendMessage(errorStr);
        file.close();
        emit Finished();
        return;
    }

    //У��
    if (!SendVerification(handle, cmd, ckSum))
    {
        errorStr = tr("Send verification");
        emit SendMessage(errorStr);
        emit Finished();
        return;
    }
    emit SendProgress(1, 1);
    emit Finished();
}
