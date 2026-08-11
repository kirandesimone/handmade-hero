#ifndef WIN32_WASAPI_H
#define WIN32_WASAPI_H

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
    uint32_t buffer_frame_capacity;
    uint32_t rb_write_offset;
    uint32_t rb_read_offset;
    uint32_t rb_size; // in bytes
};

struct Win32AudioLockRegions {
    void *region1;
    void *region2;
    uint32_t region1_size;
    uint32_t region2_size;
};

void win32_init_wasapi(Win32Audio &audio, uint32_t samples_per_sec_, uint32_t buffer_size);
Win32AudioLockRegions win32_audio_lock_buffer(Win32Audio &audio, uint32_t bytes_to_write);
void win32_audio_unlock_buffer(Win32Audio &audio, Win32AudioLockRegions &regions);

#endif // WIN32_WASAPI_H
