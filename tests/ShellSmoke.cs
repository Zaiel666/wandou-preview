using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential)] struct Size {public int Width,Height;}
[ComImport,Guid("bcc18b79-ba16-442f-80c4-8a59c30c463b"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IImageFactory {void GetImage(Size size,uint flags,out IntPtr bitmap);}
class Smoke {
    [DllImport("shell32",CharSet=CharSet.Unicode,PreserveSig=true)] static extern int SHCreateItemFromParsingName(string path,IntPtr bind,ref Guid iid,out IImageFactory factory);
    [DllImport("gdi32")]static extern bool DeleteObject(IntPtr bitmap);
    [STAThread] static int Main(string[] args){
        IImageFactory factory=null;IntPtr bitmap=IntPtr.Zero;
        try{Guid iid=typeof(IImageFactory).GUID;Marshal.ThrowExceptionForHR(SHCreateItemFromParsingName(args[0],IntPtr.Zero,ref iid,out factory));factory.GetImage(new Size{Width=256,Height=256},0x8,out bitmap);using(var image=Image.FromHbitmap(bitmap)){image.Save(args[1],ImageFormat.Png);Console.WriteLine("Shell thumbnail: {0}x{1}",image.Width,image.Height);}return 0;}
        catch(Exception e){Console.WriteLine(e);return 1;}
        finally{if(bitmap!=IntPtr.Zero)DeleteObject(bitmap);if(factory!=null)Marshal.ReleaseComObject(factory);}
    }
}
