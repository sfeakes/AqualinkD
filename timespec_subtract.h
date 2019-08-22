/* Copyright (c) 1991, 1999 Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */


/* Based on https://www.gnu.org/software/libc/manual/html_node/Elapsed-Time.html
   Subtract the struct timespec values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */

#ifndef TIMESPEC_SUBTRACT_H
#define TIMESPEC_SUBTRACT_H

#include <time.h>

int timespec_subtract (struct timespec *result, const struct timespec *x,
                       const struct timespec *y);


#endif
