#include "SpriteViewerWindow.h"
#include "Bus.h"
#include "PPU.h"

#include <QHeaderView>
#include <QTableWidgetItem>
#include <QString>

SpriteViewerWindow::SpriteViewerWindow(Bus* bus, QWidget* parent)
    : QDialog(parent), bus(bus)
{
    setWindowTitle("Sprite Viewer");
    resize(1100, 620);

    table = new QTableWidget(this);
    table->setColumnCount(10);
    table->setRowCount(64);

    QStringList headers;
    headers << "ID"
        << "X"
        << "Y"
        << "Tile"
        << "Attr"
        << "Palette"
        << "Priority"
        << "Flip X"
        << "Flip Y"
        << "Visible";

    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    screen = new SpriteScreenWidget(bus, this);

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(table, 1);
    mainLayout->addWidget(screen, 2);

    setLayout(mainLayout);

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SpriteViewerWindow::updateSprites);
    timer->start(100);

    updateSprites();
}

void SpriteViewerWindow::updateSprites()
{
    if (!bus || !bus->ppu)
        return;

    for (int i = 0; i < 64; i++)
    {
        uint8_t y = bus->ppu->GetOAMByte(i * 4 + 0);
        uint8_t tile = bus->ppu->GetOAMByte(i * 4 + 1);
        uint8_t attr = bus->ppu->GetOAMByte(i * 4 + 2);
        uint8_t x = bus->ppu->GetOAMByte(i * 4 + 3);

        int palette = attr & 0x03;
        bool priorityBehindBg = (attr & 0x20) != 0;
        bool flipX = (attr & 0x40) != 0;
        bool flipY = (attr & 0x80) != 0;

        bool visible = (y < 240 && x < 256);

        table->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
        table->setItem(i, 1, new QTableWidgetItem(QString::number(x)));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(y)));
        table->setItem(i, 3, new QTableWidgetItem(QString("0x%1").arg(tile, 2, 16, QChar('0')).toUpper()));
        table->setItem(i, 4, new QTableWidgetItem(QString("0x%1").arg(attr, 2, 16, QChar('0')).toUpper()));
        table->setItem(i, 5, new QTableWidgetItem(QString::number(palette)));
        table->setItem(i, 6, new QTableWidgetItem(priorityBehindBg ? "Sau BG" : "Trước BG"));
        table->setItem(i, 7, new QTableWidgetItem(flipX ? "Có" : "Không"));
        table->setItem(i, 8, new QTableWidgetItem(flipY ? "Có" : "Không"));
        table->setItem(i, 9, new QTableWidgetItem(visible ? "Có" : "Không"));
    }
}