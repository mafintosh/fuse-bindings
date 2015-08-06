#include <nan.h>

#define FUSE_USE_VERSION 29

#include <fuse.h>
#include <fuse_opt.h>
// #include <fuse_lowlevel.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include "abstractions.h"

using namespace v8;

enum bindings_ops_t {
  OP_INIT = 0,
  OP_ERROR,
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
  OP_MKNOD,
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
static NanCallback *callback_constructor;
static struct stat empty_stat;

struct bindings_t {
  int index;
  int gc;

  // fuse context
  int context_uid;
  int context_gid;
  int context_pid;

  // fuse data
  char mnt[1024];
  char mntopts[1024];
  thread_t thread;
  bindings_sem_t semaphore;
  uv_async_t async;

  // methods
  NanCallback *ops_init;
  NanCallback *ops_error;
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
  NanCallback *ops_mknod;
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

  NanCallback *callback;

  // method data
  bindings_ops_t op;
  fuse_fill_dir_t filler; // used in readdir
  struct fuse_file_info *info;
  char *path;
  char *name;
  FUSE_OFF_T offset;
  FUSE_OFF_T length;
  void *data; // various structs
  int mode;
  int dev;
  int uid;
  int gid;
  int result;
};

static bindings_t *bindings_mounted[1024];
static int bindings_mounted_count = 0;
static bindings_t *bindings_current = NULL;

static bindings_t *bindings_find_mounted (char *path) {
  for (int i = 0; i < bindings_mounted_count; i++) {
    bindings_t *b = bindings_mounted[i];
    if (b != NULL && !b->gc && !strcmp(b->mnt, path)) {
      return b;
    }
  }
  return NULL;
}

static void bindings_fusermount (char *path) {
  fusermount(path);
}

static void bindings_unmount (char *path) {
  mutex_lock(&mutex);
  bindings_t *b = bindings_find_mounted(path);
  if (b != NULL) b->gc = 1;
  bindings_fusermount(path);
  if (b != NULL) thread_join(b->thread);
  mutex_unlock(&mutex);
}


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

NAN_INLINE static int bindings_call (bindings_t *b) {
  uv_async_send(&(b->async));
  semaphore_wait(&(b->semaphore));
  return b->result;
}

static bindings_t *bindings_get_context () {
  fuse_context *ctx = fuse_get_context();
  bindings_t *b = (bindings_t *) ctx->private_data;
  b->context_pid = ctx->pid;
  b->context_uid = ctx->uid;
  b->context_gid = ctx->gid;
  return b;
}

static int bindings_mknod (const char *path, mode_t mode, dev_t dev) {
  bindings_t *b = bindings_get_context();

  b->op = OP_MKNOD;
  b->path = (char *) path;
  b->mode = mode;
  b->dev = dev;

  return bindings_call(b);
}

static int bindings_truncate (const char *path, FUSE_OFF_T size) {
  bindings_t *b = bindings_get_context();

  b->op = OP_TRUNCATE;
  b->path = (char *) path;
  b->length = size;

  return bindings_call(b);
}

static int bindings_ftruncate (const char *path, FUSE_OFF_T size, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_FTRUNCATE;
  b->path = (char *) path;
  b->length = size;
  b->info = info;

  return bindings_call(b);
}

static int bindings_getattr (const char *path, struct stat *stat) {
  bindings_t *b = bindings_get_context();

  b->op = OP_GETATTR;
  b->path = (char *) path;
  b->data = stat;

  return bindings_call(b);
}

static int bindings_fgetattr (const char *path, struct stat *stat, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_FGETATTR;
  b->path = (char *) path;
  b->data = stat;
  b->info = info;

  return bindings_call(b);
}

static int bindings_flush (const char *path, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_FLUSH;
  b->path = (char *) path;
  b->info = info;

  return bindings_call(b);
}

static int bindings_fsync (const char *path, int datasync, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_FSYNC;
  b->path = (char *) path;
  b->mode = datasync;
  b->info = info;

  return bindings_call(b);
}

static int bindings_fsyncdir (const char *path, int datasync, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_FSYNCDIR;
  b->path = (char *) path;
  b->mode = datasync;
  b->info = info;

  return bindings_call(b);
}

static int bindings_readdir (const char *path, void *buf, fuse_fill_dir_t filler, FUSE_OFF_T offset, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_READDIR;
  b->path = (char *) path;
  b->data = buf;
  b->filler = filler;

  return bindings_call(b);
}

static int bindings_readlink (const char *path, char *buf, size_t len) {
  bindings_t *b = bindings_get_context();

  b->op = OP_READLINK;
  b->path = (char *) path;
  b->data = (void *) buf;
  b->length = len;

  return bindings_call(b);
}

static int bindings_chown (const char *path, uid_t uid, gid_t gid) {
  bindings_t *b = bindings_get_context();

  b->op = OP_CHOWN;
  b->path = (char *) path;
  b->uid = uid;
  b->gid = gid;

  return bindings_call(b);
}

static int bindings_chmod (const char *path, mode_t mode) {
  bindings_t *b = bindings_get_context();

  b->op = OP_CHMOD;
  b->path = (char *) path;
  b->mode = mode;

  return bindings_call(b);
}

#ifdef __APPLE__
static int bindings_setxattr (const char *path, const char *name, const char *value, size_t size, int flags, uint32_t position) {
  bindings_t *b = bindings_get_context();

  b->op = OP_SETXATTR;
  b->path = (char *) path;
  b->name = (char *) name;
  b->data = (void *) value;
  b->length = size;
  b->offset = position;
  b->mode = flags;

  return bindings_call(b);
}

static int bindings_getxattr (const char *path, const char *name, char *value, size_t size, uint32_t position) {
  bindings_t *b = bindings_get_context();

  b->op = OP_GETXATTR;
  b->path = (char *) path;
  b->name = (char *) name;
  b->data = (void *) value;
  b->length = size;
  b->offset = position;

  return bindings_call(b);
}
#else
static int bindings_setxattr (const char *path, const char *name, const char *value, size_t size, int flags) {
  bindings_t *b = bindings_get_context();

  b->op = OP_SETXATTR;
  b->path = (char *) path;
  b->name = (char *) name;
  b->data = (void *) value;
  b->length = size;
  b->offset = 0;
  b->mode = flags;

  return bindings_call(b);
}

static int bindings_getxattr (const char *path, const char *name, char *value, size_t size) {
  bindings_t *b = bindings_get_context();

  b->op = OP_GETXATTR;
  b->path = (char *) path;
  b->name = (char *) name;
  b->data = (void *) value;
  b->length = size;
  b->offset = 0;

  return bindings_call(b);
}
#endif

static int bindings_statfs (const char *path, struct statvfs *statfs) {
  bindings_t *b = bindings_get_context();

  b->op = OP_STATFS;
  b->path = (char *) path;
  b->data = statfs;

  return bindings_call(b);
}

static int bindings_open (const char *path, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_OPEN;
  b->path = (char *) path;
  b->mode = info->flags;
  b->info = info;

  return bindings_call(b);
}

static int bindings_opendir (const char *path, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_OPENDIR;
  b->path = (char *) path;
  b->mode = info->flags;
  b->info = info;

  return bindings_call(b);
}

static int bindings_read (const char *path, char *buf, size_t len, FUSE_OFF_T offset, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_READ;
  b->path = (char *) path;
  b->data = (void *) buf;
  b->offset = offset;
  b->length = len;
  b->info = info;

  return bindings_call(b);
}

static int bindings_write (const char *path, const char *buf, size_t len, FUSE_OFF_T offset, struct fuse_file_info * info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_WRITE;
  b->path = (char *) path;
  b->data = (void *) buf;
  b->offset = offset;
  b->length = len;
  b->info = info;

  return bindings_call(b);
}

static int bindings_release (const char *path, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_RELEASE;
  b->path = (char *) path;
  b->info = info;

  return bindings_call(b);
}

static int bindings_releasedir (const char *path, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_RELEASEDIR;
  b->path = (char *) path;
  b->info = info;

  return bindings_call(b);
}

static int bindings_access (const char *path, int mode) {
  bindings_t *b = bindings_get_context();

  b->op = OP_ACCESS;
  b->path = (char *) path;
  b->mode = mode;

  return bindings_call(b);
}

static int bindings_create (const char *path, mode_t mode, struct fuse_file_info *info) {
  bindings_t *b = bindings_get_context();

  b->op = OP_CREATE;
  b->path = (char *) path;
  b->mode = mode;
  b->info = info;

  return bindings_call(b);
}

static int bindings_utimens (const char *path, const struct timespec tv[2]) {
  bindings_t *b = bindings_get_context();

  b->op = OP_UTIMENS;
  b->path = (char *) path;
  b->data = (void *) tv;

  return bindings_call(b);
}

static int bindings_unlink (const char *path) {
  bindings_t *b = bindings_get_context();

  b->op = OP_UNLINK;
  b->path = (char *) path;

  return bindings_call(b);
}

static int bindings_rename (const char *src, const char *dest) {
  bindings_t *b = bindings_get_context();

  b->op = OP_RENAME;
  b->path = (char *) src;
  b->data = (void *) dest;

  return bindings_call(b);
}

static int bindings_link (const char *path, const char *dest) {
  bindings_t *b = bindings_get_context();

  b->op = OP_LINK;
  b->path = (char *) path;
  b->data = (void *) dest;

  return bindings_call(b);
}

static int bindings_symlink (const char *path, const char *dest) {
  bindings_t *b = bindings_get_context();

  b->op = OP_SYMLINK;
  b->path = (char *) path;
  b->data = (void *) dest;

  return bindings_call(b);
}

static int bindings_mkdir (const char *path, mode_t mode) {
  bindings_t *b = bindings_get_context();

  b->op = OP_MKDIR;
  b->path = (char *) path;
  b->mode = mode;

  return bindings_call(b);
}

static int bindings_rmdir (const char *path) {
  bindings_t *b = bindings_get_context();

  b->op = OP_RMDIR;
  b->path = (char *) path;

  return bindings_call(b);
}

static void* bindings_init (struct fuse_conn_info *conn) {
  bindings_t *b = bindings_get_context();

  b->op = OP_INIT;

  bindings_call(b);
  return b;
}

static void bindings_destroy (void *data) {
  bindings_t *b = bindings_get_context();

  b->op = OP_DESTROY;

  bindings_call(b);
}

static void bindings_free (bindings_t *b) {
  if (b->ops_access != NULL) delete b->ops_access;
  if (b->ops_truncate != NULL) delete b->ops_truncate;
  if (b->ops_ftruncate != NULL) delete b->ops_ftruncate;
  if (b->ops_getattr != NULL) delete b->ops_getattr;
  if (b->ops_fgetattr != NULL) delete b->ops_fgetattr;
  if (b->ops_flush != NULL) delete b->ops_flush;
  if (b->ops_fsync != NULL) delete b->ops_fsync;
  if (b->ops_fsyncdir != NULL) delete b->ops_fsyncdir;
  if (b->ops_readdir != NULL) delete b->ops_readdir;
  if (b->ops_readlink != NULL) delete b->ops_readlink;
  if (b->ops_chown != NULL) delete b->ops_chown;
  if (b->ops_chmod != NULL) delete b->ops_chmod;
  if (b->ops_mknod != NULL) delete b->ops_mknod;
  if (b->ops_setxattr != NULL) delete b->ops_setxattr;
  if (b->ops_getxattr != NULL) delete b->ops_getxattr;
  if (b->ops_statfs != NULL) delete b->ops_statfs;
  if (b->ops_open != NULL) delete b->ops_open;
  if (b->ops_opendir != NULL) delete b->ops_opendir;
  if (b->ops_read != NULL) delete b->ops_read;
  if (b->ops_write != NULL) delete b->ops_write;
  if (b->ops_release != NULL) delete b->ops_release;
  if (b->ops_releasedir != NULL) delete b->ops_releasedir;
  if (b->ops_create != NULL) delete b->ops_create;
  if (b->ops_utimens != NULL) delete b->ops_utimens;
  if (b->ops_unlink != NULL) delete b->ops_unlink;
  if (b->ops_rename != NULL) delete b->ops_rename;
  if (b->ops_link != NULL) delete b->ops_link;
  if (b->ops_symlink != NULL) delete b->ops_symlink;
  if (b->ops_mkdir != NULL) delete b->ops_mkdir;
  if (b->ops_rmdir != NULL) delete b->ops_rmdir;
  if (b->ops_init != NULL) delete b->ops_init;
  if (b->ops_destroy != NULL) delete b->ops_destroy;
  if (b->callback != NULL) delete b->callback;

  bindings_mounted[b->index] = NULL;
  while (bindings_mounted_count > 0 && bindings_mounted[bindings_mounted_count - 1] == NULL) {
    bindings_mounted_count--;
  }

  free(b);
}

static void bindings_on_close (uv_handle_t *handle) {
  mutex_lock(&mutex);
  bindings_free((bindings_t *) handle->data);
  mutex_unlock(&mutex);
}

static thread_fn_rtn_t bindings_thread (void *data) {
  bindings_t *b = (bindings_t *) data;

  struct fuse_operations ops = { };

  if (b->ops_access != NULL) ops.access = bindings_access;
  if (b->ops_truncate != NULL) ops.truncate = bindings_truncate;
  if (b->ops_ftruncate != NULL) ops.ftruncate = bindings_ftruncate;
  if (b->ops_getattr != NULL) ops.getattr = bindings_getattr;
  if (b->ops_fgetattr != NULL) ops.fgetattr = bindings_fgetattr;
  if (b->ops_flush != NULL) ops.flush = bindings_flush;
  if (b->ops_fsync != NULL) ops.fsync = bindings_fsync;
  if (b->ops_fsyncdir != NULL) ops.fsyncdir = bindings_fsyncdir;
  if (b->ops_readdir != NULL) ops.readdir = bindings_readdir;
  if (b->ops_readlink != NULL) ops.readlink = bindings_readlink;
  if (b->ops_chown != NULL) ops.chown = bindings_chown;
  if (b->ops_chmod != NULL) ops.chmod = bindings_chmod;
  if (b->ops_mknod != NULL) ops.mknod = bindings_mknod;
  if (b->ops_setxattr != NULL) ops.setxattr = bindings_setxattr;
  if (b->ops_getxattr != NULL) ops.getxattr = bindings_getxattr;
  if (b->ops_statfs != NULL) ops.statfs = bindings_statfs;
  if (b->ops_open != NULL) ops.open = bindings_open;
  if (b->ops_opendir != NULL) ops.opendir = bindings_opendir;
  if (b->ops_read != NULL) ops.read = bindings_read;
  if (b->ops_write != NULL) ops.write = bindings_write;
  if (b->ops_release != NULL) ops.release = bindings_release;
  if (b->ops_releasedir != NULL) ops.releasedir = bindings_releasedir;
  if (b->ops_create != NULL) ops.create = bindings_create;
  if (b->ops_utimens != NULL) ops.utimens = bindings_utimens;
  if (b->ops_unlink != NULL) ops.unlink = bindings_unlink;
  if (b->ops_rename != NULL) ops.rename = bindings_rename;
  if (b->ops_link != NULL) ops.link = bindings_link;
  if (b->ops_symlink != NULL) ops.symlink = bindings_symlink;
  if (b->ops_mkdir != NULL) ops.mkdir = bindings_mkdir;
  if (b->ops_rmdir != NULL) ops.rmdir = bindings_rmdir;
  if (b->ops_init != NULL) ops.init = bindings_init;
  if (b->ops_destroy != NULL) ops.destroy = bindings_destroy;

  int argc = !strcmp(b->mntopts, "-o") ? 1 : 2;
  char *argv[] = {
    (char *) "fuse_bindings_dummy",
    (char *) b->mntopts
  };

  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_chan *ch = fuse_mount(b->mnt, &args);

  if (ch == NULL) {
    b->op = OP_ERROR;
    bindings_call(b);
    uv_close((uv_handle_t*) &(b->async), &bindings_on_close);
    return NULL;
  }

  struct fuse *fuse = fuse_new(ch, &args, &ops, sizeof(struct fuse_operations), b);

  if (fuse == NULL) {
    b->op = OP_ERROR;
    bindings_call(b);
    uv_close((uv_handle_t*) &(b->async), &bindings_on_close);
    return NULL;
  }

  fuse_loop(fuse);

  fuse_unmount(b->mnt, ch);
  fuse_session_remove_chan(ch);
  fuse_destroy(fuse);

  uv_close((uv_handle_t*) &(b->async), &bindings_on_close);

  return 0;
}

NAN_INLINE static Local<Date> bindings_get_date (struct timespec *out) {
  int ms = (out->tv_nsec / 1000);
  return NanNew<Date>(out->tv_sec * 1000 + ms);
}

NAN_INLINE static void bindings_set_date (struct timespec *out, Local<Date> date) {
  double ms = date->NumberValue();
  time_t secs = (time_t)(ms / 1000.0);
  time_t rem = ms - (1000.0 * secs);
  time_t ns = rem * 1000000.0;
  out->tv_sec = secs;
  out->tv_nsec = ns;
}

NAN_INLINE static void bindings_set_stat (struct stat *stat, Local<Object> obj) {
  if (obj->Has(NanNew<String>("dev"))) stat->st_dev = obj->Get(NanNew<String>("dev"))->NumberValue();
  if (obj->Has(NanNew<String>("ino"))) stat->st_ino = obj->Get(NanNew<String>("ino"))->NumberValue();
  if (obj->Has(NanNew<String>("mode"))) stat->st_mode = obj->Get(NanNew<String>("mode"))->Uint32Value();
  if (obj->Has(NanNew<String>("nlink"))) stat->st_nlink = obj->Get(NanNew<String>("nlink"))->NumberValue();
  if (obj->Has(NanNew<String>("uid"))) stat->st_uid = obj->Get(NanNew<String>("uid"))->NumberValue();
  if (obj->Has(NanNew<String>("gid"))) stat->st_gid = obj->Get(NanNew<String>("gid"))->NumberValue();
  if (obj->Has(NanNew<String>("rdev"))) stat->st_rdev = obj->Get(NanNew<String>("rdev"))->NumberValue();
  if (obj->Has(NanNew<String>("size"))) stat->st_size = obj->Get(NanNew<String>("size"))->NumberValue();
#ifndef _WIN32
  if (obj->Has(NanNew<String>("blksize"))) stat->st_blksize = obj->Get(NanNew<String>("blksize"))->NumberValue();
  if (obj->Has(NanNew<String>("blocks"))) stat->st_blocks = obj->Get(NanNew<String>("blocks"))->NumberValue();
#ifdef __APPLE__
  if (obj->Has(NanNew<String>("mtime"))) bindings_set_date(&stat->st_mtimespec, obj->Get(NanNew("mtime")).As<Date>());
  if (obj->Has(NanNew<String>("ctime"))) bindings_set_date(&stat->st_ctimespec, obj->Get(NanNew("ctime")).As<Date>());
  if (obj->Has(NanNew<String>("atime"))) bindings_set_date(&stat->st_atimespec, obj->Get(NanNew("atime")).As<Date>());
#else
  if (obj->Has(NanNew<String>("mtime"))) bindings_set_date(&stat->st_mtim, obj->Get(NanNew("mtime")).As<Date>());
  if (obj->Has(NanNew<String>("ctime"))) bindings_set_date(&stat->st_ctim, obj->Get(NanNew("ctime")).As<Date>());
  if (obj->Has(NanNew<String>("atime"))) bindings_set_date(&stat->st_atim, obj->Get(NanNew("atime")).As<Date>());
#endif
#endif
}

NAN_INLINE static void bindings_set_statfs (struct statvfs *statfs, Local<Object> obj) { // from http://linux.die.net/man/2/stat
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

NAN_INLINE static void bindings_set_dirs (bindings_t *b, Local<Array> dirs) {
  for (uint32_t i = 0; i < dirs->Length(); i++) {
    NanUtf8String dir(dirs->Get(i));
    if (b->filler(b->data, *dir, &empty_stat, 0)) break;
  }
}

NAN_METHOD(OpCallback) {
  NanScope();

  bindings_t *b = bindings_mounted[args[0]->Uint32Value()];
  b->result = (args.Length() > 1 && args[1]->IsNumber()) ? args[1]->Uint32Value() : 0;
  bindings_current = NULL;

  if (!b->result) {
    switch (b->op) {
      case OP_STATFS: {
        if (args.Length() > 2 && args[2]->IsObject()) bindings_set_statfs((struct statvfs *) b->data, args[2].As<Object>());
      }
      break;

      case OP_GETATTR:
      case OP_FGETATTR: {
        if (args.Length() > 2 && args[2]->IsObject()) bindings_set_stat((struct stat *) b->data, args[2].As<Object>());
      }
      break;

      case OP_READDIR: {
        if (args.Length() > 2 && args[2]->IsArray()) bindings_set_dirs(b, args[2].As<Array>());
      }
      break;

      case OP_CREATE:
      case OP_OPEN:
      case OP_OPENDIR: {
        if (args.Length() > 2 && args[2]->IsNumber()) {
          b->info->fh = args[2].As<Number>()->Uint32Value();
        }
      }
      break;

      case OP_READLINK: {
        if (args.Length() > 2 && args[2]->IsString()) {
          NanUtf8String path(args[2]);
          strcpy((char *) b->data, *path);
        }
      }
      break;

      case OP_INIT:
      case OP_ERROR:
      case OP_ACCESS:
      case OP_FLUSH:
      case OP_FSYNC:
      case OP_FSYNCDIR:
      case OP_TRUNCATE:
      case OP_FTRUNCATE:
      case OP_CHOWN:
      case OP_CHMOD:
      case OP_MKNOD:
      case OP_SETXATTR:
      case OP_GETXATTR:
      case OP_READ:
      case OP_UTIMENS:
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

  semaphore_signal(&(b->semaphore));
  NanReturnUndefined();
}

NAN_INLINE static void bindings_call_op (bindings_t *b, NanCallback *fn, int argc, Local<Value> *argv) {
  if (fn == NULL) semaphore_signal(&(b->semaphore));
  else fn->Call(argc, argv);
}

static void bindings_dispatch (uv_async_t* handle, int status) {
  NanScope();

  bindings_t *b = bindings_current = (bindings_t *) handle->data;
  Local<Function> callback = b->callback->GetFunction();
  b->result = -1;

  switch (b->op) {
    case OP_INIT: {
      Local<Value> tmp[] = {callback};
      bindings_call_op(b, b->ops_init, 1, tmp);
    }
    return;

    case OP_ERROR: {
      Local<Value> tmp[] = {callback};
      bindings_call_op(b, b->ops_error, 1, tmp);
    }
    return;

    case OP_STATFS: {
      Local<Value> tmp[] = {NanNew<String>(b->path), callback};
      bindings_call_op(b, b->ops_statfs, 2, tmp);
    }
    return;

    case OP_FGETATTR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_fgetattr, 3, tmp);
    }
    return;

    case OP_GETATTR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), callback};
      bindings_call_op(b, b->ops_getattr, 2, tmp);
    }
    return;

    case OP_READDIR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), callback};
      bindings_call_op(b, b->ops_readdir, 2, tmp);
    }
    return;

    case OP_CREATE: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_create, 3, tmp);
    }
    return;

    case OP_TRUNCATE: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->length), callback};
      bindings_call_op(b, b->ops_truncate, 3, tmp);
    }
    return;

    case OP_FTRUNCATE: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), NanNew<Number>(b->length), callback};
      bindings_call_op(b, b->ops_ftruncate, 4, tmp);
    }
    return;

    case OP_ACCESS: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_access, 3, tmp);
    }
    return;

    case OP_OPEN: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_open, 3, tmp);
    }
    return;

    case OP_OPENDIR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_opendir, 3, tmp);
    }
    return;

    case OP_WRITE: {
      Local<Value> tmp[] = {
        NanNew<String>(b->path),
        NanNew<Number>(b->info->fh),
        bindings_buffer((char *) b->data, b->length),
        NanNew<Number>(b->length),
        NanNew<Number>(b->offset),
        callback
      };
      bindings_call_op(b, b->ops_write, 6, tmp);
    }
    return;

    case OP_READ: {
      Local<Value> tmp[] = {
        NanNew<String>(b->path),
        NanNew<Number>(b->info->fh),
        bindings_buffer((char *) b->data, b->length),
        NanNew<Number>(b->length),
        NanNew<Number>(b->offset),
        callback
      };
      bindings_call_op(b, b->ops_read, 6, tmp);
    }
    return;

    case OP_RELEASE: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_release, 3, tmp);
    }
    return;

    case OP_RELEASEDIR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_releasedir, 3, tmp);
    }
    return;

    case OP_UNLINK: {
      Local<Value> tmp[] = {NanNew<String>(b->path), callback};
      bindings_call_op(b, b->ops_unlink, 2, tmp);
    }
    return;

    case OP_RENAME: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<String>((char *) b->data), callback};
      bindings_call_op(b, b->ops_rename, 3, tmp);
    }
    return;

    case OP_LINK: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<String>((char *) b->data), callback};
      bindings_call_op(b, b->ops_link, 3, tmp);
    }
    return;

    case OP_SYMLINK: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<String>((char *) b->data), callback};
      bindings_call_op(b, b->ops_symlink, 3, tmp);
    }
    return;

    case OP_CHMOD: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_chmod, 3, tmp);
    }
    return;

    case OP_MKNOD: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), NanNew<Number>(b->dev), callback};
      bindings_call_op(b, b->ops_mknod, 4, tmp);
    }
    return;

    case OP_CHOWN: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->uid), NanNew<Number>(b->gid), callback};
      bindings_call_op(b, b->ops_chown, 4, tmp);
    }
    return;

    case OP_READLINK: {
      Local<Value> tmp[] = {NanNew<String>(b->path), callback};
      bindings_call_op(b, b->ops_readlink, 2, tmp);
    }
    return;

    case OP_SETXATTR: {
      Local<Value> tmp[] = {
        NanNew<String>(b->path),
        NanNew<String>(b->name),
        bindings_buffer((char *) b->data, b->length),
        NanNew<Number>(b->length),
        NanNew<Number>(b->offset),
        NanNew<Number>(b->mode),
        callback
      };
      bindings_call_op(b, b->ops_setxattr, 7, tmp);
    }
    return;

    case OP_GETXATTR: {
      Local<Value> tmp[] = {
        NanNew<String>(b->path),
        NanNew<String>(b->name),
        bindings_buffer((char *) b->data, b->length),
        NanNew<Number>(b->length),
        NanNew<Number>(b->offset),
        callback
      };
      bindings_call_op(b, b->ops_getxattr, 6, tmp);
    }
    return;

    case OP_MKDIR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_mkdir, 3, tmp);
    }
    return;

    case OP_RMDIR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), callback};
      bindings_call_op(b, b->ops_rmdir, 2, tmp);
    }
    return;

    case OP_DESTROY: {
      Local<Value> tmp[] = {callback};
      bindings_call_op(b, b->ops_destroy, 1, tmp);
    }
    return;

    case OP_UTIMENS: {
      struct timespec *tv = (struct timespec *) b->data;
      Local<Value> tmp[] = {NanNew<String>(b->path), bindings_get_date(tv), bindings_get_date(tv + 1), callback};
      bindings_call_op(b, b->ops_utimens, 4, tmp);
    }
    return;

    case OP_FLUSH: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_flush, 3, tmp);
    }
    return;

    case OP_FSYNC: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_fsync, 4, tmp);
    }
    return;

    case OP_FSYNCDIR: {
      Local<Value> tmp[] = {NanNew<String>(b->path), NanNew<Number>(b->info->fh), NanNew<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_fsyncdir, 4, tmp);
    }
    return;
  }

  semaphore_signal(&(b->semaphore));
}

static int bindings_alloc () {
  int free_index = -1;
  size_t size = sizeof(bindings_t);

  for (int i = 0; i < bindings_mounted_count; i++) {
    if (bindings_mounted[i] == NULL) {
      free_index = i;
      break;
    }
  }

  if (free_index == -1 && bindings_mounted_count < 1024) free_index = bindings_mounted_count++;
  if (free_index != -1) {
    bindings_t *b = bindings_mounted[free_index] = (bindings_t *) malloc(size);
    memset(b, 0, size);
    b->index = free_index;
  }

  return free_index;
}

NAN_METHOD(Mount) {
  NanScope();

  if (!args[0]->IsString()) return NanThrowError("mnt must be a string");

  mutex_lock(&mutex);
  int index = bindings_alloc();
  mutex_unlock(&mutex);

  if (index == -1) return NanThrowError("You cannot mount more than 1024 filesystem in one process");

  mutex_lock(&mutex);
  bindings_t *b = bindings_mounted[index];
  mutex_unlock(&mutex);

  memset(&empty_stat, 0, sizeof(empty_stat));
  memset(b, 0, sizeof(bindings_t));

  NanUtf8String path(args[0]);
  Local<Object> ops = args[1].As<Object>();

  b->ops_init = ops->Has(NanNew<String>("init")) ? new NanCallback(ops->Get(NanNew<String>("init")).As<Function>()) : NULL;
  b->ops_error = ops->Has(NanNew<String>("error")) ? new NanCallback(ops->Get(NanNew<String>("error")).As<Function>()) : NULL;
  b->ops_access = ops->Has(NanNew<String>("access")) ? new NanCallback(ops->Get(NanNew<String>("access")).As<Function>()) : NULL;
  b->ops_statfs = ops->Has(NanNew<String>("statfs")) ? new NanCallback(ops->Get(NanNew<String>("statfs")).As<Function>()) : NULL;
  b->ops_getattr = ops->Has(NanNew<String>("getattr")) ? new NanCallback(ops->Get(NanNew<String>("getattr")).As<Function>()) : NULL;
  b->ops_fgetattr = ops->Has(NanNew<String>("fgetattr")) ? new NanCallback(ops->Get(NanNew<String>("fgetattr")).As<Function>()) : NULL;
  b->ops_flush = ops->Has(NanNew<String>("flush")) ? new NanCallback(ops->Get(NanNew<String>("flush")).As<Function>()) : NULL;
  b->ops_fsync = ops->Has(NanNew<String>("fsync")) ? new NanCallback(ops->Get(NanNew<String>("fsync")).As<Function>()) : NULL;
  b->ops_fsyncdir = ops->Has(NanNew<String>("fsyncdir")) ? new NanCallback(ops->Get(NanNew<String>("fsyncdir")).As<Function>()) : NULL;
  b->ops_readdir = ops->Has(NanNew<String>("readdir")) ? new NanCallback(ops->Get(NanNew<String>("readdir")).As<Function>()) : NULL;
  b->ops_truncate = ops->Has(NanNew<String>("truncate")) ? new NanCallback(ops->Get(NanNew<String>("truncate")).As<Function>()) : NULL;
  b->ops_ftruncate = ops->Has(NanNew<String>("ftruncate")) ? new NanCallback(ops->Get(NanNew<String>("ftruncate")).As<Function>()) : NULL;
  b->ops_readlink = ops->Has(NanNew<String>("readlink")) ? new NanCallback(ops->Get(NanNew<String>("readlink")).As<Function>()) : NULL;
  b->ops_chown = ops->Has(NanNew<String>("chown")) ? new NanCallback(ops->Get(NanNew<String>("chown")).As<Function>()) : NULL;
  b->ops_chmod = ops->Has(NanNew<String>("chmod")) ? new NanCallback(ops->Get(NanNew<String>("chmod")).As<Function>()) : NULL;
  b->ops_mknod = ops->Has(NanNew<String>("mknod")) ? new NanCallback(ops->Get(NanNew<String>("mknod")).As<Function>()) : NULL;
  b->ops_setxattr = ops->Has(NanNew<String>("setxattr")) ? new NanCallback(ops->Get(NanNew<String>("setxattr")).As<Function>()) : NULL;
  b->ops_getxattr = ops->Has(NanNew<String>("getxattr")) ? new NanCallback(ops->Get(NanNew<String>("getxattr")).As<Function>()) : NULL;
  b->ops_open = ops->Has(NanNew<String>("open")) ? new NanCallback(ops->Get(NanNew<String>("open")).As<Function>()) : NULL;
  b->ops_opendir = ops->Has(NanNew<String>("opendir")) ? new NanCallback(ops->Get(NanNew<String>("opendir")).As<Function>()) : NULL;
  b->ops_read = ops->Has(NanNew<String>("read")) ? new NanCallback(ops->Get(NanNew<String>("read")).As<Function>()) : NULL;
  b->ops_write = ops->Has(NanNew<String>("write")) ? new NanCallback(ops->Get(NanNew<String>("write")).As<Function>()) : NULL;
  b->ops_release = ops->Has(NanNew<String>("release")) ? new NanCallback(ops->Get(NanNew<String>("release")).As<Function>()) : NULL;
  b->ops_releasedir = ops->Has(NanNew<String>("releasedir")) ? new NanCallback(ops->Get(NanNew<String>("releasedir")).As<Function>()) : NULL;
  b->ops_create = ops->Has(NanNew<String>("create")) ? new NanCallback(ops->Get(NanNew<String>("create")).As<Function>()) : NULL;
  b->ops_utimens = ops->Has(NanNew<String>("utimens")) ? new NanCallback(ops->Get(NanNew<String>("utimens")).As<Function>()) : NULL;
  b->ops_unlink = ops->Has(NanNew<String>("unlink")) ? new NanCallback(ops->Get(NanNew<String>("unlink")).As<Function>()) : NULL;
  b->ops_rename = ops->Has(NanNew<String>("rename")) ? new NanCallback(ops->Get(NanNew<String>("rename")).As<Function>()) : NULL;
  b->ops_link = ops->Has(NanNew<String>("link")) ? new NanCallback(ops->Get(NanNew<String>("link")).As<Function>()) : NULL;
  b->ops_symlink = ops->Has(NanNew<String>("symlink")) ? new NanCallback(ops->Get(NanNew<String>("symlink")).As<Function>()) : NULL;
  b->ops_mkdir = ops->Has(NanNew<String>("mkdir")) ? new NanCallback(ops->Get(NanNew<String>("mkdir")).As<Function>()) : NULL;
  b->ops_rmdir = ops->Has(NanNew<String>("rmdir")) ? new NanCallback(ops->Get(NanNew<String>("rmdir")).As<Function>()) : NULL;
  b->ops_destroy = ops->Has(NanNew<String>("destroy")) ? new NanCallback(ops->Get(NanNew<String>("destroy")).As<Function>()) : NULL;

  Local<Value> tmp[] = {NanNew<Number>(index), NanNew<FunctionTemplate>(OpCallback)->GetFunction()};
  b->callback = new NanCallback(callback_constructor->Call(2, tmp).As<Function>());

  strcpy(b->mnt, *path);
  strcpy(b->mntopts, "-o");

  Local<Array> options = ops->Get(NanNew<String>("options")).As<Array>();
  if (options->IsArray()) {
    for (uint32_t i = 0; i < options->Length(); i++) {
      NanUtf8String option(options->Get(i));
      if (strcmp(b->mntopts, "-o")) strcat(b->mntopts, ",");
      strcat(b->mntopts, *option);
    }
  }

  semaphore_init(&(b->semaphore));
  uv_async_init(uv_default_loop(), &(b->async), (uv_async_cb) bindings_dispatch);
  b->async.data = b;

  thread_create(&(b->thread), bindings_thread, b);

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

NAN_METHOD(SetCallback) {
  NanScope();
  callback_constructor = new NanCallback(args[0].As<Function>());
  NanReturnUndefined();
}

NAN_METHOD(SetBuffer) {
  NanScope();
  NanAssignPersistent(buffer_constructor, args[0].As<Function>());
  NanReturnUndefined();
}

NAN_METHOD(PopulateContext) {
  NanScope();
  if (bindings_current == NULL) return NanThrowError("You have to call this inside a fuse operation");

  Local<Object> ctx = args[0].As<Object>();
  ctx->Set(NanNew<String>("uid"), NanNew(bindings_current->context_uid));
  ctx->Set(NanNew<String>("gid"), NanNew(bindings_current->context_gid));
  ctx->Set(NanNew<String>("pid"), NanNew(bindings_current->context_pid));

  NanReturnUndefined();
}

NAN_METHOD(Unmount) {
  NanScope();

  if (!args[0]->IsString()) return NanThrowError("mnt must be a string");
  NanUtf8String path(args[0]);
  Local<Function> callback = args[1].As<Function>();

  char *path_alloc = (char *) malloc(1024);
  strcpy(path_alloc, *path);

  NanAsyncQueueWorker(new UnmountWorker(new NanCallback(callback), path_alloc));
  NanReturnUndefined();
}

void Init(Handle<Object> exports) {
  exports->Set(NanNew("setCallback"), NanNew<FunctionTemplate>(SetCallback)->GetFunction());
  exports->Set(NanNew("setBuffer"), NanNew<FunctionTemplate>(SetBuffer)->GetFunction());
  exports->Set(NanNew("mount"), NanNew<FunctionTemplate>(Mount)->GetFunction());
  exports->Set(NanNew("unmount"), NanNew<FunctionTemplate>(Unmount)->GetFunction());
  exports->Set(NanNew("populateContext"), NanNew<FunctionTemplate>(PopulateContext)->GetFunction());
}

NODE_MODULE(fuse_bindings, Init)
