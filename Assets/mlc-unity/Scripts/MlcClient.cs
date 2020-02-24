using UnityEngine;
using MlcUnity;
using System.Text;
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Collections;

namespace MlcUnity
{
    public class MlcClient : MonoBehaviour
    {
        public delegate int OnDataReceiveDelegate(IntPtr data, int size);
        public delegate int OnStatusChangeDelegate(int status);

        private string serverUrl;
        private IntPtr cycle;
        private IntPtr session;
        private session_info info;

        private Task multiThreadTask;
        private CancellationTokenSource taskCancelSource;
        private EventWaitHandle pauseHandle;
        private bool started = false;
        private bool paused = false;
        private OnDataReceiveDelegate onDataReceiveCallback = null;
        private OnStatusChangeDelegate onStatusChangeCallback = null;
        private GCHandle thisHandle;

        public session_info Info
        {
            get
            {
                return info;
            }
        }

        public bool Ready
        {
            get
            {
                return (mlc_state)info.state == mlc_state.connected;
            }
        }

        public void Connect(string url)
        {
            serverUrl = url;
            pauseHandle = new EventWaitHandle(!paused, EventResetMode.ManualReset);
            taskCancelSource = new CancellationTokenSource();
            multiThreadTask = Task.Run(MainLoop);
        }

        public int Send(byte[] data)
        {
            lock(this)
            {
                return started ? Lib.session_data_send(session, data, data.Length) : 0;
            }
        }

        public int Receive(byte[] data)
        {
            lock(this)
            {
                return started ? Lib.session_recv(session, data, data.Length) : 0;
            }
        }

        public void SetOnDataReceiveCallback(OnDataReceiveDelegate callback)
        {
            onDataReceiveCallback = callback;
        }

        public void SetOnStatusChangeCallback(OnStatusChangeDelegate callback)
        {
            onStatusChangeCallback = callback;
        }

        public void Pause(bool value)
        {
            paused = value;

            if (paused) pauseHandle.Reset();
            else pauseHandle.Set();
        }

        public async Task Shutdown()
        {
            started = false;
            onDataReceiveCallback = null;
            thisHandle.Free();

            if (multiThreadTask != null)
            {
                taskCancelSource.Cancel();
                pauseHandle.Set();
                await multiThreadTask;
            }
            multiThreadTask = null;

            Lib.session_close(session, 0);
            session = IntPtr.Zero;
            Lib.cycle_destroy(cycle);
            cycle = IntPtr.Zero;
        }

        private async void OnDestroy()
        {
            await Shutdown();
        }

        private void MainLoop()
        {
            var conf = new cycle_conf()
            {
                connection_n = 10,
            };

            cycle = Lib.cycle_create(ref conf, false);

            session = Lib.session_create_client_by_url(cycle, serverUrl);

            if (onDataReceiveCallback != null)
            {
                thisHandle = GCHandle.Alloc(this);
                Lib.session_set_userdata(session, (IntPtr)thisHandle);
                Lib.session_set_receive_function(session, HandleReceive);
            }
            if (onStatusChangeCallback != null)
            {
                Lib.session_set_status_change_function(session, HandleStatusChange);
            }
            Lib.session_connect(session);

            started = true;

            while (!taskCancelSource.IsCancellationRequested && pauseHandle.WaitOne())
            {
                lock (this)
                {
                    Lib.cycle_step(cycle, 1);
                    Lib.session_getinfo(session, ref info);
                }
                Thread.Sleep(1);
            }
        }

        private static int HandleReceive(IntPtr s, IntPtr data, int size, IntPtr userdata)
        {
            try
            {
                var thiz = ((GCHandle)userdata).Target as MlcClient;

                if (thiz.onDataReceiveCallback != null)
                {
                    return thiz.onDataReceiveCallback(data, size);
                }
            }
            catch(Exception e)
            {
                MlcLog.LogError(e.ToString());
            }
            return size;
        }

        private static int HandleStatusChange(IntPtr s, int states, IntPtr userdata)
        {
            try
            {
                var thiz = ((GCHandle)userdata).Target as MlcClient;
                if (thiz.onStatusChangeCallback != null)
                {
                    return thiz.onStatusChangeCallback(states);
                }
            }
            catch (Exception e)
            {
                MlcLog.LogError(e.ToString());
            }
            return 0;
        }

    }
}