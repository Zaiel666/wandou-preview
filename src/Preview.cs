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
[StructLayout(LayoutKind.Sequential)] struct ShellSize { public int Width; public int Height; public ShellSize(int value){Width=value;Height=value;} }
[ComImport, Guid("bcc18b79-ba16-442f-80c4-8a59c30c463b"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IShellItemImageFactory { [PreserveSig] int GetImage(ShellSize size, uint flags, out IntPtr bitmap); }

static class Native {
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
    [DllImport("kernel32", CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("gdi32")] public static extern bool DeleteObject(IntPtr obj);
    [DllImport("shlwapi", CharSet=CharSet.Unicode, PreserveSig=true)] public static extern int SHCreateStreamOnFileEx(string path, uint mode, uint attributes, bool create, IntPtr reserved, out IStream stream);
    [DllImport("shell32", CharSet=CharSet.Unicode, PreserveSig=true)] public static extern int SHCreateItemFromParsingName(string path, IntPtr context, ref Guid iid, [MarshalAs(UnmanagedType.Interface)] out IShellItemImageFactory item);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int Factory(ref Guid clsid, ref Guid iid, out IntPtr result);

    public static void ExtractC4D(string dll, string file, string output, bool bmp) {
        IntPtr module=LoadLibraryEx(Path.GetFullPath(dll), IntPtr.Zero, 0x8);
        if(module==IntPtr.Zero) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "无法加载 Cinema 4D 缩略图组件。");
        IntPtr entry=GetProcAddress(module,"DllGetClassObject");
        if(entry==IntPtr.Zero) throw new Exception("Cinema 4D 缩略图组件没有类工厂。");
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
            if(bitmap==IntPtr.Zero) throw new Exception("场景中没有可读取的保存预览图。");
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

    static long FindPdfHeader(string file) {
        byte[] needle=Encoding.ASCII.GetBytes("%PDF-");
        using(var input=File.OpenRead(file)) {
            long limit=Math.Min(input.Length,4L*1024*1024),baseOffset=0;byte[] block=new byte[65536];int carry=0;
            while(baseOffset<limit) {
                int read=input.Read(block,carry,(int)Math.Min(block.Length-carry,limit-baseOffset));if(read<=0)break;int total=carry+read;
                for(int i=0;i<=total-needle.Length;i++){bool match=true;for(int j=0;j<needle.Length;j++)if(block[i+j]!=needle[j]){match=false;break;}if(match)return baseOffset-carry+i;}
                carry=Math.Min(needle.Length-1,total);Buffer.BlockCopy(block,total-carry,block,0,carry);baseOffset+=read;
            }
        }
        return -1;
    }

    public static void ExtractAi(string file,string output,int size,bool bmp) {
        long offset=FindPdfHeader(file);
        if(offset<0)throw new Exception("这个 AI 文件没有保存 PDF 兼容数据，请在 Illustrator 中勾选“创建 PDF 兼容文件”后重新保存。");
        string pdf=Path.Combine(Path.GetTempPath(),"WandouPreview-"+Guid.NewGuid().ToString("N")+".pdf");
        IntPtr bitmap=IntPtr.Zero;IShellItemImageFactory factory=null;
        try {
            using(var input=File.OpenRead(file))using(var target=File.Create(pdf)){input.Position=offset;input.CopyTo(target);}
            Guid iid=typeof(IShellItemImageFactory).GUID;Marshal.ThrowExceptionForHR(SHCreateItemFromParsingName(pdf,IntPtr.Zero,ref iid,out factory));
            Marshal.ThrowExceptionForHR(factory.GetImage(new ShellSize(Math.Max(128,Math.Min(1024,size))),0x8,out bitmap));
            if(bitmap==IntPtr.Zero)throw new Exception("Windows 没有返回 AI/PDF 缩略图。");
            using(var image=Image.FromHbitmap(bitmap))image.Save(output,bmp?ImageFormat.Bmp:ImageFormat.Png);
        } finally {
            if(bitmap!=IntPtr.Zero)DeleteObject(bitmap);if(factory!=null)Marshal.ReleaseComObject(factory);try{if(File.Exists(pdf))File.Delete(pdf);}catch{}
        }
    }
}

class PreviewWindow : Form {
    Image preview;float zoom=1;PointF offset;Point last;bool dragging;
    readonly Label status=new Label();readonly Panel canvas=new Panel();readonly string file;readonly string badge;
    public PreviewWindow(string path) {
        file=Path.GetFullPath(path);badge=Path.GetExtension(file).TrimStart('.').ToUpperInvariant();Text="豌豆预览 · "+Path.GetFileName(file);
        Width=1040;Height=760;MinimumSize=new Size(620,420);StartPosition=FormStartPosition.CenterScreen;Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        BackColor=Color.FromArgb(25,28,34);ForeColor=Color.White;Font=new Font("Microsoft YaHei UI",10);
        var tools=new FlowLayoutPanel{Dock=DockStyle.Top,Height=48,Padding=new Padding(10,7,0,0)};var reset=new Button{Text="适合窗口",AutoSize=true};reset.Click+=(s,e)=>{zoom=1;offset=PointF.Empty;canvas.Invalidate();};tools.Controls.Add(reset);
        string explanation=badge=="AI"?"AI 画面预览 · 滚轮缩放 · 拖动平移":"C4D 保存的场景图片 · 滚轮缩放 · 拖动平移";tools.Controls.Add(new Label{Text=explanation,AutoSize=true,Margin=new Padding(18,6,0,0)});
        status.Dock=DockStyle.Bottom;status.Height=38;status.Padding=new Padding(12,7,0,0);status.Text="正在读取预览…";canvas.Dock=DockStyle.Fill;canvas.Paint+=PaintPreview;
        canvas.MouseWheel+=(s,e)=>{zoom=Math.Max(.1f,Math.Min(12f,zoom*(e.Delta>0?1.15f:1/1.15f)));canvas.Invalidate();};canvas.MouseDown+=(s,e)=>{if(e.Button==MouseButtons.Left){dragging=true;last=e.Location;canvas.Capture=true;}};canvas.MouseMove+=(s,e)=>{if(dragging){offset.X+=e.X-last.X;offset.Y+=e.Y-last.Y;last=e.Location;canvas.Invalidate();}};canvas.MouseUp+=(s,e)=>{dragging=false;canvas.Capture=false;};canvas.MouseEnter+=(s,e)=>canvas.Focus();canvas.Resize+=(s,e)=>canvas.Invalidate();
        Controls.Add(canvas);Controls.Add(tools);Controls.Add(status);
        Shown+=async(s,e)=>{try{var cached=await Task.Run(()=>Generate(file));if(IsDisposed)return;using(var input=Image.FromFile(cached))preview=new Bitmap(input);status.Text=Path.GetFileName(file)+(badge=="AI"?" · PDF 兼容预览":" · 保存时的 C4D 场景图片");canvas.Invalidate();}catch(Exception ex){if(!IsDisposed)status.Text="无法预览："+ex.Message;}};FormClosed+=(s,e)=>{if(preview!=null)preview.Dispose();};
    }
    void PaintPreview(object sender,PaintEventArgs e) {
        if(preview!=null){float fit=Math.Min((canvas.Width-40f)/preview.Width,(canvas.Height-40f)/preview.Height)*zoom;float w=preview.Width*fit,h=preview.Height*fit;e.Graphics.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(preview,(canvas.Width-w)/2+offset.X,(canvas.Height-h)/2+offset.Y,w,h);}
        int width=Math.Max(45,badge.Length*12+16);using(var b=new SolidBrush(Color.FromArgb(45,52,64)))e.Graphics.FillRectangle(b,12,canvas.Height-36,width,23);e.Graphics.DrawString(badge,Font,Brushes.White,16,canvas.Height-34);
    }
    static string Quote(string value){return "\""+value.Replace("\"","")+"\"";}
    static string Generate(string file) {
        if(!File.Exists(file))throw new Exception("文件不存在。");string extension=Path.GetExtension(file).ToLowerInvariant();if(extension!=".c4d"&&extension!=".ai")throw new Exception("这个窗口只处理 C4D 和 AI 文件。");
        var info=new FileInfo(file);string signature=file+"|"+info.Length+"|"+info.LastWriteTimeUtc.Ticks,dll=null;
        if(extension==".c4d"){string backendFile=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"backend.txt");if(!File.Exists(backendFile))throw new Exception("没有找到本机 Cinema 4D 组件，请重新运行安装程序。");dll=File.ReadAllText(backendFile).Trim();if(!File.Exists(dll))throw new Exception("Cinema 4D 组件已移动，请重新安装豌豆预览。");signature+="|"+dll+"|"+new FileInfo(dll).LastWriteTimeUtc.Ticks;}
        string key;using(var hash=SHA256.Create())key=BitConverter.ToString(hash.ComputeHash(Encoding.UTF8.GetBytes(signature))).Replace("-","");string dir=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"WandouPreview","Cache");Directory.CreateDirectory(dir);string target=Path.Combine(dir,key+".png");if(File.Exists(target))return target;string temporary=Path.Combine(dir,Guid.NewGuid().ToString("N")+".png");
        try {
            string arguments=extension==".ai"?"--extract-ai "+Quote(file)+" "+Quote(temporary)+" 1024":"--extract "+Quote(dll)+" "+Quote(file)+" "+Quote(temporary);var start=new ProcessStartInfo(Application.ExecutablePath,arguments){UseShellExecute=false,CreateNoWindow=true};
            using(var child=Process.Start(start)){if(!child.WaitForExit(15000)){try{child.Kill();child.WaitForExit(2000);}catch{}throw new Exception("读取超过 15 秒，已停止。");}if(child.ExitCode!=0||!File.Exists(temporary)){string error=temporary+".error.txt",detail=File.Exists(error)?File.ReadAllText(error):"文件中没有可读取的预览。";try{if(File.Exists(error))File.Delete(error);}catch{}throw new Exception(detail.Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries)[0]);}}
            if(!File.Exists(target)){try{File.Move(temporary,target);}catch(IOException){if(!File.Exists(target))throw;}}return target;
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
    }
}

static class Program {
    [STAThread] static int Main(string[] args) {
        if(args.Length==4&&(args[0]=="--extract"||args[0]=="--extract-bmp")){try{Native.ExtractC4D(args[1],args[2],args[3],args[0]=="--extract-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[3]+".error.txt",ex.ToString());return 1;}}
        if(args.Length==4&&(args[0]=="--extract-ai"||args[0]=="--extract-ai-bmp")){try{Native.ExtractAi(args[1],args[2],Int32.Parse(args[3]),args[0]=="--extract-ai-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[2]+".error.txt",ex.ToString());return 1;}}
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);if(args.Length!=1){MessageBox.Show("请在资源管理器中右键 C4D 或 AI 文件，选择“豌豆预览”。","豌豆预览 0.1.0");return 0;}try{Application.Run(new PreviewWindow(args[0]));return 0;}catch(Exception ex){MessageBox.Show(ex.Message,"豌豆预览 0.1.0");return 1;}
    }
}
