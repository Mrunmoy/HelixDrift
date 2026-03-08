# Code Review: MCUboot OTA Bootloader for nRF52840 (XIAO)

**Commit:** `25da7ff5f100a8aac9ff0b20c89abefd117a7862`
**Date:** 2026-03-08
**Files Reviewed:** 26
**Lines added:** 1719 | **Lines removed:** 8

---

## Summary

This commit introduces a full OTA firmware update path for the Seeed XIAO nRF52840:
MCUboot standalone bootloader (OVERWRITE_ONLY, Ed25519-signed), a platform
flash-map backend, application linker scripts, a Cortex-M4 startup file, the
`OtaManager` state machine with 21 unit tests, and the `NrfOtaFlashBackend`
NVMC implementation. The architecture is sound and the OtaManager layer is
well-designed, but five critical issues prevent the firmware from running
correctly on real hardware: the FPU is never enabled (guaranteed UsageFault),
C++ global constructors are never called (NULL vtable crash), the bootloader
jump sequence is missing mandatory ARM barriers and interrupt masking, the
nrfx inline stub has no substitution mechanism for the real NVMC driver, and
neither linker script reserves the `.init_array` section. These must be fixed
before any on-target validation.

---

## Critical Issues

### [CR-001] No interrupt disable, DSB, or ISB before VTOR write and jump-to-app

- **File:** `bootloader/main.c`
- **Line(s):** 62-76
- **Severity:** CRITICAL

**Description:**
The boot-to-app sequence writes `SCB_VTOR` and immediately branches to the
application without disabling interrupts or issuing memory/instruction barriers:

```c
SCB_VTOR = app_vector_table;
__asm__ volatile(
    "msr msp, %0  \n"
    "bx  %1       \n"
    ...
);
```

Three distinct problems are present:

1. **No `CPSID I` before VTOR relocation.** If any NVIC-enabled interrupt
   fires between the `SCB_VTOR` write and the `BX`, the core looks up the
   handler via the NEW VTOR, which points into the application image. The
   application's ISRs are not yet running (the app's peripherals have not been
   initialized) and the application's stack pointer has not been set. This
   is a window-of-death race condition.

2. **No DSB after `SCB_VTOR` write.** The ARM Cortex-M4 TRM (section 4.2.2)
   states that after writing VTOR a Data Synchronization Barrier must
   complete before the new table is guaranteed visible to the hardware
   exception mechanism.

3. **No ISB after DSB.** An Instruction Synchronization Barrier is required
   to flush the prefetch pipeline so that no instructions fetched before the
   barrier execute against the stale VTOR.

**Recommendation:**
```c
/* Disable all maskable interrupts before relocating VTOR. */
__asm__ volatile("cpsid i" ::: "memory");

SCB_VTOR = app_vector_table;

/* Ensure the VTOR write has propagated to the bus fabric. */
__asm__ volatile("dsb" ::: "memory");
/* Flush the prefetch pipeline. */
__asm__ volatile("isb" ::: "memory");

/* Set MSP from the app vector table and branch. */
__asm__ volatile(
    "msr msp, %0  \n"
    "bx  %1       \n"
    :
    : "r"(app_sp), "r"(app_pc)
    : "memory"
);
```
The application's `Reset_Handler` is responsible for re-enabling interrupts
after it initializes its own peripherals.

---

### [CR-002] FPU not enabled in startup -- hard-float code will UsageFault

- **File:** `firmware/platform/startup_nrf52840.S`
- **Severity:** CRITICAL

**Description:**
The toolchain is configured with `-mfpu=fpv4-sp-d16 -mfloat-abi=hard`
(see `tools/toolchains/arm-none-eabi-gcc.cmake` line 12). With `hard` ABI,
the compiler generates `VPUSH`/`VPOP` instructions in every function prologue
that touches floating-point registers.

The nRF52840 (Cortex-M4F) resets with the FPU disabled: CPACR[23:20] = 0b0000
(CP10 and CP11 denied). Executing a VFP instruction while the FPU is disabled
triggers a **UsageFault** (NOCP bit set in UFSR). The startup code goes
directly to `main()` without enabling the FPU, so the very first hard-float
C++ function entered -- including `main()` itself if any sensor init function
inlines a float multiply -- will crash.

The `MocapNodePipeline`, sensor fusion, and Kalman filter code makes heavy use
of floating-point; this will fault immediately after sensor init.

**Recommendation:**
Add FPU enablement in `Reset_Handler` before the call to `main()`:

```asm
    /* Enable FPU: grant CP10 and CP11 full access (Cortex-M4F). */
    ldr   r0, =0xE000ED88     @ CPACR
    ldr   r1, [r0]
    orr   r1, r1, #(0xF << 20)
    str   r1, [r0]
    dsb
    isb
```

---

### [CR-003] `__libc_init_array()` never called -- C++ globals have uninitialized vtables

- **File:** `firmware/platform/startup_nrf52840.S` (lines 109-111),
           `examples/nrf52-mocap-node/src/board_xiao_nrf52840.cpp` (lines 9-10)
- **Severity:** CRITICAL

**Description:**
The `Reset_Handler` jumps directly to `main()` after copying `.data` and
zeroing `.bss`. It never calls `__libc_init_array()`, which is the newlib
mechanism for running C++ static-storage-duration constructors.

In `board_xiao_nrf52840.cpp`:

```cpp
helix::NrfOtaFlashBackend g_otaBackend;       // has a vtable
helix::OtaManager         g_otaManager{g_otaBackend};  // stores a reference
```

`NrfOtaFlashBackend` inherits `OtaFlashBackend` which declares `virtual
~OtaFlashBackend()`. Its constructor must set the vptr. Static storage is
zero-initialized by the `.bss`-clearing loop, so the vptr field is zero
(NULL). `OtaManager::OtaManager(OtaFlashBackend&)` stores a reference to
`g_otaBackend` -- without the constructor running, `backend_` is
uninitialized.

Any call path through `xiao_ota_write_chunk` or `xiao_ota_control` that
reaches `g_otaManager.backend_.eraseSlot()` will dispatch through a NULL
vtable pointer, causing an immediate fault.

The same problem affects every other global C++ object in the application.

**Recommendation:**
Call `__libc_init_array()` (and optionally `__libc_fini_array()` via
`atexit`) in `Reset_Handler` before branching to `main()`:

```asm
    /* Run C++ static constructors (newlib .init_array). */
    bl    __libc_init_array

    /* Call main() */
    bl    main
```

---

### [CR-004] `.init_array` section not declared in either linker script

- **File:** `tools/linker/xiao_nrf52840_app.ld`, `tools/linker/xiao_nrf52840_boot.ld`
- **Severity:** CRITICAL

**Description:**
Neither linker script defines an explicit `.init_array` output section.
Without it, GNU ld places the `.init_array` input sections as "orphans",
emitting a warning and choosing an arbitrary location. More critically, the
`_init_array_start` / `_init_array_end` symbols that `__libc_init_array()`
relies on are never defined, so even after fixing CR-003, the constructor
invocation loop would iterate over garbage pointers.

**Recommendation:**
Add the section to both linker scripts inside the `FLASH` region, after
`.rodata`:

```ld
    .init_array : {
        _init_array_start = .;
        KEEP (*(.init_array*))
        _init_array_end = .;
    } > FLASH

    .fini_array : {
        _fini_array_start = .;
        KEEP (*(.fini_array*))
        _fini_array_end = .;
    } > FLASH
```

---

### [CR-005] nrfx stub is `static inline` -- all NVMC operations are silently no-ops in every build

- **File:** `tools/nrf/stubs/include/nrfx_nvmc.h`,
           `bootloader/CMakeLists.txt` (lines 57, 74),
           `examples/nrf52-mocap-node/src/NrfOtaFlashBackend.cpp`
- **Severity:** CRITICAL

**Description:**
`nrfx_nvmc.h` declares all three NVMC entry points as `static inline` no-ops:

```c
static inline void nrfx_nvmc_page_erase(uint32_t page_addr) { (void)page_addr; }
static inline void nrfx_nvmc_word_write(uint32_t address, uint32_t value)  { ... }
static inline bool nrfx_nvmc_write_done_check(void) { return true; }
```

Both `bootloader/CMakeLists.txt` and the application build unconditionally add
`tools/nrf/stubs/include` to the include path with no target or
`CMake_BUILD_TYPE` guard. Because the functions are `static inline`, the
preprocessor substitutes them at every call site. There is no mechanism to
replace them with a real driver at link time -- a separately-compiled
`nrfx_nvmc.c` would define different symbols and cause multiply-defined-symbol
errors rather than overriding the inlines.

Consequence: the bootloader binary produced by this CMakeLists will never
erase or write flash on real hardware. Every `flash_area_erase()` and
`flash_area_write()` call in `flash_map_backend.c` and every
`eraseSlot()` / `writeChunk()` in `NrfOtaFlashBackend` is a no-op.

**Recommendation:**
Gate the stub behind an `NRFX_STUB` build option:

```cmake
if(NRFX_STUB)
    target_include_directories(... "tools/nrf/stubs/include")
else()
    # Link real nrfx_nvmc.c from nRF5 SDK / nrfx submodule
    target_sources(... "${NRFX_DIR}/drivers/src/nrfx_nvmc.c")
    target_include_directories(... "${NRFX_DIR}/...")
endif()
```

Mark all CI builds that only validate compilation (not real hardware) with
`-DNRFX_STUB=ON`. Hardware builds must use `-DNRFX_STUB=OFF` and provide the
real nrfx source.

---

## Major Issues

### [CR-006] `setPendingUpgrade()` writes BOOT_MAGIC in OVERWRITE_ONLY mode -- violates interface contract and can corrupt a full-slot image

- **File:** `examples/nrf52-mocap-node/src/NrfOtaFlashBackend.cpp` (lines 33-38),
           `firmware/common/ota/OtaFlashBackend.hpp` (lines 30-34)
- **Severity:** MAJOR

**Description:**
`OtaFlashBackend.hpp` explicitly documents `setPendingUpgrade()` as:

> "For MCUboot overwrite-only mode this is a **no-op** (MCUboot upgrades when
> it finds a valid newer image)"

The implementation contradicts this: it writes 16 bytes of `kBootMagic` to
`kSecondarySlotBase + kSecondarySlotSize - sizeof(kBootMagic)`. Two problems:

1. **OVERWRITE_ONLY does not use the trailer magic.** MCUboot's overwrite-only
   loader (`boot/bootutil/src/loader.c`, function `boot_perform_update()`)
   decides to upgrade based solely on the image version in the secondary slot
   header. Writing magic bytes to the trailer has no effect on MCUboot's
   decision and produces unnecessary flash wear.

2. **Image corruption when `imageSize == slotSize`.** `OtaManager::begin()`
   accepts `imageSize == slotSize` (the check is `>`, not `>=`). When the
   caller writes exactly `kSecondarySlotSize` bytes, `writeChunk` fills the
   entire slot. `setPendingUpgrade()` then overwrites the last 16 bytes of the
   image with `kBootMagic`, silently corrupting the signed image. MCUboot will
   then fail signature verification and refuse to upgrade.

**Recommendation:**
For OVERWRITE_ONLY, `setPendingUpgrade()` should be a documented no-op:

```cpp
bool NrfOtaFlashBackend::setPendingUpgrade() {
    // OVERWRITE_ONLY: MCUboot upgrades based on image version, no trailer
    // magic required.  For swap-with-revert builds, write BOOT_MAGIC here.
    return true;
}
```

Alternatively, reduce `begin()`'s acceptance condition to
`imageSize > slotSize - sizeof(kBootMagic)` if trailer reservation is
intended; but that breaks the OVERWRITE_ONLY use case unnecessarily.

---

### [CR-007] Integer overflow in `flash_area_read`, `flash_area_write`, and `flash_area_erase` bounds checks

- **File:** `bootloader/port/flash_map_backend.c` (lines 68, 80, 110)
- **Severity:** MAJOR

**Description:**
All three functions use the same bounds check pattern:

```c
if (off + len > fap->fa_size) return -1;
```

Both `off` and `len` are `uint32_t`. When both are large, their sum wraps
around. Example: `off = 0xFFFFFFF0`, `len = 0x20` -- the sum is `0x00000010`,
which is less than any reasonable `fa_size`, so the check passes and the
subsequent write proceeds 240 MB past the intended end of the flash area.

Although MCUboot calls these functions with trusted, bounded values, this is
a latent security defect in a security-critical component (a bootloader).

**Recommendation:**
Use an overflow-safe comparison:

```c
if (len > fap->fa_size || off > fap->fa_size - len) return -1;
```

---

### [CR-008] Private Ed25519 signing key committed to the repository in plaintext

- **File:** `keys/dev_signing_key.pem`
- **Severity:** MAJOR

**Description:**
A plaintext PKCS#8 Ed25519 private key is committed to the repository. While
the `keys/README.md` and inline comments label it a "CI-only dev key", the
safeguards are only advisory:

- No `.gitignore` entry prevents a developer from accidentally committing a
  production key in the same location.
- Every fork, archive, or clone of the repository now possesses this key
  permanently (git history).
- CI uses this key to sign artifacts uploaded as workflow artifacts -- any
  party with access to those artifacts can use the key to sign malicious
  firmware.

**Recommendation:**
1. Rotate this key immediately and treat the current one as compromised.
2. Store the new key in a GitHub Actions secret (`secrets.SIGNING_KEY`) and
   write it to a temp file in CI:
   ```yaml
   - run: echo "${{ secrets.SIGNING_KEY }}" > /tmp/signing.pem
   ```
3. Add `keys/*.pem` to `.gitignore` with a comment explaining CI secret usage.
4. The `ed25519_pub_key.h` (public key only) may remain in the repository.

---

### [CR-009] `xiao_ota_control(cmd=0, ...)` returns `ERROR_INVALID_STATE` instead of `OK`

- **File:** `examples/nrf52-mocap-node/src/board_xiao_nrf52840.cpp` (lines 94-102)
- **Severity:** MAJOR

**Description:**
The header `board_xiao_nrf52840.hpp` documents command byte 0 as a polling
no-op that returns 0:

```cpp
// 0 = none (polling, returns 0)
```

The implementation handles commands 1, 2, and 3 explicitly, but falls through
to:

```cpp
return static_cast<uint8_t>(helix::OtaStatus::ERROR_INVALID_STATE);
```

for any other value (including 0). A BLE host polling the OTA control
characteristic with cmd=0 will receive a non-zero error byte on every poll,
making it impossible to distinguish a genuine error from an idle state.

**Recommendation:**
```cpp
if (cmd == 0) {
    return static_cast<uint8_t>(helix::OtaStatus::OK);
}
```

Also: replace the raw magic numbers 1/2/3 with named constants or an `enum
class OtaControlCommand : uint8_t` to prevent future off-by-one errors.

---

### [CR-010] `flash_area_write` does not handle a non-4-byte-aligned start address

- **File:** `bootloader/port/flash_map_backend.c` (lines 82-101)
- **Severity:** MAJOR

**Description:**
`flash_area_write` performs word writes directly at `addr = fap->fa_off + off`
without checking that `addr` is 4-byte aligned:

```c
uint32_t addr = fap->fa_off + off;
// ...
while (written + 4u <= len) {
    nrfx_nvmc_word_write(addr + (uint32_t)written, w);
```

On the nRF52840, `NVMC->CONFIG` word-writes require a 4-byte-aligned address;
an unaligned address causes undefined hardware behavior (typically the write
is silently dropped or the wrong word is written). `NrfOtaFlashBackend::writeAligned()`
correctly handles leading unalignment, but the bootloader backend does not.

MCUboot's `bootutil` core respects `flash_area_align()` and passes aligned
`off` values in normal operation, so this is a latent rather than
currently-triggered bug. However, the function is exported as a general API
and the constraint is undocumented.

**Recommendation:**
Add an alignment assertion at the top of `flash_area_write`, or implement the
same leading-byte padding logic used in `NrfOtaFlashBackend::writeAligned()`.
At minimum add a comment:

```c
/* MCUboot callers always pass off aligned to flash_area_align() == 4.
 * Unaligned off is not supported; behavior is undefined on nRF52840 NVMC. */
```

---

### [CR-011] Stack placed after `.bss` instead of at the fixed top of RAM

- **File:** `tools/linker/xiao_nrf52840_app.ld` (lines 46-52),
           `tools/linker/xiao_nrf52840_boot.ld` (lines 40-46)
- **Severity:** MAJOR

**Description:**
Both linker scripts place `.stack` as the last section in `RAM`, immediately
after `.bss`. This means the initial MSP (`_stack_top`) is at an address that
changes every time global data or BSS grows. As the codebase evolves:

1. The stack end (`_stack_top`) moves upward toward the end of RAM.
2. If `.data + .bss` ever exceeds `RAM_SIZE - stack_size`, the linker errors
   at build time (acceptable).
3. More dangerously: at runtime the stack grows downward from `_stack_top`
   toward `_stack_bottom = _stack_top - 8K`. If stack usage ever exceeds 8 KB,
   it silently overwrites `.bss` variables. There is no MPU guard page between
   the stack bottom and `.bss`.

The conventional pattern for Cortex-M places the initial SP at
`ORIGIN(RAM) + LENGTH(RAM)` regardless of other section sizes, giving the
stack the maximum possible headroom and making stack-BSS proximity a
link-time (not runtime) concern.

**Recommendation:**
```ld
PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));

.stack (NOLOAD) : {
    . = ALIGN(8);
    _stack_bottom = ORIGIN(RAM) + LENGTH(RAM) - 8K;
    . = _stack_bottom;
} > RAM
```

Or use the simpler but equally common:
```ld
/* Vector table entry 0: initial MSP */
_stack_top = ORIGIN(RAM) + LENGTH(RAM);
```
with `_stack_bottom` used for optional MPU configuration.

---

## Minor Issues

### [CR-012] `.size g_pfnVectors` directive placed before the label -- reports size 0

- **File:** `firmware/platform/startup_nrf52840.S` (lines 25-27)
- **Severity:** MINOR

**Description:**
```asm
    .type  g_pfnVectors, %object
    .size  g_pfnVectors, .-g_pfnVectors    ← evaluated here: . == g_pfnVectors, size = 0
g_pfnVectors:
    .word  _stack_top
    ...
```

The `.size` directive is evaluated at its point of appearance in the assembly.
At that point `.` equals the address of `g_pfnVectors` (the label has not been
defined yet), so `.-g_pfnVectors = 0`. GNU `objdump`, GDB, and crash-analysis
tools all see the vector table as a zero-byte symbol. This makes post-mortem
analysis significantly harder.

**Recommendation:**
Move `.size` to after the last `.word` entry:
```asm
g_pfnVectors:
    .word  _stack_top
    ...
    .word  SPIM3_IRQHandler
    .size  g_pfnVectors, .-g_pfnVectors
```

---

### [CR-013] Bootloader fault LED never blinks -- only turns on

- **File:** `bootloader/main.c` (lines 46-50)
- **Severity:** MINOR

**Description:**
The error comment and loop structure both imply blinking behaviour:

```c
/* No valid image -- blink fault LED forever. */
fault_led_init();
while (1) {
    fault_led_on();
    spin_delay(3200000u);  /* ~50 ms on at 64 MHz */
}
```

`fault_led_on()` writes to `GPIO_OUTCLR` (sets the pin low, turning the LED
on). There is no corresponding `fault_led_off()` call to set the pin high
again. The LED is simply held on continuously with a busy-wait between
identical on-commands. Additionally, the spin_delay comment says "64 MHz" but
the nRF52840 boots at 16 MHz (internal RC; HFXO is not enabled by the
bootloader), making the actual hold time ~200 ms.

**Recommendation:**
Add a `fault_led_off()` helper writing to `GPIO_OUTSET`, alternate calls, and
correct the timing comment.

---

### [CR-014] `_etext` defined as `LOADADDR(.data) + SIZEOF(.data)` -- misleads debuggers

- **File:** `tools/linker/xiao_nrf52840_app.ld` (line 37)
- **Severity:** MINOR

**Description:**
```ld
_etext = LOADADDR(.data) + SIZEOF(.data);
```

By convention on ARM bare-metal, `_etext` is the end of the `.text` section
(= LMA of `.data`). Here it is defined as the end of `.data`'s LMA (= end of
all flash-resident initialized data). GDB, `arm-none-eabi-size`, and
`addr2line` all interpret `_etext` as the `.text` region boundary. This
misdefinition shifts the reported code-size boundary and makes memory map
analysis unreliable.

The startup code uses `_sidata` (correctly defined as `LOADADDR(.data)`) to
copy `.data`, so there is no runtime impact. But `_etext` should be fixed to
reflect common convention.

**Recommendation:**
```ld
.text : { ... } > FLASH
_etext = .;            /* end of .text = LMA of .data */

.data : AT(_etext) {
    _sdata = .;
    ...
} > RAM
_sidata = LOADADDR(.data);   /* = _etext */
```

---

### [CR-015] `imgtool verify` passes the private key and silently swallows failure

- **File:** `scripts/sign_firmware.sh` (line 48)
- **Severity:** MINOR

**Description:**
```bash
imgtool verify --key "${KEY_PEM}" "${OUTPUT_HEX}" || true
```

Two problems:

1. `imgtool verify` only needs the public key; passing the private key
   unnecessarily exposes `KEY_PEM` to another process and contradicts
   principle-of-least-privilege.

2. `|| true` means a failed verification (e.g., signing produced a corrupt
   image) is silently ignored. CI will mark the step as passed even when the
   signed image is unverifiable.

**Recommendation:**
Extract the public key once and use it for verification, and fail on
verification error:

```bash
imgtool getpub --key "${KEY_PEM}" --output /tmp/pub.pem
imgtool verify --key /tmp/pub.pem "${OUTPUT_HEX}"
```

---

### [CR-016] `.fpu softvfp` in startup vs `-mfloat-abi=hard` in toolchain -- assembler warning

- **File:** `firmware/platform/startup_nrf52840.S` (line 5)
- **Severity:** MINOR

**Description:**
The toolchain sets `-mfpu=fpv4-sp-d16 -mfloat-abi=hard`. The startup
assembly file declares `.fpu softvfp`. This mismatch may produce an assembler
warning about incompatible FPU directives. There is no runtime impact because
the startup file contains no floating-point instructions, but it creates
noise in the build log and could mask real warnings.

**Recommendation:**
Change to:
```asm
.fpu fpv4-sp-d16
```

---

## Positive Observations

- **OtaManager state machine** is clean and complete. The three-state
  (IDLE/RECEIVING/COMMITTED) model is minimal and correct. Early-return error
  propagation keeps nesting shallow. All transitions are guarded.

- **21 unit tests with excellent coverage.** Every error code path (bad
  offset, incomplete image, wrong CRC, erase failure, backend write failure,
  setPendingUpgrade failure) has a dedicated test. Re-use after abort and after
  commit are both covered. This is a high-quality TDD suite.

- **CRC32 algorithm is consistent end-to-end.** The IEEE 802.3 polynomial
  (0xEDB88320), pre-seeded accumulator (0xFFFFFFFF), and final XOR
  (0xFFFFFFFF) all match between `OtaManager::crc32Update()`, the
  `OtaManager::commit()` finaliser, and the test-helper `crc32()`. There is
  no risk of a systematic CRC mismatch causing false integrity failures.

- **Flash layout is consistent across all components.** Primary slot at
  0x10000 (384 KB), secondary at 0x70000 (384 KB), scratch at 0xD0000 (32 KB)
  -- this is correctly expressed in the linker scripts,
  `flash_map_backend.c`, `NrfOtaFlashBackend.hpp`, `mcuboot_config.h`,
  `sign_firmware.sh` (`SLOT_SIZE=0x60000`), and the commit message. No
  off-by-one or cross-component mismatches.

- **VTOR alignment is correct.** The application image header is 0x200 bytes,
  placing the app vector table at 0x10200. The nRF52840 requires VTOR to be
  aligned to a power-of-two >= 4 * num_vectors (80 vectors * 4 = 320 bytes,
  so 512-byte / 0x200 boundary). 0x10200 % 0x200 == 0. The 0x200 header size
  is not accidental.

- **`NrfOtaFlashBackend::writeAligned()`** correctly handles all three cases
  of unaligned data: leading unaligned bytes (partial first word), middle
  aligned words, and trailing partial word, using `memcpy` to avoid strict-
  aliasing UB. This is better than the bootloader's `flash_area_write`.

- **`OtaFlashBackend` pure virtual interface** cleanly separates the portable
  state machine from hardware-specific flash operations. The weak-symbol
  hook pattern (`xiao_ota_control` / `xiao_ota_write_chunk`) is a
  well-suited integration approach for a no-RTOS, no-BLE-stack bare-metal
  build.

- **`BOOT_MAX_IMG_SECTORS = 256`** correctly exceeds the 96 sectors actually
  in a 384 KB / 4 KB-page slot, so MCUboot's sector array is properly sized.

- **MCUboot OVERWRITE_ONLY + Ed25519** is the right v1 configuration: no
  revert complexity, strong signing, minimal flash wear.

---

## Statistics

| Category | Count |
|---|---|
| Critical | 5 |
| Major | 6 |
| Minor | 5 |
| Files reviewed | 26 |
| Lines added | 1719 |
| Lines removed | 8 |

---

## Recommended Fix Priority

| Priority | Issue | Blocker for |
|---|---|---|
| P0 | CR-002 FPU enable | Any sensor code on target |
| P0 | CR-003 `__libc_init_array` | OTA manager on target |
| P0 | CR-004 `.init_array` linker section | OTA manager on target |
| P0 | CR-001 Bootloader jump barriers | Safe boot handoff |
| P0 | CR-005 nrfx stub substitution | Any flash operation on target |
| P1 | CR-006 `setPendingUpgrade` no-op | Image integrity |
| P1 | CR-007 Integer overflow bounds | Security |
| P1 | CR-009 cmd=0 polling fix | BLE OTA protocol |
| P2 | CR-008 Key rotation | Production safety |
| P2 | CR-010 Unaligned write | Bootloader reliability |
| P2 | CR-011 Stack at top of RAM | Future firmware growth |
| P3 | CR-012 through CR-016 | Quality / tooling |
