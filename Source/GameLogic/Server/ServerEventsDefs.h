#pragma once

//////////////////////////////////////////////////////////////////////////////
//
// The server's event definitions: IDs, names, severities, message formats,
// and whether an event is logged by default.
//
// Converted once from AGC's AGCEvents.xml (whose events went to the Windows
// event log by default are the ones logged by default here). This file is
// now the source: edit it directly. See ServerEvents.h.
//
// In a format, %Name% is replaced by the event argument of that name, and
// %.SubjectName%, %.SubjectID%, %.ComputerName%, %.Context%, %.ID% and
// %.Time% by the event's own fields; %% is a literal percent sign.
//
//////////////////////////////////////////////////////////////////////////////

enum ServerEventIDs
{

    // AGC/EventLogging
    EventID_DBEventLogStarted = 101,
    EventID_DBEventLogStopped = 102,
    EventID_NTEventLogStarted = 103,
    EventID_NTEventLogStopped = 104,
    EventID_AGCNotInitialized = 105,
    EventID_AGCAssert = 106,

    // AGC/Station
    EventID_StationCreated = 201,
    EventID_StationDeleted = 202,
    EventID_StationChangesSides = 203,

    // AGC/Ship
    EventID_ShipChangesSectors = 301,
    EventID_ShipKilled = 302,
    EventID_ShipUsesRipcord = 303,

    // Allsrv
    AllsrvEventID_Initialize = 2001,
    AllsrvEventID_Terminate = 2002,
    AllsrvEventID_BadFtpPath = 2003,
    AllsrvEventID_PlayerDropped = 2004,
    AllsrvEventID_SessionLost = 2005,
    AllsrvEventID_MsgFromUnknownPlayer = 2006,
    AllsrvEventID_NoTicket = 2010,
    AllsrvEventID_LoginFailed = 2011,
    AllsrvEventID_CreateCharFailed = 2012,
    AllsrvEventID_BadClientVersion = 2013,
    AllsrvEventID_MessageBox = 2014,
    AllsrvEventID_MoneyError = 2015,
    AllsrvEventID_Connecting = 2016,
    AllsrvEventID_ConnectError = 2017,
    AllsrvEventID_NoLobby = 2018,
    AllsrvEventID_HostSession = 2019,
    AllsrvEventID_CantHostSession = 2020,
    AllsrvEventID_StartingService = 2022,
    AllsrvEventID_Assert = 2024,
    AllsrvEventID_ArtChangedInInit = 2025,
    AllsrvEventID_SystemError = 2027,
    AllsrvEventID_LostLobby = 2028,
    AllsrvEventID_GameStarted = 2029,
    AllsrvEventID_GameEnded = 2030,
    AllsrvEventID_GameOver = 2031,
    AllsrvEventID_LatencyExceeded = 2032,
    AllsrvEventID_ConnectedLobby = 2033,
    AllsrvEventID_DisconnectingLobby = 2034,
    AllsrvEventID_DisconnectedLobby = 2035,
    AllsrvEventID_Pause = 2037,
    AllsrvEventID_Continue = 2038,

    // Admin
    EventID_ServerShutdown = 4001,
    EventID_NewSession = 4010,
    EventID_LoginServer = 4020,
    EventID_LogoutServer = 4021,
    EventID_ChatMessage = 4030,
    EventID_AdminPage = 4031,
    EventID_AdminChat = 4032,
    EventID_GameCreated = 4040,
    EventID_GameDestroyed = 4041,
    EventID_GameStateChange = 4042,
    EventID_LoginGame = 4050,
    EventID_LogoutGame = 4051,
    EventID_JoinTeam = 4060,
    EventID_LeaveTeam = 4061,
    EventID_TeamInfoChange = 4062,
    EventID_HackLog = 4070,
    EventID_HackBoot = 4071,
    EventID_HackBootNoPlayer = 4072,
};

#define SERVER_EVENT_DEFS \
    { EventID_DBEventLogStarted, "DBEventLogStarted", EventSeverity_Info, true, "An AGC database event log was started for server %.ComputerName%." }, \
    { EventID_DBEventLogStopped, "DBEventLogStopped", EventSeverity_Info, true, "An AGC database event log was stopped for server %.ComputerName%." }, \
    { EventID_NTEventLogStarted, "NTEventLogStarted", EventSeverity_Info, true, "An AGC NT Event Log was started for server %.ComputerName%." }, \
    { EventID_NTEventLogStopped, "NTEventLogStopped", EventSeverity_Info, true, "An AGC NT Event Log was stopped for server %.ComputerName%." }, \
    { EventID_AGCNotInitialized, "AGCNotInitialized", EventSeverity_Error, true, "An AGC Event was triggered prior to the initialization of the AGC module." }, \
    { EventID_AGCAssert, "AGCAssert", EventSeverity_Error, true, "ASSERTION FAILED!!!\n%File%(%Line%)\n%Text%" }, \
    { EventID_StationCreated, "StationCreated", EventSeverity_Info, false, "Station was created.\nCurrently, this event isn't ever fired." }, \
    { EventID_StationDeleted, "StationDeleted", EventSeverity_Info, false, "Station was deleted.\nCurrently, this event isn't ever fired." }, \
    { EventID_StationChangesSides, "StationChangesSides", EventSeverity_Info, false, "Station changed sides.\nCurrently, this event isn't ever fired." }, \
    { EventID_ShipChangesSectors, "ShipChangesSectors", EventSeverity_Info, false, "%.SubjectName% changed sectors to '%New Sector Name%' (%New Sector%) from '%Old Sector Name%' (%Old Sector%)" }, \
    { EventID_ShipKilled, "ShipKilled", EventSeverity_Info, false, "%.SubjectName% was killed by '%LauncherName%' (%Launcher%)\nAmount = '%Amount%'\nVector1 = '%Vector1%'\nVector2 = '%Vector2%'" }, \
    { EventID_ShipUsesRipcord, "ShipUsesRipcord", EventSeverity_Info, false, "%.SubjectName% used ripcord '%ModelName%'" }, \
    { AllsrvEventID_Initialize, "Initialize", EventSeverity_Info, true, "Allsrv is initializing." }, \
    { AllsrvEventID_Terminate, "Terminate", EventSeverity_Info, true, "Allsrv is shutting down." }, \
    { AllsrvEventID_BadFtpPath, "BadFtpPath", EventSeverity_Error, true, "Invalid or missing ftp path.\nPath = '%Path%'\nRun SrvConfig or check FTP server." }, \
    { AllsrvEventID_PlayerDropped, "PlayerDropped", EventSeverity_Warning, true, "A player was dropped unexpectedly.\nPlayer = '%PlayerName%'\nConnectionID = '%ConxID%'" }, \
    { AllsrvEventID_SessionLost, "SessionLost", EventSeverity_Error, true, "DirectPlay session was lost.\nTime = '%Time%'" }, \
    { AllsrvEventID_MsgFromUnknownPlayer, "MsgFromUnknownPlayer", EventSeverity_Warning, true, "Message received from unknown player.\nPlayerID = '%PlayerID%'\nMessage = '%MsgName%'." }, \
    { AllsrvEventID_NoTicket, "NoTicket", EventSeverity_Warning, true, "Login attempted with no character name." }, \
    { AllsrvEventID_LoginFailed, "LoginFailed", EventSeverity_Warning, true, "Database error is preventing access to the character info." }, \
    { AllsrvEventID_CreateCharFailed, "CreateCharFailed", EventSeverity_Warning, true, "Character could not be created in the database." }, \
    { AllsrvEventID_BadClientVersion, "BadClientVersion", EventSeverity_Warning, true, "Client tried to login with bad version.\nServer version = '%ServerVer%'\nClient version = '%ClientVer%'" }, \
    { AllsrvEventID_MessageBox, "MessageBox", EventSeverity_Warning, true, "%Caption%\n%Message%" }, \
    { AllsrvEventID_MoneyError, "MoneyError", EventSeverity_Warning, true, "Player donated towards something, and now has negative money.\nPlayer = '%PlayerName%'\nAmount given = '%MoneyGiven%'\nNew balance = '%MoneyLeft%'" }, \
    { AllsrvEventID_Connecting, "Connecting", EventSeverity_Info, true, "Game server connecting to lobby.\nLobby = %LobbyServer%" }, \
    { AllsrvEventID_ConnectError, "ConnectError", EventSeverity_Error, true, "Game server failed to connect to lobby." }, \
    { AllsrvEventID_NoLobby, "NoLobby", EventSeverity_Error, true, "No lobby server specified.  Run SrvConfig.\nOnly lobby mode is currently supported." }, \
    { AllsrvEventID_HostSession, "HostSession", EventSeverity_Info, true, "Preparing session for clients." }, \
    { AllsrvEventID_CantHostSession, "CantHostSession", EventSeverity_Error, true, "Cannot host session. Check for correct dplay installation." }, \
    { AllsrvEventID_StartingService, "StartingService", EventSeverity_Info, true, "Game server starting as an NT service." }, \
    { AllsrvEventID_Assert, "Assert", EventSeverity_Error, true, "ASSERTION FAILED!!!\n%File%(%Line%)\n%Error%" }, \
    { AllsrvEventID_ArtChangedInInit, "ArtChangedInInit", EventSeverity_Warning, true, "Art Directory's contents changed during AllSrv's initialization.\nPlease restart the server." }, \
    { AllsrvEventID_SystemError, "SystemError", EventSeverity_Error, true, "%Text%\n%Message%" }, \
    { AllsrvEventID_LostLobby, "LostLobby", EventSeverity_Warning, true, "Lost connection to lobby server." }, \
    { AllsrvEventID_GameStarted, "GameStarted", EventSeverity_Info, true, "Game started." }, \
    { AllsrvEventID_GameEnded, "GameEnded", EventSeverity_Info, true, "Game has ended\nReason = '%Reason%'" }, \
    { AllsrvEventID_GameOver, "GameOver", EventSeverity_Info, true, "Game over (but mission still active until all players out)." }, \
    { AllsrvEventID_LatencyExceeded, "LatencyExceeded", EventSeverity_Warning, true, "Update latency exceded.\nTime now = '%TimeNow%'\nLast upd = '%TimeLast%\nDelta    = %Delta%" }, \
    { AllsrvEventID_ConnectedLobby, "ConnectedLobby", EventSeverity_Info, true, "Game server connected to lobby.\nLobby = %LobbyServer%" }, \
    { AllsrvEventID_DisconnectingLobby, "DisconnectingLobby", EventSeverity_Info, true, "Game server disconnecting from lobby.\nLobby = %LobbServer%" }, \
    { AllsrvEventID_DisconnectedLobby, "DisconnectedLobby", EventSeverity_Warning, true, "Game server disconnected from lobby.\nLobby = %LobbServer%" }, \
    { AllsrvEventID_Pause, "Pause", EventSeverity_Warning, true, "Server paused." }, \
    { AllsrvEventID_Continue, "Continue", EventSeverity_Warning, true, "Server continued." }, \
    { EventID_ServerShutdown, "ServerShutdown", EventSeverity_Info, true, "Server shut down." }, \
    { EventID_NewSession, "NewSession", EventSeverity_Info, false, "New Admin Session Started." }, \
    { EventID_LoginServer, "LoginServer", EventSeverity_Info, false, "%.SubjectName%(%.SubjectID%) logged onto Server." }, \
    { EventID_LogoutServer, "LogoutServer", EventSeverity_Info, false, "%.SubjectName%(%.SubjectID%) logged off Server." }, \
    { EventID_ChatMessage, "ChatMessage", EventSeverity_Info, false, "%.SubjectName% (%Target%): %Message%" }, \
    { EventID_AdminPage, "AdminPage", EventSeverity_Info, false, "%.SubjectName% (%Target%): %Message%" }, \
    { EventID_AdminChat, "AdminChat", EventSeverity_Info, false, "ADMIN (%.SubjectID%) @ %OriginalTime%: %Message%" }, \
    { EventID_GameCreated, "GameCreated", EventSeverity_Info, false, "Game %.SubjectName% (id=%.SubjectID%) was created." }, \
    { EventID_GameDestroyed, "GameDestroyed", EventSeverity_Info, false, "Game %.SubjectName% (%.SubjectID%) was destroyed." }, \
    { EventID_GameStateChange, "GameStateChange", EventSeverity_Info, false, "The parameters of game (id=%.SubjectID% name='%.SubjectName%' were changed." }, \
    { EventID_LoginGame, "LoginGame", EventSeverity_Info, false, "%.SubjectName% logged onto game '%GameName%' (id=%GameID%)." }, \
    { EventID_LogoutGame, "LogoutGame", EventSeverity_Info, false, "%.SubjectName% logged off '%GameName%' (id=%GameID%)." }, \
    { EventID_JoinTeam, "JoinTeam", EventSeverity_Info, false, "%.SubjectName% joined team '%TeamName%' (%Team%)" }, \
    { EventID_LeaveTeam, "LeaveTeam", EventSeverity_Info, false, "%.SubjectName% left team '%TeamName%' (%Team%)" }, \
    { EventID_TeamInfoChange, "TeamInfoChange", EventSeverity_Info, false, "Team '%.SubjectName%' (%.SubjectID%) in game '%GameName%' (%GameID%) changed.\nNew team name = '%NewTeamName%'" }, \
    { EventID_HackLog, "HackLog", EventSeverity_Warning, true, "Possible hack attempt made by %.SubjectName%. Player not booted from the server.\nLine: %Line%\nExpression: %Expr%" }, \
    { EventID_HackBoot, "HackBoot", EventSeverity_Warning, true, "Possible hack attempt made by %.SubjectName%. Player has been booted from the server.\nLine: %Line%\nExpression: %Expr%" }, \
    { EventID_HackBootNoPlayer, "HackBootNoPlayer", EventSeverity_Warning, true, "Possible hack attempt made by an unknown player. Connection has been disconnected.\nLine: %Line%\nExpression: %Expr%" }, \

