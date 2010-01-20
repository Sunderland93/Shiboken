/*
 * This file is part of the Shiboken Python Bindings Generator project.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: PySide team <contact@pyside.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation. Please
 * review the following information to ensure the GNU Lesser General
 * Public License version 2.1 requirements will be met:
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * As a special exception to the GNU Lesser General Public License
 * version 2.1, the object code form of a "work that uses the Library"
 * may incorporate material from a header file that is part of the
 * Library.  You may distribute such object code under terms of your
 * choice, provided that the incorporated material (i) does not exceed
 * more than 5% of the total size of the Library; and (ii) is limited to
 * numerical parameters, data structure layouts, accessors, macros,
 * inline functions and templates.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include <Python.h>
#include <limits>

#include "pyenum.h"
#include "basewrapper.h"
#include "bindingmanager.h"

// When the user adds a function with an argument unknown for the typesystem, the generator writes type checks as
// TYPENAME_Check, so this macro allows users to add PyObject arguments to their added functions.
#define PyObject_Check(X) true

namespace Shiboken
{
/**
*   This function template is used to get the PyTypeObject of a C++ type T.
*   All implementations should be provided by template specializations generated by the generator when
*   T isn't a C++ primitive type.
*   \see SpecialCastFunction
*/
template<typename T>
PyTypeObject* SbkType();

template<> inline PyTypeObject* SbkType<int>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<unsigned int>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<short>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<unsigned short>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<long>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<unsigned long>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<PY_LONG_LONG>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<unsigned PY_LONG_LONG>() { return &PyLong_Type; }
template<> inline PyTypeObject* SbkType<bool>() { return &PyBool_Type; }
template<> inline PyTypeObject* SbkType<float>() { return &PyFloat_Type; }
template<> inline PyTypeObject* SbkType<double>() { return &PyFloat_Type; }
template<> inline PyTypeObject* SbkType<char>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<signed char>() { return &PyInt_Type; }
template<> inline PyTypeObject* SbkType<unsigned char>() { return &PyInt_Type; }

/**
 *   This struct template is used to copy a C++ object using the proper
 *   constructor, which could be the same type as used on the wrapped library
 *   or a C++ wrapper type provided by the binding.
 *   The "isCppWrapper" constant must be set to 'true' when CppObjectCopier
 *   is reimplemented by the Shiboken generator.
 */
template <typename T>
struct CppObjectCopier
{
    static const bool isCppWrapper = false;
    static inline T* copy(const T& cppobj) { return new T(cppobj); }
};

/**
 * Convenience template to create wrappers using the proper Python type for a given C++ class instance.
 */
template<typename T>
inline PyObject* SbkCreateWrapper(const T* cppobj, bool hasOwnership = false, bool isExactType = false)
{
    return SbkBaseWrapper_New(reinterpret_cast<SbkBaseWrapperType*>(SbkType<T>()),
                              cppobj, hasOwnership, isExactType);
}

// Base Conversions ----------------------------------------------------------
template <typename T> struct Converter;

template <typename T>
struct ConverterBase
{
    static inline bool isConvertible(PyObject* pyobj) { return pyobj == Py_None; }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<T*>(cppobj)); }
    static inline PyObject* toPython(const T& cppobj)
    {
        PyObject* obj = SbkCreateWrapper<T>(CppObjectCopier<T>::copy(cppobj), true, true);
        SbkBaseWrapper_setContainsCppWrapper(obj, CppObjectCopier<T>::isCppWrapper);
        return obj;
    }
    // Classes with implicit conversions are expected to reimplement
    // this to build T from its various implicit constructors.
    static inline T toCpp(PyObject* pyobj) { return *Converter<T*>::toCpp(pyobj); }
};

// Specialization meant to be used by abstract classes and object-types
// (i.e. classes with private copy constructors and = operators).
// Example: "struct Converter<AbstractClass* > : ConverterBase<AbstractClass* >"
template <typename T>
struct ConverterBase<T*> : ConverterBase<T>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(reinterpret_cast<T*>(cppobj)); }
    static PyObject* toPython(const T* cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        PyObject* pyobj = BindingManager::instance().retrieveWrapper(cppobj);
        if (pyobj)
            Py_INCREF(pyobj);
        else
            pyobj = SbkCreateWrapper<T>(cppobj);
        return pyobj;
    }
    static T* toCpp(PyObject* pyobj)
    {
        if (pyobj == Py_None)
            return 0;
        SbkBaseWrapperType* shiboType = reinterpret_cast<SbkBaseWrapperType*>(pyobj->ob_type);
        if (shiboType->mi_specialcast)
            return (T*) shiboType->mi_specialcast(pyobj, reinterpret_cast<SbkBaseWrapperType*>(SbkType<T>()));
        return (T*) SbkBaseWrapper_cptr(pyobj);
    }
};

// Pointer Conversions
template <typename T> struct Converter : ConverterBase<T> {};

template <typename T>
struct Converter<T*> : Converter<T>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(reinterpret_cast<T*>(cppobj)); }
    static PyObject* toPython(const T* cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        PyObject* pyobj = BindingManager::instance().retrieveWrapper(cppobj);
        if (pyobj)
            Py_INCREF(pyobj);
        else
            pyobj = SbkCreateWrapper<T>(cppobj);
        return pyobj;
    }
    static T* toCpp(PyObject* pyobj)
    {
        if (Shiboken_TypeCheck(pyobj, T))
            return (T*) SbkBaseWrapper_cptr(pyobj);
        else if (Converter<T>::isConvertible(pyobj))
            return CppObjectCopier<T>::copy(Converter<T>::toCpp(pyobj));
        return 0;
    }
};
template <typename T> struct Converter<const T*> : Converter<T*> {};

// PyObject* specialization to avoid converting what doesn't need to be converted.
template<>
struct Converter<PyObject*> : ConverterBase<PyObject*>
{
    static inline PyObject* toCpp(PyObject* pyobj) { return pyobj; }
};
template <> struct Converter<const PyObject*> : Converter<PyObject*> {};

// Reference Conversions
template <typename T>
struct Converter<T&> : Converter<T*>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<T*>(cppobj)); }
    static inline PyObject* toPython(const T& cppobj) { return Converter<T*>::toPython(&cppobj); }
    static inline T& toCpp(PyObject* pyobj) { return *Converter<T*>::toCpp(pyobj); }
};
template <typename T> struct Converter<const T&> : Converter<T&> {};

// Primitive Conversions ------------------------------------------------------
template <>
struct Converter<bool>
{
    static inline bool isConvertible(PyObject* pyobj) { return PyInt_Check(pyobj); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<bool*>(cppobj)); }
    static inline PyObject* toPython(bool cppobj) { return PyBool_FromLong(cppobj); }
    static inline bool toCpp(PyObject* pyobj) { return pyobj == Py_True; }
};

/**
 * Helper template for checking if a value of SourceT overflows when cast to TargetT
 */
template<typename SourceT, typename TargetT>
inline bool overflowCheck(SourceT value)
{
    return value < std::numeric_limits<TargetT>::min() || value > std::numeric_limits<TargetT>::max();
}

template <typename PyIntEquiv>
struct Converter_PyInt
{
    static inline bool isConvertible(PyObject* pyobj) { return PyNumber_Check(pyobj); }
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<PyIntEquiv*>(cppobj)); }
    static inline PyObject* toPython(const PyIntEquiv& cppobj) { return PyInt_FromLong((long) cppobj); }
    static PyIntEquiv toCpp(PyObject* pyobj)
    {
        double d_result;
        long result;
        if (PyFloat_Check(pyobj)) {
            d_result = PyFloat_AS_DOUBLE(pyobj);
            // If cast to long directly it could overflow silently
            if (overflowCheck<double, PyIntEquiv>(d_result))
                PyErr_SetObject(PyExc_OverflowError, 0);
            return (PyIntEquiv) d_result;
        } else {
            result = PyLong_AsLong(pyobj);
        }

        if (overflowCheck<long, PyIntEquiv>(result))
            PyErr_SetObject(PyExc_OverflowError, 0);
        return (PyIntEquiv) result;
    }
};

template <> struct Converter<char> : Converter_PyInt<char> {};
template <> struct Converter<signed char> : Converter_PyInt<signed char> {};
template <> struct Converter<unsigned char> : Converter_PyInt<unsigned char> {};
template <> struct Converter<int> : Converter_PyInt<int> {};
template <> struct Converter<unsigned int> : Converter_PyInt<unsigned int> {};
template <> struct Converter<short> : Converter_PyInt<short> {};
template <> struct Converter<unsigned short> : Converter_PyInt<unsigned short> {};
template <> struct Converter<long> : Converter_PyInt<long> {};

template <>
struct Converter<unsigned long>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<unsigned long*>(cppobj)); }
    static inline PyObject* toPython(unsigned long cppobj) { return PyLong_FromUnsignedLong(cppobj); }
    static inline unsigned long toCpp(PyObject* pyobj)
    {
        unsigned long result;
        if (PyFloat_Check(pyobj)) {
            // Need to check for negatives manually
            double double_result = PyFloat_AS_DOUBLE(pyobj);
            if (overflowCheck<double, unsigned long>(double_result))
                PyErr_SetObject(PyExc_OverflowError, 0);
            result = (unsigned long) double_result;
        } else {
            result = PyLong_AsUnsignedLong(pyobj);
        }
        return result;
    }
};

template <>
struct Converter<PY_LONG_LONG>
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<PY_LONG_LONG*>(cppobj)); }
    static inline PyObject* toPython(PY_LONG_LONG cppobj) { return PyLong_FromLongLong(cppobj); }
    static inline PY_LONG_LONG toCpp(PyObject* pyobj) { return (PY_LONG_LONG) PyLong_AsLongLong(pyobj); }
};

template <>
struct Converter<unsigned PY_LONG_LONG>
{
    static inline PyObject* toPython(void* cppobj)
    {
        return toPython(*reinterpret_cast<unsigned PY_LONG_LONG*>(cppobj));
    }
    static inline PyObject* toPython(unsigned PY_LONG_LONG cppobj)
    {
        return PyLong_FromUnsignedLongLong(cppobj);
    }
    static inline unsigned PY_LONG_LONG toCpp(PyObject* pyobj)
    {
        return (unsigned PY_LONG_LONG) PyLong_AsUnsignedLongLong(pyobj);
    }
};

template <typename PyFloatEquiv>
struct Converter_PyFloat
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<PyFloatEquiv*>(cppobj)); }
    static inline PyObject* toPython(PyFloatEquiv cppobj) { return PyFloat_FromDouble((double) cppobj); }
    static inline PyFloatEquiv toCpp(PyObject* pyobj)
    {
        if (PyInt_Check(pyobj) || PyLong_Check(pyobj))
            return (PyFloatEquiv) PyLong_AsLong(pyobj);
        return (PyFloatEquiv) PyFloat_AsDouble(pyobj);
    }
};

template <> struct Converter<float> : Converter_PyFloat<float> {};
template <> struct Converter<double> : Converter_PyFloat<double> {};

// PyEnum Conversions ---------------------------------------------------------
template <typename CppEnum>
struct Converter_CppEnum
{
    static inline PyObject* toPython(void* cppobj) { return toPython(*reinterpret_cast<CppEnum*>(cppobj)); }
    static inline PyObject* toPython(CppEnum cppenum)
    {
        return SbkEnumObject_New(SbkType<CppEnum>(), (long) cppenum);
    }
    static inline CppEnum toCpp(PyObject* pyobj)
    {
        return (CppEnum) reinterpret_cast<SbkEnumObject*>(pyobj)->ob_ival;
    }
};

// C Sting Types --------------------------------------------------------------
template <typename CString>
struct Converter_CString
{
    static inline PyObject* toPython(void* cppobj) { return toPython(reinterpret_cast<CString>(cppobj)); }
    static inline PyObject* toPython(CString cppobj)
    {
        if (!cppobj)
            Py_RETURN_NONE;
        return PyString_FromString(cppobj);
    }
    static inline CString toCpp(PyObject* pyobj) { return PyString_AsString(pyobj); }
};

template <> struct Converter<char*> : Converter_CString<char*> {};
template <> struct Converter<const char*> : Converter_CString<const char*> {};

// C++ containers -------------------------------------------------------------
// The following container converters are meant to be used for pairs, lists and maps
// that are similar to the STL containers of the same name.

// For example to create a converter for a std::list the following code is enough:
// template<typename T> struct Converter<std::list<T> > : Converter_std_list<std::list<T> > {};

// And this for a std::map:
// template<typename KT, typename VT>
// struct Converter<std::map<KT, VT> > : Converter_std_map<std::map<KT, VT> > {};

template <typename StdList>
struct Converter_std_list
{
    static inline bool isConvertible(const PyObject* pyObj)
    {
        return PySequence_Check(const_cast<PyObject*>(pyObj));
    }
    static PyObject* toPython(StdList cppobj)
    {
        PyObject* result = PyList_New((int) cppobj.size());
        typedef typename StdList::iterator IT;
        IT it;
        int idx = 0;
        for (it = cppobj.begin(); it != cppobj.end(); it++) {
            typename StdList::value_type vh(*it);
            PyList_SET_ITEM(result, idx, Converter<typename StdList::value_type>::toPython(vh));
            idx++;
        }
        return result;
    }
    static StdList toCpp(PyObject* pyobj)
    {
        StdList result;
        for (int i = 0; i < PySequence_Size(pyobj); i++) {
            PyObject* pyItem = PySequence_GetItem(pyobj, i);
            result.push_back(Converter<typename StdList::value_type>::toCpp(pyItem));
        }
        return result;
    }
};

template <typename StdPair>
struct Converter_std_pair
{
    static inline bool isConvertible(const PyObject* pyObj)
    {
        return PySequence_Check(const_cast<PyObject*>(pyObj));
    }
    static PyObject* toPython(StdPair cppobj)
    {
        typename StdPair::first_type first(cppobj.first);
        typename StdPair::second_type second(cppobj.second);
        PyObject* tuple = PyTuple_New(2);
        PyTuple_SET_ITEM(tuple, 0, Converter<typename StdPair::first_type>::toPython(first));
        PyTuple_SET_ITEM(tuple, 1, Converter<typename StdPair::second_type>::toPython(second));
        return tuple;
    }
    static StdPair toCpp(PyObject* pyobj)
    {
        StdPair result;
        PyObject* pyFirst = PySequence_GetItem(pyobj, 0);
        PyObject* pySecond = PySequence_GetItem(pyobj, 1);
        result.first = Converter<typename StdPair::first_type>::toCpp(pyFirst);
        result.second = Converter<typename StdPair::second_type>::toCpp(pySecond);
        return result;
    }
};

template <typename StdMap>
struct Converter_std_map
{
    static inline bool isConvertible(const PyObject* pyObj)
    {
        return PyDict_Check(const_cast<PyObject*>(pyObj));
    }

    static PyObject* toPython(StdMap cppobj)
    {
        PyObject* result = PyDict_New();
        typedef typename StdMap::iterator IT;
        IT it;

        for (it = cppobj.begin(); it != cppobj.end(); it++) {
            typename StdMap::key_type h_key((*it).first);
            typename StdMap::mapped_type h_val((*it).second);
            PyDict_SetItem(result,
                           Converter<typename StdMap::key_type>::toPython(h_key),
                           Converter<typename StdMap::mapped_type>::toPython(h_val));
        }

        return result;
    }
    static StdMap toCpp(PyObject* pyobj)
    {
        StdMap result;

        PyObject* key;
        PyObject* value;
        Py_ssize_t pos = 0;

        Py_INCREF(pyobj);

        while (PyDict_Next(pyobj, &pos, &key, &value)) {
            result.insert(typename StdMap::value_type(
                    Converter<typename StdMap::key_type>::toCpp(key),
                    Converter<typename StdMap::mapped_type>::toCpp(value)));
        }

        Py_DECREF(pyobj);

        return result;
    }
};

} // namespace Shiboken

#endif // CONVERSIONS_H
