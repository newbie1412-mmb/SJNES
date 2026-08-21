#pragma once
#include <QDialog>
#include <QTimer>
#include "PPU.h"

class PaletteViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit PaletteViewerWindow(PPU* ppu, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void refresh();

private:
    PPU* ppu = nullptr;
    QTimer* timer = nullptr;
};