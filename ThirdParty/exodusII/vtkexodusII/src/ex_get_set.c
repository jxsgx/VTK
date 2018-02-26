/*
 * Copyright (c) 2005-2017 National Technology & Engineering Solutions
 * of Sandia, LLC (NTESS).  Under the terms of Contract DE-NA0003525 with
 * NTESS, the U.S. Government retains certain rights in this software.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of NTESS nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "exodusII.h"     // for ex_err, ex_name_of_object, etc
#include "exodusII_int.h" // for ex_check_valid_file_id, etc
#include "vtk_netcdf.h"       // for NC_NOERR, nc_get_var_int, etc
#include <inttypes.h>     // for PRId64
#include <stdio.h>        // for snprintf, NULL

/*!
 * reads the set entry list and set extra list for a single set
 * \param   exoid                   exodus file id
 * \param   set_type                set type
 * \param   set_id                  set id
 * \param  *set_entry_list          array of entries in set. Set to NULL to not
 * read.
 * \param  *set_extra_list          array of extras in set. Set to NULL to not
 * read.
 */

int ex_get_set(int exoid, ex_entity_type set_type, ex_entity_id set_id, void_int *set_entry_list,
               void_int *set_extra_list) /* NULL if dont want to retrieve data */
{

  int   dimid, entry_list_id, extra_list_id, status;
  int   set_id_ndx;
  char  errmsg[MAX_ERR_LENGTH];
  char *entryptr = NULL;
  char *extraptr = NULL;

  EX_FUNC_ENTER();
  ex_check_valid_file_id(exoid, __func__);

  /* first check if any sets are specified */
  if ((status = nc_inq_dimid(exoid, ex_dim_num_objects(set_type), &dimid)) != NC_NOERR) {
    snprintf(errmsg, MAX_ERR_LENGTH, "Warning: no %ss stored in file id %d",
             ex_name_of_object(set_type), exoid);
    ex_err(__func__, errmsg, status);
    EX_FUNC_LEAVE(EX_WARN);
  }

  /* Lookup index of set id in VAR_*S_IDS array */
  set_id_ndx = ex_id_lkup(exoid, set_type, set_id);
  if (set_id_ndx <= 0) {
    ex_get_err(NULL, NULL, &status);

    if (status != 0) {
      if (status == EX_NULLENTITY) {
        snprintf(errmsg, MAX_ERR_LENGTH, "Warning: %s %" PRId64 " is NULL in file id %d",
                 ex_name_of_object(set_type), set_id, exoid);
        ex_err(__func__, errmsg, EX_NULLENTITY);
        EX_FUNC_LEAVE(EX_WARN);
      }

      snprintf(errmsg, MAX_ERR_LENGTH,
               "ERROR: failed to locate %s id %" PRId64 " in VAR_*S_IDS array in file id %d",
               ex_name_of_object(set_type), set_id, exoid);
      ex_err(__func__, errmsg, status);
      EX_FUNC_LEAVE(EX_FATAL);
    }
  }

  /* setup more pointers based on set_type */
  if (set_type == EX_NODE_SET) {
    entryptr = VAR_NODE_NS(set_id_ndx);
    extraptr = NULL;
  }
  else if (set_type == EX_EDGE_SET) {
    entryptr = VAR_EDGE_ES(set_id_ndx);
    extraptr = VAR_ORNT_ES(set_id_ndx);
  }
  else if (set_type == EX_FACE_SET) {
    entryptr = VAR_FACE_FS(set_id_ndx);
    extraptr = VAR_ORNT_FS(set_id_ndx);
  }
  else if (set_type == EX_SIDE_SET) {
    entryptr = VAR_ELEM_SS(set_id_ndx);
    extraptr = VAR_SIDE_SS(set_id_ndx);
  }
  if (set_type == EX_ELEM_SET) {
    entryptr = VAR_ELEM_ELS(set_id_ndx);
    extraptr = NULL;
  }

  /* inquire id's of previously defined dimensions and variables */
  if ((status = nc_inq_varid(exoid, entryptr, &entry_list_id)) != NC_NOERR) {
    snprintf(errmsg, MAX_ERR_LENGTH,
             "ERROR: failed to locate entry list for %s %" PRId64 " in file id %d",
             ex_name_of_object(set_type), set_id, exoid);
    ex_err(__func__, errmsg, status);
    EX_FUNC_LEAVE(EX_FATAL);
  }

  /* If client doet not pass in an array to store the
     extra list, don't access it at all */

  /* only do extra list for edge, face and side sets */
  if (set_extra_list) {
    if ((status = nc_inq_varid(exoid, extraptr, &extra_list_id)) != NC_NOERR) {
      snprintf(errmsg, MAX_ERR_LENGTH,
               "ERROR: failed to locate extra list for %s %" PRId64 " in file id %d",
               ex_name_of_object(set_type), set_id, exoid);
      ex_err(__func__, errmsg, status);
      EX_FUNC_LEAVE(EX_FATAL);
    }
  }

  /* read in the entry list and extra list arrays unless they are NULL */
  if (set_entry_list) {
    if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
      status = nc_get_var_longlong(exoid, entry_list_id, set_entry_list);
    }
    else {
      status = nc_get_var_int(exoid, entry_list_id, set_entry_list);
    }

    if (status != NC_NOERR) {
      snprintf(errmsg, MAX_ERR_LENGTH,
               "ERROR: failed to get entry list for %s %" PRId64 " in file id %d",
               ex_name_of_object(set_type), set_id, exoid);
      ex_err(__func__, errmsg, status);
      EX_FUNC_LEAVE(EX_FATAL);
    }
  }

  /* only do extra list for edge, face and side sets */
  if (set_extra_list) {
    if (ex_int64_status(exoid) & EX_BULK_INT64_API) {
      status = nc_get_var_longlong(exoid, extra_list_id, set_extra_list);
    }
    else {
      status = nc_get_var_int(exoid, extra_list_id, set_extra_list);
    }

    if (status != NC_NOERR) {
      snprintf(errmsg, MAX_ERR_LENGTH,
               "ERROR: failed to get extra list for %s %" PRId64 " in file id %d",
               ex_name_of_object(set_type), set_id, exoid);
      ex_err(__func__, errmsg, status);
      EX_FUNC_LEAVE(EX_FATAL);
    }
  }
  EX_FUNC_LEAVE(EX_NOERR);
}