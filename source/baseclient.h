//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ICLIENT_H
#define ICLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include <inetmsghandler.h>
#include "tier0/platform.h"
#include "userid.h"

class IServer;
class INetMessage;

abstract_class IClient : public INetChannelHandler
{
public:
	virtual	~IClient() {}

	virtual int				GetPlayerSlot() const = 0; // returns client slot (usually entity number-1)
	virtual int				GetUserID() const = 0; // unique ID on this server 
	virtual const USERID_t	GetNetworkID() const = 0; // network wide ID
	virtual const char* GetClientName() const = 0;	// returns client name
	virtual INetChannel* GetNetChannel() = 0; // returns client netchannel
	virtual IServer* GetServer() = 0; // returns the object server the client belongs to
	virtual const char* GetUserSetting(const char* cvar) const = 0; // returns a clients FCVAR_USERINFO setting
	virtual const char* GetNetworkIDString() const = 0; // returns a human readable representation of the network id

	// connect client
	virtual void	Connect(const char* szName, int nUserID, INetChannel* pNetChannel, bool bFakePlayer, int clientChallenge) = 0;

	// set the client in a pending state waiting for a new game
	virtual void	Inactivate(void) = 0;

	// Reconnect without dropiing the netchannel
	virtual	void	Reconnect(void) = 0;				// froce reconnect

	// disconnects a client with a given reason
	virtual void	Disconnect(PRINTF_FORMAT_STRING const char* reason, ...) = 0;

	// set/get client data rate in bytes/second
	virtual void	SetRate(int nRate, bool bForce) = 0;
	virtual int		GetRate(void) const = 0;

	// set/get updates/second rate
	virtual void	SetUpdateRate(int nUpdateRate, bool bForce) = 0;
	virtual int		GetUpdateRate(void) const = 0;

	// clear complete object & free all memory 
	virtual void	Clear(void) = 0;

	// returns the highest world tick number acknowledge by client
	virtual int		GetMaxAckTickCount() const = 0;

	// execute a client command
	virtual bool	ExecuteStringCommand(const char* s) = 0;
	// send client a network message
	virtual bool	SendNetMsg(INetMessage& msg, bool bForceReliable = false) = 0;
	// send client a text message
	virtual void	ClientPrintf(PRINTF_FORMAT_STRING const char* fmt, ...) = 0;

	// client has established network channels, nothing else
virtual bool	IsConnected(void) const = 0;
// client is downloading signon data
virtual bool	IsSpawned(void) const = 0;
// client active is ingame, receiving snapshots
virtual bool	IsActive(void) const = 0;
// returns true, if client is not a real player
virtual bool	IsFakeClient(void) const = 0;
// returns true, if client is a HLTV proxy
virtual bool	IsHLTV(void) const = 0;
#if defined( REPLAY_ENABLED )
// returns true, if client is a Replay proxy
virtual bool	IsReplay(void) const = 0;
#else
// !KLUDGE! Reduce number of #ifdefs required
inline bool		IsReplay(void) const { return false; }
#endif
// returns true, if client hears this player
virtual bool	IsHearingClient(int index) const = 0;
// returns true, if client hears this player by proximity
virtual bool	IsProximityHearingClient(int index) const = 0;

virtual void	SetMaxRoutablePayloadSize(int nMaxRoutablePayloadSize) = 0;
};

#endif // ICLIENT_H


/*
#ifndef BASECLIENT_H
#define BASECLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include <const.h>
#include <checksum_crc.h>
#include <iclient.h>
#include <iservernetworkable.h>
#include <bspfile.h>
#include <KeyValues.h>
#include <bitvec.h>
#include <igameevents.h>
#include "smartptr.h"
#include "userid.h"
#include "tier1/bitbuf.h"

abstract_class CBaseClient : public IGameEventListener2, public IClient, public IClientMessageHandler
{
	typedef struct CustomFile_s
	{
		CRC32_t			crc;	//file CRC
		unsigned int	reqID;	// download request ID
	} CustomFile_t;

public:
	CBaseClient();
	virtual ~CBaseClient() {}

public:
	virtual void	FireGameEvent(IGameEvent* event) const = 0;
	virtual int			GetPlayerSlot() const = 0;
	virtual int			GetUserID() const = 0;
	virtual const USERID_t		GetNetworkID() const = 0;
	virtual const char* GetClientName() const = 0;
	virtual INetChannel* GetNetChannel() const = 0;
	virtual IServer* GetServer() const = 0;
	virtual const char* GetUserSetting(const char* cvar) const = 0;
	virtual const char* GetNetworkIDString() const = 0;

	virtual	void	Connect(const char* szName, int nUserID, INetChannel* pNetChannel, bool bFakePlayer);
	virtual	void	Inactivate(void);
	virtual	void	Reconnect(void);
	virtual	void	Disconnect(const char* reason, ...);

	virtual	void	SetRate(int nRate, bool bForce);
	virtual	int		GetRate(void) const;

	virtual void	SetUpdateRate(int nUpdateRate, bool bForce);
	virtual int		GetUpdateRate(void) const;

	virtual void	Clear(void);
	virtual void	DemoRestart(void); // called when client started demo recording

	virtual	int		GetMaxAckTickCount() const = 0;

	virtual bool	ExecuteStringCommand(const char* s);
	virtual bool	SendNetMsg(INetMessage& msg, bool bForceReliable = false);

	virtual void	ClientPrintf(const char* fmt, ...);

	virtual	bool	IsConnected(void) const = 0;
	virtual	bool	IsSpawned(void) const = 0;
	virtual	bool	IsActive(void) const = 0;
	virtual	bool	IsFakeClient(void) const { return m_bFakePlayer; };
	virtual	bool	IsHLTV(void) const { return m_bIsHLTV; }
	virtual	bool	IsHearingClient(int index) const { return false; };
	virtual	bool	IsProximityHearingClient(int index) const { return false; };

	virtual void	SetMaxRoutablePayloadSize(int nMaxRoutablePayloadSize);

public: // IClientMessageHandlers

	PROCESS_NET_MESSAGE(Tick);
	PROCESS_NET_MESSAGE(StringCmd);
	PROCESS_NET_MESSAGE(SetConVar);
	PROCESS_NET_MESSAGE(SignonState);

	PROCESS_CLC_MESSAGE(ClientInfo);
	PROCESS_CLC_MESSAGE(BaselineAck);
	PROCESS_CLC_MESSAGE(ListenEvents);
	PROCESS_CLC_MESSAGE(CmdKeyValues);

	virtual void	ConnectionStart(INetChannel* chan);

public:

	virtual	bool	UpdateAcknowledgedFramecount(int tick);
	virtual bool	ShouldSendMessages(void);
	virtual void	UpdateSendState(void);

	virtual bool	FillUserInfo(void);
	virtual void	UpdateUserSettings();
	virtual bool	SetSignonState(int state, int spawncount);
	virtual void	WriteGameSounds(bf_write& buf);

	virtual void*   GetDeltaFrame(int nTick);
	virtual void	SendSnapshot(void* pFrame);
	virtual bool	SendServerInfo(void);
	virtual bool	SendSignonData(void);
	virtual void	SpawnPlayer(void);
	virtual void	ActivatePlayer(void);
	virtual void	SetName(const char* name);
	virtual void	SetUserCVar(const char* cvar, const char* value);
	virtual void	FreeBaselines();
	virtual bool	IgnoreTempEntity(void* event);

	virtual void			SetSteamID(const CSteamID& steamID);

	virtual bool			IsTracing() const;
	virtual void			SetTraceThreshold(int nThreshold);
	virtual void			TraceNetworkData(bf_write& msg, char const* fmt, ...);
	virtual void			TraceNetworkMsg(int nBits, char const* fmt, ...);

	virtual bool			IsFullyAuthenticated(void) { return m_bFullyAuthenticated; }
	virtual void			SetFullyAuthenticated(void) { m_bFullyAuthenticated = true; }

private:

	virtual void			OnRequestFullUpdate();


public:

	// Array index in svs.clients:
	int				m_nClientSlot;
	// entity index of this client (different from clientSlot+1 in HLTV mode):
	int				m_nEntityIndex;

	int				m_UserID;			// identifying number on server
	char			m_Name[MAX_PLAYER_NAME_LENGTH];			// for printing to other people
	char			m_GUID[SIGNED_GUID_LEN + 1]; // the clients CD key

	USERID_t		m_NetworkID;         // STEAM assigned userID ( 0x00000000 ) if none
	CSteamID* m_SteamID;			// This is allocated when the client is authenticated, and NULL until then.

	uint32			m_nFriendsID;		// client's friends' ID
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];

	KeyValues* m_ConVars;			// stores all client side convars
	bool			m_bConVarsChanged;	// true if convars updated and not changes process yet
	bool			m_bSendServerInfo;	// true if we need to send server info packet to start connect
	void* m_Server;			// pointer to server object
	bool			m_bIsHLTV;			// if this a HLTV proxy ?

	// Client sends this during connection, so we can see if
	//  we need to send sendtable info or if the .dll matches
	CRC32_t			m_nSendtableCRC;

	// a client can have couple of cutomized files distributed to all other players
	CustomFile_t	m_nCustomFiles[MAX_CUSTOM_FILES];
	int				m_nFilesDownloaded;	// counter of how many files we downloaded from this client

	//===== NETWORK ============
	INetChannel* m_NetChannel;		// The client's net connection.
	int				m_nSignonState;		// connection state
	int				m_nDeltaTick;		// -1 = no compression.  This is where the server is creating the
										// compressed info from.
	int				m_nSignonTick;		// tick the client got his signon data
	CSmartPtr<void*, CRefCountAccessorLongName> m_pLastSnapshot;	// last send snapshot

	void* m_pBaseline;			// current entity baselines as a snapshot
	int				m_nBaselineUpdateTick;	// last tick we send client a update baseline signal or -1
	CBitVec<MAX_EDICTS>	m_BaselinesSent;	// baselines sent with last update
	int				m_nBaselineUsed;		// 0/1 toggling flag, singaling client what baseline to use


	// This is used when we send out a nodelta packet to put the client in a state where we wait 
	// until we get an ack from them on this packet.
	// This is for 3 reasons:
	// 1. A client requesting a nodelta packet means they're screwed so no point in deluging them with data.
	//    Better to send the uncompressed data at a slow rate until we hear back from them (if at all).
	// 2. Since the nodelta packet deletes all client entities, we can't ever delta from a packet previous to it.
	// 3. It can eat up a lot of CPU on the server to keep building nodelta packets while waiting for
	//    a client to get back on its feet.
	int				m_nForceWaitForTick;

	bool			m_bFakePlayer;		// JAC: This client is a fake player controlled by the game DLL
	bool		   m_bReceivedPacket;	// true, if client received a packet after the last send packet

	bool			m_bFullyAuthenticated;

	// The datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.

	// Time when we should send next world state update ( datagram )
	double         m_fNextMessageTime;
	// Default time to wait for next message
	float          m_fSnapshotInterval;

	enum
	{
		SNAPSHOT_SCRATCH_BUFFER_SIZE = 32768,
	};

	unsigned int		m_SnapshotScratchBuffer[SNAPSHOT_SCRATCH_BUFFER_SIZE / 4];

private:
	void				StartTrace(bf_write& msg);
	void				EndTrace(bf_write& msg);


	void*	m_Trace;
};



#endif // BASECLIENT_H
*/