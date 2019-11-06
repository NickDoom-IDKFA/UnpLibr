//Non-compressed archive support (not to be confused with Raw Directory Access AKA Stub Module).
//.TAR, .ISO, .WAD etc. are processed by this module.

#include <windows.h>
#include <fstream.h>
#include <process.h>	// For Threads
#include "..\\General.h"

namespace Non_Compressed_Archive_Inner_Functions	//PRIVATE SECTION. THOSE FUNCTIONS ARE NOT SUPPOSED TO BE CALLED FROM ELSEWHERE.
{

#include "None.h"	//PRIVATE DEFINITIONS

static void OpenNone( void *arglist )	//Stub
{
	Archive* ArchNone = (Archive*)(arglist);

	while (ArchNone->State!=STATE_REQUEST_CLOSE_ARCH && ArchNone->State!=STATE_ARCH_CLOSED)
	{
		ArchNone->State=STATE_ERROR;
		SetEvent(ArchNone->WaitForUnpacker);	//release the main thread (if needed)
		Sleep (100);
	}

	ArchNone->State=STATE_ARCH_CLOSED;
	_endthread();
}

} using namespace Non_Compressed_Archive_Inner_Functions; //PUBLIC SECTION. THOSE FUNCTIONS ARE BELONG TO None.CPP INTERFACE AND SHOULD BE CALLED FROM ELSEWHERE.

void StartNone (Archive *A)
{
	_beginthread (OpenNone, 4096, A);
}
