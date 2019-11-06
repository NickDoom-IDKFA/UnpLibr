# UnpLibr
Archive unpacking library.
Unpacks files in background.
Transparent unpacking provides file access similar to common files.

Currently supported: transparent unpacking into a buffer of desired size, one thread per archive, raw directory access (pseudo-archive), .RAR archives (requires external freeware unpacker, which is included but can be updated as needed).

Need help with:
1) testing, Testing, TESTING!!! It already fits my needs (so I'm not currently planning to work on it) but after few nights of randomized auto-testing I still can't trust my own thread sync.
2) unpacking .7z, .tar.xz and .iso.xz.

Suggested features:
1) Direct unpacking (memory vs speed) into a buffer while reading data (without pre-caching). It's not lock-free (unpacker have to wait until data is needed). However, it's not a very useful feature and it also requires to mess with the tread sync.
2) Multiple unpacking threads and auto-selection algorithm (for using the thread which is nearest to a requested file).

As you can see, it does not include any actual unpacking algorithms. "Raw" is a stub, and .RAR uses an external library. It's still a "To-Do" part. But the most important part (background unpacking and the thread sync, including fast re-opening of pre-cached files and background archive listing) looks OK. It can be, I hope, re-used for calling any unpacking functions.
