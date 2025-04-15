/*
  MPRep.cpp - Recognise GetGroupFeelingsTowards during multiplayer.

  Jason Hood, 25 & 26 January, 2011.

  The reputation to determine the political zones and patrol paths is retrieved
  via Reputation::Vibe::GetGroupFeelingsTowards.  This is set by the server, so
  the client doesn't see it, hence everything being neutral.  The plugin
  replaces the call, requesting the stats from the server.  It also enables the
  reputation requirement when purchasing ships (also using this function).  In
  addition, the level requirement is enabled when buying equipment and ships,
  PROVIDED servers patch server.dll, 0112A3, 1D->00 to make the rank work.

  v1.01, 28 March, 2011:
  - fixed Freelancer bug not recognising new ship order.

  v1.02, 6 July, 2022:
  - get Common.dll's base address.

  v1.03, 17 May, 2024:
  - another fix for the ship order (didn't tidy up the FP stack).

  Install:
    Copy MPREP.DLL to the EXE directory and add it to the [Libraries]
    section of EXE\dacom.ini.  Requires and assumes v1.1.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define NAKED	__declspec(naked)
#define STDCALL __stdcall


#define ADDR_FEELINGS ((PDWORD)0x5c63bc)
#define ADDR_STATS    ((PDWORD)(0x53c3e1+1))
#define ADDR_ENTER    ((PDWORD)(0x558986+1))


DWORD dummy;
#define ProtectX( addr, size ) \
  VirtualProtect( addr, size, PAGE_EXECUTE_READWRITE, &dummy );

#define RELOFS( from, to ) \
  *(PDWORD)((DWORD)(from)) = (DWORD)(to) - (DWORD)(from) - 4

#define NEWOFS( from, to, prev ) \
  prev = (DWORD)(from) + *((PDWORD)(from)) + 4; \
  RELOFS( from, to )


DWORD GetGroupFeelingsTowards_Org;
DWORD ReceivePlayerStats_Org;


struct Rep
{
  UINT	faction;
  float reputation;
};

Rep*   vibe;
int    vibe_count;

time_t last;
bool   flag;
bool*  SinglePlayer;


NAKED
void RequestPlayerStats()
{
  __asm {
	mov	flag, 1
	mov	ecx, ds:[0x67ecd0]
	push	0
	mov	eax, esp
	push	4
	push	eax
	mov	eax, [ecx]
	push	ds:[0x673344]
	call	dword ptr [eax+0x124]

	mov	dword ptr [esp], 0
	call	dword ptr [time]
	add	esp, 4
	mov	last, eax
	ret
  }
}


void STDCALL ReceivePlayerStats( Rep* v, int len )
{
  if (len != vibe_count)
  {
    delete[] vibe;
    vibe = new Rep[vibe_count = len];
  }
  memcpy( vibe, v, len * sizeof(Rep) );

  last = time( 0 );
}


NAKED
void ReceivePlayerStats_Hook()
{
  __asm {
	push	[ecx-4]
	push	edx
	call	ReceivePlayerStats
	cmp	flag, 0
	je	done
	mov	flag, 0
	ret
  done:
	jmp	ReceivePlayerStats_Org
	align	16
  }
}


int fac_cmp( const void* key, const void* elem )
{
  return (UINT)key - ((Rep*)elem)->faction;
}


int GetGroupFeelingsTowards( const int&, const UINT& faction, float& rep )
{
  if (time( 0 ) >= last + 10)
  {
    RequestPlayerStats();
    // Due to the nature of this method, this first call misses out.
    // Just use the previous value, or neutral for the first time.
    // It does mean you could buy a ship that perhaps you shouldn't, but I
    // expect the requirement would be for a positive rep, so I'll let it be.
  }

  Rep* v = (Rep*)bsearch( (void*)faction, vibe, vibe_count, sizeof(Rep), fac_cmp );
  rep = (v == 0) ? 0 : v->reputation;
  return 0;
}


NAKED
void GetGroupFeelingsTowards_Hook()
{
  __asm {
	mov	eax, SinglePlayer
	cmp	byte ptr [eax], 0
	je	mp
	jmp	GetGroupFeelingsTowards_Org
  mp:	jmp	GetGroupFeelingsTowards
	align	16
  }
}


void Patch()
{
  ProtectX( ADDR_FEELINGS, 4 );
  ProtectX( ADDR_STATS,    4 );
  ProtectX( ADDR_ENTER,    4 );

  GetGroupFeelingsTowards_Org = *ADDR_FEELINGS;
  *ADDR_FEELINGS = (DWORD)GetGroupFeelingsTowards_Hook;
  NEWOFS( ADDR_STATS, ReceivePlayerStats_Hook, ReceivePlayerStats_Org );

  SinglePlayer = (bool*)((DWORD)GetModuleHandle( "common.dll" ) - 0x6260000 + 0x63ed17c);
}


BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved )
{
  if (fdwReason == DLL_PROCESS_ATTACH)
    Patch();

  return TRUE;
}
