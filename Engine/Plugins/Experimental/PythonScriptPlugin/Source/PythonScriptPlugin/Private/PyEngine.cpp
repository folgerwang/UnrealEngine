// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PyEngine.h"
#include "PyGenUtil.h"
#include "PyWrapperTypeRegistry.h"

#include "EngineUtils.h"

#if WITH_PYTHON

template <typename IteratorType, typename SelfType>
PyTypeObject InitializePyActorIteratorType(const char* InTypeName, const char* InTypeDoc)
{
	struct FFuncs
	{
		static PyObject* New(PyTypeObject* InType, PyObject* InArgs, PyObject* InKwds)
		{
			return (PyObject*)SelfType::New(InType);
		}

		static void Dealloc(SelfType* InSelf)
		{
			SelfType::Free(InSelf);
		}

		static int Init(SelfType* InSelf, PyObject* InArgs, PyObject* InKwds)
		{
			PyObject* PyWorldObj = nullptr;
			PyObject* PyTypeObj = nullptr;

			static const char *ArgsKwdList[] = { "world", "type", nullptr };
			if (!PyArg_ParseTupleAndKeywords(InArgs, InKwds, "O|O:call", (char**)ArgsKwdList, &PyWorldObj, &PyTypeObj))
			{
				return -1;
			}

			UWorld* IterWorld = nullptr;
			if (!PyConversion::Nativize(PyWorldObj, IterWorld))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'world' (%s) to 'World'"), *PyUtil::GetFriendlyTypename(PyWorldObj)));
				return -1;
			}
			if (!IterWorld)
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("'world' cannot be 'None'"), *PyUtil::GetFriendlyTypename(PyWorldObj)));
				return -1;
			}

			UClass* IterClass = AActor::StaticClass();
			if (PyTypeObj && !PyConversion::NativizeClass(PyTypeObj, IterClass, AActor::StaticClass()))
			{
				PyUtil::SetPythonError(PyExc_TypeError, InSelf, *FString::Printf(TEXT("Failed to convert 'type' (%s) to 'Class'"), *PyUtil::GetFriendlyTypename(PyTypeObj)));
				return -1;
			}

			return SelfType::Init(InSelf, IterWorld, IterClass);
		}

		static SelfType* GetIter(SelfType* InSelf)
		{
			return SelfType::GetIter(InSelf);
		}

		static PyObject* IterNext(SelfType* InSelf)
		{
			return SelfType::IterNext(InSelf);
		}
	};

	PyTypeObject PyType = {
		PyVarObject_HEAD_INIT(nullptr, 0)
		InTypeName, /* tp_name */
		sizeof(SelfType), /* tp_basicsize */
	};

	PyType.tp_new = (newfunc)&FFuncs::New;
	PyType.tp_dealloc = (destructor)&FFuncs::Dealloc;
	PyType.tp_init = (initproc)&FFuncs::Init;
	PyType.tp_iter = (getiterfunc)&FFuncs::GetIter;
	PyType.tp_iternext = (iternextfunc)&FFuncs::IterNext;

	PyType.tp_flags = Py_TPFLAGS_DEFAULT;
	PyType.tp_doc = InTypeDoc;

	return PyType;
}


PyTypeObject PyActorIteratorType = InitializePyActorIteratorType<FActorIterator, FPyActorIterator>("ActorIterator", "Type for iterating Unreal actor instances");
PyTypeObject PySelectedActorIteratorType = InitializePyActorIteratorType<FSelectedActorIterator, FPySelectedActorIterator>("SelectedActorIterator", "Type for iterating selected Unreal actor instances");


namespace PyEngine
{

PyMethodDef PyEngineMethods[] = {
	{ nullptr, nullptr, 0, nullptr }
};

void InitializeModule()
{
	PyGenUtil::FNativePythonModule NativePythonModule;
	NativePythonModule.PyModuleMethods = PyEngineMethods;

#if PY_MAJOR_VERSION >= 3
	NativePythonModule.PyModule = PyImport_AddModule("_unreal_engine");
	PyModule_AddFunctions(NativePythonModule.PyModule, PyEngineMethods);
#else	// PY_MAJOR_VERSION >= 3
	NativePythonModule.PyModule = Py_InitModule("_unreal_engine", PyEngineMethods);
#endif	// PY_MAJOR_VERSION >= 3

	if (PyType_Ready(&PyActorIteratorType) == 0)
	{
		NativePythonModule.AddType(&PyActorIteratorType);
	}

	if (PyType_Ready(&PySelectedActorIteratorType) == 0)
	{
		NativePythonModule.AddType(&PySelectedActorIteratorType);
	}

	FPyWrapperTypeRegistry::Get().RegisterNativePythonModule(MoveTemp(NativePythonModule));
}

}

#endif	// WITH_PYTHON
