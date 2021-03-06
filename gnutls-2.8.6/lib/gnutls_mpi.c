/*
 * Copyright (C) 2001, 2002, 2003, 2004, 2005, 2008 Free Software Foundation
 *
 * Author: Nikos Mavrogiannopoulos
 *
 * This file is part of GNUTLS.
 *
 * The GNUTLS library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA
 *
 */

/* Here lie everything that has to do with large numbers, libgcrypt and
 * other stuff that didn't fit anywhere else.
 */

#include <gnutls_int.h>
#include <libtasn1.h>
#include <gnutls_errors.h>
#include <gnutls_num.h>
#include <gnutls_mpi.h>
#include <random.h>

/* Functions that refer to the mpi library.
 */

#define clearbit(v,n)    ((unsigned char)(v) & ~( (unsigned char)(1) << (unsigned)(n)))

bigint_t
_gnutls_mpi_randomize (bigint_t r, unsigned int bits,
		       gnutls_rnd_level_t level)
{
  size_t size = 1 + (bits / 8);
  int ret;
  int rem, i;
  bigint_t tmp;
  char tmpbuf[512];
  opaque *buf;
  int buf_release = 0;

  if (size < sizeof (tmpbuf))
    {
      buf = tmpbuf;
    }
  else
    {
      buf = gnutls_malloc (size);
      if (buf == NULL)
	{
	  gnutls_assert ();
	  goto cleanup;
	}
      buf_release = 1;
    }


  ret = _gnutls_rnd (level, buf, size);
  if (ret < 0)
    {
      gnutls_assert ();
      goto cleanup;
    }

  /* mask the bits that weren't requested */
  rem = bits % 8;

  if (rem == 0)
    {
      buf[0] = 0;
    }
  else
    {
      for (i = 8; i >= rem; i--)
	buf[0] = clearbit (buf[0], i);
    }

  ret = _gnutls_mpi_scan (&tmp, buf, size);
  if (ret < 0)
    {
      gnutls_assert ();
      goto cleanup;
    }

  if (buf_release != 0)
    {
      gnutls_free (buf);
      buf = NULL;
    }

  if (r != NULL)
    {
      _gnutls_mpi_set (r, tmp);
      _gnutls_mpi_release (&tmp);
      return r;
    }

  return tmp;

cleanup:
  if (buf_release != 0)
    gnutls_free (buf);
  return NULL;
}

void
_gnutls_mpi_release (bigint_t * x)
{
  if (*x == NULL)
    return;

  _gnutls_mpi_ops.bigint_release (*x);
  *x = NULL;
}

/* returns zero on success
 */
int
_gnutls_mpi_scan (bigint_t * ret_mpi, const void *buffer, size_t nbytes)
{
  *ret_mpi =
    _gnutls_mpi_ops.bigint_scan (buffer, nbytes, GNUTLS_MPI_FORMAT_USG);
  if (*ret_mpi == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_MPI_SCAN_FAILED;
    }

  return 0;
}

/* returns zero on success. Fails if the number is zero.
 */
int
_gnutls_mpi_scan_nz (bigint_t * ret_mpi, const void *buffer, size_t nbytes)
{
  int ret;

  ret = _gnutls_mpi_scan (ret_mpi, buffer, nbytes);
  if (ret < 0)
    return ret;

  /* MPIs with 0 bits are illegal
   */
  if (_gnutls_mpi_get_nbits (*ret_mpi) == 0)
    {
      _gnutls_mpi_release (ret_mpi);
      return GNUTLS_E_MPI_SCAN_FAILED;
    }

  return 0;
}

int
_gnutls_mpi_scan_pgp (bigint_t * ret_mpi, const void *buffer, size_t nbytes)
{
  *ret_mpi =
    _gnutls_mpi_ops.bigint_scan (buffer, nbytes, GNUTLS_MPI_FORMAT_PGP);
  if (*ret_mpi == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_MPI_SCAN_FAILED;
    }

  return 0;
}

/* Always has the first bit zero */
int
_gnutls_mpi_dprint_lz (const bigint_t a, gnutls_datum_t * dest)
{
  int ret;
  opaque *buf = NULL;
  size_t bytes = 0;

  if (dest == NULL || a == NULL)
    return GNUTLS_E_INVALID_REQUEST;

  _gnutls_mpi_print_lz (a, NULL, &bytes);

  if (bytes != 0)
    buf = gnutls_malloc (bytes);
  if (buf == NULL)
    return GNUTLS_E_MEMORY_ERROR;

  ret = _gnutls_mpi_print_lz (a, buf, &bytes);
  if (ret < 0)
    {
      gnutls_free (buf);
      return ret;
    }

  dest->data = buf;
  dest->size = bytes;
  return 0;
}

int
_gnutls_mpi_dprint (const bigint_t a, gnutls_datum_t * dest)
{
  int ret;
  opaque *buf = NULL;
  size_t bytes = 0;

  if (dest == NULL || a == NULL)
    return GNUTLS_E_INVALID_REQUEST;

  _gnutls_mpi_print (a, NULL, &bytes);
  if (bytes != 0)
    buf = gnutls_malloc (bytes);
  if (buf == NULL)
    return GNUTLS_E_MEMORY_ERROR;

  ret = _gnutls_mpi_print (a, buf, &bytes);
  if (ret < 0)
    {
      gnutls_free (buf);
      return ret;
    }

  dest->data = buf;
  dest->size = bytes;
  return 0;
}

/* This function will copy the mpi data into a datum,
 * but will set minimum size to 'size'. That means that
 * the output value is left padded with zeros.
 */
int
_gnutls_mpi_dprint_size (const bigint_t a, gnutls_datum_t * dest, size_t size)
{
  int ret;
  opaque *buf = NULL;
  size_t bytes = 0;
  unsigned int i;

  if (dest == NULL || a == NULL)
    return GNUTLS_E_INVALID_REQUEST;

  _gnutls_mpi_print (a, NULL, &bytes);
  if (bytes != 0)
    buf = gnutls_malloc (MAX (size, bytes));
  if (buf == NULL)
    return GNUTLS_E_MEMORY_ERROR;

  dest->size = MAX (size, bytes);

  if (bytes <= size)
    {
      size_t diff = size - bytes;
      for (i = 0; i < diff; i++)
	buf[i] = 0;
      ret = _gnutls_mpi_print (a, &buf[diff], &bytes);
    }
  else
    {
      ret = _gnutls_mpi_print (a, buf, &bytes);
    }

  if (ret < 0)
    {
      gnutls_free (buf);
      return ret;
    }

  dest->data = buf;
  dest->size = MAX (size, bytes);
  return 0;
}

/* this function reads an integer
 * from asn1 structs. Combines the read and mpi_scan
 * steps.
 */
int
_gnutls_x509_read_int (ASN1_TYPE node, const char *value, bigint_t * ret_mpi)
{
  int result;
  opaque *tmpstr = NULL;
  int tmpstr_size;

  tmpstr_size = 0;
  result = asn1_read_value (node, value, NULL, &tmpstr_size);
  if (result != ASN1_MEM_ERROR)
    {
      gnutls_assert ();
      return _gnutls_asn2err (result);
    }

  tmpstr = gnutls_malloc (tmpstr_size);
  if (tmpstr == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_MEMORY_ERROR;
    }

  result = asn1_read_value (node, value, tmpstr, &tmpstr_size);
  if (result != ASN1_SUCCESS)
    {
      gnutls_assert ();
      gnutls_free (tmpstr);
      return _gnutls_asn2err (result);
    }

  result = _gnutls_mpi_scan (ret_mpi, tmpstr, tmpstr_size);
  gnutls_free (tmpstr);

  if (result < 0)
    {
      gnutls_assert ();
      return result;
    }

  return 0;
}

/* Writes the specified integer into the specified node.
 */
int
_gnutls_x509_write_int (ASN1_TYPE node, const char *value, bigint_t mpi,
			int lz)
{
  opaque *tmpstr;
  size_t s_len;
  int result;

  s_len = 0;
  if (lz)
    result = _gnutls_mpi_print_lz (mpi, NULL, &s_len);
  else
    result = _gnutls_mpi_print (mpi, NULL, &s_len);
  
  if (result != 0)
    {
      gnutls_assert();
      return result;
    }

  tmpstr = gnutls_malloc (s_len);
  if (tmpstr == NULL)
    {
      gnutls_assert ();
      return GNUTLS_E_MEMORY_ERROR;
    }

  if (lz)
    result = _gnutls_mpi_print_lz (mpi, tmpstr, &s_len);
  else
    result = _gnutls_mpi_print (mpi, tmpstr, &s_len);

  if (result != 0)
    {
      gnutls_assert ();
      gnutls_free (tmpstr);
      return GNUTLS_E_MPI_PRINT_FAILED;
    }

  result = asn1_write_value (node, value, tmpstr, s_len);

  gnutls_free (tmpstr);

  if (result != ASN1_SUCCESS)
    {
      gnutls_assert ();
      return _gnutls_asn2err (result);
    }

  return 0;
}
