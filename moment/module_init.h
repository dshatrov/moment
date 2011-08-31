/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#ifndef __MOMENT__MODULE_INIT__H__
#define __MOMENT__MODULE_INIT__H__


#include <gmodule.h>


// This file must be included just once by module code.
// It provides Moment-specific static initialization functions.
// Currently, Glib's implementation of loadable modules is used.


extern "C" {

extern void moment_module_init (void);

extern void moment_module_unload (void);

G_MODULE_EXPORT const gchar* g_module_check_init (GModule * const /* module */)
{
    moment_module_init ();
    return NULL;
}

G_MODULE_EXPORT void g_module_unload (GModule * const /* module */)
{
    moment_module_unload ();
}

}


#endif /* __MOMENT__MODULE_INIT__H__ */

