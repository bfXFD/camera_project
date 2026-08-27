#include "MainWindow.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    IRC_USB_Init();
    int ret = -1;
    {
        MainWindow w;
        w.show();
        ret = a.exec();
    }
    IRC_USB_Deinit();
    return ret;
}
