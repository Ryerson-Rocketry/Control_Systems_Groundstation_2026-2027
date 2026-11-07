#include "telemetryhub.h"

namespace {
constexpr qsizetype kMaxHistoryFrames = 3600;
}

void TelemetryHub::addFrame(const TelemetryFrame &frame)
{
    m_history.append(frame);
    if (m_history.size() > kMaxHistoryFrames)
        m_history.removeFirst();
    ++m_frameCount;
    emit frameAdded(frame);
}
