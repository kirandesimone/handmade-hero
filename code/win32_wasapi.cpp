#include "win32_wasapi.h"
#include <atomic>
#include <cstdio>

// may want to set our own samples_per_sec and buffer size later
// 1 frame == 2 samples (1 float for left channel, 1 float for right channel) (stereo)
void
win32_init_wasapi(Win32Audio *audio, uint32_t samples_per_sec_, uint32_t buffer_size)
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

        result = endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audio->client);
        endpoint->Release();

        result = audio->client->GetMixFormat(&audio->wave_fmt);
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
        WAVEFORMATEXTENSIBLE *full_fmt = (WAVEFORMATEXTENSIBLE*)audio->wave_fmt;
        if (full_fmt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            OutputDebugString("Float");
        }
#endif
        // looking for 480 samples/10msec
        audio->client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            buffer_size, 0, audio->wave_fmt, NULL);

        audio->event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
        result = audio->client->SetEventHandle(audio->event_handle);
        result = audio->client->GetBufferSize(&audio->buffer_frame_capacity);
        result = audio->client->GetService(__uuidof(IAudioRenderClient), (void**)&audio->render_client);
        // Ensure its a power of 2
        audio->rb_capacity = pow2_round_up(audio->wave_fmt->nAvgBytesPerSec);
        audio->frame_count_bytes = (audio->wave_fmt->nSamplesPerSec/10) * audio->wave_fmt->nBlockAlign;
        // HMM not sure
        audio->ring_buffer = VirtualAlloc(NULL, audio->rb_capacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        audio->thread = CreateThread(NULL, 0, &win32_audio_thread_main, audio, 0, NULL);
    }
}

unsigned long WINAPI
win32_audio_thread_main(void *audio_ptr)
{
    Win32Audio *audio {reinterpret_cast<Win32Audio*>(audio_ptr)};
    unsigned long task_index {};
    HRESULT result {};

    audio->task_handle = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &task_index);
    ASSERT(audio->task_handle);

    while (WaitForSingleObject(audio->event_handle, INFINITE) == WAIT_OBJECT_0) {
        // uint32_t rb_free_space = g_audio.rb_size - (g_audio.rb_write_offset - g_audio.rb_read_offset);
        uint32_t padding {};
        audio->client->GetCurrentPadding(&padding);

        uint32_t available_frames = audio->buffer_frame_capacity - padding;
        uint32_t available_frame_bytes = available_frames * audio->wave_fmt->nBlockAlign;

        audio->render_client->GetBuffer(available_frames, &audio->frame_buffer);

        uint32_t rb_available_bytes = (audio->rb_write_offset.load(std::memory_order_acquire) -
            audio->rb_read_offset.load(std::memory_order_relaxed));
        uint32_t rb_available_frames = rb_available_bytes / audio->wave_fmt->nBlockAlign;

        uint32_t use_frames = min(rb_available_frames, available_frames);
        uint32_t use_bytes = use_frames * audio->wave_fmt->nBlockAlign;

        uint32_t read_region1_size = use_bytes;
        uint32_t read_region2_size = 0;

        uint32_t local_read_offset = (audio->rb_read_offset.load(std::memory_order_relaxed) &
            (audio->rb_capacity - 1));

        if (local_read_offset + read_region1_size > audio->rb_capacity) {
            read_region1_size = audio->rb_capacity - local_read_offset;
            read_region2_size = use_bytes - read_region1_size;
        }

        CopyMemory(audio->frame_buffer,
            (uint8_t*)audio->ring_buffer + local_read_offset, read_region1_size);
        CopyMemory(audio->frame_buffer + read_region1_size, (uint8_t*)audio->ring_buffer,
            read_region2_size);

        uint32_t bytes_read = read_region1_size + read_region2_size;
        uint32_t frames_read = bytes_read / audio->wave_fmt->nBlockAlign;

        audio->render_client->ReleaseBuffer(frames_read, 0);

        uint32_t new_offset = audio->rb_read_offset + bytes_read;

        audio->rb_read_offset.store(new_offset, std::memory_order_release);

        char backlog_buff[256];
        sprintf_s(backlog_buff, "available_frames: %d, padding: %d\n", available_frames, padding);
        OutputDebugStringA(backlog_buff);
    }

    return 0;
}

void
win32_audio_unlock_buffer(Win32Audio &audio, uint32_t bytes_written)
{
    uint32_t new_offset = audio.rb_write_offset + bytes_written;
    audio.rb_write_offset.store(new_offset, std::memory_order_release);
}

void
win32_audio_lock_buffer(Win32Audio &audio, GameSoundOutput &sound_output, uint32_t bytes_to_write)
{
    uint32_t rb_backlog = (audio.rb_write_offset - audio.rb_read_offset.load(std::memory_order_acquire));
    uint32_t rb_free_space = audio.rb_capacity - rb_backlog;

    unsigned char *buffer = (unsigned char*)audio.ring_buffer;
    uint32_t write_region1_size = bytes_to_write;
    uint32_t write_region2_size = 0; // if bytes_to_write + rb_write_offset > rb_size then we have to circle back
    // another way to clamp but capacity needs to be pow 2
    uint32_t local_write_offset = (audio.rb_write_offset.load(std::memory_order_relaxed) &
        (audio.rb_capacity - 1));

    // This might not be needed anymore since we only lock once we go below the threshold
    // and the threshold is set to be a 3-5 frame buffer
    if (write_region1_size > rb_free_space) {
        write_region1_size = rb_free_space;
        bytes_to_write = rb_free_space;
    }

    if (local_write_offset + bytes_to_write > audio.rb_capacity) {
        write_region1_size = audio.rb_capacity - local_write_offset;
        write_region2_size = bytes_to_write - write_region1_size;
    }

    sound_output.region1_size = write_region1_size;
    sound_output.region2_size = write_region2_size;
    sound_output.region1 = (void*)((uint8_t*)audio.ring_buffer + local_write_offset);
    sound_output.region2 = (void*)(uint8_t*)audio.ring_buffer;

#ifdef BUILD_INTERNAL
    char lock_buff[256];
    sprintf_s(lock_buff, "Backlog: %d, Free Space: %d, WR1: %d, WR2: %d\n", rb_backlog, rb_free_space, write_region1_size, write_region2_size);
    OutputDebugStringA(lock_buff);
#endif // BUILD_INTERNAL
}
