# SignalR Client

Client to connect to a SignalR Core Hub using LongPolling Transport with JSON Protocol.

Limitations :
- Only `Send` can be used. `Invoke` is not supported.
- Server can send only string arguments
- Client can either :
  - Send all arguments as string
  - Send all arguments as objects (using a class based on JsonApiStruct)

Implemented by `SignalR\Client\scripts\Game\PMAD_SignalR.c`

## Client Usage

1. Create a class derivated from `PMAD_SignalHubCallback`

```
class TAG_MySignalRCallback : PMAD_SignalHubCallback
{
	override void OnInvoke(string target, array<string> arguments)
	{
		// Handle message from server
	}
	
	override void OnConnected()
	{
		// Connected
	}

	override void OnDisconnected()
	{
		// Graceful disconnect 
	}

	override void OnError(PMAD_SignalREState newState, string reason)
	{
		// Errors
	}
}
```

2. Instanciate PMAD_SignalRHubConnection and start connection

```
m_hubcallback = new TAG_MySignalRCallback(this);
m_hub = new PMAD_SignalRHubConnection("https://servername/hubname", m_hubcallback);
m_hub.Start();
```

Remark: Enfusion does not supports self-signed certificates. For localhost use http instead.

3. Once connected invoke methods on server

With string arguments

```
m_hub.SendStringArray("MethodName", { "Argument1" });
```

With object arguments

```
TAG_MyJson arg1 = new TAG_MyJson(); // Class based on JsonApiStruct

m_hub.Send("MethodName", { JsonApiStruct.Cast(arg1) });
```

