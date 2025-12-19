/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "slicer/dex_ir.h"
#include "slicer/chronometer.h"
#include "slicer/dex_utf8.h"
#include "slicer/dex_format.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <cstdlib>

namespace ir {

// DBJ2a string hash
static uint32_t HashString(const char* cstr) {
  uint32_t hash = 5381;  // DBJ2 magic prime value
  while (*cstr) {
    hash = ((hash << 5) + hash) ^ *cstr++;
  }
  return hash;
}

uint32_t StringsHasher::Hash(const char* string_key) const {
  return HashString(string_key);
}

bool StringsHasher::Compare(const char* string_key, const String* string) const {
  return dex::Utf8Cmp(string_key, string->c_str()) == 0;
}

uint32_t ProtosHasher::Hash(const std::string& proto_key) const {
  return HashString(proto_key.c_str());
}

bool ProtosHasher::Compare(const std::string& proto_key, const Proto* proto) const {
  return proto_key == proto->Signature();
}

MethodKey MethodsHasher::GetKey(const EncodedMethod* method) const {
  MethodKey method_key;
  method_key.class_descriptor = method->decl->parent->descriptor;
  method_key.method_name = method->decl->name;
  method_key.prototype = method->decl->prototype;
  return method_key;
}

uint32_t MethodsHasher::Hash(const MethodKey& method_key) const {
  return static_cast<uint32_t>(std::hash<void*>{}(method_key.class_descriptor) ^
                               std::hash<void*>{}(method_key.method_name) ^
                               std::hash<void*>{}(method_key.prototype));
}

bool MethodsHasher::Compare(const MethodKey& method_key, const EncodedMethod* method) const {
  return method_key.class_descriptor == method->decl->parent->descriptor &&
         method_key.method_name == method->decl->name &&
         method_key.prototype == method->decl->prototype;
}

inline auto Comp(const dex::u4& i, const dex::u4& j) {
  if (i < j) return -1;
  if (j < i) return 1;
  return 0;
}

inline auto operator<=>(const AnnotationElement& a, const AnnotationElement& b) {
  return Comp(a.name->index, b.name->index);
}

inline auto operator<=>(const Annotation& a, const Annotation& b) {
  return Comp(a.type->index, b.type->index);
}

inline auto operator<=>(const FieldAnnotation& a, const FieldAnnotation& b) {
  return Comp(a.field_decl->index, b.field_decl->index);
}

inline auto operator<=>(const MethodAnnotation& a, const MethodAnnotation& b) {
  return Comp(a.method_decl->index, b.method_decl->index);
}

inline auto operator<=>(const ParamAnnotation& a, const ParamAnnotation& b) {
  return Comp(a.method_decl->index, b.method_decl->index);
}

inline auto operator<=>(const ir::Class& a, const ir::Class& b) {
  return Comp(a.index, b.index);
}

inline auto operator<=>(const ir::MethodDecl& a, const ir::MethodDecl& b) {
  return (a.parent->index != b.parent->index)
         ? Comp(a.parent->index, b.parent->index)
         : (a.name->index != b.name->index)
           ? Comp(a.name->index, b.name->index)
           : Comp(a.prototype->index, b.prototype->index);
}

inline auto operator<=>(const ir::String& a, const ir::String& b) {
  return dex::Utf8Cmp(a.c_str(), b.c_str());
}

inline auto operator<=>(const ir::Type& a, const ir::Type& b) {
  return Comp(a.descriptor->index, b.descriptor->index);
}

inline auto operator<=>(const ir::FieldDecl& a, const ir::FieldDecl& b) {
  return (a.parent->index != b.parent->index)
         ? Comp(a.parent->index, b.parent->index)
         : (a.name->index != b.name->index)
           ? Comp(a.name->index, b.name->index)
           : Comp(a.type->index, b.type->index);
}

inline auto operator<=>(const ir::EncodedField& a, const ir::EncodedField& b) {
  return Comp(a.decl->index, b.decl->index);
}

inline auto operator<=>(const ir::EncodedMethod& a, const ir::EncodedMethod& b) {
  return Comp(a.decl->index, b.decl->index);
}

inline auto operator<=>(const ir::Proto& a, const ir::Proto& b) {
  if (a.return_type->index != b.return_type->index) {
    return Comp(a.return_type->index, b.return_type->index);
  }
  std::vector<Type*> empty;
  const auto& aParamTypes = a.param_types ? a.param_types->types : empty;
  const auto& bParamTypes = b.param_types ? b.param_types->types : empty;
  auto less = [](const Type* t1, const Type* t2) { return t1->index < t2->index; };
  if (std::lexicographical_compare(
          aParamTypes.begin(), aParamTypes.end(), bParamTypes.begin(),
          bParamTypes.end(), less)) {
    return -1;
  }
  return 1;
}

// Human-readable type declaration
std::string Type::Decl() const {
  return dex::DescriptorToDecl(descriptor->c_str());
}

Type::Category Type::GetCategory() const {
  switch (*descriptor->c_str()) {
    case 'L':
    case '[':
      return Category::Reference;
    case 'V':
      return Category::Void;
    case 'D':
    case 'J':
      return Category::WideScalar;
    default:
      return Category::Scalar;
  }
}

// Create the corresponding JNI signature:
//  https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/types.html#type_signatures
std::string Proto::Signature() const {
  std::string ss;
  ss += "(";
  if (param_types != nullptr) {
    for (const auto& type : param_types->types) {
      ss += type->descriptor->c_str();
    }
  }
  ss += ")";
  ss += return_type->descriptor->c_str();
  return ss;
}


template<typename RandomIt>
inline void QuickSortPointer(RandomIt first, RandomIt last) {
  using ValueType = typename std::iterator_traits<RandomIt>::value_type;
  if (first == last) return;
  std::qsort(&*first, last - first, sizeof(ValueType),
             +[](const void* a, const void* b) -> int {
                 auto i = static_cast<const ValueType*>(a);
                 auto j = static_cast<const ValueType*>(b);
                 return **i <=> **j;
             }
  );
}

// Helper for IR normalization
// (it sorts items and update the numeric idexes to match)
template <class T>
static void IndexItems(std::vector<T>& items) {
  QuickSortPointer(items.begin(), items.end());
  for (size_t i = 0; i < items.size(); ++i) {
    items[i]->index = i;
  }
}

// Helper for IR normalization (DFS for topological sort)
//
// NOTE: this recursive version is clean and simple and we know
//  that the max depth is bounded (exactly 1 for JVMTI and a small
//  max for general case - the largest .dex file in AOSP has 5000 classes
//  total)
//
void DexFile::TopSortClassIndex(Class* irClass, dex::u4* nextIndex) {
  if (irClass->index == dex::u4(-1)) {
    if (irClass->super_class && irClass->super_class->class_def) {
      TopSortClassIndex(irClass->super_class->class_def, nextIndex);
    }

    if (irClass->interfaces) {
      for (Type* interfaceType : irClass->interfaces->types) {
        if (interfaceType->class_def) {
          TopSortClassIndex(interfaceType->class_def, nextIndex);
        }
      }
    }

    SLICER_CHECK(*nextIndex < classes.size());
    irClass->index = (*nextIndex)++;
  }
}

// Helper for IR normalization
// (topological sort the classes)
void DexFile::SortClassIndexes() {
  for (auto& irClass : classes) {
    irClass->index = dex::u4(-1);
  }

  dex::u4 nextIndex = 0;
  for (auto& irClass : classes) {
    TopSortClassIndex(irClass.get(), &nextIndex);
  }
}

// Helper for NormalizeClass()
static void SortEncodedFields(std::vector<EncodedField*>* fields) {
  QuickSortPointer(fields->begin(), fields->end());
}

// Helper for NormalizeClass()
static void SortEncodedMethods(std::vector<EncodedMethod*>* methods) {
  QuickSortPointer(methods->begin(), methods->end());
}

// Helper for IR normalization
// (sort the field & method arrays)
static void NormalizeClass(Class* irClass) {
  SortEncodedFields(&irClass->static_fields);
  SortEncodedFields(&irClass->instance_fields);
  SortEncodedMethods(&irClass->direct_methods);
  SortEncodedMethods(&irClass->virtual_methods);
}

// Prepare the IR for generating a .dex image
// (the .dex format requires a specific sort order for some of the arrays, etc...)
//
// TODO: not a great solution - move this logic to the writer!
//
// TODO: the comparison predicate can be better expressed by using std::tie()
//  Ex. FieldDecl has a method comp() returning tie(parent->index, name->index, type->index)
//
void DexFile::Normalize() {
  // sort build the .dex indexes
  IndexItems(strings);

  IndexItems(types);

  IndexItems(protos);

  IndexItems(fields);

  IndexItems(methods);

  // reverse topological sort
  //
  // the classes must be ordered such that a given class's superclass and
  // implemented interfaces appear in the list earlier than the referring
  // class
  //
  // CONSIDER: for the BCI-only scenario we can avoid this
  //
  SortClassIndexes();

  IndexItems(classes);

  // normalize class data
  for (const auto& irClass : classes) {
    NormalizeClass(irClass.get());
  }

  // normalize annotations
  for (const auto& irAnnotation : annotations) {
    // elements must be sorted in increasing order by string_id index
    auto& elements = irAnnotation->elements;
    QuickSortPointer(elements.begin(), elements.end());
  }

  // normalize "annotation_set_item"
  for (const auto& irAnnotationSet : annotation_sets) {
    // The elements must be sorted in increasing order, by type_idx
    auto& annotations = irAnnotationSet->annotations;
    QuickSortPointer(annotations.begin(), annotations.end());
  }

  // normalize "annotations_directory_item"
  for (const auto& irAnnotationDirectory : annotations_directories) {
    // field_annotations: The elements of the list must be
    // sorted in increasing order, by field_idx
    auto& field_annotations = irAnnotationDirectory->field_annotations;
    QuickSortPointer(field_annotations.begin(), field_annotations.end());

    // method_annotations: The elements of the list must be
    // sorted in increasing order, by method_idx
    auto& method_annotations = irAnnotationDirectory->method_annotations;
    QuickSortPointer(method_annotations.begin(), method_annotations.end());

    // parameter_annotations: The elements of the list must be
    // sorted in increasing order, by method_idx
    auto& param_annotations = irAnnotationDirectory->param_annotations;
    QuickSortPointer(param_annotations.begin(), param_annotations.end());
  }
}

} // namespace ir
