#include <windows.h>
#include <fstream.h>
#include <process.h>	// For Threads
#include "..\\General.h"

namespace Plain_Files_Directory_As_Virtual_Archive_dummy_access_Inner_Functions	//PRIVATE SECTION. THOSE FUNCTIONS ARE NOT SUPPOSED TO BE CALLED FROM ELSEWHERE.
{

#include "PlainDir.h"	//PRIVATE DEFINITIONS

RECURSUBDIR* RecursiveList (RECURSUBDIR* Position, char* Name)
{
	LPWIN32_FIND_DATA FileData;

	if (!Position)	//Opening Root
	{
		Position = (RECURSUBDIR*)malloc(sizeof(RECURSUBDIR)); 
		if (!Position) return NULL;
		Position->Parent = NULL;
		Position->WinSearch = INVALID_HANDLE_VALUE;
		strcpy (Position->CurrentDir, Name);
		strcat (Position->CurrentDir, "\\");
		Position->RootLength = strlen (Position->CurrentDir);
JUSTPRINT("Created "<<Position->CurrentDir);
		return Position;				//No search, just creating a root position.
	}

	FileData = (LPWIN32_FIND_DATA) malloc (sizeof(WIN32_FIND_DATA));	//Save stack size!
	if (Name) {strcpy (Name, Position->CurrentDir); strcat (Name, "*.*");}
	for (;;)
	{
		if (	!Name	//Search closing requested.
			|| Position->WinSearch == INVALID_HANDLE_VALUE && (Position->WinSearch = FindFirstFile(Name, FileData)) == INVALID_HANDLE_VALUE	//couldn't start a new search
			|| !FindNextFile(Position->WinSearch,FileData) )	//could't continue existing search
		{
			if (Position->WinSearch != INVALID_HANDLE_VALUE) FindClose(Position->WinSearch);

			free (FileData);
			RECURSUBDIR* P = Position->Parent;
			free (Position);
			if (P) return RecursiveList (P, Name);	//Return to the parent level;
			else break;
		}

		if ( !strcmp(FileData->cFileName,"..") || !strcmp(FileData->cFileName,".") ) continue;

		if (FileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			RECURSUBDIR* Child = (RECURSUBDIR*)malloc(sizeof(RECURSUBDIR));
//			if (!Child) return NULL;	ToDo!
			Child->Parent = Position;
			Child->WinSearch = INVALID_HANDLE_VALUE;
			Child->RootLength = Position->RootLength;
			strcpy (Child->CurrentDir, Position->CurrentDir);
			strcat (Child->CurrentDir, FileData->cFileName);
			strcat (Child->CurrentDir, "\\");
			free (FileData);
			return RecursiveList (Child, Name);	//Up to a next level!
		}

		strcpy (Name, Position->CurrentDir + Position->RootLength);
		strcat (Name, FileData->cFileName);
		free (FileData);
		return Position;			//Stay on the same level;
	}

	return NULL;
}

BOOL ReadFile (Archive* ArchRaw, fstream *f)
{
	while (1)			//Data copy loop
	{
		int IsState = ArchRaw->State;
		switch (IsState)
		{
			case STATE_FILE_OPENED:		//File is being unpacking into the buffer with proper threads waiting.
				if ( (ArchRaw->BufEnd+1) % (ArchRaw->BuffSize+1) == ArchRaw->BufBeg )	//Buffer is full (actually, one byte is left but we never use it in order to simplify thread sync; Beg=0 and End=BuffSize is the only exception).
				{
					ResetEvent(ArchRaw->WaitForMain);	//Main thread changes State (e. g. to SKIP) or changes BufBeg and only THEN sets Event. So racing conditions are not possible.
					if (ArchRaw->State != IsState || (ArchRaw->BufEnd+1)%ArchRaw->BuffSize != ArchRaw->BufBeg) SetEvent(ArchRaw->WaitForMain);	//False alarm! Main thread requested something new or took some data.
					else { SetEvent(ArchRaw->WaitForUnpacker); WaitForSingleObject (ArchRaw->WaitForMain, INFINITE); }	//Stop wasting CPU and wait until needed!
				}
				if (ArchRaw->BufEnd == ArchRaw->BuffSize && ArchRaw->BufBeg)
				{
					ArchRaw->FileStillFits=FALSE;	//First, warn the main thread we're going to overwrite the beginning.
					ArchRaw->BufEnd=0;		//And THEN prepare to actually do it.
				}
				int B=ArchRaw->BufBeg, E=ArchRaw->BufEnd;
				int S=ArchRaw->BuffSize-E;
				if (B>E) S=min(S,B-1-E);

				if (!S) break;

				f->read ((char*)(ArchRaw->Buffer+E), S);
				S=f->gcount();		//Actual size read

				ArchRaw->BufEnd+=S;
				SetEvent(ArchRaw->WaitForUnpacker);

				f->read ((char*)&S, 1);
				if (f->fail()) return TRUE;
				f->seekg (-1, ios::cur);
			break;
			case STATE_REQUEST_SKIP: return TRUE;
			case STATE_REQUEST_CLOSE_ARCH: return TRUE;
			default:
				cout<<"ŠŽ‹ŽŽŠ Ž‚…‘ˆ‹‘Ÿ!!! "<<IsState<<endl;	//Impossible thing just happened O_O
			return FALSE;
		}
	}

	return FALSE;
}

static void OpenDirTree( void *arglist )
{
	Archive* ArchRaw = (Archive*)(arglist);
	char DirName[MAX_PATH], FoundFile[MAX_PATH];
	RECURSUBDIR* CurDir;
	int IsState, PrefixPos;
	fstream f;

	strcpy_im (DirName, ArchRaw->CurrentFile);
	IsState = strlen (DirName);
	if (!IsState) goto FatalError;
	if (DirName[IsState-1] == '\\') DirName[IsState-1]=0;

	strcpy (FoundFile, DirName);
	strcat (FoundFile, "\\");
	PrefixPos=strlen (FoundFile);

	if (!(CurDir=RecursiveList(NULL, DirName))) goto FatalError;

	ArchRaw->Flags=CAPS_FAST_OPEN | CAPS_INSTANT_OPEN;

	for(ArchRaw->State = IsState = STATE_ARCH_OPENED; ArchRaw->State != STATE_ARCH_CLOSED; IsState = ArchRaw->State)	//Main Loop
	 switch (IsState)
	 {
		case STATE_REQUEST_OPEN_ARCH:
		case STATE_ARCH_CLOSED:
			cout<<"ŠŽ‹ŽŽŠ Ž‚…‘ˆ‹‘Ÿ!!! "<<IsState<<endl;	//Impossible thing just happened O_O
		break;
		case STATE_ARCH_OPENED:	//Main thread hadn't left OpenArchive yet :)
		break;
		case STATE_IDLE:	//No requests made yet.
		case STATE_NO_SUCH_FILE:	//No request made since failure.
			ResetEvent(ArchRaw->WaitForMain);	//Main thread changes State and THEN sets Event. So racing conditions are not possible.
			if (ArchRaw->State != IsState) SetEvent(ArchRaw->WaitForMain);	//False alarm! Main thread requested something between last check and this line.
			else { SetEvent(ArchRaw->WaitForUnpacker); WaitForSingleObject (ArchRaw->WaitForMain, INFINITE); }	//Stop wasting CPU and wait until needed!
		break;
		case STATE_REQUEST_NEXT_FILE:	//After setting this, main thread can't change the value until we give a response.
			if ( !CurDir || !(CurDir=RecursiveList(CurDir, FoundFile+PrefixPos)) )	//Can't read next file name
			{
				ArchRaw->State=STATE_NO_SUCH_FILE;	//If not, simply answer this and wait if main thread ask for something else.
				SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
				break;
			}
			f.open (FoundFile, ios::in|ios::binary);
			if (f.fail())
			{
				ArchRaw->State=STATE_NO_SUCH_FILE;	//If not, simply answer this and wait if main thread ask for something else.
				SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
				break;
			}
			strcpy_im (ArchRaw->CurrentFile, FoundFile+PrefixPos);
			ArchRaw->BufBeg=ArchRaw->BufEnd=0;	//Of course, this strict order! Init first and than change State.
			ArchRaw->FileStillFits=TRUE;
			ArchRaw->State = STATE_FILE_OPENED;
			SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)

			ReadFile(ArchRaw, &f);
			f.close();

		break;
		case STATE_REQUEST_EXACT_FILE:	//Why don't just open the file by it's name? In order to change current name inside the tree and OpenNextFile output, that's why!
			if (CurDir) RecursiveList (CurDir, NULL);	//Close current list (if any). FL_NO_REWIND flag is not supported due to obvious reasons. Most random-access formats shouldn't.
			if (!(CurDir=RecursiveList(NULL, DirName))) goto FatalError; //Reopen as a new one;

			while ( CurDir=RecursiveList(CurDir, FoundFile+PrefixPos) ) if (!stricmp ((char*)(ArchRaw->CurrentFile), FoundFile+PrefixPos)) break;
			if (!CurDir || (f.open (FoundFile, ios::in|ios::binary), f.fail() ) )	//ˆ­®£¤  ‘¨ -- íâ® ¯à®áâ® ¯®í¬  :)
			{
				ArchRaw->State=STATE_NO_SUCH_FILE;	//If not, simply answer this and wait if main thread ask for something else.
				SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
				break;
			}

			ArchRaw->BufBeg=ArchRaw->BufEnd=0;	//Of course, this strict order! Init first and than change State.
			ArchRaw->FileStillFits=TRUE;
			ArchRaw->State = STATE_FILE_OPENED;
			SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
		
			ReadFile(ArchRaw, &f);
			f.close();
		break;
		case STATE_FILE_OPENED:	//The file has been unpacked into a buffer. We returned from ReadFile, some data may remain unread so we should wait until the main thread read it to the end or request skipping the data.
			if (ArchRaw->BufBeg==ArchRaw->BufEnd)	//Already done!
			{
				ArchRaw->State = STATE_IDLE;
				SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
				break;
			}
			ResetEvent(ArchRaw->WaitForMain);	//Main thread changes State and indexes and THEN sets Event. So racing conditions are not possible.
			if (ArchRaw->State != IsState || ArchRaw->BufBeg==ArchRaw->BufEnd) SetEvent(ArchRaw->WaitForMain);	//False alarm! Main thread requested something or finished reading the data.
			else { SetEvent(ArchRaw->WaitForUnpacker); WaitForSingleObject (ArchRaw->WaitForMain, INFINITE); }	//Stop wasting CPU and wait until needed!
		break;
		case STATE_REQUEST_SKIP:	//main thread does not know and should not care if we finished unpacking the file. In this case, we did it so no special operations needed. Just reset the state.
			ArchRaw->State = STATE_IDLE;
			SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
		break;
		case STATE_REQUEST_CLOSE_ARCH:
			if (CurDir) RecursiveList (CurDir, NULL);
			CurDir = NULL;
			ArchRaw->State=STATE_ARCH_CLOSED;
		break;
		default:
			cout<<"ŠŽ‹ŽŽŠ Ž‚…‘ˆ‹‘Ÿ!!! "<<IsState<<endl;	//Impossible thing just happened O_O
		break;
	 }


FatalError:

	while (ArchRaw->State!=STATE_REQUEST_CLOSE_ARCH && ArchRaw->State!=STATE_ARCH_CLOSED)
	{
		ArchRaw->State=STATE_ERROR;
		SetEvent(ArchRaw->WaitForUnpacker);	//release the main thread (if needed)
		Sleep (100);
	}

	if (CurDir) RecursiveList (CurDir, NULL);
	ArchRaw->State=STATE_ARCH_CLOSED;	//After doing this, no access to ArchRaw is allowed. It may be already deallocated by Main.
	_endthread();
}

} using namespace Plain_Files_Directory_As_Virtual_Archive_dummy_access_Inner_Functions; //PUBLIC SECTION. THOSE FUNCTIONS ARE BELONG TO PlainDir.CPP INTERFACE AND SHOULD BE CALLED FROM ELSEWHERE.

void StartRaw (Archive *A)
{
	_beginthread (OpenDirTree, 4096, A);
}
