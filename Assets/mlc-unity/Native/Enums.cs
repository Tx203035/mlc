namespace MlcUnity
{
    public enum mlc_state
    {
        init = 0,
        connecting = 1,
        connected = 2,
        disconnect = 3,
        lost = 4,
        close_wait = 5,
        closed = 6,
    };

    public enum mlc_error
    {
        OK = 0,
        ERR = -1,
        AGAIN = -2,
        BUSY = -3,
    };
}