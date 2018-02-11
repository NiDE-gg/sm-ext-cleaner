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

#include <sourcemod_version.h>
#include <tier0/dbg.h>
#include "CDetour/detours.h"
#include "extension.h"

#if SOURCE_ENGINE > SE_LEFT4DEAD2
#define WINDOWS_SIGNATURE "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x14\x8B\x45\x08\x53\x56\x57\x8B\xF9"
#define WINDOWS_SIG_LENGTH 17
#define LINUX_SIGNATURE "_ZN14CLoggingSystem9LogDirectEi17LoggingSeverity_t5ColorPKc"
#define MAC_SIGNATURE "_ZN14CLoggingSystem9LogDirectEi17LoggingSeverity_t5ColorPKc"

#elif SOURCE_ENGINE == SE_LEFT4DEAD2
#define WINDOWS_SIGNATURE "\x55\x8B\xEC\x83\xEC\x08\xE8\x2A\x2A\x2A\x2A\x8B\x48\x08\x89\x0D"
#define WINDOWS_SIG_LENGTH 16
#define LINUX_SIGNATURE "_Z17LoggingSystem_Logi17LoggingSeverity_t5ColorPKcz"
#define MAC_SIGNATURE "_Z17LoggingSystem_Logi17LoggingSeverity_t5ColorPKcz"
#endif

Cleaner g_Cleaner;
SMEXT_LINK(&g_Cleaner);

CDetour *g_pDetour = NULL;

char **g_ppStrings = NULL;
int g_Strings = 0;

#if SOURCE_ENGINE >= SE_LEFT4DEAD2
DETOUR_DECL_MEMBER4(Detour_LogDirect, LoggingResponse_t, LoggingChannelID_t, channelID, LoggingSeverity_t, severity, Color, color, const tchar *, pMessage)
{
	for(int i = 0; i < g_Strings; i++)
	{
		if(strstr(pMessage, g_ppStrings[i]) != 0)
			return LR_CONTINUE;
	}
	return DETOUR_MEMBER_CALL(Detour_LogDirect)(channelID, severity, color, pMessage);
}
#else
DETOUR_DECL_STATIC2(Detour_DefSpew, SpewRetval_t, SpewType_t, channel, char *, text)
{
	for(int i = 0; i < g_Strings; i++)
	{
		if(strstr(text, g_ppStrings[i]) != 0)
			return SPEW_CONTINUE;
	}
	return DETOUR_STATIC_CALL(Detour_DefSpew)(channel, text);
}
#endif

bool Cleaner::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	g_pDetour = NULL;
	g_ppStrings = NULL;
	g_Strings = 0;

	CDetourManager::Init(g_pSM->GetScriptingEngine(), 0);

	char szPath[256];
	g_pSM->BuildPath(Path_SM, szPath, sizeof(szPath), "configs/cleaner.cfg");

	FILE *pFile = fopen(szPath, "r");
	if(pFile == NULL)
	{
		snprintf(error, maxlength, "Could not read configs/cleaner.cfg.");
		return false;
	}

	int Lines = 0;
	char aStr[256];
	while(!feof(pFile))
	{
		fgets(aStr, sizeof(aStr), pFile);
		Lines++;
	}

	rewind(pFile);

	g_ppStrings = (char **)malloc(Lines * sizeof(char **));
	memset(g_ppStrings, 0, Lines * sizeof(char **));

	while(!feof(pFile))
	{
		g_ppStrings[g_Strings] = (char *)malloc(256 * sizeof(char *));
		fgets(g_ppStrings[g_Strings], 255, pFile);

		int Len = strlen(g_ppStrings[g_Strings]);
		if(g_ppStrings[g_Strings][Len - 1] == '\r' || g_ppStrings[g_Strings][Len - 1] == '\n')
			g_ppStrings[g_Strings][Len - 1] = 0;

		if(g_ppStrings[g_Strings][Len - 2] == '\r')
			g_ppStrings[g_Strings][Len - 2] = 0;

		Len = strlen(g_ppStrings[g_Strings]);
		if(!Len)
		{
			free(g_ppStrings[g_Strings]);
			g_ppStrings[g_Strings] = NULL;
		}
		else
			g_Strings++;
	}
	fclose(pFile);

	if(!g_Strings)
	{
		SDK_OnUnload();

		snprintf(error, maxlength, "Config is empty, disabling extension.");
		return false;
	}

#if SOURCE_ENGINE >= SE_LEFT4DEAD2
#ifdef PLATFORM_WINDOWS
	HMODULE pTier0 = GetModuleHandle("tier0.dll");
	void *pFn = memutils->FindPattern(pTier0, WINDOWS_SIGNATURE, WINDOWS_SIG_LENGTH);
#elif defined PLATFORM_LINUX
	void *pTier0 = dlopen("libtier0.so", RTLD_NOW);
	void *pFn = memutils->ResolveSymbol(pTier0, LINUX_SIGNATURE);
	dlclose(pTier0);
#elif defined PLATFORM_APPLE
	void *pTier0 = dlopen("libtier0.dylib", RTLD_NOW);
	void *pFn = memutils->ResolveSymbol(pTier0, MAC_SIGNATURE);
	dlclose(pTier0);
#endif
	if(!pFn)
	{
		snprintf(error, maxlength, "Failed to find signature. Please contact the author.");
		return false;
	}
#endif

#if SOURCE_ENGINE >= SE_LEFT4DEAD2
	g_pDetour = DETOUR_CREATE_MEMBER(Detour_LogDirect, pFn);
#else
	g_pDetour = DETOUR_CREATE_STATIC(Detour_DefSpew, (void *)GetSpewOutputFunc());
#endif

	if(g_pDetour == NULL)
	{
		snprintf(error, maxlength, "Failed to initialize the detours. Please contact the author.");
		return false;
	}

	g_pDetour->EnableDetour();

	return true;
}

void Cleaner::SDK_OnUnload()
{
	if(g_pDetour != NULL)
	{
		g_pDetour->Destroy();
		g_pDetour = NULL;
	}

	if(g_ppStrings != NULL)
	{
		for(int i = 0; i < g_Strings; i++)
		{
			free(g_ppStrings[i]);
			g_ppStrings[i] = NULL;
		}
		free(g_ppStrings);
		g_ppStrings = NULL;
	}
}
