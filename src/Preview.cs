using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

[ComImport, Guid("00000001-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IClassFactory { [PreserveSig] int CreateInstance(IntPtr outer, ref Guid iid, out IntPtr obj); [PreserveSig] int LockServer(bool value); }
[ComImport, Guid("b824b49d-22ac-4161-ac8a-9916e8fa3f7f"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IInitializeWithStream { void Initialize(IStream stream, uint mode); }
[ComImport, Guid("e357fccd-a995-4576-b01f-234630154e96"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IThumbnailProvider { void GetThumbnail(uint size, out IntPtr bitmap, out uint alpha); }

static class Native {
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
    [DllImport("kernel32", CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("gdi32")] public static extern bool DeleteObject(IntPtr obj);
    [DllImport("shlwapi", CharSet=CharSet.Unicode, PreserveSig=true)] public static extern int SHCreateStreamOnFileEx(string path, uint mode, uint attributes, bool create, IntPtr reserved, out IStream stream);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int Factory(ref Guid clsid, ref Guid iid, out IntPtr result);
    public static void Extract(string dll, string file, string output, bool bmp) {
        IntPtr module=LoadLibraryEx(Path.GetFullPath(dll), IntPtr.Zero, 0x8);
        if(module==IntPtr.Zero) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "Cannot load the Cinema 4D thumbnail component.");
        IntPtr entry=GetProcAddress(module,"DllGetClassObject");
        if(entry==IntPtr.Zero) throw new Exception("Thumbnail component has no class factory.");
        Guid clsid=new Guid("2BAA7283-B8CA-4993-91F7-CE75B780E1F0"), factoryIid=typeof(IClassFactory).GUID, unknown=new Guid("00000000-0000-0000-C000-000000000046");
        IntPtr factoryPtr=IntPtr.Zero, objectPtr=IntPtr.Zero, bitmap=IntPtr.Zero;
        object provider=null; IClassFactory factory=null; IStream stream=null;
        try {
            var get=(Factory)Marshal.GetDelegateForFunctionPointer(entry,typeof(Factory));
            Marshal.ThrowExceptionForHR(get(ref clsid,ref factoryIid,out factoryPtr));
            factory=(IClassFactory)Marshal.GetObjectForIUnknown(factoryPtr);
            Marshal.ThrowExceptionForHR(factory.CreateInstance(IntPtr.Zero,ref unknown,out objectPtr));
            provider=Marshal.GetObjectForIUnknown(objectPtr);
            Marshal.ThrowExceptionForHR(SHCreateStreamOnFileEx(file,0x20,0,false,IntPtr.Zero,out stream));
            ((IInitializeWithStream)provider).Initialize(stream,0);
            uint alpha; ((IThumbnailProvider)provider).GetThumbnail(512,out bitmap,out alpha);
            if(bitmap==IntPtr.Zero) throw new Exception("This scene has no readable saved preview.");
            using(var img=Image.FromHbitmap(bitmap)) img.Save(output,bmp?ImageFormat.Bmp:ImageFormat.Png);
        } finally {
            if(bitmap!=IntPtr.Zero) DeleteObject(bitmap);
            if(provider!=null) Marshal.ReleaseComObject(provider);
            if(stream!=null) Marshal.ReleaseComObject(stream);
            if(factory!=null) Marshal.ReleaseComObject(factory);
            if(objectPtr!=IntPtr.Zero) Marshal.Release(objectPtr);
            if(factoryPtr!=IntPtr.Zero) Marshal.Release(factoryPtr);
        }
    }
}

class PreviewWindow : Form {
    Image preview; float zoom=1; PointF offset; Point last; bool dragging;
    readonly Label status=new Label(); readonly Panel canvas=new Panel();
    readonly string file;
    public PreviewWindow(string path) {
        file=Path.GetFullPath(path); Text="C4D 快速预览 · "+Path.GetFileName(file);
        Width=1040; Height=760; MinimumSize=new Size(620,420); StartPosition=FormStartPosition.CenterScreen;
        BackColor=Color.FromArgb(25,28,34); ForeColor=Color.White; Font=new Font("Microsoft YaHei UI",10);
        var tools=new FlowLayoutPanel { Dock=DockStyle.Top,Height=48,Padding=new Padding(10,7,0,0) };
        var reset=new Button {Text="适合窗口",AutoSize=true}; reset.Click+=(s,e)=>{zoom=1;offset=PointF.Empty;canvas.Invalidate();};
        tools.Controls.Add(reset);
        tools.Controls.Add(new Label {Text="已保存的场景图片 · 滚轮缩放 · 拖动平移（非三维旋转）",AutoSize=true,Margin=new Padding(18,6,0,0)});
        status.Dock=DockStyle.Bottom;status.Height=38;status.Padding=new Padding(12,7,0,0);status.Text="正在读取预览…";
        canvas.Dock=DockStyle.Fill;canvas.Paint+=PaintPreview;
        canvas.MouseWheel+=(s,e)=>{zoom=Math.Max(.1f,Math.Min(12f,zoom*(e.Delta>0?1.15f:1/1.15f)));canvas.Invalidate();};
        canvas.MouseDown+=(s,e)=>{if(e.Button==MouseButtons.Left){dragging=true;last=e.Location;canvas.Capture=true;}};
        canvas.MouseMove+=(s,e)=>{if(dragging){offset.X+=e.X-last.X;offset.Y+=e.Y-last.Y;last=e.Location;canvas.Invalidate();}};
        canvas.MouseUp+=(s,e)=>{dragging=false;canvas.Capture=false;}; canvas.MouseEnter+=(s,e)=>canvas.Focus();
        canvas.Resize+=(s,e)=>canvas.Invalidate();
        Controls.Add(canvas);Controls.Add(tools);Controls.Add(status);
        Shown+=async(s,e)=>{ try {var cached=await Task.Run(()=>Generate(file)); if(IsDisposed)return; using(var input=Image.FromFile(cached))preview=new Bitmap(input);status.Text=Path.GetFileName(file)+" · 保存时的预览；清晰度取决于原文件";canvas.Invalidate();}catch(Exception ex){if(!IsDisposed)status.Text="无法预览："+ex.Message;} };
        FormClosed+=(s,e)=>{if(preview!=null)preview.Dispose();};
    }
    void PaintPreview(object sender,PaintEventArgs e) {
        if(preview!=null){float fit=Math.Min((canvas.Width-40f)/preview.Width,(canvas.Height-40f)/preview.Height)*zoom;float w=preview.Width*fit,h=preview.Height*fit;e.Graphics.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(preview,(canvas.Width-w)/2+offset.X,(canvas.Height-h)/2+offset.Y,w,h);}
        using(var b=new SolidBrush(Color.FromArgb(45,95,185))) e.Graphics.FillRectangle(b,12,canvas.Height-36,45,23);
        e.Graphics.DrawString("C4D",Font,Brushes.White,15,canvas.Height-34);
    }
    static string Quote(string value) { return "\""+value.Replace("\"", "")+"\""; }
    static string Generate(string file) {
        if(!File.Exists(file))throw new Exception("文件不存在。");
        if(!String.Equals(Path.GetExtension(file),".c4d",StringComparison.OrdinalIgnoreCase))throw new Exception("此试用版仅支持 .c4d。");
        string dll=File.ReadAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"backend.txt")).Trim();
        if(!File.Exists(dll))throw new Exception("C4D 缩略图组件已移动，请重新安装本工具。");
        var info=new FileInfo(file);var backend=new FileInfo(dll);string key;
        using(var hash=SHA256.Create())key=BitConverter.ToString(hash.ComputeHash(Encoding.UTF8.GetBytes(file+"|"+info.Length+"|"+info.LastWriteTimeUtc.Ticks+"|"+dll+"|"+backend.LastWriteTimeUtc.Ticks))).Replace("-", "");
        string dir=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"C4DQuickPreview","Cache");Directory.CreateDirectory(dir);
        string target=Path.Combine(dir,key+".png");if(File.Exists(target))return target;
        string temporary=Path.Combine(dir,Guid.NewGuid().ToString("N")+".png");
        try {
            var start=new ProcessStartInfo(Application.ExecutablePath,"--extract "+Quote(dll)+" "+Quote(file)+" "+Quote(temporary)){UseShellExecute=false,CreateNoWindow=true};
            using(var child=Process.Start(start)) {
                if(!child.WaitForExit(12000)){try{child.Kill();child.WaitForExit(2000);}catch{}throw new Exception("读取超过 12 秒，已停止。可先在 C4D 保存场景预览。");}
                if(child.ExitCode!=0||!File.Exists(temporary))throw new Exception("文件缺少可读预览，或当前 C4D 组件不兼容。请在 C4D 中打开并保存后重试。");
            }
            if(!File.Exists(target)){try{File.Move(temporary,target);}catch(IOException){if(!File.Exists(target))throw;}}
            return target;
        } finally { if(File.Exists(temporary))File.Delete(temporary); }
    }
}
static class Program {
    [STAThread] static int Main(string[] args) {
        if(args.Length==4 && (args[0]=="--extract" || args[0]=="--extract-bmp")) {try{Native.Extract(args[1],args[2],args[3],args[0]=="--extract-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[3]+".error.txt",ex.ToString());return 1;}}
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        if(args.Length!=1){MessageBox.Show("请右键 .c4d 文件，选择“C4D 快速预览”。\n此版本显示保存的场景图片，不支持三维旋转。","C4D 快速预览");return 0;}
        try{Application.Run(new PreviewWindow(args[0]));return 0;}catch(Exception ex){MessageBox.Show(ex.Message,"C4D 快速预览");return 1;}
    }
}
