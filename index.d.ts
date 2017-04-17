interface file_stat {
    mtime: Date,
    atime: Date,
    ctime: Date,
    size: number,
    mode: number,
    uid: number,
    gid: number,

}
interface filesystem_stat {
    bsize: number,
    frsize: number,
    blocks: number,
    bfree: number,
    bavail: number,
    files: number,
    ffree: number,
    favail: number,
    fsid: number,
    flag: number,
    namemax: number
}

interface mountcallback {
    (returnCode: number, value?: any)
}

interface mount_options {
    displayFolder?: boolean
    force?: boolean
    options?: any[]
    init?(callback?: mountcallback)
    statfs?(path: string, callback: (returnCode: number, stat?: filesystem_stat) => void)
    access?(path: string, mode, callback: mountcallback)
    readdir?(path: string, callback: (returnCode: number, file_dir?: string[]) => void)
    rmdir?(path: string, callback: mountcallback)
    mkdir?(path: string, callback: mountcallback)
    symlink?(src: string, dst: string, callback: mountcallback)
    unlink?(src: string, dst: string, callback: mountcallback)
    rename?(src: string, dst: string, callback: mountcallback)
    releasedir?(path: string, fd: number, callback: mountcallback)
    release?(path: string, fd: number, callback: mountcallback)
    create?(path: string, mode, callback: mountcallback)
    open?(path: string, flags, callback: (returnCode: number, file_descriptor?: number) => void)
    getattr?(path: string, callback: (returnCode: number, stat?: file_stat) => void)
    read?(path: string, fd: number, buffer: Buffer, length: number, position: number, callback?: mountcallback)
    flush?(path: string, fd: number, callback: mountcallback)
    fsync?(path: string, fd: number, datasync, callback: mountcallback)
    fsyncdir?(path:string, fd, datasync, callback: mountcallback)
    truncate?(path:string, size:number, callback: mountcallback)
    ftruncate?(path:string, fd:number, size:number, callback: mountcallback)
    readlink?(path:string, callback:mountcallback)
    chown?(path:string, uid:number, gid:number, callback:mountcallback)
    chmod?(path:string, mode:number, callback:mountcallback)
    mknod?(path:number, mode:number, dev:number, callback:mountcallback)
    setxattr?(path:string, name:string, buffer:Buffer, length:number, offset:number, flags:number,callback:mountcallback)
    getxattr?(path:string, name:string, buffer:Buffer, length:number, offset:number, callback:mountcallback)
    listxattr?(path:string, buffer:Buffer, length:number, callback:mountcallback)
    removexattr?(path:string, name:string, callback:mountcallback)
    opendir?(path:string, flags:number, callback:mountcallback)
    read?(path:string, fd:number, buffer:Buffer, length:number, position:number,callback:mountcallback)
    write?(path:string, fd:number, buffer:Buffer, length:number, position:number, callback:mountcallback)
    utimens?(path:string, atime:Date, mtime:Date, callback:mountcallback)
    destroy?(callback:mountcallback)

}

declare class fusebind {
    readonly EPERM;
    readonly ENOENT;
    readonly ESRCH;
    readonly EINTR;
    readonly EIO;
    readonly ENXIO;
    readonly E2BIG;
    readonly ENOEXEC;
    readonly EBADF;
    readonly ECHILD;
    readonly EAGAIN;
    readonly ENOMEM;
    readonly EACCES;
    readonly EFAULT;
    readonly ENOTBLK;
    readonly EBUSY;
    readonly EEXIST;
    readonly EXDEV;
    readonly ENODEV;
    readonly ENOTDIR;
    readonly EISDIR;
    readonly EINVAL;
    readonly ENFILE;
    readonly EMFILE;
    readonly ENOTTY;
    readonly ETXTBSY;
    readonly EFBIG;
    readonly ENOSPC;
    readonly ESPIPE;
    readonly EROFS;
    readonly EMLINK;
    readonly EPIPE;
    readonly EDOM;
    readonly ERANGE;
    readonly EDEADLK;
    readonly ENAMETOOLONG;
    readonly ENOLCK;
    readonly ENOSYS;
    readonly ENOTEMPTY;
    readonly ELOOP;
    readonly EWOULDBLOCK;
    readonly ENOMSG;
    readonly EIDRM;
    readonly ECHRNG;
    readonly EL2NSYNC;
    readonly EL3HLT;
    readonly EL3RST;
    readonly ELNRNG;
    readonly EUNATCH;
    readonly ENOCSI;
    readonly EL2HLT;
    readonly EBADE;
    readonly EBADR;
    readonly EXFULL;
    readonly ENOANO;
    readonly EBADRQC;
    readonly EBADSLT;
    readonly EDEADLOCK;
    readonly EBFONT;
    readonly ENOSTR;
    readonly ENODATA;
    readonly ETIME;
    readonly ENOSR;
    readonly ENONET;
    readonly ENOPKG;
    readonly EREMOTE;
    readonly ENOLINK;
    readonly EADV;
    readonly ESRMNT;
    readonly ECOMM;
    readonly EPROTO;
    readonly EMULTIHOP;
    readonly EDOTDOT;
    readonly EBADMSG;
    readonly EOVERFLOW;
    readonly ENOTUNIQ;
    readonly EBADFD;
    readonly EREMCHG;
    readonly ELIBACC;
    readonly ELIBBAD;
    readonly ELIBSCN;
    readonly ELIBMAX;
    readonly ELIBEXEC;
    readonly EILSEQ;
    readonly ERESTART;
    readonly ESTRPIPE;
    readonly EUSERS;
    readonly ENOTSOCK;
    readonly EDESTADDRREQ;
    readonly EMSGSIZE;
    readonly EPROTOTYPE;
    readonly ENOPROTOOPT;
    readonly EPROTONOSUPPORT;
    readonly ESOCKTNOSUPPORT;
    readonly EOPNOTSUPP;
    readonly EPFNOSUPPORT;
    readonly EAFNOSUPPORT;
    readonly EADDRINUSE;
    readonly EADDRNOTAVAIL;
    readonly ENETDOWN;
    readonly ENETUNREACH;
    readonly ENETRESET;
    readonly ECONNABORTED;
    readonly ECONNRESET;
    readonly ENOBUFS;
    readonly EISCONN;
    readonly ENOTCONN;
    readonly ESHUTDOWN;
    readonly ETOOMANYREFS;
    readonly ETIMEDOUT;
    readonly ECONNREFUSED;
    readonly EHOSTDOWN;
    readonly EHOSTUNREACH;
    readonly EALREADY;
    readonly EINPROGRESS;
    readonly ESTALE;
    readonly EUCLEAN;
    readonly ENOTNAM;
    readonly ENAVAIL;
    readonly EISNAM;
    readonly EREMOTEIO;
    readonly EDQUOT;
    readonly ENOMEDIUM;
    readonly EMEDIUMTYPE;

    mount(mountPath: string, mountops: mount_options, callback?: Function)

    unmount(mountpath: string, callback?: Function)

    context(): { pid: number, uid: number, gid: number }
}
