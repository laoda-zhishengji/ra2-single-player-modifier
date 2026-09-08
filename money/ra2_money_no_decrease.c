#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>

static volatile LONG g_running = 1;
static HANDLE g_thread = NULL;
static const uintptr_t kMoneyOffsetA = 0x004373CC;
static const uintptr_t kMoneyOffsetB = 0x004373D0;

static void log_line(const wchar_t *event_name, LONG value)
{
    wchar_t temp[MAX_PATH], path[MAX_PATH], line[256];
    DWORD n = GetTempPathW(MAX_PATH, temp);
    if (!n || n >= MAX_PATH) return;
    if (FAILED(StringCchPrintfW(path, MAX_PATH, L"%sra2_modifier_money.log", temp))) return;
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    int count = _snwprintf_s(line, 256, _TRUNCATE,
        L"%04u-%02u-%02u %02u:%02u:%02u event=%s pid=%lu value=%ld\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        event_name, GetCurrentProcessId(), (long)value);
    if (count > 0) { DWORD written; WriteFile(file, line, (DWORD)(count * sizeof(wchar_t)), &written, NULL); }
    CloseHandle(file);
}

static DWORD WINAPI no_decrease_thread(void *unused)
{
    (void)unused;
    HMODULE game = GetModuleHandleW(L"game.exe");
    if (!game) return 1;
    volatile LONG *a = (volatile LONG *)((uintptr_t)game + kMoneyOffsetA);
    volatile LONG *b = (volatile LONG *)((uintptr_t)game + kMoneyOffsetB);
    LONG stable = 0;
    for (int i = 0; i < 20 && InterlockedCompareExchange(&g_running, 1, 1); ++i) {
        LONG va = *a, vb = *b;
        if (va == vb && va >= 0) { stable = va; break; }
        Sleep(50);
    }
    if (!stable) { log_line(L"money_value_not_ready", 0); return 2; }
    log_line(L"no_decrease_started", stable);
    while (InterlockedCompareExchange(&g_running, 1, 1)) {
        LONG va = *a, vb = *b;
        if (va == vb && va >= stable) {
            stable = va;
        } else if (va < stable || vb < stable) {
            *a = stable;
            *b = stable;
            log_line(L"expense_blocked", stable);
        }
        Sleep(50);
    }
    log_line(L"no_decrease_stopped", stable);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_thread = CreateThread(NULL, 0, no_decrease_thread, NULL, 0, NULL);
        if (!g_thread) return FALSE;
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_running, 0);
        if (g_thread) CloseHandle(g_thread);
    }
    return TRUE;
}
