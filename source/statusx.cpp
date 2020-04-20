#include <statusx.hpp>
#include <main.hpp>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <GarrysMod/Interfaces.hpp>
#include <string>
#include <strtools.h>
#include <inetchannel.h>
#include <iclient.h>
#include <iserver.h>
#include <eiface.h>
#include <cdll_int.h>
#include <cvar.h>
#include <steam/steam_gameserver.h>
#include <player.h>
#include <netadr.h>
#include <vector>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>

namespace statusx
{
	class CSteamGameServerAPIContext
	{
	public:
		ISteamClient* m_pSteamClient;
		ISteamGameServer* m_pSteamGameServer;
		ISteamUtils* m_pSteamGameServerUtils;
		ISteamNetworking* m_pSteamGameServerNetworking;
		ISteamGameServerStats* m_pSteamGameServerStats;
		ISteamHTTP* m_pSteamHTTP;
		ISteamInventory* m_pSteamInventory;
		ISteamUGC* m_pSteamUGC;
		ISteamApps* m_pSteamApps;
	};

	struct status_info
	{
		bool override;
		const char* hostname;
		const char* version_date;
		int version_protocol;
		int version_build;
		bool secure;
		const char* local_ip;
		int32_t udp_port;
		const char* public_ip;
		const char* map;
		Vector hostpos;
		int players;
		int maxplayers;
	};

	struct status_playerinfo
	{
		bool override;
		int userid;
		const char* name;
		const char* steamid;
		const char* connected;
		int ping;
		int loss;
		const char* state;
		const char* ip;
	};

	SourceSDK::FactoryLoader cvar_binary("vstdlib", false, true, "bin/");
	static std::string server_binary = Helpers::GetBinaryFileName("server", false, true, "garrysmod/bin/");
#if defined SYSTEM_WINDOWS
	static const char Host_Status_PrintClient_sym[] = "\x55\x8B\xEC\x81\xEC\x04\x02\x00\x00\x53\x56\x8B\x75\x08\x8B\xCE\x57\x8B\x06";
	static const size_t Host_Status_PrintClient_symlen = sizeof(Host_Status_PrintClient_sym) - 1;

	static const char StatusCommand_sym[] = "\x55\x8B\xEC\x81\xEC\x30\x02\x00\x00\x83\x3D\x58\x73\x68";
	static const size_t StatusCommand_symlen = sizeof(StatusCommand_sym) - 1;

	static const char UTIL_GetCommandClientIndex_sym[] = "\xA1\x2A\x2A\x2A\x2A\x40\xC3\xCC\xCC\xCC\xCC\xCC";
	static const size_t UTIL_GetCommandClientIndex_symlen = sizeof(UTIL_GetCommandClientIndex_sym) - 1;

	static const char Host_Client_Printf_sym[] = "\x55\x8B\xEC\x81\xEC\x00\x04\x00\x00\x8D\x45\x0C";
	static const size_t Host_Client_Printf_symlen = sizeof(Host_Client_Printf_symlen) - 1;

	static const char IServer_sig[] = "\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\xD8\x6D\x24\x83\x4D\xEC\x10";
	static const size_t IServer_siglen = sizeof(IServer_sig) - 1;

	static const char SteamGameServerAPIContext_sym[] = "\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x6A\x00\x68\x2A\x2A\x2A\x2A\xFF\x55\x08\x83\xC4\x08\xA3";
	static const size_t SteamGameServerAPIContext_symlen = sizeof(SteamGameServerAPIContext_sym) - 1;
#elif defined SYSTEM_POSIX
	static const char Host_Status_PrintClient_sym[] = "@_Z23Host_Status_PrintClientP7IClientbPFvPKczE";
	static const size_t Host_Status_PrintClient_symlen = 0;

	static const char StatusCommand_sym[] = "@_ZL6statusRK8CCommand";
	static const size_t StatusCommand_symlen = 0;

	static const char UTIL_GetCommandClientIndex_sym[] = "@_Z26UTIL_GetCommandClientIndexv";
	static const size_t UTIL_GetCommandClientIndex_symlen = 0;

	static const char IServer_sig[] = "@sv";
	static const size_t IServer_siglen = sizeof(IServer_sig) - 1;

	static const char SteamGameServerAPIContext_sym[] = "@_ZL27s_SteamGameServerAPIContext";
	static const size_t SteamGameServerAPIContext_symlen = 0;

	static const char netLocalAdr_sig[] = "@net_local_adr";
	static const size_t netLocalAdr_siglen = sizeof(netLocalAdr_sig) - 1;

	static const char version_sig[] = "@gpszVersionString";
	static const size_t version_siglen = sizeof(version_sig) - 1;

	static const char buildnumber_sig[] = "@_ZL13g_BuildNumber";
	static const size_t buildnumber_symlen = 0;

	static const char mainView_sig[] = "@g_MainViewOrigin";
	static const size_t mainView_siglen = sizeof(mainView_sig) - 1;
#endif
	static GarrysMod::Lua::ILuaInterface* lua = nullptr;
	static IVEngineServer* engine_server = nullptr;
	static IServer* server = nullptr;
	static CSteamGameServerAPIContext* gameserver_context = nullptr;
	static ICvar* icvar = nullptr;
	static int luaPlayer;
	static int luaEyePos;

	void* pMainOrigin;
	int version_build;
	const char* version_date;
	netadr_t* local_addr;
	IClient* host_client;
	Vector host_pos;

	typedef void(__cdecl* Host_Status_PrintClient_t)(IClient* client, bool bShowAddress, void (*print) (const char* fmt, ...));
	typedef int(*UTIL_GetCommandClientIndex_t)();

	static UTIL_GetCommandClientIndex_t UTIL_GetCommandClientIndex;
	static Detouring::Hook Host_Status_PrintClient_detour;
	static Detouring::Hook Status_detour;
	char statusInfoHook[] = "STATUS_INFO";
	char statusPlayerInfoHook[] = "STATUS_PLAYERINFO";

	bool UTIL_IsCommandIssuedByServerAdmin()
	{
		int issuingPlayerIndex = UTIL_GetCommandClientIndex();

		if (engine_server->IsDedicatedServer() && issuingPlayerIndex > 0)
			return false;

		if (issuingPlayerIndex > 1)
			return false;

		return true;
	}

	static const char* COM_FormatSeconds(int seconds)
	{
		static char string[64];

		int hours = 0;
		int minutes = seconds / 60;

		if (minutes > 0)
		{
			seconds -= (minutes * 60);
			hours = minutes / 60;

			if (hours > 0)
			{
				minutes -= (hours * 60);
			}
		}

		if (hours > 0)
		{
			Q_snprintf(string, sizeof(string), "%2i:%02i:%02i", hours, minutes, seconds);
		}
		else
		{
			Q_snprintf(string, sizeof(string), "%02i:%02i", minutes, seconds);
		}

		return string;
	}

	status_info BuildStatusInfo(IClient* user, void (*print) (const char* fmt, ...)) {
		ISteamGameServer* steamGS = gameserver_context != nullptr ? gameserver_context->m_pSteamGameServer : nullptr;

		double x = *reinterpret_cast<double*>(pMainOrigin);
		double y = *reinterpret_cast<double*>((DWORD)pMainOrigin + 0x04);
		double z = *reinterpret_cast<double*>((DWORD)pMainOrigin + 0x08);

		status_info newstatus;
		newstatus.override = false;
		newstatus.hostname = server->GetName();
		newstatus.version_date = version_date;
		newstatus.version_protocol = 24;
		newstatus.version_build = version_build;
		newstatus.secure = false;

		if (steamGS != nullptr)
		{
			newstatus.secure = steamGS->BSecure();
		}

		if (server->IsMultiplayer()) {
			newstatus.local_ip = local_addr->ToString(true);
			newstatus.udp_port = server->GetUDPPort();

			if (steamGS != nullptr) {
				uint32 pubIP = steamGS->GetPublicIP();

				if (pubIP > 0) {
					netadr_s publicIP;
					publicIP.SetIP(0);
					publicIP.SetPort(0);
					publicIP.SetType(NA_IP);
					publicIP.SetIP(pubIP);

					newstatus.public_ip = publicIP.ToString(true);
				}
			}
		}

		newstatus.map = server->GetMapName();
		newstatus.hostpos = Vector(x, y, z);
		newstatus.players = server->GetNumClients();
		newstatus.maxplayers = server->GetMaxClients();

		GarrysMod::Lua::ILuaObject* pPlayer;

		if (user != NULL) {
			edict_t* pent = engine_server->PEntityOfEntIndex(user->GetUserID() + 1);

			lua->GetField(GarrysMod::Lua::INDEX_GLOBAL, "Player");
			lua->PushNumber(engine_server->GetPlayerUserId(pent));
			lua->Call(1, 1);

			pPlayer = lua->GetObject(0);

			lua->Pop(1);
		}

		lua->GetField(GarrysMod::Lua::INDEX_GLOBAL, "hook");
		if (!lua->IsType(-1, GarrysMod::Lua::Type::TABLE))
		{
			lua->ErrorNoHalt("[%s] Global hook is not a table!\n", statusInfoHook);
			lua->Pop(2);
			return newstatus;
		}

		lua->GetField(-1, "Run");
		lua->Remove(-2);

		if (!lua->IsType(-1, GarrysMod::Lua::Type::FUNCTION))
		{
			lua->ErrorNoHalt("[%s] Global hook.Run is not a function!\n", statusInfoHook);
			lua->Pop(2);
			return newstatus;
		}

		lua->PushString(statusInfoHook);

		if (user != NULL) {
			lua->PushLuaObject(pPlayer);
		}
		else {
			lua->PushNil();
		}

		lua->CreateTable();
		lua->PushString(newstatus.hostname);
		lua->SetField(-2, "hostname");

		lua->PushString(newstatus.version_date);
		lua->SetField(-2, "version");

		lua->PushNumber(newstatus.version_protocol);
		lua->SetField(-2, "protocol");

		lua->PushNumber(newstatus.version_build);
		lua->SetField(-2, "build");

		lua->PushBool(newstatus.secure);
		lua->SetField(-2, "VAC");

		lua->PushString(newstatus.local_ip);
		lua->SetField(-2, "localIP");

		lua->PushNumber(newstatus.udp_port);
		lua->SetField(-2, "gameport");

		lua->PushString(newstatus.public_ip);
		lua->SetField(-2, "publicIP");

		lua->PushString(newstatus.map);
		lua->SetField(-2, "map");

		lua->PushVector(newstatus.hostpos);
		lua->SetField(-2, "pos");

		lua->PushNumber(newstatus.players);
		lua->SetField(-2, "players");

		lua->PushNumber(newstatus.maxplayers);
		lua->SetField(-2, "maxplayers");

		if (lua->PCall(3, 1, 0) != 0)
			lua->ErrorNoHalt("\n[%s] %s\n\n", statusInfoHook, lua->GetString(-1));

		if (lua->IsType(-1, GarrysMod::Lua::Type::STRING)) {
			if (lua->GetString(-1)) {
				newstatus.override = true;

				print("%s\n", lua->GetString(-1));

				lua->Pop(1);

				return newstatus;
			}
		}
		else if (lua->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
			lua->GetField(-1, "hostname");
			newstatus.hostname = lua->GetString(-1);
			lua->Pop(1);

			lua->GetField(-1, "version");
			newstatus.version_date = lua->GetString(-1);
			lua->Pop(1);

			lua->GetField(-1, "protocol");
			newstatus.version_protocol = lua->GetNumber(-1);
			lua->Pop(1);

			lua->GetField(-1, "build");
			newstatus.version_build = lua->GetNumber(-1);
			lua->Pop(1);

			lua->GetField(-1, "VAC");
			newstatus.secure = lua->GetBool(-1);
			lua->Pop(1);

			lua->GetField(-1, "localIP");
			newstatus.local_ip = lua->GetString(-1);
			lua->Pop(1);

			lua->GetField(-1, "gameport");
			newstatus.udp_port = lua->GetNumber(-1);
			lua->Pop(1);

			lua->GetField(-1, "publicIP");
			newstatus.public_ip = lua->GetString(-1);
			lua->Pop(1);

			lua->GetField(-1, "map");
			newstatus.map = lua->GetString(-1);
			lua->Pop(1);

			lua->GetField(-1, "pos");
			newstatus.hostpos = lua->GetVector(-1);
			lua->Pop(1);

			lua->GetField(-1, "players");
			newstatus.players = lua->GetNumber(-1);
			lua->Pop(1);

			lua->GetField(-1, "maxplayers");
			newstatus.maxplayers = lua->GetNumber(-1);
			lua->Pop(1);
		}

		lua->Pop(1);

		return newstatus;
	}

	std::vector<status_playerinfo> BuildStatusPlayerInfo(IClient* user) {
		std::vector<status_playerinfo> clients(server->GetClientCount());
		GarrysMod::Lua::ILuaObject* pPlayer;

		if (user != NULL) {
			edict_t* pent = engine_server->PEntityOfEntIndex(user->GetUserID() + 1);

			lua->GetField(GarrysMod::Lua::INDEX_GLOBAL, "Player");
			lua->PushNumber(engine_server->GetPlayerUserId(pent));
			lua->Call(1, 1);

			pPlayer = lua->GetObject(0);

			lua->Pop(1);
		}

		lua->GetField(GarrysMod::Lua::INDEX_GLOBAL, "hook");
		if (!lua->IsType(-1, GarrysMod::Lua::Type::TABLE))
		{
			lua->ErrorNoHalt("[%s] Global hook is not a table!\n", statusPlayerInfoHook);
			lua->Pop(2);
			return clients;
		}

		lua->GetField(-1, "Run");
		lua->Remove(-2);
		if (!lua->IsType(-1, GarrysMod::Lua::Type::FUNCTION))
		{
			lua->ErrorNoHalt("[%s] Global hook.Run is not a function!\n", statusPlayerInfoHook);
			lua->Pop(2);
			return clients;
		}

		lua->PushString(statusPlayerInfoHook);
		if (user != NULL) {
			lua->PushLuaObject(pPlayer);
		}
		else {
			lua->PushNil();
		}
		lua->CreateTable();

		int index = 1;
		int j;

		for (j = 0; j < server->GetClientCount(); j++)
		{
			IClient* client = server->GetClient(j);

			if (!client->IsConnected())
				continue;

			status_playerinfo info;

			INetChannelInfo* nci = client->GetNetChannel();
			edict_t* pent = engine_server->PEntityOfEntIndex(client->GetUserID() + 1);
			const char* state = "challenging";

			if (client->IsActive())
				state = "active";
			else if (client->IsSpawned())
				state = "spawning";
			else if (client->IsConnected())
				state = "connecting";

			int userid = 0;

			if (pent != NULL) {
				userid = engine_server->GetPlayerUserId(pent);
			}

			info.userid = userid;
			info.name = client->GetClientName();
			info.steamid = client->GetNetworkIDString();
			if (nci != NULL) {
				info.connected = COM_FormatSeconds(nci->GetTimeConnected());
				info.ping = (int)(1000.0f * nci->GetAvgLatency(FLOW_OUTGOING));
				info.loss = (int)(100.0f * nci->GetAvgLoss(FLOW_INCOMING));
				info.ip = nci->GetAddress();
			}
			else {
				info.connected = "";
				info.ping = 0;
				info.loss = 0;
				info.ip = "";
			}
			info.state = state;

			clients.at(j) = info;

			lua->PushNumber(index++);
			lua->CreateTable();

			lua->PushNumber(info.userid);
			lua->SetField(-2, "userid");

			lua->PushString(info.name);
			lua->SetField(-2, "name");

			lua->PushString(info.steamid);
			lua->SetField(-2, "steamid");

			lua->PushString(info.connected);
			lua->SetField(-2, "connected");

			lua->PushNumber(info.ping);
			lua->SetField(-2, "ping");

			lua->PushNumber(info.loss);
			lua->SetField(-2, "loss");

			lua->PushString(info.state);
			lua->SetField(-2, "state");

			lua->PushString(info.ip);
			lua->SetField(-2, "ip");

			lua->SetTable(-3);
		}

		if (lua->PCall(3, 1, 0) != 0)
			lua->ErrorNoHalt("\n[%s] %s\n\n", statusPlayerInfoHook, lua->GetString(-1));

		if (lua->IsType(-1, GarrysMod::Lua::Type::TABLE)) {
			int count = lua->ObjLen(-1);

			std::vector<status_playerinfo> newclients(count);

			for (int i = 0; i < count; i++) {
				status_playerinfo info;

				lua->PushNumber((double)(i + 1));
				lua->GetTable(-2);

				lua->GetField(-1, "userid");
				info.userid = lua->GetNumber(-1);
				lua->Pop(1);

				lua->GetField(-1, "name");
				info.name = lua->GetString(-1);
				lua->Pop(1);

				lua->GetField(-1, "steamid");
				info.steamid = lua->GetString(-1);
				lua->Pop(1);

				lua->GetField(-1, "connected");
				info.connected = lua->GetString(-1);
				lua->Pop(1);

				lua->GetField(-1, "ping");
				info.ping = lua->GetNumber(-1);
				lua->Pop(1);

				lua->GetField(-1, "loss");
				info.loss = lua->GetNumber(-1);
				lua->Pop(1);

				lua->GetField(-1, "state");
				info.state = lua->GetString(-1);
				lua->Pop(1);

				lua->GetField(-1, "ip");
				info.ip = lua->GetString(-1);
				lua->Pop(1);

				lua->Pop(1);
				newclients.at(i) = info;
			}

			clients = newclients;
		}

		lua->Pop(1);

		return clients;
	}

	void __cdecl Host_Status_PrintClient_d(status_playerinfo info, bool bShowAddress, void (*print) (const char* fmt, ...))
	{
		int lenName = strlen(info.name);
		int lenSteamID = strlen(info.steamid);
		int lenState = strlen(info.state);
		int lenConnected = strlen(info.connected);
		int width = 20 - (lenName + 2);

		if (lenConnected > 0) {
			print("# %6i ", info.userid);
			print("\"%s\"", info.name);
			print("%*s", width, "");
			print("%s", info.steamid);
			width = 20 - lenSteamID;
			print("%*s", width, "");
			print("%s", info.connected);
			width = 10 - lenConnected;
			print("%*s", width, "");
			print("%4i %4i %s", info.ping, info.loss, info.state);

			if (bShowAddress)
			{
				print(" %s", info.ip);
			}
		}
		else
		{
			print("# %6i ", info.userid);
			print("\"%s\"", info.name);
			print("%*s", width, "");
			print("%s", info.steamid);
			width = 40 - lenSteamID;
			print("%*s", width, "");
			print("%s", info.state);
		}

		print("\n");
	}

	void Host_Client_Printf(const char* fmt, ...)
	{
		va_list		argptr;
		char		string[1024];

		va_start(argptr, fmt);
		Q_vsnprintf(string, sizeof(string), fmt, argptr);
		va_end(argptr);

		host_client->ClientPrintf("%s", string);
	}

	CON_COMMAND(status, "Display map and connection status.") {
		IClient* client;
		int j;
		void (*print) (const char* fmt, ...);
		bool isServerside = (UTIL_IsCommandIssuedByServerAdmin() == 1);
		int index = UTIL_GetCommandClientIndex();

		if (isServerside)
		{
			print = Msg;
		}
		else
		{
			host_client = server->GetClient(index - 1);

			print = Host_Client_Printf;
		}

		status_info info = BuildStatusInfo(host_client, print);

		if (info.override) {
			return;
		}

		print("hostname: %s\n", info.hostname);
		print("version : %s/%d %d %s\n", info.version_date, info.version_protocol, info.version_build, info.secure ? "secure" : "insecure");

		if (server->IsMultiplayer()) {
			print("udp/ip  : %s:%i", info.local_ip, info.udp_port);

			if (info.public_ip != "") {
				print(" (public ip: %s)", info.public_ip);
			}

			print("\n");
		}

		host_pos = info.hostpos;

		print("map     : %s at: %d x, %d y, %d z\n", info.map, host_pos.x, host_pos.y, host_pos.z);
		print("players : %i (%i max)\n\n", info.players, info.maxplayers);
		print("# userid name                uniqueid            connected ping loss state ");

		if (isServerside)
		{
			print(" adr");
		}

		print("\n");

		std::vector<status_playerinfo> clients = BuildStatusPlayerInfo(host_client);

		for (j = 0; j < clients.size(); j++) {
			status_playerinfo info = clients[j];

			Host_Status_PrintClient_d(info, isServerside, print);
		}
	}

	void Initialize(GarrysMod::Lua::ILuaBase* LUA)
	{
		lua = static_cast<GarrysMod::Lua::ILuaInterface*>(LUA);

		engine_server = global::engine_loader.GetInterface<IVEngineServer>(INTERFACEVERSION_VENGINESERVER);
		if (engine_server == nullptr)
			LUA->ThrowError("gmsv_statusx: Failed to load required IVEngineServer interface");

		icvar = cvar_binary.GetInterface<ICvar>(CVAR_INTERFACE_VERSION);
		if (icvar == nullptr)
			LUA->ThrowError("gmsv_statusx: Failed to load required ICVar interface");

		SymbolFinder symfinder;

		void* Host_Status_PrintClient = symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			Host_Status_PrintClient_sym,
			Host_Status_PrintClient_symlen
		);

		if (Host_Status_PrintClient == nullptr)
			LUA->ThrowError("gmsv_statusx: Unable to sigscan function Host_Status_PrintClient");

		if (!Host_Status_PrintClient_detour.Create(
			Host_Status_PrintClient, reinterpret_cast<void*>(&Host_Status_PrintClient_d)
		))
			LUA->ThrowError("gmsv_statusx: Unable to detour function Host_Status_PrintClient");

		UTIL_GetCommandClientIndex = reinterpret_cast<UTIL_GetCommandClientIndex_t>(symfinder.ResolveOnBinary(
			server_binary.c_str(),
			UTIL_GetCommandClientIndex_sym,
			UTIL_GetCommandClientIndex_symlen
		));

		if (UTIL_GetCommandClientIndex == nullptr)
			LUA->ThrowError("gmsv_statusx: Unable to sigscan function UTIL_GetCommandClientIndex");

#if defined SYSTEM_WINDOWS
		CSteamGameServerAPIContext** gameserver_context_pointer = reinterpret_cast<CSteamGameServerAPIContext**>(symfinder.ResolveOnBinary(
			server_binary.c_str(),
			SteamGameServerAPIContext_sym,
			SteamGameServerAPIContext_symlen
		));

		if (gameserver_context_pointer == nullptr)
			LUA->ThrowError("gmsv_statusx: Failed to load required CSteamGameServerAPIContext interface pointer.");

		gameserver_context = *gameserver_context_pointer;
#else
		gameserver_context = reinterpret_cast<CSteamGameServerAPIContext*>(symfinder.ResolveOnBinary(
			server_binary.c_str(),
			SteamGameServerAPIContext_sym,
			SteamGameServerAPIContext_symlen
		));
#endif

		if (gameserver_context == nullptr)
			LUA->ThrowError("gmsv_statusx: Failed to load required CSteamGameServerAPIContext interface.");

#if defined SYSTEM_WINDOWS
		server = *reinterpret_cast<IServer**>(symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			IServer_sig,
			IServer_siglen
		));
#else
		server = reinterpret_cast<IServer*>(symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			IServer_sig,
			IServer_siglen
		));
#endif

		if (server == nullptr)
			LUA->ThrowError("gmsv_statusx: Failed to locate IServer");


#if defined SYSTEM_WINDOWS
		local_addr = reinterpret_cast<netadr_t*>((DWORD)global::engine_loader.GetFactory() + 0x777138);
		version_date = reinterpret_cast<const char*>((DWORD)global::engine_loader.GetFactory() + 0x449F30);
		version_build = *reinterpret_cast<int*>((DWORD)global::engine_loader.GetFactory() + 0x433A80);

		pMainOrigin = (void*)((DWORD)global::engine_loader.GetFactory() + 0x2BB940);
#else
		local_addr = reinterpret_cast<netadr_t*>(symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			netLocalAdr_sig,
			netLocalAdr_siglen
		));

		version_date = reinterpret_cast<const char*>(symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			version_sig,
			version_siglen
		));

		version_build = *reinterpret_cast<int*>(symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			buildnumber_sig,
			buildnumber_symlen
		));

		pMainOrigin = symfinder.ResolveOnBinary(
			global::engine_lib.c_str(),
			mainView_sig,
			mainView_siglen
		);
#endif	
		if (local_addr == nullptr)
			LUA->ThrowError("gmsv_statusx: Failed to locate net_local_adr");

		Host_Status_PrintClient_detour.Enable();

		icvar->RegisterConCommand(&status_command);
	}

	void Deinitialize(GarrysMod::Lua::ILuaBase*)
	{
		Host_Status_PrintClient_detour.Destroy();
	}
}