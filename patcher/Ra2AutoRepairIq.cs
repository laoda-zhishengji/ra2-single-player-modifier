using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

class Ra2AutoRepairIq
{
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint a,bool b,int p);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h,IntPtr a,byte[] b,int n,out IntPtr r);
    [DllImport("kernel32.dll")] static extern bool WriteProcessMemory(IntPtr h,IntPtr a,byte[] b,int n,out IntPtr w);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualQueryEx(IntPtr h,IntPtr a,out Mbi m,uint n);
    [StructLayout(LayoutKind.Sequential)] struct Mbi { public IntPtr BaseAddress,AllocationBase; public uint AllocationProtect,RegionSize,State,Protect,Type; }
    const uint Read=0x10, Write=0x20, Operate=0x8, Query=0x400, Commit=0x1000, Guard=0x100;
    static bool R(uint p){uint x=p&255;return x==2||x==4||x==8||x==32||x==64||x==128;}
    static Process Game(){foreach(var p in Process.GetProcessesByName("game"))try{if(String.Equals(p.MainModule.FileName,@"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe",StringComparison.OrdinalIgnoreCase))return p;}catch{}return null;}
    static IntPtr Find(IntPtr h,out int old){old=-1;IntPtr cur=IntPtr.Zero;Mbi m;while(VirtualQueryEx(h,cur,out m,(uint)Marshal.SizeOf(typeof(Mbi)))!=IntPtr.Zero){long ba=m.BaseAddress.ToInt64(),sz=m.RegionSize;if(m.State==Commit&&(m.Protect&Guard)==0&&R(m.Protect))for(long off=0;off<sz;off+=65536){int n=(int)Math.Min(65536,sz-off);byte[] b=new byte[n];IntPtr got;if(!ReadProcessMemory(h,new IntPtr(ba+off),b,n,out got))continue;for(int i=0;i+44<got.ToInt64();i+=4){int[]v=new int[11];for(int j=0;j<11;j++)v[j]=BitConverter.ToInt32(b,i+j*4);if(v[0]==5&&v[1]==4&&v[2]==5&&v[3]==2&&v[4]>=0&&v[4]<=5&&v[5]==2&&v[6]==2&&v[7]==3&&v[8]==3&&v[9]==2&&v[10]==2){old=v[4];return new IntPtr(ba+off+i+16);}}}long nx=ba+sz;if(nx<=cur.ToInt64())return IntPtr.Zero;cur=new IntPtr(nx);}return IntPtr.Zero;}
    static void Main(string[] args){bool off=args.Length==1&&args[0].Equals("--off",StringComparison.OrdinalIgnoreCase);if(args.Length>1||(args.Length==1&&!off)){Console.WriteLine("Usage: ra2_auto_repair_iq.exe [--off]");return;}var g=Game();if(g==null){Console.WriteLine("未找到匹配路径的 Steam game.exe。");return;}IntPtr h=OpenProcess(Read|Write|Operate|Query,false,g.Id);int old;IntPtr a=Find(h,out old);if(a==IntPtr.Zero){Console.WriteLine("IQ_RULES_NOT_FOUND");return;}int want=off?1:0;byte[] data=BitConverter.GetBytes(want);IntPtr w;if(WriteProcessMemory(h,a,data,4,out w)&&w.ToInt64()==4)Console.WriteLine("AUTO_REPAIR_IQ_{0} addr=0x{1:X8} old={2} new={3}",off?"OFF":"ON",a.ToInt64(),old,want);else Console.WriteLine("WRITE_FAILED");}
}
