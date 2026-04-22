#include "toggleswitch.h"
#include <QPainter>
#include <QMouseEvent>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(48, 26);
    setCursor(Qt::PointingHandCursor);

    m_animation = new QPropertyAnimation(this, "position", this);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);
}

bool ToggleSwitch::isChecked() const
{
    return m_checked;
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked == checked) return;
    m_checked = checked;

    m_animation->stop();
    m_animation->setStartValue(m_position);
    m_animation->setEndValue(checked ? 1.0 : 0.0);
    m_animation->start();

    emit toggled(checked);
}

QSize ToggleSwitch::sizeHint() const
{
    return {48, 26};
}

void ToggleSwitch::setPosition(qreal pos)
{
    m_position = pos;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const qreal margin = 3.0;
    const qreal thumbDiameter = h - 2 * margin;

    // Track colors
    const QColor trackOff(62, 62, 66);     // #3e3e42
    const QColor trackOn(0, 122, 204);     // #007acc
    const QColor thumbColor(255, 255, 255);

    // Interpolate track color
    const int r = static_cast<int>(trackOff.red()   + m_position * (trackOn.red()   - trackOff.red()));
    const int g = static_cast<int>(trackOff.green() + m_position * (trackOn.green() - trackOff.green()));
    const int b = static_cast<int>(trackOff.blue()  + m_position * (trackOn.blue()  - trackOff.blue()));
    const QColor trackColor(r, g, b);

    // Draw track (rounded rectangle)
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(QRectF(0, 0, w, h), h / 2.0, h / 2.0);

    // Draw thumb (circle)
    const qreal thumbX = margin + m_position * (w - thumbDiameter - 2 * margin);
    p.setBrush(thumbColor);
    p.drawEllipse(QRectF(thumbX, margin, thumbDiameter, thumbDiameter));
}

void ToggleSwitch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked);
    }
    QWidget::mousePressEvent(event);
}
