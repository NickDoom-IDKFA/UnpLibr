#include <windows.h>
#include <iostream.h>
#include <fstream.h>
#include <STDIO.h>

#include "..\\Projects\\Win32\\Client\\UnpLibr.h"

#define FILES 7000

char NameList[FILES][MAX_PATH]=
{"bar.foo",
 };

P_OPEN_ARCHIVE   pOpenArchive;
P_CLOSE_ARCHIVE  pCloseArchive;
P_OPEN_NEXT_FILE pOpenNextFile;
P_SEEK_FILE      pSeekFile;
P_READ_BODY      pReadBody;
P_SKIP_BODY      pSkipBody;

void main (void)
{
	HINSTANCE LibHinst;

	LibHinst=LoadLibrary("..\\Projects\\Win32\\Client\\DLL_P\\UnpLibr.dll");
	cout<<"LoadLibrary returned "<<LibHinst<<endl;

	pOpenArchive = (P_OPEN_ARCHIVE) GetProcAddress(LibHinst, "OpenArchive");
	cout<<"OpenArchive address found as "<<pOpenArchive<<endl;

	pCloseArchive = (P_CLOSE_ARCHIVE) GetProcAddress(LibHinst, "CloseArchive");
	cout<<"CloseArchive address found as "<<pCloseArchive<<endl;

	pOpenNextFile = (P_OPEN_NEXT_FILE) GetProcAddress(LibHinst, "OpenNextFile");
	cout<<"OpenNextFile address found as "<<pOpenNextFile<<endl;

	pSeekFile = (P_SEEK_FILE) GetProcAddress(LibHinst, "SeekFile");
	cout<<"SeekFile address found as "<<pSeekFile<<endl;

	pReadBody = (P_READ_BODY) GetProcAddress(LibHinst, "ReadBody");
	cout<<"ReadBody address found as "<<pReadBody<<endl;

	pSkipBody = (P_SKIP_BODY) GetProcAddress(LibHinst, "SkipBody");
	cout<<"SkipBody address found as "<<pSkipBody<<endl;

flushall();

	unsigned long Flags;
	int S, PreS, TestStage, LastOpened;
	void* ArcHandle;
	char FN[MAX_PATH],TN[MAX_PATH];
	char B[64*1024];
	char PreB[64*1024];
	fstream T;


//*

	for (LastOpened=TestStage=0;;)
	{
		if (!TestStage)		//First test: open %)
		{
			ArcHandle = (*pOpenArchive)("7000files.rar",1*1024*1024,&Flags,NULL);	//RAR
//			ArcHandle = (*pOpenArchive)("z:\\7000files\\",1024,&Flags,NULL);	//Raw

			if (!ArcHandle)
			{
				cout<<"!Error opening!"<<endl;
				flushall();
				break;
			}
			TestStage = 1;
			cout<<"Archive opened"<<endl;
			flushall();
		}

		if (TestStage > 2)	//give it some time!
		{				//250 runs ave ~100 000 mS (1 delay ave 480), RM = 32767 so RM/2/480 = 34
			Sleep (rand()/34);	//Подобрано под размер моего архива и его файлов, чтобы то удавалось найти, то не успевал.
		}

		if (rand() < RAND_MAX/1000)	//Second test: close it at any moment.
		{
			(*pCloseArchive)(ArcHandle);
			TestStage = 0;
			cout<<"Archive closed"<<endl;
			flushall();
			continue;
		}

		if (TestStage > 2 && rand() < RAND_MAX/250 || TestStage < 3 && rand() < RAND_MAX/50 || TestStage == 1) //Third test: open something else.
		{
			if (TestStage > 2) cout<<"Aborted bgnd seek ";
			flushall();
			if (rand() < RAND_MAX/100)	//Random file to seek in foreground.
			{
				strcpy (TN,"z:\\Ethalone7000\\");
				strcpy (FN,NameList[FILES * rand()/(RAND_MAX+1)]);
				strcat (TN, FN);
				T.close();

				OemToChar (TN, TN);
//				OemToChar (FN, FN);

				T.open (TN, ios::in|ios::binary);

				cout<<"Seeking ";
				flushall();
				if (rand() < RAND_MAX/10)	//Sometimes it's purposedly wrong.
				{
					strcat (FN,"$$$$$BADNAME$$$$$$");
					cout<<"for wrong "<<FN<<endl;
					flushall();
					if ((*pSeekFile)(ArcHandle, FN, 0) )
					{
						cout<<"!Error foreground search -- bogus name found!"<<endl;
//continue;
						break;
					}
					TestStage = 1;
					continue;
				}

				cout<<FN<<endl;
				if (T.fail()) {cout<<"!Error testing! Can't open ethalone O_O ->"<<TN<<endl; break;}
				flushall();

				if (!(*pSeekFile)(ArcHandle, FN, 0) )
				{
					cout<<"!Error foreground search!"<<endl;
					flushall();
//continue;
					break;
				}

				TestStage = 2;
				cout<<" opened"<<endl;
				flushall();
				continue;
			}

			if (rand() < RAND_MAX/100)	//Random file to seek in background.
			{
				strcpy (TN,"z:\\Ethalone7000\\");
				strcpy (FN,NameList[FILES * rand()/(RAND_MAX+1)]);
				strcat (TN, FN);
				T.close();

				OemToChar (TN, TN);
//				OemToChar (FN, FN);

				T.open (TN, ios::in|ios::binary);

				cout<<"Bgnd seeking ";
				flushall();
				if (rand() < RAND_MAX/10)	//Sometimes it's purposedly wrong.
				{
					cout<<"for wrong ";
					strcat (FN,"$$$$$BADNAME$$$$$$");
					cout<<FN<<endl;
					flushall();
					(*pSeekFile)(ArcHandle, FN, 4);
					TestStage = 4;
					continue;
				}

				cout<<FN<<endl;
				if (T.fail()) {cout<<"!Error testing! Can't open ethalone O_o ->"<<TN<<endl; break;}
				flushall();
				(*pSeekFile)(ArcHandle, FN, 4);
				TestStage = 3;
				
				continue;
			}

			//Just open the next file.

			cout<<"Opening next ";
			flushall();
			if ( !(*pOpenNextFile)(ArcHandle, FN) )
			{
				//if archive had simply ended
				{
cout<<"?Error or arch ended?"<<endl;	//ToDo
flushall();
					TestStage=1;
					continue;
				}
				cout<<"!Error opening next!"<<endl;
//continue;
				break;
			}
			strcpy (TN,"z:\\Ethalone7000\\");
			strcat (TN, FN);
			T.close();
			OemToChar (TN, TN);
			T.open (TN, ios::in|ios::binary);

			TestStage = 2;
			cout<<FN<<" opened"<<endl;
			if (T.fail()) {cout<<"!Error testing! Can't open ethalone o_O ->"<<TN<<endl; break;}
			flushall();
			continue;
		}

		if (TestStage == 4 && !((*pReadBody)(ArcHandle, B, 64*1024, &Flags)) && Flags == RES_FAILED_SEEKING)
		{
			cout<<"Bgnd search found nothing"<<endl;
			flushall();
			TestStage = 1;
			continue;
		}
		if ( (TestStage == 1 || TestStage == 4) && (*pReadBody)(ArcHandle, B, 64*1024, &Flags))
		{
			cout<<"!Error: something has been read from nowhere! "<<Flags<<endl;
			flushall();
//continue;
			break;
		}
		if (TestStage == 2 || TestStage == 3)	//Try reading and comparing something
		{
			S=(*pReadBody)(ArcHandle, B, 64*1024, &Flags);

			if (TestStage == 3 && Flags == RES_BUSY_SEEKING)
			{
				if (S)
				{
					cout<<"!Error: something has been read before file is found! "<<Flags<<endl;
					flushall();
//continue;
					break;
				}
				continue;
			}

			T.read (PreB, 64*1024);
			PreS = T.gcount();

			if (S!=PreS)
			{
				cout<<"!Error: Read sizes mismatch! Read "<<S<<", real is "<<PreS<<", Flags = "<<Flags<<endl;
				flushall();
//continue;
				break;
			}
			if (memcmp(B,PreB,PreS))
			{
				cout<<"!Error: Content mismatch! "<<Flags<<endl;
				flushall();
//continue;
				break;
			}
			if (!S) TestStage = 1;

			cout<<" Tested "<<S<<" bytes, "<<Flags<<endl;
			if (Flags & (RES_BUSY_SEEKING|RES_FAILED_SEEKING)) cout<<"!Error: bogus flags!"<<endl;
			flushall();
			continue;
		}

	}

/*/


	ArcHandle = (*pOpenArchive)("7000files.rar",16*1024*1024,&Flags,NULL);
	cout<<ArcHandle<<" "<<Flags<<endl;

	while ( (*pOpenNextFile)(ArcHandle, FN) )
	{
//cout<<FN<<endl;
		strcpy (TN,"z:\\Ethalone7000\\");
		if (strchr(FN,':')) return;
		strcat (TN, FN);
		OemToChar (TN, TN);
		T.open (TN, ios::out|ios::binary);

		PreS=(*pReadBody)(ArcHandle, PreB, 64*1024, &Flags);
		cout<<PreS<<" "<<Flags<<" (Pre)"<<endl;
		
cout<<TN<<endl;
		if (!(*pSeekFile)(ArcHandle, FN, 0) ) cout<<"Re-open ERROR!!!"<<endl;
		else 
			while ( S=(*pReadBody)(ArcHandle, B, 64*1024, &Flags) )
		{
			if (memcmp(B,PreB,PreS)) cout<<"Pre-cache error!!!"<<endl;

			cout<<S<<" "<<Flags<<endl;
			T.write (B, S);

			PreS=0;
		}	
		T.close();
cout<<"File ended"<<endl;
	}
cout<<"arch ENDED?"<<endl;
	(*pCloseArchive)(ArcHandle);
cout<<"arch closed"<<endl;
	cin>>Flags;
	
	
//	*/
}