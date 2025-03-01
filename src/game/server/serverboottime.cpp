#include "cbase.h"
#include "serverboottime.h"
#include <ctime>
#include <cstdio>

#include "tier0/memdbgon.h"

#ifdef LINUX
static char serverBootTimeBuffer[ 80 ] = "Unknown";

void InitializeServerBootTime()
{
	std::time_t now = std::time( nullptr );
	if ( std::strftime( serverBootTimeBuffer, sizeof( serverBootTimeBuffer ), "%Y-%m-%d %H:%M:%S", std::localtime( &now ) ) )
	{
		// Time successfully formatted into serverBootTimeBuffer
	}
	else
	{
		Msg( "Failed to initialize server boot time.\n" );
	}
}

const char *GetServerBootTime()
{
	return serverBootTimeBuffer;
}
#endif // LINUX