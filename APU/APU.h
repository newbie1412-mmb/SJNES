#pragma once
#include <cstdint>
#include <vector>
#include "F1.h"
#include "F2.h"
#include "Triangle.h"
#include "DMC.h"
#include "Noise.h"
#include "Mapper_024.h"
#include <QMetaType>
#include <QString>
struct AudioDebugChannels {
    float pulse1 = 0.0f;
    float pulse2 = 0.0f;
    float triangle = 0.0f;
    float noise = 0.0f;
    float dmc = 0.0f;

    float vrc6Pulse1 = 0.0f;
    float vrc6Pulse2 = 0.0f;
    float vrc6Saw = 0.0f;

    float s5bToneA = 0.0f;
    float s5bToneB = 0.0f;
    float s5bToneC = 0.0f;

    float vrc7Wave1 = 0.0f;
    float vrc7Wave2 = 0.0f;
    float vrc7Wave3 = 0.0f;
    float vrc7Wave4 = 0.0f;
    float vrc7Wave5 = 0.0f;
    float vrc7Wave6 = 0.0f;

    float mmc5Pulse1 = 0.0f;
    float mmc5Pulse2 = 0.0f;
    float mmc5PCM = 0.0f;

    float n163Wave1 = 0.0f;
    float n163Wave2 = 0.0f;
    float n163Wave3 = 0.0f;
    float n163Wave4 = 0.0f;
    float n163Wave5 = 0.0f;
    float n163Wave6 = 0.0f;
    float n163Wave7 = 0.0f;
    float n163Wave8 = 0.0f;

    float pulse1Period = 0.0f;
    float pulse2Period = 0.0f;
    float trianglePeriod = 0.0f;
    float noisePeriod = 0.0f;
    float dmcPeriod = 0.0f;

    float vrc6Pulse1Period = 0.0f;
    float vrc6Pulse2Period = 0.0f;
    float vrc6SawPeriod = 0.0f;

    float s5bToneAPeriod = 0.0f;
    float s5bToneBPeriod = 0.0f;
    float s5bToneCPeriod = 0.0f;

    float vrc7Wave1Period = 0.0f;
    float vrc7Wave2Period = 0.0f;
    float vrc7Wave3Period = 0.0f;
    float vrc7Wave4Period = 0.0f;
    float vrc7Wave5Period = 0.0f;
    float vrc7Wave6Period = 0.0f;

    float vrc7Wave1Phase = 0.0f;
    float vrc7Wave2Phase = 0.0f;
    float vrc7Wave3Phase = 0.0f;
    float vrc7Wave4Phase = 0.0f;
    float vrc7Wave5Phase = 0.0f;
    float vrc7Wave6Phase = 0.0f;

    float mmc5Pulse1Period = 0.0f;
    float mmc5Pulse2Period = 0.0f;

    float n163Period1 = 0.0f;
    float n163Period2 = 0.0f;
    float n163Period3 = 0.0f;
    float n163Period4 = 0.0f;
    float n163Period5 = 0.0f;
    float n163Period6 = 0.0f;
    float n163Period7 = 0.0f;
    float n163Period8 = 0.0f;

    float pulse1Duty = -1.0f;
    float pulse2Duty = -1.0f;
    float trianglePhase = -1.0f;

    float vrc6Pulse1Duty = -1.0f;
    float vrc6Pulse2Duty = -1.0f;

    float s5bToneADuty = 0.5f;
    float s5bToneBDuty = 0.5f;
    float s5bToneCDuty = 0.5f;

    float mmc5Pulse1Duty = -1.0f;
    float mmc5Pulse2Duty = -1.0f;
};
Q_DECLARE_METATYPE(AudioDebugChannels)
class Bus;

class APU {
public:
    APU();
    ~APU() = default;
    void reset();
    Bus* bus = nullptr;

    double             sample_counter = 0.0;
    double             cycles_per_sample = 1789773.0 / 44100.0;
    std::vector<float> audio_buffer;
    AudioDebugChannels GetDebugChannels();
    void    cpuWrite(uint16_t addr, uint8_t data);
    uint8_t cpuRead(uint16_t addr);
    void    Step();
    float   GetOutputSample();
    void    GetOutputSampleStereo(float& left, float& right);
    // Áp filter chain thật (2 highpass + 1 lowpass, giả lập mạch NES) lên TỔNG
    // đã cộng cả NES 5 kênh + expansion audio (VRC6/S5B/VRC7/MMC5/N163).
    // Gọi hàm này SAU KHI cộng expansion vào, KHÔNG gọi trước, vì trên phần cứng
    // thật expansion audio hòa vào cùng đường tín hiệu analog trước khi qua filter.
    void    ApplyOutputFilterStereo(float& left, float& right);
    float   ApplyOutputFilterMono(float sample);
    bool mutePulse1 = false;
    bool mutePulse2 = false;
    bool muteTriangle = false;
    bool muteNoise = false;
    bool muteDMC = false;

    // Gain riêng từng kênh (0.0 - 2.0, mặc định 1.0 = không đổi so với gốc).
    // Tách biệt hoàn toàn với mute: mute = tắt hẳn, gain = chỉnh to/nhỏ khi
    // KHÔNG mute. Áp dụng ngay tại GetOutputSampleStereo(), trước khi mix TND/Pulse.
    float gainPulse1 = 1.0f;
    float gainPulse2 = 1.0f;
    float gainTriangle = 1.0f;
    float gainNoise = 1.0f;
    float gainDMC = 1.0f;

    // channelId khớp với chuỗi SettingsDialog gửi qua channelGainChanged:
    // "nes.pulse1", "nes.pulse2", "nes.triangle", "nes.noise", "nes.dmc".
    // Kênh không khớp id nào thì bị bỏ qua (không crash), để mở rộng dần
    // sang expansion chip (VRC6/S5B/VRC7/MMC5/N163) ở các mapper riêng sau này.
    void SetChannelGain(const QString& channelId, float gain);
    bool smoothSawEnabled = false;
    void SetSmoothSaw(bool enable);
    // Bật/tắt "Reverse DPCM Bit Order" — bù cho ROM encode sample DMC sai thứ tự bit
    // (Double Dribble, Gimmick!, một số famiclone...). Mặc định tắt.
    void SetReverseDpcmBits(bool enable) { dmc.reverseBits = enable; }
    bool GetReverseDpcmBits() const { return dmc.reverseBits; }
    // Bật/tắt "Reduce popping sounds on DMC channel" — làm mượt (ramp +-2 mỗi
    // timer clock) khi CPU ghi trực tiếp $4011 thay vì set output_level ngay
    // lập tức, tránh tiếng click/pop. Mặc định tắt (giữ hardware-accurate).
    void SetDMCReducePopping(bool enable) { dmc.reducePopping = enable; }
    bool GetDMCReducePopping() const { return dmc.reducePopping; }
    Mapper* mapper = nullptr;
    void SetSmoothTriangle(bool smooth) { tri.smooth = smooth; }
    bool GetSmoothTriangle() const { return tri.smooth; }
    uint8_t readStatus();

private:
    F1       f1;
    F2       f2;
    Triangle tri;
    DMC      dmc;
    Noise    noise;

    int  frame_seq_count = 0;
    bool use_5step_mode = false;
    bool apu_half_clock = false;
    bool frame_irq_flag = false;
    bool irq_inhibit = false;

    float hp1 = 0.0f, prev_in1 = 0.0f;
    float hp2 = 0.0f, prev_in2 = 0.0f;
    float lp = 0.0f;

    float hp1L = 0.0f, prev_in1L = 0.0f;
    float hp2L = 0.0f, prev_in2L = 0.0f;
    float lpL = 0.0f;
    float hp1R = 0.0f, prev_in1R = 0.0f;
    float hp2R = 0.0f, prev_in2R = 0.0f;
    float lpR = 0.0f;
};