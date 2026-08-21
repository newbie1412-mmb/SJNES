#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "SpriteScreenWidget.h"

class Bus;

class SpriteViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SpriteViewerWindow(Bus* bus, QWidget* parent = nullptr);

private slots:
    void updateSprites();

private:
    Bus* bus = nullptr;
    QTableWidget* table = nullptr;
    SpriteScreenWidget* screen = nullptr;
    QTimer* timer = nullptr;
};