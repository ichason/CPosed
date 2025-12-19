module;

#include "dex_builder.h"
#include "slicer/dex_format.h"
#include "slicer/reader.h"
#include "slicer/writer.h"

export module dex_builder;

export namespace startop::dex {
using startop::dex::ClassBuilder;
using startop::dex::DexBuilder;
using startop::dex::FieldBuilder;
using startop::dex::Instruction;
using startop::dex::LiveRegister;
using startop::dex::MethodBuilder;
using startop::dex::Prototype;
using startop::dex::TypeDescriptor;
using startop::dex::Value;
}  // namespace startop::dex

export namespace slicer {
using slicer::MemView;
}  // namespace slicer

export namespace ir {
using ir::Annotation;
using ir::AnnotationElement;
using ir::AnnotationsDirectory;
using ir::AnnotationSet;
using ir::AnnotationSetRefList;
using ir::Class;
using ir::Code;
using ir::DebugInfo;
using ir::DexFile;
using ir::EncodedArray;
using ir::EncodedField;
using ir::EncodedMethod;
using ir::EncodedValue;
using ir::FieldAnnotation;
using ir::FieldDecl;
using ir::IndexedNode;
using ir::MethodAnnotation;
using ir::MethodDecl;
using ir::Node;
using ir::ParamAnnotation;
using ir::Proto;
using ir::String;
using ir::Type;
using ir::TypeList;
}  // namespace ir

export namespace dex {
using dex::AnnotationItem;
using dex::AnnotationsDirectoryItem;
using dex::AnnotationSetItem;
using dex::AnnotationSetRefItem;
using dex::AnnotationSetRefList;
using dex::ClassDef;
using dex::Code;
using dex::CodeItem;
using dex::CompactCode;
using dex::FieldAnnotationsItem;
using dex::FieldId;
using dex::MapItem;
using dex::MapList;
using dex::MethodAnnotationsItem;
using dex::MethodId;
using dex::ParameterAnnotationsItem;
using dex::ProtoId;
using dex::Reader;
using dex::StringId;
using dex::TryBlock;
using dex::TypeId;
using dex::TypeItem;
using dex::TypeList;
using dex::Writer;

using dex::ReadSLeb128;
using dex::ReadULeb128;

using dex::DBG_ADVANCE_LINE;
using dex::DBG_ADVANCE_PC;
using dex::DBG_END_LOCAL;
using dex::DBG_END_SEQUENCE;
using dex::DBG_FIRST_SPECIAL;
using dex::DBG_LINE_BASE;
using dex::DBG_LINE_RANGE;
using dex::DBG_RESTART_LOCAL;
using dex::DBG_SET_EPILOGUE_BEGIN;
using dex::DBG_SET_FILE;
using dex::DBG_SET_PROLOGUE_END;
using dex::DBG_START_LOCAL;
using dex::DBG_START_LOCAL_EXTENDED;
using dex::kAccAbstract;
using dex::kAccAnnotation;
using dex::kAccBridge;
using dex::kAccConstructor;
using dex::kAccDeclaredSynchronized;
using dex::kAccEnum;
using dex::kAccFinal;
using dex::kAccInterface;
using dex::kAccNative;
using dex::kAccPrivate;
using dex::kAccProtected;
using dex::kAccPublic;
using dex::kAccStatic;
using dex::kAccStrict;
using dex::kAccSuper;
using dex::kAccSynchronized;
using dex::kAccSynthetic;
using dex::kAccTransient;
using dex::kAccVarargs;
using dex::kAccVolatile;
using dex::kAnnotationItem;
using dex::kAnnotationsDirectoryItem;
using dex::kAnnotationSetItem;
using dex::kAnnotationSetRefList;
using dex::kClassDataItem;
using dex::kClassDefItem;
using dex::kCodeItem;
using dex::kDebugInfoItem;
using dex::kEncodedAnnotation;
using dex::kEncodedArray;
using dex::kEncodedArrayItem;
using dex::kEncodedBoolean;
using dex::kEncodedByte;
using dex::kEncodedChar;
using dex::kEncodedDouble;
using dex::kEncodedEnum;
using dex::kEncodedField;
using dex::kEncodedFloat;
using dex::kEncodedInt;
using dex::kEncodedLong;
using dex::kEncodedMethod;
using dex::kEncodedMethodHandle;
using dex::kEncodedMethodType;
using dex::kEncodedNull;
using dex::kEncodedShort;
using dex::kEncodedString;
using dex::kEncodedType;
using dex::kEncodedValueArgShift;
using dex::kEncodedValueTypeMask;
using dex::kEndianConstant;
using dex::kFieldIdItem;
using dex::kHeaderItem;
using dex::kMapList;
using dex::kMethodIdItem;
using dex::kNoIndex;
using dex::kProtoIdItem;
using dex::kSHA1DigestLen;
using dex::kStringDataItem;
using dex::kStringIdItem;
using dex::kTypeIdItem;
using dex::kTypeList;
using dex::kVisibilityBuild;
using dex::kVisibilityEncoded;
using dex::kVisibilityRuntime;
using dex::kVisibilitySystem;
using dex::s1;
using dex::s2;
using dex::s4;
using dex::s8;
using dex::u1;
using dex::u2;
using dex::u4;
using dex::u8;

using dex::opcode_len;
}  // namespace dex
