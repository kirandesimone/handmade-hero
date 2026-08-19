#include "win32_handmade.h"

// Globals
static bool g_running;
static Win32Buffer g_back_buffer;
static Win32Audio g_audio;
static LARGE_INTEGER g_performance_freq;


void*
DEBUGplatform_read_entire_file(const char *filename)
{
    void *result = nullptr;
    LARGE_INTEGER file_size {};
    HANDLE file_handle = CreateFile(filename, GENERIC_READ, 0,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file_handle) {
        if(GetFileSizeEx(file_handle, &file_size)) {
            ASSERT(file_size.QuadPart < MAX_UINT32)
            unsigned long trunc_file_size = static_cast<unsigned long>(file_size.QuadPart);
            result = VirtualAlloc(NULL, trunc_file_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (result) {
                unsigned long bytes_read {};
                if (ReadFile(file_handle, result, trunc_file_size, &bytes_read, NULL) &&
                    trunc_file_size == bytes_read)
                {} else {
                    DEBUGplatform_free_file(result);
                    result = nullptr;
                }
            }
        }
    }

    CloseHandle(file_handle);

    return result;
}

void
DEBUGplatform_free_file(void *memory)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

void
win32_process_keyboard_event(GameButtonState &button, bool is_down)
{
    button.ended_down = is_down;
    ++button.half_transition_state;
}

static void
win32_debug_draw_audio_frame(uint32_t frame_pixel_col, int32_t top, int32_t bottom)
{
    uint8_t *pixel_addr = ((uint8_t*)g_back_buffer.bitmap_mem + // base
        (frame_pixel_col * g_back_buffer.bytes_per_pixel) + // column
        (top * g_back_buffer.bitmap_pitch)); // row

    for (int32_t pixel_index {top}; pixel_index < bottom; ++pixel_index) {
        uint32_t *pixel = (uint32_t*)pixel_addr;
        *pixel = 0xFFFFFFFF;
        pixel_addr += g_back_buffer.bitmap_pitch;
    }
}

static void
win32_debug_display_audio(uint32_t *play_cursors, uint32_t play_cursors_count,
    GameSoundOutput &sound_output, float target_seconds_per_frame)
{
    uint32_t ypad = 16;
    uint32_t xpad = 16;

    uint32_t top = ypad;
    uint32_t bottom = g_back_buffer.bitmap_height - ypad;
    // audio frames to pixels ratio to map into back_buffer
    float coefficient = (float)g_back_buffer.bitmap_width / g_audio.buffer_frame_capacity;
    for (uint32_t play_cursor_index {};
        play_cursor_index < play_cursors_count;
        ++play_cursor_index)
    {
        uint32_t frame_pixel_col= (uint32_t)(coefficient * play_cursors[play_cursor_index]);
        win32_debug_draw_audio_frame(frame_pixel_col, top, bottom);
    }
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
      ASSERT(!"INPUT CAME FROM SOMEWHERE ELSE")
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
    WNDCLASS window_class {};
    window_class.style = CS_HREDRAW | CS_VREDRAW; // Repaint the whole window instead of just the new section
    window_class.lpfnWndProc = win32_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "HandmadeWindowClass";
    win32_resize_DIB_section(g_back_buffer, 1280, 720);

    QueryPerformanceFrequency(&g_performance_freq);

    constexpr uint32_t hns_wasapi_buffer_duration = 100000;
    // TODO: Need to query monitor refresh rate through Windows
    constexpr uint32_t monitor_refresh_hz = 60;
    constexpr uint32_t game_refresh_hz = monitor_refresh_hz / 2;
    constexpr float target_seconds_per_frame = 1.0f / game_refresh_hz;

    if (RegisterClass(&window_class)) {
        HWND window_handle = CreateWindowEx(
            0, window_class.lpszClassName, "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

        if (window_handle) {
            win32_init_wasapi(g_audio, 0, hns_wasapi_buffer_duration);
            g_audio.client->Start();
            g_running = true;
            uint32_t debug_play_cursor_index = 0;
            uint32_t debug_play_cursors[15] {};

            // NOTE: Might have to move if we ever allow audio endpoint to change
            GameSoundOutput sound_output {};
            sound_output.channel_count = static_cast<uint8_t>(g_audio.wave_fmt->nChannels);
            sound_output.tone_hz = 256;
            sound_output.running_frame_index = 0;
            sound_output.volume = 0.3f;
            sound_output.samples_per_sec = g_audio.wave_fmt->nSamplesPerSec;
            sound_output.frame_size = g_audio.wave_fmt->nBlockAlign;

            uint32_t padding {};
            uint32_t available_frames {};
            uint32_t target_audio_frame_bytes_per_frame = static_cast<uint32_t>((target_seconds_per_frame *
                    g_audio.wave_fmt->nSamplesPerSec) * g_audio.wave_fmt->nBlockAlign);
            g_audio.rb_backlog_threshold = target_audio_frame_bytes_per_frame * 2;
            // Memory allocation
            void *starting_address = NULL;
#ifdef BUILD_INTERNAL
            // we always want the memory to start here for dev builds
            starting_address = reinterpret_cast<void *>(TEBIBYTES(2));
#endif
            constexpr uint64_t persistent_mem_size = MEBIBYTES(64);
            constexpr uint64_t transient_mem_size = GIBIBYTES(4);
            constexpr uint64_t total_mem_size = persistent_mem_size + transient_mem_size;

            GameMemory memory {};
            memory.persistent_storage_size = persistent_mem_size;
            memory.transient_storage_size = transient_mem_size;
            memory.persistent_storage = VirtualAlloc(starting_address, total_mem_size,
                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            memory.transient_storage = reinterpret_cast<uint8_t*>(memory.persistent_storage) +
                memory.persistent_storage_size;

            if (!memory.persistent_storage || !memory.transient_storage) {
                return 0 ;
            }

            GameInput input[2] {};
            GameInput *new_input = &input[0];
            GameInput *old_input = &input[1];

            // we can also add the cpu clock cycles with __rdtsc()
            LARGE_INTEGER last_counts;
            QueryPerformanceCounter(&last_counts);

            // 1 iteration = 1 frame
            while (g_running) {
                MSG msg;
                GameControllerInput *new_keyboard = &new_input->controllers[0];
                GameControllerInput *old_keyboard = &old_input->controllers[0];

                for (uint32_t button_i {}; button_i < ARRAY_SIZE(new_keyboard->Input.buttons_array); ++button_i) {
                    new_keyboard->Input.buttons_array[button_i].ended_down =
                        old_keyboard->Input.buttons_array[button_i].ended_down;
                }

                while (PeekMessage(&msg, window_handle, 0, 0, PM_REMOVE)) {
                    if (msg.message == WM_QUIT) {
                        g_running = false;
                    }

                    switch (msg.message) {
                    case WM_KEYDOWN:
                    case WM_KEYUP:
                    case WM_SYSKEYDOWN:
                    case WM_SYSKEYUP: {
                        uint64_t vk_code {msg.wParam};
                        bool prev_key_state {(msg.lParam & (1 << 30)) != 0};
                        bool is_key_down {(msg.lParam & (1 << 31)) == 0};

                        if (prev_key_state != is_key_down) {
                            if (vk_code == VK_LEFT) {
                            } else if (vk_code == VK_RIGHT) {
                            } else if (vk_code == VK_UP) {
                            } else if (vk_code == VK_DOWN) {
                            } else if (vk_code == 'W') {
                                win32_process_keyboard_event(new_keyboard->Input.Buttons.up, is_key_down);
                                OutputDebugStringA("Going UP");
                            } else if (vk_code == 'A') {
                                win32_process_keyboard_event(new_keyboard->Input.Buttons.left, is_key_down);
                            } else if (vk_code == 'S') {
                                win32_process_keyboard_event(new_keyboard->Input.Buttons.down, is_key_down);
                            } else if (vk_code == 'D') {
                                win32_process_keyboard_event(new_keyboard->Input.Buttons.right, is_key_down);
                            }
                        }
                    } break;
                    default:
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }

                // BUFFER for RENDERING
                BackgroundScreenBuffer buffer {};
                buffer.bitmap_mem = g_back_buffer.bitmap_mem;
                buffer.bitmap_height = g_back_buffer.bitmap_height;
                buffer.bitmap_width = g_back_buffer.bitmap_width;
                buffer.bytes_per_pixel = g_back_buffer.bytes_per_pixel;
                buffer.bitmap_pitch = g_back_buffer.bitmap_pitch;

                // AUDIO
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
                HDC dest_dc = GetDC(window_handle);

                // GAME INPUT SWITCH
                GameInput *temp = new_input;
                new_input = old_input;
                old_input = temp;

                // Performance tracking and Frame Rate Locking(Wall Clock or Real World Time)
                LARGE_INTEGER end_counts;
                QueryPerformanceCounter(&end_counts);

                int64_t elapsed_counts = end_counts.QuadPart - last_counts.QuadPart;
                float work_seconds_per_frame = ((float)elapsed_counts) / g_performance_freq.QuadPart;
                while (work_seconds_per_frame < target_seconds_per_frame) {
                    QueryPerformanceCounter(&end_counts);
                    elapsed_counts = end_counts.QuadPart - last_counts.QuadPart;
                    work_seconds_per_frame = ((float)elapsed_counts) / g_performance_freq.QuadPart;
                }

                QueryPerformanceCounter(&end_counts);
                last_counts = end_counts;

                Win32WinDimensions dimensions = win32_get_win_dimensions(window_handle);

#ifdef BUILD_INTERNAL
                win32_debug_display_audio(debug_play_cursors, ARRAY_SIZE(debug_play_cursors),
                    sound_output, target_seconds_per_frame);
#endif // BUILD_INTERNAL

                win32_display_buffer(dest_dc, g_back_buffer, dimensions.height, dimensions.width);
                ReleaseDC(window_handle, dest_dc);
                g_audio.render_client->ReleaseBuffer(frames_read, 0);

#ifdef BUILD_INTERNAL
                {
                    uint32_t play_cursor = ((sound_output.running_frame_index) - padding) %
                        g_audio.buffer_frame_capacity;
                    uint32_t write_cursor = (sound_output.running_frame_index) % g_audio.buffer_frame_capacity;
                    debug_play_cursors[debug_play_cursor_index++] = play_cursor;
                    debug_play_cursor_index = debug_play_cursor_index % ARRAY_SIZE(debug_play_cursors);
                }
#endif // BUILD_INTERNAL

                float msecs_per_frame = (1000.0f * elapsed_counts) / g_performance_freq.QuadPart;
                float fps = ((float)g_performance_freq.QuadPart) / elapsed_counts;
                char msecs_per_frame_buff[256];
                sprintf_s(msecs_per_frame_buff, "Milliseconds/frame: %.6f / %.6f FPS\n", msecs_per_frame, fps);
                OutputDebugStringA(msecs_per_frame_buff);

                char backlog_buff[256];
                sprintf_s(backlog_buff, "frame backlog: %d, available_frame: %d, padding: %d\n", rb_backlog, available_frames, padding);
                OutputDebugStringA(backlog_buff);



            }
        }
    }

    return 0;
}
