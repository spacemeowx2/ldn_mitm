/*
 * debug.hpp -- logging, libnx-only.
 *
 * The original used ams::log (which wrote through ams::fs). Instead of pulling
 * that in, logging goes to a pluggable sink: the mock tests capture it, the
 * sysmodule installs a file sink (see sysmodule_main in poc/src).
 */
#pragma once

#include <switch.h>

#include <cstdarg>
#include <cstddef>

Result SetLogging(u32 enabled);
Result GetLogging(u32 *enabled);

namespace ams::log {

    using LogSink = void (*)(const char *text, size_t size);

    void SetSink(LogSink sink);
    void LogFormatImpl(const char *fmt, ...);
    void LogHexImpl(const void *data, int size);
    Result Initialize();
    void Finalize();

#define LogFormat(fmt, ...) ams::log::LogFormatImpl(fmt "\n", ##__VA_ARGS__)
#define LogHex(data, size) ams::log::LogHexImpl(data, size)

}
