// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

// FXJS_V8 is a layer that makes it easier to define native objects in V8, but
// has no knowledge of PDF-specific native objects. It could in theory be used
// to implement other sets of native objects.

// PDFium code should include this file rather than including V8 headers
// directly.

#ifndef FXJS_INCLUDE_FXJS_V8_H_
#define FXJS_INCLUDE_FXJS_V8_H_

#include <v8-util.h>
#include <v8.h>

#include <map>
#include <vector>

#include "core/fxcrt/include/fx_string.h"

class CFXJS_Engine;
class CFXJS_ObjDefinition;

// FXJS_V8 places no restrictions on this class; it merely passes it
// on to caller-provided methods.
class IJS_Context;  // A description of the event that caused JS execution.

#ifdef PDF_ENABLE_XFA
// FXJS_V8 places no interpreation on this calass; it merely passes it
// along to XFA.
class CFXJSE_RuntimeData;
#endif  // PDF_ENABLE_XFA

enum FXJSOBJTYPE {
  FXJSOBJTYPE_DYNAMIC = 0,  // Created by native method and returned to JS.
  FXJSOBJTYPE_STATIC,       // Created by init and hung off of global object.
  FXJSOBJTYPE_GLOBAL,       // The global object itself (may only appear once).
};

struct FXJSErr {
  const wchar_t* message;
  const wchar_t* srcline;
  unsigned linnum;
};

// Global weak map to save dynamic objects.
class V8TemplateMapTraits : public v8::StdMapTraits<void*, v8::Object> {
 public:
  typedef v8::GlobalValueMap<void*, v8::Object, V8TemplateMapTraits> MapType;
  typedef void WeakCallbackDataType;

  static WeakCallbackDataType* WeakCallbackParameter(
      MapType* map,
      void* key,
      const v8::Local<v8::Object>& value) {
    return key;
  }
  static MapType* MapFromWeakCallbackInfo(
      const v8::WeakCallbackInfo<WeakCallbackDataType>&);

  static void* KeyFromWeakCallbackInfo(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
    return data.GetParameter();
  }
  static const v8::PersistentContainerCallbackType kCallbackType =
      v8::kWeakWithInternalFields;
  static void DisposeWeak(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {}
  static void OnWeakCallback(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {}
  static void Dispose(v8::Isolate* isolate,
                      v8::Global<v8::Object> value,
                      void* key);
  static void DisposeCallbackData(WeakCallbackDataType* callbackData) {}
};

class V8TemplateMap {
 public:
  typedef v8::GlobalValueMap<void*, v8::Object, V8TemplateMapTraits> MapType;

  explicit V8TemplateMap(v8::Isolate* isolate);
  ~V8TemplateMap();

  void set(void* key, v8::Local<v8::Object> handle);

  friend class V8TemplateMapTraits;

 private:
  MapType m_map;
};

class FXJS_PerIsolateData {
 public:
  ~FXJS_PerIsolateData();

  static void SetUp(v8::Isolate* pIsolate);
  static FXJS_PerIsolateData* Get(v8::Isolate* pIsolate);

  void CreateDynamicObjsMap(v8::Isolate* pIsolate) {
    if (!m_pDynamicObjsMap)
      m_pDynamicObjsMap = new V8TemplateMap(pIsolate);
  }
  void ReleaseDynamicObjsMap() {
    delete m_pDynamicObjsMap;
    m_pDynamicObjsMap = nullptr;
  }

  std::vector<CFXJS_ObjDefinition*> m_ObjectDefnArray;
#ifdef PDF_ENABLE_XFA
  CFXJSE_RuntimeData* m_pFXJSERuntimeData;
#endif  // PDF_ENABLE_XFA
  V8TemplateMap* m_pDynamicObjsMap;

 protected:
  FXJS_PerIsolateData();
};

class FXJS_ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  void* Allocate(size_t length) override;
  void* AllocateUninitialized(size_t length) override;
  void Free(void* data, size_t length) override;
};

void FXJS_Initialize(unsigned int embedderDataSlot, v8::Isolate* pIsolate);
void FXJS_Release();

// Gets the global isolate set by FXJS_Initialize(), or makes a new one each
// time if there is no such isolate. Returns true if a new isolate had to be
// created.
bool FXJS_GetIsolate(v8::Isolate** pResultIsolate);

// Get the global isolate's ref count.
size_t FXJS_GlobalIsolateRefCount();

class CFXJS_Engine {
 public:
  CFXJS_Engine();
  ~CFXJS_Engine();

  using Constructor = void (*)(CFXJS_Engine* pEngine,
                               v8::Local<v8::Object> obj);
  using Destructor = void (*)(CFXJS_Engine* pEngine, v8::Local<v8::Object> obj);

  static CFXJS_Engine* CurrentEngineFromIsolate(v8::Isolate* pIsolate);
  static int GetObjDefnID(v8::Local<v8::Object> pObj);

#ifdef PDF_ENABLE_XFA
  // Called as part of FXJS_InitializeEngine, exposed so PDF can make its
  // own contexts compatible with XFA or vice versa.
  static void SetForV8Context(v8::Local<v8::Context> v8Context,
                              CFXJS_Engine* pEngine);
#endif  // PDF_ENABLE_XFA

  // TODO(tsepez): to constructor.
  void SetIsolate(v8::Isolate* pIsolate) { m_isolate = pIsolate; }
  v8::Isolate* GetIsolate() const { return m_isolate; }

  // Always returns a valid, newly-created objDefnID.
  int DefineObj(const wchar_t* sObjName,
                FXJSOBJTYPE eObjType,
                Constructor pConstructor,
                Destructor pDestructor);

  void DefineObjMethod(int nObjDefnID,
                       const wchar_t* sMethodName,
                       v8::FunctionCallback pMethodCall);
  void DefineObjProperty(int nObjDefnID,
                         const wchar_t* sPropName,
                         v8::AccessorGetterCallback pPropGet,
                         v8::AccessorSetterCallback pPropPut);
  void DefineObjAllProperties(int nObjDefnID,
                              v8::NamedPropertyQueryCallback pPropQurey,
                              v8::NamedPropertyGetterCallback pPropGet,
                              v8::NamedPropertySetterCallback pPropPut,
                              v8::NamedPropertyDeleterCallback pPropDel);
  void DefineObjConst(int nObjDefnID,
                      const wchar_t* sConstName,
                      v8::Local<v8::Value> pDefault);
  void DefineGlobalMethod(const wchar_t* sMethodName,
                          v8::FunctionCallback pMethodCall);
  void DefineGlobalConst(const wchar_t* sConstName,
                         v8::FunctionCallback pConstGetter);

  // Called after FXJS_Define* calls made.
  void InitializeEngine();
  void ReleaseEngine();

  // Called after FXJS_InitializeEngine call made.
  int Execute(const CFX_WideString& script, FXJSErr* perror);

  v8::Local<v8::Context> NewLocalContext();
  v8::Local<v8::Context> GetPersistentContext();

  v8::Local<v8::Value> NewNull();
  v8::Local<v8::Array> NewArray();
  v8::Local<v8::Value> NewNumber(int number);
  v8::Local<v8::Value> NewNumber(double number);
  v8::Local<v8::Value> NewNumber(float number);
  v8::Local<v8::Value> NewBoolean(bool b);
  v8::Local<v8::Value> NewString(const wchar_t* str);
  v8::Local<v8::Date> NewDate(double d);
  v8::Local<v8::Object> NewFxDynamicObj(int nObjDefnID, bool bStatic = false);

  v8::Local<v8::Object> GetThisObj();
  int ToInt32(v8::Local<v8::Value> pValue);
  bool ToBoolean(v8::Local<v8::Value> pValue);
  double ToNumber(v8::Local<v8::Value> pValue);
  CFX_WideString ToString(v8::Local<v8::Value> pValue);
  v8::Local<v8::Object> ToObject(v8::Local<v8::Value> pValue);
  v8::Local<v8::Array> ToArray(v8::Local<v8::Value> pValue);

  unsigned GetArrayLength(v8::Local<v8::Array> pArray);
  v8::Local<v8::Value> GetArrayElement(v8::Local<v8::Array> pArray,
                                       unsigned index);
  unsigned PutArrayElement(v8::Local<v8::Array> pArray,
                           unsigned index,
                           v8::Local<v8::Value> pValue);

  std::vector<CFX_WideString> GetObjectPropertyNames(
      v8::Local<v8::Object> pObj);
  v8::Local<v8::Value> GetObjectProperty(v8::Local<v8::Object> pObj,
                                         const CFX_WideString& PropertyName);

  void PutObjectString(v8::Local<v8::Object> pObj,
                       const CFX_WideString& wsPropertyName,
                       const CFX_WideString& wsValue);
  void PutObjectNumber(v8::Local<v8::Object> pObj,
                       const CFX_WideString& PropertyName,
                       int nValue);
  void PutObjectNumber(v8::Local<v8::Object> pObj,
                       const CFX_WideString& PropertyName,
                       float fValue);
  void PutObjectNumber(v8::Local<v8::Object> pObj,
                       const CFX_WideString& PropertyName,
                       double dValue);
  void PutObjectBoolean(v8::Local<v8::Object> pObj,
                        const CFX_WideString& PropertyName,
                        bool bValue);
  void PutObjectObject(v8::Local<v8::Object> pObj,
                       const CFX_WideString& PropertyName,
                       v8::Local<v8::Object> pPut);
  void PutObjectNull(v8::Local<v8::Object> pObj,
                     const CFX_WideString& PropertyName);

  // Native object binding.
  void SetObjectPrivate(v8::Local<v8::Object> pObj, void* p);
  void* GetObjectPrivate(v8::Local<v8::Object> pObj);
  static void FreeObjectPrivate(void* p);
  static void FreeObjectPrivate(v8::Local<v8::Object> pObj);

  void SetConstArray(const CFX_WideString& name, v8::Local<v8::Array> array);
  v8::Local<v8::Array> GetConstArray(const CFX_WideString& name);

  v8::Local<v8::String> WSToJSString(const CFX_WideString& wsPropertyName);
  void Error(const CFX_WideString& message);

 private:
  v8::Isolate* m_isolate;
  v8::Global<v8::Context> m_V8PersistentContext;
  std::vector<v8::Global<v8::Object>*> m_StaticObjects;
  std::map<CFX_WideString, v8::Global<v8::Array>> m_ConstArrays;
};

#endif  // FXJS_INCLUDE_FXJS_V8_H_
