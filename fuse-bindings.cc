#include <nan.h>

#define FUSE_USE_VERSION 29

#if defined(_WIN32) && _MSC_VER < 1900
// Visual Studio 2015 adds struct timespec,
// this #define will make Dokany define its
// own struct timespec on earlier versions
#define _CRT_NO_TIME_T
#endif

#include <fuse.h>
#include <fuse_opt.h>

#ifndef _MSC_VER
// Need to use FUSE_STAT when using Dokany with Visual Studio.
// To keep things simple, when not using Visual Studio,
// define FUSE_STAT to be "stat" so we can use FUSE_STAT in the code anyway.
#define FUSE_STAT stat
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

#include "abstractions.h"

using namespace v8;

#define LOCAL_STRING(s) Nan::New<String>(s).ToLocalChecked()
#define LOOKUP_CALLBACK(map, name) map->Has(LOCAL_STRING(name)) ? new Nan::Callback(map->Get(LOCAL_STRING(name)).As<Function>()) : NULL

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
  OP_LISTXATTR,
  OP_REMOVEXATTR,
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

static Nan::Persistent<Function> buffer_constructor;
static Nan::Callback *callback_constructor;
static struct FUSE_STAT empty_stat;

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
  abstr_thread_t thread;
  bindings_sem_t semaphore;
  bindings_sem_t semaphore_readdir;
  uv_async_t async;

  // methods
  Nan::Callback *ops_init;
  Nan::Callback *ops_error;
  Nan::Callback *ops_access;
  Nan::Callback *ops_statfs;
  Nan::Callback *ops_getattr;
  Nan::Callback *ops_fgetattr;
  Nan::Callback *ops_flush;
  Nan::Callback *ops_fsync;
  Nan::Callback *ops_fsyncdir;
  Nan::Callback *ops_readdir;
  Nan::Callback *ops_truncate;
  Nan::Callback *ops_ftruncate;
  Nan::Callback *ops_readlink;
  Nan::Callback *ops_chown;
  Nan::Callback *ops_chmod;
  Nan::Callback *ops_mknod;
  Nan::Callback *ops_setxattr;
  Nan::Callback *ops_getxattr;
  Nan::Callback *ops_listxattr;
  Nan::Callback *ops_removexattr;
  Nan::Callback *ops_open;
  Nan::Callback *ops_opendir;
  Nan::Callback *ops_read;
  Nan::Callback *ops_write;
  Nan::Callback *ops_release;
  Nan::Callback *ops_releasedir;
  Nan::Callback *ops_create;
  Nan::Callback *ops_utimens;
  Nan::Callback *ops_unlink;
  Nan::Callback *ops_rename;
  Nan::Callback *ops_link;
  Nan::Callback *ops_symlink;
  Nan::Callback *ops_mkdir;
  Nan::Callback *ops_rmdir;
  Nan::Callback *ops_destroy;

  Nan::Callback *callback;

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

static int bindings_fusermount (char *path) {
  return fusermount(path);
}

static int bindings_unmount (char *path) {
  mutex_lock(&mutex);
  bindings_t *b = bindings_find_mounted(path);
  int result = bindings_fusermount(path);
  if (b != NULL && result == 0) b->gc = 1;
  mutex_unlock(&mutex);

  if (b != NULL && result == 0) thread_join(b->thread);

  return result;
}

#if (NODE_MODULE_VERSION > NODE_0_10_MODULE_VERSION && NODE_MODULE_VERSION < IOJS_3_0_MODULE_VERSION)
NAN_INLINE v8::Local<v8::Object> bindings_buffer (char *data, size_t length) {
  Local<Object> buf = Nan::New(buffer_constructor)->NewInstance(0, NULL);
  Local<String> k = LOCAL_STRING("length");
  Local<Number> v = Nan::New<Number>(length);
  buf->Set(k, v);
  buf->SetIndexedPropertiesToExternalArrayData((char *) data, kExternalUnsignedByteArray, length);
  return buf;
}
#else
void noop (char *data, void *hint) {}
NAN_INLINE v8::Local<v8::Object> bindings_buffer (char *data, size_t length) {
  return Nan::NewBuffer(data, length, noop, NULL).ToLocalChecked();
}
#endif

NAN_INLINE static int bindings_call_ex (bindings_t *b, bool isreaddir) {
  uv_async_send(&(b->async));
  if (isreaddir) {
    semaphore_wait(&(b->semaphore_readdir));  
  } else {
    semaphore_wait(&(b->semaphore));  
  }

  return b->result;
}

NAN_INLINE static int bindings_call (bindings_t *b) {
  return bindings_call_ex(b, false);
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

static int bindings_getattr (const char *path, struct FUSE_STAT *stat) {
  bindings_t *b = bindings_get_context();

  b->op = OP_GETATTR;
  b->path = (char *) path;
  b->data = stat;

  return bindings_call(b);
}

static int bindings_fgetattr (const char *path, struct FUSE_STAT *stat, struct fuse_file_info *info) {
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

  return bindings_call_ex(b, true);
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

static int bindings_listxattr (const char *path, char *list, size_t size) {
  bindings_t *b = bindings_get_context();

  b->op = OP_LISTXATTR;
  b->path = (char *) path;
  b->data = (void *) list;
  b->length = size;

  return bindings_call(b);
}

static int bindings_removexattr (const char *path, const char *name) {
  bindings_t *b = bindings_get_context();

  b->op = OP_REMOVEXATTR;
  b->path = (char *) path;
  b->name = (char *) name;

  return bindings_call(b);
}

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
  if (b->ops_listxattr != NULL) delete b->ops_listxattr;
  if (b->ops_removexattr != NULL) delete b->ops_removexattr;
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
  if (b->ops_listxattr != NULL) ops.listxattr = bindings_listxattr;
  if (b->ops_removexattr != NULL) ops.removexattr = bindings_removexattr;
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
  return Nan::New<Date>(out->tv_sec * 1000 + ms).ToLocalChecked();
}

NAN_INLINE static void bindings_set_date (struct timespec *out, Local<Date> date) {
  double ms = date->NumberValue();
  time_t secs = (time_t)(ms / 1000.0);
  time_t rem = ms - (1000.0 * secs);
  time_t ns = rem * 1000000.0;
  out->tv_sec = secs;
  out->tv_nsec = ns;
}

NAN_INLINE static void bindings_set_stat (struct FUSE_STAT *stat, Local<Object> obj) {
  if (obj->Has(LOCAL_STRING("dev"))) stat->st_dev = obj->Get(LOCAL_STRING("dev"))->NumberValue();
  if (obj->Has(LOCAL_STRING("ino"))) stat->st_ino = obj->Get(LOCAL_STRING("ino"))->NumberValue();
  if (obj->Has(LOCAL_STRING("mode"))) stat->st_mode = obj->Get(LOCAL_STRING("mode"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("nlink"))) stat->st_nlink = obj->Get(LOCAL_STRING("nlink"))->NumberValue();
  if (obj->Has(LOCAL_STRING("uid"))) stat->st_uid = obj->Get(LOCAL_STRING("uid"))->NumberValue();
  if (obj->Has(LOCAL_STRING("gid"))) stat->st_gid = obj->Get(LOCAL_STRING("gid"))->NumberValue();
  if (obj->Has(LOCAL_STRING("rdev"))) stat->st_rdev = obj->Get(LOCAL_STRING("rdev"))->NumberValue();
  if (obj->Has(LOCAL_STRING("size"))) stat->st_size = obj->Get(LOCAL_STRING("size"))->NumberValue();
  if (obj->Has(LOCAL_STRING("blocks"))) stat->st_blocks = obj->Get(LOCAL_STRING("blocks"))->NumberValue();
  if (obj->Has(LOCAL_STRING("blksize"))) stat->st_blksize = obj->Get(LOCAL_STRING("blksize"))->NumberValue();
#ifdef __APPLE__
  if (obj->Has(LOCAL_STRING("mtime"))) bindings_set_date(&stat->st_mtimespec, obj->Get(LOCAL_STRING("mtime")).As<Date>());
  if (obj->Has(LOCAL_STRING("ctime"))) bindings_set_date(&stat->st_ctimespec, obj->Get(LOCAL_STRING("ctime")).As<Date>());
  if (obj->Has(LOCAL_STRING("atime"))) bindings_set_date(&stat->st_atimespec, obj->Get(LOCAL_STRING("atime")).As<Date>());
#else
  if (obj->Has(LOCAL_STRING("mtime"))) bindings_set_date(&stat->st_mtim, obj->Get(LOCAL_STRING("mtime")).As<Date>());
  if (obj->Has(LOCAL_STRING("ctime"))) bindings_set_date(&stat->st_ctim, obj->Get(LOCAL_STRING("ctime")).As<Date>());
  if (obj->Has(LOCAL_STRING("atime"))) bindings_set_date(&stat->st_atim, obj->Get(LOCAL_STRING("atime")).As<Date>());
#endif
}

NAN_INLINE static void bindings_set_statfs (struct statvfs *statfs, Local<Object> obj) { // from http://linux.die.net/man/2/stat
  if (obj->Has(LOCAL_STRING("bsize"))) statfs->f_bsize = obj->Get(LOCAL_STRING("bsize"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("frsize"))) statfs->f_frsize = obj->Get(LOCAL_STRING("frsize"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("blocks"))) statfs->f_blocks = obj->Get(LOCAL_STRING("blocks"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("bfree"))) statfs->f_bfree = obj->Get(LOCAL_STRING("bfree"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("bavail"))) statfs->f_bavail = obj->Get(LOCAL_STRING("bavail"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("files"))) statfs->f_files = obj->Get(LOCAL_STRING("files"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("ffree"))) statfs->f_ffree = obj->Get(LOCAL_STRING("ffree"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("favail"))) statfs->f_favail = obj->Get(LOCAL_STRING("favail"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("fsid"))) statfs->f_fsid = obj->Get(LOCAL_STRING("fsid"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("flag"))) statfs->f_flag = obj->Get(LOCAL_STRING("flag"))->Uint32Value();
  if (obj->Has(LOCAL_STRING("namemax"))) statfs->f_namemax = obj->Get(LOCAL_STRING("namemax"))->Uint32Value();
}

class SetDirWorker : public Nan::AsyncWorker {
 public:
  SetDirWorker(bindings_t *b, char **dirs, int dirs_length)
    : Nan::AsyncWorker(NULL), b(b), dirs(dirs), dirs_length(dirs_length) {}
  ~SetDirWorker() {}

  void Execute () {
    fuse_fill_dir_t fillerToCall = b->filler;
    void *data = b->data;
    for (int i = 0; i < dirs_length; i++) {
      fillerToCall(data, dirs[i], &empty_stat, 0);
    }
  }
  void WorkComplete(){
    semaphore_signal(&(b->semaphore_readdir));
    for (int i = 0; i < dirs_length; i++) {
      free(dirs[i]);
    }
    free(dirs);
  }
 private:
  bindings_t *b;
  char **dirs;
  int dirs_length;
};


NAN_METHOD(OpCallback) {
  bindings_t *b = bindings_mounted[info[0]->Uint32Value()];
  b->result = (info.Length() > 1 && info[1]->IsNumber()) ? info[1]->Uint32Value() : 0;
  bindings_current = NULL;
  
  if (!b->result) {
    switch (b->op) {
      case OP_STATFS: {
        if (info.Length() > 2 && info[2]->IsObject()) bindings_set_statfs((struct statvfs *) b->data, info[2].As<Object>());
      }
      break;

      case OP_GETATTR:
      case OP_FGETATTR: {
        if (info.Length() > 2 && info[2]->IsObject()) bindings_set_stat((struct FUSE_STAT *) b->data, info[2].As<Object>());
      }
      break;

      case OP_READDIR: {
        if (info.Length() > 2 && info[2]->IsArray()) {
          Local<Array> dirs = info[2].As<Array>();
          
          char **dirs_alloc = (char**)malloc(sizeof(char*)*dirs->Length());
          
          for (uint32_t i = 0; i < dirs->Length(); i++) {
            
            Nan::Utf8String dir(dirs->Get(i));
            
            dirs_alloc[i] = (char *) malloc(1024);
            strcpy(dirs_alloc[i], *dir);
          }
          
          Nan::AsyncQueueWorker(new SetDirWorker(b, dirs_alloc, dirs->Length()));
          return;
        }
      }
      break;

      case OP_CREATE:
      case OP_OPEN:
      case OP_OPENDIR: {
        if (info.Length() > 2 && info[2]->IsNumber()) {
          b->info->fh = info[2].As<Number>()->Uint32Value();
        }
      }
      break;

      case OP_READLINK: {
        if (info.Length() > 2 && info[2]->IsString()) {
          Nan::Utf8String path(info[2]);
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
      case OP_LISTXATTR:
      case OP_REMOVEXATTR:
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
}

NAN_INLINE static void bindings_call_op_ex (bindings_t *b, Nan::Callback *fn, int argc, Local<Value> *argv, bool isreaddir) {
  if (fn == NULL){
    if (isreaddir) {
      semaphore_signal(&(b->semaphore_readdir));
    } else {
      semaphore_signal(&(b->semaphore));}  
    } 
  else {
    fn->Call(argc, argv);
  }
}

NAN_INLINE static void bindings_call_op (bindings_t *b, Nan::Callback *fn, int argc, Local<Value> *argv) {
  bindings_call_op_ex(b, fn, argc, argv, false);
}

static void bindings_dispatch (uv_async_t* handle, int status) {
  Nan::HandleScope scope;
  
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
      Local<Value> tmp[] = {LOCAL_STRING(b->path), callback};
      bindings_call_op(b, b->ops_statfs, 2, tmp);
    }
    return;

    case OP_FGETATTR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_fgetattr, 3, tmp);
    }
    return;

    case OP_GETATTR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), callback};
      bindings_call_op(b, b->ops_getattr, 2, tmp);
    }
    return;

    case OP_READDIR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), callback};
      bindings_call_op_ex(b, b->ops_readdir, 2, tmp, true);
    }
    return;

    case OP_CREATE: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_create, 3, tmp);
    }
    return;

    case OP_TRUNCATE: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->length), callback};
      bindings_call_op(b, b->ops_truncate, 3, tmp);
    }
    return;

    case OP_FTRUNCATE: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), Nan::New<Number>(b->length), callback};
      bindings_call_op(b, b->ops_ftruncate, 4, tmp);
    }
    return;

    case OP_ACCESS: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_access, 3, tmp);
    }
    return;

    case OP_OPEN: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_open, 3, tmp);
    }
    return;

    case OP_OPENDIR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_opendir, 3, tmp);
    }
    return;

    case OP_WRITE: {
      Local<Value> tmp[] = {
        LOCAL_STRING(b->path),
        Nan::New<Number>(b->info->fh),
        bindings_buffer((char *) b->data, b->length),
        Nan::New<Number>(b->length), // TODO: remove me
        Nan::New<Number>(b->offset),
        callback
      };
      bindings_call_op(b, b->ops_write, 6, tmp);
    }
    return;

    case OP_READ: {
      Local<Value> tmp[] = {
        LOCAL_STRING(b->path),
        Nan::New<Number>(b->info->fh),
        bindings_buffer((char *) b->data, b->length),
        Nan::New<Number>(b->length), // TODO: remove me
        Nan::New<Number>(b->offset),
        callback
      };
      bindings_call_op(b, b->ops_read, 6, tmp);
    }
    return;

    case OP_RELEASE: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_release, 3, tmp);
    }
    return;

    case OP_RELEASEDIR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_releasedir, 3, tmp);
    }
    return;

    case OP_UNLINK: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), callback};
      bindings_call_op(b, b->ops_unlink, 2, tmp);
    }
    return;

    case OP_RENAME: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), LOCAL_STRING((char *) b->data), callback};
      bindings_call_op(b, b->ops_rename, 3, tmp);
    }
    return;

    case OP_LINK: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), LOCAL_STRING((char *) b->data), callback};
      bindings_call_op(b, b->ops_link, 3, tmp);
    }
    return;

    case OP_SYMLINK: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), LOCAL_STRING((char *) b->data), callback};
      bindings_call_op(b, b->ops_symlink, 3, tmp);
    }
    return;

    case OP_CHMOD: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_chmod, 3, tmp);
    }
    return;

    case OP_MKNOD: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), Nan::New<Number>(b->dev), callback};
      bindings_call_op(b, b->ops_mknod, 4, tmp);
    }
    return;

    case OP_CHOWN: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->uid), Nan::New<Number>(b->gid), callback};
      bindings_call_op(b, b->ops_chown, 4, tmp);
    }
    return;

    case OP_READLINK: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), callback};
      bindings_call_op(b, b->ops_readlink, 2, tmp);
    }
    return;

    case OP_SETXATTR: {
      Local<Value> tmp[] = {
        LOCAL_STRING(b->path),
        LOCAL_STRING(b->name),
        bindings_buffer((char *) b->data, b->length),
        Nan::New<Number>(b->length),
        Nan::New<Number>(b->offset),
        Nan::New<Number>(b->mode),
        callback
      };
      bindings_call_op(b, b->ops_setxattr, 7, tmp);
    }
    return;

    case OP_GETXATTR: {
      Local<Value> tmp[] = {
        LOCAL_STRING(b->path),
        LOCAL_STRING(b->name),
        bindings_buffer((char *) b->data, b->length),
        Nan::New<Number>(b->length),
        Nan::New<Number>(b->offset),
        callback
      };
      bindings_call_op(b, b->ops_getxattr, 6, tmp);
    }
    return;

    case OP_LISTXATTR: {
      Local<Value> tmp[] = {
        LOCAL_STRING(b->path),
        bindings_buffer((char *) b->data, b->length),
        Nan::New<Number>(b->length),
        callback
      };
      bindings_call_op(b, b->ops_listxattr, 4, tmp);
    }
    return;

    case OP_REMOVEXATTR: {
      Local<Value> tmp[] = {
        LOCAL_STRING(b->path),
        LOCAL_STRING(b->name),
        callback
      };
      bindings_call_op(b, b->ops_removexattr, 3, tmp);
    }
    return;

    case OP_MKDIR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_mkdir, 3, tmp);
    }
    return;

    case OP_RMDIR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), callback};
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
      Local<Value> tmp[] = {LOCAL_STRING(b->path), bindings_get_date(tv), bindings_get_date(tv + 1), callback};
      bindings_call_op(b, b->ops_utimens, 4, tmp);
    }
    return;

    case OP_FLUSH: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), callback};
      bindings_call_op(b, b->ops_flush, 3, tmp);
    }
    return;

    case OP_FSYNC: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), Nan::New<Number>(b->mode), callback};
      bindings_call_op(b, b->ops_fsync, 4, tmp);
    }
    return;

    case OP_FSYNCDIR: {
      Local<Value> tmp[] = {LOCAL_STRING(b->path), Nan::New<Number>(b->info->fh), Nan::New<Number>(b->mode), callback};
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
  if (!info[0]->IsString()) return Nan::ThrowError("mnt must be a string");

  mutex_lock(&mutex);
  int index = bindings_alloc();
  mutex_unlock(&mutex);

  if (index == -1) return Nan::ThrowError("You cannot mount more than 1024 filesystem in one process");

  mutex_lock(&mutex);
  bindings_t *b = bindings_mounted[index];
  mutex_unlock(&mutex);

  memset(&empty_stat, 0, sizeof(empty_stat));

  Nan::Utf8String path(info[0]);
  Local<Object> ops = info[1].As<Object>();

  b->ops_init = LOOKUP_CALLBACK(ops, "init");
  b->ops_error = LOOKUP_CALLBACK(ops, "error");
  b->ops_access = LOOKUP_CALLBACK(ops, "access");
  b->ops_statfs = LOOKUP_CALLBACK(ops, "statfs");
  b->ops_getattr = LOOKUP_CALLBACK(ops, "getattr");
  b->ops_fgetattr = LOOKUP_CALLBACK(ops, "fgetattr");
  b->ops_flush = LOOKUP_CALLBACK(ops, "flush");
  b->ops_fsync = LOOKUP_CALLBACK(ops, "fsync");
  b->ops_fsyncdir = LOOKUP_CALLBACK(ops, "fsyncdir");
  b->ops_readdir = LOOKUP_CALLBACK(ops, "readdir");
  b->ops_truncate = LOOKUP_CALLBACK(ops, "truncate");
  b->ops_ftruncate = LOOKUP_CALLBACK(ops, "ftruncate");
  b->ops_readlink = LOOKUP_CALLBACK(ops, "readlink");
  b->ops_chown = LOOKUP_CALLBACK(ops, "chown");
  b->ops_chmod = LOOKUP_CALLBACK(ops, "chmod");
  b->ops_mknod = LOOKUP_CALLBACK(ops, "mknod");
  b->ops_setxattr = LOOKUP_CALLBACK(ops, "setxattr");
  b->ops_getxattr = LOOKUP_CALLBACK(ops, "getxattr");
  b->ops_listxattr = LOOKUP_CALLBACK(ops, "listxattr");
  b->ops_removexattr = LOOKUP_CALLBACK(ops, "removexattr");
  b->ops_open = LOOKUP_CALLBACK(ops, "open");
  b->ops_opendir = LOOKUP_CALLBACK(ops, "opendir");
  b->ops_read = LOOKUP_CALLBACK(ops, "read");
  b->ops_write = LOOKUP_CALLBACK(ops, "write");
  b->ops_release = LOOKUP_CALLBACK(ops, "release");
  b->ops_releasedir = LOOKUP_CALLBACK(ops, "releasedir");
  b->ops_create = LOOKUP_CALLBACK(ops, "create");
  b->ops_utimens = LOOKUP_CALLBACK(ops, "utimens");
  b->ops_unlink = LOOKUP_CALLBACK(ops, "unlink");
  b->ops_rename = LOOKUP_CALLBACK(ops, "rename");
  b->ops_link = LOOKUP_CALLBACK(ops, "link");
  b->ops_symlink = LOOKUP_CALLBACK(ops, "symlink");
  b->ops_mkdir = LOOKUP_CALLBACK(ops, "mkdir");
  b->ops_rmdir = LOOKUP_CALLBACK(ops, "rmdir");
  b->ops_destroy = LOOKUP_CALLBACK(ops, "destroy");

  Local<Value> tmp[] = {Nan::New<Number>(index), Nan::New<FunctionTemplate>(OpCallback)->GetFunction()};
  b->callback = new Nan::Callback(callback_constructor->Call(2, tmp).As<Function>());

  strcpy(b->mnt, *path);
  strcpy(b->mntopts, "-o");

  Local<Array> options = ops->Get(LOCAL_STRING("options")).As<Array>();
  if (options->IsArray()) {
    for (uint32_t i = 0; i < options->Length(); i++) {
      Nan::Utf8String option(options->Get(i));
      if (strcmp(b->mntopts, "-o")) strcat(b->mntopts, ",");
      strcat(b->mntopts, *option);
    }
  }

  semaphore_init(&(b->semaphore));
  semaphore_init(&(b->semaphore_readdir));
  uv_async_init(uv_default_loop(), &(b->async), (uv_async_cb) bindings_dispatch);
  b->async.data = b;

  thread_create(&(b->thread), bindings_thread, b);
}

class UnmountWorker : public Nan::AsyncWorker {
 public:
  UnmountWorker(Nan::Callback *callback, char *path)
    : Nan::AsyncWorker(callback), path(path), result(0) {}
  ~UnmountWorker() {}

  void Execute () {
    result = bindings_unmount(path);
    free(path);

    if (result != 0) {
      SetErrorMessage("Error");
    }
  }

  void HandleOKCallback () {
    Nan::HandleScope scope;
    callback->Call(0, NULL);
  }

 private:
  char *path;
  int result;
};

NAN_METHOD(SetCallback) {
  callback_constructor = new Nan::Callback(info[0].As<Function>());
}

NAN_METHOD(SetBuffer) {
  buffer_constructor.Reset(info[0].As<Function>());
}

NAN_METHOD(PopulateContext) {
  if (bindings_current == NULL) return Nan::ThrowError("You have to call this inside a fuse operation");

  Local<Object> ctx = info[0].As<Object>();
  ctx->Set(LOCAL_STRING("uid"), Nan::New(bindings_current->context_uid));
  ctx->Set(LOCAL_STRING("gid"), Nan::New(bindings_current->context_gid));
  ctx->Set(LOCAL_STRING("pid"), Nan::New(bindings_current->context_pid));
}

NAN_METHOD(Unmount) {
  if (!info[0]->IsString()) return Nan::ThrowError("mnt must be a string");
  Nan::Utf8String path(info[0]);
  Local<Function> callback = info[1].As<Function>();

  char *path_alloc = (char *) malloc(1024);
  strcpy(path_alloc, *path);

  Nan::AsyncQueueWorker(new UnmountWorker(new Nan::Callback(callback), path_alloc));
}

void Init(Handle<Object> exports) {
  exports->Set(LOCAL_STRING("setCallback"), Nan::New<FunctionTemplate>(SetCallback)->GetFunction());
  exports->Set(LOCAL_STRING("setBuffer"), Nan::New<FunctionTemplate>(SetBuffer)->GetFunction());
  exports->Set(LOCAL_STRING("mount"), Nan::New<FunctionTemplate>(Mount)->GetFunction());
  exports->Set(LOCAL_STRING("unmount"), Nan::New<FunctionTemplate>(Unmount)->GetFunction());
  exports->Set(LOCAL_STRING("populateContext"), Nan::New<FunctionTemplate>(PopulateContext)->GetFunction());
}

NODE_MODULE(fuse_bindings, Init)
