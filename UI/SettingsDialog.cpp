#include "SettingsDialog.h"
#include <QLabel>
#include <QGroupBox>
#include <QButtonGroup>
#include <QScrollArea>
#include <QSlider>
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(
    const Actions& actions,
    EmuWorker* worker,
    const float initialGains[9],
    bool initialEnabled,
    const QMap<QString, float>& initialChannelGains,
    QWidget* parent
)
    : QDialog(parent), acts(actions), worker(worker), eqInitialEnabled(initialEnabled),
    channelInitialGains(initialChannelGains)
{
    for (int i = 0; i < 9; i++)
        eqInitialGains[i] = initialGains[i];

    setWindowTitle("SJNES Settings");
    resize(560, 460);

    QTabWidget* tabs = new QTabWidget(this);
    tabs->addTab(buildVideoTab(), "Video");
    tabs->addTab(buildAudioTab(), "Audio");
    tabs->addTab(buildPerformanceTab(), "Performance");
    tabs->addTab(buildInputTab(), "Input");
    tabs->addTab(buildAudioChannelTab(), "Audio Channel");

    QPushButton* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(tabs);
    mainLayout->addWidget(closeBtn, 0, Qt::AlignRight);
    setLayout(mainLayout);
}

// Đồng bộ 2 chiều với action dùng signal "toggled" (checkbox thường, không loại trừ
// lẫn nhau). Đây là kiểu chiếm đa số các tính năng trong SJNES (SmoothSaw, dmcReverse,
// PixelPerfect, mute channel...).
QCheckBox* SettingsDialog::makeSyncedCheckBox(const QString& label, QAction* action, QWidget* parentWidget)
{
    QCheckBox* box = new QCheckBox(label, parentWidget);
    if (!action)
    {
        box->setEnabled(false);
        return box;
    }

    box->setChecked(action->isChecked());

    // Checkbox -> action
    connect(box, &QCheckBox::toggled, action, [action](bool checked) {
        if (action->isChecked() != checked)
            action->setChecked(checked);
        });

    // Action -> checkbox (đề phòng có nơi khác trong code cũng đổi action này,
    // ví dụ khi load ROM mới re-apply state).
    connect(action, &QAction::toggled, box, [box](bool checked) {
        if (box->isChecked() != checked)
            box->setChecked(checked);
        });

    return box;
}

// Đồng bộ 2 chiều với action dùng signal "triggered" (Mono/Stereo, Overclock) —
// những action này KHÔNG chạy logic qua "toggled" mà qua "triggered", nên phải gọi
// action->trigger() (y hệt hành vi khi người dùng click vào menu item) thay vì chỉ
// setChecked().
QRadioButton* SettingsDialog::makeSyncedRadio(const QString& label, QAction* action, QWidget* parentWidget)
{
    QRadioButton* radio = new QRadioButton(label, parentWidget);
    if (!action)
    {
        radio->setEnabled(false);
        return radio;
    }

    radio->setChecked(action->isChecked());

    connect(radio, &QRadioButton::clicked, action, [action](bool checked) {
        if (checked && !action->isChecked())
            action->trigger();
        });

    connect(action, &QAction::toggled, radio, [radio](bool checked) {
        if (radio->isChecked() != checked)
            radio->setChecked(checked);
        });

    return radio;
}

// Tạo 1 hàng "Tên kênh  [checkbox mute]  [====slider 0-200%====]  120%".
// channelId là chuỗi định danh gửi kèm signal channelGainChanged, để nơi nhận
// (MainWindow -> EmuWorker -> APU) biết gain này thuộc kênh nào mà set đúng chỗ,
// ví dụ "nes.pulse1", "vrc6.saw", "vrc7.ch3", "n163.ch5"...
int SettingsDialog::initialPercentFor(const QString& channelId) const
{
    auto it = channelInitialGains.constFind(channelId);
    if (it == channelInitialGains.constEnd())
        return 100;
    return qRound(it.value() * 100.0f);
}

QWidget* SettingsDialog::makeChannelRow(const QString& label, const QString& channelId,
    QAction* muteAction, QWidget* parentWidget, int maxPercent)
{
    QWidget* row = new QWidget(parentWidget);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);

    QCheckBox* box = makeSyncedCheckBox(label, muteAction, row);
    box->setMinimumWidth(110);
    rowLayout->addWidget(box);

    // Lấy đúng gain đã lưu từ lần chỉnh trước (channelInitialGains), thay vì luôn
    // hard-code 100% — nếu không, mỗi lần đóng/mở lại Settings slider sẽ bị "reset"
    // về 100% trên UI dù giá trị thực tế trong APU/worker vẫn giữ nguyên.
    int initialPercent = qBound(0, initialPercentFor(channelId), maxPercent);

    QSlider* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(0, maxPercent);   // 0% -> maxPercent% gain, 100% = mặc định gốc
    slider->setValue(initialPercent);
    slider->setFixedWidth(140);
    rowLayout->addWidget(slider);

    QLabel* pctLabel = new QLabel(QString("%1%").arg(initialPercent), row);
    pctLabel->setFixedWidth(40);
    rowLayout->addWidget(pctLabel);
    rowLayout->addStretch();

    // Mute thì disable slider luôn, tránh gây hiểu lầm "đang chỉnh volume" khi
    // kênh đang bị tắt tiếng hoàn toàn.
    slider->setEnabled(box->isChecked());
    connect(box, &QCheckBox::toggled, slider, &QSlider::setEnabled);

    connect(slider, &QSlider::valueChanged, this, [this, channelId, pctLabel](int value) {
        pctLabel->setText(QString("%1%").arg(value));
        emit channelGainChanged(channelId, float(value) / 100.0f);
        });

    return row;
}

QWidget* SettingsDialog::buildVideoTab()
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);

    layout->addWidget(makeSyncedCheckBox("Pixel perfect", acts.pixelPerfect, w));
    layout->addWidget(makeSyncedCheckBox("Scanline", acts.scanline, w));
    layout->addWidget(makeSyncedCheckBox("CRT Lite", acts.crtLite, w));
    layout->addWidget(makeSyncedCheckBox("60fps", acts.video60fps, w));
    layout->addStretch();

    return w;
}

QWidget* SettingsDialog::buildAudioTab()
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QGroupBox* channelModeBox = new QGroupBox("Output", w);
    QVBoxLayout* channelModeLayout = new QVBoxLayout(channelModeBox);
    QRadioButton* monoRadio = makeSyncedRadio("Mono", acts.mono, channelModeBox);
    QRadioButton* stereoRadio = makeSyncedRadio("Stereo", acts.stereo, channelModeBox);
    channelModeLayout->addWidget(monoRadio);
    channelModeLayout->addWidget(stereoRadio);
    layout->addWidget(channelModeBox);

    QGroupBox* qualityBox = new QGroupBox("Chip Emulation (Not Accuracy)", w);
    QVBoxLayout* qualityLayout = new QVBoxLayout(qualityBox);
    qualityLayout->addWidget(makeSyncedCheckBox("Smooth Triangle", acts.smoothTriangle, qualityBox));
    qualityLayout->addWidget(makeSyncedCheckBox("Smooth Sawtooth", acts.smoothSaw, qualityBox));
    layout->addWidget(qualityBox);

    layout->addWidget(makeSyncedCheckBox("Reduce popping sounds on the DMC channel", acts.dmcReducePopping, w));
    layout->addWidget(makeSyncedCheckBox("Reverse DPCM bit order", acts.dmcReverse, w));

    // Nhúng thẳng EQPanel (9-band equalizer) vào đây thay vì nút mở cửa sổ riêng —
    // để chỉnh âm lượng/EQ ngay trong Settings, không phải mở thêm cửa sổ khác,
    // và không bị chặn tương tác bởi dialog modal như trước.
    QGroupBox* eqBox = new QGroupBox("Equalizer", w);
    QVBoxLayout* eqLayout = new QVBoxLayout(eqBox);
    EQPanel* eqPanel = new EQPanel(worker, eqInitialGains, eqInitialEnabled, eqBox);
    connect(eqPanel, &EQPanel::bandGainChanged, this, &SettingsDialog::eqBandGainChanged);
    connect(eqPanel, &EQPanel::enabledChanged, this, &SettingsDialog::eqEnabledChanged);
    eqLayout->addWidget(eqPanel);
    layout->addWidget(eqBox);

    layout->addStretch();
    return w;
}

QWidget* SettingsDialog::buildPerformanceTab()
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QGroupBox* ocBox = new QGroupBox("Overclock", w);
    QVBoxLayout* ocLayout = new QVBoxLayout(ocBox);
    ocLayout->addWidget(makeSyncedRadio("OFF", acts.overclockOff, ocBox));
    ocLayout->addWidget(makeSyncedRadio("+50", acts.overclock50, ocBox));
    ocLayout->addWidget(makeSyncedRadio("+100", acts.overclock100, ocBox));
    ocLayout->addWidget(makeSyncedRadio("+200", acts.overclock200, ocBox));
    ocLayout->addWidget(makeSyncedRadio("+250", acts.overclock250, ocBox));
    layout->addWidget(ocBox);

    layout->addWidget(makeSyncedCheckBox("Remove 8 sprite limit", acts.removeSpriteLimit, w));
    layout->addStretch();

    return w;
}

QWidget* SettingsDialog::buildInputTab()
{
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);

    layout->addWidget(makeSyncedCheckBox("Auto A", acts.autoA, w));
    layout->addWidget(makeSyncedCheckBox("Auto B", acts.autoB, w));

    QGroupBox* zapperBox = new QGroupBox("Controller 2", w);
    QVBoxLayout* zapperLayout = new QVBoxLayout(zapperBox);
    zapperLayout->addWidget(makeSyncedCheckBox("Zapper (Duck Hunt)", acts.zapperEnabled, zapperBox));
    layout->addWidget(zapperBox);

    layout->addStretch();

    return w;
}

QWidget* SettingsDialog::buildAudioChannelTab()
{
    QWidget* inner = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(inner);

    QLabel* hint = new QLabel("Bật/tắt và chỉnh âm lượng từng kênh:", inner);
    layout->addWidget(hint);

    // --- NES gốc ---
    QGroupBox* nesBox = new QGroupBox("NES", inner);
    QVBoxLayout* nesLayout = new QVBoxLayout(nesBox);
    nesLayout->addWidget(makeChannelRow("Pulse 1", "nes.pulse1", acts.chPulse1, nesBox));
    nesLayout->addWidget(makeChannelRow("Pulse 2", "nes.pulse2", acts.chPulse2, nesBox));
    nesLayout->addWidget(makeChannelRow("Triangle", "nes.triangle", acts.chTriangle, nesBox));
    nesLayout->addWidget(makeChannelRow("Noise", "nes.noise", acts.chNoise, nesBox));
    nesLayout->addWidget(makeChannelRow("DMC", "nes.dmc", acts.chDMC, nesBox));
    layout->addWidget(nesBox);

    // --- VRC6 ---
    QGroupBox* vrc6Box = new QGroupBox("VRC6", inner);
    QVBoxLayout* vrc6Layout = new QVBoxLayout(vrc6Box);
    vrc6Layout->addWidget(makeChannelRow("Pulse 1", "vrc6.pulse1", acts.chVRC6Pulse1, vrc6Box));
    vrc6Layout->addWidget(makeChannelRow("Pulse 2", "vrc6.pulse2", acts.chVRC6Pulse2, vrc6Box));
    vrc6Layout->addWidget(makeChannelRow("Saw", "vrc6.saw", acts.chVRC6Saw, vrc6Box));
    layout->addWidget(vrc6Box);

    // --- VRC7 ---
    QGroupBox* vrc7Box = new QGroupBox("VRC7", inner);
    QVBoxLayout* vrc7Layout = new QVBoxLayout(vrc7Box);
    for (int i = 0; i < 6; i++)
        vrc7Layout->addWidget(makeChannelRow(QString("FM CH%1").arg(i + 1),
            QString("vrc7.ch%1").arg(i + 1), acts.chVRC7[i], vrc7Box, 100));
    layout->addWidget(vrc7Box);

    // --- S5B (Sunsoft 5B) ---
    QGroupBox* s5bBox = new QGroupBox("S5B", inner);
    QVBoxLayout* s5bLayout = new QVBoxLayout(s5bBox);
    s5bLayout->addWidget(makeChannelRow("Tone A", "s5b.toneA", acts.chS5BToneA, s5bBox));
    s5bLayout->addWidget(makeChannelRow("Tone B", "s5b.toneB", acts.chS5BToneB, s5bBox));
    s5bLayout->addWidget(makeChannelRow("Tone C", "s5b.toneC", acts.chS5BToneC, s5bBox));
    layout->addWidget(s5bBox);

    // --- MMC5 ---
    QGroupBox* mmc5Box = new QGroupBox("MMC5", inner);
    QVBoxLayout* mmc5Layout = new QVBoxLayout(mmc5Box);
    mmc5Layout->addWidget(makeChannelRow("Pulse 1", "mmc5.pulse1", acts.chMMC5Pulse1, mmc5Box));
    mmc5Layout->addWidget(makeChannelRow("Pulse 2", "mmc5.pulse2", acts.chMMC5Pulse2, mmc5Box));
    mmc5Layout->addWidget(makeChannelRow("PCM", "mmc5.pcm", acts.chMMC5PCM, mmc5Box));
    layout->addWidget(mmc5Box);

    // --- N163 (Namco 163) ---
    QGroupBox* n163Box = new QGroupBox("N163", inner);
    QVBoxLayout* n163Layout = new QVBoxLayout(n163Box);
    for (int i = 0; i < 8; i++)
        n163Layout->addWidget(makeChannelRow(QString("CH%1").arg(i + 1),
            QString("n163.ch%1").arg(i + 1), acts.chN163[i], n163Box));
    layout->addWidget(n163Box);

    layout->addStretch();

    // Bọc trong QScrollArea vì danh sách giờ khá dài (6 nhóm chip, ~28 hàng checkbox+slider)
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidget(inner);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}