#pragma once
#include <QDialog>
#include <QAction>
#include <QCheckBox>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QMap>
#include "EQPanel.h"

class EmuWorker;

// SettingsDialog: gom toàn bộ Setting (Video/Audio/Performance/Input) + Audio Channel
// (trước đây nằm rải rác trong menu Setting lồng nhau và menu Debug) vào 1 cửa sổ
// riêng, dạng tab, cho gọn và dễ nhìn hơn. Tab Audio còn nhúng thẳng EQPanel (9-band
// equalizer) thay vì phải bấm mở cửa sổ riêng.
//
// Thiết kế: KHÔNG viết lại logic bật/tắt từng tính năng. Mỗi checkbox/radio trong
// dialog này chỉ đơn thuần forward trạng thái sang đúng QAction gốc (setChecked() /
// trigger()) đã có sẵn signal/slot wiring đầy đủ trong SJNES.cpp — nên mọi logic cũ
// (SetSmoothSaw, SetReverseDpcmBits, mute channel, overclock...) vẫn hoạt động y hệt,
// chỉ đổi chỗ hiển thị UI.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Truyền vào toàn bộ QAction* cần thiết từ Ui::SJNESClass. Dùng struct để tránh
    // constructor có quá nhiều tham số rời rạc dễ nhầm thứ tự.
    struct Actions
    {
        // Video
        QAction* pixelPerfect = nullptr;
        QAction* scanline = nullptr;
        QAction* crtLite = nullptr;
        QAction* video60fps = nullptr;

        // Audio
        QAction* mono = nullptr;
        QAction* stereo = nullptr;
        QAction* smoothTriangle = nullptr;
        QAction* smoothSaw = nullptr;
        QAction* dmcReverse = nullptr;
        QAction* dmcReducePopping = nullptr;

        // Performance
        QAction* overclockOff = nullptr;
        QAction* overclock50 = nullptr;
        QAction* overclock100 = nullptr;
        QAction* overclock200 = nullptr;
        QAction* overclock250 = nullptr;
        QAction* removeSpriteLimit = nullptr;

        // Input
        QAction* autoA = nullptr;
        QAction* autoB = nullptr;
        QAction* zapperEnabled = nullptr;

        // Audio Channel (mute per-channel) — NES gốc
        QAction* chPulse1 = nullptr;
        QAction* chPulse2 = nullptr;
        QAction* chTriangle = nullptr;
        QAction* chNoise = nullptr;
        QAction* chDMC = nullptr;

        // VRC6 — tách riêng từng kênh con thay vì 1 action gộp chung
        QAction* chVRC6Pulse1 = nullptr;
        QAction* chVRC6Pulse2 = nullptr;
        QAction* chVRC6Saw = nullptr;

        // VRC7 — 6 kênh FM riêng biệt
        QAction* chVRC7[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        // S5B (Sunsoft 5B) — 3 kênh tone (A/B/C)
        QAction* chS5BToneA = nullptr;
        QAction* chS5BToneB = nullptr;
        QAction* chS5BToneC = nullptr;

        // MMC5 — 2 pulse + 1 PCM
        QAction* chMMC5Pulse1 = nullptr;
        QAction* chMMC5Pulse2 = nullptr;
        QAction* chMMC5PCM = nullptr;

        // N163 (Namco 163) — tối đa 8 kênh wavetable, checkbox nào không dùng
        // (do ROM chỉ bật ít kênh hơn) thì cứ để disabled/không tick, không ảnh hưởng gì.
        QAction* chN163[8] = { nullptr, nullptr, nullptr, nullptr,
                                nullptr, nullptr, nullptr, nullptr };
    };

    // worker/initialGains/initialEnabled: cần cho EQPanel nhúng trong tab Audio,
    // truyền y hệt tham số mày đang dùng để tạo EQWindow trước đây.
    // initialChannelGains: gain hiện tại (0.0 - 2.0, 1.0 = 100%) của từng channel,
    // key là channelId y hệt chuỗi truyền cho makeChannelRow (vd "nes.pulse1",
    // "vrc7.ch3"...). Channel nào không có trong map thì slider mặc định 100%.
    // Cần truyền vào để slider hiển thị đúng giá trị đã lưu thay vì luôn reset về 100%
    // mỗi lần đóng/mở lại dialog.
    explicit SettingsDialog(
        const Actions& actions,
        EmuWorker* worker,
        const float initialGains[9],
        bool initialEnabled,
        const QMap<QString, float>& initialChannelGains,
        QWidget* parent = nullptr
    );

signals:
    // Forward lại từ EQPanel nhúng bên trong, để SJNES.cpp vẫn cập nhật được
    // eqGains[]/eqEnabled y hệt cách nó đang làm với EQWindow.
    void eqBandGainChanged(int band, float gainDB);
    void eqEnabledChanged(bool on);
    void channelGainChanged(QString channelId, float gain);

private:
    Actions acts;
    EmuWorker* worker = nullptr;
    float eqInitialGains[9] = {};
    bool eqInitialEnabled = false;
    QMap<QString, float> channelInitialGains;

    QWidget* buildVideoTab();
    QWidget* buildAudioTab();
    QWidget* buildPerformanceTab();
    QWidget* buildInputTab();
    QWidget* buildAudioChannelTab();

    // Helper: tạo 1 QCheckBox đồng bộ 2 chiều với 1 QAction checkable có sẵn.
    QCheckBox* makeSyncedCheckBox(const QString& label, QAction* action, QWidget* parentWidget);

    // Helper: tạo 1 QRadioButton đồng bộ 2 chiều với 1 QAction checkable (dùng cho
    // nhóm loại trừ lẫn nhau như Overclock).
    QRadioButton* makeSyncedRadio(const QString& label, QAction* action, QWidget* parentWidget);

    QWidget* makeChannelRow(const QString& label, const QString& channelId,
        QAction* muteAction, QWidget* parentWidget, int maxPercent = 200);

    // Đọc gain đã lưu (channelInitialGains) cho channelId, trả về phần trăm int
    // để set trực tiếp cho slider. Không có trong map -> mặc định 100%.
    int initialPercentFor(const QString& channelId) const;
};