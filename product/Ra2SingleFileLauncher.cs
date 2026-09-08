using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Threading;

internal static class Ra2SingleFileLauncher
{
    private static readonly string[] Files = new[]
    {
        "ra2_product_auto_repair.exe",
        "ra2_product_build_distance.exe",
        "ra2_product_control.exe",
        "ra2_product_fog.exe",
        "ra2_product_garrison_repair.exe",
        "ra2_product_hub.exe",
        "ra2_product_instant_service.exe",
        "ra2_product_production.exe",
        "ra2_product_services.exe",
        "ra2_product_status.exe",
        "ra2_product_super_service.exe",
        "ra2_product_super.exe",
        "ra2_product_ui.exe",
        "README.txt",
        "RELEASE_MANIFEST.txt",
        "assets/feature-icons.png",
        "assets/ra2-battlefield-background.png",
        "assets/ra2-hero-art.png",
        "assets/ra2-steam-hero.png",
        "assets/tactical-map.png"
    };

    [STAThread]
    private static int Main()
    {
        string root = Path.Combine(Path.GetTempPath(), "RA2SinglePlayerModifier", Guid.NewGuid().ToString("N"));
        try
        {
            Directory.CreateDirectory(root);
            foreach (string file in Files) Extract(file, root);

            string ui = Path.Combine(root, "ra2_product_ui.exe");
            Process child = Process.Start(new ProcessStartInfo(ui) { WorkingDirectory = root, UseShellExecute = false });
            if (child == null) throw new InvalidOperationException("无法启动修改器界面。");
            child.WaitForExit();
            return child.ExitCode;
        }
        catch (Exception ex)
        {
            try { File.WriteAllText(Path.Combine(Path.GetTempPath(), "RA2SingleFileLauncher-error.txt"), ex.ToString()); } catch { }
            System.Windows.Forms.MessageBox.Show("修改器启动失败：" + ex.Message, "红色警戒2修改器", System.Windows.Forms.MessageBoxButtons.OK, System.Windows.Forms.MessageBoxIcon.Error);
            return 1;
        }
        finally
        {
            for (int i = 0; i < 8; i++)
            {
                try { if (Directory.Exists(root)) Directory.Delete(root, true); break; }
                catch { Thread.Sleep(250); }
            }
        }
    }

    private static void Extract(string file, string root)
    {
        string resourceName = "payload." + file.Replace('/', '.');
        using (Stream input = Assembly.GetExecutingAssembly().GetManifestResourceStream(resourceName))
        {
            if (input == null) throw new FileNotFoundException("缺少内置组件：" + file);
            string destination = Path.Combine(root, file.Replace('/', Path.DirectorySeparatorChar));
            string directory = Path.GetDirectoryName(destination);
            if (!String.IsNullOrEmpty(directory)) Directory.CreateDirectory(directory);
            using (FileStream output = File.Create(destination)) input.CopyTo(output);
        }
    }
}
