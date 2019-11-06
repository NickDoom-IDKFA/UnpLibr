#include <windows.h>
#include <process.h>	// For Threads
#include "..\\General.h"

#include <iostream.h>	// For Debug Log

namespace WinUnRAR_via_Extrenal_DLL_Inner_Functions	//PRIVATE SECTION. THOSE FUNCTIONS ARE NOT SUPPOSED TO BE CALLED FROM ELSEWHERE.
{

#include "RAR_Ext.h"	//PRIVATE DEFINITIONS

//Purely manual DLL call, no UnRAR.lib needed
pRAROPENARCHIVE        pRAROpenArchive       ;
pRAROPENARCHIVEEX      pRAROpenArchiveEx     ;
pRARCLOSEARCHIVE       pRARCloseArchive      ;
pRARREADHEADER         pRARReadHeader        ;
pRARREADHEADEREX       pRARReadHeaderEx      ;
pRARPROCESSFILE        pRARProcessFile       ;
pRARPROCESSFILEW       pRARProcessFileW      ;
pRARSETCALLBACK        pRARSetCallback       ;
pRARSETCHANGEVOLPROC   pRARSetChangeVolProc  ;
pRARSETPROCESSDATAPROC pRARSetProcessDataProc;
pRARSETPASSWORD        pRARSetPassword       ;
pRARGETDLLVERSION      pRARGetDllVersion     ;

int CALLBACK Callback(UINT msg,LPARAM UserData,LPARAM P1,LPARAM P2)
{
	Archive* ArchRAR = (Archive*)(UserData);

	if (msg==UCM_NEEDPASSWORD )//|| msg==UCM_NEEDPASSWORDW)
	{
		if (ArchRAR->PasswordString && ArchRAR->PasswordString[0] && P2>=strlen(ArchRAR->PasswordString))
			strcpy((char*)P1,ArchRAR->PasswordString);
		else	return -1;

		return 1;
	}

	if (msg==UCM_PROCESSDATA)
	 while (1)			//Data copy loop
	 {
		int IsState = ArchRAR->State;

		switch (IsState)
		{
			case STATE_FILE_OPENED:		//File is being unpacking into the buffer with proper threads waiting.
				if (!P2) return 1;	//All data has been copied in the prev. loops
				if ( (ArchRAR->BufEnd+1) % (ArchRAR->BuffSize+1) == ArchRAR->BufBeg )	//Buffer is full (actually, one byte is left but we never use it in order to simplify thread sync; Beg=0 and End=BuffSize is the only exception).
				{
					ResetEvent(ArchRAR->WaitForMain);	//Main thread changes State (e. g. to SKIP) or changes BufBeg and only THEN sets Event. So racing conditions are not possible.
					if (ArchRAR->State != IsState || (ArchRAR->BufEnd+1)%ArchRAR->BuffSize != ArchRAR->BufBeg) SetEvent(ArchRAR->WaitForMain);	//False alarm! Main thread requested something new or took some data.
					else { SetEvent(ArchRAR->WaitForUnpacker); WaitForSingleObject (ArchRAR->WaitForMain, INFINITE); }	//Stop wasting CPU and wait until needed!
				}
				if (ArchRAR->BufEnd == ArchRAR->BuffSize && ArchRAR->BufBeg)
				{
					ArchRAR->FileStillFits=FALSE;	//First, warn the main thread we're going to overwrite the beginning.
					ArchRAR->BufEnd=0;		//And THEN prepare to actually do it.
				}
				int B=ArchRAR->BufBeg, E=ArchRAR->BufEnd, S=P2;
				if (B>E) S=min(S,B-1-E);
				S=min(S,ArchRAR->BuffSize-E);
				if (!S) break;

				memcpy_im (ArchRAR->Buffer+E, (char*)P1, S);
				ArchRAR->BufEnd+=S;
				P1+=S;
				P2-=S;

				SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed). Тут можно сэкономить и взводить только если ситуация реально поменялась (т. е. не просто появлились данные, а появились после того, как их не было), но гарантировать синхронность при этом... в общем, пока есть более актуальные вопросы.
			break;
			case STATE_REQUEST_SKIP:	//Unpacking the rest of the file into nothing.
				ArchRAR->FileStillFits = FALSE;
				return 1;	//toDo: we must return -1 for one file or for whole archive?
			break;
			case STATE_REQUEST_CLOSE_ARCH:
			return -1;
			default:
				cout<<"КОЛОБОК ПОВЕСИЛСЯ!!! "<<IsState<<endl;	//Impossible thing just happened O_O
			break;
		}
	 }

	return 0;
}

static void UnpackRAR( void *arglist )
{
	Archive* ArchRAR = (Archive*)(arglist);
	char ArchName[MAX_PATH], SearchFirstName[MAX_PATH]={0};
	HINSTANCE UnRARDLLhInst;
	RAROpenArchiveDataEx OpenData={ArchName,NULL,RAR_OM_EXTRACT,0,NULL,0,0,0,0,Callback,(long)arglist};
	RARHeaderDataEx      FileData={0};
	HANDLE hRARFile;
	int IsState;
	int Res;

	strcpy_im (ArchName, ArchRAR->CurrentFile);
THREADLOG(101)
	if (!(UnRARDLLhInst = LoadLibrary("unrar.dll"))) goto FatalError;	//На меня нашло сумрачное настроение и я не захотел выносить ничего, связанного со сторонней библиотекой, в основной проект. Т. е. оставить всё в пределах RAR_Ext.obj и его исходников. Поэтому я не стал линковать к проекту UnRAR.lib и элементарно дёргаю DLL вручную.
THREADLOG(102)
	if (!(pRAROpenArchiveEx=(pRAROPENARCHIVEEX)GetProcAddress(UnRARDLLhInst,"RAROpenArchiveEx"))) goto FatalError;
THREADLOG(103)
	if (!(pRARReadHeaderEx =(pRARREADHEADEREX )GetProcAddress(UnRARDLLhInst,"RARReadHeaderEx" ))) goto FatalError;
THREADLOG(104)
	if (!(pRARProcessFileW =(pRARPROCESSFILEW )GetProcAddress(UnRARDLLhInst,"RARProcessFileW" ))) goto FatalError;
THREADLOG(105)
	if (!(pRARCloseArchive =(pRARCLOSEARCHIVE )GetProcAddress(UnRARDLLhInst,"RARCloseArchive" ))) goto FatalError;
THREADLOG(106)
	if (!(hRARFile=(*pRAROpenArchiveEx)(&OpenData))) goto FatalError;
THREADLOG(107)

	ArchRAR->Flags=CAPS_FAST_OPEN;
	if (OpenData.Flags & ROADF_SOLID) ArchRAR->Flags=0;

	for(ArchRAR->State = IsState = STATE_ARCH_OPENED; ArchRAR->State != STATE_ARCH_CLOSED; IsState = ArchRAR->State)	//Main Loop
	 switch (IsState)
	 {
		case STATE_REQUEST_OPEN_ARCH:
		case STATE_ARCH_CLOSED:
			cout<<"КОЛОБОК ПОВЕСИЛСЯ!!! "<<IsState<<endl;	//Impossible thing just happened O_O
		break;
		case STATE_ARCH_OPENED:	//Main thread hadn't left OpenArchive yet :)
		break;
		case STATE_IDLE:	//No requests made yet.
		case STATE_NO_SUCH_FILE:	//No request made since failure.
			*(int*)(SearchFirstName)=0;
			ResetEvent(ArchRAR->WaitForMain);	//Main thread changes State and THEN sets Event. So racing conditions are not possible.
			if (ArchRAR->State != IsState) SetEvent(ArchRAR->WaitForMain);	//False alarm! Main thread requested something between last check and this line.
			else { SetEvent(ArchRAR->WaitForUnpacker); WaitForSingleObject (ArchRAR->WaitForMain, INFINITE); }	//Stop wasting CPU and wait until needed!
		break;
		case STATE_REQUEST_NEXT_FILE:	//After setting this, main thread can't change the value until we give a response.
			*(int*)(SearchFirstName)=0;
			if ( Res=((*pRARReadHeaderEx)(hRARFile, &FileData)) )	//Can't read next file name
			{
THREADLOG(155)
				if (Res!=ERAR_END_ARCHIVE) goto FatalError;	//...because of ERROR!
THREADLOG(156)
				ArchRAR->State=STATE_NO_SUCH_FILE;	//If not, simply answer this and wait if main thread ask for something else.
				SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)

				break;
			}
			else if (FileData.Flags&RHDF_DIRECTORY) break;
			strcpy_im (ArchRAR->CurrentFile, FileData.FileName);
			ArchRAR->BufBeg=ArchRAR->BufEnd=0;	//Of course, this strict order! Init first and than change State.
			ArchRAR->FileStillFits=TRUE;
			ArchRAR->State = STATE_FILE_OPENED;
			SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)

			if ((*pRARProcessFileW)(hRARFile, RAR_TEST, NULL, NULL) ) goto FatalError;
		break;
		case STATE_REQUEST_EXACT_FILE:	//The most complicated one. ToDo: keeping file name history for better search (immediate re-opening archive from beginning if the file is there).
			if ( Res=((*pRARReadHeaderEx)(hRARFile, &FileData)) )	//Can't read next file name
			{
				if (Res!=ERAR_END_ARCHIVE) goto FatalError;	//...because of ERROR!
				if (ArchRAR->Flags & FL_NO_REWIND)		//If not, process the end as requested.
				{
					ArchRAR->State=STATE_NO_SUCH_FILE;
					SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)
					break;
				} else {
					if ( (*pRARCloseArchive)(hRARFile) ) goto FatalError;	//What the fuck, we can't even close it???
					if (!(hRARFile=(*pRAROpenArchiveEx)(&OpenData))) goto FatalError;
					break;	//Try next (first) file unless request has been changed
				}
			} else {
				if (!SearchFirstName[0]) strcpy (SearchFirstName, FileData.FileName);
				else if ( !stricmp (SearchFirstName, FileData.FileName) )	//We met again, arn't we???
				{
					if ((*pRARProcessFileW)(hRARFile, RAR_SKIP, NULL, NULL) ) goto FatalError;
					ArchRAR->State=STATE_NO_SUCH_FILE;
					SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)
					break;
				}
				if (FileData.Flags&RHDF_DIRECTORY || stricmp ((char*)(ArchRAR->CurrentFile), FileData.FileName))
				{
					if ((*pRARProcessFileW)(hRARFile, RAR_SKIP, NULL, NULL) ) goto FatalError;
					break;	//Try next file unless request has been changed
				}
			}

			ArchRAR->BufBeg=ArchRAR->BufEnd=0;	//Of course, this strict order! Init first and than change State.
			ArchRAR->FileStillFits=TRUE;
			ArchRAR->State = STATE_FILE_OPENED;
			SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)

			if ((*pRARProcessFileW)(hRARFile, RAR_TEST, NULL, NULL) ) goto FatalError;
		break;
		case STATE_FILE_OPENED:	//The file has been unpacked into a buffer. We returned from RARProcessFileW, some data may remain unread so we should wait until the main thread read it to the end or request skipping the data.
			*(int*)(SearchFirstName)=0;
			if (ArchRAR->BufBeg==ArchRAR->BufEnd)	//Already done!
			{
				ArchRAR->State = STATE_IDLE;
				SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)
				break;
			}

			ResetEvent(ArchRAR->WaitForMain);	//Main thread changes State and indexes and THEN sets Event. So racing conditions are not possible.
			if (ArchRAR->State != IsState || ArchRAR->BufBeg==ArchRAR->BufEnd) SetEvent(ArchRAR->WaitForMain);	//False alarm! Main thread requested something or finished reading the data.
			else { SetEvent(ArchRAR->WaitForUnpacker); WaitForSingleObject (ArchRAR->WaitForMain, INFINITE); }	//Stop wasting CPU and wait until needed!
		break;
		case STATE_REQUEST_SKIP:	//main thread does not know and should not care if we finished unpacking the file. In this case, we did it so no special operations needed. Just reset the state.
			*(int*)(SearchFirstName)=0;
			ArchRAR->State = STATE_IDLE;
			SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)
		break;
		case STATE_REQUEST_CLOSE_ARCH:
THREADLOG(108)
			(*pRARCloseArchive)(hRARFile);
			if (UnRARDLLhInst) FreeLibrary (UnRARDLLhInst);
			UnRARDLLhInst = NULL;
			ArchRAR->State=STATE_ARCH_CLOSED;	//After doing this, no access to ArchRAR is allowed. It may be already deallocated by Main.
THREADLOG(109)
		break;
		default:
			cout<<"КОЛОБОК ПОВЕСИЛСЯ!!! "<<IsState<<endl;	//Impossible thing just happened O_O
		break;
	 }

THREADLOG(114)

FatalError:

	if (UnRARDLLhInst && pRARCloseArchive && hRARFile) (*pRARCloseArchive)(hRARFile);

THREADLOG(110)
	while (ArchRAR->State!=STATE_REQUEST_CLOSE_ARCH && ArchRAR->State!=STATE_ARCH_CLOSED)
	{
THREADLOG(111)
		ArchRAR->State=STATE_ERROR;
		SetEvent(ArchRAR->WaitForUnpacker);	//release the main thread (if needed)
		Sleep (100);	//Looks weird but it's very reliable for processing a "close on error" case.
THREADLOG(112)
	}
THREADLOG(113)

	if (UnRARDLLhInst) FreeLibrary (UnRARDLLhInst);
	ArchRAR->State=STATE_ARCH_CLOSED;	//After doing this, no access to ArchRAR is allowed. It may be already deallocated by Main.
	_endthread();
}

} using namespace WinUnRAR_via_Extrenal_DLL_Inner_Functions; //PUBLIC SECTION. THOSE FUNCTIONS ARE BELONG TO RAR_Ext.CPP INTERFACE AND SHOULD BE CALLED FROM ELSEWHERE.

void StartRAR (Archive *A)
{
	_beginthread (UnpackRAR, 4096, A);
}
