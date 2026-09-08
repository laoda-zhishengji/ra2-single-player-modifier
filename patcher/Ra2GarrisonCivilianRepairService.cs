using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

class Ra2GarrisonCivilianRepairService
{
    const string Path = @"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe";
    const uint Read=0x10, Write=0x20, Operate=0x8, Query=0x400, Commit=0x1000, Guard=0x100;
    const int TypeVtable=0x0079D1F8;
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint a,bool i,int p);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h,IntPtr a,byte[] b,int n,out IntPtr r);
    [DllImport("kernel32.dll")] static extern bool WriteProcessMemory(IntPtr h,IntPtr a,byte[] b,int n,out IntPtr w);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualQueryEx(IntPtr h,IntPtr a,out Mbi m,uint n);
    [StructLayout(LayoutKind.Sequential)] struct Mbi { public IntPtr BaseAddress,AllocationBase; public uint AllocationProtect,RegionSize,State,Protect,Type; }
    struct Obj { public long Address; public Obj(long a){Address=a;} }
    static bool Readable(uint p){uint x=p&255;return x==2||x==4||x==8||x==32||x==64||x==128;}
    static Process Game(){foreach(var p in Process.GetProcessesByName("game")){try{if(String.Equals(p.MainModule.FileName,Path,StringComparison.OrdinalIgnoreCase))return p;}catch{}}return null;}
    static byte[] ReadMem(IntPtr h,long a,int n){if(a<0||a>0x7FFFFFF0L)return null;byte[] b=new byte[n];IntPtr got;if(!ReadProcessMemory(h,new IntPtr(a),b,n,out got)||got.ToInt64()!=n)return null;return b;}
    static int I(byte[] b,int o){return BitConverter.ToInt32(b,o);}
    static string Ascii(byte[] b){int n=0;while(n<b.Length&&b[n]!=0)n++;return System.Text.Encoding.ASCII.GetString(b,0,n);}
    static bool IdOk(string s){if(s.Length<3||s.Length>22)return false;foreach(char c in s)if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
    static bool Civilian(string s){if(!s.StartsWith("CA",StringComparison.Ordinal))return false;if(s.Equals("CAUSFGL",StringComparison.OrdinalIgnoreCase)||s.StartsWith("CAPARK",StringComparison.OrdinalIgnoreCase)||s.StartsWith("CAMISC",StringComparison.OrdinalIgnoreCase)||s.StartsWith("CATEXS",StringComparison.OrdinalIgnoreCase))return false;return true;}
    static List<Obj> Scan(IntPtr h){var result=new List<Obj>();IntPtr cur=IntPtr.Zero;Mbi m;while(VirtualQueryEx(h,cur,out m,(uint)Marshal.SizeOf(typeof(Mbi)))!=IntPtr.Zero){long ba=m.BaseAddress.ToInt64(),sz=m.RegionSize;if(ba<0||sz<=0||ba>0x70000000L||sz>0x7FFFFFF0L-ba)break;if(m.State==Commit&&(m.Protect&Guard)==0&&Readable(m.Protect)){for(long off=0;off<sz;off+=65536){int n=(int)Math.Min(65536L,sz-off);byte[] b=new byte[n];IntPtr got;if(!ReadProcessMemory(h,new IntPtr(ba+off),b,n,out got))continue;int lim=(int)got.ToInt64()-4;for(int i=0;i<=lim;i+=4){int vt=I(b,i);if(vt<0x00400000||vt>=0x00800000)continue;long obj=ba+off+i;byte[] tp=ReadMem(h,obj+0x418,4);if(tp==null)continue;long type=(uint)I(tp,0);if(type<0x00400000L||type>0x70000000L)continue;byte[] tv=ReadMem(h,type,4);if(tv==null||I(tv,0)!=TypeVtable)continue;byte[] ib=ReadMem(h,type+0x24,23);if(ib==null)continue;string id=Ascii(ib);if(IdOk(id)&&Civilian(id)&&!result.Exists(x=>x.Address==obj))result.Add(new Obj(obj));}}}long nx=ba+sz;if(nx<=cur.ToInt64()||nx>0x7FFFFFF0)break;cur=new IntPtr(nx);}return result;}
    static void Pass(IntPtr h,long player,List<Obj> list){int changed=0;foreach(var x in list){long o=x.Address;byte[] tp=ReadMem(h,o+0x418,4);if(tp==null)continue;long type=(uint)I(tp,0);byte[] tv=ReadMem(h,type,4),ib=ReadMem(h,type+0x24,23),own=ReadMem(h,o+0x1B4,4),hp=ReadMem(h,o+0x6C,4),mx=ReadMem(h,type+0xA0,4),flag=ReadMem(h,o+0x5B8,1);if(tv==null||I(tv,0)!=TypeVtable||ib==null||own==null||hp==null||mx==null||flag==null)continue;string id=Ascii(ib);int owner=I(own,0),health=I(hp,0),max=I(mx,0);if(!IdOk(id)||!Civilian(id)||((uint)owner)!=(uint)player||health<=0||max<=0||health>=max||flag[0]!=0)continue;IntPtr w;if(WriteProcessMemory(h,new IntPtr(o+0x5B8),new byte[]{1},1,out w)&&w.ToInt64()==1&&((changed++%8)==0))Console.WriteLine("GARRISON_REPAIR_FLAG {0} {1}/{2} obj=0x{3:X8}",id,health,max,o);}}
    static void Main(){try{Console.CancelKeyPress+=(s,e)=>{e.Cancel=true;stop=true;};Console.WriteLine("GARRISON_CIVILIAN_REPAIR_SERVICE running; Ctrl+C to stop.");int last=0;List<Obj> list=new List<Obj>();while(!stop){var g=Game();if(g!=null&&g.Id!=last){last=g.Id;list.Clear();}if(g!=null){IntPtr h=OpenProcess(Read|Write|Operate|Query,false,g.Id);if(h!=IntPtr.Zero){byte[] pp=ReadMem(h,0x00A35DB4,4);if(pp!=null){long player=(uint)I(pp,0);if(player!=0){if(list.Count==0||Environment.TickCount-lastScan>250){list=Scan(h);lastScan=Environment.TickCount;Console.WriteLine("SCAN candidates={0}",list.Count);}Pass(h,player,list);}}CloseHandle(h);}}Thread.Sleep(30);}Console.WriteLine("GARRISON_CIVILIAN_REPAIR_SERVICE stopped.");}catch(Exception ex){Console.WriteLine("SERVICE_ERROR "+ex.GetType().FullName+" "+ex.Message+" "+ex.StackTrace);}}
    static volatile bool stop=false;static int lastScan=0;
}
