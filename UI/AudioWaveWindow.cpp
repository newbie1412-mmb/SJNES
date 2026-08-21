#include "AudioWaveWindow.h"
#include <QPainterPath>
#include <QPainter>
#include <QMutexLocker>
#include <QPen>
#include <QColor>
#include <algorithm>
#include <cmath>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QOpenGLFunctions>

// Màu sóng: trắng đồng bộ
static const QColor CHANNEL_COLORS[] = {
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
    QColor(235, 235, 235),
};

// Catmull-Rom cubic interpolation
static float catmullRom(float p0, float p1, float p2, float p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
        );
}

// Lấy sample với Catmull-Rom (index có thể là float)
static float sampleInterp(const QVector<float>& buf, float fidx)
{
    int n = buf.size();
    int i1 = (int)fidx;
    float t = fidx - i1;

    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, n - 1);
    int i3 = std::min(i1 + 2, n - 1);

    return catmullRom(buf[i0], buf[i1], buf[i2], buf[i3], t);
}
static float sampleLinear(const QVector<float>& buf, float fidx)
{
    int n = buf.size();
    int i0 = std::clamp((int)fidx, 0, n - 1);
    int i1 = std::clamp(i0 + 1, 0, n - 1);
    float t = fidx - float(i0);
    return buf[i0] + (buf[i1] - buf[i0]) * t;
}

// Catmull-Rom cho VRC6 Saw: cong mượt như corrscope trong từng đoạn ramp,
// nhưng chặn không cho control point kéo qua điểm reset (falling edge)
// để tránh overshoot giả ở đỉnh ramp / đáy reset.
// `buf` là dữ liệu để LẤY GIÁ TRỊ nội suy (có thể đã qua RC filter),
// `rawBuf` là dữ liệu GỐC dùng để PHÁT HIỆN điểm reset chính xác
// (vì sau khi lọc RC, chênh lệch tại điểm reset có thể bị làm mượt bớt,
// khiến ngưỡng phát hiện không còn đáng tin cậy nếu đo trên buf đã lọc).
static float sampleSawSmooth(const QVector<float>& buf, const QVector<float>& rawBuf, float fidx)
{
    int n = buf.size();
    int i1 = std::clamp((int)fidx, 0, n - 1);
    float t = fidx - float((int)fidx);

    int i0 = std::max(i1 - 1, 0);
    int i2 = std::min(i1 + 1, n - 1);
    int i3 = std::min(i1 + 2, n - 1);

    const float kResetDrop = 0.06f; // cùng ngưỡng dùng khi detect trigger falling edge

    auto isReset = [&](int a, int b) -> bool
        {
            return (rawBuf[a] - rawBuf[b]) > kResetDrop;
        };

    // Nếu chính đoạn đang nội suy (i1 -> i2) là điểm reset,
    // dùng linear thẳng để giữ cạnh rơi sắc nét, không làm mượt qua nó.
    if (isReset(i1, i2))
        return buf[i1] + (buf[i2] - buf[i1]) * t;

    float v0 = buf[i0];
    float v1 = buf[i1];
    float v2 = buf[i2];
    float v3 = buf[i3];

    if (isReset(i0, i1))
        v0 = v1;

    // Tương tự cho i3 nếu nó đã sang chu kỳ kế tiếp.
    if (isReset(i2, i3))
        v3 = v2;

    return catmullRom(v0, v1, v2, v3, t);
}

static float findTrigger(const QVector<float>& buf, int searchStart, int searchEnd, float trigLevel)
{
    float bestIdx = float(searchStart);
    float bestSlope = -1.0f;
    bool  found = false;

    for (int i = searchEnd; i >= searchStart + 1; i--)
    {
        float prev = buf[i - 1];
        float cur = buf[i];

        if (prev < trigLevel && cur >= trigLevel)
        {
            float slope = cur - prev;  // manh hơn = ổn định hơn

            if (!found || slope > bestSlope)
            {
                bestSlope = slope;
                float frac = (slope > 0.0f) ? (trigLevel - prev) / slope : 0.0f;
                bestIdx = float(i - 1) + frac;
                found = true;

                if (slope > 0.3f)
                    break;
            }
        }
    }

    return bestIdx;
}
//saw và tam giác lọc riêng =)) vì lọc mạnh quá nó xấu vl
static QVector<float> computeRealFilterBuffer(const QVector<float>& raw, bool isLightFilter = false)
{
    int n = raw.size();
    QVector<float> out(n);
    if (n == 0) return out;

    float hp1 = 0.0f, prevIn1 = raw[0];
    float hp2 = 0.0f, prevIn2 = 0.0f;
    float lp = 0.0f;

    for (int i = 0; i < n; i++)
    {
        float x = raw[i];

        if (isLightFilter)
        {
            hp1 = 0.998f * (hp1 + x - prevIn1);   prevIn1 = x;
            hp2 = 0.994f * (hp2 + hp1 - prevIn2); prevIn2 = hp1;
            lp += 0.960f * (hp2 - lp); 
        }
        else
        {
            hp1 = 0.997f * (hp1 + x - prevIn1);   prevIn1 = x;
            hp2 = 0.992f * (hp2 + hp1 - prevIn2); prevIn2 = hp1;
            lp += 0.930f * (hp2 - lp);
        }

        out[i] = lp;
    }
    return out;
}

static bool validPhase01(float p)
{
    return std::isfinite(p) && p >= 0.0f && p <= 1.0f;
}

// VRC7 PHASE-BUFFER TRIGGER:
// Mỗi sample waveform có một phase carrier đi kèm. Ta tìm điểm phase wrap (1 -> 0)
// gần mép trái vùng hiển thị để làm fStart. Như vậy VRC7 đứng yên theo phase thật của
// emu2413, không còn phụ thuộc zero-crossing của dạng sóng FM nhiều cạnh phụ.
static float findVrc7PhaseStart(
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    bool& ok
)
{
    ok = false;

    if (phase.size() != n || n < visibleSamples + 4)
        return float(n - visibleSamples);

    if (!(periodHint >= 4.0f && periodHint <= 8192.0f))
        return float(n - visibleSamples);

    const float desired = float(n - visibleSamples);
    const int center = std::clamp(int(std::round(desired)), 1, n - 2);
    const int radius = std::clamp(int(periodHint * 2.5f), 24, std::max(32, visibleSamples * 3));

    const int scanStart = std::max(1, center - radius);
    const int scanEnd = std::min(n - 2, center + radius);

    float bestIdx = -1.0f;
    float bestDist = 1e30f;

    for (int i = scanStart + 1; i <= scanEnd; ++i)
    {
        float prev = phase[i - 1];
        float cur = phase[i];

        if (!validPhase01(prev) || !validPhase01(cur))
            continue;

        // Wrap thật của carrier phase. Dùng ngưỡng rộng để tránh nhầm vibrato nhỏ thành wrap.
        if (prev > 0.55f && cur < 0.45f && (prev - cur) > 0.25f)
        {
            float denom = (1.0f - prev) + cur;
            float frac = (denom > 0.000001f) ? (1.0f - prev) / denom : 0.0f;
            frac = std::clamp(frac, 0.0f, 1.0f);

            float idx = float(i - 1) + frac;
            float dist = std::abs(idx - desired);

            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = idx;
            }
        }
    }

    if (bestIdx >= 0.0f)
    {
        ok = true;
        return bestIdx;
    }

    // Fallback cho nốt quá trầm / chưa có wrap trong cửa sổ: dùng phase tại vị trí desired
    // để suy ra phase-0 gần đó. Fallback này vẫn dùng phase buffer theo sample, không dùng
    // zero-crossing waveform.
    int di = std::clamp(int(desired), 0, n - 1);
    float ph = phase[di];
    if (validPhase01(ph))
    {
        float start = desired - ph * periodHint;

        while (start < desired - periodHint * 0.5f)
            start += periodHint;
        while (start > desired + periodHint * 0.5f)
            start -= periodHint;

        ok = true;
        return start;
    }

    return float(n - visibleSamples);
}


// Tạo fingerprint nhỏ cho một cửa sổ waveform VRC7.
// Đây là kiểu CorrScope: không bắt một cạnh zero-cross duy nhất nữa, mà so cả hình sóng
// quanh vùng hiển thị với hình của frame trước. Nhờ vậy VRC7/FM không bị nhảy sang
// cạnh phụ khác nhau mỗi frame.
static void buildVrc7CorrRef(
    const QVector<float>& wave,
    float start,
    int visibleSamples,
    QVector<float>& out
)
{
    const int n = wave.size();
    const int refPoints = 128;
    out.resize(refPoints);

    if (n < 8 || visibleSamples < 8)
    {
        for (int i = 0; i < refPoints; ++i) out[i] = 0.0f;
        return;
    }

    start = std::clamp(start, 0.0f, float(std::max(0, n - visibleSamples - 2)));
    int a = std::clamp(int(std::floor(start)), 0, n - 1);
    int b = std::clamp(int(std::ceil(start + float(visibleSamples))), 0, n - 1);

    float mn = wave[a];
    float mx = wave[a];
    for (int i = a + 1; i <= b; ++i)
    {
        mn = std::min(mn, wave[i]);
        mx = std::max(mx, wave[i]);
    }

    const float center = (mn + mx) * 0.5f;
    const float half = std::max((mx - mn) * 0.5f, 0.001f);

    for (int i = 0; i < refPoints; ++i)
    {
        float t = float(i) / float(refPoints - 1);
        float idx = start + t * float(visibleSamples);
        idx = std::clamp(idx, 0.0f, float(n - 2));

        float v = sampleInterp(wave, idx);
        out[i] = std::clamp((v - center) / half, -1.35f, 1.35f);
    }
}

static float vrc7RefError(const QVector<float>& a, const QVector<float>& b)
{
    if (a.size() != b.size() || a.isEmpty())
        return 1e30f;

    float err = 0.0f;
    for (int i = 0; i < a.size(); ++i)
    {
        float d = a[i] - b[i];
        err += d * d;
    }
    return err / float(a.size());
}

// CorrScope-style VRC7 start picker + FIXED X ANCHOR:
// Bản gốc đã đứng yên tốt, nhưng khi VRC7 vibrato/pitch uốn cao-thấp-cao-thấp,
// nếu lấy phase-wrap làm mép trái hiển thị thì cả hàng waveform sẽ bị kéo trôi ngang.
// Cách này vẫn giữ logic CorrScope correlation của bản ổn, nhưng đổi ý nghĩa candidate:
// candidate là phase-wrap thật được NEO ở một vị trí X cố định trong màn hình.
// Khi period thay đổi, waveform sẽ co/dãn ngang quanh điểm neo đó, không trượt cả hàng.
static float findVrc7CorrscopeStart(
    const QVector<float>& wave,
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    const QVector<float>& prevRef,
    bool hasPrevRef,
    QVector<float>& newRef,
    bool& ok,
    qint64 absBase,
    qint64& lastAnchorAbs,
    bool& hasLastAnchorAbs
)
{
    ok = false;
    newRef.clear();

    if (phase.size() != n || wave.size() != n || n < visibleSamples + 8)
        return float(n - visibleSamples);

    const float defaultStart = float(n - visibleSamples);
    const bool hasHint = (periodHint >= 4.0f && periodHint <= 8192.0f);
    const float anchorOffset = float(visibleSamples) * 0.50f;
    const float desiredAnchor = defaultStart + anchorOffset;

    QVector<float> anchorCandidates;
    anchorCandidates.reserve(256);

    int radius = visibleSamples * 3;
    if (hasHint)
        radius = std::clamp(int(periodHint * 10.0f), visibleSamples, visibleSamples * 5);

    const int scanStart = std::max(1, int(desiredAnchor) - radius);
    const int scanEnd = std::min(n - 2, int(desiredAnchor) + radius);

    for (int i = scanStart + 1; i <= scanEnd; ++i)
    {
        float prev = phase[i - 1];
        float cur = phase[i];

        if (!validPhase01(prev) || !validPhase01(cur))
            continue;

        // Phase wrap thật của carrier: gần 1.0 nhảy về 0.0.
        if (prev > 0.55f && cur < 0.45f && (prev - cur) > 0.25f)
        {
            float denom = (1.0f - prev) + cur;
            float frac = (denom > 0.000001f) ? (1.0f - prev) / denom : 0.0f;
            frac = std::clamp(frac, 0.0f, 1.0f);
            float idx = float(i - 1) + frac;

            // Candidate là phase-wrap sẽ được đặt vào đúng X anchor cố định.
            float candidateStart = idx - anchorOffset;
            if (candidateStart >= 0.0f && candidateStart <= float(n - visibleSamples - 2))
                anchorCandidates.push_back(idx);
        }
    }

    // Fallback: nếu chưa bắt được wrap trong vùng, dùng phase ngay tại X anchor để suy ra phase-0.
    if (anchorCandidates.isEmpty() && hasHint)
    {
        int ai = std::clamp(int(std::round(desiredAnchor)), 0, n - 1);
        float ph = phase[ai];
        if (validPhase01(ph))
        {
            if (anchorCandidates.isEmpty() && hasHint)
            {
                int ai = std::clamp(int(std::round(desiredAnchor)), 0, n - 1);
                float ph = phase[ai];

                if (validPhase01(ph))
                {
                    const float maxStart = float(n - visibleSamples - 2);
                    const float baseAnchor = desiredAnchor - ph * periodHint;

                    for (int k = -16; k <= 16; ++k)
                    {
                        float anchor = baseAnchor + float(k) * periodHint;
                        float candidateStart = anchor - anchorOffset;

                        if (candidateStart >= 0.0f && candidateStart <= maxStart)
                            anchorCandidates.push_back(anchor);
                    }

                    if (anchorCandidates.isEmpty())
                    {
                        float anchor = baseAnchor;
                        while (anchor < desiredAnchor - periodHint * 0.5f)
                            anchor += periodHint;
                        while (anchor > desiredAnchor + periodHint * 0.5f)
                            anchor -= periodHint;

                        float candidateStart = anchor - anchorOffset;
                        candidateStart = std::clamp(candidateStart, 0.0f, maxStart);
                        anchorCandidates.push_back(candidateStart + anchorOffset);
                    }
                }
            }
        }
    }

    if (anchorCandidates.isEmpty())
        return defaultStart;

    float bestStart = defaultStart;
    float bestScore = 1e30f;
    QVector<float> bestRef;

    for (float anchor : anchorCandidates)
    {
        float candStart = anchor - anchorOffset;
        candStart = std::clamp(candStart, 0.0f, float(n - visibleSamples - 2));

        QVector<float> ref;
        buildVrc7CorrRef(wave, candStart, visibleSamples, ref);

        float score = 0.0f;
        if (hasPrevRef && prevRef.size() == ref.size())
        {
            score = vrc7RefError(ref, prevRef);
            score += std::abs(anchor - desiredAnchor) / std::max(1.0f, float(visibleSamples)) * 0.035f;
        }
        else
        {
            score = std::abs(anchor - desiredAnchor);
        }
        if (hasLastAnchorAbs)
        {
            float anchorAbs = float(absBase) + anchor;
            float driftFromLast = std::abs(anchorAbs - float(lastAnchorAbs));
            score += driftFromLast / std::max(1.0f, float(visibleSamples)) * 0.6f;
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestStart = candStart;
            bestRef = ref;
        }
    }

    if (!bestRef.isEmpty())
    {
        newRef = bestRef;
        ok = true;
        lastAnchorAbs = absBase + (qint64)std::llround(bestStart + anchorOffset);
        hasLastAnchorAbs = true;

        return bestStart;
    }

    return defaultStart;
}


static bool findVrc7LatestCycle(
    const QVector<float>& phase,
    int n,
    int visibleSamples,
    float periodHint,
    float& cycleStart,
    float& cycleEnd
)
{
    cycleStart = 0.0f;
    cycleEnd = 0.0f;

    if (phase.size() != n || n < 64)
        return false;

    const int scanBack = std::clamp(visibleSamples * 8, 512, std::max(512, n - 2));
    const int scanStart = std::max(1, n - scanBack);
    const int scanEnd = n - 2;

    QVector<float> wraps;
    wraps.reserve(512);

    for (int i = scanStart + 1; i <= scanEnd; ++i)
    {
        float prev = phase[i - 1];
        float cur = phase[i];

        if (!validPhase01(prev) || !validPhase01(cur))
            continue;

        if (prev > 0.55f && cur < 0.45f && (prev - cur) > 0.25f)
        {
            float denom = (1.0f - prev) + cur;
            float frac = (denom > 0.000001f) ? (1.0f - prev) / denom : 0.0f;
            frac = std::clamp(frac, 0.0f, 1.0f);
            wraps.push_back(float(i - 1) + frac);
        }
    }

    if (wraps.size() < 2)
        return false;

    const bool hasHint = (periodHint >= 4.0f && periodHint <= 8192.0f);
    const int last = wraps.size() - 1;

    // Thử đoạn dài trước để CH6/sóng mềm không bị đứt tại seam.
    // Nếu không đủ wrap thì tự lùi xuống 3/2/1 chu kỳ.
    const int preferredCycles[] = { 6, 4, 3, 2, 1 };

    for (int wantCycles : preferredCycles)
    {
        if (last - wantCycles < 0)
            continue;

        float a = wraps[last - wantCycles];
        float b = wraps[last];
        float segment = b - a;
        float avgPeriod = segment / float(wantCycles);

        if (segment < 4.0f || segment > float(visibleSamples) * 3.5f)
            continue;

        if (hasHint)
        {
            if (avgPeriod < periodHint * 0.35f || avgPeriod > periodHint * 2.80f)
                continue;
        }

        // Kiểm tra các chu kỳ con không lệch quá vô lý, tránh ăn nhầm wrap lỗi.
        bool stable = true;
        for (int k = last - wantCycles + 1; k <= last; ++k)
        {
            float p = wraps[k] - wraps[k - 1];
            if (p < 4.0f || p > float(visibleSamples) * 2.5f)
            {
                stable = false;
                break;
            }

            if (hasHint && (p < periodHint * 0.25f || p > periodHint * 3.2f))
            {
                stable = false;
                break;
            }
        }

        if (!stable)
            continue;

        cycleStart = a;
        cycleEnd = b;
        return true;
    }

    return false;
}


static float resolveLockedTrigger(
    AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,                 // = totalPushed[c] - n  (abs index của copy[c][0])
    const QVector<float>& idxs,
    const QVector<float>& slopes,
    float periodHint,               // sample / chu kỳ lấy từ timer thật, 0 nếu không có
    float minPeriod, float maxPeriod,
    float n)
{
    if (idxs.isEmpty())
        return -1.0f;

    const bool hasPeriodHint = periodHint >= minPeriod && periodHint <= 8192.0f;

    if (!lock.locked)
    {
        // Acquisition lần đầu (hoặc sau khi mất lock): lấy cạnh mạnh nhất làm mốc.
        int best = 0;
        for (int i = 1; i < idxs.size(); i++)
            if (slopes[i] > slopes[best]) best = i;

        lock.lockAbsPos = absBase + (qint64)std::llround(idxs[best]);
        lock.period = hasPeriodHint ? periodHint : 0.0f;
        lock.locked = true;
        return idxs[best];
    }

    // Nếu mapper/APU gửi period thật thì dùng nó làm anchor tuyệt đối.
    // Không low-pass period đo từ waveform nữa, vì đó chính là nguồn gây drift.
    if (hasPeriodHint)
        lock.period = periodHint;

    // Quy đổi lock cũ sang index của buffer hiện tại (an toàn qua các lần trim vì dùng absBase).
    float lockIdx = float(lock.lockAbsPos - absBase);

    float target = lockIdx;
    if (lock.period >= minPeriod)
    {
        float k = std::round((n - lockIdx) / lock.period);
        target = lockIdx + k * lock.period;
    }

    int best = -1;
    float bestDist = 1e30f;
    for (int i = 0; i < idxs.size(); i++)
    {
        float dist = std::abs(idxs[i] - target);
        if (dist < bestDist) { bestDist = dist; best = i; }
    }

    float maxJitter = (lock.period >= minPeriod) ? lock.period * 0.5f : n * 0.5f;
    if (best < 0 || bestDist > maxJitter)
    {
        // Ứng viên gần nhất vẫn lệch quá xa dự đoán => coi như mất lock, acquisition lại.
        int strongest = 0;
        for (int i = 1; i < idxs.size(); i++)
            if (slopes[i] > slopes[strongest]) strongest = i;

        lock.lockAbsPos = absBase + (qint64)std::llround(idxs[strongest]);
        lock.period = hasPeriodHint ? periodHint : 0.0f;
        return idxs[strongest];
    }

    float selected = idxs[best];

    if (!hasPeriodHint)
    {
        // Fallback cũ: chỉ dùng khi channel chưa có period thật.
        // Low-pass cho period đo từ waveform. Cái này giảm jitter nhưng vẫn có thể drift,
        // nên các kênh có timer thật sẽ không đi nhánh này nữa.
        float measuredPeriod = selected - lockIdx;
        if (lock.period >= minPeriod)
        {
            float kk = std::round(measuredPeriod / lock.period);
            if (kk > 0.0f)
            {
                float onePeriod = measuredPeriod / kk;
                if (onePeriod >= minPeriod && onePeriod <= maxPeriod)
                    lock.period = lock.period * 0.9f + onePeriod * 0.1f;
            }
        }
        else if (measuredPeriod >= minPeriod && measuredPeriod <= maxPeriod)
        {
            lock.period = measuredPeriod;
        }
    }

    // Phase correction kiểu PLL. Khi có periodHint, gain thấp hơn để chỉ căn pha,
    // không cho crossing đo từ waveform kéo lệch period thật.
    float phaseError = selected - target;
    float correctionGain = hasPeriodHint ? 0.18f : 0.35f;
    float corrected = target + phaseError * correctionGain;

    lock.lockAbsPos = absBase + (qint64)std::llround(corrected);
    return corrected;

}

// =====================================================================================
// HARD PERIOD ANCHOR
// -------------------------------------------------------------------------------------
// resolveLockedTrigger phía trên vẫn là PLL: nó còn cho crossing kéo phase từng frame.
// Cách đó giảm rung, nhưng vẫn có thể trôi nếu crossing đo được bị bias.
// Với các kênh đã có period thật từ APU/mapper, ta lock 1 cạnh làm mốc duy nhất,
// sau đó dự đoán vị trí các cạnh tiếp theo bằng period thật. Crossing chỉ dùng để
// acquire ban đầu hoặc khi note đổi mạnh; tuyệt đối không kéo phase mỗi frame nữa.
// =====================================================================================
static int pickLatestStrongCandidate(const QVector<float>& idxs, const QVector<float>& slopes)
{
    if (idxs.isEmpty())
        return -1;

    int strongest = 0;
    for (int i = 1; i < slopes.size(); ++i)
        if (slopes[i] > slopes[strongest])
            strongest = i;

    float gate = slopes[strongest] * 0.55f;

    // Ưu tiên cạnh mới nhất nhưng vẫn đủ mạnh, để lúc acquire lấy chu kỳ gần hiện tại.
    for (int i = idxs.size() - 1; i >= 0; --i)
        if (slopes[i] >= gate)
            return i;

    return strongest;
}

static bool updateHardPeriodAnchor(
    AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,
    const QVector<float>& idxs,
    const QVector<float>& slopes,
    float periodHint,
    float minPeriod,
    float maxPeriod,
    float changeRatio,
    float periodAlpha = 1.0f) // 1.0 = ghi đè cứng (behavior cũ); nhỏ hơn = low-pass
{
    const bool hasHint = periodHint >= minPeriod && periodHint <= maxPeriod;
    if (!hasHint)
        return false;

    bool needAcquire = !lock.locked || lock.period < minPeriod;

    if (!needAcquire)
    {
        float diff = std::abs(periodHint - lock.period);
        float limit = std::max(1.25f, lock.period * changeRatio);
        if (diff > limit)
            needAcquire = true;
    }


    if (needAcquire)
    {
        int best = pickLatestStrongCandidate(idxs, slopes);
        if (best < 0)
            return false;

        lock.lockAbsPos = absBase + (qint64)std::llround(idxs[best]);
        lock.period = periodHint;
        lock.locked = true;
        return true;
    }

    lock.period += (periodHint - lock.period) * periodAlpha;
    return true;
}


static float predictEdgeNearEnd(
    const AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,
    float n)
{
    if (!lock.locked || lock.period < 1.0f)
        return -1.0f;

    float p = lock.period;
    float lockIdx = float(lock.lockAbsPos - absBase);
    float k = std::floor((n - 2.0f - lockIdx) / p);
    float edge = lockIdx + k * p;

    while (edge < 1.0f) edge += p;
    while (edge > n - 2.0f) edge -= p;

    return edge;
}

static float predictEdgeNearStart(
    const AudioWaveWindow::TrigLockState& lock,
    qint64 absBase,
    float desiredStart,
    float minStart,
    float maxStart)
{
    if (!lock.locked || lock.period < 1.0f)
        return -1.0f;

    float p = lock.period;
    float lockIdx = float(lock.lockAbsPos - absBase);
    float k = std::round((desiredStart - lockIdx) / p);
    float edge = lockIdx + k * p;

    while (edge < minStart) edge += p;
    while (edge > maxStart) edge -= p;

    if (edge < minStart || edge > maxStart)
        return -1.0f;

    return edge;
}

AudioWaveWindow::AudioWaveWindow(WaveMode mode, QWidget* parent)
    : QOpenGLWidget(parent), mode(mode)
{
    setWindowTitle("Audio Waveform Debug");
    if (mode == WaveMode::VRC7)
    {
        resize(1100, 900);
        setMinimumSize(900, 820);
    }
    else
    {
        if (mode == WaveMode::NES)
        {
            resize(1100, 560);
            setMinimumSize(900, 500);
        }
        else if (mode == WaveMode::VRC7)
        {
            resize(1100, 720);
            setMinimumSize(900, 620);
        }
        else
        {
            resize(1100, 780);
            setMinimumSize(900, 700);
        }
    }

    QSurfaceFormat fmt;
    fmt.setSamples(4);
    setFormat(fmt);

    activeChannelCount = 8;

    names[0] = "Pulse 1";
    names[1] = "Pulse 2";
    names[2] = "Triangle";
    names[3] = "Noise";
    names[4] = "DMC";

    if (mode == WaveMode::NES)
    {
        setWindowTitle("NES 5 Channels Waveform Debug");
        activeChannelCount = 5;
    }
    else if (mode == WaveMode::VRC6)
    {
        setWindowTitle("NES + VRC6 Waveform Debug");
        activeChannelCount = 8;

        names[5] = "VRC6 Pulse 1";
        names[6] = "VRC6 Pulse 2";
        names[7] = "VRC6 Saw";
    }
    else if (mode == WaveMode::VRC7)
    {
        setWindowTitle("VRC7 6 Channels Waveform Debug");
        activeChannelCount = 6;

        names[0] = "VRC7 CH 1";
        names[1] = "VRC7 CH 2";
        names[2] = "VRC7 CH 3";
        names[3] = "VRC7 CH 4";
        names[4] = "VRC7 CH 5";
        names[5] = "VRC7 CH 6";
    }
    else if (mode == WaveMode::S5B)
    {
        setWindowTitle("NES + Sunsoft 5B Waveform Debug");
        activeChannelCount = 8;

        names[5] = "S5B Tone A";
        names[6] = "S5B Tone B";
        names[7] = "S5B Tone C";
    }
    else if (mode == WaveMode::MMC5)
    {
        setWindowTitle("NES + MMC5 Waveform Debug");
        activeChannelCount = 8;

        names[5] = "MMC5 Pulse 1";
        names[6] = "MMC5 Pulse 2";
        names[7] = "MMC5 PCM";
    }
    else if (mode == WaveMode::N163)
    {
        setWindowTitle("Namco 163 Waveform Debug");

        activeChannelCount = 8;

        names[0] = "N163 CH 1";
        names[1] = "N163 CH 2";
        names[2] = "N163 CH 3";
        names[3] = "N163 CH 4";
        names[4] = "N163 CH 5";
        names[5] = "N163 CH 6";
        names[6] = "N163 CH 7";
        names[7] = "N163 CH 8";
    }
    for (int i = 0; i < activeChannelCount; i++)
        buffers[i].reserve(MAX_SAMPLES);

    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, [this]() { update(); });
    refreshTimer->start(16);
    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        dmcSmoothY[i].clear();
        dmcSmoothValid[i] = false;
    }
}

void AudioWaveWindow::pushChannels(const AudioDebugChannels& ch)
{
    QMutexLocker lock(&mutex);

    float values[CHANNEL_COUNT]{};
    float periods[CHANNEL_COUNT]{};
    float phases[CHANNEL_COUNT]{};
    if (mode == WaveMode::N163)
    {
        values[0] = ch.n163Wave1;
        values[1] = ch.n163Wave2;
        values[2] = ch.n163Wave3;
        values[3] = ch.n163Wave4;
        values[4] = ch.n163Wave5;
        values[5] = ch.n163Wave6;
        values[6] = ch.n163Wave7;
        values[7] = ch.n163Wave8;

        periods[0] = ch.n163Period1;
        periods[1] = ch.n163Period2;
        periods[2] = ch.n163Period3;
        periods[3] = ch.n163Period4;
        periods[4] = ch.n163Period5;
        periods[5] = ch.n163Period6;
        periods[6] = ch.n163Period7;
        periods[7] = ch.n163Period8;
    }
    else if (mode == WaveMode::VRC7)
    {
        values[0] = ch.vrc7Wave1;
        values[1] = ch.vrc7Wave2;
        values[2] = ch.vrc7Wave3;
        values[3] = ch.vrc7Wave4;
        values[4] = ch.vrc7Wave5;
        values[5] = ch.vrc7Wave6;

        periods[0] = ch.vrc7Wave1Period;
        periods[1] = ch.vrc7Wave2Period;
        periods[2] = ch.vrc7Wave3Period;
        periods[3] = ch.vrc7Wave4Period;
        periods[4] = ch.vrc7Wave5Period;
        periods[5] = ch.vrc7Wave6Period;

        phases[0] = ch.vrc7Wave1Phase;
        phases[1] = ch.vrc7Wave2Phase;
        phases[2] = ch.vrc7Wave3Phase;
        phases[3] = ch.vrc7Wave4Phase;
        phases[4] = ch.vrc7Wave5Phase;
        phases[5] = ch.vrc7Wave6Phase;
    }
    else
    {
        values[0] = ch.pulse1;
        values[1] = ch.pulse2;
        values[2] = ch.triangle;
        values[3] = ch.noise;
        values[4] = ch.dmc;

        periods[0] = ch.pulse1Period;
        periods[1] = ch.pulse2Period;
        periods[2] = ch.trianglePeriod;
        periods[3] = ch.noisePeriod;
        periods[4] = ch.dmcPeriod;

        if (mode == WaveMode::VRC6)
        {
            values[5] = ch.vrc6Pulse1;
            values[6] = ch.vrc6Pulse2;
            values[7] = ch.vrc6Saw;

            periods[5] = ch.vrc6Pulse1Period;
            periods[6] = ch.vrc6Pulse2Period;
            periods[7] = ch.vrc6SawPeriod;
        }
        else if (mode == WaveMode::S5B)
        {
            values[5] = ch.s5bToneA;
            values[6] = ch.s5bToneB;
            values[7] = ch.s5bToneC;

            periods[5] = ch.s5bToneAPeriod;
            periods[6] = ch.s5bToneBPeriod;
            periods[7] = ch.s5bToneCPeriod;
        }
        else if (mode == WaveMode::MMC5)
        {
            values[5] = ch.mmc5Pulse1;
            values[6] = ch.mmc5Pulse2;
            values[7] = ch.mmc5PCM;

            periods[5] = ch.mmc5Pulse1Period;
            periods[6] = ch.mmc5Pulse2Period;
            periods[7] = 0.0f;
        }
    }

    for (int i = 0; i < activeChannelCount; i++)
    {
        periodHint[i] = std::clamp(periods[i], 0.0f, 8192.0f);
        const bool isNesTriangle =
            (mode != WaveMode::VRC7 && mode != WaveMode::N163 && i == 2);
        const bool triangleOff =
            isNesTriangle && std::abs(values[i]) <= 0.0001f && periods[i] <= 0.0f;

        if (triangleOff)
        {

            lastVisual[i] = 0.0f;
            periodHint[i] = 0.0f;

            if (!triangleZeroGate[i] && genericTrigLock[i].locked &&
                genericTrigLock[i].period >= 8.0f)
            {
                int period = std::clamp((int)std::lround(genericTrigLock[i].period),
                    8, MAX_SAMPLES);
                int bsize = buffers[i].size();
                if (bsize >= period)
                {
                    triFrozenCycle[i].resize(period);
                    for (int k = 0; k < period; k++)
                        triFrozenCycle[i][k] = buffers[i][bsize - period + k];
                    triFreezeActive[i] = true;
                    triFreezeAmp[i] = 1.0f;
                }
            }
            triangleZeroGate[i] = true;
        }
        else if (isNesTriangle && triangleZeroGate[i])
        {
            // BỎ lệnh buffers[i].clear(); 
            lastVisual[i] = 0.0f;
            genericTrigLock[i] = TrigLockState{};
            triangleZeroGate[i] = false;
            triFreezeActive[i] = false;
        }

        float v = values[i];
        if (triangleOff)
        {
            v = 0.0f;
        }

        if (mode != WaveMode::VRC7)
        {
            v = std::clamp(v, -1.0f, 1.0f);
        }

        // VRC6: dòng 5/6 là pulse, dòng 7 là saw
        if (mode == WaveMode::VRC6)
        {
            if (i == 5 || i == 6) { v *= 1.4f; v = std::clamp(v, -1.0f, 1.0f); }
            if (i == 7) { v *= 1.4f; }
        }

        // VRC7-only: giữ raw debug sample, KHÔNG scale/clamp ở pushChannels.
        // Lý do: nếu scale rồi clamp/tanh ở đây thì đỉnh sóng bị bẹt từ dữ liệu buffer,
        // paintGL không thể phục hồi lại được. VRC7 sẽ được phóng to bằng auto-gain khi vẽ.
        if (mode == WaveMode::VRC7)
        {
            // VRC7 debug polarity test:
            // Reference oscilloscope shows the flat/limited side on the bottom,
            // while SJNES was showing it on the top. Flip only VRC7 here.
            // Do NOT clamp/scale here, paintGL will auto-gain it.
            if (!std::isfinite(v))
                v = 0.0f;

            v = -v;
        }

        if (mode == WaveMode::N163)
        {
            // N163 raw thường nhỏ, cần phóng lên để nhìn rõ
            float n163Scale = 6.0f;

            v *= n163Scale;
            v = std::clamp(v, -0.95f, 0.95f);
        }


        // S5B: dòng 5..7
        if (mode == WaveMode::S5B && i >= 5)
        {
            v *= 1.4f;
            v = std::clamp(v, -1.0f, 1.0f);
        }

        buffers[i].push_back(v);

        // Lọc NGAY tại đây, một lần duy nhất mỗi sample, state không reset —
        // đây là điểm mấu chốt để filter có đủ lịch sử settle vào trạng thái
        // AC cân bằng quanh 0 thay vì bị "khởi động lại" mỗi frame vẽ.
        {
            RcFilterState& st = filterState[i];
            if (!st.initialized)
            {
                st.prevIn1 = v;   
                st.initialized = true;
            }
            bool isVrc6Saw = (mode == WaveMode::VRC6 && i == 7);
            bool isNesTriangle = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && i == 2);

            if (isVrc6Saw || isNesTriangle)
            {
                // BỘ LỌC NHẸ: Giữ góc cạnh sắc nét cho riêng âm Saw
                st.hp1 = 0.998f * (st.hp1 + v - st.prevIn1);      st.prevIn1 = v; 
                st.hp2 = 0.994f * (st.hp2 + st.hp1 - st.prevIn2); st.prevIn2 = st.hp1; 
                st.lp += 0.960f * (st.hp2 - st.lp);     
            }
            else
            {
                // BỘ LỌC MẠNH: Làm mượt cho tất cả các kênh còn lại
                st.hp1 = 0.997f * (st.hp1 + v - st.prevIn1);      st.prevIn1 = v;
                st.hp2 = 0.992f * (st.hp2 + st.hp1 - st.prevIn2); st.prevIn2 = st.hp1;
                st.lp += 0.930f * (st.hp2 - st.lp);
            }
            filteredBuffers[i].push_back(st.lp);
        }

        if (mode == WaveMode::VRC7)
        {
            float ph = phases[i];
            if (!std::isfinite(ph))
                ph = 0.0f;
            ph = ph - std::floor(ph);
            phaseBuffers[i].push_back(ph);
        }

        totalPushed[i]++;

        if (buffers[i].size() > MAX_SAMPLES + 512)
        {
            int removeCount = buffers[i].size() - MAX_SAMPLES;
            buffers[i].remove(0, removeCount);
            // filteredBuffers[i] phải bị trim CÙNG SỐ LƯỢNG, cùng đầu, để giữ
            // khớp index 1-1 với buffers[i] (raw). Không reset filterState ở đây —
            // state là "bộ nhớ tụ điện" phải sống xuyên suốt, không liên quan gì
            // đến việc ring buffer hiển thị bị cắt bớt phần cũ.
            if (filteredBuffers[i].size() >= removeCount)
                filteredBuffers[i].remove(0, removeCount);
            if (mode == WaveMode::VRC7 && phaseBuffers[i].size() >= removeCount)
                phaseBuffers[i].remove(0, removeCount);
        }
    }
}

void AudioWaveWindow::clearSamples()
{
    QMutexLocker lock(&mutex);
    for (int i = 0; i < activeChannelCount; i++)
    {
        buffers[i].clear();
        phaseBuffers[i].clear();
        filteredBuffers[i].clear();
        filterState[i] = RcFilterState{};
        lastVisual[i] = 0.0f;
        periodHint[i] = 0.0f;
        vrc7LastPhaseStart[i] = 0.0f;
        vrc7HasPhaseStart[i] = false;
        vrc7CorrRef[i].clear();
        vrc7CorrValid[i] = false;
        vrc7HasLastAnchorAbs[i] = false;
        vrc7LastAnchorAbs[i] = 0;
        triangleZeroGate[i] = false;
        triFreezeActive[i] = false;
        triFrozenCycle[i].clear();
        triFreezeAmp[i] = 0.0f;
        n163LastTrigger[i] = 0.0f;
        n163Period[i] = 0.0f;
        n163LastBufferSize[i] = 0;
        n163HasTrigger[i] = false;
        n163SmoothY[i].clear();
        n163SmoothValid[i] = false;

        totalPushed[i] = 0;
        genericTrigLock[i] = TrigLockState{};
        n163TrigLock[i] = TrigLockState{};
        dmcSmoothY[i].clear();
        dmcSmoothValid[i] = false;
        dmcCorrRef[i].clear();
        dmcCorrValid[i] = false;
    }
}
static float ClampFStartStep(float rawFStart, float& lastFStart, bool& hasLast, float maxStep)
{
    if (!hasLast)
    {
        lastFStart = rawFStart;
        hasLast = true;
        return rawFStart;
    }

    float diff = rawFStart - lastFStart;

    if (diff > maxStep)
        rawFStart = lastFStart + maxStep;
    else if (diff < -maxStep)
        rawFStart = lastFStart - maxStep;

    lastFStart = rawFStart;
    return rawFStart;
}
void AudioWaveWindow::paintGL()
{
    QVector<float> copy[CHANNEL_COUNT];
    QVector<float> filteredCopy[CHANNEL_COUNT];
    QVector<float> phaseCopy[CHANNEL_COUNT];
    float hintCopy[CHANNEL_COUNT]{};
    quint64 pushedCopy[CHANNEL_COUNT]{};
    {
        QMutexLocker lock(&mutex);
        for (int i = 0; i < CHANNEL_COUNT; i++)
        {
            copy[i] = buffers[i];
            filteredCopy[i] = filteredBuffers[i];
            phaseCopy[i] = phaseBuffers[i];
            hintCopy[i] = periodHint[i];
            pushedCopy[i] = totalPushed[i];
        }
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(0, 0, 0));

    int w = width();
    int h = height();
    int topMargin = 8;
    int bottomMargin = 8;
    int drawH = h - topMargin - bottomMargin;

    if (w <= 0 || h <= 0) return;

    int panelH = drawH / activeChannelCount;

    for (int c = 0; c < activeChannelCount; c++)
    {
        int top = topMargin + c * panelH;
        int midY = top + panelH / 2;

        p.fillRect(0, top, w, panelH, QColor(0, 0, 0));

        // Trục dọc (Y-axis) tại điểm trigger - luôn nằm giữa panel
        int midX = w / 2;
        p.setPen(QPen(QColor(70, 70, 70), 1));
        p.drawLine(midX, top, midX, top + panelH);
        // Tên kênh màu theo channel
        p.setPen(CHANNEL_COLORS[c]);
        p.drawText(10, top + 18, names[c]);

        if (copy[c].size() < 32)
            continue;

        // --- Số sample hiển thị ---
        const int n = copy[c].size();

        // Buffer đã qua filter thật, dùng để VẼ giá trị (không dùng để detect
        // trigger/range — detect vẫn dựa trên copy[c] gốc để không lệch ngưỡng).
        // Filter được tính LIÊN TỤC ngay tại pushChannels() (state không reset
        // mỗi frame), ở đây chỉ lấy phần snapshot đã lọc sẵn khớp với copy[c].
        QVector<float> filteredBuf = filteredCopy[c];

        // Khuếch đại CỐ ĐỊNH cho riêng phần dương của VRC6 Saw sau filter:
        // KHÔNG dùng max của cả buffer hiển thị để chuẩn hoá, vì nếu nhiều chu kỳ
        // trong khung có biên độ đỉnh khác nhau (do note đang decay/pitch rung),
        // đỉnh nhỏ hơn sẽ bị "nuốt" gần về 0 khi so với đỉnh cao nhất trong khung.
        // Hệ số cố định đảm bảo MỌI đỉnh đều được khuếch đại cùng 1 tỉ lệ,
        // không phụ thuộc các chu kỳ lân cận.
        constexpr float kVrc6SawPosBoost = 0.80f; // chỉnh số này để tăng/giảm độ cao đỉnh

        int visibleSamples = DISPLAY_SAMPLES;

        if (mode == WaveMode::VRC7)
        {
            // VRC7 mode chỉ có 6 kênh VRC7, cho mỗi sóng to và dễ nhìn hơn
            visibleSamples = DISPLAY_SAMPLES / 2;
        }
        else if (mode == WaveMode::N163)
        {
            visibleSamples = DISPLAY_SAMPLES / 2;
        }
        else
        {
            // NES Pulse: trigger và zoom vừa phải
            if (c == 0 || c == 1)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }

            // Noise/DMC: KHÔNG trigger, cho thưa bớt để đỡ rối
            if (c == 3)
            {
                visibleSamples = DISPLAY_SAMPLES * 1.1;
            }
            if (c == 4)
            {
                // Giảm từ *1.4 (quá lớn so với các kênh khác, nghi ngờ gây chậm FPS)
                // xuống mức vừa phải, tương tự Noise.
                visibleSamples = DISPLAY_SAMPLES / 1.4;
            }
        }

        // Expansion channels của VRC6/S5B
        if (mode != WaveMode::VRC7 && mode != WaveMode::N163 && c >= 5)
        {
            if (mode == WaveMode::VRC6)
            {
                if (c == 5 || c == 6)
                {
                    // VRC6 Pulse
                    visibleSamples = DISPLAY_SAMPLES / 3;
                }
                else if (c == 7)
                {

                    visibleSamples = DISPLAY_SAMPLES / 1.4;

                    const auto& buf = copy[c];
                    int resetA = -1;
                    int resetB = -1;

                    // Tìm 2 cạnh rơi/reset gần nhất để ước lượng chu kỳ hiện tại.
                    for (int i = n - 1; i >= 1; i--)
                    {
                        float prev = buf[i - 1];
                        float cur = buf[i];

                        if ((prev - cur) > 0.06f)
                        {
                            if (resetB < 0)
                                resetB = i;
                            else
                            {
                                resetA = i;
                                break;
                            }
                        }
                    }

                    if (resetA >= 0 && resetB > resetA)
                    {
                        int period = resetB - resetA;
                        period = std::clamp(period, 8, DISPLAY_SAMPLES * 1);

                        const float minTeeth = 1.0f;
                        const float maxTeeth = 9.0f;

                        float rawTeeth = float(visibleSamples) / float(period);
                        float teeth = std::clamp(rawTeeth, minTeeth, maxTeeth);

                        visibleSamples = int(std::round(float(period) * teeth));
                        visibleSamples = std::clamp(visibleSamples, 32, DISPLAY_SAMPLES * 3);

                        if (visibleSamples > n - 4)
                            visibleSamples = n - 4;
                    }
                }
            }
            else if (mode == WaveMode::S5B)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }
            else if (mode == WaveMode::MMC5)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }
            else if (mode == WaveMode::VRC7)
            {
                visibleSamples = DISPLAY_SAMPLES / 3;
            }
        }

        if (visibleSamples < 32 || n < visibleSamples + 4)
            continue;

        float minV = copy[c][0];
        float maxV = copy[c][0];

        for (int i = 1; i < n; i++)
        {
            minV = std::min(minV, copy[c][i]);
            maxV = std::max(maxV, copy[c][i]);
        }

        float range = maxV - minV;
        float triggerLevel = (minV + maxV) * 0.5f;
        float fStart = float(n - visibleSamples);
        qint64 chanAbsBase = (qint64)pushedCopy[c] - (qint64)n;

        bool isNoiseDMC = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && (c == 3 || c == 4));
        bool isVrc6Saw = (mode == WaveMode::VRC6 && c == 7);
        bool isTriangle = (mode != WaveMode::VRC7 && mode != WaveMode::N163 && c == 2);

        if (mode == WaveMode::VRC7)
        {
            bool corrOk = false;
            QVector<float> newRef;
            float corrStart = findVrc7CorrscopeStart(
                copy[c], phaseCopy[c], n, visibleSamples, hintCopy[c],
                vrc7CorrRef[c], vrc7CorrValid[c], newRef, corrOk,
                chanAbsBase, vrc7LastAnchorAbs[c], vrc7HasLastAnchorAbs[c]
            );

            if (corrOk)
            {
                fStart = corrStart;
                vrc7LastPhaseStart[c] = corrStart;
                vrc7HasPhaseStart[c] = true;
                vrc7CorrRef[c] = newRef;
                vrc7CorrValid[c] = true;
            }
            else if (vrc7HasPhaseStart[c])
            {
                fStart = vrc7LastPhaseStart[c];
            }
        }
        else if (range > 0.02f && !isNoiseDMC)
        {
            if (mode == WaveMode::N163)
            {
            }
            else if (isVrc6Saw)
            {
                int searchStart = n - visibleSamples - 1;

                if (searchStart < 1)
                    searchStart = 1;

                for (int i = searchStart; i >= 1; i--)
                {
                    float prev = copy[c][i - 1];
                    float cur = copy[c][i];

                    if ((prev - cur) > 0.06f)
                    {
                        fStart = float(i - 1) - float(visibleSamples) * 0.50f;
                        break;
                    }
                }
            }
            else
            {
                int searchStart = n - visibleSamples - 1;

                if (searchStart < 1)
                    searchStart = 1;

                for (int i = searchStart; i >= 1; i--)
                {
                    if (copy[c][i - 1] < triggerLevel && copy[c][i] >= triggerLevel)
                    {
                        float slope = copy[c][i] - copy[c][i - 1];
                        float frac = (slope > 0.0f) ? (triggerLevel - copy[c][i - 1]) / slope : 0.0f;
                        fStart = float(i - 1) + frac - float(visibleSamples) * 0.50f;
                        break;
                    }
                }
            }
        }
        else if (isTriangle)
        {
            QVector<float> idxs;
            QVector<float> slopes;
            idxs.reserve(128);
            slopes.reserve(128);

            int scanStart = std::max(1, n - visibleSamples * 4);
            int scanEnd = n - 2;

            float minSlope = std::max(0.0001f, range * 0.05f);

            for (int si = scanStart + 1; si <= scanEnd; si++)
            {
                float prev = copy[c][si - 1];
                float cur = copy[c][si];

                if (prev < triggerLevel && cur >= triggerLevel)
                {
                    float slope = cur - prev;

                    if (slope >= minSlope)
                    {
                        float frac = (triggerLevel - prev) / slope;
                        frac = std::clamp(frac, 0.0f, 1.0f);

                        idxs.push_back(float(si - 1) + frac);
                        slopes.push_back(slope);
                    }
                }
            }

            qint64 absBase = chanAbsBase;
            float hint = hintCopy[c];

            bool locked = updateHardPeriodAnchor(
                genericTrigLock[c],
                absBase,
                idxs,
                slopes,
                hint,
                8.0f,
                8192.0f,
                0.06f,
                0.25f
            );


            float halfVisible = float(visibleSamples) * 0.5f;

            if (locked && genericTrigLock[c].period >= 8.0f)
            {
                float desiredCenter = float(n) - halfVisible;
                float edge = predictEdgeNearStart(
                    genericTrigLock[c],
                    absBase,
                    desiredCenter,
                    halfVisible,
                    float(n) - halfVisible - 2.0f
                );

                if (edge >= 0.0f)
                    fStart = edge - halfVisible;
            }
            else if (!idxs.isEmpty())
            {
                // Fallback nếu chưa có period hint
                fStart = idxs.back() - halfVisible;
            }
        }
        else if (isNoiseDMC && c == 4)
        {
            // DMC: nếu $4010 bit 6 (loop) đang bật, hardware TỰ lặp lại sample sau
            // đúng `sample_length` byte — đây là chu kỳ THẬT, đo được chính xác từ
            // DMC.h (đếm cycle giữa 2 lần Restart() do loop), gửi qua hintCopy[c].
            // Ưu tiên tuyệt đối cách này, dùng lại đúng cơ chế hard-anchor như
            // Triangle/N163 (updateHardPeriodAnchor + predictEdgeNearEnd), KHÔNG
            // đoán bằng correlation nữa khi đã có period thật.
            bool dmcHardLocked = false;
            float dmcHint = hintCopy[c];

            if (dmcHint >= 8.0f && dmcHint <= 8192.0f)
            {
                QVector<float> dmcIdxs;
                QVector<float> dmcSlopes;
                dmcIdxs.reserve(64);
                dmcSlopes.reserve(64);

                int scanStart = std::max(1, n - visibleSamples * 4);
                int scanEnd = n - 2;

                // QUAN TRỌNG: quét trên filteredBuf (đã AC-coupled, mean tiến về 0 thật)
                // thay vì copy[c] thô. Vì copy[c] là DMC unipolar chưa lọc, "trung điểm
                // min/max thô" của nó KHÁC với "điểm 0 thật" mà filteredBuf hội tụ về —
                // dẫn đến điểm neo (anchor) bị lệch khỏi y=0 khi vẽ (vì cái thực sự được
                // vẽ lên màn hình là filteredBuf, không phải copy[c]). Dùng level = 0.0f
                // cố định để mốc cắt luôn khớp đúng với gridline 0 khi hiển thị.
                const QVector<float>& scanBuf = (scanEnd < filteredBuf.size()) ? filteredBuf : copy[c];
                float dmcLevel = 0.0f;
                float filteredRange = range; // fallback nếu chưa đủ dữ liệu filtered
                if (scanBuf.size() >= n)
                {
                    float fMin = scanBuf[0], fMax = scanBuf[0];
                    for (int i = std::max(0, n - visibleSamples * 4); i < n; i++)
                    {
                        fMin = std::min(fMin, scanBuf[i]);
                        fMax = std::max(fMax, scanBuf[i]);
                    }
                    filteredRange = fMax - fMin;
                }
                float dmcMinSlope = std::max(0.00005f, filteredRange * 0.04f);

                for (int si = scanStart + 1; si <= scanEnd && si < scanBuf.size(); si++)
                {
                    float prev = scanBuf[si - 1];
                    float cur = scanBuf[si];
                    if (prev < dmcLevel && cur >= dmcLevel)
                    {
                        float slope = cur - prev;
                        if (slope >= dmcMinSlope)
                        {
                            float frac = (dmcLevel - prev) / slope;
                            dmcIdxs.push_back(float(si - 1) + std::clamp(frac, 0.0f, 1.0f));
                            dmcSlopes.push_back(slope);
                        }
                    }
                }

                bool locked = updateHardPeriodAnchor(
                    genericTrigLock[c], chanAbsBase, dmcIdxs, dmcSlopes,
                    dmcHint, 8.0f, 8192.0f, 0.03f // đổi period ratio nhỏ vì đây là số đo cứng, gần như không đổi
                );

                if (locked && genericTrigLock[c].period >= 8.0f)
                {
                    float halfVisible = float(visibleSamples) * 0.5f;
                    float desiredCenter = float(n) - halfVisible;
                    float edge = predictEdgeNearStart(
                        genericTrigLock[c], chanAbsBase, desiredCenter,
                        halfVisible, float(n) - halfVisible - 2.0f
                    );

                    if (edge >= 0.0f)
                    {
                        float bestEdge = edge;
                        float minDiff = 1e9f;

                        for (float actualIdx : dmcIdxs) {
                            float diff = std::abs(actualIdx - edge);
                            if (diff < minDiff) {
                                minDiff = diff;
                                bestEdge = actualIdx;
                            }
                        }
                        // Áp dụng bộ lọc pha (Soft-PLL) thay vì gán cứng để chống trôi/rung nhẹ.
                        if (minDiff < dmcHint * 0.20f) {
                            float phaseError = bestEdge - edge;

                            // Tận dụng mảng n163LastTrigger (rảnh ở mode này) làm I-term để bù pha từ từ
                            n163LastTrigger[c] = n163LastTrigger[c] * 0.95f + phaseError * 0.05f;

                            // Cộng dồn sai số pha đã làm mượt vào dự đoán toán học
                            edge += n163LastTrigger[c];
                        }
                        else {
                            // Mất lock hoặc lệch quá xa thì reset bù pha
                            n163LastTrigger[c] = 0.0f;
                        }

                        fStart = edge - halfVisible;
                        dmcHardLocked = true;
                    }
                }
            }
            else
            {
                // Sample không loop (one-shot) -> không có gì để hard-lock,
                // period đo cũ (nếu có từ sample trước) không còn ý nghĩa.
                genericTrigLock[c] = TrigLockState{};
            }

            if (!dmcHardLocked)
            {
                // Fallback: sample không loop / chưa đo được period thật.
                // Tìm vị trí trong buffer hiện tại khớp NHẤT (cross-correlation) với
                // đoạn đã hiển thị ở frame TRƯỚC — chỉ mang tính "đỡ trôi" tạm thời,
                // không chính xác tuyệt đối như nhánh hard-lock ở trên.
                int defaultStart = std::clamp(n - visibleSamples, 1, std::max(1, n - visibleSamples - 2));
                int bestStart = defaultStart;

                const QVector<float>& buf = copy[c];

                if (dmcCorrValid[c] && dmcCorrRef[c].size() == visibleSamples && n >= visibleSamples + 2)
                {
                    // Giới hạn phạm vi tìm kiếm quanh vị trí mặc định để đỡ tốn tính toán,
                    // đồng thời tránh nhảy quá xa gây giật hình nếu khớp nhầm đoạn khác.
                    int searchRadius = std::min(visibleSamples, 800);
                    int scanLo = std::max(1, defaultStart - searchRadius);
                    int scanHi = std::min(n - visibleSamples - 2, defaultStart + searchRadius);
                    if (scanHi < scanLo) scanHi = scanLo;

                    const QVector<float>& ref = dmcCorrRef[c];
                    float bestScore = -1e18f;

                    // Stride 2 để giảm ~1 nửa chi phí tính, vẫn đủ chính xác để so hình dạng.
                    for (int s = scanLo; s <= scanHi; s++)
                    {
                        float dot = 0.0f;
                        for (int k = 0; k < visibleSamples; k += 2)
                            dot += ref[k] * buf[s + k];

                        if (dot > bestScore)
                        {
                            bestScore = dot;
                            bestStart = s;
                        }
                    }
                }

                fStart = float(bestStart);
                fStart = std::clamp(fStart, 0.0f, float(n - visibleSamples - 2));

                // Lưu lại đoạn vừa chọn làm reference cho frame kế tiếp.
                dmcCorrRef[c].resize(visibleSamples);
                for (int k = 0; k < visibleSamples; k++)
                    dmcCorrRef[c][k] = buf[bestStart + k];
                dmcCorrValid[c] = true;
            }
        }
        fStart = std::clamp(fStart, 0.0f, float(n - visibleSamples - 2));

        float amp = panelH * 0.45f;

        if (mode == WaveMode::VRC7)
        {
            // VRC7 sẽ được normalize theo range từng kênh ở lúc vẽ.
            // amp này là độ cao thực tế trên panel; tăng ở đây để sóng to hơn mà không clip.
            amp = panelH * 0.42f;
        }
        else if (mode == WaveMode::N163)
        {
            // Chừa biên để sóng không chạm trên/dưới
            amp = panelH * 0.42f;
        }
        else
        {
            if (c == 0 || c == 1)   amp = panelH * 0.36f;
            if (c == 2)             amp = panelH * 0.36f;
            if (c == 3)             amp = panelH * 0.28f;
            if (c == 4)             amp = panelH * 0.85f;
            if (c >= 5)
            {
                if (mode == WaveMode::VRC6)
                    amp = (c == 7) ? panelH * 0.80f : panelH * 0.48f;
                else if (mode == WaveMode::S5B)
                    amp = panelH * 0.30f;
                else if (mode == WaveMode::MMC5)
                    amp = (c == 7) ? panelH * 0.36f : panelH * 0.44f;
            }
        }

        if (mode == WaveMode::N163)
        {
            int drawPoints = std::max(2, w * 2);

            QPen n163Pen(CHANNEL_COLORS[c]);
            n163Pen.setWidthF(1.35f);
            n163Pen.setCapStyle(Qt::RoundCap);
            n163Pen.setJoinStyle(Qt::RoundJoin);
            p.setPen(n163Pen);

            // Tìm các rising crossing đủ mạnh trong vùng gần cuối buffer.
            QVector<float> idxs;
            QVector<float> slopes;
            idxs.reserve(128);
            slopes.reserve(128);

            int scanStart = std::max(1, n - visibleSamples * 4);
            int scanEnd = n - 2;
            float minSlope = std::max(0.0001f, range * 0.08f);

            for (int si = scanStart + 1; si <= scanEnd; si++)
            {
                float prev = copy[c][si - 1];
                float cur = copy[c][si];

                if (prev < triggerLevel && cur >= triggerLevel)
                {
                    float slope = cur - prev;
                    if (slope >= minSlope)
                    {
                        float frac = (triggerLevel - prev) / slope;
                        float idx = float(si - 1) + frac;
                        idxs.push_back(idx);
                        slopes.push_back(slope);
                    }
                }
            }

            bool drewN163 = false;

            qint64 absBase = (qint64)pushedCopy[c] - (qint64)n;
            float hint = hintCopy[c];

            // N163 dùng hard anchor thật sự: sau khi acquire cạnh đầu tiên, phase không còn
            // bị crossing kéo từng frame nữa. Period thật quyết định vị trí chu kỳ.
            bool hasHardAnchor = updateHardPeriodAnchor(
                n163TrigLock[c], absBase, idxs, slopes,
                hint, 8.0f, 8192.0f, 0.055f
            );

            if (hasHardAnchor && n163TrigLock[c].period >= 8.0f)
            {
                float period = n163TrigLock[c].period;
                float cycleEnd = predictEdgeNearEnd(n163TrigLock[c], absBase, float(n));
                float cycleStart = cycleEnd - period;

                if (cycleEnd >= 0.0f && cycleStart >= 1.0f && cycleEnd < float(n - 2))
                {
                    // Giữ zoom giống visibleSamples cũ: số chu kỳ trên màn hình = visibleSamples / period.
                    float cyclesAcross = float(visibleSamples) / period;
                    cyclesAcross = std::clamp(cyclesAcross, 1.0f, 24.0f);

                    if (n163SmoothY[c].size() != drawPoints)
                    {
                        n163SmoothY[c].resize(drawPoints);
                        n163SmoothValid[c] = false;
                    }

                    QPainterPath n163Path;

                    for (int pi = 0; pi < drawPoints; pi++)
                    {
                        float t = float(pi) / float(drawPoints - 1);
                        // Dịch mốc trigger từ mép trái (t=0) ra ĐÚNG GIỮA (t=0.5) để
                        // khớp với đường kẻ dọc midX đang vẽ, giống cách Pulse/Triangle
                        // căn giữa trigger point của chúng.
                        float tShifted = t - 0.5f;
                        float phase = std::fmod(tShifted * cyclesAcross, 1.0f);
                        if (phase < 0.0f)
                            phase += 1.0f;

                        float fIdx = cycleStart + phase * period;
                        fIdx = std::clamp(fIdx, 0.0f, float(n - 2));

                        float sample = sampleInterp(filteredBuf, fIdx);
                        float yTarget = midY - sample * amp;

                        float y = yTarget;
                        if (n163SmoothValid[c])
                        {
                            // Bám nhanh theo frame, không relax chậm;
                            // mọi điểm X cập nhật cùng lúc, không lệch tick.
                            y = n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f;
                        }

                        n163SmoothY[c][pi] = y;

                        float x = t * float(w);
                        if (pi == 0) n163Path.moveTo(x, y);
                        else         n163Path.lineTo(x, y);
                    }

                    n163SmoothValid[c] = true;
                    p.drawPath(n163Path);
                    drewN163 = true;
                }
            }

            // Fallback cho trường hợp chưa có period hint hợp lệ.
            if (!drewN163 && !idxs.isEmpty())
            {
                float maxP = std::max(float(visibleSamples), hint * 2.0f);
                float lockedEdge = resolveLockedTrigger(n163TrigLock[c], absBase, idxs, slopes,
                    hint, 8.0f, maxP, float(n));

                if (lockedEdge >= 0.0f && n163TrigLock[c].period >= 8.0f)
                {
                    float period = n163TrigLock[c].period;
                    float cycleEnd = lockedEdge;
                    float cycleStart = cycleEnd - period;

                    if (cycleStart >= 1.0f && cycleEnd < float(n - 2))
                    {
                        float cyclesAcross = std::clamp(float(visibleSamples) / period, 1.0f, 24.0f);

                        if (n163SmoothY[c].size() != drawPoints)
                        {
                            n163SmoothY[c].resize(drawPoints);
                            n163SmoothValid[c] = false;
                        }

                        QPainterPath n163Path;
                        for (int pi = 0; pi < drawPoints; pi++)
                        {
                            float t = float(pi) / float(drawPoints - 1);
                            float tShifted = t - 0.5f;
                            float phase = std::fmod(tShifted * cyclesAcross, 1.0f);
                            if (phase < 0.0f)
                                phase += 1.0f;
                            float fIdx = std::clamp(cycleStart + phase * period, 0.0f, float(n - 2));
                            float yTarget = midY - sampleInterp(filteredBuf, fIdx) * amp;
                            float y = n163SmoothValid[c] ? (n163SmoothY[c][pi] * 0.08f + yTarget * 0.92f) : yTarget;
                            n163SmoothY[c][pi] = y;
                            float x = t * float(w);
                            if (pi == 0) n163Path.moveTo(x, y);
                            else         n163Path.lineTo(x, y);
                        }
                        n163SmoothValid[c] = true;
                        p.drawPath(n163Path);
                        drewN163 = true;
                    }
                }
            }

            if (drewN163)
                continue;

            if (n163SmoothValid[c] && n163SmoothY[c].size() == drawPoints)
            {
                QPainterPath releasePath;

                bool stillVisible = false;
                const float releaseKeep = 0.72f;
                const float releaseToCenter = 1.0f - releaseKeep;

                for (int pi = 0; pi < drawPoints; pi++)
                {
                    float t = float(pi) / float(drawPoints - 1);

                    float y = n163SmoothY[c][pi] * releaseKeep + float(midY) * releaseToCenter;
                    n163SmoothY[c][pi] = y;

                    if (std::abs(y - float(midY)) > 0.75f)
                        stillVisible = true;

                    float x = t * float(w);
                    if (pi == 0) releasePath.moveTo(x, y);
                    else         releasePath.lineTo(x, y);
                }

                p.drawPath(releasePath);

                if (!stillVisible)
                    n163SmoothValid[c] = false;

                continue;
            }

            n163SmoothValid[c] = false;
        }


        // VRC7 STROBE-CYCLE LOCK:
        if (false && mode == WaveMode::VRC7)
        {
            float cycleStart = 0.0f;
            float cycleEnd = 0.0f;
            bool gotCycle = findVrc7LatestCycle(phaseCopy[c], n, visibleSamples, hintCopy[c], cycleStart, cycleEnd);

            if (gotCycle)
            {
                float period = cycleEnd - cycleStart;
                int drawPoints = std::max(2, w * 2);

                int r0 = std::clamp(int(std::floor(cycleStart)) - 2, 0, n - 1);
                int r1 = std::clamp(int(std::ceil(cycleEnd)) + 2, 0, n - 1);

                float localMin = copy[c][r0];
                float localMax = copy[c][r0];
                for (int ri = r0 + 1; ri <= r1; ++ri)
                {
                    localMin = std::min(localMin, copy[c][ri]);
                    localMax = std::max(localMax, copy[c][ri]);
                }

                float localRange = localMax - localMin;
                float localCenter = (localMin + localMax) * 0.5f;
                float localHalf = std::max(localRange * 0.5f, 0.0015f);

                float cyclesAcross = float(visibleSamples) / std::max(period, 1.0f);
                cyclesAcross = std::clamp(cyclesAcross, 1.0f, 22.0f);

                QPen vrc7Pen(CHANNEL_COLORS[c]);
                vrc7Pen.setWidthF(2.0f);
                vrc7Pen.setCapStyle(Qt::RoundCap);
                vrc7Pen.setJoinStyle(Qt::RoundJoin);
                p.setPen(vrc7Pen);

                QPainterPath vrc7Path;
                bool pathStarted = false;

                auto normVrc7 = [&](float s) -> float
                    {
                        float v = (s - localCenter) / localHalf;
                        return std::clamp(v, -1.12f, 1.12f);
                    };
                float startSample = sampleInterp(copy[c], std::clamp(cycleStart + 1.0f, 0.0f, float(n - 2)));
                float endSample = sampleInterp(copy[c], std::clamp(cycleEnd - 1.0f, 0.0f, float(n - 2)));
                float seamValue = (startSample + endSample) * 0.5f;
                float seamWidth = std::clamp(6.0f / std::max(period, 1.0f), 0.012f, 0.055f);

                auto smooth01 = [](float x) -> float
                    {
                        x = std::clamp(x, 0.0f, 1.0f);
                        return x * x * (3.0f - 2.0f * x);
                    };

                for (int pi = 0; pi < drawPoints; ++pi)
                {
                    float t = float(pi) / float(drawPoints - 1);
                    float phase = std::fmod(t * cyclesAcross, 1.0f);
                    if (phase < 0.0f)
                        phase += 1.0f;

                    float fIdx = cycleStart + phase * period;
                    fIdx = std::clamp(fIdx, 0.0f, float(n - 2));

                    float sample = sampleInterp(copy[c], fIdx);

                    if (phase < seamWidth)
                    {
                        float a = smooth01(phase / seamWidth);
                        sample = seamValue * (1.0f - a) + sample * a;
                    }
                    else if (phase > 1.0f - seamWidth)
                    {
                        float a = smooth01((phase - (1.0f - seamWidth)) / seamWidth);
                        sample = sample * (1.0f - a) + seamValue * a;
                    }

                    float x = t * float(w);
                    float y = midY - normVrc7(sample) * amp;

                    if (!pathStarted)
                    {
                        vrc7Path.moveTo(x, y);
                        pathStarted = true;
                    }
                    else
                    {
                        vrc7Path.lineTo(x, y);
                    }
                }

                if (pathStarted)
                    p.drawPath(vrc7Path);

                continue;
            }

        }
        // Filter chain THẬT lấy từ APU::GetOutputSampleStereo (2 highpass + 1 lowpass,
        // hệ số 0.996/0.990/0.899). Không tính lại ở đây nữa — dùng chung filteredBuf
        // (đã lọc liên tục, persistent, tại pushChannels()) để tránh 2 filter instance
        // riêng biệt cho cùng 1 kênh gây khác pha/khác trạng thái nhau.
        QVector<float> vrc6SawFiltered;
        if (isVrc6Saw)
        {
            vrc6SawFiltered = filteredBuf;
        }

        bool stepWave = false;

        QPen pen(CHANNEL_COLORS[c]);
        if (mode == WaveMode::VRC7)
            pen.setWidthF(2.0f);
        else if (mode == WaveMode::VRC6 && c == 7)
            pen.setWidthF(2.0f);
        else
            pen.setWidthF((c == 0 || c == 1 || c >= 5) ? 1.8f : 1.2f);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);

        QPainterPath path;



        bool useFrozenCycle = isTriangle && triFreezeActive[c] && !triFrozenCycle[c].isEmpty();
        float freezeAmpNow = triFreezeAmp[c];

        auto sampleTriFrozen = [&](float fIdx) -> float
            {
                int period = triFrozenCycle[c].size();
                double absPos = double(chanAbsBase) + double(fIdx);
                double lockPos = double(genericTrigLock[c].lockAbsPos);
                double rel = std::fmod(absPos - lockPos, double(period));
                if (rel < 0.0) rel += double(period);

                int i0 = int(rel);
                int i1 = (i0 + 1) % period;
                float frac = float(rel - double(i0));
                return (triFrozenCycle[c][i0] * (1.0f - frac) + triFrozenCycle[c][i1] * frac)
                    * freezeAmpNow;
            };

        int drawPoints = (mode == WaveMode::N163) ? w * 3 : w * 2;
        int triDrawPoints = w * 8;
        int currentPoints = isTriangle ? triDrawPoints : drawPoints;
        if (c == 4)
        {
            if (dmcSmoothY[c].size() != currentPoints)
            {
                dmcSmoothY[c].resize(currentPoints);
                dmcSmoothValid[c] = false;
            }
        }
        int viewStart = std::clamp(int(fStart), 0, n - 1);
        int viewEnd = std::clamp(int(fStart + visibleSamples), 0, n - 1);
        float fMin = filteredBuf[viewStart];
        float fMax = filteredBuf[viewStart];
        for (int i = viewStart; i <= viewEnd; i++)
        {
            fMin = std::min(fMin, filteredBuf[i]);
            fMax = std::max(fMax, filteredBuf[i]);
        }
        float fCenter = (fMin + fMax) * 0.5f;
        for (int i = 0; i < currentPoints; i++)
        {
            float fIdx = fStart + (float(i) / float(currentPoints - 1)) * float(visibleSamples);

            if (fIdx < 0 || fIdx >= float(n - 1)) continue;

            float x = float(i) / float(currentPoints - 1) * float(w);
            float y;
            auto drawSample = [&](float s) -> float
                {
                    if (mode == WaveMode::VRC7)
                    {
                        // Auto-gain theo từng kênh VRC7: dùng range thật của buffer hiện tại,
                        // không hard-clamp ở pushChannels nên đỉnh không còn bị bẹt giả.
                        const float halfRange = std::max(range * 0.5f, 0.0015f);
                        float vrc7 = (s - triggerLevel) / halfRange;
                        return std::clamp(vrc7, -1.18f, 1.18f);
                    }
                    if (mode == WaveMode::VRC6 && c == 7)
                    {
                        if (range > 0.02f)
                        {
                            // 1. Đưa sóng về giữa để cân bằng cơ bản
                            float centered = s - fCenter;

                            // 2. LẬT NGƯỢC sóng (thêm dấu trừ) để cạnh thẳng đứng nhảy lên trên
                            float inverted = centered;

                            // 3. Tinh chỉnh độ cao để ép bậc thứ 3 dính vào trục 0x
                            // Biến này tính theo % chiều cao sóng. Bạn có thể thay đổi số 0.10f
                            // (ví dụ: thử 0.08f, 0.12f, 0.15f) để dịch sóng lên/xuống cho đến khi ưng ý nhất.
                            float customOffset = range * -0.10f;

                            // Cộng offset sẽ đẩy đồ thị nhích lên trên một chút
                            return inverted + customOffset;
                        }
                        else
                        {
                            // Hết tín hiệu: Ghim thẳng tắp về trục x
                            return 0.0f;
                        }
                    }
                    bool isPulseChannel = (c == 0 || c == 1 ||
                        (mode == WaveMode::VRC6 && (c == 5 || c == 6 || c == 7 )) ||
                        (mode == WaveMode::S5B && c >= 5) ||
                        (mode == WaveMode::MMC5 && (c == 5 || c == 6)));

                    if (isPulseChannel)
                    {
                        if (range > 0.02f)
                        {
                            // Có tín hiệu: Nhấc đồ thị xuống để đối xứng qua 0
                            return s - fCenter;
                        }
                        else
                        {
                            // Hết tín hiệu: Ghim thẳng tắp về trục x
                            return 0.0f;
                        }
                    }
                    return s;
                };

            if (stepWave)
            {
                // Step wave: dùng nearest sample (không interpolate)
                int idx = int(fIdx);
                idx = std::clamp(idx, 0, n - 1);
                float sCur = useFrozenCycle ? sampleTriFrozen(fIdx) : filteredBuf[idx];
                y = midY - drawSample(sCur) * amp;

                if (i == 0) { path.moveTo(x, y); }
                else
                {
                    float prevFIdx = fStart + (float(i - 1) / float(drawPoints - 1)) * float(visibleSamples);
                    int prevIdx = std::clamp(int(prevFIdx), 0, n - 1);
                    float sPrev = useFrozenCycle ? sampleTriFrozen(prevFIdx) : filteredBuf[prevIdx];
                    float prevY = midY - drawSample(sPrev) * amp;
                    path.lineTo(x, prevY);
                    path.lineTo(x, y);
                }
            }

            else
            {
                float s = 0.0f;
                if (useFrozenCycle)
                {
                    s = sampleTriFrozen(fIdx);
                }
                else if (mode == WaveMode::VRC6 && c == 7)
                {
                    s = sampleSawSmooth(vrc6SawFiltered, copy[c], fIdx);
                }
                else if (c == 0 || c == 1 || (mode == WaveMode::VRC6 && c >= 5) || (mode == WaveMode::S5B && c >= 5) || (mode == WaveMode::MMC5 && (c == 5 || c == 6)))
                {
                    s = sampleLinear(filteredBuf, fIdx);
                }
                else
                {
                    s = sampleInterp(filteredBuf, fIdx);
                }
                y = midY - drawSample(s) * amp;



                if (c == 4)
                {
                    if (dmcSmoothValid[c])
                    {
                        y = dmcSmoothY[c][i] * 0.30f + y * 0.70f;
                    }
                    dmcSmoothY[c][i] = y;
                }
                if (i == 0) path.moveTo(x, y);
                else        path.lineTo(x, y);
            }
        }
        if (c == 4)
        {
            dmcSmoothValid[c] = true;
        }

        p.drawPath(path);

        if (isTriangle && triFreezeActive[c])
        {
            triFreezeAmp[c] *= 0.93f;
            if (triFreezeAmp[c] < 0.01f)
            {
                triFreezeActive[c] = false;
                triFrozenCycle[c].clear();
            }
        }
    }
}

void AudioWaveWindow::initializeGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void AudioWaveWindow::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}