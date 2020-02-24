using System;
using System.Runtime.InteropServices;

namespace MlcUnity
{
    public class Delegates
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnLog(string message);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int OnReceive(IntPtr s, IntPtr data, int size, IntPtr userdata);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int OnStatusChange(IntPtr s, int state, IntPtr userdata);
    }

    public class Lib
    {
#if !UNITY_EDITOR && UNITY_IOS
        public const string LIB_NAME = "__Internal";
#else
        public const string LIB_NAME = "mlc";
#endif

        [Obsolete]
        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr mlc_set_logger(Delegates.OnLog function);


        [DllImport(LIB_NAME, CallingConvention=CallingConvention.Cdecl)]
        public static extern IntPtr cycle_create(ref cycle_conf conf, bool is_server);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int cycle_destroy(IntPtr cycle);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int cycle_step(IntPtr cycle, int wait_ms);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int cycle_pause(int pause);


        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr session_create_client(IntPtr cycle, ref mlc_addr_conf conf);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr session_create_client_by_url(IntPtr cycle, string url);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_connect(IntPtr session);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_close(IntPtr session, int reason);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_getinfo(IntPtr session, ref session_info info);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_data_send(IntPtr session, byte[] data, int data_len);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_recv(IntPtr session, byte[] data, int data_len);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_set_receive_function(IntPtr session, Delegates.OnReceive function);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_set_userdata(IntPtr session, IntPtr data);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr session_get_userdata(IntPtr session);

        [DllImport(LIB_NAME, CallingConvention = CallingConvention.Cdecl)]
        public static extern int session_set_status_change_function(IntPtr session, Delegates.OnStatusChange function);
    }
}
