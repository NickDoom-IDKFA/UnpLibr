DoomWAD::DoomWAD()
{
//	Create new archive and allocate structures
}

DoomWAD::ProcessData(char* Data, unsigned long Size, RewindCallback Rewind)
{
//	Processes next chunk of uncompressed data. Reads and remembers a header until it ends. Finds actual data and returns them.
//	Due to non-unique WAD resource names, it can't open a resource by it's name (only OpenNextFile is supported, not SeekFile).
//	Of course, this is not implemented here but using SeekFile for WADs will cause unpredictable results.
//	RewindCallback is never used by this module due to linear structure of WADs.
}