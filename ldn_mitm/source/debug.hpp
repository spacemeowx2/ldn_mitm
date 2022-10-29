/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <stratosphere.hpp>
#include <stdio.h>
#include <cstdarg>

ams::Result SetLogging(u32 enabled);
ams::Result GetLogging(u32 *enabled);

namespace ams::log
{
    void LogFormatImpl(const char *fmt, ...);
    void LogHexImpl(const void *data, int size);
    Result Initialize();
    void Finalize();

#define LogFormat(fmt, ...) ams::log::LogFormatImpl(fmt "\n", ##__VA_ARGS__)
#define LogHex(data, size) ams::log::LogHexImpl(data, size)
}
