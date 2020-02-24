using System.Runtime.InteropServices;

namespace MlcUnity
{
    [StructLayout(LayoutKind.Sequential)]
    public struct mlc_addr_conf
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 64)]
        public byte[] addr;
        public ushort port;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct session_info
    {
        public byte health;
        public byte state;
        public byte active;
        public byte send_busy;
        public uint ping;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct rs_conf
    {
        public uint data_shard;
        public uint parity_shard;
    };

    [StructLayout(LayoutKind.Sequential)]
    public struct cycle_conf
    {
        public uint connection_n;
        public uint mtu;
        public uint debugger;
        public uint pool_block_size;
        public uint pool_block_cnt;
        public uint pool_small_block_size;
        public uint pool_small_block_cnt;
        public rs_conf rs;
        public uint heartbeat_check_ms;
        public uint heartbeat_timeout_ms;
        public uint connect_timeout_ms;
        public int backlog;
        public Delegates.OnLog logger;
    };
}