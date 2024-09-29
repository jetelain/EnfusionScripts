enum PMAD_SignalREState
{
	Disconnected,
	Connecting,
	Connected,
	Disconnecting,
	ConnectionFailed,
	ConnectionLost
}

//! Represents a connection to a SignalR Hub.
sealed class PMAD_SignalRHubConnection
{
	private ref PMAD_SignalHubCallback m_callback;
	private RestContext m_restContext;
	private string m_connectionToken;
	private string m_connectionId;
	private string m_lastError;
	private ref PMAD_SignalRLongPollCallback m_longPoll;
	private ref PMAD_SignalRNegotiateCallback m_negotiate;
	private ref PMAD_SignalRSendCallback m_send;
	private ref RestCallback m_close;
	private PMAD_SignalREState m_state;

	//------------------------------------------------------------------------------------------------
	//! Create a a connection to a SignalR Hub.
	//! \param uri Base uri of the hub
	void PMAD_SignalRHubConnection(string uri, notnull PMAD_SignalHubCallback callback)
	{
		m_restContext = GetGame().GetRestApi().GetContext(uri);
		//m_restContext.SetHeaders("Extension","Enfusion/1.0");
		m_callback = callback;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the connection.
	void Start()
	{
		if (!m_negotiate)
		{
			m_negotiate = new PMAD_SignalRNegotiateCallback(this);
		}
		m_state = PMAD_SignalREState.Connecting;
		m_restContext.POST(m_negotiate, "/negotiate?negotiateVersion=1", "");
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the connection.
	bool Stop()
	{		
		if (m_state != PMAD_SignalREState.Connected)
		{
			return false;
		}
		if ( !m_close )
		{
			m_close = new RestCallback();
		}
		m_state = PMAD_SignalREState.Disconnecting;
		m_callback.OnDisconnected();
		m_restContext.DELETE(m_close, "?id=" + m_connectionToken, string.Empty);
		// TODO: change state when DELETE successed
		return true;	
	}

	//------------------------------------------------------------------------------------------------
	//! Invokes a hub method on the server using the specified name and arguments. Does not wait for a response from the receiver.
	//! \param   target    The name of the server method to invoke.
	//! \param   arguments The arguments used to invoke the server method.
	//! \return  true if state allows to send message
	bool SendStringArray(string target, notnull array<string> arguments)
	{
		PMAD_SignalRHubMessageStringArrayJson msg = new PMAD_SignalRHubMessageStringArrayJson();
		msg.target = target;
		msg.arguments = arguments;
		msg.type = 1; // Invocation
		return this._SendMessage(msg);
	}

	//------------------------------------------------------------------------------------------------
	//! Invokes a hub method on the server using the specified name and arguments. Does not wait for a response from the receiver.
	//! \param   target    The name of the server method to invoke.
	//! \param   arguments The arguments used to invoke the server method.
	//! \return  true if state allows to send message
	bool Send(string target, notnull array<JsonApiStruct> arguments)
	{
		PMAD_SignalRHubMessageGenericJson msg = new PMAD_SignalRHubMessageGenericJson();
		msg.target = target;
		msg.arguments = arguments;
		msg.type = 1; // Invocation
		return this._SendMessage(msg);
	}

	private bool _SendMessage(notnull JsonApiStruct msg)
	{
		if (m_state != PMAD_SignalREState.Connected)
		{
			Print("Not sent : State=" + m_state + ", Payload=" + msg.AsString(), LogLevel.WARNING);
			return false;
		}
		if (!m_send)
		{
			m_send = new PMAD_SignalRSendCallback(this);
		}
		msg.Pack();
		m_restContext.POST(m_send, "?id=" + m_connectionToken, msg.AsString() + ""); // 0x1e ""
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void _NegotiateSuccess(string connectionToken, string connectionId)
	{
		m_connectionToken = connectionToken;
		m_connectionId = connectionId;
		m_state = PMAD_SignalREState.Connected;
		this._BeginPoll();
		m_callback.OnConnected();
	}

	//------------------------------------------------------------------------------------------------
	void _NegotiateFailed(string reason)
	{
		Print("Negotiate Failed due to " + reason, LogLevel.ERROR);
		m_state = PMAD_SignalREState.ConnectionFailed;
		m_lastError = reason;
		m_callback.OnError(m_state, reason);
	}

	//------------------------------------------------------------------------------------------------
	void _PollFailed(string reason)
	{
		Print("Poll Failed due to " + reason, LogLevel.ERROR);
		m_state = PMAD_SignalREState.ConnectionLost;
		m_lastError = reason;
		m_callback.OnError(m_state, reason);
	}

	//------------------------------------------------------------------------------------------------
	void _SendFailed(string reason)
	{
		Print("Send Failed due to " + reason, LogLevel.ERROR);
		// m_state = PMAD_SignalREState.ConnectionLost; FIXME: change state ?
		m_lastError = reason;
		m_callback.OnError(m_state, reason);
	}

	//------------------------------------------------------------------------------------------------
	void _Poll()
	{
		if (m_state != PMAD_SignalREState.Connected)
		{
			return;
		}
		if (!m_longPoll)
		{
			m_longPoll = new PMAD_SignalRLongPollCallback(this);
		}
		m_restContext.GET(m_longPoll, "?id=" + m_connectionToken);
	}
	
	//------------------------------------------------------------------------------------------------
	void _BeginPoll()
	{
		if (!m_longPoll)
		{
			m_longPoll = new PMAD_SignalRLongPollCallback(this);
		}
		m_restContext.POST(m_longPoll, "?id=" + m_connectionToken, "{\"protocol\":\"json\",\"version\":1}");
	}
	
	//------------------------------------------------------------------------------------------------
	void _InvokeClient(string target, notnull array<string> arguments)
	{
		m_callback.OnInvoke(target, arguments);
	}
}

sealed class PMAD_SignalRNegotiateCallback extends RestCallback
{
	private PMAD_SignalRHubConnection m_hubConnection;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRNegotiateCallback(PMAD_SignalRHubConnection hubConnection)
	{
		m_hubConnection = hubConnection;
	}

	//------------------------------------------------------------------------------------------------
	override void OnError(int errorCode)
	{
		m_hubConnection._NegotiateFailed("http:" + errorCode);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTimeout()
	{
		m_hubConnection._NegotiateFailed("timeout");
	}

	//------------------------------------------------------------------------------------------------
	override void OnSuccess(string data, int dataSize)
	{
		PMAD_SignalRNegotiateJson json = new PMAD_SignalRNegotiateJson();
		json.ExpandFromRAW(data);
		if (json.negotiateVersion != 1)
		{
			m_hubConnection._NegotiateFailed("invalid:version");
			return;
		}
		bool canTextLongPoll = false;
		foreach (PMAD_SignalRTransportJson transport : json.availableTransports)
		{
			if (transport.transport == "LongPolling")
			{
				if (transport.transferFormats.Contains("Text"))
				{
					canTextLongPoll = true;
				}
			}
		}
		if (!canTextLongPoll)
		{
			m_hubConnection._NegotiateFailed("invalid:notextlongpolling");
			return;
		}
		m_hubConnection._NegotiateSuccess(json.connectionToken, json.connectionId);
	}
}

sealed class PMAD_SignalRTransportJson extends JsonApiStruct
{
	string transport;
	ref array<string> transferFormats;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRTransportJson()
	{
		RegV("transport");
		RegV("transferFormats");
	}
}

sealed class PMAD_SignalRNegotiateJson extends JsonApiStruct
{
	string connectionToken;
	string connectionId;
	int negotiateVersion;
	ref array<ref PMAD_SignalRTransportJson> availableTransports;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRNegotiateJson()
	{
		RegV("connectionToken");
		RegV("connectionId");
		RegV("negotiateVersion");
		RegV("availableTransports");
	}
}

sealed class PMAD_SignalRLongPollCallback extends RestCallback
{
	private PMAD_SignalRHubConnection m_hubConnection;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRLongPollCallback(PMAD_SignalRHubConnection hubConnection)
	{
		m_hubConnection = hubConnection;
	}

	//------------------------------------------------------------------------------------------------
	override void OnError(int errorCode)
	{
		m_hubConnection._PollFailed("http:" + errorCode);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTimeout()
	{
		m_hubConnection._Poll();
	}

	//------------------------------------------------------------------------------------------------
	override void OnSuccess(string data, int dataSize)
	{
		if (data)
		{
			array<string> dataParts = {};
			data.Split("", dataParts, true); // 0x1e ""
			PMAD_SignalRHubMessageStringArrayJson msg = new PMAD_SignalRHubMessageStringArrayJson();
			foreach (string dataPart : dataParts)
			{
				msg.ExpandFromRAW(dataPart);
				switch (msg.type)
				{
					case 1: // Invocation
						m_hubConnection._InvokeClient(msg.target, msg.arguments);
						break;
					case 7: // Close
						m_hubConnection.Stop();
						break;
				}
			}
		}
		m_hubConnection._Poll();
	}

}

sealed class PMAD_SignalRSendCallback extends RestCallback
{
	private PMAD_SignalRHubConnection m_hubConnection;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRSendCallback(PMAD_SignalRHubConnection hubConnection)
	{
		m_hubConnection = hubConnection;
	}

	//------------------------------------------------------------------------------------------------
	override void OnError(int errorCode)
	{
		m_hubConnection._SendFailed("http:" + errorCode);
	}

	//------------------------------------------------------------------------------------------------
	override void OnTimeout()
	{
		m_hubConnection._SendFailed("timeout");
	}

	//------------------------------------------------------------------------------------------------
	override void OnSuccess(string data, int dataSize)
	{
		// Nothing
	}

}

sealed class PMAD_SignalRHubMessageStringArrayJson extends JsonApiStruct
{
	int type;
	string target;
	ref array<string> arguments;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRHubMessageStringArrayJson()
	{
		RegV("type");
		RegV("target");
		RegV("arguments");
	}
}

sealed class PMAD_SignalRHubMessageGenericJson extends JsonApiStruct
{
	int type;
	string target;
	ref array<JsonApiStruct> arguments;

	//------------------------------------------------------------------------------------------------
	void PMAD_SignalRHubMessageGenericJson()
	{
		RegV("type");
		RegV("target");
		RegV("arguments");
	}
}

class PMAD_SignalHubCallback
{
	//------------------------------------------------------------------------------------------------
	void OnInvoke(string target, array<string> arguments)
	{

	}

	//------------------------------------------------------------------------------------------------
	void OnConnected()
	{

	}

	//------------------------------------------------------------------------------------------------
	void OnDisconnected()
	{

	}

	//------------------------------------------------------------------------------------------------
	void OnError(PMAD_SignalREState newState, string reason)
	{

	}
}
