#pragma once
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVector>
#include <QMutex>
#include <QString>
#include "APU.h"

class AudioWaveWindow : public QOpenGLWidget
{
    Q_OBJECT

public:
    enum class WaveMode
    {
        NES,
        VRC6,
        VRC7,
        S5B,
        MMC5,
        N163
    };
    AudioWaveWindow(WaveMode mode, QWidget* parent = nullptr);
    void pushChannels(const AudioDebugChannels& ch);
    void clearSamples();
    struct TrigLockState
    {
        qint64 lockAbsPos = 0;
        float  period = 0.0f;
        bool   locked = false;
    };
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
private:
    static constexpr int CHANNEL_COUNT = 11;
    static constexpr int MAX_SAMPLES = 8192;
    static constexpr int DISPLAY_SAMPLES = 2048;
    QString names[CHANNEL_COUNT];
    QVector<float> buffers[CHANNEL_COUNT];
    QVector<float> phaseBuffers[CHANNEL_COUNT];

    // Filter chain (2 highpass + 1 lowpass) chạy LIÊN TỤC theo sample thật,
    // không reset mỗi frame vẽ. filteredBuffers[i] luôn khớp index 1-1 với buffers[i].
    struct RcFilterState
    {
        float hp1 = 0.0f, prevIn1 = 0.0f;
        float hp2 = 0.0f, prevIn2 = 0.0f;
        float lp = 0.0f;
        bool  initialized = false;
    };
    RcFilterState filterState[CHANNEL_COUNT];
    QVector<float> filteredBuffers[CHANNEL_COUNT];
    float lastVisual[CHANNEL_COUNT]{};
    float periodHint[CHANNEL_COUNT]{};
    QVector<float> n163SmoothY[CHANNEL_COUNT];
    bool n163SmoothValid[CHANNEL_COUNT]{};
    float n163LastTrigger[CHANNEL_COUNT]{};
    float n163Period[CHANNEL_COUNT]{};
    int n163LastBufferSize[CHANNEL_COUNT]{};
    bool n163HasTrigger[CHANNEL_COUNT]{};
    quint64 totalPushed[CHANNEL_COUNT]{};
    TrigLockState genericTrigLock[CHANNEL_COUNT];
    TrigLockState n163TrigLock[CHANNEL_COUNT];
    float vrc7LastPhaseStart[CHANNEL_COUNT]{};
    bool vrc7HasPhaseStart[CHANNEL_COUNT]{};
    QVector<float> vrc7CorrRef[CHANNEL_COUNT];
    bool vrc7CorrValid[CHANNEL_COUNT]{};
    qint64 vrc7LastAnchorAbs[CHANNEL_COUNT]{};
    bool vrc7HasLastAnchorAbs[CHANNEL_COUNT]{};
    QTimer* refreshTimer = nullptr;
    QMutex mutex;
    WaveMode mode;
    int activeChannelCount = 8;
    bool triangleZeroGate[CHANNEL_COUNT]{};
    QVector<float> triFrozenCycle[CHANNEL_COUNT];
    float triFreezeAmp[CHANNEL_COUNT]{};
    bool triFreezeActive[CHANNEL_COUNT]{};
    QVector<float> dmcSmoothY[CHANNEL_COUNT];
    bool dmcSmoothValid[CHANNEL_COUNT]{};

    // Cross-correlation "khoá" hình cho DMC: DMC không phải sóng tuần hoàn nên
    // không thể trigger theo chu kỳ như Pulse/Triangle. Thay vào đó, mỗi frame
    // tìm vị trí trong buffer mới khớp NHẤT với đoạn đã hiển thị ở frame trước
    // (dmcCorrRef), giúp hình ảnh đỡ "trôi" ngang liên tục dù không có chu kỳ thật.
    QVector<float> dmcCorrRef[CHANNEL_COUNT];
    bool dmcCorrValid[CHANNEL_COUNT]{};
    float sawSmoothAnchor[CHANNEL_COUNT]{};
    bool  sawHasSmoothAnchor[CHANNEL_COUNT]{};
    float sawSmoothPeriod[CHANNEL_COUNT]{};
    bool  sawHasSmoothPeriod[CHANNEL_COUNT]{};
};