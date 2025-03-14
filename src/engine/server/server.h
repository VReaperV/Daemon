/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/
// server.h
#ifndef SERVER_H_
#define SERVER_H_

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "sg_api.h"
#include "framework/VirtualMachine.h"
#include "framework/CommonVMServices.h"

//=============================================================================

#define PERS_SCORE        0 // !!! MUST NOT CHANGE, SERVER AND
// GAME BOTH REFERENCE !!!

#define MAX_BPS_WINDOW 20 // NERVE - SMF - net debugging

struct svEntity_t
{
	entityState_t        baseline; // for delta compression of initial sighting
	int                  snapshotCounter; // used to prevent double adding from portal views
};

enum class serverState_t
{
  SS_DEAD, // no map loaded
  SS_LOADING, // spawning level entities
  SS_GAME // actively running
};

struct server_t
{
	serverState_t state;
	bool      restarting; // if true, send configstring changes during SS_LOADING
	int           serverId; // changes each server start
	int           restartedServerId; // serverId before a map_restart
	int             snapshotCounter; // incremented for each snapshot built
	int             timeResidual; // <= 1000 / sv_frame->value
	int             nextFrameTime; // when time > nextFrameTime, process world

	char            *configstrings[ MAX_CONFIGSTRINGS ];
	bool        configstringsmodified[ MAX_CONFIGSTRINGS ];
	svEntity_t      svEntities[ MAX_GENTITIES ];

	// this is apparently just a proxy, this pointer
	// is set to contain the strings that define entities
	// which must be parsed by sgame for spawning map entities,
	// notably.
	const char            *entityParsePoint; // used during game VM init

	// the game virtual machine will update these on init and changes
	sharedEntity_t *gentities;
	int            gentitySize;
	int            num_entities; // current number, <= MAX_GENTITIES

	OpaquePlayerState *gameClients;
	int            gameClientSize; // will be > sizeof(playerState_t) due to game private data

	int            restartTime;
	int            time;

	// NERVE - SMF - net debugging
	int   bpsWindow[ MAX_BPS_WINDOW ];
	int   bpsWindowSteps;
	int   bpsTotalBytes;
	int   bpsMaxBytes;

	int   ubpsWindow[ MAX_BPS_WINDOW ];
	int   ubpsTotalBytes;
	int   ubpsMaxBytes;

	float ucompAve;
	int   ucompNum;
	// -NERVE - SMF
};

struct clientSnapshot_t
{
	int           areabytes;
	byte          areabits[ MAX_MAP_AREA_BYTES ]; // portalarea visibility bits
	OpaquePlayerState ps;
	int           num_entities;
	int           first_entity; // into the circular sv_packet_entities[]
	// the entities MUST be in increasing state number
	// order, otherwise the delta compression will fail
	int messageSent; // time the message was transmitted
	int messageAcked; // time the message was acked
	int messageSize; // used to rate drop packets
};

enum class clientState_t
{
  CS_FREE, // can be reused for a new connection
  CS_ZOMBIE, // client has been disconnected, but don't reuse connection for a couple seconds
  CS_CONNECTED, // has been assigned to a client_t, but no gamestate yet
  CS_PRIMED, // gamestate has been sent, but client hasn't sent a usercmd
  CS_ACTIVE // client is fully in game
};

struct netchan_buffer_t
{
	msg_t                   msg;
	byte                    msgBuffer[ MAX_MSGLEN ];
	char                    lastClientCommandString[ MAX_STRING_CHARS ];
	netchan_buffer_t *next;
};

struct client_t
{
	clientState_t  state;
	char           userinfo[ MAX_INFO_STRING ]; // name, etc

	char           reliableCommands[ MAX_RELIABLE_COMMANDS ][ MAX_STRING_CHARS ];
	int            reliableSequence; // last added reliable message, not necessarily sent or acknowledged yet
	int            reliableAcknowledge; // last acknowledged reliable message
	int            reliableSent; // last sent reliable message, not necessarily acknowledged yet
	int            messageAcknowledge;

	int            gamestateMessageNum; // netchan->outgoingSequence of gamestate

	usercmd_t      lastUsercmd;
	int            lastMessageNum; // for delta compression
	int            lastClientCommand; // reliable client message sequence
	char           lastClientCommandString[ MAX_STRING_CHARS ];
	sharedEntity_t *gentity; // SV_GentityNum(clientnum)
	char           name[ MAX_NAME_LENGTH ]; // extracted from userinfo, high bits masked

	// downloading
	char          downloadName[ MAX_OSPATH ]; // if not empty string, we are downloading
	FS::File*     download; // file being downloaded
	int           downloadSize; // total bytes (can't use EOF because of paks)
	int           downloadCount; // bytes sent
	int           downloadClientBlock; // last block we sent to the client, awaiting ack
	int           downloadCurrentBlock; // current block number
	int           downloadXmitBlock; // last block we xmited
	unsigned char *downloadBlocks[ MAX_DOWNLOAD_WINDOW ]; // the buffers for the download blocks
	int           downloadBlockSize[ MAX_DOWNLOAD_WINDOW ];
	bool      downloadEOF; // We have sent the EOF block
	int           downloadSendTime; // time we last got an ack from the client

	// www downloading
	char     downloadURL[ MAX_OSPATH ]; // the URL we redirected the client to
	bool bWWWDl; // we have a www download going
	bool bWWWing; // the client is doing an ftp/http download
	bool bFallback; // last www download attempt failed, fallback to regular download
	// note: this is one-shot, multiple downloads would cause a www download to be attempted again

	int              deltaMessage; // frame last client usercmd message
	int              nextReliableTime; // svs.time when another reliable command will be allowed
	int              lastPacketTime; // svs.time when packet was last received
	int              lastConnectTime; // svs.time when connection started
	int              nextSnapshotTime; // send another snapshot when svs.time >= nextSnapshotTime
	bool         rateDelayed; // true if nextSnapshotTime was set based on rate instead of snapshotMsec
	int              timeoutCount; // must timeout a few frames in a row so debugging doesn't break
	clientSnapshot_t frames[ PACKET_BACKUP ]; // updates can be delta'd from here
	int              ping;
	int              rate; // bytes / second
	int              snapshotMsec; // requests a snapshot every snapshotMsec unless rate choked
	netchan_t        netchan;
	// TTimo
	// queuing outgoing fragmented messages to send them properly, without udp packet bursts
	// in case large fragmented messages are stacking up
	// buffer them into this queue, and hand them out to netchan as needed
	netchan_buffer_t *netchan_start_queue;
	//% netchan_buffer_t **netchan_end_queue;
	netchan_buffer_t *netchan_end_queue;

	char             pubkey[ RSA_STRING_LENGTH ];

	//bani
	int downloadnotify;
};

//=============================================================================

#define STATFRAMES 100
struct svstats_t
{
	double active;
	double idle;
	int    count;
	int    packets;

	double latched_active;
	double latched_idle;
	int    latched_packets;
};

struct receipt_t
{
	netadr_t adr;
	int      time;
};

// MAX_INFO_RECEIPTS is the maximum number of getstatus+getinfo responses that we send
// in a two second time period.
#define MAX_INFO_RECEIPTS 48

#define SERVER_PERFORMANCECOUNTER_FRAMES  600
#define SERVER_PERFORMANCECOUNTER_SAMPLES 6

// this structure will be cleared only when the game module changes
struct serverStatic_t
{
	bool      initialized; // sv_init has completed

	bool warnedNetworkScopeNotAdvertisable;

	int           time; // will be strictly increasing across level changes

	int           snapFlagServerBit; // ^= SNAPFLAG_SERVERCOUNT every SV_SpawnServer()

	client_t      *clients; // [sv_maxClients.Get()];
	int           numSnapshotEntities; // sv_maxClients.Get()*PACKET_BACKUP*MAX_PACKET_ENTITIES
	int           nextSnapshotEntities; // next snapshotEntities to use
	std::unique_ptr<entityState_t[]> snapshotEntities; // [numSnapshotEntities]
	receipt_t     infoReceipts[ MAX_INFO_RECEIPTS ];

	int       sampleTimes[ SERVER_PERFORMANCECOUNTER_SAMPLES ];
	int       currentSampleIndex;
	int       totalFrameTime;
	int       currentFrameIndex;
	int       serverLoad;
	svstats_t stats;
};

//=============================================================================

class GameVM: public VM::VMBase {
public:
	GameVM();
	void Start();

	void GameStaticInit();
	void GameInit(int levelTime, int randomSeed);
	void GameShutdown(bool restart);
	bool GameClientConnect(char* reason, size_t size, int clientNum, bool firstTime, bool isBot);
	void GameClientBegin(int clientNum);
	void GameClientUserInfoChanged(int clientNum);
	void GameClientDisconnect(int clientNum);
	void GameClientCommand(int clientNum, const char* command);
	void GameClientThink(int clientNum);
	void GameRunFrame(int levelTime);
	NORETURN void BotAIStartFrame(int levelTime);

private:
	virtual void Syscall(uint32_t id, Util::Reader reader, IPC::Channel& channel) override final;
	void QVMSyscall(int syscallNum, Util::Reader& reader, IPC::Channel& channel);

	IPC::SharedMemory shmRegion;

	std::unique_ptr<VM::CommonVMServices> services;
};

//=============================================================================

extern serverStatic_t svs; // persistent server info across maps
extern server_t       sv; // cleared each map
extern GameVM         gvm; // game virtual machine

extern cvar_t         *sv_fps;
extern cvar_t         *sv_timeout;
extern cvar_t         *sv_zombietime;
extern cvar_t         *sv_privatePassword;
extern cvar_t         *sv_allowDownload;
extern Cvar::Range<Cvar::Cvar<int>> sv_maxClients;

extern Cvar::Range<Cvar::Cvar<int>> sv_privateClients;
extern cvar_t         *sv_hostname;
extern cvar_t         *sv_statsURL;
extern cvar_t         *sv_reconnectlimit;
extern cvar_t         *sv_padPackets;
extern cvar_t         *sv_killserver;
extern cvar_t         *sv_mapname;
extern cvar_t         *sv_mapChecksum;
extern cvar_t         *sv_serverid;
extern cvar_t         *sv_maxRate;

extern cvar_t *sv_floodProtect;
extern cvar_t *sv_lanForceRate;

extern cvar_t *sv_showAverageBPS; // NERVE - SMF - net debugging

// TTimo - autodl
extern cvar_t *sv_dl_maxRate;

//fretn
extern cvar_t *sv_fullmsg;

extern Cvar::Range<Cvar::Cvar<int>> sv_networkScope;

//===========================================================

//
// sv_main.c
//
void       SV_FinalCommand( char *cmd, bool disconnect );  // ydnar: added disconnect flag so map changes can use this function as well
void       SV_SendServerCommand( client_t *cl, const char *fmt, ... ) PRINTF_LIKE(2);
void       SV_PrintTranslatedText( const char *text, bool broadcast, bool plural );

void       SV_AddOperatorCommands();
void       SV_RemoveOperatorCommands();

void       SV_NET_Config();

void       SV_Heartbeat_f();
void       SV_MasterHeartbeat( const char *hbname );
void       SV_MasterShutdown();

//
// sv_init.c
//
void SV_UpdateConfigStrings();
void SV_SetConfigstring( int index, const char *val );
void SV_UpdateConfigStrings();
void SV_GetConfigstring( int index, char *buffer, int bufferSize );
void SV_SetConfigstringRestrictions( int index, const clientList_t *clientList );

void SV_SetUserinfo( int index, const char *val );
void SV_GetUserinfo( int index, char *buffer, int bufferSize );
void SV_GetPlayerPubkey( int clientNum, char *pubkey, int size );

void SV_CreateBaseline();

void SV_ChangeMaxClients();
void SV_SpawnServer(std::string pakname, std::string server);

//
// sv_client.c
//
void SV_GetChallenge( const netadr_t& from );

void SV_DirectConnect( const netadr_t& from, const Cmd::Args& args );

void SV_ExecuteClientMessage( client_t *cl, msg_t *msg );
void SV_UserinfoChanged( client_t *cl );

void SV_ClientEnterWorld( client_t *client, usercmd_t *cmd );
void SV_FreeClient( client_t *client );
void SV_DropClient( client_t *drop, const char *reason );

void SV_ExecuteClientCommand( client_t *cl, const char *s, bool clientOK, bool premaprestart );
void SV_ClientThink( client_t *cl, usercmd_t *cmd );

void SV_WriteDownloadToClient( client_t *cl, msg_t *msg );

//
// sv_snapshot.c
//
void SV_AddServerCommand( client_t *client, const char *cmd );
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg );
void SV_SendMessageToClient( msg_t *msg, client_t *client );
void SV_SendClientMessages();
void SV_SendClientSnapshot( client_t *client );

//bani
void SV_SendClientIdle( client_t *client );

//
// sv_sgame.c
//
sharedEntity_t *SV_GentityNum( int num );
OpaquePlayerState *SV_GameClientNum( int num );

svEntity_t     *SV_SvEntityForGentity( sharedEntity_t *gEnt );
void           SV_InitGameProgs();
void           SV_ShutdownGameProgs();
void           SV_RestartGameProgs();

//
// sv_bot.c
//
int  SV_BotAllocateClient();
void SV_BotFreeClient( int clientNum );
bool SV_IsBot( const client_t* client );

int  SV_BotGetConsoleMessage( int client, char *buf, int size );

//
// sv_net_chan.c
//
void     SV_Netchan_Transmit( client_t *client, msg_t *msg );
void     SV_Netchan_TransmitNextFragment( client_t *client );
void     SV_Netchan_FreeQueue( client_t *client );

//bani - cl->downloadnotify
#define DLNOTIFY_REDIRECT 0x00000001 // "Redirecting client ..."
#define DLNOTIFY_BEGIN    0x00000002 // "clientDownload: 4 : beginning ..."
#define DLNOTIFY_ALL      ( DLNOTIFY_REDIRECT | DLNOTIFY_BEGIN )


enum class ServerPrivate
{
	Public,      // Actively advertise, don't refuse anything
	NoAdvertise, // Do not advertise but reply to all out of band messages
	NoStatus,    // Do not advertise nor reply to status out of band messages but allow all connections
};

/*
 * Returns whether the server has a private level equal to or greater than
 * the one provided
 */
bool SV_Private(ServerPrivate level);

namespace Cvar {
template<> std::string GetCvarTypeName<ServerPrivate>();
} // namespace Cvar

bool ParseCvarValue(Str::StringRef value, ServerPrivate& result);

std::string SerializeCvarValue(ServerPrivate value);

#endif /* SERVER_H_ */
