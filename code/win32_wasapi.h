#ifndef WIN32_WASAPI_H
#define WIN32_WASAPI_H

#include "handmade.h"
#include <mmdeviceapi.h>
#include <objbase.h>
#include <Audioclient.h>
#include <mmreg.h>
#include <cstdint>

struct Win32Audio {
    unsigned char *frame_buffer; // the buffer we get from wasapi
    void *ring_buffer;
    IAudioClient *client;
    IAudioRenderClient *render_client;
    WAVEFORMATEX *wave_fmt;
    uint32_t rb_capacity; // in bytes
    uint32_t buffer_frame_capacity;
    uint32_t rb_write_offset;
    uint32_t rb_read_offset;
    uint32_t rb_backlog_threshold;
};


void win32_init_wasapi(Win32Audio &audio, uint32_t samples_per_sec_, uint32_t buffer_size);
void win32_audio_lock_buffer(Win32Audio &audio, GameSoundOutput &sound_output, uint32_t bytes_to_write);
uint32_t win32_audio_unlock_buffer(Win32Audio &audio, uint32_t available_frames);

#endif // WIN32_WASAPI_H
