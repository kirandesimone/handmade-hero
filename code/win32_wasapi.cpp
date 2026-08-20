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
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            buffer_size, 0, audio.wave_fmt, NULL);

        audio.eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
        result = audio.client->SetEventHandle(audio.eventHandle);
        result = audio.client->GetBufferSize(&audio.buffer_frame_capacity);
        result = audio.client->GetService(__uuidof(IAudioRenderClient), (void**)&audio.render_client);
        // Ensure its a power of 2
        audio.rb_capacity = pow2_round_up(audio.wave_fmt->nAvgBytesPerSec);
        // HMM not sure
        audio.ring_buffer = VirtualAlloc(NULL, audio.rb_capacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
}

void
win32_audio_thread_main()
{
    uint32_t padding {};
    uint32_t available_frames {};

    while (WaitForSingleObject()) {
        // uint32_t rb_free_space = g_audio.rb_size - (g_audio.rb_write_offset - g_audio.rb_read_offset);
        g_audio.client->GetCurrentPadding(&padding);
        uint32_t rb_backlog = (g_audio.rb_write_offset - g_audio.rb_read_offset);
        if (rb_backlog < g_audio.rb_backlog_threshold) {
            win32_audio_lock_buffer(g_audio, sound_output, g_audio.rb_backlog_threshold - rb_backlog);
        }

        game_update_and_render(memory, sound_output, new_input, buffer);

        available_frames = g_audio.buffer_frame_capacity - padding;
        g_audio.render_client->GetBuffer(available_frames, &g_audio.frame_buffer);

        uint32_t bytes_read = win32_audio_unlock_buffer(g_audio, available_frames);
        uint32_t frames_read = bytes_read / g_audio.wave_fmt->nBlockAlign;

        g_audio.render_client->ReleaseBuffer(frames_read, 0);

        /*
        char backlog_buff[256];
        sprintf_s(backlog_buff, "frame backlog: %d, available_frame: %d, padding: %d\n", rb_backlog, available_frames, padding);
        OutputDebugStringA(backlog_buff);
        */
    }
}

uint32_t
win32_audio_unlock_buffer(Win32Audio &audio, uint32_t available_frames)
{
    // The buffer is empty
    if (audio.rb_read_offset == audio.rb_write_offset) {
        return 0;
    }

    uint32_t available_frame_bytes = available_frames * audio.wave_fmt->nBlockAlign;
    uint32_t read_region1_size = available_frame_bytes;
    uint32_t read_region2_size = 0;
    uint32_t rb_frame_bytes = audio.rb_write_offset - audio.rb_read_offset;
    uint32_t local_read_offset = audio.rb_read_offset & (audio.rb_capacity - 1);

    if (read_region1_size > rb_frame_bytes) {
        read_region1_size = rb_frame_bytes;
        available_frame_bytes = rb_frame_bytes;
    }

    if (local_read_offset + read_region1_size > audio.rb_capacity) {
        read_region1_size = audio.rb_capacity - local_read_offset;
        read_region2_size = available_frame_bytes - read_region1_size;
    }

    CopyMemory(audio.frame_buffer,
        (uint8_t*)audio.ring_buffer + local_read_offset, read_region1_size);
    CopyMemory(audio.frame_buffer + read_region1_size, (uint8_t*)audio.ring_buffer,
        read_region2_size);

    audio.rb_read_offset += (read_region1_size + read_region2_size);

    return read_region1_size + read_region2_size;
}

void
win32_audio_lock_buffer(Win32Audio &audio, GameSoundOutput &sound_output, uint32_t bytes_to_write)
{
    uint32_t rb_backlog = audio.rb_write_offset - audio.rb_read_offset;
    uint32_t rb_free_space = audio.rb_capacity - rb_backlog;
    unsigned char *buffer = (unsigned char*)audio.ring_buffer;
    uint32_t write_region1_size = bytes_to_write;
    uint32_t write_region2_size = 0; // if bytes_to_write + rb_write_offset > rb_size then we have to circle back
    // another way to clamp but capacity needs to be pow 2
    uint32_t local_write_offset = audio.rb_write_offset & (audio.rb_capacity - 1);

    // This might not be needed anymore since we only lock once we go below the threshold
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

    audio.rb_write_offset += (write_region1_size + write_region2_size);
}
