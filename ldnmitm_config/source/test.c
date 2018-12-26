#include "test.h"
#include "ldn.h"
#include <stdio.h>
#include <stdlib.h>
#include <switch.h>
#include <string.h>

Result scanTest(u16 num) {
    Service ldnSrv;
    UserLocalCommunicationService ldnU;
    Result rc = 0;

    rc = smGetService(&ldnSrv, "ldn:u");
    if (R_FAILED(rc)) {
        goto quit;
    }
    
    printf("ldnCreateUserLocalCommunicationService\n");
    rc = ldnCreateUserLocalCommunicationService(&ldnSrv, &ldnU);
    if (R_FAILED(rc)) {
        goto quit_srv;
    }

    printf("ldnInitialize\n");
    rc = ldnInitialize(&ldnU);
    if (R_FAILED(rc)) {
        goto quit_ldn;
    }

    printf("ldnOpenStation\n");
    rc = ldnOpenStation(&ldnU);
    if (R_FAILED(rc)) {
        goto quit_inf;
    }

    u32 state = 0;
    ldnGetState(&ldnU, &state);
    printf("Initialize succeed state: %d\n", state);

    size_t size = 0x480 * num;
    u8 nothing[0x60];
    u16 out;
    u8 *outBuf = malloc(size);
    memset(nothing, 0, 0x60);
    printf("ldnScan\n");
    rc = ldnScan(&ldnU, 0, nothing, &out, outBuf, size);
    free(outBuf);
    if (R_FAILED(rc)) {
        printf("Failed 1\n");
        goto quit_inf;
    }

    printf("Scan succeed. out: %d\n", out);

quit_inf:
    
quit_ldn:
    serviceClose(&ldnU.s);
quit_srv:
    serviceClose(&ldnSrv);
quit:
    return rc;
}
