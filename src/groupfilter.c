/*
 * Copyright 2012 Vaibhav Bajpai <contact@vaibhavbajpai.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "groupfilter.h"

#include "errorhandlers.h"
#include "auto-assign.h"

struct groupfilter_result*
groupfilter(
            size_t num_groupfilter_clauses,
            struct groupfilter_clause** const groupfilter_clauseset,

            const struct grouper_result* const gresult,

            struct io_handler_s* io,
            struct io_reader_s*  read_ctxt,

            int branch_id
           ) {

  /* free'd just before calling ungrouper(...) */
  struct groupfilter_result*
  gfilter_result = calloc(1, sizeof(struct groupfilter_result));
  if (gfilter_result == NULL)
    errExit("calloc");

  /* assign group filter func for each term */
  for (int k = 0; k < num_groupfilter_clauses; k++) {

    struct groupfilter_clause* gclause = groupfilter_clauseset[k];

    for (int j = 0; j < gclause->num_terms; j++) {

      /* assign a uintX_t specific function depending on grule->op */
      struct groupfilter_term* term = gclause->termset[j];
      assign_groupfilter_func(term);
    }
  }

  /* initialize an output stream if file write is requested */
  io_writer_t* writer_ctxt = NULL;
  if (verbose_v && file) {

    /* get a file descriptor */
    char* filename = (char*)0L;
    if (asprintf(&filename, "%s/groupfilter-branch-%d-filtered-groups.%s",
                 dirpath, branch_id, io->io_get_format_suffix()) < 0)
      errExit("asprintf(...): failed");
    int out_fd = get_wronly_fd(filename);
    if(out_fd == -1) errExit("get_wronly_fd(...) returned -1");
    else free(filename);

    /* get the output stream */
    writer_ctxt = io->io_write_init(read_ctxt, out_fd,
                                    gfilter_result->num_filtered_groups);
    exitOn(writer_ctxt == NULL);
  }

  /* iterate over each group */
  for (int i = 0, j = 0; i < gresult->num_groups; i++) {

    struct group* group = gresult->groupset[i];
    bool satisfied = false;

    /* process each group filter clause (clauses are OR'd) */
    for (int k = 0; k < num_groupfilter_clauses; k++) {

      struct groupfilter_clause* const gfclause = groupfilter_clauseset[k];

      /* process each group filter term (terms within a clause are AND'd) */
      for (j = 0; j < gfclause->num_terms; j++) {

        struct groupfilter_term* const term = gfclause->termset[j];

        /* run the comparator function of the filter term on the record */
        if (
            !term->func(
                        group,
                        term->field,
                        term->value,
                        term->delta
                       )
           )
          break;
      }

      /* if any term is not satisfied, move to the next clause */
      if (j < gfclause->num_terms)
        continue;
      /* else this clause is TRUE; so everything is TRUE; break out */
      else {
        satisfied = true; break;
      }
    }

    /* if rules are satisfied then save this record */
    if (satisfied) {
      gfilter_result->num_filtered_groups += 1;

      /* free'd just after returning from ungrouper(...) */
      gfilter_result->filtered_groupset = (struct group**)
      realloc(gfilter_result->filtered_groupset,
             (gfilter_result->num_filtered_groups) * sizeof(struct group*));
      if (gfilter_result->filtered_groupset == NULL)
        errExit("realloc");
      gfilter_result->filtered_groupset
      [gfilter_result->num_filtered_groups-1] = gresult->groupset[i];

      /* write the record to the output stream */
      if (verbose_v && file) {
        struct group* fgroup = gresult->groupset[i];
        exitOn(io->io_write_record(writer_ctxt,
                    fgroup->aggr_result->aggr_record->aggr_record) < 0);
      }
    }
  }

  /* close the output stream */
  if (verbose_v && file) {
    exitOn(io->io_write_close(writer_ctxt) < 0);
  }

  return gfilter_result;
}
