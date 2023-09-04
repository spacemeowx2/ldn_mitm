#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include <string.h>
#include "ldn.h"

#define MODULEID 0x233
static Service g_ldnSrv;
static LdnMitmConfigService g_ldnConfig;

Result saveLogToFile() {
    Result rc = 0;
    rc = ldnMitmSaveLogToFile(&g_ldnConfig);
    if (R_FAILED(rc)) {
        printf("Save log to file failed %x\n", rc);
        return rc;
    }

    return rc;
}

void cleanup() {
    serviceClose(&g_ldnSrv);
    serviceClose(&g_ldnConfig.s);
}

void die(const char *reason) {
    
    printf("fatal: %s\npress any key to exit.", reason);
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeAny(&pad);

    while(appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown) {
            break;
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    cleanup();
    exit(1);
}

void printHeader() {
    char version[32];
    Result rc = ldnMitmGetVersion(&g_ldnConfig, version);
    if (R_FAILED(rc)) {
        strcpy(version, "Error");
    }

    printf("    ldnmitm_config " VERSION_STRING "\n          ldn_mitm %s\n\n", version);
}

const char * getOnOff(u32 enabled) {
    if (enabled) {
        return CONSOLE_GREEN "ON" CONSOLE_RESET;
    } else {
        return CONSOLE_RED "OFF" CONSOLE_RESET;
    }
}

void printStatus() {
    u32 enabled;
    Result rc = ldnMitmGetLogging(&g_ldnConfig, &enabled);
    if (R_FAILED(rc)) {
        die("failed to get logging status");
    }
    printf("Logging(X): %s\n", getOnOff(enabled));

    rc = ldnMitmGetEnabled(&g_ldnConfig, &enabled);
    if (R_FAILED(rc)) {
        die("failed to get enabled status");
    }
    printf("ldn_mitm(Y): %s\n", getOnOff(enabled));

    putchar('\n');
    puts("Press X: toggle logging (sd:/ldn_mitm.log)");
    puts("Press Y: toggle ldn_mitm");
    puts("Press +: exit");
}

void reprint() {
    consoleClear();

    printHeader();
    printStatus();
}

void toggleLogging() {
    u32 enabled;
    Result rc = ldnMitmGetLogging(&g_ldnConfig, &enabled);
    if (R_FAILED(rc)) {
        die("failed to get logging status");
    }
    rc = ldnMitmSetLogging(&g_ldnConfig, !enabled);
    if (R_FAILED(rc)) {
        die("failed to set logging status");
    }
}

void toggleEnabled() {
    u32 enabled;
    Result rc = ldnMitmGetEnabled(&g_ldnConfig, &enabled);
    if (R_FAILED(rc)) {
        die("failed to get enabled status");
    }
    rc = ldnMitmSetEnabled(&g_ldnConfig, !enabled);
    if (R_FAILED(rc)) {
        die("failed to set enabled status");
    }
}

void getLdnMitmConfig() {
    Result rc = ldnMitmGetConfig(&g_ldnConfig);
    if (R_SUCCEEDED(rc)) {
        return;
    }

    Result namedRc = rc;
    rc = smGetService(&g_ldnSrv, "ldn:u");
    if (R_FAILED(rc)) {
        die("failed to get service ldn:u");
    }
    rc = ldnMitmGetConfigFromService(&g_ldnSrv, &g_ldnConfig);
    if (R_SUCCEEDED(rc)) {
        return;
    }

    printf("error code: 0x%x, 0x%x\n", rc, namedRc);
    die("ldn_mitm is not loaded");
}

int main(int argc, char* argv[]) {
    consoleInit(NULL);

    getLdnMitmConfig();
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeAny(&pad);

    u32 kDownOld = 0;

    reprint();
    while(appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus) {
            break;
        }
        if(kDown != kDownOld)
        {
            if (kDown & HidNpadButton_StickL) {
                Result rc = saveLogToFile();
                if (R_SUCCEEDED(rc)) {
                    puts("Export complete");
                }
            }

            if (kDown & HidNpadButton_X) {
                toggleLogging();
                reprint();
            }

            if (kDown & HidNpadButton_Y) {
                toggleEnabled();
                reprint();
            }
        }

        kDownOld = kDown;

        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    cleanup();
    return 0;
}
