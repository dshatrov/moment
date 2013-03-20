/*  Moment Video Server - High performance media server
    Copyright (C) 2013 Dmitry Shatrov
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


#ifndef MOMENT__MP4_MUXER__H__
#define MOMENT__MP4_MUXER__H__


#include <libmary/libmary.h>


namespace Moment {

using namespace M;

class Mp4Muxer : public DependentCodeReferenced
{
private:
    mt_const DataDepRef<PagePool> page_pool;
    mt_const DataDepRef<Sender> sender;

    PagePool::PageListHead header;

public:
    mt_const void init (PagePool * mt_nonnull page_pool,
                        Sender   * mt_nonnull sender);

    Mp4Muxer (Object * const coderef_container)
        : DependentCodeReferenced (coderef_container),
          page_pool (coderef_container),
          sender    (coderef_container)
    {}
};

}


#endif /* MOMENT__MP4_MUXER__H__ */

