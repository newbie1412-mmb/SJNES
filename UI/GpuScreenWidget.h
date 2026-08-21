#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QImage>
#include <QMutex>
#include <QString>

class GpuScreenWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GpuScreenWidget(QWidget* parent = nullptr);
    ~GpuScreenWidget();

    void setFrame(const QImage& image);
    void setScanlineEnabled(bool enabled);
    void setCrtLiteEnabled(bool enabled);
    void setOverlayText(const QString& text);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QImage frame;
    QMutex frameMutex;

    GLuint textureId = 0;
    bool textureCreated = false;

    bool scanlineEnabled = false;
    bool crtLiteEnabled = false;

    QString overlayText;
};