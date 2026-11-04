#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

// Every oscillator's state (synthinfo, its modulation and breakpoints, about 530 bytes), allocated by
// AMY's alloc_osc through this (a build-time patch, cmake/CardenzaSynthSourceLists.cmake) and given
// back when its voice is released. It is read and written every block the oscillator sounds: in PSRAM,
// whose bus the flash code shares, four DX7 tracks rendered about a sixth slower (the stress test,
// 2026-11-04). So internal RAM while that leaves this much, PSRAM after: the rest needs internal RAM
// more (a sample streamed from the card takes up to 16 KB of it above a 40 KB floor, its DMA cannot
// reach PSRAM; then the effects, loading, the drivers).
#define CARDENZA_AMY_OSC_FAST_FLOOR_BYTES (64u * 1024u)

void *cardenza_amy_osc_malloc(uint32_t size) {
#if defined(ESP_PLATFORM)
    const uint32_t fast = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    void *block = NULL;
    if (heap_caps_get_free_size(fast) >= size + CARDENZA_AMY_OSC_FAST_FLOOR_BYTES) block = heap_caps_malloc(size, fast);
    if (block == NULL) block = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (block == NULL) block = heap_caps_malloc(size, fast);
    return block;
#else
    return malloc(size);
#endif
}

void amy_platform_init(void) {}
void amy_platform_deinit(void) {}
void amy_update_tasks(void) {}
int16_t *amy_render_audio(void) { return (int16_t *)0; }
size_t amy_i2s_write(const uint8_t *buffer, size_t nbytes) {
    (void)buffer;
    return nbytes;
}

void stop_midi(void) {}
void run_midi(void) {}
void amy_send_midi_note_on(uint16_t osc) { (void)osc; }
void amy_send_midi_note_off(uint16_t osc) { (void)osc; }
void midi_out(uint8_t *bytes, uint16_t len) { (void)bytes; (void)len; }
void midi_local(uint8_t *bytes, uint16_t len) { (void)bytes; (void)len; }

void delay_ms(uint32_t ms) { (void)ms; }
// AMY's event pool: every note, preset and parameter goes to the oscillators as deltas, drawn from
// blocks of 256 (4 KB) that are never given back, and a preset's load queues a few hundred for every
// voice. Each note inserts its deltas into a time-sorted list, walked node by node: in PSRAM a note-on
// cost some 180 us (the stress test's profile, 2026-11-04). So internal RAM while that leaves the same
// floor as the oscillators' state, PSRAM after (the pool grows with every voice loaded).
void *cardenza_amy_pool_malloc(uint32_t size) {
#if defined(ESP_PLATFORM)
    const uint32_t fast = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    void *block = NULL;
    if (heap_caps_get_free_size(fast) >= size + CARDENZA_AMY_OSC_FAST_FLOOR_BYTES) block = heap_caps_malloc(size, fast);
    if (block == NULL) block = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (block == NULL) block = heap_caps_malloc(size, fast);
    return block;
#else
    return malloc(size);
#endif
}
