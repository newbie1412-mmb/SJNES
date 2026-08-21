#include "AviWriter.h"
#include <cstring>

// ---- helper ghi thô ----
static void writeU32(QFile& f, uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); }
static void writeU16(QFile& f, uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); }
static void writeFourCC(QFile& f, const char* s) { f.write(s, 4); }

bool AviWriter::start(const QString& path, int width, int height, int fps, int sampleRate, int channels) {
    if (m_open) stop();

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    m_width = width; m_height = height; m_fps = fps;
    m_sampleRate = sampleRate; m_channels = channels;
    m_videoFrameCount = 0; m_audioSampleCount = 0;
    m_index.clear();

    writeHeader();
    m_open = true;
    return true;
}

// Cấu trúc AVI tối giản: RIFF/AVI -> hdrl (avih+strl video+strl audio) -> movi -> idx1
void AviWriter::writeHeader() {
    QFile& f = m_file;

    // RIFF header
    writeFourCC(f, "RIFF");
    m_riffSizePos = f.pos();
    writeU32(f, 0); // patch sau
    writeFourCC(f, "AVI ");

    // ---- hdrl LIST ----
    writeFourCC(f, "LIST");
    qint64 hdrlSizePos = f.pos();
    writeU32(f, 0);
    qint64 hdrlStart = f.pos();
    writeFourCC(f, "hdrl");

    // avih (main header)
    writeFourCC(f, "avih");
    writeU32(f, 56); // size of avih struct
    writeU32(f, 1000000 / m_fps);      // dwMicroSecPerFrame
    writeU32(f, 0);                    // dwMaxBytesPerSec (không quan trọng)
    writeU32(f, 0);                    // dwPaddingGranularity
    writeU32(f, 0x10);                 // dwFlags: AVIF_HASINDEX
    m_aviHFramesPos = f.pos();
    writeU32(f, 0);                    // dwTotalFrames - patch sau
    writeU32(f, 0);                    // dwInitialFrames
    writeU32(f, 2);                    // dwStreams (video + audio)
    writeU32(f, m_width * m_height * 3); // dwSuggestedBufferSize
    writeU32(f, m_width);
    writeU32(f, m_height);
    writeU32(f, 0); writeU32(f, 0); writeU32(f, 0); writeU32(f, 0); // reserved[4]

    // ---- strl (video) ----
    writeFourCC(f, "LIST");
    qint64 strlVidSizePos = f.pos(); writeU32(f, 0);
    qint64 strlVidStart = f.pos();
    writeFourCC(f, "strl");

    writeFourCC(f, "strh");
    writeU32(f, 56);
    writeFourCC(f, "vids");
    writeU32(f, 0); // handler = 0 (thay vì fourCC "DIB "): một số app Windows
    // (Media Player/Photos) nhận diện sai "DIB " và tự chọn
    // nhầm codec giải mã, gây đảo kênh màu G/B. handler=0 báo
    // rõ đây là raw uncompressed, ít gây nhầm hơn.
    writeU32(f, 0);  // dwFlags
    writeU16(f, 0);  writeU16(f, 0); // priority, language
    writeU32(f, 0);  // dwInitialFrames
    writeU32(f, 1);  // dwScale
    writeU32(f, m_fps); // dwRate -> fps = rate/scale
    writeU32(f, 0);  // dwStart
    m_strhVidLengthPos = f.pos();
    writeU32(f, 0);  // dwLength - patch sau (số frame video)
    writeU32(f, m_width * m_height * 3); // dwSuggestedBufferSize
    writeU32(f, 0xFFFFFFFF); // dwQuality
    writeU32(f, 0);  // dwSampleSize
    writeU16(f, 0); writeU16(f, 0); writeU16(f, (uint16_t)m_width); writeU16(f, (uint16_t)m_height); // rcFrame

    writeFourCC(f, "strf");
    writeU32(f, 40); // BITMAPINFOHEADER size
    writeU32(f, 40); // biSize
    writeU32(f, (uint32_t)m_width);
    writeU32(f, (uint32_t)m_height);
    writeU16(f, 1);  // biPlanes
    writeU16(f, 24); // biBitCount (BGR24)
    writeU32(f, 0);  // BI_RGB
    writeU32(f, m_width * m_height * 3);
    writeU32(f, 0); writeU32(f, 0); writeU32(f, 0); writeU32(f, 0);

    patch32(strlVidSizePos, (uint32_t)(f.pos() - strlVidStart));

    // ---- strl (audio) ----
    writeFourCC(f, "LIST");
    qint64 strlAudSizePos = f.pos(); writeU32(f, 0);
    qint64 strlAudStart = f.pos();
    writeFourCC(f, "strl");

    writeFourCC(f, "strh");
    writeU32(f, 56);
    writeFourCC(f, "auds");
    writeU32(f, 0); // handler (0 = PCM)
    writeU32(f, 0);
    writeU16(f, 0); writeU16(f, 0);
    writeU32(f, 0);
    writeU32(f, 1);            // dwScale
    writeU32(f, (uint32_t)m_sampleRate); // dwRate
    writeU32(f, 0);
    m_strhAudLengthPos = f.pos();
    writeU32(f, 0); // dwLength - patch sau (số sample)
    writeU32(f, 0);
    writeU32(f, 0xFFFFFFFF);
    writeU32(f, 2 * m_channels); // dwSampleSize (bytes per sample block)
    writeU16(f, 0); writeU16(f, 0); writeU16(f, 0); writeU16(f, 0);

    writeFourCC(f, "strf"); // WAVEFORMATEX
    writeU32(f, 18);
    writeU16(f, 1); // WAVE_FORMAT_PCM
    writeU16(f, (uint16_t)m_channels);
    writeU32(f, (uint32_t)m_sampleRate);
    writeU32(f, (uint32_t)(m_sampleRate * m_channels * 2)); // avg bytes/sec
    writeU16(f, (uint16_t)(m_channels * 2)); // block align
    writeU16(f, 16); // bits per sample
    writeU16(f, 0);  // cbSize

    patch32(strlAudSizePos, (uint32_t)(f.pos() - strlAudStart));
    patch32(hdrlSizePos, (uint32_t)(f.pos() - hdrlStart));

    // ---- movi LIST ----
    writeFourCC(f, "LIST");
    m_moviSizePos = f.pos();
    writeU32(f, 0);
    writeFourCC(f, "movi");
    m_moviDataStart = f.pos(); // offset gốc cho idx1 (offset tính từ đây theo chuẩn cũ)
}

void AviWriter::writeChunk(const char fourcc[4], const uint8_t* data, uint32_t size, bool isKeyFrame) {
    qint64 chunkOffset = m_file.pos() - m_moviDataStart; // offset tương đối tính từ đầu 'movi' data
    writeFourCC(m_file, fourcc);
    writeU32(m_file, size);
    m_file.write(reinterpret_cast<const char*>(data), size);
    if (size % 2 != 0) { char pad = 0; m_file.write(&pad, 1); } // AVI chunk phải align 2 byte

    IdxEntry e{};
    memcpy(e.fourcc, fourcc, 4);
    e.flags = isKeyFrame ? 0x10 : 0; // AVIIF_KEYFRAME
    e.offset = (uint32_t)chunkOffset;
    e.size = size;
    m_index.push_back(e);
}

void AviWriter::writeVideoFrame(const uint8_t* bgrBuffer, size_t sizeBytes) {
    if (!m_open) return;
    // bgrBuffer PHẢI đã là BGR24 sẵn (đúng thứ tự AVI DIB cần) - không swap ở đây nữa.
    // AVI lưu ảnh từ dưới lên (bottom-up); nếu buffer đang top-down thì đảo dòng:
    static thread_local std::vector<uint8_t> flipped;
    flipped.resize(sizeBytes);
    int stride = m_width * 3;
    for (int y = 0; y < m_height; ++y)
        memcpy(&flipped[y * stride], &bgrBuffer[(m_height - 1 - y) * stride], stride);

    writeChunk("00dc", flipped.data(), (uint32_t)sizeBytes, true);
    m_videoFrameCount++;
}

void AviWriter::writeAudioSamples(const int16_t* pcm, size_t sampleCount) {
    if (!m_open) return;
    uint32_t bytes = (uint32_t)(sampleCount * sizeof(int16_t));
    writeChunk("01wb", reinterpret_cast<const uint8_t*>(pcm), bytes, true);
    m_audioSampleCount += (uint32_t)sampleCount;
}

void AviWriter::patch32(qint64 pos, uint32_t value) {
    qint64 cur = m_file.pos();
    m_file.seek(pos);
    writeU32(m_file, value);
    m_file.seek(cur);
}

void AviWriter::stop() {
    if (!m_open) return;

    QFile& f = m_file;

    // patch size của 'movi' LIST
    qint64 moviEnd = f.pos();
    patch32(m_moviSizePos, (uint32_t)(moviEnd - m_moviDataStart + 4)); // +4 cho fourcc 'movi'

    // ghi idx1
    writeFourCC(f, "idx1");
    writeU32(f, (uint32_t)(m_index.size() * 16));
    for (auto& e : m_index) {
        f.write(e.fourcc, 4);
        writeU32(f, e.flags);
        writeU32(f, e.offset);
        writeU32(f, e.size);
    }

    // patch tổng size file (RIFF size = filesize - 8)
    qint64 fileEnd = f.pos();
    patch32(m_riffSizePos, (uint32_t)(fileEnd - 8));

    // patch số frame / số sample
    patch32(m_aviHFramesPos, m_videoFrameCount);
    patch32(m_strhVidLengthPos, m_videoFrameCount);
    patch32(m_strhAudLengthPos, m_audioSampleCount);

    f.close();
    m_open = false;
}