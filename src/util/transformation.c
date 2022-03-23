#include <fcntl.h>
#include <unistd.h>
#include <string.h> /*strlen*/
#include <stdio.h> /*perror*/
#include <stdlib.h> /*exit*/
#include <ctype.h> /*macros isdigit,isalpha,etc*/

#include "transformation.h"
#include "env.h"
#include "../util.h"

/*! @addtogroup transformations
 * @{ */
void transformation_apply (const char *transf, const char *src, const char *dst, const char *transformation_path)
{
  char file[strlen (transformation_path)];
  strcpy (file, transformation_path);
  strcat (file, transf);

  int in_fd = oopen (src, O_RDONLY);
  dup2 (in_fd, STDIN_FILENO);
  cclose (in_fd);
  int out_fd = oopen (dst, O_WRONLY);
  dup2 (in_fd, STDOUT_FILENO);
  close (out_fd);

  pid_t pid;
  if ((pid = fork ()) == 0)
    execlp (file, transf, src, dst, NULL);

  else if (pid < 0)
    {
      perror ("transformation failed forking");
      exit (EXIT_FAILURE);
    }
}

/*! @addtogroup parsing
 * @{ */

/*!
 *
 * @param string A string that may start with whitespace
 * @param i position to start reading string
 * @return The first transformation found reading from string[i]
 */
TRANSFORMATION parse_transformation (const char *string, unsigned int *i)
{
  if (i == NULL) i = 0;
  while (!isalpha(string[*i]))
    *i += 1;

  const int transformation_name_length_upper_bound = 20;
  char buf[transformation_name_length_upper_bound];
  int j;
  for (j = 0; isalpha(string[*i]) && j < transformation_name_length_upper_bound; *i += 1, j++)
    buf[j] = string[*i];

  buf[j] = '\0';

  if (!strcmp (TRANSFORMATION_NAMES.bcompress, buf))
    return bcompress;
  else if (!strcmp (TRANSFORMATION_NAMES.bdecompress, buf))
    return bdecompress;
  else if (!strcmp (TRANSFORMATION_NAMES.decrypt, buf))
    return decrypt;
  else if (!strcmp (TRANSFORMATION_NAMES.encrypt, buf))
    return encrypt;
  else if (!strcmp (TRANSFORMATION_NAMES.gcompress, buf))
    return gcompress;
  else if (!strcmp (TRANSFORMATION_NAMES.gdecompress, buf))
    return gdecompress;
  else if (!strcmp (TRANSFORMATION_NAMES.nop, buf))
    return nop;
  else
    {
      fprintf (stderr, "Invalid operation ending at: i=%d\n", *i);
      return -1;
    }
}
//! @} end of group parsing

void print_transformations (const TRANSFORMATION transformation[])
{
  for (unsigned int i = 0; transformation[i] != -1 && i < 20; i++)
    fprintf(stderr,"%s ", transformation_get_name (transformation[i]));
}
//! @} end of group transformations