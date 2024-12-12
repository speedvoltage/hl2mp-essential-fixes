#include "cbase.h"
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12 // ARM64 architecture value in later SDKs
#endif
#undef GetCommandLine
#undef ReadConsoleInput
#undef RegCreateKey
#undef RegCreateKeyEx
#undef RegOpenKey
#undef RegOpenKeyEx
#undef RegQueryValue
#undef RegQueryValueEx
#undef RegSetValue
#undef RegSetValueEx
#endif

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <sstream>
#include <intrin.h>
#endif

#ifdef LINUX
#include <fstream>
#include <string>
#include <sstream>
#include <sys/utsname.h>
#endif

#include <ctime>
#include <cstring>

#include "tier0/memdbgon.h"

#ifdef _WIN32
extern std::string g_ServerBootTime;
#endif

#ifdef _WIN32
std::string GetCPUName()
{
	int cpuInfo[ 4 ] = { -1 };
	char cpuName[ 0x40 ] = { 0 };

	__cpuid( cpuInfo, 0x80000000 );
	unsigned int nExIds = cpuInfo[ 0 ];

	if ( nExIds >= 0x80000004 )
	{
		__cpuid( ( int* ) cpuInfo, 0x80000002 );
		memcpy( cpuName, cpuInfo, sizeof( cpuInfo ) );
		__cpuid( ( int* ) cpuInfo, 0x80000003 );
		memcpy( cpuName + 16, cpuInfo, sizeof( cpuInfo ) );
		__cpuid( ( int* ) cpuInfo, 0x80000004 );
		memcpy( cpuName + 32, cpuInfo, sizeof( cpuInfo ) );
	}

	return std::string( cpuName );
}

std::string GetWindowsVersion()
{
	typedef LONG( WINAPI* RtlGetVersionFunc )( PRTL_OSVERSIONINFOW );
	HMODULE hMod = GetModuleHandleA( "ntdll.dll" );
	if ( hMod )
	{
		RtlGetVersionFunc rtlGetVersion = ( RtlGetVersionFunc ) GetProcAddress( hMod, "RtlGetVersion" );
		if ( rtlGetVersion )
		{
			RTL_OSVERSIONINFOW osvi = {};
			osvi.dwOSVersionInfoSize = sizeof( osvi );
			if ( rtlGetVersion( &osvi ) == 0 ) // STATUS_SUCCESS
			{
				return "Windows " + std::to_string( osvi.dwMajorVersion ) + "." +
					std::to_string( osvi.dwMinorVersion ) + " (Build " +
					std::to_string( osvi.dwBuildNumber ) + ")";
			}
		}
	}
	return "Unknown Windows Version";
}

// system info
std::string GetWindowsSystemInfo()
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo( &sysInfo );

	MEMORYSTATUSEX memStatus;
	memStatus.dwLength = sizeof( memStatus );
	GlobalMemoryStatusEx( &memStatus );

	std::ostringstream oss;

	// Processor information
	oss << "CPU Architecture: ";
	switch ( sysInfo.wProcessorArchitecture )
	{
	case PROCESSOR_ARCHITECTURE_AMD64:
		oss << "x64 (AMD or Intel)";
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		oss << "ARM";
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		oss << "ARM64";
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		oss << "Intel Itanium-based";
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		oss << "x86";
		break;
	default:
		oss << "Unknown architecture";
		break;
	}
	oss << "\n  Number of Processors: " << sysInfo.dwNumberOfProcessors;

	// Memory information
	oss << "\n  Total Physical Memory: "
		<< ( memStatus.ullTotalPhys / ( 1024 * 1024 ) ) << " MB";
	oss << "\n  Available Physical Memory: "
		<< ( memStatus.ullAvailPhys / ( 1024 * 1024 ) ) << " MB";

	return oss.str();
}
#endif

#ifdef LINUX

std::string GetCPUName()
{
	std::ifstream cpuInfo( "/proc/cpuinfo" );
	std::string line, cpuName;
	if ( cpuInfo.is_open() )
	{
		while ( std::getline( cpuInfo, line ) )
		{
			if ( line.find( "model name" ) != std::string::npos )
			{
				size_t pos = line.find( ':' );
				if ( pos != std::string::npos )
				{
					cpuName = line.substr( pos + 2 );
				}
				break;
			}
		}
	}
	return cpuName.empty() ? "Unknown CPU" : cpuName;
}

std::string GetLinuxKernelVersion()
{
	struct utsname buffer;
	if ( uname( &buffer ) == 0 )
	{
		return std::string( buffer.sysname ) + " " + buffer.release;
	}
	return "Unknown Kernel Version";
}

std::string GetLinuxSystemInfo()
{
	std::ostringstream oss;

	std::string cpuName = GetCPUName();
	oss << "CPU: " << cpuName << "\n";

	std::string kernelVersion = GetLinuxKernelVersion();
	oss << "Kernel: " << kernelVersion << "\n";

	std::ifstream cpuInfo( "/proc/cpuinfo" );
	int processorCount = 0;
	if ( cpuInfo.is_open() )
	{
		std::string line;
		while ( std::getline( cpuInfo, line ) )
		{
			if ( line.find( "processor" ) != std::string::npos )
			{
				processorCount++;
			}
		}
	}
	oss << "Number of Processors: " << processorCount << "\n";

	std::ifstream memInfo( "/proc/meminfo" );
	std::string memTotal, memAvailable;
	if ( memInfo.is_open() )
	{
		std::string line;
		while ( std::getline( memInfo, line ) )
		{
			if ( line.find( "MemTotal:" ) == 0 )
			{
				memTotal = line.substr( line.find( ":" ) + 1 );
			}
			else if ( line.find( "MemAvailable:" ) == 0 )
			{
				memAvailable = line.substr( line.find( ":" ) + 1 );
			}

			if ( !memTotal.empty() && !memAvailable.empty() )
				break;
		}
	}
	if ( !memTotal.empty() )
	{
		oss << "Total Physical Memory: " << memTotal << "\n";
	}
	if ( !memAvailable.empty() )
	{
		oss << "Available Physical Memory: " << memAvailable << "\n";
	}

	return oss.str();
}

std::string GetLinuxDistro()
{
	std::ifstream osRelease( "/etc/os-release" );
	if ( !osRelease.is_open() )
	{
		return "Unknown Linux Distribution";
	}

	std::string line, distro;
	while ( std::getline( osRelease, line ) )
	{
		if ( line.find( "PRETTY_NAME=" ) == 0 )
		{
			// Extract the value after PRETTY_NAME=
			size_t start = line.find( '"' ) + 1;
			size_t end = line.rfind( '"' );
			if ( start != std::string::npos && end != std::string::npos )
			{
				distro = line.substr( start, end - start );
			}
			break;
		}
	}
	return distro.empty() ? "Unknown Linux Distribution" : distro;
}
#endif

#define BASE_YEAR 2024
#define BASE_MONTH 5
#define BASE_DAY 19
int GetBuildNumber()
{
	// Base date
	struct tm baseDate = {};
	baseDate.tm_year = BASE_YEAR - 1900; // tm_year is years since 1900
	baseDate.tm_mon = BASE_MONTH - 1;   // tm_mon is 0-based
	baseDate.tm_mday = BASE_DAY;

	struct tm currentDate = {};
	const char* compileDate = __DATE__;
	char monthStr[ 4 ] = {};
	int day, year;

	sscanf( compileDate, "%3s %d %d", monthStr, &day, &year );

	const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
	const char* pos = strstr( months, monthStr );
	if ( pos )
	{
		currentDate.tm_mon = ( pos - months ) / 3;
	}
	currentDate.tm_year = year - 1900;
	currentDate.tm_mday = day;

	time_t baseTime = mktime( &baseDate );
	time_t currentTime = mktime( &currentDate );

	if ( baseTime == -1 || currentTime == -1 )
	{
		return 0;
	}

	return static_cast< int >( difftime( currentTime, baseTime ) / ( 60 * 60 * 24 ) );
}

#define VERSION	"1.2.5"
CON_COMMAND( sv_version, "Binaries version" )
{
	if ( UTIL_IsCommandIssuedByServerAdmin() )
	{
		Msg( "\n" );
		// must not forget to update the version for the client below as well
		Msg( "Server Binaries - Community Edition Build Info:\n" );

		Msg( "  Binaries version: %s.%d\n", VERSION, GetBuildNumber() ); // May 19 2024
		Msg( "  Compiled on: %s %s\n", __DATE__, __TIME__ );
#ifdef _WIN32
		Msg( "  Binaries running on: Windows\n  %s\n\n", GetWindowsVersion() );
		Msg( "Server System Information:\n  CPU: %s\n  %s\n\n", GetCPUName().c_str(), GetWindowsSystemInfo().c_str() );
		Msg( "Server Information:\n" );
		Msg( "  Server Boot Time: %s\n", g_ServerBootTime.c_str() );
#elif defined(OSX)
		Msg( "  Binaries running on: OSX\n" ); // not really used, so we are just posting its OS name
#elif defined(LINUX)
		Msg( "  Binaries running on: Linux\n  Linux Distro: %s\n\n",
			GetLinuxDistro().c_str() );
		Msg( "Server System Information:\n  %s\n", GetLinuxSystemInfo().c_str() );
#else
		Msg( "Could not determine OS\n" );
#endif
		Msg( "\n" );
		return;
	}

	CBasePlayer* pPlayer = UTIL_GetCommandClient();

	if ( !pPlayer )
		return;
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "\n" );
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "\"Server Binaries - Community Edition\" Build Info:\n" );
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Binaries version: %s.%d\n", VERSION, GetBuildNumber() ) );
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Compiled on: %s %s\n", __DATE__, __TIME__ ) );
#ifdef _WIN32
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Binaries running on: Windows\n  %s\n", GetWindowsVersion() ) );
#elif defined(OSX)
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Binaries running on: OSX\n" );
#elif defined(LINUX)
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Binaries running on: Linux\n  Linux Distro: %s\n  Linux Kernel: %s\n",
		GetLinuxDistro().c_str(),
		GetLinuxKernelVersion().c_str() ) );
#else
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "Could not determine OS\n" );
#endif
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, "\n" );

	return;
}