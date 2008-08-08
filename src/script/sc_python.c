/*
===========================================================================
Copyright (C) 2008 John Black

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_PYTHON

#define MAX_PYTHONFILE 32768

#include "sc_public.h"
#include "sc_python.h"
//#include "../python/python_local.h"

// TODO: move these to something like src/python/py_game.c
static PyMethodDef game_methods[] = {
 {NULL, NULL, 0, NULL}
};

PyObject *gamemodule;
PyObject *vec3d_module;
PyObject *vec3d;

/* 
 * We can't keep making new python module objects everytime we call 
 * update_context so we steal them from the main dict
 */
PyObject *mainModule;
PyObject *mainDict;

static scDataTypeHash_t* convert_to_hash( PyObject *hash_obj )
{
  scDataTypeValue_t val;
  scDataTypeHash_t *hash = SC_HashNew();
  PyObject *key, *value, *keystr;
  Py_ssize_t pos = 0;

  while (PyDict_Next(hash_obj, &pos, &key, &value)) {
      convert_to_value( value, &val, TYPE_ANY );
      keystr = PyObject_Str( key );
      SC_HashSet(hash, PyString_AsString( keystr ), &val);
      Py_DECREF(keystr);
  }
  return hash;
}

static scDataTypeArray_t* convert_to_array( PyObject *array_obj )
{
  scDataTypeValue_t val;
  int index;
  scDataTypeArray_t *array = SC_ArrayNew();
  PyObject *iterator = PyObject_GetIter(array_obj);
  PyObject *item;

  if (iterator == NULL) {
    return array;
  }

  index = 0;
  while ( (item = PyIter_Next(iterator) ) ) {
    convert_to_value( item, &val, TYPE_ANY );
    SC_ArraySet(array, index, &val);
    Py_DECREF(item);
    index++;
  }

  Py_DECREF(iterator);

  if (PyErr_Occurred()) {
      /* propagate error */
  }
  else {
      /* continue doing useful work */
  }
  return array;
}
static scDataTypeString_t* convert_to_string( PyObject *str_obj  )
{
  const char *pystr = PyString_AsString( str_obj );
  scDataTypeString_t *string = SC_StringNewFromChar(pystr);
  return string;
}

static scDataTypeFunction_t* convert_to_function( PyObject *function_obj  )
{
  scDataTypeFunction_t *function = SC_FunctionNew();
  function->langage = LANGAGE_PYTHON;
  Py_INCREF(function_obj);
  function->data.pyfunc= function_obj;
  return function;
}

/* Convert a python object into a script data value */
void convert_to_value ( PyObject *pyvalue, scDataTypeValue_t *value, scDataType_t type )
{
  value->gc.count = 0;
  if (pyvalue == Py_None){
    value->type = TYPE_UNDEF;
  }
  else if ( PyInt_Check(pyvalue) )
  {
    value->type = TYPE_INTEGER;
    value->data.integer = PyInt_AsLong( pyvalue );
  }
  else if ( PyFloat_Check(pyvalue) )
  {
    value->type = TYPE_FLOAT;
    value->data.floating = PyFloat_AsDouble( pyvalue );
  }
  else if (  PyBool_Check(pyvalue) )
  {
    value->type = TYPE_BOOLEAN;
    value->data.boolean = ( pyvalue == Py_True) ? qtrue : qfalse;
  }
  else if (  PyString_Check(pyvalue) )
  {
    value->type = TYPE_STRING;
    value->data.string = convert_to_string( pyvalue );
  }
  else if ( PyDict_Check(pyvalue) )
  {
    value->type = TYPE_HASH;
    value->data.hash = convert_to_hash( pyvalue );
  }
  else if ( PyList_Check(pyvalue) || PyTuple_Check( pyvalue ) )
  {
    value->type = TYPE_ARRAY;
    value->data.array = convert_to_array( pyvalue );
  }
  else if ( PyCallable_Check(pyvalue) )
  {
    value->type = TYPE_FUNCTION;
    value->data.function = convert_to_function( pyvalue );
  }
  else if ( PyType_IsSubtype(pyvalue->ob_type, &PyScBaseObject_Type) )
  {
    PyScBaseObject *py_scobject;
    py_scobject = (PyScBaseObject*)pyvalue;
    value->type = TYPE_OBJECT;
    value->data.object = py_scobject->sc_object;
//    value->data.function = convert_to_function( pyvalue );
  }
//    case LUA_TFUNCTION:
//      value->type = TYPE_FUNCTION;
//      pop_function(L, value->data.function);
//      break;
//
//    default:
//      // TODO: Error
//      break;
//  }
  else
  {
#ifdef UNITTEST
    printf("convert_to_value type fallthrough\n");
    print_pyobject( pyvalue->ob_type );
#endif
    value->type = TYPE_UNDEF;
  }
}

static PyObject *convert_from_array( scDataTypeArray_t *array )
{
  int i;
  PyObject *list;

  list = PyList_New( array->size ); // Creat a new python list
  for( i = 0; i < array->size; i++ )
  {
    PyList_SetItem( list, i, convert_from_value( &array->data[i] ) );
  }
  return list;
}

static PyObject *convert_from_hash( scDataTypeHash_t *hash )
{
  int i;
  PyObject *dict, *temp;

  dict = PyDict_New();
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    temp = convert_from_value( &hash->data[i].value );
    PyDict_SetItemString( dict, SC_StringToChar(&hash->data[i].key), temp);
  }
  return dict;
}

static PyObject *convert_from_function( scDataTypeFunction_t *function )
{
#ifndef UNITTEST
  PyObject *temp;
  temp = PyFunction_new( &PyFunctionType, NULL, NULL );
  PyFunction_init( (PyFunction*)temp, function );
  if ( temp == NULL ) return Py_BuildValue("");
  return temp;
#else
  return Py_BuildValue("");
#endif
}

static PyObject *convert_from_object( scObject_t *sc_object )
{
  PyScBaseObject *py_object;
  py_object = (PyScBaseObject*)PyScObject_new( (ScPyTypeObject*)sc_object->class->python_type, NULL, NULL);
  
  py_object->sc_object = sc_object;
  
  return (PyObject*)py_object;
}

static PyObject *convert_from_class( scClass_t *class )
{
  return class->python_type;
}

/* Convert a script data value to a python object */
PyObject *convert_from_value( scDataTypeValue_t *value )
{
  switch( value->type )
  {
    case TYPE_UNDEF:
      return Py_BuildValue(""); // Python None object
    case TYPE_BOOLEAN:
      if (value->data.boolean)
        Py_RETURN_TRUE;
      else  // Py_RETURN_TRUE and Py_RETURN_FALSE are 
        Py_RETURN_FALSE;//  Macros for returning Py_True or Py_False, respectively
    case TYPE_INTEGER:
      return Py_BuildValue("i", value->data.integer ); // Python int or long type
    case TYPE_FLOAT:
      return Py_BuildValue("f", value->data.floating ); // Python float type
    case TYPE_STRING:
      return Py_BuildValue("s", value->data.string->data ); // Python str type
    case TYPE_ARRAY:
      return convert_from_array( value->data.array );
    case TYPE_HASH:
      return convert_from_hash( value->data.hash );
//    case TYPE_NAMESPACE:
//      push_hash( L, (scDataTypeHash_t*) value->data.namespace );
//      break;
    case TYPE_FUNCTION:
      return convert_from_function( value->data.function );
    case TYPE_OBJECT:
      return convert_from_object( value->data.object );
    case TYPE_CLASS:
      return convert_from_class( value->data.class );
    default:
#ifdef UNITTEST
      printf("convert_from_value type fallthrough %d \n", value->type);
#endif
      return Py_BuildValue(""); // Python None object
      break;
  }
}
#ifndef UNITTEST

void SC_Python_Init( void )
{
//  scDataTypeValue_t *value;
  
  G_Printf("------- Game Python Initialization -------\n");
  Py_Initialize();
  // Make python threads work at all
  PyEval_InitThreads( );
  
  trap_Cvar_Set( "py_initialized", "1" );
  trap_Cvar_Update( &py_initialized ); 
  
  mainModule = PyImport_AddModule("__main__"); // get __main__ ...
  mainDict = PyModule_GetDict( mainModule ); // ... so we can get its dict ...
//  
//  value = BG_Alloc( sizeof( scDataTypeValue_t ));
//  value->type = TYPE_INTEGER;
//  value->data.integer = 1337;
  
//  SC_NamespaceSet("game.test", value);
//  PyImport_AddModule("game");
//  gamemodule = Py_InitModule("game", game_methods);
//  if (PyType_Ready(&EntityType) < 0)
//    return;
//  Py_INCREF(&EntityType);
//  PyModule_AddObject(gamemodule, "Entity", (PyObject *)&EntityType);
//  if (PyType_Ready(&EntityStateType) < 0)
//    return;
//  Py_INCREF(&EntityStateType);
//  PyModule_AddObject(gamemodule, "EntityState", (PyObject *)&EntityStateType);
  
//  if (PyType_Ready(&Vec3dType) < 0)
//    return;
//  Py_INCREF(&Vec3dType);
//  PyModule_AddObject(gamemodule, "Vec3d", (PyObject *)&Vec3dType);
//  PyRun_SimpleString("sys.path.append(\"/home/john/tremulous/server/test_base/stfu-trem/python\")");
//  vec3d_module= PyImport_ImportModule("vec3d");
//  if (!vec3d_module){
//    Com_Printf("^1Cannot find vec3d.py\n");
//    vec3d = NULL;
//  } else {
//    vec3d = PyObject_GetAttrString(vec3d_module, "vec3d" );
//  }
  if (PyType_Ready(&PyFunctionType) < 0)
    return;
  if (PyType_Ready(&PyScMethod_Type) < 0)
    return;
  Py_INCREF(&PyScMethod_Type);

//  if (PyType_Ready(&PyScObject_Type) < 0)
//    return;
//  Py_INCREF(&EntityType);
  G_Printf("-----------------------------------\n");
}

/*
=================
SC_Python_Shutdown
=================
*/
void SC_Python_Shutdown( void )
{
  G_Printf("------- Game Python Finalization -------\n");
  Py_DECREF( mainModule );
  Py_DECREF( mainDict );

//  if (vec3d_module){ 
//    Py_DECREF( vec3d_module);
//    vec3d = NULL;
//  }
//  if (vec3d){
//    Py_DECREF( vec3d );
//    vec3d = NULL;
//  }
//  Py_DECREF( &EntityStateType );
//  Py_DECREF( &EntityType );
//  if (gamemodule){
//    Py_DECREF( gamemodule );
//    gamemodule = NULL;
//  }
  trap_Cvar_Set( "py_initialized", "0" );
  Py_Finalize();
  G_Printf("-----------------------------------\n");
}

static void update_module( scDataTypeString_t *module, scDataTypeValue_t *value )
{
  PyObject *pyModule;
  scDataTypeHash_t* hash =(scDataTypeHash_t*) value->data.namespace;
  char *keystring;
  
  pyModule = PyImport_AddModule( SC_StringToChar( module ) );
  if (!pyModule)
    return; //ERROR!
  int i;
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    keystring = (char*)SC_StringToChar(&hash->data[i].key);
    if( PyObject_HasAttrString(pyModule, keystring) )
      continue;
    PyModule_AddObject( pyModule, keystring, convert_from_value( &hash->data[i].value ));
  }
}

static void update_context( void )
{
  // TODO: make better updating system
  scDataTypeHash_t* hash = (scDataTypeHash_t*) namespace_root;
  char *keystring;
  int i;
  for( i = 0; i < hash->buflen; i++ )
  {
    if(SC_StringIsEmpty(&hash->data[i].key))
      continue;
    if ( hash->data[i].value.type == TYPE_NAMESPACE )
    {
      update_module( &hash->data[i].key,  &hash->data[i].value );
    }
    else 
    {
      keystring = (char*)SC_StringToChar(&hash->data[i].key);
      PyModule_AddObject( mainModule, keystring, convert_from_value( &hash->data[i].value ));
    }
  }
}

/*
=================
SC_Python_RunScript
=================
*/
qboolean SC_Python_RunScript( const char *filename )
{
  int             len;
  fileHandle_t    f;
  char            buf[MAX_PYTHONFILE];
  PyObject       *codeObject, *loc, *dum;

  Com_Printf("...loading '%s'\n", filename);

  len = trap_FS_FOpenFile(filename, &f, FS_READ);
  if(!f)
  {
    Com_Printf(va(S_COLOR_RED "Cannot load %s: file not found\n", filename));
    return qfalse;
  }

  if(len >= MAX_PYTHONFILE)
  {
    Com_Printf(S_COLOR_RED "Cannot load %s: file too large, file is %i, max allowed is %i\n", filename, len, MAX_PYTHONFILE);
    trap_FS_FCloseFile(f);
    return qfalse;
  }
#ifndef UNITTEST
  if (!sc_python.integer){
    Com_Printf(S_COLOR_RED "Cannot load %s: python disabled\n", filename);
    return qfalse;
  }
  
  if (!py_initialized.integer){
    Com_Printf(S_COLOR_RED "Cannot load %s: python not initilized\n", filename);
    return qfalse;
  }
#endif
  trap_FS_Read(buf, len, f);
  buf[len] = 0;
  trap_FS_FCloseFile(f);

  update_context();
  loc = PyDict_New ();
  codeObject = Py_CompileString( buf, filename, Py_file_input );
  if (!codeObject){
    PyErr_Print( );
    return qfalse;
  }
  PyDict_SetItemString (mainDict, "__builtins__", PyEval_GetBuiltins ());
  dum = PyEval_EvalCode( (PyCodeObject*)codeObject, mainDict, loc );
  if (!dum){
    PyErr_Print( );
    return qfalse;
  }
  Py_XDECREF(loc);
  Py_XDECREF(dum);
  return qtrue;
}

#endif /*#ifndef UNITTEST*/

void SC_Python_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  PyObject *ArgsTuple, *ReturnValue;
  
#ifndef UNITTEST
  if (!sc_python.integer){
    Com_Printf(S_COLOR_RED "Cannot run function: python disabled\n");
    return;
  }
  
  if (!py_initialized.integer){
    Com_Printf(S_COLOR_RED "Cannot run function: python not initilized\n");
    return;
  }
#endif
  
  if (args)
  {
    scDataTypeValue_t *value;
    int index, narg;
    value = args;
    
    narg = 0;
    
    while ( value->type != TYPE_UNDEF && value->type <= TYPE_NAMESPACE )
    {
      narg++;
      value++;
    }
#ifdef UNITTEST
    printf("%d args\n", narg);
#endif
    ArgsTuple = PyTuple_New( narg );
    
    value = args;
    index = 0;
    while ( value->type != TYPE_UNDEF && value->type <= TYPE_NAMESPACE)
    {
      PyTuple_SetItem( ArgsTuple, index++, convert_from_value( value ) );
      narg++;
      value++;
    }
  }
  else
    ArgsTuple = PyTuple_New( 0 );
  ReturnValue = PyObject_CallObject( func->data.pyfunc, ArgsTuple); // do the call
  Py_DECREF(ArgsTuple);
  if (!ReturnValue)
  {
    PyErr_Print();
    return;
  }
  convert_to_value(ReturnValue, ret, TYPE_ANY);
  Py_DECREF(ReturnValue);
}


#endif