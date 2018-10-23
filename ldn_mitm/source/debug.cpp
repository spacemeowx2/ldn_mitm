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
 
#include <switch.h>
#include <stratosphere.hpp>
#include <cstring>
#include "debug.hpp"

const size_t TlsBackupSize = 0x100;

void Reboot() {
    /* ... */
    LogStr("Reboot\n");
}

void Log(const void *data, int size) {
    (void)(data);
    (void)(size);
    /* ... */
#if 0
    u8 backup[TlsBackupSize];
    memcpy(backup, armGetTls(), TlsBackupSize);
    const u8 *dat = (const u8 *)data;
    char buf[128];
    sprintf(buf, "Bin Log: %d\n", size);
    LogStr(buf);
    for (int i = 0; i < size; i += 16) {
        int s = std::min(size - i, 16);
        buf[0] = 0;
        for (int j = 0; j < s; j++) {
            sprintf(buf + strlen(buf), "%02x", dat[i + j]);
        }
        sprintf(buf + strlen(buf), "\n");
        LogStr(buf);
    }
    memcpy(armGetTls(), backup, TlsBackupSize);
#endif
}

void LogStr(const char *str) {
    (void)(str);
    u8 backup[TlsBackupSize];
    memcpy(backup, armGetTls(), TlsBackupSize);
    FILE *file = fopen("sdmc:/space.log", "ab+");
    fwrite(str, 1, strlen(str), file);
    fclose(file);
    memcpy(armGetTls(), backup, TlsBackupSize);
}


struct fatalLaterIpc
{
    u64 magic;
    u64 cmd_id;
    u64 result;
    u64 unknown;
};
void fatalLater(Result err)
{
    Handle srv;

    while (R_FAILED(smGetServiceOriginal(&srv, smEncodeName("fatal:u"))))
    {
        // wait one sec and retry
        svcSleepThread(1000000000L);
    }

    // fatal is here time, fatal like a boss
    IpcCommand c;
    ipcInitialize(&c);
    ipcSendPid(&c);

    struct fatalLaterIpc* raw;

    raw = (struct fatalLaterIpc*) ipcPrepareHeader(&c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 1;
    raw->result = err;
    raw->unknown = 0;

    ipcDispatch(srv);
    svcCloseHandle(srv);
}
