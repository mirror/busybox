/* vi: set sw=4 ts=4: */
/*
 * Shell wrapper file for busybox
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "Config.h"

/* This is to make testing things a bit simpler (to avoid
 * a full recompile) till we get the new build system in place */
#if 0
#undef BB_FEATURE_LASH
#undef BB_FEATURE_HUSH
#undef BB_FEATURE_MSH
#define BB_FEATURE_ASH
#endif

#if defined BB_FEATURE_ASH
#include "ash.c"
#elif defined BB_FEATURE_MSH
#include "msh.c"
#elif defined BB_FEATURE_HUSH
#include "hush.c"
#elif defined BB_FEATURE_LASH
#include "lash.c"
#endif	

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
