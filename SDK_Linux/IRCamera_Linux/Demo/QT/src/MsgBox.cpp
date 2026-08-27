#include "MsgBox.h"


MsgBox::MsgBox(QWidget* parent)
{
}
MsgBox::~MsgBox()
{
}
void MsgBox::ShowErrorMessage(QString& str,int errorCode)
{
    QString ret;
    switch (errorCode)
    {
    case IRC_USB_ERROR_OK:
        ret = str + " Success!";
        QMessageBox::critical(this, tr("OK"), tr(ret.toUtf8().constData()));
        break;
    case IRC_USB_ERROR_FAILED:
        ret = str + " Failed!";
        QMessageBox::critical(this, tr("Error"), tr(ret.toUtf8().constData()));
        break;
    case IRC_USB_ERROR_NOT_SUPPORTED:
        ret = str + " Not Supported!";
        QMessageBox::critical(this, tr("Error"), tr(ret.toUtf8().constData()));
        break;
    case IRC_USB_ERROR_PARAM_WRONG:
        ret = str + " Param Wrong!";
        QMessageBox::critical(this, tr("Error"), tr(ret.toUtf8().constData()));
        break;
    case IRC_USB_ERROR_DEV_ERROR:
        ret = "Dev error, please search again.";
        QMessageBox::critical(this, tr("Error"), tr(ret.toUtf8().constData()));
        break;
    default:
        ret = str;
        QMessageBox::critical(this, tr("Error"), tr(ret.toUtf8().constData()));
        break;
    }
}