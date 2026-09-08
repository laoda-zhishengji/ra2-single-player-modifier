#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>

static BYTE g_original[7] = {0x89,0x06,0x85,0xC0,0x0F,0x9E,0xC1};
static BYTE *g_patch = NULL;
static BYTE *g_cave = NULL;
static volatile LONG g_installed = 0;

static void log_line(const wchar_t *event_name)
{
    wchar_t temp[MAX_PATH], path[MAX_PATH], line[256]; DWORD n=GetTempPathW(MAX_PATH,temp);
    if(!n||n>=MAX_PATH||FAILED(StringCchPrintfW(path,MAX_PATH,L"%sra2_modifier_money.log",temp)))return;
    HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return; SYSTEMTIME t;GetLocalTime(&t);
    int c=_snwprintf_s(line,256,_TRUNCATE,L"%04u-%02u-%02u %02u:%02u:%02u event=%s pid=%lu\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,event_name,GetCurrentProcessId());
    if(c>0){DWORD w;WriteFile(f,line,(DWORD)(c*sizeof(wchar_t)),&w,NULL);} CloseHandle(f);
}
static void put_rel_jmp(BYTE *at, BYTE *to){at[0]=0xE9;*(INT32 *)(at+1)=(INT32)(to-(at+5));}
static BOOL install_hook(void)
{
    HMODULE game=GetModuleHandleW(L"game.exe"); if(!game)return FALSE;
    g_patch=(BYTE *)game+0x94670;
    if(memcmp(g_patch,g_original,sizeof(g_original))!=0){log_line(L"money_hook_signature_mismatch");return FALSE;}
    g_cave=(BYTE *)VirtualAlloc(NULL,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE); if(!g_cave)return FALSE;
    BYTE code[]={0x3B,0x06,0x7D,0x02,0x8B,0x06,0x89,0x06,0x85,0xC0,0x0F,0x9E,0xC1,0xE9,0,0,0,0};
    memcpy(g_cave,code,sizeof(code));
    put_rel_jmp(g_cave+13,g_patch+7);
    DWORD old; if(!VirtualProtect(g_patch,7,PAGE_EXECUTE_READWRITE,&old))return FALSE;
    BYTE patch[7]; memset(patch,0x90,sizeof(patch)); put_rel_jmp(patch,g_cave); memcpy(g_patch,patch,7);
    FlushInstructionCache(GetCurrentProcess(),g_patch,7); VirtualProtect(g_patch,7,old,&old);
    InterlockedExchange(&g_installed,1); log_line(L"money_hook_installed"); return TRUE;
}
static void remove_hook(void)
{
    if(!g_patch||!g_installed)return; DWORD old;
    if(VirtualProtect(g_patch,7,PAGE_EXECUTE_READWRITE,&old)){memcpy(g_patch,g_original,7);FlushInstructionCache(GetCurrentProcess(),g_patch,7);VirtualProtect(g_patch,7,old,&old);}
    if(g_cave)VirtualFree(g_cave,0,MEM_RELEASE); InterlockedExchange(&g_installed,0); log_line(L"money_hook_removed");
}
static DWORD WINAPI init_thread(void *unused){(void)unused;Sleep(500);install_hook();return 0;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved){(void)instance;(void)reserved;if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);HANDLE t=CreateThread(NULL,0,init_thread,NULL,0,NULL);if(t)CloseHandle(t);}else if(reason==DLL_PROCESS_DETACH)remove_hook();return TRUE;}
