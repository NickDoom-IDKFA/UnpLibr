#include <windows.h>
#include <process.h>	// For Threads
#include "..\\General.h"

#include <iostream.h>	// For Debug Log

namespace Good_Old_Zip_Inner_Functions	//PRIVATE SECTION. THOSE FUNCTIONS ARE NOT SUPPOSED TO BE CALLED FROM ELSEWHERE.
{

#include "Zip.h"	//PRIVATE DEFINITIONS


static void UnpackZip( void *arglist )
{
	Archive* ArchZip = (Archive*)(arglist);

	while (ArchZip->State!=STATE_REQUEST_CLOSE_ARCH && ArchZip->State!=STATE_ARCH_CLOSED)
	{
		ArchZip->State=STATE_ERROR;
		SetEvent(ArchZip->WaitForUnpacker);	//release the main thread (if needed)
		Sleep (100);
	}

	ArchZip->State=STATE_ARCH_CLOSED;
	_endthread();
}

} using namespace Good_Old_Zip_Inner_Functions; //PUBLIC SECTION. THOSE FUNCTIONS ARE BELONG TO Zip.CPP INTERFACE AND SHOULD BE CALLED FROM ELSEWHERE.

void StartZip (Archive *A)
{
	_beginthread (UnpackZip, 4096, A);
}
