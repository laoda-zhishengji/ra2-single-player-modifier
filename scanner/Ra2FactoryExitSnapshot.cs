using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

// Read-only diagnostic. It never opens a process with VM_WRITE and never
// changes game memory. It samples validated Techno objects and reports
// object creation/removal between frames.
internal static class Ra2FactoryExitSnapshot
{
    const uint QueryRead = 0x0410;
    const uint PageGuard = 0x100;
    const uint Commit = 0x1000;
    const uint ReadOnly = 0x02, ReadWrite = 0x04, WriteCopy = 0x08;
    const uint ExecuteRead = 0x20, ExecuteReadWrite = 0x40, ExecuteWriteCopy = 0x80;
    const uint TypeVtable = 0x0079D1F8;
    const int TypeOffset = 0x418;
    // Current RA2 object pools use a fixed 0x608-byte slot alignment; the
    // observed live objects are all at slot remainder 0x180.
    const uint ObjectStride = 0x608, ObjectRemainder = 0x180;
    static bool InKnownObjectPool(ulong a)
    {
        return (a >= 0x1E600000 && a < 0x1EB00000) ||
               (a >= 0x20900000 && a < 0x20A20000) ||
               (a >= 0x20B00000 && a < 0x20B50000) ||
               (a >= 0x20E00000 && a < 0x20E60000);
    }

    [StructLayout(LayoutKind.Sequential)] struct Mbi
    {
        public IntPtr BaseAddress, AllocationBase;
        public uint AllocationProtect, RegionSize, State, Protect, Type;
    }
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualQueryEx(IntPtr h, IntPtr address, out Mbi mbi, uint length);
    [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h, IntPtr address, byte[] buffer, int size, out IntPtr read);

    sealed class Item
    {
        public uint Address, Type, Owner, Health;
        public uint Factory, F494, F498, F49C, F4A0, F4A4, F4A8;
        public string Id = "?";
        public bool IsFactoryCandidate
        {
            get
            {
                switch (Id)
                {
                    case "GAPILE": case "NAHAND": case "GAWEAP": case "NAWEAP":
                    case "GAYARD": case "NAYARD": case "GAAIRP": case "NAAIRP":
                    case "GAHPAD": case "NAPAD": case "GADEPT": case "NADEPT":
                        return true;
                    default: return false;
                }
            }
        }
        public string FactoryState()
        {
            return string.Format("factory=0x{0:X8} +494={1:X8} +498={2:X8} +49C={3:X8} +4A0={4:X8} +4A4={5:X8} +4A8={6:X8}",
                Factory, F494, F498, F49C, F4A0, F4A4, F4A8);
        }
        public override string ToString() { return string.Format("{0}@0x{1:X8} owner=0x{2:X8} hp={3} {4}", Id, Address, Owner, Health, IsFactoryCandidate ? FactoryState() : ""); }
    }

    static bool Read(IntPtr h, uint address, byte[] b)
    {
        IntPtr got;
        return ReadProcessMemory(h, new IntPtr(unchecked((int)address)), b, b.Length, out got) && got.ToInt32() == b.Length;
    }
    static uint U32(IntPtr h, uint address)
    {
        var b = new byte[4];
        return Read(h, address, b) ? BitConverter.ToUInt32(b, 0) : 0;
    }
    static bool Readable(uint p)
    {
        p &= 0xff;
        return p == ReadOnly || p == ReadWrite || p == WriteCopy ||
               p == ExecuteRead || p == ExecuteReadWrite || p == ExecuteWriteCopy;
    }
    static bool ValidId(string s)
    {
        if (s.Length < 3 || s.Length > 22) return false;
        foreach (char c in s)
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return false;
        return true;
    }
    static Dictionary<uint, Item> Scan(IntPtr h)
    {
        var result = new Dictionary<uint, Item>();
        IntPtr cursor = IntPtr.Zero;
        int mbiSize = Marshal.SizeOf(typeof(Mbi));
        Mbi mbi;
        while (VirtualQueryEx(h, cursor, out mbi, (uint)mbiSize) != IntPtr.Zero)
        {
            ulong regionBegin = (ulong)mbi.BaseAddress.ToInt64();
            ulong regionEnd = regionBegin + mbi.RegionSize;
            if (mbi.State == Commit && mbi.Type == 0x20000 && Readable(mbi.Protect) &&
                (mbi.Protect & PageGuard) == 0 && InKnownObjectPool(regionBegin))
            {
                ulong begin = regionBegin;
                ulong end = regionEnd;
                byte[] region = new byte[65536];
                for (ulong chunk = begin; chunk < end; chunk += (ulong)region.Length)
                {
                    int want = (int)Math.Min((ulong)region.Length, end - chunk);
                    IntPtr got;
                    if (!ReadProcessMemory(h, new IntPtr(unchecked((long)chunk)), region, want, out got) || got.ToInt32() < 4) continue;
                    int gotCount = got.ToInt32();
                    for (int off = 0; off + 4 <= gotCount; off += 4)
                    {
                        uint obj = unchecked((uint)(chunk + (ulong)off));
                        uint vt = BitConverter.ToUInt32(region, off);
                        if ((obj % ObjectStride) != ObjectRemainder || vt < 0x00400000 || vt >= 0x00800000 || obj > 0x7F000000) continue;
                        uint type = U32(h, unchecked(obj + TypeOffset));
                        if (type == 0 || U32(h, type) != TypeVtable) continue;
                        var idbuf = new byte[23];
                        if (!Read(h, unchecked(type + 0x24), idbuf)) continue;
                        int n = Array.IndexOf(idbuf, (byte)0);
                        if (n < 0) n = idbuf.Length;
                        string id = System.Text.Encoding.ASCII.GetString(idbuf, 0, n);
                        if (!ValidId(id) || result.ContainsKey(obj)) continue;
                        result[obj] = new Item {
                            Address = obj, Type = type,
                            Owner = U32(h, unchecked(obj + 0x1B4)),
                            Health = U32(h, unchecked(obj + 0x6C)), Id = id,
                            Factory = U32(h, unchecked(obj + 0x494)),
                            F494 = U32(h, unchecked(obj + 0x494)),
                            F498 = U32(h, unchecked(obj + 0x498)),
                            F49C = U32(h, unchecked(obj + 0x49C)),
                            F4A0 = U32(h, unchecked(obj + 0x4A0)),
                            F4A4 = U32(h, unchecked(obj + 0x4A4)),
                            F4A8 = U32(h, unchecked(obj + 0x4A8))
                        };
                    }
                }
            }
            ulong next = (ulong)mbi.BaseAddress.ToInt64() + mbi.RegionSize;
            if (next <= (ulong)cursor.ToInt64()) break;
            cursor = new IntPtr(unchecked((long)next));
        }
        return result;
    }
    static Process FindGame()
    {
        foreach (var p in Process.GetProcessesByName("game"))
            try { if (p.MainModule.FileName.Equals(@"C:\Program Files (x86)\Steam\steamapps\common\Command & Conquer Red Alert II\game.exe", StringComparison.OrdinalIgnoreCase)) return p; }
            catch { }
        return null;
    }
    public static void Main(string[] args)
    {
        bool waitForGame = args.Length > 0 && string.Equals(args[0], "--wait", StringComparison.OrdinalIgnoreCase);
        int sampleArg = waitForGame ? 1 : 0;
        int delayArg = waitForGame ? 2 : 1;
        int s, d;
        int samples = args.Length > sampleArg && int.TryParse(args[sampleArg], out s) ? Math.Max(1, Math.Min(s, 300)) : 120;
        int delay = args.Length > delayArg && int.TryParse(args[delayArg], out d) ? Math.Max(20, Math.Min(d, 2000)) : 100;
        var game = FindGame();
        while (game == null && waitForGame)
        {
            Console.WriteLine("WAITING_FOR_GAME");
            System.Threading.Thread.Sleep(500);
            game = FindGame();
        }
        if (game == null) { Console.WriteLine("GAME_NOT_FOUND"); return; }
        IntPtr h = OpenProcess(QueryRead, false, game.Id);
        if (h == IntPtr.Zero) { Console.WriteLine("READ_HANDLE_FAILED"); return; }
        try
        {
            var previous = new Dictionary<uint, Item>();
            Console.WriteLine(string.Format("READ_ONLY pid={0} samples={1} interval_ms={2}", game.Id, samples, delay));
            for (int i = 0; i < samples; ++i)
            {
                var now = Scan(h);
                foreach (var kv in now)
                    if (!previous.ContainsKey(kv.Key)) Console.WriteLine(string.Format("+ sample={0} {1}", i, kv.Value));
                foreach (var kv in now)
                {
                    Item old;
                    if (kv.Value.IsFactoryCandidate && previous.TryGetValue(kv.Key, out old) &&
                        (old.Factory != kv.Value.Factory || old.F498 != kv.Value.F498 || old.F49C != kv.Value.F49C ||
                         old.F4A0 != kv.Value.F4A0 || old.F4A4 != kv.Value.F4A4 || old.F4A8 != kv.Value.F4A8))
                        Console.WriteLine(string.Format("~ sample={0} {1} -> {2}", i, old.FactoryState(), kv.Value.FactoryState()));
                }
                foreach (var kv in previous)
                    if (!now.ContainsKey(kv.Key)) Console.WriteLine(string.Format("- sample={0} {1}", i, kv.Value));
                previous = now;
                System.Threading.Thread.Sleep(delay);
            }
            Console.WriteLine(string.Format("DONE live_objects={0}", previous.Count));
        }
        finally { CloseHandle(h); }
    }
}
