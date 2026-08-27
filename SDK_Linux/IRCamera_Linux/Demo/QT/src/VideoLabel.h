#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>

class VideoLabel : public QWidget
{
    Q_OBJECT

public:
    VideoLabel(QWidget *parent = nullptr);
    ~VideoLabel();
    void ClearUi();
    void SetImg(const QImage& img);
    void SetMagnificationFlag(double magnificationFlag);
    void SetTempData(const unsigned short* tempData, int width, int height, bool flag, int unit);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void wheelEvent(QWheelEvent*) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;


private:
    QImage m_img;
    bool m_magnificationFlag = false;
    double m_scale = 1.0;
    const double m_minScale = 1.0;
    const double m_maxScale = 8.0;
    unsigned short m_tempData[1280 * 1024];
    int m_width;
    int m_height;
    bool m_openTempShow = false;
    int m_unit = 0;
    QPoint m_pos;
};
