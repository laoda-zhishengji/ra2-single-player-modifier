using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
class Ra2ObjectDiff {
 [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint a,bool b,int p);
 [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h,IntPtr a,byte[] b,int n,out IntPtr r);
 static void Main(){Process g=null;foreach(var p in Process.GetProcessesByName("game"))try{if(p.MainModule.FileName.Equals(@"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe",StringComparison.OrdinalIgnoreCase)){g=p;break;}}catch{}if(g==null){Console.WriteLine("GAME_NOT_FOUND");return;}IntPtr h=OpenProcess(0x410,false,g.Id);uint[] a={0x1E8B9530,0x1E8B7100};for(int k=0;k<a.Length;k++){byte[] b=new byte[0x300];IntPtr n;if(!ReadProcessMemory(h,new IntPtr(a[k]+0x300),b,b.Length,out n)){Console.WriteLine("READ_FAILED");continue;}Console.WriteLine("OBJECT 0x{0:X8}",a[k]);for(int i=0;i+4<=b.Length;i+=4){uint v=BitConverter.ToUInt32(b,i);if(v!=0)Console.WriteLine("+0x{0:X3}=0x{1:X8} ({1})",0x300+i,v);}}}
}
