QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
DESTDIR = $$PWD/../bin/linux/

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AnalysisPage.cpp \
    GeneralPage.cpp \
    ImagePage.cpp \
    MainWindow.cpp \
    MsgBox.cpp \
    TempPage.cpp \
    VideoLabel.cpp \
    main.cpp

HEADERS += \
    AnalysisPage.h \
    GeneralPage.h \
    ImagePage.h \
    MainWindow.h \
    MsgBox.h \
    TempPage.h \
    VideoLabel.h

FORMS += \
    AnalysisPage.ui \
    GeneralPage.ui \
    ImagePage.ui \
    MainWindow.ui \
    TempPage.ui

INCLUDEPATH += $$PWD/../../../../include \
               $$PWD/../include \


LIBS += -L$$PWD/../bin/linux \
        -lIRCUSBSDK \
        -lPocoCrypto -lPocoFoundation -lPocoJSON \
        -lopencv_core -lopencv_imgproc\
        -luvc \
        -ludev

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    MainWindow.qrc
