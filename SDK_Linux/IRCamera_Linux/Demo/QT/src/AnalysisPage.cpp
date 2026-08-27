#include "AnalysisPage.h"
#include "MainWindow.h"

AnalysisPage::AnalysisPage(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    m_mainWindow = (MainWindow*)parent;
    InitForm();
    ConnectSignalSlot();
}

AnalysisPage::~AnalysisPage()
{}

void AnalysisPage::ClearUi()
{
}

void AnalysisPage::InitForm()
{
}

void AnalysisPage::ConnectSignalSlot()
{
    connect(m_mainWindow, SIGNAL(ChangeHandle(IRC_USB_HANDLE)), this, SLOT(OnChangeHandle(IRC_USB_HANDLE)));
}

void AnalysisPage::OnChangeHandle(IRC_USB_HANDLE handle)
{
    m_handle = handle;
}
