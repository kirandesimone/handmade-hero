#include "win32_wasapi.h"

// may want to set our own samples_per_sec and buffer size later
// 1 frame == 2 samples (1 float for left channel, 1 float for right channel) (stereo)
void
win32_init_wasapi(Win32Audio &audio, uint32_t samples_per_sec_, uint32_t buffer_size)
{
    // Needed for COM bullshit and putting audio on a diff thread still needed
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HRESULT result {};
    // Get enumerator for all endpoints (devices)
    IMMDeviceEnumerator *device_enumerator = nullptr;

    result = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&device_enumerator);

    // TODO: Error check result here
    if(SUCCEEDED(result)) {
        IMMDevice *endpoint = nullptr;
        result = device_enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &endpoint);
        device_enumerator->Release();

        result = endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audio.client);
        endpoint->Release();

        result = audio.client->GetMixFormat(&audio.wave_fmt);

        /*
        Custom format??
        wave_fmt.wFormatTag = WAVE_FORMAT_PCM;
        wave_fmt.nChannels = 2;
        wave_fmt.wBitsPerSample = 16;
        wave_fmt.cbSize = 0;
        wave_fmt.nBlockAlign = (wave_fmt.wBitsPerSample * wave_fmt.nChannels) / 8;
        wave_fmt.nSamplesPerSec = samples_per_sec;
        wave_fmt.nAvgBytesPerSec = wave_fmt.nSamplesPerSec * wave_fmt.nBlockAlign;
        */

#ifdef BUILD_INTERNAL
        WAVEFORMATEXTENSIBLE *full_fmt = (WAVEFORMATEXTENSIBLE*)audio.wave_fmt;
        if (full_fmt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            OutputDebugString("Float");
        }
#endif

        // looking for 480 samples/10msec
        audio.client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            buffer_size, 0, audio.wave_fmt, NULL);

        result = audio.client->GetBufferSize(&audio.buffer_frame_capacity);
        result = audio.client->GetService(__uuidof(IAudioRenderClient), (void**)&audio.render_client);

        uint32_t padding {};
        uint32_t available_frames {};
        audio.client->GetCurrentPadding(&padding);
        available_frames = audio.buffer_frame_capacity - padding;
        audio.render_client->GetBuffer(available_frames, &audio.frame_buffer);

        // audio.out_size = audio.buffer_frame_capacity * audio.wave_fmt->nBlockAlign;
        audio.rb_size = audio.wave_fmt->nAvgBytesPerSec;
        // HMM not sure
        audio.ring_buffer = VirtualAlloc(NULL, audio.rb_size, MEM_RESERVE, PAGE_READWRITE);
    }
}

void
win32_audio_read_from_buffer(Win32Audio &audio)
{

}

void
win32_audio_write_to_buffer(Win32Audio &audio, uint32_t frame_count)
{

}
