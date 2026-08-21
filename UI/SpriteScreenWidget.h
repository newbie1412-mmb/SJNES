#pragma once

#include <QWidget>
#include <QTimer>

class Bus;

class SpriteScreenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpriteScreenWidget(Bus* bus, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawOneSprite(QPainter& p, int spriteIndex, float scaleX, float scaleY);

private:
    Bus* bus = nullptr;
    QTimer* timer = nullptr;
};