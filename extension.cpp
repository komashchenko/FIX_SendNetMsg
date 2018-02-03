/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "CDetour/detours.h"
#include <sys/time.h>
/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

FIX_SNM g_FIX_SNM;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_FIX_SNM);

IGameConfig *g_pGameConf = nullptr;
CDetour *g_pCNetChan_SendNetMsg = nullptr;

char *spamer_ip = nullptr;
int count = 0;
long int lastban_sleep;

DETOUR_DECL_MEMBER3(CNetChan_SendNetMsg, bool, INetMessage &, msg, bool, bForceReliable, bool, bVoice)
{
	if(!DETOUR_MEMBER_CALL(CNetChan_SendNetMsg)(msg, bForceReliable, bVoice))
	{
		char *ip = (char *)this + 204;
		if(spamer_ip == nullptr) spamer_ip = ip;
		else if(spamer_ip == ip && lastban_sleep < time(NULL))
		{
			IGamePlayer *player;
			for(int i = 1; i < 65; i++)
			{
				player = playerhelpers->GetGamePlayer(i);
				if(player && strcmp(ip, player->GetIPAddress()) == 0)
				{
					if(++count == 25)
					{
						if(!player->IsInKickQueue())
						{
							lastban_sleep = time(NULL) + 5;
							smutils->LogMessage(myself, "%s (%s | %s) was kicked for spamming SendNetMsg.", player->GetName(), player->GetAuthString(false), ip);
							gamehelpers->AddDelayedKick(player->GetIndex(), player->GetUserId(), "Block spamming SendNetMsg");
						}
						spamer_ip = nullptr;
						count = 0;
					}
					break;
				}
			}
		}
		return false;
	}
	else
	{
		spamer_ip = nullptr;
		count = 0;
	}
	
	return true;
}
 
bool FIX_SNM::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	char conf_error[255];
	
	if(!gameconfs->LoadGameConfigFile("FIX_SendNetMsg", &g_pGameConf, conf_error, sizeof(conf_error)))
	{
		snprintf(error, maxlength, "Could not read FIX_SendNetMsg: %s", conf_error);
		return false;
	}
	
	CDetourManager::Init(g_pSM->GetScriptingEngine(), g_pGameConf);
	
	g_pCNetChan_SendNetMsg = DETOUR_CREATE_MEMBER(CNetChan_SendNetMsg, "CNetChan::SendNetMsg");
	if (!g_pCNetChan_SendNetMsg)
	{
		snprintf(error, maxlength, "Detour failed CNetChan::SendNetMsg");
		return false;
	}
	else g_pCNetChan_SendNetMsg->EnableDetour();
	
	return true;
}

void FIX_SNM::SDK_OnUnload()
{
	if(g_pCNetChan_SendNetMsg) g_pCNetChan_SendNetMsg->Destroy();
	gameconfs->CloseGameConfigFile(g_pGameConf);
}