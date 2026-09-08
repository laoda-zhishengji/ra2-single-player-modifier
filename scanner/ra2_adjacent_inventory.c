#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const wchar_t kPath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD find_target(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={sizeof(e)};DWORD r=0;if(s!=INVALID_HANDLE_VALUE&&Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(h&&QueryFullProcessImageNameW(h,0,q,&n)&&!_wcsicmp(q,kPath))r=e.th32ProcessID;if(h)CloseHandle(h);}}while(!r&&Process32NextW(s,&e));if(s!=INVALID_HANDLE_VALUE)CloseHandle(s);return r;}
static int readable(DWORD p){p&=0xff;return p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;}
static int valid_id(const char *s){int n=0;while(n<23&&s[n]){unsigned char c=(unsigned char)s[n];if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;n++;}return n>2&&n<23&&s[n]==0;}
int wmain(void){DWORD id=find_target();if(!id){wprintf(L"未找到 game.exe\n");return 2;}HANDLE h=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,id);if(!h)return 3;const DWORD vtable=0x0079D1F8;MEMORY_BASIC_INFORMATION m;uintptr_t cur=0;BYTE buf[65536];int hits=0;while(VirtualQueryEx(h,(void*)cur,&m,sizeof(m))==sizeof(m)){if(m.State==MEM_COMMIT&&readable(m.Protect)&&!(m.Protect&PAGE_GUARD)){SIZE_T off=0;while(off<m.RegionSize){SIZE_T want=m.RegionSize-off;if(want>sizeof(buf))want=sizeof(buf);SIZE_T got=0;if(ReadProcessMemory(h,(BYTE*)m.BaseAddress+off,buf,want,&got)&&got>=4){for(SIZE_T i=0;i+4<=got;i+=4){DWORD vt=0;memcpy(&vt,buf+i,4);if(vt!=vtable)continue;uintptr_t obj=(uintptr_t)m.BaseAddress+off+i;char name[24]={0};DWORD adj=0;SIZE_T x=0;if(!ReadProcessMemory(h,(void*)(obj+0x24),name,23,&x)||x<23||!valid_id(name))continue;if(!ReadProcessMemory(h,(void*)(obj+0xC40),&adj,4,&x)||adj>64)continue;wprintf(L"TYPE %-20S obj=%p Adjacent=%lu\n",name,(void*)obj,(unsigned long)adj);if(++hits>=256){CloseHandle(h);return 0;}}}off+=want;}}cur=(uintptr_t)m.BaseAddress+m.RegionSize;if(!cur)break;}wprintf(L"READ_ONLY types=%d\n",hits);CloseHandle(h);return 0;}
