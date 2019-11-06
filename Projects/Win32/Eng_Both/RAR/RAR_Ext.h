#include "unrar.h"	//external headers supplied with external dll

//Additional crotch definitions
typedef HANDLE PASCAL (*pRAROPENARCHIVE       )(struct RAROpenArchiveData *ArchiveData);
typedef HANDLE PASCAL (*pRAROPENARCHIVEEX     )(struct RAROpenArchiveDataEx *ArchiveData);
typedef int    PASCAL (*pRARCLOSEARCHIVE      )(HANDLE hArcData);
typedef int    PASCAL (*pRARREADHEADER        )(HANDLE hArcData,struct RARHeaderData *HeaderData);
typedef int    PASCAL (*pRARREADHEADEREX      )(HANDLE hArcData,struct RARHeaderDataEx *HeaderData);
typedef int    PASCAL (*pRARPROCESSFILE       )(HANDLE hArcData,int Operation,char *DestPath,char *DestName);
typedef int    PASCAL (*pRARPROCESSFILEW      )(HANDLE hArcData,int Operation,wchar_t *DestPath,wchar_t *DestName);
typedef void   PASCAL (*pRARSETCALLBACK       )(HANDLE hArcData,UNRARCALLBACK Callback,LPARAM UserData);
typedef void   PASCAL (*pRARSETCHANGEVOLPROC  )(HANDLE hArcData,CHANGEVOLPROC ChangeVolProc);
typedef void   PASCAL (*pRARSETPROCESSDATAPROC)(HANDLE hArcData,PROCESSDATAPROC ProcessDataProc);
typedef void   PASCAL (*pRARSETPASSWORD       )(HANDLE hArcData,char *Password);
typedef int    PASCAL (*pRARGETDLLVERSION     )();

//Crotch unrar.lib undefine so we don't need to link unrar.lib anymore
#define RAROpenArchive       
#define RAROpenArchiveEx     
#define RARCloseArchive      
#define RARReadHeader        
#define RARReadHeaderEx      
#define RARProcessFile       
#define RARProcessFileW      
#define RARSetCallback       
#define RARSetChangeVolProc  
#define RARSetProcessDataProc
#define RARSetPassword       
#define RARGetDllVersion     
