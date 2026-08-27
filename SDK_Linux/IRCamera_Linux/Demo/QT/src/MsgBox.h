#pragma once
#include <QDebug>
#include <QWidget>
#include <QMessageBox>
#include "IRCUSBSDKDef.h"

class MsgBox: public QWidget
{
    Q_OBJECT
public:
    MsgBox(QWidget* parent = Q_NULLPTR);
    ~MsgBox();

    static MsgBox& GetInstance()
    {
        static MsgBox instance;
        return instance;
    }
    void ShowErrorMessage(QString& str, int errorCode);
};

#define MSG_BOX (MsgBox::GetInstance())