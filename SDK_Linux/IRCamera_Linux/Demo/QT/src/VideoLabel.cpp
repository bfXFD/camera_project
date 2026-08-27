#include "VideoLabel.h"
#include <QPainter>

VideoLabel::VideoLabel(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

VideoLabel::~VideoLabel()
{}

void VideoLabel::ClearUi()
{
    m_magnificationFlag = false;
    m_scale = 1.0;
    m_openTempShow = false;
    m_unit = 0;
}

void VideoLabel::SetImg(const QImage & img)
{
    m_img = img;
    update();
}

void VideoLabel::SetMagnificationFlag(double magnificationFlag)
{
    m_magnificationFlag = magnificationFlag;
    if (!m_magnificationFlag)
    {
        m_scale = 1.0;
    }
}

void VideoLabel::SetTempData(const unsigned short* tempData, int width, int height, bool flag, int unit)
{
    m_width = width;
    m_height = height;
    m_openTempShow = flag;
    m_unit = unit;
    if (m_openTempShow)
    {
        memcpy(&m_tempData[0], tempData, width * height * 2);
    }
}

void VideoLabel::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (m_img.isNull())
    {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QSize widgetSize = this->size();

    QPixmap originalPixmap = QPixmap::fromImage(m_img);
    if (originalPixmap.isNull())
    {
        return;
    }
    const QSize originalSize = originalPixmap.size();

    qreal scaleWidth = static_cast<qreal>(widgetSize.width()) / originalSize.width();
    qreal scaleHeight = static_cast<qreal>(widgetSize.height()) / originalSize.height();
    qreal initialScale = qMin(scaleWidth, scaleHeight);
    qreal totalScale = initialScale * m_scale;
    int scaledWidth = static_cast<int>(originalSize.width() * totalScale);
    int scaledHeight = static_cast<int>(originalSize.height() * totalScale);
    const int offsetX = (widgetSize.width() - scaledWidth) / 2;
    const int offsetY = (widgetSize.height() - scaledHeight) / 2;
    painter.drawPixmap(offsetX, offsetY, scaledWidth, scaledHeight, originalPixmap);
    painter.drawPixmap(offsetX, offsetY, scaledWidth, scaledHeight, originalPixmap);
    if (m_openTempShow)
    {
        QPoint imgPos = m_pos - QPoint(offsetX, offsetY);
        imgPos.setX(static_cast<int>(imgPos.x() / totalScale));
        imgPos.setY(static_cast<int>(imgPos.y() / totalScale));

        // 检查映射后的坐标是否在原始图片范围内
        if (imgPos.x() >= 0 && imgPos.x() < originalSize.width() &&
            imgPos.y() >= 0 && imgPos.y() < originalSize.height())
        {
            int index = imgPos.y() * originalSize.width() + imgPos.x();
            unsigned short tempValue = m_tempData[index];
            float fK = tempValue / 10.0f;
            float fC = fK - 273.15f;
            float fF = (fC * 9 / 5) + 32;

            QString tempStr;
            if (0 == m_unit)
            {
                tempStr = QString::number(fC, 'f', 1) + "C";
            }
            else if (1 == m_unit)
            {
                tempStr = QString::number(fK, 'f', 1) + "K";
            }
            else
            {
                tempStr = QString::number(fF, 'f', 1) + "F";
            }

            QFont font("Arial", 12);
            painter.setFont(font);
            painter.setPen(Qt::red);

            // 将原始坐标映射回缩放后的坐标以绘制文本
            QPoint drawPos = QPoint(static_cast<int>(imgPos.x() * totalScale) + offsetX,
                static_cast<int>(imgPos.y() * totalScale) + offsetY);
            painter.drawText(drawPos, tempStr);
        }
    }
}

void VideoLabel::wheelEvent(QWheelEvent* event)
{
    if (m_magnificationFlag)
    {
        if (event->angleDelta().y() > 0)
        {
            m_scale *= 1.1; // 放大
        }
        else if (event->angleDelta().y() < 0)
        {
            m_scale /= 1.1; // 缩小
        }


        if (m_scale < m_minScale) m_scale = m_minScale;
        if (m_scale > m_maxScale) m_scale = m_maxScale;

        event->accept();
        update(); // 更新绘制区域
    }
}

void VideoLabel::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
    if (m_openTempShow)
    {
        m_pos = event->pos();
    }
}
