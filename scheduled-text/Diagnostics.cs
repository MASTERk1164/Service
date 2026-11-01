using System;
using System.IO;
using System.Text;
internal static class Diagnostics {
    internal static void Write(string message) {
        try {
            string folder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ScheduledText");
            Directory.CreateDirectory(folder);
            string path = Path.Combine(folder, "diagnostics.log");
            if (File.Exists(path) && new FileInfo(path).Length > 65536) File.WriteAllText(path, "", Encoding.UTF8);
            File.AppendAllText(path, DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + " " + message.Replace('\r',' ').Replace('\n',' ') + Environment.NewLine, Encoding.UTF8);
        } catch (Exception) { /* A log failure must never stop the scheduler. */ }
    }
}
