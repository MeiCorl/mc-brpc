// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: brpc/errno.proto

#include "brpc/errno.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/port.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// This is a temporary google only hack
#ifdef GOOGLE_PROTOBUF_ENFORCE_UNIQUENESS
#include "third_party/protobuf/version.h"
#endif
// @@protoc_insertion_point(includes)

namespace brpc {
}  // namespace brpc
namespace protobuf_brpc_2ferrno_2eproto {
void InitDefaults() {
}

const ::google::protobuf::EnumDescriptor* file_level_enum_descriptors[1];
const ::google::protobuf::uint32 TableStruct::offsets[1] = {};
static const ::google::protobuf::internal::MigrationSchema* schemas = NULL;
static const ::google::protobuf::Message* const* file_default_instances = NULL;

void protobuf_AssignDescriptors() {
  AddDescriptors();
  AssignDescriptors(
      "brpc/errno.proto", schemas, file_default_instances, TableStruct::offsets,
      NULL, file_level_enum_descriptors, NULL);
}

void protobuf_AssignDescriptorsOnce() {
  static ::google::protobuf::internal::once_flag once;
  ::google::protobuf::internal::call_once(once, protobuf_AssignDescriptors);
}

void protobuf_RegisterTypes(const ::std::string&) GOOGLE_PROTOBUF_ATTRIBUTE_COLD;
void protobuf_RegisterTypes(const ::std::string&) {
  protobuf_AssignDescriptorsOnce();
}

void AddDescriptorsImpl() {
  InitDefaults();
  static const char descriptor[] GOOGLE_PROTOBUF_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
      "\n\020brpc/errno.proto\022\004brpc*\256\003\n\005Errno\022\017\n\nEN"
      "OSERVICE\020\351\007\022\016\n\tENOMETHOD\020\352\007\022\r\n\010EREQUEST\020"
      "\353\007\022\r\n\010ERPCAUTH\020\354\007\022\022\n\rETOOMANYFAILS\020\355\007\022\021\n"
      "\014EPCHANFINISH\020\356\007\022\023\n\016EBACKUPREQUEST\020\357\007\022\021\n"
      "\014ERPCTIMEDOUT\020\360\007\022\022\n\rEFAILEDSOCKET\020\361\007\022\n\n\005"
      "EHTTP\020\362\007\022\021\n\014EOVERCROWDED\020\363\007\022\025\n\020ERTMPPUBL"
      "ISHABLE\020\364\007\022\026\n\021ERTMPCREATESTREAM\020\365\007\022\t\n\004EE"
      "OF\020\366\007\022\014\n\007EUNUSED\020\367\007\022\t\n\004ESSL\020\370\007\022\025\n\020EH2RUN"
      "OUTSTREAMS\020\371\007\022\014\n\007EREJECT\020\372\007\022\016\n\tEINTERNAL"
      "\020\321\017\022\016\n\tERESPONSE\020\322\017\022\014\n\007ELOGOFF\020\323\017\022\013\n\006ELI"
      "MIT\020\324\017\022\013\n\006ECLOSE\020\325\017\022\t\n\004EITP\020\326\017\022\n\n\005ERDMA\020"
      "\271\027\022\r\n\010ERDMAMEM\020\272\027B\031\n\010com.brpcB\rBaiduRpcE"
      "rrno"
  };
  ::google::protobuf::DescriptorPool::InternalAddGeneratedFile(
      descriptor, 484);
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedFile(
    "brpc/errno.proto", &protobuf_RegisterTypes);
}

void AddDescriptors() {
  static ::google::protobuf::internal::once_flag once;
  ::google::protobuf::internal::call_once(once, AddDescriptorsImpl);
}
// Force AddDescriptors() to be called at dynamic initialization time.
struct StaticDescriptorInitializer {
  StaticDescriptorInitializer() {
    AddDescriptors();
  }
} static_descriptor_initializer;
}  // namespace protobuf_brpc_2ferrno_2eproto
namespace brpc {
const ::google::protobuf::EnumDescriptor* Errno_descriptor() {
  protobuf_brpc_2ferrno_2eproto::protobuf_AssignDescriptorsOnce();
  return protobuf_brpc_2ferrno_2eproto::file_level_enum_descriptors[0];
}
bool Errno_IsValid(int value) {
  switch (value) {
    case 1001:
    case 1002:
    case 1003:
    case 1004:
    case 1005:
    case 1006:
    case 1007:
    case 1008:
    case 1009:
    case 1010:
    case 1011:
    case 1012:
    case 1013:
    case 1014:
    case 1015:
    case 1016:
    case 1017:
    case 1018:
    case 2001:
    case 2002:
    case 2003:
    case 2004:
    case 2005:
    case 2006:
    case 3001:
    case 3002:
      return true;
    default:
      return false;
  }
}


// @@protoc_insertion_point(namespace_scope)
}  // namespace brpc
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

// @@protoc_insertion_point(global_scope)
