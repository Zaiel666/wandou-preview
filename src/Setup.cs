using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows.Forms;

static class Setup {
    [STAThread] static int Main(string[] args) {
        bool quiet=args.Length==1&&args[0]=="--quiet";
        string temporary=Path.Combine(Path.GetTempPath(),"WandouPreviewSetup-"+Guid.NewGuid().ToString("N"));
        try {
            Directory.CreateDirectory(temporary);Assembly assembly=Assembly.GetExecutingAssembly();const string prefix="WandouPayload.";
            foreach(string resource in assembly.GetManifestResourceNames()){
                if(!resource.StartsWith(prefix,StringComparison.Ordinal))continue;string name=resource.Substring(prefix.Length);if(name.IndexOfAny(Path.GetInvalidFileNameChars())>=0)throw new Exception("安装资源名称无效。");
                using(Stream input=assembly.GetManifestResourceStream(resource))using(FileStream output=File.Create(Path.Combine(temporary,name)))input.CopyTo(output);
            }
            string script=Path.Combine(temporary,"Install.ps1");if(!File.Exists(script))throw new Exception("安装程序缺少 Install.ps1。");
            var start=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32","WindowsPowerShell","v1.0","powershell.exe"),"-NoProfile -ExecutionPolicy Bypass -File \""+script+"\""){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=temporary};
            using(Process process=Process.Start(start)){if(!process.WaitForExit(90000)){try{process.Kill();}catch{}throw new Exception("安装超过 90 秒，已停止。");}if(process.ExitCode!=0)throw new Exception("安装脚本返回错误代码 "+process.ExitCode+"。");}
            if(!quiet)MessageBox.Show("豌豆预览 0.2.0 已安装完成。\n\n打开模型或视频文件夹，切换到“大图标”或“超大图标”即可查看缩略图。","豌豆预览",MessageBoxButtons.OK,MessageBoxIcon.Information);return 0;
        } catch(Exception error){if(!quiet)MessageBox.Show("安装失败："+error.Message,"豌豆预览",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
        finally{try{if(Directory.Exists(temporary))Directory.Delete(temporary,true);}catch{}}
    }
}
