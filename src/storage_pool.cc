// Copyright 2010, Camilo Aguilar. Cloudescape, LLC.
#include <stdlib.h>

#include "node_libvirt.h"
#include "hypervisor.h"
#include "error.h"

#include "storage_volume.h"
#include "storage_pool.h"
//FIXME default params, default flags

namespace NodeLibvirt {

Persistent<FunctionTemplate> StoragePool::constructor_template;
void StoragePool::Initialize(Handle<Object> exports)
{
  NanScope();
  Local<FunctionTemplate> t = FunctionTemplate::New();
  t->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(t, "build",               Build);
  NODE_SET_PROTOTYPE_METHOD(t, "start",               Start);
  NODE_SET_PROTOTYPE_METHOD(t, "stop",                Stop);
  NODE_SET_PROTOTYPE_METHOD(t, "erase",               Erase);
  NODE_SET_PROTOTYPE_METHOD(t, "setAutostart",        SetAutostart);
  NODE_SET_PROTOTYPE_METHOD(t, "getAutostart",        GetAutostart);
  NODE_SET_PROTOTYPE_METHOD(t, "getInfo",             GetInfo);
  NODE_SET_PROTOTYPE_METHOD(t, "getName",             GetName);
  NODE_SET_PROTOTYPE_METHOD(t, "getUUID",             GetUUID);
  NODE_SET_PROTOTYPE_METHOD(t, "undefine",            Undefine);
  NODE_SET_PROTOTYPE_METHOD(t, "toXml",               ToXml);
  NODE_SET_PROTOTYPE_METHOD(t, "isActive",            IsActive);
  NODE_SET_PROTOTYPE_METHOD(t, "isPersistent",        IsPersistent);
  NODE_SET_PROTOTYPE_METHOD(t, "getVolumes",          GetVolumes);
  NODE_SET_PROTOTYPE_METHOD(t, "refresh",             Refresh);
  NODE_SET_PROTOTYPE_METHOD(t, "createVolume",        StorageVolume::Create);
  NODE_SET_PROTOTYPE_METHOD(t, "cloneVolume",         StorageVolume::Clone);
  NODE_SET_PROTOTYPE_METHOD(t, "lookupStorageVolumeByName",  StorageVolume::LookupByName);

  NanAssignPersistent(constructor_template, t);
  constructor_template->SetClassName(NanNew("StoragePool"));
  exports->Set(NanNew("StoragePool"), t->GetFunction());

  Local<ObjectTemplate> object_tmpl = t->InstanceTemplate();

  //Constants initialization
  //virStoragePoolDeleteFlags
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_DELETE_NORMAL);
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_DELETE_ZEROED);

  //virStoragePoolState
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_INACTIVE);
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_BUILDING);
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_RUNNING);
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_DEGRADED);
  NODE_DEFINE_CONSTANT(object_tmpl, VIR_STORAGE_POOL_INACCESSIBLE);
}

Local<Object> StoragePool::NewInstance(const LibVirtHandle &handle)
{
  NanScope();
  StoragePool *storagePool = new StoragePool(handle.ToStoragePool());
  Local<Object> object = constructor_template->GetFunction()->NewInstance();
  storagePool->Wrap(object);
  return NanEscapeScope(object);
}

StoragePool::~StoragePool()
{
  if (handle_ != NULL)
    virStoragePoolFree(handle_);
  handle_ = 0;
}

NLV_LOOKUP_BY_VALUE_EXECUTE_IMPL(StoragePool, LookupByName, virStoragePoolLookupByName)
NAN_METHOD(StoragePool::LookupByName)
{
  NanScope();
  if (args.Length() < 2 ||
      (!args[0]->IsString() && !args[1]->IsFunction())) {
    NanThrowTypeError("You must specify a valid name and callback.");
    NanReturnUndefined();
  }

  Local<Object> object = args.This();
  if (!NanHasInstance(Hypervisor::constructor_template, object)) {
    NanThrowTypeError("You must specify a Hypervisor instance");
    NanReturnUndefined();
  }

  Hypervisor *hv = ObjectWrap::Unwrap<Hypervisor>(object);
  std::string name(*NanUtf8String(args[0]->ToString()));
  NanCallback *callback = new NanCallback(args[1].As<Function>());
  NanAsyncQueueWorker(new LookupByNameWorker(callback, hv->handle_, name));
  NanReturnUndefined();
}

NLV_LOOKUP_BY_VALUE_EXECUTE_IMPL(StoragePool, LookupByUUID, virStoragePoolLookupByUUIDString)
NAN_METHOD(StoragePool::LookupByUUID)
{
  NanScope();
  if (args.Length() < 2 ||
      (!args[0]->IsString() && !args[1]->IsFunction())) {
    NanThrowTypeError("You must specify a valid uuid and callback.");
    NanReturnUndefined();
  }

  if (!NanHasInstance(Hypervisor::constructor_template, args.This())) {
    NanThrowTypeError("You must specify a Hypervisor instance");
    NanReturnUndefined();
  }

  Hypervisor *hv = ObjectWrap::Unwrap<Hypervisor>(args.This());
  std::string uuid(*NanUtf8String(args[0]->ToString()));
  NanCallback *callback = new NanCallback(args[1].As<Function>());
  NanAsyncQueueWorker(new LookupByUUIDWorker(callback, hv->handle_, uuid));
  NanReturnUndefined();
}

NAN_METHOD(StoragePool::LookupByVolume)
{
  NanScope();
  NanReturnUndefined();
}

NAN_METHOD(StoragePool::Create)
{
  NanScope();
  if (args.Length() < 2 ||
      (!args[0]->IsString() && !args[1]->IsFunction())) {
    NanThrowTypeError("You must specify a string and callback");
    NanReturnUndefined();
  }

  if (!NanHasInstance(Hypervisor::constructor_template, args.This())) {
    NanThrowTypeError("You must specify a Hypervisor instance");
    NanReturnUndefined();
  }

  Hypervisor *hv = ObjectWrap::Unwrap<Hypervisor>(args.This());
  std::string xmlData(*NanUtf8String(args[0]->ToString()));
  NanCallback *callback = new NanCallback(args[1].As<Function>());
  NanAsyncQueueWorker(new CreateWorker(callback, hv->handle_, xmlData));
  NanReturnUndefined();
}

NLV_WORKER_EXECUTE(StoragePool, Create)
{
  unsigned int flags = 0;
  lookupHandle_ =
    virStoragePoolCreateXML(Handle().ToConnection(), value_.c_str(), flags);
  if (lookupHandle_.ToStoragePool() == NULL) {
    SetVirError(virGetLastError());
    return;
  }
}

NLV_WORKER_METHOD_DEFINE(StoragePool)
NLV_WORKER_EXECUTE(StoragePool, Define)
{
  unsigned int flags = 0;
  lookupHandle_ =
    virStoragePoolDefineXML(Handle().ToConnection(), value_.c_str(), flags);
  if (lookupHandle_.ToStoragePool() == NULL) {
    SetVirError(virGetLastError());
    return;
  }
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, Build)
NLV_WORKER_EXECUTE(StoragePool, Build)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  unsigned int flags = 0;
  int result = virStoragePoolBuild(Handle().ToStoragePool(), flags);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = true;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, Undefine)
NLV_WORKER_EXECUTE(StoragePool, Undefine)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  int result = virStoragePoolUndefine(Handle().ToStoragePool());
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = true;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, Start)
NLV_WORKER_EXECUTE(StoragePool, Start)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  unsigned int flags = 0;
  int result = virStoragePoolCreate(Handle().ToStoragePool(), flags);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = true;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, Stop)
NLV_WORKER_EXECUTE(StoragePool, Stop)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  int result = virStoragePoolDestroy(Handle().ToStoragePool());
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  if (Handle().ToStoragePool() != NULL) {
    Handle().Clear();
  }

  data_ = true;
}

NAN_METHOD(StoragePool::Erase)
{
  NanScope();
  if (args.Length() < 2 ||
      (!args[0]->IsArray() && !args[1]->IsFunction())) {
    NanThrowTypeError("You must specify an array of flags and a callback");
    NanReturnUndefined();
  }

  unsigned int flags = 0;
  Local<Array> flagsArray = Local<Array>::Cast(args[0]);
  unsigned int length = flagsArray->Length();
  for (unsigned int i = 0; i < length; ++i) {
    flags |= flagsArray->Get(NanNew(i))->Int32Value();
  }

  NanCallback *callback = new NanCallback(args[1].As<Function>());
  StoragePool *storagePool = ObjectWrap::Unwrap<StoragePool>(args.This());
  NanAsyncQueueWorker(new EraseWorker(callback, storagePool->handle_, flags));
  NanReturnUndefined();
}

NLV_WORKER_EXECUTE(StoragePool, Erase)
{
  if (flags_ == 0) {
    flags_ |= VIR_STORAGE_POOL_DELETE_NORMAL;
  }

  int result = virStoragePoolDelete(Handle().ToStoragePool(), flags_);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  if (Handle().ToStoragePool() != NULL) {
    Handle().Clear();
  }

  data_ = true;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, GetAutostart)
NLV_WORKER_EXECUTE(StoragePool, GetAutostart)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  int autostart;
  int result = virStoragePoolGetAutostart(Handle().ToStoragePool(), &autostart);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = static_cast<bool>(autostart);
}

NAN_METHOD(StoragePool::SetAutostart)
{
  NanScope();
  if (args.Length() < 2 ||
      (!args[0]->IsString() && !args[1]->IsFunction())) {
    NanThrowTypeError("You must specify a bool and callback");
    NanReturnUndefined();
  }

  bool autoStart = args[0]->IsTrue();
  NanCallback *callback = new NanCallback(args[1].As<Function>());
  StoragePool *storagePool = ObjectWrap::Unwrap<StoragePool>(args.This());
  NanAsyncQueueWorker(new SetAutostartWorker(callback, storagePool->handle_, autoStart));
  NanReturnUndefined();
}

NLV_WORKER_EXECUTE(StoragePool, SetAutostart)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  int result = virStoragePoolSetAutostart(Handle().ToStoragePool(), autoStart_ ? 1 : 0);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = true;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, GetInfo)
NLV_WORKER_EXECUTE(StoragePool, GetInfo)
{
  int result = virStoragePoolGetInfo(Handle().ToStoragePool(), &info_);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }
}

NLV_WORKER_OKCALLBACK(StoragePool, GetInfo)
{
  NanScope();
  Local<Object> result = NanNew<Object>();
  result->Set(NanNew("state"), NanNew<Integer>(info_.state));
  result->Set(NanNew("capacity"), NanNew<Number>(info_.capacity));
  result->Set(NanNew("allocation"), NanNew<Number>(info_.allocation));
  result->Set(NanNew("available"), NanNew<Number>(info_.available));

  v8::Local<v8::Value> argv[] = { NanNull(), result };
  callback->Call(2, argv);
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, GetName)
NLV_WORKER_EXECUTE(StoragePool, GetName)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  const char *result = virStoragePoolGetName(Handle().ToStoragePool());
  if (result == NULL) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = result;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, GetUUID)
NLV_WORKER_EXECUTE(StoragePool, GetUUID)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  char *uuid = new char[VIR_UUID_STRING_BUFLEN];
  int result = virStoragePoolGetUUIDString(Handle().ToStoragePool(), uuid);
  if (result == -1) {
    SetVirError(virGetLastError());
    delete[] uuid;
    return;
  }

  data_ = uuid;
  delete[] uuid;
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, ToXml)
NLV_WORKER_EXECUTE(StoragePool, ToXml)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  unsigned int flags = 0;
  char *result = virStoragePoolGetXMLDesc(Handle().ToStoragePool(), flags);
  if (result == NULL) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = result;
  free(result);
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, IsActive)
NLV_WORKER_EXECUTE(StoragePool, IsActive)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  int result = virStoragePoolIsActive(Handle().ToStoragePool());
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = static_cast<bool>(result);
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, IsPersistent)
NLV_WORKER_EXECUTE(StoragePool, IsPersistent)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  int result = virStoragePoolIsPersistent(Handle().ToStoragePool());
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = static_cast<bool>(result);
}

NAN_METHOD(StoragePool::GetVolumes)
{
  NanScope();
  if (args.Length() != 1) {
    NanThrowTypeError("You must specify a callback");
    NanReturnUndefined();
  }

  NanCallback *callback = new NanCallback(args[0].As<Function>());
  StoragePool *storagePool = ObjectWrap::Unwrap<StoragePool>(args.This());
  NanAsyncQueueWorker(new GetVolumesWorker(callback, storagePool->handle_));
  NanReturnUndefined();
}

NLV_WORKER_EXECUTE(StoragePool, GetVolumes)
{
  int num_volumes = virStoragePoolNumOfVolumes(Handle().ToStoragePool());
  if (num_volumes == -1) {
    SetVirError(virGetLastError());
    return;
  }

  char **volumes = (char**) malloc(sizeof(*volumes) * num_volumes);
  if (volumes == NULL) {
    SetErrorMessage("unable to allocate memory");
    return;
  }

  num_volumes = virStoragePoolListVolumes(Handle().ToStoragePool(), volumes, num_volumes);
  if (num_volumes == -1) {
    free(volumes);
    SetVirError(virGetLastError());
    return;
  }

  for (int i = 0; i < num_volumes; ++i) {
    // TODO new StorageVolume and return array of StorageVolume instances
    data_.push_back(volumes[i]);
    free(volumes[i]);
  }

  free(volumes);
}

NLV_WORKER_METHOD_NO_ARGS(StoragePool, Refresh)
NLV_WORKER_EXECUTE(StoragePool, Refresh)
{
  NLV_WORKER_ASSERT_STORAGEPOOL();

  unsigned int flags = 0;
  int result = virStoragePoolRefresh(Handle().ToStoragePool(), flags);
  if (result == -1) {
    SetVirError(virGetLastError());
    return;
  }

  data_ = true;
}

} //namespace NodeLibvirt
