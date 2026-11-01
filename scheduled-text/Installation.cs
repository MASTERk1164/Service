using System;
using System.IO;
using System.Reflection;
using System.Text;
using System.Windows.Forms;
using Microsoft.Win32;

// Explicit /install action only: per-user install, shortcuts and uninstall registration.
internal static class Installation
{
    internal static void Install() {
        try {
            string source = Path.GetDirectoryName(Application.ExecutablePath);
            string destination = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ScheduledText");
            string programs = Environment.GetFolderPath(Environment.SpecialFolder.Programs);
            string desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            string exe = Path.Combine(destination, "ScheduledText.exe");
            Directory.CreateDirectory(destination);
            string[] files = { "ScheduledText.exe", "App.cs", "Schedule.cs", "Installation.cs", "Diagnostics.cs", "app.manifest", "Build.cmd", "Start.cmd", "Install.cmd", "README.txt", "SECURITY-REVIEW.txt", "SHA256SUMS.txt" };
            if (!String.Equals(source.TrimEnd('\\'), destination.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase)) {
                foreach (string name in files) File.Copy(Path.Combine(source, name), Path.Combine(destination, name), true);
            }
            string menuLink = Path.Combine(programs, "예약 문구 입력.lnk");
            string desktopLink = Path.Combine(desktop, "예약 문구 입력.lnk");
            Shortcut(menuLink, exe, destination); Shortcut(desktopLink, exe, destination);
            string uninstall = Path.Combine(destination, "Uninstall.cmd");
            // Delete a fixed allowlist only. Do not recursively remove user-created files.
            string script = "@echo off\r\nchcp 65001 >nul\r\nsetlocal\r\ncd /d \"%TEMP%\"\r\n";
            script += "del /q \"" + exe + "\" 2>nul\r\nif exist \"" + exe + "\" (\r\n echo Close ScheduledText before uninstalling.\r\n pause\r\n exit /b 1\r\n)\r\n";
            script += "reg delete \"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ScheduledText\" /f >nul 2>nul\r\n";
            script += "del /q \"" + menuLink + "\" \"" + desktopLink + "\" 2>nul\r\n";
            foreach (string name in files) if (name != "ScheduledText.exe") script += "del /q \"" + Path.Combine(destination, name) + "\" 2>nul\r\n";
            // cmd reads this final command group before deleting its own batch file.
            script += "(\r\n del /q \"" + uninstall + "\"\r\n rmdir \"" + destination + "\" 2>nul\r\n)\r\n";
            File.WriteAllText(uninstall, script, new UTF8Encoding(false));
            using (RegistryKey key = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ScheduledText")) {
                key.SetValue("DisplayName", "예약 문구 입력"); key.SetValue("DisplayVersion", "2.3");
                key.SetValue("InstallLocation", destination); key.SetValue("DisplayIcon", "\"" + exe + "\",0");
                string command = "\"" + Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System), "cmd.exe") + "\" /d /c \"\"" + uninstall + "\"\"";
                key.SetValue("UninstallString", command); key.SetValue("NoModify", 1, RegistryValueKind.DWord); key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
            }
            MessageBox.Show("설치 완료. 시작 메뉴 또는 바탕화면의 ‘예약 문구 입력’을 실행하세요.\\nWindows 설정의 설치된 앱에서 제거할 수 있습니다.".Replace("\\n", "\n"), "예약 문구 입력 설치");
        } catch (Exception ex) {
            MessageBox.Show("설치를 완료하지 못했습니다. 실행 중인 이전 버전을 종료한 뒤 다시 시도하세요.\n\n" + ex.Message, "설치 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
            Environment.ExitCode = 1;
        }
    }
    static void Shortcut(string path, string exe, string working) {
        Type type = Type.GetTypeFromProgID("WScript.Shell");
        object shell = Activator.CreateInstance(type), link = null;
        try {
            link = type.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell, new object[] { path });
            Type lt = link.GetType();
            lt.InvokeMember("TargetPath", BindingFlags.SetProperty, null, link, new object[] { exe });
            lt.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, link, new object[] { working });
            lt.InvokeMember("Save", BindingFlags.InvokeMethod, null, link, null);
        } finally {
            if (link != null) System.Runtime.InteropServices.Marshal.FinalReleaseComObject(link);
            System.Runtime.InteropServices.Marshal.FinalReleaseComObject(shell);
        }
    }
}
