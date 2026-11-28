//////////////////////////////////////////////////////////////////////////////
//
// The kinds of session the game's hosts offer (FedApplication), each on its
// own UDP port; discovery answers on port + 1. They were DirectPlay
// application GUIDs (guids.h). A public game server is listed by a lobby,
// a private one (the LAN session and the training server) is found by LAN
// broadcast or by address. The lobby server itself is not in this repository.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "messagecore.h"

const FedApplication c_appGameServer        = { 1, 47700 };   // public
const FedApplication c_appGameServerPrivate = { 2, 47700 };   // LAN and training
const FedApplication c_appLobbyClients      = { 3, 47710 };   // the lobby, for clients
const FedApplication c_appLobbyServers      = { 4, 47720 };   // the lobby, for game servers
