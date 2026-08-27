#ifndef WIN32_WASAPI_H
#define WIN32_WASAPI_H

#include "handmade.h"

#include <mmdeviceapi.h>
#include <objbase.h>
#include <Audioclient.h>
#include <mmreg.h>
#include <cstdint>
#include <synchapi.h>
#include <avrt.h>

#include <cstdio>
#include <atomic>

struct Win32Audio {
    unsigned char *frame_buffer; // the buffer we get from wasapi
    void *ring_buffer;
    IAudioClient *client;
    IAudioRenderClient *render_client;
    WAVEFORMATEX *wave_fmt;
    void *event_handle;
    void *task_handle;
    void *thread;
    uint32_t rb_capacity; // in bytes
    uint32_t buffer_frame_capacity; // PLEASE CHANGE THIS SHITTY NAME
    uint32_t frame_count_bytes;
    std::atomic<uint32_t> rb_write_offset; // producer side
    std::atomic<uint32_t> rb_read_offset; // consumer side
};


void win32_init_wasapi(Win32Audio *audio, uint32_t samples_per_sec_, uint32_t buffer_size);
void win32_audio_lock_buffer(Win32Audio &audio, GameSoundOutput &sound_output, uint32_t bytes_to_write);
void win32_audio_unlock_buffer(Win32Audio &audio, uint32_t bytes_written);
unsigned long WINAPI win32_audio_thread_main(void *param);

#endif // WIN32_WASAPI_H
