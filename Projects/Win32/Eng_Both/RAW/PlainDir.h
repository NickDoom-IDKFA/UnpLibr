struct RECURSUBDIR
{
	RECURSUBDIR* Parent;
	HANDLE WinSearch;
	char CurrentDir [MAX_PATH];
	int RootLength;
};