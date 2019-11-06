#include <windows.h>
#include <process.h>	// For Threads
#include <iostream.h>	// For Debug Log

	//Unpack Library Main file (for Win32 / DLL)

#include "..\General.h"

#ifdef DEBUGTHREADS
volatile char ThreadLog[16*1024*1024]={0};
volatile int  ThreadLogged=0;
#endif

extern "C" BOOL __export __syscall CloseArchive(void* Arc)	//Closes the archive and deallocates memory.
{
	Archive* A=(Archive*)(Arc);
	int IsState;

	for(A->State = IsState = STATE_REQUEST_CLOSE_ARCH; A->State != STATE_ARCH_CLOSED; IsState = A->State)	// We can't use Events because Unpacker can't set 'em because Unpacker can't be sure we hadn't deallocate 'em yet!
	{
		SetEvent(A->WaitForMain);
		if (IsState != STATE_REQUEST_CLOSE_ARCH && IsState != STATE_ARCH_CLOSED) A->State = STATE_REQUEST_CLOSE_ARCH;	//This is possible only in case of thread desync (we written request right before Unpacker updated the State so it didn't notice the problem).
	}

	CloseHandle(A->WaitForMain);
	CloseHandle(A->WaitForUnpacker );
	free ((char*)(A->Buffer));
	free (A->PasswordString);
	free (A);

#ifdef DEBUGTHREADS
	cout<<"Log started!"<<endl;
	for (int i=0; i<ThreadLogged; i++)
	{
		if (!ThreadLog[i])
		{
			cout<<"Log Access Conflict! Next line is probably lost."<<endl;
			continue;
		}
		for (int j=0;j<ThreadLog[i]/100;j++)
			cout<<" ";
		cout<<(int)(ThreadLog[i])<<endl;
	}
	cout<<"Log ended!"<<endl;
	ThreadLogged=0;
#endif

	return TRUE;	//Success
}

extern "C" void * __export __syscall OpenArchive(char* Name, int BuffSize, unsigned long* Flags, char* Password)	//Opens an archive. Archive type will be auto-detected by extension. To access uncompressed files, set Name to a directory containing those files.
{
	Archive* A;
	char Ext[_MAX_EXT];
	unsigned long Attr;

	Attr=GetFileAttributes(Name);
	if (Attr == 0xFFFFFFFF) return NULL;

	//First of all, allocate all required structures;

	A = (Archive*) malloc (sizeof(Archive));
	if (!A) return NULL;
	memset (A, 0, sizeof(Archive));
	if (Password && *Password)
	{
		A->PasswordString = (char*) malloc (strlen (Password) + 1);
		if (!A->PasswordString)
		{
			free (A);
			return NULL;
		}
		strcpy (A->PasswordString, Password);
	}
	A->WaitForMain = CreateEvent(NULL, TRUE, FALSE, NULL);	//An unnamed, manual-reset event for this process (no child).
	if (!A->WaitForMain)
	{
		free (A->PasswordString);
		free (A);
		return NULL;
	}
	A->WaitForUnpacker  = CreateEvent(NULL, TRUE, FALSE, NULL);	//An unnamed, manual-reset event for this process (no child).
	if (!A->WaitForUnpacker )
	{
		CloseHandle(A->WaitForMain);
		free (A->PasswordString);
		free (A);
		return NULL;
	}
	A->BuffSize=BuffSize;
	if (BuffSize) 
	{
		A->Buffer = (unsigned char*) malloc (BuffSize);
		if (!A->Buffer)
		{
			CloseHandle(A->WaitForMain);
			CloseHandle(A->WaitForUnpacker );
			free (A->PasswordString);
			free (A);
			return NULL;
		}
	}
	strcpy_im (A->CurrentFile, Name);		//First file to open is the archive itself.

	//Now let's determine the archive type and spawn a proper processing thread for it.

	_splitpath( Name, NULL, NULL, NULL, Ext);

	if (Attr&FILE_ATTRIBUTE_DIRECTORY)	//The "archive" is a directory. Dummy direct access treating it as a virtual archive. Useful for pre-caching files from slow network resources and for listing subdir. tree.
		StartRaw(A);
	else if (!stricmp(Ext, ".rar"))	//.RAR archives. Uses free external UnRAR.dll.
		StartRAR(A);
//	else if (!stricmp(Ext, ".7z"))	//.7zip archives.
//		Start7z(A);
//	else if (!stricmp(Ext, ".zip"))	//.zip archives.
//		StartZip(A);
//	else if (!stricmp(Ext, ".tar"))	//.tar (uncompressed)
//		StartTar(A);
//	else if (!stricmp(Ext, ".iso"))	//.iso (uncompressed)
//		StartISO(A);
//	else if (!stricmp(Ext, ".tgz") || !stricmp(Ext, ".gz"))	//.gz compression, may content .tar, .iso etc.
//		StartGZ(A);
//	else if (!stricmp(Ext, ".txz") || !stricmp(Ext, ".xz"))	//.xz compression, may content .tar, .iso etc.
//		StartXZ(A);
	else			//No proper thread for requested archive!
	{
		CloseHandle(A->WaitForMain);
		CloseHandle(A->WaitForUnpacker );
		free ((char*)(A->Buffer));
		free (A->PasswordString);
		free (A);
		return NULL;
	}

	//Now let's wait for the thread.
	while (A->State != STATE_ARCH_OPENED)
		if (A->State == STATE_ERROR)
		{
			CloseArchive(A);
			return NULL;
		}

	if (Flags) *Flags=A->Flags;	//Reports the archive capabilities (fast random file access vs solid archive etc).
	A->State = STATE_IDLE;

	return (void*)(A);	//Returns the archive ptr
}

extern "C" BOOL __export __syscall OpenNextFile (void* Arc, char* FileName)	//Bypasses the remaining content of the current file (if any) and begins unpacking the next file in separate thread. Pre-caches the file immediately until either file or free buffer space ends.
{
	Archive* A=(Archive*)(Arc);

	//Exactly in this order. STATE_REQUEST_EXACT_FILE may change to STATE_NO_SUCH_FILE if search is failed right now.
	if (A->State == STATE_REQUEST_EXACT_FILE || A->State == STATE_NO_SUCH_FILE) A->State = STATE_REQUEST_SKIP;	//Abort last bgnd search. STATE_REQUEST_SKIP is a failproof choice, it'll cause both states (and STATE_FILE_OPENED, too) to finish properly.

	while (A->State == STATE_FILE_OPENED || A->State == STATE_REQUEST_SKIP)
	{
		A->State = STATE_REQUEST_SKIP;
		SetEvent (A->WaitForMain);

		ResetEvent(A->WaitForUnpacker);	//Unpacker thread changes State and THEN sets Event. So racing conditions are not possible.
		if (A->State != STATE_REQUEST_SKIP) SetEvent(A->WaitForUnpacker);	//False alarm! Unpacker thread reported reaching IDLE state between last check and this line.
		else { SetEvent(A->WaitForMain); WaitForSingleObject (A->WaitForUnpacker, INFINITE); }	//Stop wasting CPU and wait until needed!
	}
	
	if (A->State != STATE_IDLE) return FALSE;	//Something went very wrong: instead of reaching IDLE state, Unpacker reached ERROR due to fatal archive error.

	A->State = STATE_REQUEST_NEXT_FILE;
	SetEvent (A->WaitForMain);
	while (A->State == STATE_REQUEST_NEXT_FILE)
	{
		ResetEvent(A->WaitForUnpacker);	//Unpacker thread changes State and THEN sets Event. So racing conditions are not possible.
		if (A->State != STATE_REQUEST_NEXT_FILE) SetEvent(A->WaitForUnpacker);	//False alarm! Unpacker thread reported reaching OPENED state between last check and this line.
		else { SetEvent(A->WaitForMain); WaitForSingleObject (A->WaitForUnpacker, INFINITE); }	//Stop wasting CPU and wait until needed!
	}

	if (A->State == STATE_FILE_OPENED)
	{
		if (FileName) strcpy_im (FileName, A->CurrentFile);	//Next file name. It's OK to call with NULL if client does not care about name!
		return TRUE;	//File exists
	}
	else if (A->State == STATE_NO_SUCH_FILE)
	{
		A->State = STATE_IDLE;			//We still can use SeekFile!
		SetEvent(A->WaitForMain);
		return FALSE;
	}
	else if (A->State == STATE_IDLE)	//Always checked last (AFTER unpacker can possibly change STATE_FILE_OPENED to STATE_IDLE because the file is empty).
	{
//cout<<(char*)(A->CurrentFile)<<" is Empty!!!"<<endl;
		if (FileName) strcpy_im (FileName, A->CurrentFile);	//Next file name. It's OK to call with NULL if client does not care about name!
		return TRUE;	//File exists
	}

	return FALSE;	//Fatal errors, closed archives etc.
}

extern "C" BOOL __export __syscall SeekFile (void* Arc, char* FileName, unsigned long Flags)	//Bypasses the remaining content of the current file (if any) and winds the archive until the requested file is found. Re-starts from beginning if needed.
{
	//Умная открывалка. Если задан файл с тем же именем, что текущий, плюс мы знаем, что текущий в буфере весь и весь влез -- вообще ничего делать не надо, просто индекс опять в начало буфера переставить.
	//Солиды: если файл уже был в истории файлов -- начинаем листать архив сначала, если такого файла не было -- продолжаем листать до конца.
	//Не солиды: штатными средствами находим файл, доступ-заглушка к плейнам (т. е. неупакованным) будет отличным полигоном для тестов.
	//Флаги могут включать, например, "открывать только при наличии быстрого доступа", "перематывать только вперёд", самый адок -- "искать в бэкграунде".

	//Берём на себя случай, если файл помещается в буфере и совпадает имя. Остальное переваливаем на тред.

	//If the re-opening of current file is requested, we don't event check Flags (it's the fastest way, it's always allowed).
	//Also, it's done by Main thread, not by Unpacker (it's nothing to unpack in this case).

	Archive* A=(Archive*)(Arc);
	if ( A->FileStillFits && !stricmp ( FileName, (char*)(A->CurrentFile) )  )	//A->CurrentFile is allowed to change only on {STATE_REQUEST_NEXT_FILE to STATE_FILE_OPENED} which is obviously not here.
	{
		A->BufBeg = 0;	//Yep, that's that simple. Even I still can't believe it. If Unpacker notice the change, we're at time. If not, no problem either way, it'll reset the Fits flag.
		while (WaitForSingleObject (A->WaitForMain, 0)==WAIT_OBJECT_0) if (A->State==STATE_ERROR) return FALSE;	//Sooner or later, Unpacker will stop either way: buffer is full, file is ended or it uses old Begin value and overwrites the beginning.
		SetEvent (A->WaitForMain);	//Be sure Unpacker noticed the Beg has been changed!
		while (WaitForSingleObject (A->WaitForMain, 0)==WAIT_OBJECT_0) if (A->State==STATE_ERROR) return FALSE;	//After it stopped again, that means it's OK now.
		if (A->FileStillFits)	//It stopped and file fits. We're sorta lucky :)
		{
			if (A->State == STATE_IDLE) A->State = STATE_FILE_OPENED;	//most probable case
			if (A->State == STATE_FILE_OPENED) return TRUE;		//No fatal errors etc.
		}
	}

	if (Flags & FL_ONLY_PRECACHED) return FALSE;

	//Exactly in this order. STATE_REQUEST_EXACT_FILE may change to STATE_NO_SUCH_FILE if search is failed right now.
	if (A->State == STATE_REQUEST_EXACT_FILE || A->State == STATE_NO_SUCH_FILE) A->State = STATE_REQUEST_SKIP;	//Abort last bgnd search. STATE_REQUEST_SKIP is a failproof choice, it'll cause both states (and STATE_FILE_OPENED, too) to finish properly.

	while (A->State == STATE_FILE_OPENED || A->State == STATE_REQUEST_SKIP)
	{
		A->State = STATE_REQUEST_SKIP;
		SetEvent (A->WaitForMain);

		ResetEvent(A->WaitForUnpacker);	//Unpacker thread changes State and THEN sets Event. So racing conditions are not possible.
		if (A->State != STATE_REQUEST_SKIP) SetEvent(A->WaitForUnpacker);	//False alarm! Unpacker thread reported reaching IDLE state between last check and this line.
		else { SetEvent(A->WaitForMain); WaitForSingleObject (A->WaitForUnpacker, INFINITE); }	//Stop wasting CPU and wait until needed!
	}
	
	if (A->State != STATE_IDLE) return FALSE;	//Something went very wrong: instead of reaching IDLE state, Unpacker reached ERROR due to fatal archive error.

	strcpy_im (A->CurrentFile, FileName);	//Requesting exact file
	A->FileStillFits = FALSE;		//The content of the buffer is NOT the "A->CurrentFile" anymore.
	A->Flags = Flags;			//under exact conditions
	A->State = STATE_REQUEST_EXACT_FILE;	//Go!!!
	SetEvent (A->WaitForMain);

	if (Flags & FL_BACKGROUND_SEEK) return FALSE;

	while (A->State == STATE_REQUEST_EXACT_FILE)
	{
		ResetEvent(A->WaitForUnpacker);	//Unpacker thread changes State and THEN sets Event. So racing conditions are not possible.
		if (A->State != STATE_REQUEST_EXACT_FILE) SetEvent(A->WaitForUnpacker);	//False alarm! Unpacker thread reported reaching OPENED state between last check and this line.
		else { SetEvent(A->WaitForMain); WaitForSingleObject (A->WaitForUnpacker, INFINITE); }	//Stop wasting CPU and wait until needed!
	}

	if (A->State == STATE_FILE_OPENED)
	{
		return TRUE;	//File exists
	}
	else if (A->State == STATE_NO_SUCH_FILE)
	{
		A->State = STATE_IDLE;
		SetEvent(A->WaitForMain);
		return FALSE;	//file does not exist, or can't be found with this combination of Flags.
	}
	else if (A->State == STATE_IDLE)	//Always checked last (AFTER unpacker can possibly change STATE_FILE_OPENED to STATE_IDLE because file is empty).
	{
		return TRUE;	//File exists
	}

	return FALSE;	//Fatal errors, closed archives etc.
}

extern "C" unsigned int __export __syscall ReadBody (void* Arc, char* Buffer, unsigned int Size, unsigned long *Flags)	//Reads the file content into Buffer until Size expires or the file ends. The inner buffer we read the data from (allocated in OpenArchive) is filled from the separate thread.
{
	unsigned int RealSize=0;
	Archive* A=(Archive*)(Arc);
	
	//Тут мы смотрим, что есть в нашем буфере, копируем в клиентский и даём треду добро на затирание этой области.
	//Если работаем в безбуферном режиме, то просто даём треду адрес и ждём, когда он выставит готовность. Это на сладкое.

	if (A->State == STATE_REQUEST_EXACT_FILE)
	{
		if (Flags) *Flags = RES_BUSY_SEEKING;
		return 0;
	}
	if (A->State == STATE_NO_SUCH_FILE)
	{
		if (Flags) *Flags = RES_FAILED_SEEKING;
		return 0;
	}

	while (A->State == STATE_FILE_OPENED)	//Until thread reports the file is ended or the request is fully satisfied.
	{
		if (!Size) break;	//buffer is already filled on previous loops
		if (A->BufBeg == A->BuffSize && A->BufEnd != A->BuffSize) A->BufBeg=0;
		if (A->BufEnd == A->BufBeg)	//Buffer is empty
		{
			ResetEvent(A->WaitForUnpacker);	//Unpacker thread changes State (e. g. to IDLE) or changes BufEnd and only THEN sets Event. So racing conditions are not possible.
			if (A->State != STATE_FILE_OPENED || A->BufEnd != A->BufBeg) SetEvent(A->WaitForUnpacker);	//False alarm! Unpacker thread reported EOF or provided some data.
			else { SetEvent(A->WaitForMain); WaitForSingleObject (A->WaitForUnpacker, INFINITE); }	//Stop wasting CPU and wait until needed!
		}

		int B=A->BufBeg, E=A->BufEnd, S=Size;
		if (E>=B) S=min(S,E-B);
		S=min(S,A->BuffSize-B);
		if (!S) continue;

		memcpy_im (Buffer, A->Buffer+B, S);
		A->BufBeg+=S;
		Buffer+=S;
		Size-=S;
		RealSize+=S;

		SetEvent(A->WaitForMain);
	}

	if (Flags)
	{
		*Flags=0;		//The whole file is unpacked, the whole file fits in the buffer etc.
		if (A->State == STATE_IDLE) *Flags|=RES_FILE_ENDED;	//If unpacker already reached the end of the file, A->FileStillFits is already up to date. It's done BEFORE changing state.
		if (A->FileStillFits) *Flags|=RES_FILE_FULLY_PRECACHED; //This is meaningful only for ended files. If file is still unpacking, this flag is nothing but "FYI".
	}
	return RealSize;	//Return the actual bytes read.
}

extern "C" unsigned int __export __syscall SkipBody (void* Arc, unsigned int Size, unsigned long *Flags)	//Skips the file content until Size expires or the file ends.
{
	unsigned int RealSize=0;
	Archive* A=(Archive*)(Arc);
	
	//Ничего не копируем, просто перемещаем указатели, чтобы тред шёл по файлу дальше. Я сомневаюсь, что какие-то форматы файлов, кроме бесполезного raw, допускают быстрый пропуск части тела. Файла ещё может быть (не-солиды), а части файла -- вряд ли. Так что пропускаем тут, путём игнора результата.
	//Аналогично, в безбуферном просто указываем, сколько пропустить. Это тоже на сладкое, как и весь безбуферный. Я вообще сомневаюсь, имеет ли этот безбуферный хотя бы бит смысла.

	if (A->State == STATE_REQUEST_EXACT_FILE)
	{
		if (Flags) *Flags = RES_BUSY_SEEKING;
		return 0;
	}
	if (A->State == STATE_NO_SUCH_FILE)
	{
		if (Flags) *Flags = RES_FAILED_SEEKING;
		return 0;
	}

	while (A->State == STATE_FILE_OPENED)	//Until thread report the file is ended or the request is fully satisfied.
	{
		if (!Size) break;	//buffer is already filled on previous loops
		if (A->BufBeg == A->BuffSize && A->BufEnd != A->BuffSize) A->BufBeg=0;
		if (A->BufEnd == A->BufBeg)	//Buffer is empty
		{
			ResetEvent(A->WaitForUnpacker);	//Unpacker thread changes State (e. g. to IDLE) or changes BufEnd and only THEN sets Event. So racing conditions are not possible.
			if (A->State != STATE_FILE_OPENED || A->BufEnd != A->BufBeg) SetEvent(A->WaitForUnpacker);	//False alarm! Unpacker thread reporte EOF or provided some data.
			else { SetEvent(A->WaitForMain); WaitForSingleObject (A->WaitForUnpacker, INFINITE); }	//Stop wasting CPU and wait until needed!
		}

		int B=A->BufBeg, E=A->BufEnd, S=Size;
		if (E>=B) S=min(S,E-B);
		S=min(S,A->BuffSize-B);
		if (!S) continue;

//		memcpy_im (Buffer, A->Buffer+B, S);
		A->BufBeg+=S;
//		Buffer+=S;
		Size-=S;
		RealSize+=S;

		SetEvent(A->WaitForMain);
	}

	if (Flags)
	{
		*Flags=0;		//The whole file is unpacked, the whole file fits in the buffer etc.
		if (A->State == STATE_IDLE) *Flags|=RES_FILE_ENDED;	//If unpacker already reached the end of the file, A->FileStillFits is already up to date. It's done BEFORE changing state.
		if (A->FileStillFits) *Flags|=RES_FILE_FULLY_PRECACHED; //This is meaningful only for ended files. If file is still unpacking, this flag is nothing but "FYI".
	}
	return RealSize;	//Return the actual bytes read.
}

// extern "C" BOOL __export __syscall ChangePassword (void* Arc, char* NewPassword)
// ToDo! Waits for the safe moment like SeekFile does
// while (WaitForSingleObject (A->WaitForMain, 0)==WAIT_OBJECT_0) if (A->State==STATE_ERROR) return FALSE;	//Sooner or later, Unpacker will stop either way: buffer is full, file is ended or it uses old Begin value and overwrites the beginning.
// SetEvent (A->WaitForMain);	//Be sure Unpacker noticed the Beg has been changed!
// while (WaitForSingleObject (A->WaitForMain, 0)==WAIT_OBJECT_0) if (A->State==STATE_ERROR) return FALSE;	//After it stopped again, that means it's OK now.
// ...and then changes the password + realloc (strlen+1)
