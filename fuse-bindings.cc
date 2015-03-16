#include <nan.h>

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

using namespace v8;

enum bindings_ops_t {
  OP_INIT = 0,
  OP_ACCESS,
  OP_STATFS,
  OP_FGETATTR,
  OP_GETATTR,
  OP_FLUSH,
  OP_FSYNC,
  OP_FSYNCDIR,
  OP_READDIR,
  OP_TRUNCATE,
  OP_FTRUNCATE,
  OP_UTIMENS,
  OP_READLINK,
  OP_CHOWN,
  OP_CHMOD,
  OP_SETXATTR,
  OP_GETXATTR,
  OP_OPEN,
  OP_OPENDIR,
  OP_READ,
  OP_WRITE,
  OP_RELEASE,
  OP_RELEASEDIR,
  OP_CREATE,
  OP_UNLINK,
  OP_RENAME,
  OP_LINK,
  OP_SYMLINK,
  OP_MKDIR,
  OP_RMDIR,
  OP_DESTROY
};

static Persistent<Function> buffer_constructor;
static struct stat empty_stat;

// TODO support more than a single mount
static struct {
  // fuse data
  char mnt[1024];
  char mntopts[1024];
  pthread_t thread;
#ifdef __APPLE__
  dispatch_semaphore_t semaphore;
#else
  sem_t semaphore;
#endif
  uv_async_t async;

  // methods
  NanCallback *ops_init;
  NanCallback *ops_access;
  NanCallback *ops_statfs;
  NanCallback *ops_getattr;
  NanCallback *ops_fgetattr;
  NanCallback *ops_flush;
  NanCallback *ops_fsync;
  NanCallback *ops_fsyncdir;
  NanCallback *ops_readdir;
  NanCallback *ops_truncate;
  NanCallback *ops_ftruncate;
  NanCallback *ops_readlink;
  NanCallback *ops_chown;
  NanCallback *ops_chmod;
  NanCallback *ops_setxattr;
  NanCallback *ops_getxattr;
  NanCallback *ops_open;
  NanCallback *ops_opendir;
  NanCallback *ops_read;
  NanCallback *ops_write;
  NanCallback *ops_release;
  NanCallback *ops_releasedir;
  NanCallback *ops_create;
  NanCallback *ops_utimens;
  NanCallback *ops_unlink;
  NanCallback *ops_rename;
  NanCallback *ops_link;
  NanCallback *ops_symlink;
  NanCallback *ops_mkdir;
  NanCallback *ops_rmdir;
  NanCallback *ops_destroy;

  // method data
  bindings_ops_t op;
  NanCallback *callback;
  fuse_fill_dir_t filler; // used in readdir
  struct fuse_file_info *info;
  char *path;
  char *name;
  off_t offset;
  off_t length;
  void *data; // various structs
  int mode;
  int uid;
  int gid;
  int result;
} bindings;

#ifdef __APPLE__
static void bindings_unmount (char *path) {
  unmount(path, 0);
}

static int semaphore_init (dispatch_semaphore_t *sem) {
  *sem = dispatch_semaphore_create(0);
  return *sem == NULL ? -1 : 0;
}

static void semaphore_wait (dispatch_semaphore_t *sem) {
  dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
}

static void semaphore_signal (dispatch_semaphore_t *sem) {
  dispatch_semaphore_signal(*sem);
}
#else
static void bindings_unmount (char *path) {
  umount(path);
}

static int semaphore_init (sem_t *sem) {
  return sem_init(sem, 0, 0);
}

static void semaphore_wait (sem_t *sem) {
  sem_wait(sem);
}

static void semaphore_signal (sem_t *sem) {
  sem_post(sem);
}
#endif

#if (NODE_MODULE_VERSION > NODE_0_10_MODULE_VERSION)
NAN_INLINE v8::Local<v8::Object> bindings_buffer (char *data, size_t length) {
  Local<Object> buf = NanNew(buffer_constructor)->NewInstance(0, NULL);
  buf->Set(NanNew<String>("length"), NanNew<Number>(length));
  buf->SetIndexedPropertiesToExternalArrayData((char *) data, kExternalUnsignedByteArray, length);
  return buf;
}
#else
void noop (char *data, void *hint) {}
NAN_INLINE v8::Local<v8::Object> bindings_buffer (char *data, size_t length) {
  return NanNewBufferHandle(data, length, noop, NULL);
}
#endif

NAN_INLINE static int bindings_call () {
  uv_async_send(&bindings.async);
  semaphore_wait(&bindings.semaphore);
  return bindings.result;
}

static int bindings_truncate (const char *path, off_t size) {
  bindings.op = OP_TRUNCATE;
  bindings.path = (char *) path;
  bindings.length = size;
  return bindings_call();
}

static int bindings_ftruncate (const char *path, off_t size, struct fuse_file_info *info) {
  bindings.op = OP_FTRUNCATE;
  bindings.path = (char *) path;
  bindings.length = size;
  bindings.info = info;
  return bindings_call();
}

static int bindings_getattr (const char *path, struct stat *stat) {
  bindings.op = OP_GETATTR;
  bindings.path = (char *) path;
  bindings.data = stat;
  return bindings_call();
}

static int bindings_fgetattr (const char *path, struct stat *stat, struct fuse_file_info *info) {
  bindings.op = OP_FGETATTR;
  bindings.path = (char *) path;
  bindings.data = stat;
  bindings.info = info;
  return bindings_call();
}

static int bindings_flush (const char *path, struct fuse_file_info *info) {
  bindings.op = OP_FLUSH;
  bindings.path = (char *) path;
  bindings.info = info;
  return bindings_call();
}

static int bindings_fsync (const char *path, int datasync, struct fuse_file_info *info) {
  bindings.op = OP_FSYNC;
  bindings.path = (char *) path;
  bindings.mode = datasync;
  bindings.info = info;
  return bindings_call();
}

static int bindings_fsyncdir (const char *path, int datasync, struct fuse_file_info *info) {
  bindings.op = OP_FSYNCDIR;
  bindings.path = (char *) path;
  bindings.mode = datasync;
  bindings.info = info;
  return bindings_call();
}

static int bindings_readdir (const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *info) {
  bindings.op = OP_READDIR;
  bindings.path = (char *) path;
  bindings.data = buf;
  bindings.filler = filler;
  return bindings_call();
}

static int bindings_readlink (const char *path, char *buf, size_t len) {
  bindings.op = OP_READLINK;
  bindings.path = (char *) path;
  bindings.data = (void *) buf;
  bindings.length = len;
  return bindings_call();
}

static int bindings_chown (const char *path, uid_t uid, gid_t gid) {
  bindings.op = OP_CHOWN;
  bindings.path = (char *) path;
  bindings.uid = uid;
  bindings.gid = gid;
  return bindings_call();
}

static int bindings_chmod (const char *path, mode_t mode) {
  bindings.op = OP_CHMOD;
  bindings.path = (char *) path;
  bindings.mode = mode;
  return bindings_call();
}

#ifdef __APPLE__
static int bindings_setxattr (const char *path, const char *name, const char *value, size_t size, int flags, uint32_t position) {
  bindings.op = OP_SETXATTR;
  bindings.path = (char *) path;
  bindings.name = (char *) name;
  bindings.data = (void *) value;
  bindings.length = size;
  bindings.offset = position;
  bindings.mode = flags;
  return bindings_call();
}

static int bindings_getxattr (const char *path, const char *name, char *value, size_t size, uint32_t position) {
  bindings.op = OP_GETXATTR;
  bindings.path = (char *) path;
  bindings.name = (char *) name;
  bindings.data = (void *) value;
  bindings.length = size;
  bindings.offset = position;
  return bindings_call();
}
#else
static int bindings_setxattr (const char *path, const char *name, const char *value, size_t size, int flags) {
  bindings.op = OP_SETXATTR;
  bindings.path = (char *) path;
  bindings.name = (char *) name;
  bindings.data = (void *) value;
  bindings.length = size;
  bindings.offset = 0;
  bindings.mode = flags;
  return bindings_call();
}

static int bindings_getxattr (const char *path, const char *name, char *value, size_t size) {
  bindings.op = OP_GETXATTR;
  bindings.path = (char *) path;
  bindings.name = (char *) name;
  bindings.data = (void *) value;
  bindings.length = size;
  bindings.offset = 0;
  return bindings_call();
}
#endif

static int bindings_statfs (const char *path, struct statvfs *statfs) {
  bindings.op = OP_STATFS;
  bindings.path = (char *) path;
  bindings.data = statfs;
  return bindings_call();
}

static int bindings_open (const char *path, struct fuse_file_info *info) {
  bindings.op = OP_OPEN;
  bindings.path = (char *) path;
  bindings.mode = info->flags;
  bindings.info = info;
  return bindings_call();
}

static int bindings_opendir (const char *path, struct fuse_file_info *info) {
  bindings.op = OP_OPENDIR;
  bindings.path = (char *) path;
  bindings.mode = info->flags;
  bindings.info = info;
  return bindings_call();
}

static int bindings_read (const char *path, char *buf, size_t len, off_t offset, struct fuse_file_info *info) {
  bindings.op = OP_READ;
  bindings.path = (char *) path;
  bindings.data = (void *) buf;
  bindings.offset = offset;
  bindings.length = len;
  bindings.info = info;
  return bindings_call();
}

static int bindings_write (const char *path, const char *buf, size_t len, off_t offset, struct fuse_file_info * info) {
  bindings.op = OP_WRITE;
  bindings.path = (char *) path;
  bindings.data = (void *) buf;
  bindings.offset = offset;
  bindings.length = len;
  bindings.info = info;
  return bindings_call();
}

static int bindings_release (const char *path, struct fuse_file_info *info) {
  bindings.op = OP_RELEASE;
  bindings.path = (char *) path;
  bindings.info = info;
  return bindings_call();
}

static int bindings_releasedir (const char *path, struct fuse_file_info *info) {
  bindings.op = OP_RELEASEDIR;
  bindings.path = (char *) path;
  bindings.info = info;
  return bindings_call();
}

static int bindings_access (const char *path, int mode) {
  bindings.op = OP_ACCESS;
  bindings.path = (char *) path;
  bindings.mode = mode;
  return bindings_call();
}

static int bindings_create (const char *path, mode_t mode, struct fuse_file_info *info) {
  bindings.op = OP_CREATE;
  bindings.path = (char *) path;
  bindings.mode = mode;
  bindings.info = info;
  return bindings_call();
}

static int bindings_utimens (const char *path, const struct timespec tv[2]) {
  bindings.op = OP_UTIMENS;
  bindings.path = (char *) path;
  bindings.data = (void *) tv;
  return bindings_call();
}

static int bindings_unlink (const char *path) {
  bindings.op = OP_UNLINK;
  bindings.path = (char *) path;
  return bindings_call();
}

static int bindings_rename (const char *src, const char *dest) {
  bindings.op = OP_RENAME;
  bindings.path = (char *) src;
  bindings.data = (void *) dest;
  return bindings_call();
}

static int bindings_link (const char *path, const char *dest) {
  bindings.op = OP_LINK;
  bindings.path = (char *) path;
  bindings.data = (void *) dest;
  return bindings_call();
}

static int bindings_symlink (const char *path, const char *dest) {
  bindings.op = OP_SYMLINK;
  bindings.path = (char *) path;
  bindings.data = (void *) dest;
  return bindings_call();
}

static int bindings_mkdir (const char *path, mode_t mode) {
  bindings.op = OP_MKDIR;
  bindings.path = (char *) path;
  bindings.mode = mode;
  return bindings_call();
}

static int bindings_rmdir (const char *path) {
  bindings.op = OP_RMDIR;
  bindings.path = (char *) path;
  return bindings_call();
}

static void* bindings_init (struct fuse_conn_info *conn) {
  bindings.op = OP_INIT;
  bindings_call();
  return NULL;
}

static void bindings_destroy (void *data) {
  bindings.op = OP_DESTROY;
  bindings_call();
}

static void *bindings_thread (void *) {
  struct fuse_operations ops = { };

  if (bindings.ops_access != NULL) ops.access = bindings_access;
  if (bindings.ops_truncate != NULL) ops.truncate = bindings_truncate;
  if (bindings.ops_ftruncate != NULL) ops.ftruncate = bindings_ftruncate;
  if (bindings.ops_getattr != NULL) ops.getattr = bindings_getattr;
  if (bindings.ops_fgetattr != NULL) ops.fgetattr = bindings_fgetattr;
  if (bindings.ops_flush != NULL) ops.flush = bindings_flush;
  if (bindings.ops_fsync != NULL) ops.fsync = bindings_fsync;
  if (bindings.ops_fsyncdir != NULL) ops.fsyncdir = bindings_fsyncdir;
  if (bindings.ops_readdir != NULL) ops.readdir = bindings_readdir;
  if (bindings.ops_readlink != NULL) ops.readlink = bindings_readlink;
  if (bindings.ops_chown != NULL) ops.chown = bindings_chown;
  if (bindings.ops_chmod != NULL) ops.chmod = bindings_chmod;
  if (bindings.ops_setxattr != NULL) ops.setxattr = bindings_setxattr;
  if (bindings.ops_getxattr != NULL) ops.getxattr = bindings_getxattr;
  if (bindings.ops_statfs != NULL) ops.statfs = bindings_statfs;
  if (bindings.ops_open != NULL) ops.open = bindings_open;
  if (bindings.ops_opendir != NULL) ops.opendir = bindings_opendir;
  if (bindings.ops_read != NULL) ops.read = bindings_read;
  if (bindings.ops_write != NULL) ops.write = bindings_write;
  if (bindings.ops_release != NULL) ops.release = bindings_release;
  if (bindings.ops_releasedir != NULL) ops.releasedir = bindings_releasedir;
  if (bindings.ops_create != NULL) ops.create = bindings_create;
  if (bindings.ops_utimens != NULL) ops.utimens = bindings_utimens;
  if (bindings.ops_unlink != NULL) ops.unlink = bindings_unlink;
  if (bindings.ops_rename != NULL) ops.rename = bindings_rename;
  if (bindings.ops_link != NULL) ops.link = bindings_link;
  if (bindings.ops_symlink != NULL) ops.symlink = bindings_symlink;
  if (bindings.ops_mkdir != NULL) ops.mkdir = bindings_mkdir;
  if (bindings.ops_rmdir != NULL) ops.rmdir = bindings_rmdir;
  if (bindings.ops_init != NULL) ops.init = bindings_init;
  if (bindings.ops_destroy != NULL) ops.destroy = bindings_destroy;

  char *argv[] = {
    (char *) "dummy",
    (char *) "-s",
    (char *) "-f",
    (char *) bindings.mnt,
    (char *) bindings.mntopts
  };

  bindings_unmount(bindings.mnt); // should probably throw instead if mounted

  if (fuse_main(!strcmp(bindings.mntopts, "-o") ? 4 : 5, argv, &ops, NULL)) {
    // TODO: error handle somehow
    printf("fuse-binding: mount failed\n");
  }

  return NULL;
}

NAN_INLINE static void bindings_set_date (Local<Date> date, struct timespec *out) {
  double ms = date->NumberValue();
  time_t secs = (time_t)(ms / 1000.0);
  time_t rem = ms - (1000.0 * secs);
  time_t ns = rem * 1000000.0;
  out->tv_sec = secs;
  out->tv_nsec = ns;
}

NAN_INLINE static void bindings_set_stat (Local<Object> obj, struct stat *stat) {
  if (obj->Has(NanNew<String>("dev"))) stat->st_dev = obj->Get(NanNew<String>("dev"))->NumberValue();
  if (obj->Has(NanNew<String>("ino"))) stat->st_ino = obj->Get(NanNew<String>("ino"))->NumberValue();
  if (obj->Has(NanNew<String>("mode"))) stat->st_mode = obj->Get(NanNew<String>("mode"))->Uint32Value();
  if (obj->Has(NanNew<String>("nlink"))) stat->st_nlink = obj->Get(NanNew<String>("nlink"))->NumberValue();
  if (obj->Has(NanNew<String>("uid"))) stat->st_uid = obj->Get(NanNew<String>("uid"))->NumberValue();
  if (obj->Has(NanNew<String>("gid"))) stat->st_gid = obj->Get(NanNew<String>("gid"))->NumberValue();
  if (obj->Has(NanNew<String>("rdev"))) stat->st_rdev = obj->Get(NanNew<String>("rdev"))->NumberValue();
  if (obj->Has(NanNew<String>("size"))) stat->st_size = obj->Get(NanNew<String>("size"))->NumberValue();
  if (obj->Has(NanNew<String>("blksize"))) stat->st_blksize = obj->Get(NanNew<String>("blksize"))->NumberValue();
  if (obj->Has(NanNew<String>("blocks"))) stat->st_blocks = obj->Get(NanNew<String>("blocks"))->NumberValue();
#ifdef __APPLE__
  if (obj->Has(NanNew<String>("mtime"))) bindings_set_date(obj->Get(NanNew("mtime")).As<Date>(), &stat->st_mtimespec);
  if (obj->Has(NanNew<String>("ctime"))) bindings_set_date(obj->Get(NanNew("ctime")).As<Date>(), &stat->st_ctimespec);
  if (obj->Has(NanNew<String>("atime"))) bindings_set_date(obj->Get(NanNew("atime")).As<Date>(), &stat->st_atimespec);
#else
  if (obj->Has(NanNew<String>("mtime"))) bindings_set_date(obj->Get(NanNew("mtime")).As<Date>(), &stat->st_mtim);
  if (obj->Has(NanNew<String>("ctime"))) bindings_set_date(obj->Get(NanNew("ctime")).As<Date>(), &stat->st_ctim);
  if (obj->Has(NanNew<String>("atime"))) bindings_set_date(obj->Get(NanNew("atime")).As<Date>(), &stat->st_atim);
#endif
}

NAN_INLINE static void bindings_set_utimens (Local<Object> obj, struct timespec tv[2]) {
  if (obj->Has(NanNew<String>("atime"))) bindings_set_date(obj->Get(NanNew("atime")).As<Date>(), &tv[0]);
  if (obj->Has(NanNew<String>("mtime"))) bindings_set_date(obj->Get(NanNew("mtime")).As<Date>(), &tv[1]);
}

NAN_INLINE static void bindings_set_statfs (Local<Object> obj, struct statvfs *statfs) { // from http://linux.die.net/man/2/stat
  if (obj->Has(NanNew<String>("bsize"))) statfs->f_bsize = obj->Get(NanNew<String>("bsize"))->Uint32Value();
  if (obj->Has(NanNew<String>("frsize"))) statfs->f_frsize = obj->Get(NanNew<String>("frsize"))->Uint32Value();
  if (obj->Has(NanNew<String>("blocks"))) statfs->f_blocks = obj->Get(NanNew<String>("blocks"))->Uint32Value();
  if (obj->Has(NanNew<String>("bfree"))) statfs->f_bfree = obj->Get(NanNew<String>("bfree"))->Uint32Value();
  if (obj->Has(NanNew<String>("bavail"))) statfs->f_bavail = obj->Get(NanNew<String>("bavail"))->Uint32Value();
  if (obj->Has(NanNew<String>("files"))) statfs->f_files = obj->Get(NanNew<String>("files"))->Uint32Value();
  if (obj->Has(NanNew<String>("ffree"))) statfs->f_ffree = obj->Get(NanNew<String>("ffree"))->Uint32Value();
  if (obj->Has(NanNew<String>("favail"))) statfs->f_favail = obj->Get(NanNew<String>("favail"))->Uint32Value();
  if (obj->Has(NanNew<String>("fsid"))) statfs->f_fsid = obj->Get(NanNew<String>("fsid"))->Uint32Value();
  if (obj->Has(NanNew<String>("flag"))) statfs->f_flag = obj->Get(NanNew<String>("flag"))->Uint32Value();
  if (obj->Has(NanNew<String>("namemax"))) statfs->f_namemax = obj->Get(NanNew<String>("namemax"))->Uint32Value();
}

NAN_INLINE static void bindings_set_dirs (Local<Array> dirs) {
  for (uint32_t i = 0; i < dirs->Length(); i++) {
    NanUtf8String dir(dirs->Get(i));
    if (bindings.filler(bindings.data, *dir, &empty_stat, 0)) break;
  }
}

NAN_INLINE static void bindings_set_fd (Local<Number> fd) {
  bindings.info->fh = fd->Uint32Value();
}

NAN_METHOD(OpCallback) {
  NanScope();
  bindings.result = args[0]->Uint32Value();

  if (!bindings.result) {
    switch (bindings.op) {
      case OP_STATFS: {
        if (args.Length() > 1 && args[1]->IsObject()) bindings_set_statfs(args[1].As<Object>(), (struct statvfs *) bindings.data);
      }
      break;

      case OP_GETATTR:
      case OP_FGETATTR: {
        if (args.Length() > 1 && args[1]->IsObject()) bindings_set_stat(args[1].As<Object>(), (struct stat *) bindings.data);
      }
      break;

      case OP_READDIR: {
        if (args.Length() > 1 && args[1]->IsArray()) bindings_set_dirs(args[1].As<Array>());
      }
      break;

      case OP_CREATE:
      case OP_OPEN:
      case OP_OPENDIR: {
        if (args.Length() > 1 && args[1]->IsNumber()) bindings_set_fd(args[1].As<Number>());
      }
      break;

      case OP_UTIMENS: {
        if (args.Length() > 1 && args[1]->IsObject()) bindings_set_utimens(args[1].As<Object>(), (struct timespec *) bindings.data);
      }
      break;

      case OP_INIT:
      case OP_ACCESS:
      case OP_FLUSH:
      case OP_FSYNC:
      case OP_FSYNCDIR:
      case OP_TRUNCATE:
      case OP_FTRUNCATE:
      case OP_READLINK:
      case OP_CHOWN:
      case OP_CHMOD:
      case OP_SETXATTR:
      case OP_GETXATTR:
      case OP_READ:
      case OP_WRITE:
      case OP_RELEASE:
      case OP_RELEASEDIR:
      case OP_UNLINK:
      case OP_RENAME:
      case OP_LINK:
      case OP_SYMLINK:
      case OP_MKDIR:
      case OP_RMDIR:
      case OP_DESTROY:
      break;
    }
  }

  semaphore_signal(&bindings.semaphore);
  NanReturnUndefined();
}

NAN_INLINE static void bindings_call_op (NanCallback *fn, int argc, Local<Value> *argv) {
  if (fn == NULL) semaphore_signal(&bindings.semaphore);
  else fn->Call(argc, argv);
}

static void bindings_dispatch (uv_async_t* handle, int status) {
  Local<Function> callback = bindings.callback->GetFunction();
  bindings.result = -1;

  switch (bindings.op) {
    case OP_INIT: {
      Local<Value> tmp[] = {callback};
      bindings_call_op(bindings.ops_init, 1, tmp);
    }
    return;

    case OP_STATFS: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), callback};
      bindings_call_op(bindings.ops_statfs, 2, tmp);
    }
    return;

    case OP_FGETATTR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), callback};
      bindings_call_op(bindings.ops_fgetattr, 3, tmp);
    }
    return;

    case OP_GETATTR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), callback};
      bindings_call_op(bindings.ops_getattr, 2, tmp);
    }
    return;

    case OP_READDIR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), callback};
      bindings_call_op(bindings.ops_readdir, 2, tmp);
    }
    return;

    case OP_CREATE: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_create, 3, tmp);
    }
    return;

    case OP_TRUNCATE: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.offset), callback};
      bindings_call_op(bindings.ops_truncate, 3, tmp);
    }
    return;

    case OP_FTRUNCATE: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), NanNew<Number>(bindings.offset), callback};
      bindings_call_op(bindings.ops_ftruncate, 4, tmp);
    }
    return;

    case OP_ACCESS: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_access, 3, tmp);
    }
    return;

    case OP_OPEN: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_open, 3, tmp);
    }
    return;

    case OP_OPENDIR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_opendir, 3, tmp);
    }
    return;

    case OP_WRITE: {
      Local<Value> tmp[] = {
        NanNew<String>(bindings.path),
        NanNew<Number>(bindings.info->fh),
        bindings_buffer((char *) bindings.data, bindings.length),
        NanNew<Number>(bindings.length),
        NanNew<Number>(bindings.offset),
        callback
      };
      bindings_call_op(bindings.ops_write, 6, tmp);
    }
    return;

    case OP_READ: {
      Local<Value> tmp[] = {
        NanNew<String>(bindings.path),
        NanNew<Number>(bindings.info->fh),
        bindings_buffer((char *) bindings.data, bindings.length),
        NanNew<Number>(bindings.length),
        NanNew<Number>(bindings.offset),
        callback
      };
      bindings_call_op(bindings.ops_read, 6, tmp);
    }
    return;

    case OP_RELEASE: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), callback};
      bindings_call_op(bindings.ops_release, 3, tmp);
    }
    return;

    case OP_RELEASEDIR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), callback};
      bindings_call_op(bindings.ops_releasedir, 3, tmp);
    }
    return;

    case OP_UNLINK: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), callback};
      bindings_call_op(bindings.ops_unlink, 2, tmp);
    }
    return;

    case OP_RENAME: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<String>((char *) bindings.data), callback};
      bindings_call_op(bindings.ops_rename, 3, tmp);
    }
    return;

    case OP_LINK: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<String>((char *) bindings.data), callback};
      bindings_call_op(bindings.ops_link, 3, tmp);
    }
    return;

    case OP_SYMLINK: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<String>((char *) bindings.data), callback};
      bindings_call_op(bindings.ops_symlink, 3, tmp);
    }
    return;

    case OP_CHMOD: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_chmod, 3, tmp);
    }
    return;

    case OP_CHOWN: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.uid), NanNew<Number>(bindings.gid), callback};
      bindings_call_op(bindings.ops_chown, 4, tmp);
    }
    return;

    case OP_READLINK: {
      Local<Value> tmp[] = {
        NanNew<String>(bindings.path),
        bindings_buffer((char *) bindings.data, bindings.length),
        NanNew<Number>(bindings.length),
        callback
      };
      bindings_call_op(bindings.ops_readlink, 4, tmp);
    }
    return;

    case OP_SETXATTR: {
      Local<Value> tmp[] = {
        NanNew<String>(bindings.path),
        NanNew<String>(bindings.name),
        bindings_buffer((char *) bindings.data, bindings.length),
        NanNew<Number>(bindings.length),
        NanNew<Number>(bindings.offset),
        NanNew<Number>(bindings.mode),
        callback
      };
      bindings_call_op(bindings.ops_setxattr, 7, tmp);
    }
    return;

    case OP_GETXATTR: {
      Local<Value> tmp[] = {
        NanNew<String>(bindings.path),
        NanNew<String>(bindings.name),
        bindings_buffer((char *) bindings.data, bindings.length),
        NanNew<Number>(bindings.length),
        NanNew<Number>(bindings.offset),
        callback
      };
      bindings_call_op(bindings.ops_getxattr, 6, tmp);
    }
    return;

    case OP_MKDIR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_mkdir, 3, tmp);
    }
    return;

    case OP_RMDIR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), callback};
      bindings_call_op(bindings.ops_rmdir, 2, tmp);
    }
    return;

    case OP_DESTROY: {
      Local<Value> tmp[] = {callback};
      bindings_call_op(bindings.ops_destroy, 1, tmp);
    }
    return;

    case OP_UTIMENS: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), callback};
      bindings_call_op(bindings.ops_utimens, 2, tmp);
    }
    return;

    case OP_FLUSH: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), callback};
      bindings_call_op(bindings.ops_flush, 3, tmp);
    }
    return;

    case OP_FSYNC: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_fsync, 4, tmp);
    }
    return;

    case OP_FSYNCDIR: {
      Local<Value> tmp[] = {NanNew<String>(bindings.path), NanNew<Number>(bindings.info->fh), NanNew<Number>(bindings.mode), callback};
      bindings_call_op(bindings.ops_fsyncdir, 4, tmp);
    }
    return;
  }

  semaphore_signal(&bindings.semaphore);
}

static void bindings_append_opt (char *name) {
  if (strcmp(bindings.mntopts, "-o")) strcat(bindings.mntopts, ",");
  strcat(bindings.mntopts, name);
}

NAN_METHOD(Mount) {
  NanScope();

  memset(&empty_stat, 0, sizeof(empty_stat)); // zero empty stat

  NanUtf8String path(args[0]);
  Local<Object> ops = args[1].As<Object>();

  bindings.ops_init = ops->Has(NanNew<String>("init")) ? new NanCallback(ops->Get(NanNew<String>("init")).As<Function>()) : NULL;
  bindings.ops_access = ops->Has(NanNew<String>("access")) ? new NanCallback(ops->Get(NanNew<String>("access")).As<Function>()) : NULL;
  bindings.ops_statfs = ops->Has(NanNew<String>("statfs")) ? new NanCallback(ops->Get(NanNew<String>("statfs")).As<Function>()) : NULL;
  bindings.ops_getattr = ops->Has(NanNew<String>("getattr")) ? new NanCallback(ops->Get(NanNew<String>("getattr")).As<Function>()) : NULL;
  bindings.ops_fgetattr = ops->Has(NanNew<String>("fgetattr")) ? new NanCallback(ops->Get(NanNew<String>("fgetattr")).As<Function>()) : NULL;
  bindings.ops_flush = ops->Has(NanNew<String>("flush")) ? new NanCallback(ops->Get(NanNew<String>("flush")).As<Function>()) : NULL;
  bindings.ops_fsync = ops->Has(NanNew<String>("fsync")) ? new NanCallback(ops->Get(NanNew<String>("fsync")).As<Function>()) : NULL;
  bindings.ops_fsyncdir = ops->Has(NanNew<String>("fsyncdir")) ? new NanCallback(ops->Get(NanNew<String>("fsyncdir")).As<Function>()) : NULL;
  bindings.ops_readdir = ops->Has(NanNew<String>("readdir")) ? new NanCallback(ops->Get(NanNew<String>("readdir")).As<Function>()) : NULL;
  bindings.ops_truncate = ops->Has(NanNew<String>("truncate")) ? new NanCallback(ops->Get(NanNew<String>("truncate")).As<Function>()) : NULL;
  bindings.ops_ftruncate = ops->Has(NanNew<String>("ftruncate")) ? new NanCallback(ops->Get(NanNew<String>("ftruncate")).As<Function>()) : NULL;
  bindings.ops_readlink = ops->Has(NanNew<String>("readlink")) ? new NanCallback(ops->Get(NanNew<String>("readlink")).As<Function>()) : NULL;
  bindings.ops_chown = ops->Has(NanNew<String>("chown")) ? new NanCallback(ops->Get(NanNew<String>("chown")).As<Function>()) : NULL;
  bindings.ops_chmod = ops->Has(NanNew<String>("chmod")) ? new NanCallback(ops->Get(NanNew<String>("chmod")).As<Function>()) : NULL;
  bindings.ops_setxattr = ops->Has(NanNew<String>("setxattr")) ? new NanCallback(ops->Get(NanNew<String>("setxattr")).As<Function>()) : NULL;
  bindings.ops_getxattr = ops->Has(NanNew<String>("getxattr")) ? new NanCallback(ops->Get(NanNew<String>("getxattr")).As<Function>()) : NULL;
  bindings.ops_open = ops->Has(NanNew<String>("open")) ? new NanCallback(ops->Get(NanNew<String>("open")).As<Function>()) : NULL;
  bindings.ops_opendir = ops->Has(NanNew<String>("opendir")) ? new NanCallback(ops->Get(NanNew<String>("opendir")).As<Function>()) : NULL;
  bindings.ops_read = ops->Has(NanNew<String>("read")) ? new NanCallback(ops->Get(NanNew<String>("read")).As<Function>()) : NULL;
  bindings.ops_write = ops->Has(NanNew<String>("write")) ? new NanCallback(ops->Get(NanNew<String>("write")).As<Function>()) : NULL;
  bindings.ops_release = ops->Has(NanNew<String>("release")) ? new NanCallback(ops->Get(NanNew<String>("release")).As<Function>()) : NULL;
  bindings.ops_releasedir = ops->Has(NanNew<String>("releasedir")) ? new NanCallback(ops->Get(NanNew<String>("releasedir")).As<Function>()) : NULL;
  bindings.ops_create = ops->Has(NanNew<String>("create")) ? new NanCallback(ops->Get(NanNew<String>("create")).As<Function>()) : NULL;
  bindings.ops_utimens = ops->Has(NanNew<String>("utimens")) ? new NanCallback(ops->Get(NanNew<String>("utimens")).As<Function>()) : NULL;
  bindings.ops_unlink = ops->Has(NanNew<String>("unlink")) ? new NanCallback(ops->Get(NanNew<String>("unlink")).As<Function>()) : NULL;
  bindings.ops_rename = ops->Has(NanNew<String>("rename")) ? new NanCallback(ops->Get(NanNew<String>("rename")).As<Function>()) : NULL;
  bindings.ops_link = ops->Has(NanNew<String>("link")) ? new NanCallback(ops->Get(NanNew<String>("link")).As<Function>()) : NULL;
  bindings.ops_symlink = ops->Has(NanNew<String>("symlink")) ? new NanCallback(ops->Get(NanNew<String>("symlink")).As<Function>()) : NULL;
  bindings.ops_mkdir = ops->Has(NanNew<String>("mkdir")) ? new NanCallback(ops->Get(NanNew<String>("mkdir")).As<Function>()) : NULL;
  bindings.ops_rmdir = ops->Has(NanNew<String>("rmdir")) ? new NanCallback(ops->Get(NanNew<String>("rmdir")).As<Function>()) : NULL;
  bindings.ops_destroy = ops->Has(NanNew<String>("destroy")) ? new NanCallback(ops->Get(NanNew<String>("destroy")).As<Function>()) : NULL;

  bindings.callback = new NanCallback(NanNew<FunctionTemplate>(OpCallback)->GetFunction());
  stpcpy(bindings.mnt, *path);
  stpcpy(bindings.mntopts, "-o");

  Local<Array> options = ops->Get(NanNew<String>("options")).As<Array>();
  if (options->IsArray()) {
    for (uint32_t i = 0; i < options->Length(); i++) {
      NanUtf8String option(options->Get(i));
      bindings_append_opt(*option);
    }
  }

  // yolo
  semaphore_init(&bindings.semaphore);
  uv_async_init(uv_default_loop(), &bindings.async, (uv_async_cb) bindings_dispatch);

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_create(&bindings.thread, &attr, bindings_thread, NULL);

  NanReturnUndefined();
}

NAN_METHOD(UnmountSync) {
  NanScope();
  NanUtf8String path(args[0]);
  bindings_unmount(*path);
  NanReturnUndefined();
}

class UnmountWorker : public NanAsyncWorker {
 public:
  UnmountWorker(NanCallback *callback, char *path)
    : NanAsyncWorker(callback), path(path) {}
  ~UnmountWorker() {}

  void Execute () {
    bindings_unmount(path);
    free(path);
  }

  void HandleOKCallback () {
    NanScope();
    callback->Call(0, NULL);
  }

 private:
  char *path;
};

NAN_METHOD(SetBuffer) {
  NanScope();
  NanAssignPersistent(buffer_constructor, args[0].As<Function>());
  NanReturnUndefined();
}

NAN_METHOD(Unmount) {
  NanScope();
  NanUtf8String path(args[0]);
  Local<Function> callback = args[1].As<Function>();

  char *path_alloc = (char *) malloc(1024);
  stpcpy(path_alloc, *path);

  NanAsyncQueueWorker(new UnmountWorker(new NanCallback(callback), path_alloc));
  NanReturnUndefined();
}

void Init(Handle<Object> exports) {
  exports->Set(NanNew("setBuffer"), NanNew<FunctionTemplate>(SetBuffer)->GetFunction());
  exports->Set(NanNew("mount"), NanNew<FunctionTemplate>(Mount)->GetFunction());
  exports->Set(NanNew("unmount"), NanNew<FunctionTemplate>(Unmount)->GetFunction());
  exports->Set(NanNew("unmountSync"), NanNew<FunctionTemplate>(UnmountSync)->GetFunction());
}

NODE_MODULE(fuse_bindings, Init)