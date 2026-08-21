#include "EmuWorker.h"
#include "LogBuffer.h"
#include "Mapper_005.h"
#include "Mapper_019.h"
#include "Mapper_085.h"
#include "Mapper_069.h"
#include "Mapper_024.h"
#include <cmath>

EmuWorker::EmuWorker(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<AudioDebugChannels>("AudioDebugChannels");
    qRegisterMetaType<std::vector<AudioDebugChannels>>("std::vector<AudioDebugChannels>");
    qRegisterMetaType<NSFFile>("NSFFile");
}

void EmuWorker::start()
{
    if (!timer) {
        timer = new QTimer(this);
        timer->setTimerType(Qt::PreciseTimer);
        connect(timer, &QTimer::timeout, this, &EmuWorker::runFrame);
    }
    timer->start(1);
}
void EmuWorker::stop()
{
    if (timer) timer->stop();
}
void EmuWorker::runFrame()
{
    if (!machinePowered) return;
    if (nes_bus.nsfMode) { runNSFFrame(); return; }

    constexpr double NES_CPU_HZ = 1789773.0;
    constexpr double NES_PPU_CYCLES_PER_FRAME = 89342.0;
    constexpr double NES_FPS = NES_CPU_HZ / (NES_PPU_CYCLES_PER_FRAME / 3.0);
    const qint64 FRAME_NS = static_cast<qint64>(1000000000.0 / NES_FPS);

    if (!framePacer.isValid()) { framePacer.start(); nextFrameNs = framePacer.nsecsElapsed(); }
    qint64 now = framePacer.nsecsElapsed();
    if (now < nextFrameNs) return;
    if (now - nextFrameNs > FRAME_NS * 5) nextFrameNs = now;
    nextFrameNs += FRAME_NS;

    int framesToRun = fastForward ? fastForwardMultiplier.load() : 1;

    for (int ff = 0; ff < framesToRun; ff++)
    {
        dbgBatch.clear();
        static std::vector<float> audio_samples;
        static double audio_accumulator = 0.0;
        audio_samples.clear();
        if (audio_samples.capacity() < 8192) audio_samples.reserve(8192);

        const float MASTER_VOLUME = 0.95f;
        const bool outputAudio = !fastForward && wantOutputAudio;
        const bool needAudioMix = outputAudio || wantWaveDebug;
        const bool stereo = is_stereo;

        Mapper* mapper = nullptr;
        Mapper_005* mmc5 = nullptr;
        Mapper_019* n163 = nullptr;
        Mapper_085* vrc7 = nullptr;
        Mapper_069* s5b = nullptr;
        Mapper_024* vrc6 = nullptr;

        if (nes_bus.cart && nes_bus.cart->pMapper)
        {
            mapper = nes_bus.cart->pMapper.get();
            mmc5 = dynamic_cast<Mapper_005*>(mapper);
            n163 = dynamic_cast<Mapper_019*>(mapper);
            vrc7 = dynamic_cast<Mapper_085*>(mapper);
            s5b = dynamic_cast<Mapper_069*>(mapper);
            vrc6 = dynamic_cast<Mapper_024*>(mapper);
        }

        AudioDebugChannels dbg;
        bool dbgFilled = false;

        auto fillDebugIfNeeded = [&]() {
            if (!wantWaveDebug) return;
            dbg = nes_bus.n_apu.GetDebugChannels();
            if (n163) {
                n163->GetN163DebugChannels(dbg.n163Wave1, dbg.n163Wave2, dbg.n163Wave3, dbg.n163Wave4,
                    dbg.n163Wave5, dbg.n163Wave6, dbg.n163Wave7, dbg.n163Wave8);
                n163->GetN163DebugPeriods(dbg.n163Period1, dbg.n163Period2, dbg.n163Period3, dbg.n163Period4,
                    dbg.n163Period5, dbg.n163Period6, dbg.n163Period7, dbg.n163Period8);
            }
            else if (mmc5) {
                mmc5->GetMMC5DebugChannels(dbg.mmc5Pulse1, dbg.mmc5Pulse2, dbg.mmc5PCM);
                mmc5->GetMMC5DebugPeriods(dbg.mmc5Pulse1Period, dbg.mmc5Pulse2Period);
                mmc5->GetMMC5DebugDuty(dbg.mmc5Pulse1Duty, dbg.mmc5Pulse2Duty);
            }
            else if (vrc7) {
                vrc7->GetVrc7DebugChannels(dbg.vrc7Wave1, dbg.vrc7Wave2, dbg.vrc7Wave3, dbg.vrc7Wave4, dbg.vrc7Wave5, dbg.vrc7Wave6);
                vrc7->GetVrc7DebugPeriods(dbg.vrc7Wave1Period, dbg.vrc7Wave2Period, dbg.vrc7Wave3Period, dbg.vrc7Wave4Period, dbg.vrc7Wave5Period, dbg.vrc7Wave6Period);
                vrc7->GetVrc7DebugPhases(dbg.vrc7Wave1Phase, dbg.vrc7Wave2Phase, dbg.vrc7Wave3Phase, dbg.vrc7Wave4Phase, dbg.vrc7Wave5Phase, dbg.vrc7Wave6Phase);
            }
            else if (s5b) {
                s5b->GetExpansionDebugChannels(dbg.s5bToneA, dbg.s5bToneB, dbg.s5bToneC);
                s5b->GetS5BDebugPeriods(dbg.s5bToneAPeriod, dbg.s5bToneBPeriod, dbg.s5bToneCPeriod);
                s5b->GetS5BDebugDuty(dbg.s5bToneADuty, dbg.s5bToneBDuty, dbg.s5bToneCDuty);
            }
            else if (vrc6) {
                vrc6->GetExpansionDebugChannels(dbg.vrc6Pulse1, dbg.vrc6Pulse2, dbg.vrc6Saw);
                vrc6->GetVRC6DebugPeriods(dbg.vrc6Pulse1Period, dbg.vrc6Pulse2Period, dbg.vrc6SawPeriod);
                vrc6->GetVRC6DebugDuty(dbg.vrc6Pulse1Duty, dbg.vrc6Pulse2Duty);
            }
            else if (mapper) {
                mapper->GetExpansionDebugChannels(dbg.vrc6Pulse1, dbg.vrc6Pulse2, dbg.vrc6Saw);
            }
            dbgBatch.push_back(dbg);
            dbgFilled = true;
            };

        const int PPU_CYCLES_PER_FRAME = 89342;
        for (int i = 0; i < PPU_CYCLES_PER_FRAME; i++)
        {
            nes_bus.ppu->Step();

            if (system_clock_counter % 3 == 0)
            {
                if (mapper) {
                    mapper->irqStep();
                    if (mapper->irqState()) nes_cpu.SetIrqSource(CPU6502::IRQ_EXTERNAL);
                    else nes_cpu.ClearIrqSource(CPU6502::IRQ_EXTERNAL);
                }

                if (nes_bus.dma_transfer) {
                    if (dma_dummy_counter < 512) dma_dummy_counter++;
                    else { nes_bus.dma_transfer = false; dma_dummy_counter = 0; }
                }
                else {
                    nes_cpu.clock();
                }

                nes_bus.n_apu.Step();
                if (mmc5) mmc5->ClockAudio();

                audio_accumulator += 44100.0 / 1789773.0;
                if (audio_accumulator >= 1.0)
                {
                    audio_accumulator -= 1.0;
                    if (needAudioMix)
                    {
                        if (stereo)
                        {
                            float left = 0.0f, right = 0.0f;
                            nes_bus.n_apu.GetOutputSampleStereo(left, right);
                            float expL = 0.0f, expR = 0.0f;
                            if (mapper) mapper->GetExpansionAudioStereo(expL, expR);
                            if (n163) { expL *= 2.0f; expR *= 2.0f; }
                            fillDebugIfNeeded();
                            left += expL; right += expR;
                            // Filter chain thật (2 highpass + 1 lowpass) áp SAU khi cộng
                            // expansion audio, đúng với đường tín hiệu analog phần cứng thật.
                            nes_bus.n_apu.ApplyOutputFilterStereo(left, right);
                            left = std::tanh(left * MASTER_VOLUME);
                            right = std::tanh(right * MASTER_VOLUME);

                            if (audioEQ.enabled)
                            {
                                left = audioEQ.ProcessLeft(left);
                                right = audioEQ.ProcessRight(right);
                            }

                            if (outputAudio) { audio_samples.push_back(left); audio_samples.push_back(right); }
                        }
                        else
                        {
                            float sample = nes_bus.n_apu.GetOutputSample();
                            float exp = mapper ? mapper->GetExpansionAudio() : 0.0f;
                            if (n163) exp *= 2.0f;
                            fillDebugIfNeeded();
                            sample += exp;
                            sample = nes_bus.n_apu.ApplyOutputFilterMono(sample);
                            sample = std::tanh(sample * MASTER_VOLUME);
                            if (audioEQ.enabled)
                            {
                                sample = audioEQ.ProcessLeft(sample);
                            }
                            if (outputAudio) audio_samples.push_back(sample);
                        }
                    }
                }
            }

            if (nes_bus.ppu->nmi_requested) { nes_bus.ppu->nmi_requested = false; nes_cpu.nmi(); }
            system_clock_counter++;
        }

        int extraScanlines = nes_ppu.GetExtraScanlinesBeforeNMI();
        int extraCpuCycles = (extraScanlines * 341) / 3;
        for (int i = 0; i < extraCpuCycles; i++)
        {
            if (mapper) {
                mapper->irqStep();
                if (mapper->irqState()) nes_cpu.SetIrqSource(CPU6502::IRQ_EXTERNAL);
                else nes_cpu.ClearIrqSource(CPU6502::IRQ_EXTERNAL);
            }
            if (nes_bus.dma_transfer) {
                if (dma_dummy_counter < 512) dma_dummy_counter++;
                else { nes_bus.dma_transfer = false; dma_dummy_counter = 0; }
            }
            else {
                nes_cpu.clock();
            }
            if (mmc5) mmc5->ClockAudio();
        }

        if (outputAudio && !audio_samples.empty())
            emit audioReady(audio_samples, stereo);

        if (!dbgBatch.empty())
            emit debugChannelsReady(dbgBatch);

        bool shouldOutputVideoFrame = video60fps || ((videoFrameCounter & 1) == 0);

        if (shouldOutputVideoFrame)
        {
            QImage frameImage(
                reinterpret_cast<const uchar*>(nes_ppu.GetScreenBuffer()),
                256, 240, QImage::Format_RGB32
            );
            emit frameReady(frameImage.copy());
        }

        emit gameFrameTicked();

        videoFrameCounter++;

        uint8_t p1 = controllerState1;
        uint8_t p2 = controllerState2;

        if (holdturboA && (p1 & 0x80)) { if ((videoFrameCounter & 1) == 0) p1 &= ~0x80; }
        if (holdturboB && (p1 & 0x40)) { if ((videoFrameCounter & 1) == 0) p1 &= ~0x40; }

        nes_bus.controller_state = p1;
        nes_bus.controller_state2 = p2;
        LogClear();
    }
}

void EmuWorker::loadNSF(NSFFile nsf)
{
    stop();

    currentNSF = std::move(nsf);
    const NSFInfo& info = currentNSF.Info();

    currentNSFSong = info.startingSong;
    nsfAudioAccumulator = 0.0;
    system_clock_counter = 0;
    dma_dummy_counter = 0;

    for (int i = 0; i < 2048; i++)
        nes_bus.ram[i] = 0x00;

    nes_bus.cart = nullptr;
    nes_bus.n_apu.reset();

    nes_bus.EnableNSFMode(info.loadAddress, currentNSF.ProgramData(), info.bankSwitch);

    nsfVRC6.reset(); nsfS5B.reset(); nsfVRC7.reset(); nsfMMC5.reset(); nsfN163.reset();
    nes_bus.nsfExpansionWrite = nullptr;
    nes_bus.nsfExpansionRead = nullptr;

    const bool useVRC6 = currentNSF.UsesVRC6();
    const bool useS5B = currentNSF.UsesS5B();
    const bool useVRC7 = currentNSF.UsesVRC7();
    const bool useMMC5 = currentNSF.UsesMMC5();
    const bool useN163 = currentNSF.UsesN163();

    if (useVRC6) { nsfVRC6 = std::make_unique<Mapper_024>(1, 1); nsfVRC6->reset(); emit nsfConsoleMessage("NSF VRC6 audio: ON"); }
    if (useS5B) { nsfS5B = std::make_unique<Mapper_069>(1, 1); nsfS5B->reset();  emit nsfConsoleMessage("NSF S5B audio: ON"); }
    if (useVRC7) { nsfVRC7 = std::make_unique<Mapper_085>(1, 1); nsfVRC7->reset(); emit nsfConsoleMessage("NSF VRC7 audio: ON"); }
    if (useN163) {
        nsfN163 = std::make_unique<Mapper_019>(1, 1);
        nsfN163->reset();
        uint32_t dummy = 0;
        nsfN163->cpuMapWrite(0xE000, dummy, 0x00);
        emit nsfConsoleMessage("NSF N163 audio: ON");
    }
    if (useMMC5) { nsfMMC5 = std::make_unique<Mapper_005>(1, 1); nsfMMC5->reset(); emit nsfConsoleMessage("NSF MMC5 audio: ON"); }

    emit nsfExpansionDetected(useVRC6, useS5B, useVRC7, useMMC5, useN163);

    if (nsfMMC5 || nsfN163)
    {
        nes_bus.nsfExpansionRead = [this](uint16_t addr, uint8_t& data) -> bool {
            if (nsfMMC5) {
                if (addr == 0x5015 || addr == 0x5204 || addr == 0x5205 || addr == 0x5206 ||
                    (addr >= 0x5C00 && addr <= 0x5FFF))
                    return nsfMMC5->cpuReadRegister(addr, data);
            }
            if (nsfN163) {
                if (addr >= 0x4800 && addr <= 0x4FFF)
                    return nsfN163->cpuReadRegister(addr, data);
            }
            return false;
            };
    }

    if (nsfVRC6 || nsfS5B || nsfVRC7 || nsfMMC5 || nsfN163)
    {
        nes_bus.nsfExpansionWrite = [this](uint16_t addr, uint8_t data) -> bool {
            uint32_t mapped_addr = 0;
            if (nsfMMC5) {
                if ((addr >= 0x5000 && addr <= 0x5015) || addr == 0x5205 || addr == 0x5206) {
                    nsfMMC5->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC6) {
                if ((addr >= 0x9000 && addr <= 0x9003) || (addr >= 0xA000 && addr <= 0xA003) || (addr >= 0xB000 && addr <= 0xB003)) {
                    nsfVRC6->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfN163) {
                if ((addr >= 0x4800 && addr <= 0x5FFF) || (addr >= 0x8000 && addr <= 0xFFFF)) {
                    nsfN163->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC7) {
                if (addr == 0x9010 || addr == 0x9030) {
                    nsfVRC7->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfS5B) {
                if (addr >= 0xC000 && addr <= 0xFFFF) {
                    nsfS5B->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            return false;
            };
    }

    nes_cpu.reset();
    nes_bus.cpuWrite(0x4015, 0x1F);
    nes_bus.cpuWrite(0x4017, 0x40);

    std::vector<float> dummyAudio;
    dummyAudio.reserve(4096);
    uint8_t songIndex = static_cast<uint8_t>(currentNSFSong - 1);
    callNSFRoutine(info.initAddress, songIndex, 0, 200000, dummyAudio);

    emit nsfConsoleMessage(QString("Title: %1").arg(QString::fromStdString(info.songName)));
    emit nsfConsoleMessage(QString("Artist: %1").arg(QString::fromStdString(info.artist)));
    emit nsfTrackChanged(currentNSFSong, info.totalSongs);

    start();
}

int EmuWorker::callNSFRoutine(uint16_t address, uint8_t a, uint8_t x, int maxCpuCycles, std::vector<float>& audioSamples)
{
    nes_cpu.a = a;
    nes_cpu.x = x;
    nes_cpu.y = 0;
    nes_cpu.stkp = 0xFD;
    nes_bus.cpuWrite(0x01FE, 0xFE);
    nes_bus.cpuWrite(0x01FF, 0xFF);
    nes_cpu.pc = address;

    int cycles = 0;
    while (nes_cpu.pc != 0xFFFF && cycles < maxCpuCycles)
    {
        nes_cpu.clock();
        nes_bus.n_apu.Step();

        if (nsfVRC6) nsfVRC6->irqStep();
        if (nsfS5B) nsfS5B->irqStep();
        if (nsfMMC5) nsfMMC5->ClockAudio();
        if (nsfN163) nsfN163->irqStep();

        nsfAudioAccumulator += 44100.0 / 1789773.0;
        if (nsfAudioAccumulator >= 1.0)
        {
            nsfAudioAccumulator -= 1.0;
            pushNSFAudioSample(audioSamples);
        }
        cycles++;
    }
    return cycles;
}

void EmuWorker::pushNSFAudioSample(std::vector<float>& audioSamples)
{
    const float MASTER_VOLUME = 0.95f;
    const float VRC6_GAIN = 1.0f, S5B_GAIN = 0.75f, VRC7_GAIN = 2.0f, MMC5_GAIN = 1.0f, N163_GAIN = 2.0f;
    float expL = 0.0f, expR = 0.0f;

    if (nsfVRC6) { float vL = 0, vR = 0; nsfVRC6->GetExpansionAudioStereo(vL, vR); expL += vL * VRC6_GAIN; expR += vR * VRC6_GAIN; }
    if (nsfS5B) { float v = nsfS5B->GetExpansionAudio() * S5B_GAIN; expL += v; expR += v; }
    if (nsfVRC7) { float v = nsfVRC7->GetExpansionAudio() * VRC7_GAIN; expL += v; expR += v; }
    if (nsfMMC5) { float v = nsfMMC5->GetExpansionAudio() * MMC5_GAIN; expL += v; expR += v; }
    if (nsfN163) { float v = nsfN163->GetExpansionAudio() * N163_GAIN; expL += v; expR += v; }

    float left = 0.0f, right = 0.0f, mono = 0.0f;
    const bool stereo = is_stereo;
    if (stereo) nes_bus.n_apu.GetOutputSampleStereo(left, right);
    else mono = nes_bus.n_apu.GetOutputSample();

    if (wantWaveDebug)
    {
        AudioDebugChannels dbg = nes_bus.n_apu.GetDebugChannels();
        if (nsfN163) {
            nsfN163->GetN163DebugChannels(dbg.n163Wave1, dbg.n163Wave2, dbg.n163Wave3, dbg.n163Wave4, dbg.n163Wave5, dbg.n163Wave6, dbg.n163Wave7, dbg.n163Wave8);
            nsfN163->GetN163DebugPeriods(dbg.n163Period1, dbg.n163Period2, dbg.n163Period3, dbg.n163Period4, dbg.n163Period5, dbg.n163Period6, dbg.n163Period7, dbg.n163Period8);
        }
        else if (nsfMMC5) {
            nsfMMC5->GetMMC5DebugChannels(dbg.mmc5Pulse1, dbg.mmc5Pulse2, dbg.mmc5PCM);
            nsfMMC5->GetMMC5DebugPeriods(dbg.mmc5Pulse1Period, dbg.mmc5Pulse2Period);
            nsfMMC5->GetMMC5DebugDuty(dbg.mmc5Pulse1Duty, dbg.mmc5Pulse2Duty);
        }
        else if (nsfVRC7) {
            nsfVRC7->GetVrc7DebugChannels(dbg.vrc7Wave1, dbg.vrc7Wave2, dbg.vrc7Wave3, dbg.vrc7Wave4, dbg.vrc7Wave5, dbg.vrc7Wave6);
            nsfVRC7->GetVrc7DebugPeriods(dbg.vrc7Wave1Period, dbg.vrc7Wave2Period, dbg.vrc7Wave3Period, dbg.vrc7Wave4Period, dbg.vrc7Wave5Period, dbg.vrc7Wave6Period);
            nsfVRC7->GetVrc7DebugPhases(dbg.vrc7Wave1Phase, dbg.vrc7Wave2Phase, dbg.vrc7Wave3Phase, dbg.vrc7Wave4Phase, dbg.vrc7Wave5Phase, dbg.vrc7Wave6Phase);
        }
        else if (nsfS5B) {
            nsfS5B->GetExpansionDebugChannels(dbg.s5bToneA, dbg.s5bToneB, dbg.s5bToneC);
            nsfS5B->GetS5BDebugPeriods(dbg.s5bToneAPeriod, dbg.s5bToneBPeriod, dbg.s5bToneCPeriod);
            nsfS5B->GetS5BDebugDuty(dbg.s5bToneADuty, dbg.s5bToneBDuty, dbg.s5bToneCDuty);
        }
        else if (nsfVRC6) {
            nsfVRC6->GetExpansionDebugChannels(dbg.vrc6Pulse1, dbg.vrc6Pulse2, dbg.vrc6Saw);
            nsfVRC6->GetVRC6DebugPeriods(dbg.vrc6Pulse1Period, dbg.vrc6Pulse2Period, dbg.vrc6SawPeriod);
            nsfVRC6->GetVRC6DebugDuty(dbg.vrc6Pulse1Duty, dbg.vrc6Pulse2Duty);
        }
        dbgBatch.push_back(dbg);
    }

    if (stereo)
    {
        left += expL; right += expR;
        // Đồng bộ filter chain thật với game loop: lọc SAU khi cộng expansion.
        nes_bus.n_apu.ApplyOutputFilterStereo(left, right);
        left = std::clamp(left * MASTER_VOLUME, -1.0f, 1.0f);
        right = std::clamp(right * MASTER_VOLUME, -1.0f, 1.0f);
        if (audioEQ.enabled)
        {
            left = audioEQ.ProcessLeft(left);
            right = audioEQ.ProcessRight(right);
        }
        audioSamples.push_back(left);
        audioSamples.push_back(right);
    }
    else
    {
        mono += (expL + expR) * 0.5f;
        mono = nes_bus.n_apu.ApplyOutputFilterMono(mono);
        mono = std::tanh(mono * MASTER_VOLUME);
        if (audioEQ.enabled)
        {
            mono = audioEQ.ProcessLeft(mono);
        }
        audioSamples.push_back(mono);
    }
}

void EmuWorker::runNSFFrame()
{
    constexpr double CPU_HZ = 1789773.0;
    constexpr double NSF_FPS = 60.0988;
    const qint64 FRAME_NS = static_cast<qint64>(1000000000.0 / NSF_FPS);

    if (!nsfFramePacer.isValid()) { nsfFramePacer.start(); nsfNextFrameNs = nsfFramePacer.nsecsElapsed(); }
    qint64 now = nsfFramePacer.nsecsElapsed();
    if (now < nsfNextFrameNs) return;
    if (now - nsfNextFrameNs > FRAME_NS * 5) nsfNextFrameNs = now;
    nsfNextFrameNs += FRAME_NS;

    dbgBatch.clear();
    std::vector<float> audioSamples;
    audioSamples.reserve(is_stereo ? 2048 : 1024);

    const NSFInfo& info = currentNSF.Info();
    constexpr int CPU_CYCLES_PER_FRAME = 29780;

    int usedCycles = callNSFRoutine(info.playAddress, 0, 0, 20000, audioSamples);
    int remainCycles = CPU_CYCLES_PER_FRAME - usedCycles;
    if (remainCycles < 0) remainCycles = 0;

    for (int i = 0; i < remainCycles; i++)
    {
        nes_bus.n_apu.Step();
        if (nsfVRC6) nsfVRC6->irqStep();
        if (nsfS5B) nsfS5B->irqStep();
        if (nsfMMC5) nsfMMC5->ClockAudio();
        if (nsfN163) nsfN163->irqStep();

        nsfAudioAccumulator += 44100.0 / CPU_HZ;
        if (nsfAudioAccumulator >= 1.0)
        {
            nsfAudioAccumulator -= 1.0;
            pushNSFAudioSample(audioSamples);
        }
    }

    if (!audioSamples.empty())
        emit audioReady(audioSamples, is_stereo);

    if (!dbgBatch.empty())
        emit debugChannelsReady(dbgBatch);

    emit gameFrameTicked();
}

void EmuWorker::restartCurrentNSFTrack()
{
    if (!nes_bus.nsfMode) return;
    const NSFInfo& info = currentNSF.Info();

    nsfAudioAccumulator = 0.0;
    for (int i = 0; i < 2048; i++) nes_bus.ram[i] = 0x00;
    nes_bus.EnableNSFMode(info.loadAddress, currentNSF.ProgramData(), info.bankSwitch);

    if (nsfVRC6) nsfVRC6->reset();
    if (nsfS5B) nsfS5B->reset();
    if (nsfVRC7) nsfVRC7->reset();
    if (nsfMMC5) nsfMMC5->reset();
    if (nsfN163) {
        nsfN163->reset();
        uint32_t dummy = 0;
        nsfN163->cpuMapWrite(0xE000, dummy, 0x00);
    }

    if (nsfVRC6 || nsfS5B || nsfVRC7 || nsfMMC5 || nsfN163)
    {
        nes_bus.nsfExpansionWrite = [this](uint16_t addr, uint8_t data) -> bool {
            uint32_t mapped_addr = 0;
            if (nsfMMC5) {
                if ((addr >= 0x5000 && addr <= 0x5015) || addr == 0x5205 || addr == 0x5206) {
                    nsfMMC5->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC6) {
                if ((addr >= 0x9000 && addr <= 0x9003) || (addr >= 0xA000 && addr <= 0xA003) || (addr >= 0xB000 && addr <= 0xB003)) {
                    nsfVRC6->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfN163) {
                if ((addr >= 0x4800 && addr <= 0x5FFF) || (addr >= 0x8000 && addr <= 0xFFFF)) {
                    nsfN163->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfVRC7) {
                if (addr == 0x9010 || addr == 0x9030) {
                    nsfVRC7->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            if (nsfS5B) {
                if (addr >= 0xC000 && addr <= 0xFFFF) {
                    nsfS5B->cpuMapWrite(addr, mapped_addr, data); return true;
                }
            }
            return false;
            };

        nes_bus.nsfExpansionRead = nullptr;
        if (nsfMMC5 || nsfN163)
        {
            nes_bus.nsfExpansionRead = [this](uint16_t addr, uint8_t& data) -> bool {
                if (nsfMMC5) {
                    if (addr == 0x5015 || addr == 0x5204 || addr == 0x5205 || addr == 0x5206 ||
                        (addr >= 0x5C00 && addr <= 0x5FFF))
                        return nsfMMC5->cpuReadRegister(addr, data);
                }
                if (nsfN163) {
                    if (addr >= 0x4800 && addr <= 0x4FFF)
                        return nsfN163->cpuReadRegister(addr, data);
                }
                return false;
                };
        }
    }

    nes_bus.cpuWrite(0x4015, 0x1F);
    nes_bus.cpuWrite(0x4017, 0x40);

    uint8_t songIndex = static_cast<uint8_t>(currentNSFSong - 1);
    std::vector<float> dummyAudio;
    dummyAudio.reserve(4096);
    callNSFRoutine(info.initAddress, songIndex, 0, 200000, dummyAudio);

    emit nsfTrackChanged(currentNSFSong, info.totalSongs);
}

void EmuWorker::nsfNextTrack()
{
    if (!nes_bus.nsfMode) return;
    const NSFInfo& info = currentNSF.Info();
    currentNSFSong++;
    if (currentNSFSong > info.totalSongs) currentNSFSong = 1;
    restartCurrentNSFTrack();
}

void EmuWorker::nsfPrevTrack()
{
    if (!nes_bus.nsfMode) return;
    const NSFInfo& info = currentNSF.Info();
    if (currentNSFSong <= 1) currentNSFSong = info.totalSongs;
    else currentNSFSong--;
    restartCurrentNSFTrack();
}