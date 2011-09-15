/*  Moment Video Server - High performance media server
    Copyright (C) 2011 Dmitry Shatrov
    e-mail: shatrov@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <moment/local_storage.h>


namespace Moment {

Storage::FileKey
LocalStorage::openFile (ConstMemory    const filename,
			Connection  ** const ret_conn)
{
    exc_throw<InternalException> (InternalException::NotImplemented);

    Ref<FileEntry> file_entry = grab (new FileEntry);

    if (!file_entry->file.open (filename,
				File::OpenFlags::Create | File::OpenFlags::Truncate,
				File::AccessMode::WriteOnly))
    {
	logE_ (_func, "file.open() failed: ", exc->toString());
	return NULL;
    }

    file_entry->conn.setFile (&file_entry->file);

    mutex.lock ();
    file_list.append (file_entry);
    mutex.unlock ();

    if (ret_conn)
	*ret_conn = &file_entry->conn;

    FileEntry * const tmp_file_entry = file_entry;
    file_entry.setNoUnref ((FileEntry*) NULL);
    return tmp_file_entry;
}

void
LocalStorage::releaseFile (FileKey const file_key)
{
    FileEntry * const file_entry = static_cast <FileEntry*> (file_key);

    if (!file_entry->file.close ())
	logE_ (_func, "file.close() failed: ", exc->toString());

    mutex.lock ();
    file_list.remove (file_entry);
    mutex.unlock ();

    file_entry->unref ();
}

LocalStorage::~LocalStorage ()
{
    mutex.lock ();

    FileEntryList::iter iter (file_list);
    while (!file_list.iter_done (iter)) {
	FileEntry * const file_entry = file_list.iter_next (iter);
	file_entry->file.close ();
	file_entry->unref ();
    }

    mutex.unlock ();
}

}

