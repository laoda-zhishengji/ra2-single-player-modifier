#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include "ra2_product_manifest.h"

typedef struct { const wchar_t *exe; const wchar_t *event_name; } ServiceSpec;
static const ServiceSpec kAuto = { L"ra2_product_auto_repair.exe", L"Local\\RA2ProductAutoRepairStop" };
static const ServiceSpec kGarrison = { L"ra2_product_garrison_repair.exe", L"Local\\RA2ProductGarrisonRepairStop" };

static BOOL service_pid(const wchar_t *name, DWORD *out) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W e = { sizeof(e) }; BOOL found = FALSE;
    if (s == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(s, &e)) do { if (!_wcsicmp(e.szExeFile, name)) { if (out) *out = e.th32ProcessID; found = TRUE; break; } } while (Process32NextW(s, &e));
    CloseHandle(s); return found;
}

static BOOL game_present(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W e = { sizeof(e) }; BOOL found = FALSE;
    if (s == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(s, &e)) do { if (!_wcsicmp(e.szExeFile, L"game.exe")) { HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID); wchar_t path[MAX_PATH]; DWORD n = ARRAYSIZE(path); if (p && QueryFullProcessImageNameW(p, 0, path, &n) && !_wcsicmp(path, RA2_PRODUCT_GAME_PATH)) found = TRUE; if (p) CloseHandle(p); } } while (!found && Process32NextW(s, &e));
    CloseHandle(s); return found;
}

static void module_dir(wchar_t *out, DWORD cap) {
    DWORD n = GetModuleFileNameW(NULL, out, cap); if (!n || n >= cap) { out[0] = 0; return; }
    wchar_t *slash = wcsrchr(out, L'\\'); if (slash) *(slash + 1) = 0;
}

static int start_service(const ServiceSpec *spec) {
    if (!game_present()) { wprintf(L"GAME_NOT_FOUND\n"); return 3; }
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, spec->event_name); if (!ev) return 4; ResetEvent(ev);
    DWORD pid = 0; if (service_pid(spec->exe, &pid)) { CloseHandle(ev); wprintf(L"%ls=ON pid=%lu\n", spec->exe, pid); return 0; }
    wchar_t dir[MAX_PATH], cmd[MAX_PATH * 2]; module_dir(dir, ARRAYSIZE(dir)); if (!dir[0]) { CloseHandle(ev); return 5; }
    swprintf_s(cmd, ARRAYSIZE(cmd), L"\"%ls%ls\"", dir, spec->exe);
    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, dir, &si, &pi);
    if (ok) { wprintf(L"%ls=ON pid=%lu\n", spec->exe, pi.dwProcessId); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    else wprintf(L"%ls=START_FAILED error=%lu\n", spec->exe, GetLastError());
    CloseHandle(ev); return ok ? 0 : 6;
}

static int stop_service(const ServiceSpec *spec) {
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, spec->event_name); if (!ev) return 4; SetEvent(ev);
    int remaining = 0;
    for (int attempt = 0; attempt < 16; ++attempt) {
        DWORD pid = 0; if (!service_pid(spec->exe, &pid)) break;
        ++remaining;
        HANDLE p = NULL;
        for (int open_try = 0; open_try < 20 && !p; ++open_try) {
            p = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!p) Sleep(50);
        }
        if (!p) { Sleep(100); continue; }
        DWORD result = p ? WaitForSingleObject(p, 3000) : WAIT_FAILED;
        if (p) CloseHandle(p);
        if (result != WAIT_OBJECT_0) {
            /* The garrison scanner may be inside a large ReadProcessMemory
             * pass. If it does not leave promptly, stop only this helper so
             * the user-facing toggle still has deterministic off semantics. */
            BOOL killed = p && TerminateProcess(p, 0);
            WaitForSingleObject(p, 5000);
            CloseHandle(p);
            for (int settle = 0; settle < 100 && service_pid(spec->exe, NULL); ++settle) Sleep(50);
            if (service_pid(spec->exe, NULL)) {
                CloseHandle(ev);
                wprintf(L"%ls=STOP_TIMEOUT pid=%lu terminate=%ls error=%lu\n", spec->exe, pid, killed ? L"OK" : L"FAILED", killed ? 0UL : GetLastError());
                return 7;
            }
            CloseHandle(ev);
            wprintf(L"%ls=STOP_FORCED pid=%lu\n", spec->exe, pid);
            return 0;
        }
    }
    CloseHandle(ev);
    wprintf(L"%ls=OFF\n", spec->exe); return 0;
}

int wmain(int argc, wchar_t **argv) {
    BOOL status = argc == 1 || (argc == 2 && !_wcsicmp(argv[1], L"--status"));
    BOOL auto_on = argc == 2 && !_wcsicmp(argv[1], L"--auto-on");
    BOOL auto_off = argc == 2 && !_wcsicmp(argv[1], L"--auto-off");
    BOOL gar_on = argc == 2 && !_wcsicmp(argv[1], L"--garrison-on");
    BOOL gar_off = argc == 2 && !_wcsicmp(argv[1], L"--garrison-off");
    BOOL all_off = argc == 2 && !_wcsicmp(argv[1], L"--all-off");
    if (argc > 2 || (!status && !auto_on && !auto_off && !gar_on && !gar_off && !all_off)) { wprintf(L"Usage: ra2_product_services.exe [--status|--auto-on|--auto-off|--garrison-on|--garrison-off|--all-off]\n"); return 2; }
    if (status) { DWORD a = 0, g = 0; wprintf(L"auto_repair=%ls garrison_repair=%ls\n", service_pid(kAuto.exe, &a) ? L"ON" : L"OFF", service_pid(kGarrison.exe, &g) ? L"ON" : L"OFF"); return 0; }
    if (all_off) { int a = stop_service(&kAuto); int g = stop_service(&kGarrison); return a ? a : g; }
    if (auto_on) return start_service(&kAuto); if (auto_off) return stop_service(&kAuto); if (gar_on) return start_service(&kGarrison); return stop_service(&kGarrison);
}
