#include "debug.hpp"

#include <cstdio>
#include <cstring>

namespace {

bool g_logging_enabled = false;
ams::log::LogSink g_sink = nullptr;

void Emit(const char *text, size_t size) {
    if (g_sink != nullptr) {
        g_sink(text, size);
    } else {
        std::fwrite(text, 1, size, stderr);
    }
}

} // namespace

Result SetLogging(u32 enabled) {
    g_logging_enabled = (enabled != 0);
    return 0;
}

Result GetLogging(u32 *enabled) {
    *enabled = g_logging_enabled ? 1 : 0;
    return 0;
}

namespace ams::log {

    void SetSink(LogSink sink) {
        g_sink = sink;
    }

    void LogFormatImpl(const char *fmt, ...) {
        if (!g_logging_enabled) {
            return;
        }
        char buffer[0x400];
        std::va_list args;
        va_start(args, fmt);
        const int len = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        if (len > 0) {
            const size_t size = static_cast<size_t>(len) < sizeof(buffer) ? static_cast<size_t>(len) : sizeof(buffer) - 1;
            Emit(buffer, size);
        }
    }

    void LogHexImpl(const void *data, int size) {
        if (!g_logging_enabled) {
            return;
        }
        LogFormatImpl("Bin Log: %d (%p)", size, data);

        const auto *bytes = static_cast<const u8 *>(data);
        char line[0x40];
        for (int i = 0; i < size; i += 16) {
            const int count = (size - i) < 16 ? (size - i) : 16;
            int offset = 0;
            for (int j = 0; j < count; ++j) {
                offset += std::snprintf(line + offset, sizeof(line) - static_cast<size_t>(offset), "%02x", bytes[i + j]);
            }
            offset += std::snprintf(line + offset, sizeof(line) - static_cast<size_t>(offset), "\n");
            Emit(line, static_cast<size_t>(offset));
        }
    }

    Result Initialize() {
        return 0;
    }

    void Finalize() {
        /* ... */
    }

} // namespace ams::log
