FAT_ISO::FAT_ISO()
{
//	Create new archive and allocate structures
}

FAT_ISO::ProcessData(char* Data, unsigned long Size, RewindCallback Rewind)
{
//	Processes next chunk of uncompressed data.
//	It's possible to have more than one FAT, and also fragmentation.
//	So in order to continue unpacking current file, this module requires the RewindCallback implementation (this function must rewind the packed file to a desired unpacked position and pass it to next call of ProcessData).
}