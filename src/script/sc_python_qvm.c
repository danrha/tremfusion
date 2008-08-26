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

#include "sc_public.h"

void SC_Python_Init( void )
{
  trap_PythonInit(namespace_root, sc_constants);
}

void SC_Python_Shutdown( void )
{
  trap_PythonShutdown();
}

qboolean SC_Python_RunScript( const char *filename )
{
  return trap_PythonRunFile( filename );
}

void SC_Python_InitClass( scClass_t *class )
{
  trap_PythonInitClass( class );
}

int SC_Python_RunFunction( const scDataTypeFunction_t *func, scDataTypeValue_t *args, scDataTypeValue_t *ret )
{
  return trap_PythonRunFunction( func, args, ret);
}