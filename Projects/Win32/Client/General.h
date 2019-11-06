// Unpack Library Win32 DLL / LIB main header.

//Thread Debug
/*		//Add a single slash to switch option. Warning! This option limits .dll lifetime to the Log size.
extern volatile char ThreadLog[16*1024*1024];
extern volatile int  ThreadLogged;
#define THREADLOG(X) ThreadLog[ThreadLogged++]=X;
#define DEBUGTHREADS
/*/
#define THREADLOG(X)
//*/

/*		//Add a single slash to enable another option
#include <stdio.h>
#define THREADLOG(X) cout<<X<<endl; flushall();
#define JUSTPRINT(X) cout<<X<<endl; flushall();
/*/
#define JUSTPRINT(X)
// */

//"Copy Immediately" functions. They can copy memory/string areas in ANY order because the area CAN'T be simultaneously accessed from another thread,
//but they MUST finish copying exactly in THIS line (NO "order of execution" optimisation allowed). For Watcom, the following syntax seems to be enough.
#define strcpy_im(D,S)   strcpy((char*)(D), (char*)(S))
#define memcpy_im(D,S,Z) memcpy((char*)(D), (char*)(S), Z)
//Note for Linux port: don't forget to replace stricmp with strcmp.

#define STATE_REQUEST_OPEN_ARCH 0	//Strictly (assigned via memset to 0)
#define STATE_ARCH_OPENED 1		//Unpacker notifies main thread the archive is opened and capabilities are written.
#define STATE_IDLE 2			//Each thread notifies another one the job is finished.
#define STATE_REQUEST_NEXT_FILE 3	//Main thread requests unacking the next file.
#define STATE_REQUEST_EXACT_FILE 4	//Main thread requests unacking the file with exact name.
#define STATE_FILE_OPENED 5		//Unpacker notifies the main thread the file is opened and it's name is either written, or exist.
#define STATE_NO_SUCH_FILE 6		//Unpacker notifies the main thread the archive is either ended, or exact file can't be found (at all or with chosen criteria).
#define STATE_REQUEST_SKIP 7		//Main thread requests skipping the remaining part of the file body.
#define STATE_ERROR 8			//Unpacker notifies the main thread the archive is dead.
#define STATE_REQUEST_CLOSE_ARCH 9	//Main thread requests finishing the work.
#define STATE_ARCH_CLOSED 10		//Work is finished, the thread is ending now.

//Allowed state changes:
// STATE_REQUEST_OPEN_ARCH	//Initial state. By Unpacker: to STATE_ARCH_OPENED
// STATE_ARCH_OPENED		//By Main: to STATE_IDLE;
// STATE_IDLE			//By Main: to STATE_REQUEST_NEXT_FILE, STATE_REQUEST_EXACT_FILE, STATE_REQUEST_CLOSE_ARCH.
				//Sometimes may be changed to STATE_REQUEST_SKIP if Main hadn't noticed the file is already ended by itself (thread desync), but those cases immediately get fixed by Unpacker.
				//To STATE_FILE_OPENED if we re-open the last file from Buffer.
// STATE_REQUEST_NEXT_FILE	//By Unpacker: to STATE_FILE_OPENED, STATE_NO_SUCH_FILE, STATE_ERROR (on fatal errors).
// STATE_REQUEST_EXACT_FILE	//By Unpacker: to STATE_FILE_OPENED, STATE_NO_SUCH_FILE, STATE_ERROR (on fatal errors). By Main (only when search is Bgnd): to STATE_REQUEST_SKIP.
// STATE_FILE_OPENED		//By Main: to STATE_REQUEST_SKIP, by Unpacker: to STATE_IDLE after Main read all the data (which happens very soon for empty files), to STATE_ERROR (on fatal errors).
// STATE_NO_SUCH_FILE		//By Main: to STATE_IDLE. Also (only when search is Bgnd): to STATE_REQUEST_SKIP.
// STATE_REQUEST_SKIP		//By Unpacker: to STATE_IDLE (on file end).
// STATE_ERROR			//By Main: to STATE_REQUEST_CLOSE_ARCH.
// STATE_REQUEST_CLOSE_ARCH	//By Unpacker: to STATE_ARCH_CLOSED.
// STATE_ARCH_CLOSED		//Final state. Can't be changed to anything.

//Call result Flags:
#define RES_FILE_ENDED 1		//The whole file is unpacked (if any). If file ends exactly as the last byte has been written/skipped, it's not guaranteed to be set in this call. In such case, next call will return 0 and set this flag.
#define RES_FILE_FULLY_PRECACHED 2	//The unpacked part of the file is fully pre-cached in RAM and can be quickly reopened (quickly becomes obsolete unless RES_FILE_ENDED is also set)
#define RES_BUSY_SEEKING 4		//Background search for a certain file is not finished yet.
#define RES_FAILED_SEEKING 8		//Background search for a certain file finished and file is not exist/found.

//Archive Capabilities:
#define CAPS_FAST_OPEN 1		//Non-solid archive (quick access to a random file). Only archive file reading is required (no unpacking needed).
#define CAPS_INSTANT_OPEN 2		//Non-solid archive with a FAT header (instant access to a random file). Even archive file reading is not required.

//Call flags:
#define FL_ONLY_PRECACHED 1	//Tells SeekFile only to re-open the last file if it's still pre-cached. If file was opened and never was read, SeekFile is guaranteed to successfully re-open it, so this call may also be used to check if file was found in background search.
#define FL_NO_REWIND 2		//Tells SeekFile to search until the archive ends. If the file is in the processed part of the archive, it'll not be opened. This flag may be IGNORED if the archive has a random-access file list so it can be read in any order (CAPS_INSTANT_OPEN).
#define FL_BACKGROUND_SEEK 4	//Tells SeekFile to exit immediately. The archive will be listed in background, all other calls will fail until search will be completed. To check if it is, call again with FL_ONLY_PRECACHED or try reading data. To abort search, open a different file (e. g. next one).

struct Archive {
	char volatile CurrentFile[MAX_PATH];
	int volatile Flags;			//Generic flags, related to actual content of CurrentFile.
	int volatile State;
	unsigned char volatile *Buffer;		//Loop buffer, NULL for Direct Mode (very slow due to constant thread blocking but saves some memory; WiP for now).
	BOOL volatile FileStillFits;		//The whole file still fits in Buffer. Used for fast re-reading.
	unsigned long volatile BufBeg, BufEnd;	//Indexes of data in Buffer. Asynchronous access (unpacking thread can modify only End, client (main) thread can modify only Begin.

	unsigned long BuffSize;		//0 for Direct Mode (WiP).
	char* PasswordString;		//For encrypted archives, may be NULL or zero-size string.
	HANDLE WaitForMain, WaitForUnpacker;	//To avoid waiting in {while(Archive->State!=Needed);} loops and wasting CPU time, these additional Events are made.
};

//Interfaces for each archive types.
//They are simple: just call a proper function and it will launch a thread.
//All threads work with shared data area the same way.

void StartRaw (Archive *A);
void StartRAR (Archive *A);
void Start7z  (Archive *A);
void StartZip (Archive *A);
void StartTar (Archive *A);
void StartISO (Archive *A);
void StartGZ  (Archive *A);
void StartXZ  (Archive *A);

//Additional functions for use FROM an archive thread (not to be called directly by main thread).
void* ParseTarData (char *Data, int Size);	//Parses .Tar, returns the actual size of data processed and it's result: file name found, file body found etc.
void* ParseISOData (char *Data, int Size);	//То же самое: скармливается ИСОшка по мере распаковки, каждый раз возвращается, сколько реально обработано байт и нашлось ли что-то интересное (таблица файлов, тело одного из них и т. д.)
