/* Copyright (c) 1991, 1999 Free Software Foundation, Inc.
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */


/* Based on https://www.gnu.org/software/libc/manual/html_node/Calculating-Elapsed-Time.html
   Subtract the struct timespec values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */

#include <string.h>
#include "timespec_subtract.h"

int timespec_subtract (struct timespec *result, const struct timespec *x,
                       const struct timespec *y)
{
  struct timespec tmp;

  memcpy (&tmp, y, sizeof(struct timespec));
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_nsec < tmp.tv_nsec)
    {
      int nsec = (tmp.tv_nsec - x->tv_nsec) / 1000000000 + 1;
      tmp.tv_nsec -= 1000000000 * nsec;
      tmp.tv_sec += nsec;
    }
  if (x->tv_nsec - tmp.tv_nsec > 1000000000)
    {
      int nsec = (x->tv_nsec - tmp.tv_nsec) / 1000000000;
      tmp.tv_nsec += 1000000000 * nsec;
      tmp.tv_sec -= nsec;
    }

  /* Compute the time remaining to wait.
   tv_nsec is certainly positive. */
  result->tv_sec = x->tv_sec - tmp.tv_sec;
  result->tv_nsec = x->tv_nsec - tmp.tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < tmp.tv_sec;
}
