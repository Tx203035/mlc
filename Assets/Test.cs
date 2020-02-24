using UnityEngine;
using MlcUnity;
using System;
using System.Text;
using System.Runtime.InteropServices;

public class Test : MonoBehaviour
{
    public string serverUrl = "mlc://127.0.0.1:8888";

    private MlcClient mlc;

    private byte[] buffer = new byte[0x400];

    void Start()
    {
        mlc = gameObject.AddComponent<MlcClient>();
        mlc.SetOnDataReceiveCallback(OnDataReceive);
        mlc.Connect(serverUrl);
    }

    private int OnDataReceive(IntPtr data, int size)
    {
        Debug.Log(Marshal.PtrToStringAnsi(data, size));
        return size;
    }

    private void Update()
    {
        if (mlc.Ready)
        {
            mlc.Send(Encoding.UTF8.GetBytes($"Hello {Time.frameCount}"));

            while(true)
            {
                var size = mlc.Receive(buffer);
                if (size > 0)
                {
                    Debug.Log(Encoding.UTF8.GetString(buffer, 0, size));
                }
                else
                {
                    if ((mlc_error)size == mlc_error.AGAIN)
                    {
                        buffer = new byte[buffer.Length * 2];
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }
}
