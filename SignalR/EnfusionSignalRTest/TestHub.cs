using Microsoft.AspNetCore.SignalR;

namespace EnfusionSignalRTest
{
    internal sealed class TestHub : Hub
    {
        public async Task Ping(string data)
        {
            await Clients.Caller.SendAsync("Pong", data);
        }
    }
}