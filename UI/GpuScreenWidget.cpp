#include "GpuScreenWidget.h"
#pragma comment(lib, "opengl32.lib")
#include <QPainter>
#include <QFont>
#include <QMutexLocker>
#include <QOpenGLContext>

GpuScreenWidget::GpuScreenWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setAutoFillBackground(false);
}

GpuScreenWidget::~GpuScreenWidget()
{
    makeCurrent();

    if (textureCreated) {
        glDeleteTextures(1, &textureId);
        textureCreated = false;
    }

    doneCurrent();
}

void GpuScreenWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // Pixel-perfect, không làm mờ ảnh NES
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Tạo texture trống 256x240
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        256,
        240,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    textureCreated = true;
}

void GpuScreenWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GpuScreenWidget::setFrame(const QImage& image)
{
    {
        QMutexLocker lock(&frameMutex);

        // OpenGL thích RGBA8888 hơn
        frame = image.convertToFormat(QImage::Format_RGBA8888);
    }

    update();
}

void GpuScreenWidget::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    QImage img;
    {
        QMutexLocker lock(&frameMutex);
        img = frame;
    }

    if (img.isNull()) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, textureId);

    // Upload framebuffer NES 256x240 lên GPU
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0,
        0,
        256,
        240,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        img.constBits()
    );

    glEnable(GL_TEXTURE_2D);

    // Giữ đúng tỉ lệ 256:240
    float widgetAspect = width() / float(height());
    float nesAspect = 256.0f / 240.0f;

    float x = 1.0f;
    float y = 1.0f;

    if (widgetAspect > nesAspect) {
        x = nesAspect / widgetAspect;
    }
    else {
        y = widgetAspect / nesAspect;
    }


    // Vẽ quad bằng OpenGL fixed pipeline
    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-x, y);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x, y);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x, -y);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-x, -y);

    glEnd();

    glDisable(GL_TEXTURE_2D);

    if (scanlineEnabled)
    {
        glDisable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Alpha càng thấp càng tự nhiên
        glColor4f(0.0f, 0.0f, 0.0f, 0.22f);

        glBegin(GL_QUADS);

        for (int line = 1; line < 240; line += 2)
        {
            // Mỗi scanline NES trong tọa độ OpenGL
            float lineTop = y - (2.0f * y * line / 240.0f);
            float lineBottom = y - (2.0f * y * (line + 1) / 240.0f);

            // Chỉ phủ khoảng 55% chiều cao dòng cho mềm hơn
            float center = (lineTop + lineBottom) * 0.5f;
            float halfHeight = (lineTop - lineBottom) * 0.28f;

            float y1 = center + halfHeight;
            float y2 = center - halfHeight;

            glVertex2f(-x, y1);
            glVertex2f(x, y1);
            glVertex2f(x, y2);
            glVertex2f(-x, y2);
        }

        glEnd();

        glDisable(GL_BLEND);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);


    }
    if (crtLiteEnabled)
    {
        glDisable(GL_TEXTURE_2D);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glShadeModel(GL_SMOOTH);

        // 1) Scanline rất nhẹ
        glColor4f(0.0f, 0.0f, 0.0f, 0.12f);

        glBegin(GL_QUADS);

        for (int line = 1; line < 240; line += 2)
        {
            float lineTop = y - (2.0f * y * line / 240.0f);
            float lineBottom = y - (2.0f * y * (line + 1) / 240.0f);

            float center = (lineTop + lineBottom) * 0.5f;
            float halfHeight = (lineTop - lineBottom) * 0.22f;

            float y1 = center + halfHeight;
            float y2 = center - halfHeight;

            glVertex2f(-x, y1);
            glVertex2f(x, y1);
            glVertex2f(x, y2);
            glVertex2f(-x, y2);
        }

        glEnd();

        // 2) Vignette mềm 4 cạnh, dùng gradient alpha
        float edgeW = x * 0.18f;
        float edgeH = y * 0.14f;

        glBegin(GL_QUADS);

        // Cạnh trái: ngoài tối -> trong trong suốt
        glColor4f(0.0f, 0.0f, 0.0f, 0.22f);
        glVertex2f(-x, y);
        glVertex2f(-x, -y);

        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(-x + edgeW, -y);
        glVertex2f(-x + edgeW, y);

        // Cạnh phải
        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(x - edgeW, y);
        glVertex2f(x - edgeW, -y);

        glColor4f(0.0f, 0.0f, 0.0f, 0.22f);
        glVertex2f(x, -y);
        glVertex2f(x, y);

        // Cạnh trên
        glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
        glVertex2f(-x, y);
        glVertex2f(x, y);

        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(x, y - edgeH);
        glVertex2f(-x, y - edgeH);

        // Cạnh dưới
        glColor4f(0.0f, 0.0f, 0.0f, 0.00f);
        glVertex2f(-x, -y + edgeH);
        glVertex2f(x, -y + edgeH);

        glColor4f(0.0f, 0.0f, 0.0f, 0.18f);
        glVertex2f(x, -y);
        glVertex2f(-x, -y);

        glEnd();

        // 3) Phủ tối toàn màn cực nhẹ để dịu màu
        glColor4f(0.0f, 0.0f, 0.0f, 0.04f);

        glBegin(GL_QUADS);
        glVertex2f(-x, y);
        glVertex2f(x, y);
        glVertex2f(x, -y);
        glVertex2f(-x, -y);
        glEnd();

        glDisable(GL_BLEND);

        glShadeModel(GL_FLAT);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (!overlayText.isEmpty())
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QFont font("Consolas", 11, QFont::Bold);
        painter.setFont(font);

        // Nền đen mờ phía sau chữ
        QRect box(8, 8, 145, 42);
        painter.fillRect(box, QColor(0, 0, 0, 150));

        painter.setPen(QColor(255, 255, 0));
        painter.drawText(
            QRect(14, 12, width() - 28, 60),
            Qt::AlignLeft | Qt::AlignTop,
            overlayText
        );
    }
}

void GpuScreenWidget::setScanlineEnabled(bool enabled)
{
    scanlineEnabled = enabled;
    update();
}

void GpuScreenWidget::setCrtLiteEnabled(bool enabled)
{
    crtLiteEnabled = enabled;
    update();
}
void GpuScreenWidget::setOverlayText(const QString& text)
{
    overlayText = text;
    update();
}