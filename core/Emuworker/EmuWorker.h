#pragma once
#include <QObject>
#include <QImage>
#include <QTimer>
#include "AudioEQ.h"
#include <QElapsedTimer>
#include <atomic>
#include <vector>
#include "Bus.h"
#include "CPU6502.h"
#include "PPU.h"
#include "APU.h"  
#include "NSFFile.h"
#include "Mapper_024.h"
#include "Mapper_069.h"
#include "Mapper_085.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
class EmuWorker : public QObject
{
    Q_OBJECT

public:
    explicit EmuWorker(QObject* parent = nullptr);

    Bus nes_bus;
    CPU6502 nes_cpu;
    PPU nes_ppu;

public slots:
    void start();
	void stop();
    void loadNSF(NSFFile nsf);
    void nsfNextTrack();
    void nsfPrevTrack();
    void restartCurrentNSFTrack();
    void setPowered(bool on) { machinePowered = on; }
    void setFastForward(bool on, int multiplier) { fastForward = on; fastForwardMultiplier = multiplier; }
    void setVideo60fps(bool on) { video60fps = on; }
    void setStereo(bool on) { is_stereo = on; }
    void setControllerState(uint8_t p1, uint8_t p2) { controllerState1 = p1; controllerState2 = p2; }
    void setNeedAudioMix(bool needAudio, bool needDebug) { wantOutputAudio = needAudio; wantWaveDebug = needDebug; }
    void setNsfSmoothSaw(bool enable) { if (nsfVRC6) nsfVRC6->setSmoothSaw(enable); }
    void setNsfMuteVRC6(bool mute) { if (nsfVRC6) nsfVRC6->muteVRC6 = mute; }
    bool isNsfMode() const { return nes_bus.nsfMode; }
    void setEQEnabled(bool on) { audioEQ.enabled = on; }
    void setEQBandGain(int band, float gainDB) { audioEQ.SetBandGain(band, gainDB); }
private slots:
    void runFrame();

signals:
    void frameReady(QImage frame);
    void audioReady(std::vector<float> samples, bool stereo);
    void debugChannelsReady(std::vector<AudioDebugChannels> dbgBatch);
    void gameFrameTicked(); // để SJNES tự đếm FPS
    void nsfConsoleMessage(QString msg);
    void nsfTrackChanged(int current, int total);
    void nsfExpansionDetected(bool vrc6, bool s5b, bool vrc7, bool mmc5, bool n163);
private:
    QTimer* timer = nullptr;
    QElapsedTimer framePacer;
    qint64 nextFrameNs = 0;

    uint32_t system_clock_counter = 0;
    uint16_t dma_dummy_counter = 0;
    uint64_t videoFrameCounter = 0;

    std::atomic<bool> machinePowered{ true };
    std::atomic<bool> fastForward{ false };
    std::atomic<int>  fastForwardMultiplier{ 3 };
    std::atomic<bool> is_stereo{ true };
    std::atomic<bool> wantOutputAudio{ false };
    std::atomic<bool> wantWaveDebug{ false };
    std::atomic<uint8_t> controllerState1{ 0 };
    std::atomic<uint8_t> controllerState2{ 0 };
    std::atomic<bool> video60fps{ false };
    std::vector<AudioDebugChannels> dbgBatch;

    bool holdturboA = false;
    bool holdturboB = false;

    void runNSFFrame();
    int callNSFRoutine(uint16_t address, uint8_t a, uint8_t x, int maxCpuCycles, std::vector<float>& audioSamples);
    void pushNSFAudioSample(std::vector<float>& audioSamples);

    std::unique_ptr<Mapper_024> nsfVRC6;
    std::unique_ptr<Mapper_069> nsfS5B;
    std::unique_ptr<Mapper_085> nsfVRC7;
    std::unique_ptr<Mapper_005> nsfMMC5;
    std::unique_ptr<Mapper_019> nsfN163;
    NSFFile currentNSF;
    int currentNSFSong = 1;
    double nsfAudioAccumulator = 0.0;
    QElapsedTimer nsfFramePacer;
    qint64 nsfNextFrameNs = 0;

    AudioEQ9Band audioEQ;
};