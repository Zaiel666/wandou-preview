using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Drawing;
using System.Drawing.Imaging;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Threading;
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
    [DllImport("shell32", CharSet=CharSet.Unicode)] public static extern void SHChangeNotify(uint eventId, uint flags, IntPtr item1, IntPtr item2);
    [DllImport("shell32", CharSet=CharSet.Unicode, EntryPoint="SHChangeNotify")] public static extern void SHChangeNotifyPath(uint eventId, uint flags, string item1, IntPtr item2);
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

    static string ReadAsciiLine(BinaryReader reader) {
        var bytes=new MemoryStream();
        while(reader.BaseStream.Position<reader.BaseStream.Length){byte value=reader.ReadByte();if(value==10)break;if(value!=13)bytes.WriteByte(value);}
        return Encoding.ASCII.GetString(bytes.ToArray());
    }

    public static void ExtractHdr(string file,string output,int requestedSize,bool bmp) {
        int width,height;byte[] rgbe;
        using(var input=File.OpenRead(file))using(var reader=new BinaryReader(input)){
            string first=ReadAsciiLine(reader);if(!first.StartsWith("#?"))throw new Exception("不是有效的 Radiance HDR 文件。");
            string line;do{line=ReadAsciiLine(reader);if(input.Position>=input.Length)throw new Exception("HDR 文件头不完整。");}while(line.Length>0);
            string resolution=ReadAsciiLine(reader);var match=Regex.Match(resolution,@"^([+-])Y\s+(\d+)\s+([+-])X\s+(\d+)$");if(!match.Success)throw new Exception("暂不支持这个 HDR 像素排列。");
            height=Int32.Parse(match.Groups[2].Value);width=Int32.Parse(match.Groups[4].Value);if(width<1||height<1||((long)width*height)>150000000)throw new Exception("HDR 图片尺寸无效或过大。");rgbe=new byte[width*height*4];
            for(int y=0;y<height;y++){
                byte a=reader.ReadByte(),b=reader.ReadByte(),c=reader.ReadByte(),d=reader.ReadByte();int row=y*width*4;
                if(width>=8&&width<=32767&&a==2&&b==2&&(c&128)==0&&((c<<8)|d)==width){
                    for(int channel=0;channel<4;channel++){int x=0;while(x<width){byte code=reader.ReadByte();if(code>128){int count=code-128;if(count==0||x+count>width)throw new Exception("HDR 扫描行损坏。");byte value=reader.ReadByte();for(int i=0;i<count;i++)rgbe[row+(x+i)*4+channel]=value;x+=count;}else{int count=code;if(count==0||x+count>width)throw new Exception("HDR 扫描行损坏。");for(int i=0;i<count;i++)rgbe[row+(x+i)*4+channel]=reader.ReadByte();x+=count;}}}
                }else{
                    rgbe[row]=a;rgbe[row+1]=b;rgbe[row+2]=c;rgbe[row+3]=d;for(int x=1;x<width;x++){int p=row+x*4;rgbe[p]=reader.ReadByte();rgbe[p+1]=reader.ReadByte();rgbe[p+2]=reader.ReadByte();rgbe[p+3]=reader.ReadByte();}
                }
            }
        }
        int maxSize=Math.Max(128,Math.Min(1024,requestedSize));double scale=Math.Min(1.0,Math.Min((double)maxSize/width,(double)maxSize/height));int outWidth=Math.Max(1,(int)Math.Round(width*scale)),outHeight=Math.Max(1,(int)Math.Round(height*scale));
        double sum=0;int samples=0;for(int i=0;i<rgbe.Length;i+=Math.Max(4,(rgbe.Length/200000)&~3)){byte e=rgbe[i+3];if(e==0)continue;double f=Math.Pow(2.0,e-136);double lum=(rgbe[i]*.2126+rgbe[i+1]*.7152+rgbe[i+2]*.0722)*f;sum+=Math.Log(1e-5+lum);samples++;}double exposure=samples>0?.18/Math.Exp(sum/samples):1;
        using(var image=new Bitmap(outWidth,outHeight,PixelFormat.Format24bppRgb)){
            var area=new Rectangle(0,0,outWidth,outHeight);var data=image.LockBits(area,ImageLockMode.WriteOnly,PixelFormat.Format24bppRgb);byte[] pixels=new byte[Math.Abs(data.Stride)*outHeight];
            for(int y=0;y<outHeight;y++)for(int x=0;x<outWidth;x++){int sx=Math.Min(width-1,(int)(x/scale)),sy=Math.Min(height-1,(int)(y/scale)),source=(sy*width+sx)*4,target=y*data.Stride+x*3;byte e=rgbe[source+3];double f=e==0?0:Math.Pow(2.0,e-136);for(int channel=0;channel<3;channel++){double linear=rgbe[source+channel]*f*exposure,mapped=Math.Pow(1-Math.Exp(-Math.Max(0,linear)),1/2.2);pixels[target+2-channel]=(byte)Math.Max(0,Math.Min(255,(int)Math.Round(mapped*255)));}}
            Marshal.Copy(pixels,0,data.Scan0,pixels.Length);image.UnlockBits(data);image.Save(output,bmp?ImageFormat.Bmp:ImageFormat.Png);
        }
    }

    static int ReadBlendInt(byte[] bytes,int offset,bool bigEndian) {
        if(bigEndian)return (bytes[offset]<<24)|(bytes[offset+1]<<16)|(bytes[offset+2]<<8)|bytes[offset+3];
        return bytes[offset]|(bytes[offset+1]<<8)|(bytes[offset+2]<<16)|(bytes[offset+3]<<24);
    }

    static byte[] ReadExact(Stream input,int length) {
        byte[] result=new byte[length];int offset=0;
        while(offset<length){int count=input.Read(result,offset,length-offset);if(count<=0)throw new EndOfStreamException();offset+=count;}
        return result;
    }

    static void SkipExact(Stream input,int length) {
        byte[] buffer=new byte[65536];int remaining=length;
        while(remaining>0){int count=input.Read(buffer,0,Math.Min(buffer.Length,remaining));if(count<=0)throw new EndOfStreamException();remaining-=count;}
    }

    public static void ExtractBlend(string file,string output,int requestedSize,bool bmp) {
        Stream source=File.OpenRead(file),input=source;
        try {
            byte[] first=ReadExact(source,2);source.Position=0;
            if(first[0]==0x1f&&first[1]==0x8b)input=new GZipStream(source,CompressionMode.Decompress,false);
            byte[] header=ReadExact(input,12);
            if(Encoding.ASCII.GetString(header,0,7)!="BLENDER")throw new Exception("不是有效的 Blender 文件。");
            bool pointer64=header[7]==(byte)'-',bigEndian=header[8]==(byte)'V';int blockHeaderSize=pointer64?24:20;
            byte[] pixels=null;int width=0,height=0;
            while(true){
                byte[] block=ReadExact(input,blockHeaderSize);string code=Encoding.ASCII.GetString(block,0,4);int length=ReadBlendInt(block,4,bigEndian);
                if(length<0||length>1024*1024*1024)throw new Exception("Blender 文件块尺寸无效。");
                if(code=="REND"){SkipExact(input,length);continue;}
                if(code!="TEST")throw new Exception("这个 Blender 文件没有保存内嵌预览图，请在 Blender 中保存一次后重试。");
                if(length<8)throw new Exception("Blender 预览数据不完整。");byte[] dimensions=ReadExact(input,8);width=ReadBlendInt(dimensions,0,bigEndian);height=ReadBlendInt(dimensions,4,bigEndian);
                long expected=(long)width*height*4;if(width<1||height<1||width>8192||height>8192||expected!=length-8)throw new Exception("Blender 预览图尺寸无效。");pixels=ReadExact(input,(int)expected);break;
            }
            using(var original=new Bitmap(width,height,PixelFormat.Format32bppArgb)){
                var data=original.LockBits(new Rectangle(0,0,width,height),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);byte[] row=new byte[width*4];
                try{for(int y=0;y<height;y++){int sourceRow=(height-1-y)*width*4;for(int x=0;x<width;x++){int s=sourceRow+x*4,d=x*4;row[d]=pixels[s+2];row[d+1]=pixels[s+1];row[d+2]=pixels[s];row[d+3]=pixels[s+3];}Marshal.Copy(row,0,IntPtr.Add(data.Scan0,y*data.Stride),row.Length);}}finally{original.UnlockBits(data);}
                int maxSize=Math.Max(128,Math.Min(1024,requestedSize));double scale=Math.Min(1.0,Math.Min((double)maxSize/width,(double)maxSize/height));int outWidth=Math.Max(1,(int)Math.Round(width*scale)),outHeight=Math.Max(1,(int)Math.Round(height*scale));
                using(var result=new Bitmap(outWidth,outHeight,PixelFormat.Format32bppArgb)){using(var graphics=Graphics.FromImage(result)){graphics.CompositingMode=System.Drawing.Drawing2D.CompositingMode.SourceCopy;graphics.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;graphics.DrawImage(original,0,0,outWidth,outHeight);}result.Save(output,bmp?ImageFormat.Bmp:ImageFormat.Png);}
            }
        } finally {if(input!=source)input.Dispose();else source.Dispose();}
    }

    public static void RefreshAssociations() {
        Thread.Sleep(3500);
        SHChangeNotify(0x08000000,0x1000,IntPtr.Zero,IntPtr.Zero);
        var extensions=new HashSet<string>(StringComparer.OrdinalIgnoreCase){
            ".3ds",".3mf",".dae",".dxf",".fbx",".glb",".gltf",".ifc",".lwo",".lws",".lxo",".obj",".ply",".stl",".stp",".usd",".usda",".usdc",".usdz",".x",".x3d",".x3db",
            ".c4d",".blend",".ai",".hdr",
            ".mp4",".avi",".mov",".m4v",".wmv",".asf",".mpg",".mpeg",".mpe",".m1v",".m2v",".mts",".m2ts",".mkv",".webm",".ogv",".flv",".f4v",".vob",".3gp",".3g2"
        };
        string profile=Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        string[] roots={Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),Path.Combine(profile,"Downloads"),Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),Environment.GetFolderPath(Environment.SpecialFolder.MyVideos)};
        var visited=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(string root in roots){
            if(String.IsNullOrEmpty(root)||!Directory.Exists(root)||!visited.Add(root))continue;
            try{
                int files=0,directories=0;var pending=new Queue<KeyValuePair<string,int>>();pending.Enqueue(new KeyValuePair<string,int>(root,0));
                while(pending.Count>0&&files<10000&&directories<2000){var item=pending.Dequeue();directories++;try{foreach(string file in Directory.EnumerateFiles(item.Key,"*.*",SearchOption.TopDirectoryOnly)){if(!extensions.Contains(Path.GetExtension(file)))continue;SHChangeNotifyPath(0x00002000,0x0005,file,IntPtr.Zero);if(++files>=10000)break;}if(item.Value<3)foreach(string directory in Directory.EnumerateDirectories(item.Key,"*",SearchOption.TopDirectoryOnly)){string name=Path.GetFileName(directory);if(name.Equals("node_modules",StringComparison.OrdinalIgnoreCase)||name.Equals(".git",StringComparison.OrdinalIgnoreCase)||name.StartsWith("."))continue;pending.Enqueue(new KeyValuePair<string,int>(directory,item.Value+1));}}catch{}
                }
                SHChangeNotifyPath(0x00001000,0x0005,root,IntPtr.Zero);
            }catch{}
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
        string explanation=badge=="AI"?"AI 画面预览 · 滚轮缩放 · 拖动平移":badge=="HDR"?"HDR 自动曝光预览 · 滚轮缩放 · 拖动平移":"C4D 保存的场景图片 · 滚轮缩放 · 拖动平移";tools.Controls.Add(new Label{Text=explanation,AutoSize=true,Margin=new Padding(18,6,0,0)});
        status.Dock=DockStyle.Bottom;status.Height=38;status.Padding=new Padding(12,7,0,0);status.Text="正在读取预览…";canvas.Dock=DockStyle.Fill;canvas.Paint+=PaintPreview;
        canvas.MouseWheel+=(s,e)=>{zoom=Math.Max(.1f,Math.Min(12f,zoom*(e.Delta>0?1.15f:1/1.15f)));canvas.Invalidate();};canvas.MouseDown+=(s,e)=>{if(e.Button==MouseButtons.Left){dragging=true;last=e.Location;canvas.Capture=true;}};canvas.MouseMove+=(s,e)=>{if(dragging){offset.X+=e.X-last.X;offset.Y+=e.Y-last.Y;last=e.Location;canvas.Invalidate();}};canvas.MouseUp+=(s,e)=>{dragging=false;canvas.Capture=false;};canvas.MouseEnter+=(s,e)=>canvas.Focus();canvas.Resize+=(s,e)=>canvas.Invalidate();
        Controls.Add(canvas);Controls.Add(tools);Controls.Add(status);
        Shown+=async(s,e)=>{try{var cached=await Task.Run(()=>Generate(file));if(IsDisposed)return;using(var input=Image.FromFile(cached))preview=new Bitmap(input);status.Text=Path.GetFileName(file)+(badge=="AI"?" · PDF 兼容预览":badge=="HDR"?" · 自动曝光预览":" · 保存时的 C4D 场景图片");canvas.Invalidate();}catch(Exception ex){if(!IsDisposed)status.Text="无法预览："+ex.Message;}};FormClosed+=(s,e)=>{if(preview!=null)preview.Dispose();};
    }
    void PaintPreview(object sender,PaintEventArgs e) {
        if(preview!=null){float fit=Math.Min((canvas.Width-40f)/preview.Width,(canvas.Height-40f)/preview.Height)*zoom;float w=preview.Width*fit,h=preview.Height*fit;e.Graphics.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(preview,(canvas.Width-w)/2+offset.X,(canvas.Height-h)/2+offset.Y,w,h);}
        int width=Math.Max(45,badge.Length*12+16);using(var b=new SolidBrush(Color.FromArgb(45,52,64)))e.Graphics.FillRectangle(b,12,canvas.Height-36,width,23);e.Graphics.DrawString(badge,Font,Brushes.White,16,canvas.Height-34);
    }
    static string Quote(string value){return "\""+value.Replace("\"","")+"\"";}
    static string Generate(string file) {
        if(!File.Exists(file))throw new Exception("文件不存在。");string extension=Path.GetExtension(file).ToLowerInvariant();if(extension!=".c4d"&&extension!=".ai"&&extension!=".hdr")throw new Exception("这个窗口只处理 C4D、AI 和 HDR 文件。");
        var info=new FileInfo(file);string signature=file+"|"+info.Length+"|"+info.LastWriteTimeUtc.Ticks,dll=null;
        if(extension==".c4d"){string backendFile=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"backend.txt");if(!File.Exists(backendFile))throw new Exception("没有找到本机 Cinema 4D 组件，请重新运行安装程序。");dll=File.ReadAllText(backendFile).Trim();if(!File.Exists(dll))throw new Exception("Cinema 4D 组件已移动，请重新安装豌豆预览。");signature+="|"+dll+"|"+new FileInfo(dll).LastWriteTimeUtc.Ticks;}
        string key;using(var hash=SHA256.Create())key=BitConverter.ToString(hash.ComputeHash(Encoding.UTF8.GetBytes(signature))).Replace("-","");string dir=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"WandouPreview","Cache");Directory.CreateDirectory(dir);string target=Path.Combine(dir,key+".png");if(File.Exists(target))return target;string temporary=Path.Combine(dir,Guid.NewGuid().ToString("N")+".png");
        try {
            string arguments=extension==".ai"?"--extract-ai "+Quote(file)+" "+Quote(temporary)+" 1024":extension==".hdr"?"--extract-hdr "+Quote(file)+" "+Quote(temporary)+" 1024":"--extract "+Quote(dll)+" "+Quote(file)+" "+Quote(temporary);var start=new ProcessStartInfo(Application.ExecutablePath,arguments){UseShellExecute=false,CreateNoWindow=true};
            using(var child=Process.Start(start)){if(!child.WaitForExit(15000)){try{child.Kill();child.WaitForExit(2000);}catch{}throw new Exception("读取超过 15 秒，已停止。");}if(child.ExitCode!=0||!File.Exists(temporary)){string error=temporary+".error.txt",detail=File.Exists(error)?File.ReadAllText(error):"文件中没有可读取的预览。";try{if(File.Exists(error))File.Delete(error);}catch{}throw new Exception(detail.Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries)[0]);}}
            if(!File.Exists(target)){try{File.Move(temporary,target);}catch(IOException){if(!File.Exists(target))throw;}}return target;
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
    }
}

static class Program {
    [STAThread] static int Main(string[] args) {
        if(args.Length==1&&args[0]=="--refresh-associations"){try{Native.RefreshAssociations();return 0;}catch{return 1;}}
        if(args.Length==4&&(args[0]=="--extract"||args[0]=="--extract-bmp")){try{Native.ExtractC4D(args[1],args[2],args[3],args[0]=="--extract-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[3]+".error.txt",ex.ToString());return 1;}}
        if(args.Length==4&&(args[0]=="--extract-ai"||args[0]=="--extract-ai-bmp")){try{Native.ExtractAi(args[1],args[2],Int32.Parse(args[3]),args[0]=="--extract-ai-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[2]+".error.txt",ex.ToString());return 1;}}
        if(args.Length==4&&(args[0]=="--extract-hdr"||args[0]=="--extract-hdr-bmp")){try{Native.ExtractHdr(args[1],args[2],Int32.Parse(args[3]),args[0]=="--extract-hdr-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[2]+".error.txt",ex.ToString());return 1;}}
        if(args.Length==4&&(args[0]=="--extract-blend"||args[0]=="--extract-blend-bmp")){try{Native.ExtractBlend(args[1],args[2],Int32.Parse(args[3]),args[0]=="--extract-blend-bmp");return 0;}catch(Exception ex){File.WriteAllText(args[2]+".error.txt",ex.ToString());return 1;}}
        MessageBox.Show("这是豌豆预览的资源管理器缩略图后台组件。安装后请在文件夹中使用大图标查看文件。","豌豆预览 0.2.0");return 0;
    }
}
