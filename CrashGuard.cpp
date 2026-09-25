// CrashGuard.cpp -- see CrashGuard.h.
#include "CrashGuard.h"

#if defined(__SAMD51__)

namespace {

const uint32_t kMagic = 0xC7A5B00Fu;

struct CrashRecord {
  uint32_t magic, kind, pc, lr, cfsr, bfar, count;
};

CrashRecord* const gRec = (CrashRecord*)BKUPRAM_ADDR;  // kept across resets
CrashRecord gLast;
bool gHave = false;

}  // namespace

// frame: r0 r1 r2 r3 r12 lr pc xpsr, as stacked on exception entry.
extern "C" __attribute__((used)) void crashGuardRecord(uint32_t* frame, uint32_t kind) {
  gRec->count = gRec->magic == kMagic ? gRec->count + 1 : 1;
  gRec->kind = kind;
  gRec->pc = frame[6];
  gRec->lr = frame[5];
  gRec->cfsr = SCB->CFSR;
  gRec->bfar = SCB->BFAR;
  gRec->magic = kMagic;
  NVIC_SystemReset();
}

extern "C" __attribute__((naked)) void HardFault_Handler(void) {
  __asm volatile(
      "tst lr, #4            \n"
      "ite eq                \n"
      "mrseq r0, msp         \n"
      "mrsne r0, psp         \n"
      "movs r1, #0           \n"
      "b crashGuardRecord    \n");
}

// Watchdog early warning: the main loop has stalled for a second.
extern "C" __attribute__((naked)) void WDT_Handler(void) {
  __asm volatile(
      "tst lr, #4            \n"
      "ite eq                \n"
      "mrseq r0, msp         \n"
      "mrsne r0, psp         \n"
      "movs r1, #1           \n"
      "b crashGuardRecord    \n");
}

void crashGuardBegin() {
  MCLK->AHBMASK.reg |= MCLK_AHBMASK_BKUPRAM;
  if (gRec->magic == kMagic) {
    gLast = *gRec;
    gHave = true;
    gRec->magic = 0;
  }
  // Watchdog on the always-on 1.024 kHz clock: early-warning interrupt after
  // ~1 s without a feed (logs and reboots), hard reset after ~2 s regardless.
  WDT->CTRLA.reg = 0;
  while (WDT->SYNCBUSY.reg) {
  }
  WDT->CONFIG.reg = WDT_CONFIG_PER_CYC2048;
  WDT->EWCTRL.reg = WDT_EWCTRL_EWOFFSET_CYC1024;
  WDT->INTFLAG.reg = WDT_INTFLAG_EW;
  WDT->INTENSET.reg = WDT_INTENSET_EW;
  NVIC_SetPriority(WDT_IRQn, 0);
  NVIC_EnableIRQ(WDT_IRQn);
  WDT->CTRLA.reg = WDT_CTRLA_ENABLE;
  while (WDT->SYNCBUSY.reg) {
  }
}

void crashGuardFeed() {
  if (!WDT->SYNCBUSY.bit.CLEAR) WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
}

bool crashGuardReport(Print& out) {
  if (!gHave) return false;
  char line[112];
  snprintf(line, sizeof(line),
           "Recovered from a %s (x%lu): pc=0x%08lx lr=0x%08lx cfsr=0x%08lx bfar=0x%08lx",
           gLast.kind ? "hang" : "crash", (unsigned long)gLast.count,
           (unsigned long)gLast.pc, (unsigned long)gLast.lr, (unsigned long)gLast.cfsr,
           (unsigned long)gLast.bfar);
  out.println(line);
  return true;
}

#else

void crashGuardBegin() {}
void crashGuardFeed() {}
bool crashGuardReport(Print& out) {
  (void)out;
  return false;
}

#endif
