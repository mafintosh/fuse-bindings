
declare module 'fuse-bindings' {

	function mount(mountPoint: string, ops: MountOptions,
		cb: (code: number) => void): void;

	interface MountOptions {
	}

	// XXX readme on https://github.com/mafintosh/fuse-bindings has all things
	// XXX https://github.com/bcle/fuse4js/blob/master/example/mirrorFS.js has
	// fuse4js comments, that may be useful, as these libraries should be
	// comparable in an overlapping area.

	interface MountOptions {
		/**
		 * Set to true to force mount the filesystem (will do an unmount first).
		 */
		force?: boolean;

		options?: string[];

		displayFolder?: boolean;

		/**
		 * Called on filesystem init.
		 * @param cb 
		 */
		init?(cb: (code: number) => void): void;

		/**
		 * Called before the filesystem accessed a file.
		 * @param path 
		 * @param mode 
		 * @param cb 
		 */
		access?(path: string, mode: any, cb: (code: number) => void): void;

		/**
		 * Called when the filesystem is being stat'ed.
		 * @param path 
		 * @param cb accepts a fs stat object after the return code.
		 */
		statfs?(path: string, cb: (code: number, fsStat: FSStat) => void): void;

		/**
		 * Called when a path is being stat'ed. 
		 * @param path 
		 * @param cb accepts a stat object (similar to the one returned in fs.stat
		 * path, cb)) after the return code.
		 */
		getattr?(path: string, cb: (code: number, stats?: Stats) => void): void;

		/**
		 * Called when a file descriptor is being stat'ed. Similar to getattr().
		 * @param path 
		 * @param fd 
		 * @param cb 
		 */
		fgetattr?(path: string, fd: number,
			cb: (code: number, stats: Stats) => void): void;

		/**
		 * Called when a file descriptor is being flushed.
		 * @param path 
		 * @param fd 
		 * @param cb 
		 */
		flush?(path: string, fd: number, cb: (code: number) => void): void;

		/**
		 * Called when a file descriptor is being fsync'ed.
		 * @param path 
		 * @param fd 
		 * @param datasync 
		 * @param cb 
		 */
		fsync?(path: string, fd: number, datasync: any,
			cb: (code: number) => void): void;
		
		/**
		 * Called when a folder descriptor is being fsync'ed. Similar to fsync().
		 * @param path 
		 * @param fd 
		 * @param datasync 
		 * @param cb 
		 */
		fsyncdir?(path: string, fd: number, datasync: any,
			cb: (code: number) => void): void;

		/**
		 * Called when a directory is being listed.
		 * @param path 
		 * @param cb accepts an array of file/directory names after the return
		 * code.
		 */
		readdir?(path: string, cb: (code: number, lst: string[]) => void): void;

		/**
		 * Called when a path is being truncated to a specific size.
		 * @param path 
		 * @param size 
		 * @param cb 
		 */
		truncate?(path: string, size: number, cb: (code: number) => void): void;

		/**
		 * Called when a file via descriptor is being truncated to a specific
		 * size. Similar to truncate().
		 * @param path 
		 * @param fd 
		 * @param size 
		 * @param cb 
		 */
		ftruncate?(path: string, fd: number, size: number,
			cb: (code: number) => void): void;
		
		/**
		 * Called when a symlink is being resolved.
		 * @param path 
		 * @param cb accepts a pathname (that the link should resolve to) after
		 * the return code.
		 */
		readlink?(path: string,
			cb: (code: number|null, targetPathname: string) => void): void;
		
		/**
		 * Called when ownership of a path is being changed.
		 * @param path 
		 * @param uid 
		 * @param gid 
		 * @param cb 
		 */
		chown?(path: string, uid: number, gid: number,
			cb: (code: number) => void): void;
		
		/**
		 * Called when the mode of a path is being changed.
		 * @param path 
		 * @param mode 
		 * @param cb 
		 */
		chmod?(path: string, mode: number, cb: (code: number) => void): void;

		/**
		 * Called when the a new device file is being made.
		 * @param path 
		 * @param mode 
		 * @param dev 
		 * @param cb 
		 */
		mknod?(path: string, mode: number, dev: number,
			cb: (code: number) => void): void;

		/**
		 * Called when extended attributes is being set (see the extended docs
		 * for your platform). Currently you can read the attribute value being
		 * set in buffer at offset.
		 * @param path 
		 * @param name 
		 * @param buffer 
		 * @param length 
		 * @param offset 
		 * @param flags 
		 * @param cb 
		 */
		setxattr?(path: string, name: string, buffer: Buffer, length: number,
			offset: number, flags: number, cb: (code: number) => void): void;
		
		/**
		 * Called when extended attributes is being read. Currently you have to
		 * write the result to the provided buffer at offset.
		 * @param path 
		 * @param name 
		 * @param buffer 
		 * @param length 
		 * @param offset 
		 * @param cb 
		 */
		getxattr?(path: string, name: string, buffer: Buffer, length: number,
			offset: number, cb: (code: number) => void): void;
		
		/**
		 * Called when extended attributes of a path are being listed.
		 * @param path 
		 * @param buffer should be filled with the extended attribute names as
		 * null-terminated strings, one after the other, up to a total of length
		 * in length. (ERANGE should be passed to the callback if length is
		 * insufficient.)
		 * @param length 
		 * @param cb The size of buffer required to hold all the names should be
		 * passed to the callback either on success, or if the supplied length
		 * was zero.
		 */
		listxattr?(path: string, buffer: Buffer, length: number,
			cb: (code: number, reqBufSize: number) => void): void;

		/**
		 * Called when an extended attribute is being removed.
		 * @param path 
		 * @param name 
		 * @param cb 
		 */
		removexattr?(path: string, name: string, cb: (code: number) => void): void;

		/**
		 * Called when a path is being opened.
		 * @param path 
		 * @param flags in a number containing the permissions being requested.
		 * @param cb accepts a file descriptor after the return code.
		 */
		open?(path: string, flags: number,
			cb: (code: number, fd: number) => void): void;

		
		/**
		 * Called when a path is being opened. Similar to open(), but for
		 * directories.
		 * @param path 
		 * @param flags in a number containing the permissions being requested.
		 * @param cb accepts a file descriptor after the return code.
		 */
		opendir?(path: string, flags: number,
			cb: (code: number, fd: number) => void): void;
		
		/**
		 * Called when contents of a file is being read. You should write the
		 * result of the read to the buffer and return the number of bytes
		 * written as the first argument in the callback. If no bytes were
		 * written (read is complete) return 0 in the callback.
		 * @param path 
		 * @param fd 
		 * @param buffer 
		 * @param length 
		 * @param position 
		 * @param cb 
		 */
		read?(path: string, fd: number, buffer: Buffer, length: number,
			position: number, cb: (bytesReadOrErr: number) => void): void;
		
		/**
		 * Called when a file is being written to. You can get the data being
		 * written in buffer and you should return the number of bytes written in
		 * the callback as the first argument.
		 * @param path 
		 * @param fd 
		 * @param buffer 
		 * @param length 
		 * @param position 
		 * @param cb 
		 */
		write?(path: string, fd: number, buffer: Buffer, length: number,
			position: number, cb: (bytesWrittenOrErr: number) => void): void;

		/**
		 * Called when a file descriptor is being released. Happens when a
		 * read/write is done etc.
		 * @param path 
		 * @param fd 
		 * @param cb 
		 */
		release?(path: string, fd: number, cb: (code: number) => void): void;

		/**
		 * Called when a directory's file descriptor is being released. Similar
		 * to release().
		 * @param path 
		 * @param fd 
		 * @param cb 
		 */
		releasedir?(path: string, fd: number, cb: (code: number) => void): void;

		/**
		 * Called when a new file is being opened.
		 * @param path 
		 * @param mode 
		 * @param cb 
		 */
		create?(path: string, mode: number, cb: (code: number) => void): void;

		/**
		 * Called when the atime/mtime of a file is being changed.
		 * @param path 
		 * @param atime 
		 * @param mtime 
		 * @param cb 
		 */
		utimens?(path: string, atime: number, mtime: number,
			cb: (code: number) => void): void;
		
		/**
		 * Called when a file is being unlinked.
		 * @param path 
		 * @param cb 
		 */
		unlink?(path: string, cb: (code: number) => void): void;

		/**
		 * Called when a file is being renamed.
		 * @param src 
		 * @param dest 
		 * @param cb 
		 */
		rename?(src: string, dest: string, cb: (code: number) => void): void;

		/**
		 * Called when a new link is created.
		 * @param path 
		 * @param target 
		 * @param cb 
		 */
		link?(path: string, target: string, cb: (code: number) => void): void;

		/**
		 * Called when a new symlink is created.
		 * @param path 
		 * @param target 
		 * @param cb 
		 */
		symlink?(path: string, target: string, cb: (code: number) => void): void;

		/**
		 * Called when a new directory is being created.
		 * @param path 
		 * @param mode 
		 * @param cb 
		 */
		mkdir?(path: string, mode: number, cb: (code: number) => void): void;

		/**
		 * Called when a directory is being removed.
		 * @param path 
		 * @param cb 
		 */
		rmdir?(path: string, cb: (code: number) => void): void;

		/**
		 * Returns the current fuse context (pid, uid, gid). Must be called
		 * inside a fuse callback.
		 */
		context?(): { pid: number, uid: number, gid: number };

		destroy?(cb: (code: number) => void): void;

	}

	interface FSStat {
		bsize: number;
		frsize: number;
		blocks: number;
		bfree: number;
		bavail: number;
		files: number;
		ffree: number;
		favail: number;
		fsid: number;
		flag: number;
		namemax: number;
	}

	export interface Stats {
		isFile(): boolean;
		isDirectory(): boolean;
		isBlockDevice(): boolean;
		isCharacterDevice(): boolean;
		isSymbolicLink(): boolean;
		isFIFO(): boolean;
		isSocket(): boolean;
		dev: number;
		ino: number;
		mode: number;
		nlink: number;
		uid: number;
		gid: number;
		rdev: number;
		size: number;
		blksize: number;
		blocks: number;
		atimeMs: number;
		mtimeMs: number;
		ctimeMs: number;
		birthtimeMs: number;
		atime: Date;
		mtime: Date;
		ctime: Date;
		birthtime: Date;
	}

	function unmount(mountPoint: string, cb: (code: number) => void): void;

	function errno(code: string): number;

	const EPERM = -1;
	const ENOENT = -2;
	const ESRCH = -3;
	const EINTR = -4;
	const EIO = -5;
	const ENXIO = -6;
	const E2BIG = -7;
	const ENOEXEC = -8;
	const EBADF = -9;
	const ECHILD = -10;
	const EAGAIN = -11;
	const ENOMEM = -12;
	const EACCES = -13;
	const EFAULT = -14;
	const ENOTBLK = -15;
	const EBUSY = -16;
	const EEXIST = -17;
	const EXDEV = -18;
	const ENODEV = -19;
	const ENOTDIR = -20;
	const EISDIR = -21;
	const EINVAL = -22;
	const ENFILE = -23;
	const EMFILE = -24;
	const ENOTTY = -25;
	const ETXTBSY = -26;
	const EFBIG = -27;
	const ENOSPC = -28;
	const ESPIPE = -29;
	const EROFS = -30;
	const EMLINK = -31;
	const EPIPE = -32;
	const EDOM = -33;
	const ERANGE = -34;
	const EDEADLK = -35;
	const ENAMETOOLONG = -36;
	const ENOLCK = -37;
	const ENOSYS = -38;
	const ENOTEMPTY = -39;
	const ELOOP = -40;
	const EWOULDBLOCK = -11;
	const ENOMSG = -42;
	const EIDRM = -43;
	const ECHRNG = -44;
	const EL2NSYNC = -45;
	const EL3HLT = -46;
	const EL3RST = -47;
	const ELNRNG = -48;
	const EUNATCH = -49;
	const ENOCSI = -50;
	const EL2HLT = -51;
	const EBADE = -52;
	const EBADR = -53;
	const EXFULL = -54;
	const ENOANO = -55;
	const EBADRQC = -56;
	const EBADSLT = -57;
	const EDEADLOCK = -35;
	const EBFONT = -59;
	const ENOSTR = -60;
	const ENODATA = -61;
	const ETIME = -62;
	const ENOSR = -63;
	const ENONET = -64;
	const ENOPKG = -65;
	const EREMOTE = -66;
	const ENOLINK = -67;
	const EADV = -68;
	const ESRMNT = -69;
	const ECOMM = -70;
	const EPROTO = -71;
	const EMULTIHOP = -72;
	const EDOTDOT = -73;
	const EBADMSG = -74;
	const EOVERFLOW = -75;
	const ENOTUNIQ = -76;
	const EBADFD = -77;
	const EREMCHG = -78;
	const ELIBACC = -79;
	const ELIBBAD = -80;
	const ELIBSCN = -81;
	const ELIBMAX = -82;
	const ELIBEXEC = -83;
	const EILSEQ = -84;
	const ERESTART = -85;
	const ESTRPIPE = -86;
	const EUSERS = -87;
	const ENOTSOCK = -88;
	const EDESTADDRREQ = -89;
	const EMSGSIZE = -90;
	const EPROTOTYPE = -91;
	const ENOPROTOOPT = -92;
	const EPROTONOSUPPORT = -93;
	const ESOCKTNOSUPPORT = -94;
	const EOPNOTSUPP = -95;
	const EPFNOSUPPORT = -96;
	const EAFNOSUPPORT = -97;
	const EADDRINUSE = -98;
	const EADDRNOTAVAIL = -99;
	const ENETDOWN = -100;
	const ENETUNREACH = -101;
	const ENETRESET = -102;
	const ECONNABORTED = -103;
	const ECONNRESET = -104;
	const ENOBUFS = -105;
	const EISCONN = -106;
	const ENOTCONN = -107;
	const ESHUTDOWN = -108;
	const ETOOMANYREFS = -109;
	const ETIMEDOUT = -110;
	const ECONNREFUSED = -111;
	const EHOSTDOWN = -112;
	const EHOSTUNREACH = -113;
	const EALREADY = -114;
	const EINPROGRESS = -115;
	const ESTALE = -116;
	const EUCLEAN = -117;
	const ENOTNAM = -118;
	const ENAVAIL = -119;
	const EISNAM = -120;
	const EREMOTEIO = -121;
	const EDQUOT = -122;
	const ENOMEDIUM = -123;
	const EMEDIUMTYPE = -124;

}
