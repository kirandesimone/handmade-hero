#include "handmade.cpp"

#include <Windows.h>
#include <cstdint>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <Audioclient.h>
#include <winerror.h>


struct Win32Audio {
    IAudioClient *audio_client;
    IAudioRenderClient *render_client;
    WAVEFORMATEX *wave_fmt;
    uint32_t frame_capacity;
};

struct Win32Buffer {
    BITMAPINFO bitmap_info;
    void *bitmap_mem;
    int bitmap_height;
    int bitmap_width;
    int bitmap_pitch;
    int bytes_per_pixel;
};

struct Win32WinDimensions {
    int width;
    int height;
};

// Globals
static bool g_running;
static Win32Buffer g_back_buffer;
static Win32Audio g_audio;

// may want to set our own samples_per_sec and buffer size later
static void
win32_audio_init(uint32_t samples_per_sec_, uint32_t buffer_size)
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

        result = endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&g_audio.audio_client);
        endpoint->Release();

        result = g_audio.audio_client->GetMixFormat(&g_audio.wave_fmt);
        WAVEFORMATEXTENSIBLE *full_fmt = (WAVEFORMATEXTENSIBLE*)g_audio.wave_fmt;
        if (full_fmt->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            OutputDebugString("Float");
        }
        // looking for 480 samples/10msec
        g_audio.audio_client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            buffer_size, 0, g_audio.wave_fmt, NULL);

        result = g_audio.audio_client->GetBufferSize(&g_audio.frame_capacity);
        result = g_audio.audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&g_audio.render_client);
    }

    /*
    wave_fmt.wFormatTag = WAVE_FORMAT_PCM;
    wave_fmt.nChannels = 2;
    wave_fmt.wBitsPerSample = 16;
    wave_fmt.cbSize = 0;
    wave_fmt.nBlockAlign = (wave_fmt.wBitsPerSample * wave_fmt.nChannels) / 8;
    wave_fmt.nSamplesPerSec = samples_per_sec;
    wave_fmt.nAvgBytesPerSec = wave_fmt.nSamplesPerSec * wave_fmt.nBlockAlign;
    */
}

static Win32WinDimensions
win32_get_win_dimensions(HWND win_handle)
{
    Win32WinDimensions res {};
    RECT client_rect {};

    GetClientRect(win_handle, &client_rect);
    res.width = client_rect.right - client_rect.left;
    res.height = client_rect.bottom - client_rect.top;

    return res;
}

static void
win32_resize_DIB_section(Win32Buffer &buffer, int win_height, int win_width)
{
    if (buffer.bitmap_mem) {
        VirtualFree(buffer.bitmap_mem, 0, MEM_RELEASE);
    }

    buffer.bitmap_height = win_height;
    buffer.bitmap_width = win_width;
    buffer.bytes_per_pixel = 4;
    buffer.bitmap_pitch = buffer.bitmap_width * buffer.bytes_per_pixel;
    buffer.bitmap_info.bmiHeader.biSize = sizeof(buffer.bitmap_info.bmiHeader);
    buffer.bitmap_info.bmiHeader.biWidth = buffer.bitmap_width;
    buffer.bitmap_info.bmiHeader.biHeight = -buffer.bitmap_height; // Top-down
    buffer.bitmap_info.bmiHeader.biPlanes = 1;
    buffer.bitmap_info.bmiHeader.biBitCount = 32;
    buffer.bitmap_info.bmiHeader.biCompression = BI_RGB;

    int bitmap_size {buffer.bitmap_width * buffer.bitmap_height * buffer.bytes_per_pixel};
    buffer.bitmap_mem = VirtualAlloc(NULL, bitmap_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void
win32_display_buffer(HDC dest_device_context, const Win32Buffer &buffer, int win_height, int win_width)
{
    StretchDIBits(dest_device_context, 0, 0, win_width, win_height, 0, 0,
                buffer.bitmap_width, buffer.bitmap_height, buffer.bitmap_mem,
                &buffer.bitmap_info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT
win32_window_proc(HWND win_handle, UINT msg, WPARAM wparam, LPARAM lparam)
{
  LRESULT res {};

  switch (msg) {
  case WM_SIZE:
  {
  } break;
  case WM_DESTROY:
  {
  } break;
  case WM_CLOSE:
  {
    g_running = false;
  } break;
  case WM_ACTIVATEAPP:
  {

  } break;
  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP: {
    uint64_t vk_code {wparam};
    bool prev_key_state {(lparam & (1 << 30)) != 0};
    bool is_key_down {(lparam & (1 << 31)) == 0};

    if (vk_code == VK_LEFT) {

    } else if (vk_code == VK_RIGHT) {

    } else if (vk_code == VK_UP) {

    } else if (vk_code == VK_DOWN) {

    } else if (vk_code == 'w') {

    } else if (vk_code == 'a') {

    } else if (vk_code == 's') {

    } else if (vk_code == 'd') {

    }
  } break;
  case WM_PAINT:
  {
    PAINTSTRUCT paint_area {};
    HDC device_context = BeginPaint(win_handle, &paint_area);
    int width {paint_area.rcPaint.right - paint_area.rcPaint.left};
    int height {paint_area.rcPaint.bottom - paint_area.rcPaint.top};

    Win32WinDimensions dimensions {win32_get_win_dimensions(win_handle)};
    win32_display_buffer(device_context, g_back_buffer, height, width);
    EndPaint(win_handle, &paint_area);
  } break;
  default:
  {
    res = DefWindowProc(win_handle, msg, wparam, lparam);
  } break;
  }

  return res;
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR cmd_line, int cmd_show)
{
    LARGE_INTEGER performance_freq;
    QueryPerformanceFrequency(&performance_freq);

    WNDCLASS window_class {};
    window_class.style = CS_HREDRAW | CS_VREDRAW; // Repaint the whole window instead of just the new section
    window_class.lpfnWndProc = win32_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "HandmadeWindowClass";

    win32_resize_DIB_section(g_back_buffer, 1280, 720);

    if (RegisterClass(&window_class)) {
        HWND window_handle = CreateWindowEx(
            0, window_class.lpszClassName, "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

        if (window_handle) {
            win32_audio_init(0, 100000);
            g_audio.audio_client->Start();
            g_running = true;

            // NOTE: Might have to move if we ever allow audio endpoint to change
            uint8_t channel_count = g_audio.wave_fmt->nChannels;
            GameSoundOutput sound_output {};
            sound_output.channel_count = channel_count;
            sound_output.tone_hz = 256;
            sound_output.running_sample_index = 0;
            sound_output.volume = 0.3f;
            sound_output.samples_per_sec = g_audio.wave_fmt->nSamplesPerSec;

            // we can also add the cpu clock cycles with __rdtsc()
            LARGE_INTEGER last_ticks;
            QueryPerformanceCounter(&last_ticks);

            // 1 frame per iteration
            while (g_running) {
                MSG msg;
                while (PeekMessage(&msg, window_handle, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        g_running = false;
                    }
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

                uint32_t padding;
                g_audio.audio_client->GetCurrentPadding(&padding);

                uint32_t available_frames = g_audio.frame_capacity - padding;
                sound_output.available_frames = available_frames;
                g_audio.render_client->GetBuffer(available_frames, &sound_output.buffer);

                BackgroundScreenBuffer buffer {};
                buffer.bitmap_mem = g_back_buffer.bitmap_mem;
                buffer.bitmap_height = g_back_buffer.bitmap_height;
                buffer.bitmap_width = g_back_buffer.bitmap_width;
                buffer.bytes_per_pixel = g_back_buffer.bytes_per_pixel;
                buffer.bitmap_pitch = g_back_buffer.bitmap_pitch;

                game_update_and_render(sound_output, buffer, 0, 0);

                HDC dest_dc = GetDC(window_handle);
                Win32WinDimensions dimensions = win32_get_win_dimensions(window_handle);
                win32_display_buffer(dest_dc, g_back_buffer, dimensions.height, dimensions.width);
                ReleaseDC(window_handle, dest_dc);
                g_audio.render_client->ReleaseBuffer(available_frames, 0);

                // Performance tracking
                LARGE_INTEGER end_ticks;
                QueryPerformanceCounter(&end_ticks);

                int64_t elapsed_ticks = end_ticks.QuadPart - last_ticks.QuadPart;
                int32_t milsecs_per_frame = (1000 * elapsed_ticks) / performance_freq.QuadPart;
                int32_t fps = performance_freq.QuadPart / elapsed_ticks;
                last_ticks = end_ticks;

                char milsecs_per_frame_buff[256];
                wsprintf(milsecs_per_frame_buff, "Milliseconds/frame: %d / %dFPS\n", milsecs_per_frame, fps);
                OutputDebugStringA(milsecs_per_frame_buff);
            }
        }
    }

    return 0;
}
