/*
 * Minimal stand-in for newlib's <sys/lock.h>, which libnx's kernel headers
 * include. The host build does not use libnx's mutex/condvar implementation;
 * it only needs the types to exist so the headers parse.
 */
#pragma once

#include <stdint.h>

typedef int _LOCK_T;

typedef struct {
    int lock;
    int counter;
} _LOCK_RECURSIVE_T;
