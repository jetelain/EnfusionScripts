[EntityEditorProps(category: "Tutorial/Entities", description: "SignalR Demo")]
class PMAD_SignalREntityClass :  GenericEntityClass
{
}

class PMAD_SignalREntity : GenericEntity
{
	[Attribute(defvalue: "http://localhost:5262/hub", desc: "Base URI")]
	protected string m_baseUri;
	
	private ref PMAD_SignalRHubConnection m_hub;
	private ref PMAD_SignalDemoCallback m_hubcallback;
	
	void PMAD_SignalREntity(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}
	
	protected override void EOnInit(IEntity owner)
	{
		GetGame().GetCallqueue().CallLater(DoConnect);
	}
	
	void DoConnect()
	{
		m_hubcallback = new PMAD_SignalDemoCallback(this);
		m_hub = new PMAD_SignalRHubConnection(m_baseUri, m_hubcallback);
		m_hub.Start();
	}
	
	void BeginLoop()
	{
		GetGame().GetCallqueue().CallLater(DoSend, 1000, true);
	}
	
	void DoSend()
	{
		m_hub.SendStringArray("Ping", { "Data" });
	}
}

class PMAD_SignalDemoCallback : PMAD_SignalHubCallback
{
	private PMAD_SignalREntity m_owner;
	
	void PMAD_SignalDemoCallback(PMAD_SignalREntity owner)
	{
		m_owner = owner;
	}
	
	override void OnInvoke(string target, array<string> arguments)
	{
		PrintFormat("Invoke '%1'('%2')", target, arguments[0]);
	}
	
	override void OnConnected()
	{
		Print("CONNECTED !", LogLevel.WARNING);
		m_owner.BeginLoop();
	}
}
