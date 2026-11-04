#include <stddef.h>
#include <stdint.h>

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