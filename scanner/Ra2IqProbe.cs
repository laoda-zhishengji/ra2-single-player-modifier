using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;

class Ra2IqProbe
{
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h, IntPtr address, byte[] buffer, int size, out IntPtr read);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualQueryEx(IntPtr h, IntPtr address, out Mbi mbi, uint size);
    [StructLayout(LayoutKind.Sequential)] struct Mbi
    {
        public IntPtr BaseAddress, AllocationBase;
        public uint AllocationProtect, RegionSize, State, Protect, Type;
    }
    const uint VMRead = 0x0010, Query = 0x0400, Commit = 0x1000, Guard = 0x100;
    static bool Readable(uint p) { uint x=p&0xff; return x==2||x==4||x==8||x==0x20||x==0x40||x==0x80; }
    static void Main()
    {
        Process target=null;
        foreach (var p in Process.GetProcessesByName("game"))
            try { if (String.Equals(p.MainModule.FileName, @"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe", StringComparison.OrdinalIgnoreCase)) { target=p; break; } } catch {}
        if (target==null) { Console.WriteLine("未找到匹配路径的 Steam game.exe。"); return; }
        IntPtr h=OpenProcess(VMRead|Query,false,target.Id); if(h==IntPtr.Zero) { Console.WriteLine("OPEN_FAILED"); return; }
        IntPtr cur=IntPtr.Zero; int hits=0;
        Mbi m;
        while(VirtualQueryEx(h,cur,out m,(uint)Marshal.SizeOf(typeof(Mbi)))!=IntPtr.Zero)
        {
            long baseAddr=m.BaseAddress.ToInt64(); long region=m.RegionSize;
            if(m.State==Commit && (m.Protect&Guard)==0 && Readable(m.Protect) && region>0)
            {
                for(long off=0; off<region; off+=65536)
                {
                    int size=(int)Math.Min(65536,region-off); byte[] b=new byte[size]; IntPtr got;
                    if(!ReadProcessMemory(h,new IntPtr(baseAddr+off),b,size,out got)) continue;
                    int n=(int)got.ToInt64();
                    for(int i=0;i+44<=n;i+=4)
                    {
                        int[] v=new int[11]; for(int j=0;j<11;j++) v[j]=BitConverter.ToInt32(b,i+j*4);
                        if(v[0]!=5||v[1]!=4||v[2]!=5||v[3]!=2) continue;
                        Console.WriteLine("IQ_PREFIX addr=0x{0:X8} values={1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11} regionProtect=0x{12:X}",baseAddr+off+i,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],m.Protect); hits++;
                        if(hits>=32) return;
                    }
                }
            }
            long next=baseAddr+region; if(next<=cur.ToInt64()) break; cur=new IntPtr(next);
        }
        Console.WriteLine("IQ_CANDIDATES={0}",hits);
    }
}
