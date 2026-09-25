// CrashGuard.h -- turns a crash or a hang into a logged reboot.
//
// Normally a fault or a stuck loop freezes the board (and its USB port) until
// someone presses reset. With this, a hard fault -- or two seconds without
// crashGuardFeed() (watchdog) -- records where the code was in the SAMD51's
// backup RAM (which survives a reset) and restarts, so the costume keeps
// going and the board stays reprogrammable. The next boot reports it over USB
// serial; look the address up with arm-none-eabi-addr2line.
#pragma once

#include <Arduino.h>

void crashGuardBegin();             // call first thing in setup()
void crashGuardFeed();              // call at least once a second
bool crashGuardReport(Print& out);  // prints the last crash/hang, if any
