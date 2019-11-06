//This is not a part of the Unpack Library project. You don't need it to build the library itself.
//This is a file you should supply with the Unpack Library for end users and their projects.

//The major version of the library is 0. That means it's on early beta and it's functionality is not established yet. It still may change WITHOUT providing backward compatibility functions, causing minor but urgent fixes in your software.

//Export functions and their parameters left to right:
typedef          void* (*P_OPEN_ARCHIVE  )(char*, int, unsigned long*, char*);
//Opens archive by Name, allocates pre-cache buffer of Size (for background thread), fills Capabilities (unless it's NULL), you also can provide Password (may be null). Returns pointer to Archive or NULL (if failed). You may only use this pointer for subsequent calls.
typedef          BOOL  (*P_CLOSE_ARCHIVE )(void*);
//Closes archive by Pointer. Must return TRUE unless unpacking thread is frosen and not reporting. After call, the pointer is not valid anymore.
typedef          BOOL  (*P_OPEN_NEXT_FILE)(void*, char*);
//Opens next file in archive by Pointer, writes it's Name (unless you provide NULL), returns TRUE if archive is OK and not ended yet.
typedef          BOOL  (*P_SEEK_FILE     )(void*, char*, unsigned long);
//In the archive by Pointer, this function looks for file by it's Name, accordingly to Flags. Returns TRUE if file exists, returns FALSE if it does not, or it's not known yet because of the background search.
typedef unsigned int   (*P_READ_BODY     )(void*, char*, unsigned int, unsigned long*);
//Reads file body from archive by Pointer to Buffer (at most Size bytes), writes additional Flags (unless you provide NULL). Returns actual size read (it can be smaller than requested size ONLY if the file reached it's end).
//Flags can be RES_FILE_ENDED (not guaranteed to be set immediately in that call!) and RES_FILE_FULLY_PRECACHED (the unpacked part of the file still fits in the buffer). The combination of them means you can use SeekFile for instant file reopening from RAM.
typedef unsigned int   (*P_SKIP_BODY     )(void*, unsigned int, unsigned long*);
//The same as ReadBody but simply skips the requested amount of data.

//Call result Flags:
// ReadBody:
#define RES_FILE_ENDED 1		//The whole file is unpacked (if any). If file ended exactly as last byte was written/skipped, it's not guaranteed to be set in this call. In such case, next call will return 0 and set this flag.
#define RES_FILE_FULLY_PRECACHED 2	//The unpacked part of the file is fully pre-cached in RAM and can be quickly reopened (quickly becomes obsolete unless RES_FILE_ENDED is also set)
#define RES_BUSY_SEEKING 4		//Background search for a certain file is not finished yet.
#define RES_FAILED_SEEKING 8		//Background search for a certain file finished and file is not exist/found.

//Archive Capabilities:
#define CAPS_FAST_OPEN 1		//Non-solid archive (quick access to a random file). Only archive file reading is required (no unpacking needed).
#define CAPS_INSTANT_OPEN 2		//Non-solid archive with a FAT header (instant access to a random file). Even archive file reading is not required.

//Call flags:
#define FL_ONLY_PRECACHED 1	//Tells SeekFile only to re-open the last file if it's still pre-cached. If file was opened and never was read, SeekFile is quaranteed to successfully re-open it, so this call may also be used to check if file was found in background search.
#define FL_NO_REWIND 2		//Tells SeekFile to search until the archive ends. If the file is in the processed part of the archive, it'll not be opened. This flag may be IGNORED if the archive has a random-access file list so it can be read in any order (CAPS_INSTANT_OPEN).
#define FL_BACKGROUND_SEEK 4	//Tells SeekFile to exit immediately. The archive will be listed in background, all other calls will fail until search will be completed. To check if it is, call again with FL_ONLY_PRECACHED or try reading data. To abort search, open a different file (e. g. next one).

//доработки по соображениям геймдевовских архаровцев (здравое зерно, отделённое от "нусейчастакнемодно"):
//1) добавить реквест/стейт (или просто флаг для искалки) для выдачи расстояния от текущей позиции до запрошенного файла (в любых попугаях, т. к. сравниваются только сами с собой).
//2) сделать пул указателей на архив и апи для открытия их всех чохом (ессно с указанием на один файл, который передаст юзер). Внутренним вызовом своего же опенархива ессно.
//3) обсемафорить "сырые" апи, чтобы можно было дёргать из нескольких тредов.
//4) а вот апи для работы с пулом хитрее -- они выбирают незанятные треды распаковщика (ну хотя бы те, где вернулись из вызова; фоновые операции можно запретить как не особо нужные пулу)
//   и из этих тредов выбирают тот, которому меньше надо распаковывать объём до заданного файла. Особенно если "частичная солидовость" (файлы склеены в группы).
//   Соответственно, множественные треды пользовательской задачи могут рандомно дёргать распаковщики пула, а им будет отвечать самый на тот момент готовый.
//   Списки файлов хранить в каждом распаковщике накладно, а в менеджере распаковщиков -- ёбко (особенно в плане стандартизации). С другой стороны, то и другое вполне терпимо, и ХЗ,
//   есть ли критерии большей/меньшей готовности, кроме положения в списке файлов.
