#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QWidget>
#include <QPropertyAnimation>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);

    QSize sizeHint() const override;

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    qreal position() const { return m_position; }
    void  setPosition(qreal pos);

    bool   m_checked  = false;
    qreal  m_position = 0.0;   // 0.0 = off, 1.0 = on
    QPropertyAnimation *m_animation = nullptr;
};

#endif // TOGGLESWITCH_H
