#pragma once

//////////////////////////////////////////////////////////////////////////////
//
// Training mission 7 runs on a server the client starts for it:
//
//   AllSrv32.exe -training <civ ID> -stopevent <inherited event handle>
//
// The server hosts its LAN session with one game of this name (the
// player's civ against the Bios), starts it when the player has joined, and
// shuts down when the event is set. See trainingscreen.cpp (client) and
// FedSrv.CPP (server).
//
//////////////////////////////////////////////////////////////////////////////

const char c_szTrainingGameName[] = "Training Mission 7";
