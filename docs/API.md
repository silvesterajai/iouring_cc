## APIs
### Register File Description
`IoUringParams::SetMaxFdSlots` will set maximum possible registered files in `IoUring`. Each file descriptor(s) can be registered using `RegisterFile(int fd)` or `RegisterFiles(std::vector<fd_t> &fds)`. Registered file descriptor can be unregistered using `UnregisterFile(int fd)`. If the registered `fd` is used for `Read`/`Write` operation, registered fd_index will be used internal along with enabling SQE flag `IOSQE_FIXED_FILE`.   
`IoUringParams::SetMaxFdSlots` can be set to 0 (default value). In such case use `RegisterFilesExternal(std::vector<fd_t> &fds, int index = 0, bool is_update = false)` API. User have to explicitly use fd_index and set SQE flags in such case.  
After initial registration, file descriptor(s) can be modified/replaced. If more file descriptors need to be registered then first unregister all the fds and re-register along with new fd(s).  

