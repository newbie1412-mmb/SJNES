#include "SpriteScreenWidget.h"
#include "Bus.h"
#include "PPU.h"

#include <QPainter>
#include <QPen>
#include <QColor>
#include <algorithm>

SpriteScreenWidget::SpriteScreenWidget(Bus* bus, QWidget* parent)
    : QWidget(parent), bus(bus)
{
    setMinimumSize(512, 480); // NES 256x240 scale 2x

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        update();
        });

    timer->start(16); // 60fps
}

void SpriteScreenWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), Qt::black);

    if (!bus || !bus->ppu)
        return;

    float scaleX = width() / 256.0f;
    float scaleY = height() / 240.0f;

    // Giữ tỉ lệ đúng 256x240
    float scale = std::min(scaleX, scaleY);

    int viewW = int(256 * scale);
    int viewH = int(240 * scale);

    int offsetX = (width() - viewW) / 2;
    int offsetY = (height() - viewH) / 2;

    p.save();
    p.translate(offsetX, offsetY);

    // Khung màn hình NES
    p.setPen(QPen(QColor(70, 70, 70), 1));
    p.drawRect(0, 0, viewW - 1, viewH - 1);

    // Vẽ sprite theo thứ tự ngược để sprite index nhỏ nằm lên trên
    // Sprite 0 ưu tiên cao hơn sprite 63
    for (int i = 63; i >= 0; i--)
    {
        drawOneSprite(p, i, scale, scale);
    }

    p.restore();
}

void SpriteScreenWidget::drawOneSprite(QPainter& p, int spriteIndex, float scaleX, float scaleY)
{
    if (!bus || !bus->ppu)
        return;

    PPU* ppu = bus->ppu;

    uint8_t y = ppu->GetOAMByte(spriteIndex * 4 + 0);
    uint8_t tile = ppu->GetOAMByte(spriteIndex * 4 + 1);
    uint8_t attr = ppu->GetOAMByte(spriteIndex * 4 + 2);
    uint8_t x = ppu->GetOAMByte(spriteIndex * 4 + 3);

    // Sprite ngoài màn hình
    if (y >= 240)
        return;

    uint8_t ctrl = ppu->GetPPUCtrl();

    bool sprite8x16 = (ctrl & 0x20) != 0;
    int spriteHeight = sprite8x16 ? 16 : 8;

    bool flipX = (attr & 0x40) != 0;
    bool flipY = (attr & 0x80) != 0;

    uint8_t palette = attr & 0x03;

    // Vẽ từng pixel của sprite
    for (int py = 0; py < spriteHeight; py++)
    {
        int row = flipY ? (spriteHeight - 1 - py) : py;

        uint16_t patternAddr = 0;

        if (!sprite8x16)
        {
            // Sprite 8x8
            uint16_t patternBase = (ctrl & 0x08) ? 0x1000 : 0x0000;
            patternAddr = patternBase | ((uint16_t)tile << 4) | row;
        }
        else
        {
            // Sprite 8x16
            uint16_t table = (tile & 0x01) ? 0x1000 : 0x0000;
            uint16_t baseTile = tile & 0xFE;

            if (row < 8)
            {
                patternAddr = table | (baseTile << 4) | row;
            }
            else
            {
                patternAddr = table | ((baseTile + 1) << 4) | (row - 8);
            }
        }

        uint8_t lo = ppu->DebugPPURead(patternAddr);
        uint8_t hi = ppu->DebugPPURead(patternAddr + 8);

        for (int px = 0; px < 8; px++)
        {
            int col = flipX ? (7 - px) : px;

            uint8_t bit0 = (lo >> (7 - col)) & 0x01;
            uint8_t bit1 = (hi >> (7 - col)) & 0x01;
            uint8_t pixel = (bit1 << 1) | bit0;

            // Pixel 0 của sprite là trong suốt
            if (pixel == 0)
                continue;

            uint16_t palAddr = 0x3F10 + palette * 4 + pixel;
            uint8_t colorIndex = ppu->DebugPPURead(palAddr) & 0x3F;
            PPU::RGBColor c = ppu->GetNESColor(colorIndex);
            QColor color(c.r, c.g, c.b);

            int screenX = x + px;
            int screenY = y + py;

            // Nếu thấy sprite lệch xuống/lên 1 pixel, thử đổi thành:
            // int screenY = y + py + 1;

            if (screenX < 0 || screenX >= 256 || screenY < 0 || screenY >= 240)
                continue;

            QRectF r(
                screenX * scaleX,
                screenY * scaleY,
                scaleX,
                scaleY
            );

            p.fillRect(r, color);
        }
    }
}