#ifndef WIN32_WASAPI_H
#define WIN32_WASAPI_H

#include <mmdeviceapi.h>
#include <objbase.h>
#include <Audioclient.h>
#include <mmreg.h>
#include <cstdint>

struct Win32Audio {
    void *rb;
    uint32_t rb_write_offset;
    uint32_t rb_read_offset;
    uint32_t rb_size; // in bytes

    IAudioClient *client;
    IAudioRenderClient *render_client;
    WAVEFORMATEX *wave_fmt;
    uint32_t buffer_frame_capacity;
};

void win32_audio_init(Win32Audio &audio, uint32_t samples_per_sec_, uint32_t buffer_size);
void win32_audio_write_to_buffer(Win32Audio &audio, uint32_t frame_count);
void win32_audio_read_from_buffer(Win32Audio &audio);

#endif // WIN32_WASAPI_H
