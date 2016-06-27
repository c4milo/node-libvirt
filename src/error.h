// Copyright 2010, Camilo Aguilar. Cloudescape, LLC.
#ifndef ERROR_H
#define ERROR_H

#include "node_libvirt.h"

namespace NLV {

class Error : public Nan::ObjectWrap
{
public:
  static void Initialize(Handle<Object> exports);
  static Local<Value> New(virErrorPtr error, const char *context);

};

#define _LIBVIRT_STRINGIFY_DETAIL(x) #x
#define _LIBVIRT_STRINGIFY(x) _LIBVIRT_STRINGIFY_DETAIL(x)
#define NLV_ERROR(e) Error::New(e, __FILE__ ":" _LIBVIRT_STRINGIFY(__LINE__))

} //namespace NLV

#endif // ERROR_H
