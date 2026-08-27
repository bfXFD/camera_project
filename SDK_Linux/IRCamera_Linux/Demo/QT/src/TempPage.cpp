#include "TempPage.h"
#include "MainWindow.h"
#include "MsgBox.h"
#include <QFile>
#include <QFileDialog>

TempPage::TempPage(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    m_mainWindow = (MainWindow*)parent;
    InitForm();
    ConnectSignalSlot();
}

TempPage::~TempPage()
{}

void TempPage::ClearUi()
{
    ui.tempUnitcombo->setCurrentIndex(-1);
    ui.gainCombo->setCurrentIndex(-1);
    ui.openTempShowBtn->setEnabled(false);
    ui.saveTempDataBtn->setEnabled(false);
    ui.reflTempEdit->clear();
    ui.atmTempEdit->clear();
    ui.atmTranEdit->clear();
    ui.emissivityEdit->clear();
    ui.distanceEdit->clear();
    m_mainWindow->SetTempShowFlag(false);
    ui.openTempShowBtn->setText(tr("OpenTempShow"));
}

void TempPage::InitForm()
{
    ClearUi();
}

void TempPage::ConnectSignalSlot()
{
    connect(m_mainWindow, SIGNAL(ChangeHandle(IRC_USB_HANDLE)), this, SLOT(OnChangeHandle(IRC_USB_HANDLE)));
    connect(m_mainWindow, SIGNAL(TempCallBack(bool)), this, SLOT(OnTempCallBack(bool)));
    connect(ui.tempUnitGetBtn, SIGNAL(clicked()), this, SLOT(OnTempUnitGetBtnClicked()));
    connect(ui.tempUnitSetBtn, SIGNAL(clicked()), this, SLOT(OnTempUnitSetBtnClicked()));
    connect(ui.gainGetBtn, SIGNAL(clicked()), this, SLOT(OnGainGetBtnClicked()));
    connect(ui.gainSetBtn, SIGNAL(clicked()), this, SLOT(OnGainSetBtnClicked()));
    connect(ui.openTempShowBtn, SIGNAL(clicked()), this, SLOT(OnOpenTempShowBtnClicked()));
    connect(ui.saveTempDataBtn, SIGNAL(clicked()), this, SLOT(OnSaveTempDataBtnClicked()));
    connect(ui.envParamGetBtn, SIGNAL(clicked()), this, SLOT(OnEnvParamGetBtnClicked()));
    connect(ui.envParamSetBtn, SIGNAL(clicked()), this, SLOT(OnEnvParnSetBtnClicked()));
}

void TempPage::RefreshEnvParam(IRC_USB_ENV_PARAM envParam)
{
    ui.reflTempEdit->setText(QString::number(envParam.reflectedTemp));
    ui.atmTempEdit->setText(QString::number(envParam.atmosphereTemp));
    ui.atmTranEdit->setText(QString::number(envParam.transmittance));
    ui.emissivityEdit->setText(QString::number(envParam.emissivity));
    ui.distanceEdit->setText(QString::number(envParam.distance));
}

void TempPage::OnTempCallBack(bool flag)
{
    ui.openTempShowBtn->setEnabled(flag);
    ui.saveTempDataBtn->setEnabled(flag);
}

void TempPage::OnTempUnitGetBtnClicked()
{
    QString temp;
    int unit;
    int err = IRC_USB_GetTempUnit(m_handle, &unit);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get temp unit");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    m_tempUnit = unit;
    ui.tempUnitcombo->setCurrentIndex(unit);
}

void TempPage::OnTempUnitSetBtnClicked()
{
    QString temp;
    int unit = ui.tempUnitcombo->currentIndex();
    int err = IRC_USB_SetTempUnit(m_handle, unit);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set temp unit");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    m_tempUnit = unit;
}

void TempPage::OnGainGetBtnClicked()
{
    QString temp;
    int gain;
    int err = IRC_USB_GetTempLevel(m_handle, &gain);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get gain type");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    ui.gainCombo->setCurrentIndex(gain);
}

void TempPage::OnGainSetBtnClicked()
{
    QString temp;
    int gain = ui.gainCombo->currentIndex();
    int err = IRC_USB_SeTempLevel(m_handle, gain);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set gain type");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void TempPage::OnOpenTempShowBtnClicked()
{
    if (tr("OpenTempShow") == ui.openTempShowBtn->text())
    {
        m_mainWindow->SetTempShowFlag(true);
        ui.openTempShowBtn->setText(tr("CloseTempShow"));
    }
    else if (tr("CloseTempShow") == ui.openTempShowBtn->text())
    {
        m_mainWindow->SetTempShowFlag(false);
        ui.openTempShowBtn->setText(tr("OpenTempShow"));
    }
}

void TempPage::OnSaveTempDataBtnClicked()
{
    QFileDialog dialog(this);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter("Text File(*.txt);;RAW File(*.raw)");
    QMap<QString, QString> filterExtensions = {
        {"Text File(*.txt)", "txt"},
        {"RAW File(*.raw)", "raw"}
    };
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    QString fileName = dialog.selectedFiles().first();
    QString selectedFilter = dialog.selectedNameFilter();
    QString ext = filterExtensions.value(selectedFilter, "");
    if (!ext.isEmpty()) {
        QFileInfo fi(fileName);
        fileName = fi.absolutePath() + "/" + fi.completeBaseName() + "." + ext;
    }
    m_mainWindow->SetSavaOneTemp(true, fileName);
}

void TempPage::OnEnvParamGetBtnClicked()
{
    QString temp;
    IRC_USB_ENV_PARAM envParam;
    int err = IRC_USB_GetEnvParam(m_handle, &envParam);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Get env param");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
    RefreshEnvParam(envParam);
}

void TempPage::OnEnvParnSetBtnClicked()
{
    QString temp;
    IRC_USB_ENV_PARAM envParam;
    envParam.reflectedTemp = ui.reflTempEdit->text().toFloat();
    envParam.atmosphereTemp = ui.atmTempEdit->text().toFloat();
    envParam.transmittance = ui.atmTranEdit->text().toFloat();
    envParam.emissivity = ui.emissivityEdit->text().toFloat();
    envParam.distance = ui.distanceEdit->text().toFloat();
    int err = IRC_USB_SetEnvParam(m_handle, &envParam);
    if (IRC_USB_ERROR_OK != err)
    {
        temp = tr("Set env param");
        MSG_BOX.ShowErrorMessage(temp, err);
        return;
    }
}

void TempPage::OnChangeHandle(IRC_USB_HANDLE handle)
{
    m_handle = handle;
}
