#pragma once
#include <QFile>
#include <QString>
#include <cstdint>
#include <vector>

// AviWriter - ghi raw AVI (video BGR24 không nén + audio PCM16 không nén)
// Dùng cho SJNES: cắm vào EmuWorker::frameReady / audioReady
class AviWriter {
public:
    AviWriter() = default;
    ~AviWriter() { if (m_open) stop(); }

    // width/height: kích thước khung hình NES (thường 256x240)
    // sampleRate: sample rate audio hiện tại của APU mixer (vd 44100)
    bool start(const QString& path, int width, int height, int fps, int sampleRate, int channels = 1);

    // rgbBuffer: buffer từ PPU::RGBColor, size = width*height*3 (RGB24)
    void writeVideoFrame(const uint8_t* rgbBuffer, size_t sizeBytes);

    // pcm: PCM 16-bit signed, interleaved nếu channels>1
    void writeAudioSamples(const int16_t* pcm, size_t sampleCount);

    void stop(); // patch lại header + đóng file

    bool isRecording() const { return m_open; }

private:
    QFile m_file;
    bool m_open = false;

    int m_width = 0, m_height = 0, m_fps = 0;
    int m_sampleRate = 0, m_channels = 1;

    uint32_t m_videoFrameCount = 0;
    uint32_t m_audioSampleCount = 0;

    // vị trí các field cần patch lại size sau khi ghi xong
    qint64 m_riffSizePos = 0;
    qint64 m_moviSizePos = 0;
    qint64 m_aviHFramesPos = 0;
    qint64 m_strhVidLengthPos = 0;
    qint64 m_strhAudLengthPos = 0;

    // idx1: (fourcc, flags, offset-from-movi, size) cho mỗi chunk đã ghi
    struct IdxEntry { char fourcc[4]; uint32_t flags; uint32_t offset; uint32_t size; };
    std::vector<IdxEntry> m_index;
    qint64 m_moviDataStart = 0;

    void writeHeader();
    void writeChunk(const char fourcc[4], const uint8_t* data, uint32_t size, bool isKeyFrame);
    void patch32(qint64 pos, uint32_t value);
};