# Wiring this into the application firmware

The bootloader's rollback logic depends on the **application** telling it
"I'm healthy" after a fresh update. Without this, a new image that boots
but is subtly broken would otherwise be treated as permanently active.

In the application's startup code (after your own sanity/self-checks —
e.g. sensors initialize, watchdog is running, no fault flags set):

```cpp
#include "rollback.h" 

void app_main() {
    // ... init and self-test ...

    boot::confirm_current_firmware(); // marks this bank as the new "active" bank

    // ... normal application loop ...
}
```

Call this **once**, only after you're confident the new firmware is
actually working — for example, after a successful sensor read, a
successful connection to your usual server, or N clean seconds of the
main loop with no faults. If you never call it and the device resets
`MAX_BOOT_ATTEMPTS` times, the bootloader automatically falls back to the
previous known-good bank (`rollback.cpp::should_rollback`).
