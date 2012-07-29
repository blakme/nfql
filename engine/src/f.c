/*
 * Copyright 2012 Vaibhav Bajpai <contact@vaibhavbajpai.com>
 * Copyright 2011 Johannes 'josch' Schauer <j.schauer@email.de>
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

#define _GNU_SOURCE
#define STR(x) #x
#define XSTR(x) STR(x)
#define VERSION 2.5
#include "f.h"

struct parameters*
parse_cmdline_args(int argc, char** const argv) {

  int                                 opt;
  char*                               shortopts = "z:d:v:DhV";
  static struct option                longopts[] = {
    { "zlevel",     required_argument,  NULL,           'z' },
    { "dirpath",    required_argument,  NULL,           'd' },
    { "debug",      no_argument,        NULL,           'D' },
    { "verbose",    required_argument,  NULL,           'v' },
    { "version",    no_argument,        NULL,           'V' },
    { "help",       no_argument,        NULL,           'h' },
    {  NULL,        0,                  NULL,            0  }
  };
  enum verbosity_levels               verbose_level = -1;
  const char usage_string[] =
  "%s [OPTIONS] queryfile tracefile\t\t query the specified trace\n"
  "   or: %s [OPTIONS] queryfile -\t\t\t read the trace from stdin\n\n"
  "OPTIONS:\n"
  "-z, --zlevel\t\t change the compression level (default:5)\n"
  "-d, --dirpath\t\t save the results as flow-tools files in given dirpath\n"
  "-D, --debug\t\t enable debugging mode\n"
  "-v, --verbose\t\t increase the verbosity level\n"
  "-V, --version\t\t output version information and exit\n"
  "-h, --help\t\t display this help and exit\n";


  /* free'd after calling read_param_data(...) */
  struct parameters* param = calloc(1, sizeof(struct parameters));
  if (param == NULL)
    errExit("calloc");

  while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
    switch (opt) {
      case 'D': debug = TRUE; verbose_level = HIGH;
      case 'v':
        if(optarg) verbose_level = atoi(optarg);
        switch (verbose_level) {
          case HIGH: verbose_vvv = TRUE;
          case MEDIUM: verbose_vv = TRUE;
          case LOW: verbose_v = TRUE; break;
          default: errExit("valid verbosity levels: (1-3)");
        }
        break;
      case 'z':
        errno = 0;
        zlevel = (int) strtol(optarg, (char**) NULL, 10);
        if(errno != 0) errExit("invalid zlevel");
        else if(zlevel < 0 || zlevel > 9) errExit("valid zlevels: (0-9)");
        break;
      case 'd': file = TRUE; dirpath = optarg; break;
      case 'h': usageErr(usage_string, argv[0], argv[0]);
      case 'V': fprintf(stdout, "%s version %s", argv[0], XSTR(VERSION));
                exit(EXIT_SUCCESS);
      case ':': usageError(argv[0], "Missing argument", optopt);
      default: exit(EXIT_FAILURE);
    }
  }

  if (argc != optind + 2)
    usageErr(usage_string, argv[0], argv[0]);
  else {
    param->query_filename = argv[optind];
    param->trace_filename = argv[optind+1];
  }
  return param;
}

struct parameters_data*
open_trace_read_query(const struct parameters* const param) {

  /* param_data->query_mmap is free'd after calling parse_json_query(...)
   * param_data->query_mmap_stat is free'd after freeing param_data->query_mmap
   * param_data->trace is free'd before exiting from main(...)
   * param_data is free'd before exiting from main(...)
   */
  struct parameters_data* param_data = calloc(1,
                                              sizeof(struct parameters_data));
  if (param_data == NULL)
    errExit("calloc");

  int fsock;
  if(!strcmp(param->trace_filename,"-"))
    fsock = STDIN_FILENO;
  else {
    fsock = open(param->trace_filename, O_RDONLY);
    if (fsock == -1)
      errExit("open");
  }

  param_data->trace = ft_init(fsock);
  if (param_data->trace == NULL)
    errExit("ft_init(...) returned NULL");
  else
    param_data->trace_fsock = fsock;

  /* param_data->query_mmap_stat is free'd after freeing
   * param_data->query_mmap
   */
  param_data->query_mmap_stat = calloc(1, sizeof(struct stat));
  if (param_data->query_mmap_stat == NULL)
    errExit("calloc");
  fsock = open(param->query_filename, O_RDONLY);
  if (fsock == -1)
    errExit("open");
  if (fstat(fsock, param_data->query_mmap_stat) == -1)
    errExit("fstat");

  /* param_data->query_mmap is free'd after calling parse_json_query(...) */
  param_data->query_mmap =
  mmap(
       NULL,                                   /* starting page address */
       param_data->query_mmap_stat->st_size,   /* bytes to be mappped */
       PROT_READ,                              /* region accessibility */
       MAP_PRIVATE,                            /* flags */
       fsock,                                  /* file object */
       0                                       /* offset */
      );
  if (param_data->query_mmap == MAP_FAILED)
    errExit("mmap");
  if (close(fsock) == -1)
    errExit("close");

  return param_data;
}

struct json*
parse_json_query(const char* const query_mmap) {

  struct json_tokener* tok = json_tokener_new();
  struct json_object* query = json_tokener_parse_ex(tok, query_mmap, -1);
  if(tok->err != json_tokener_success)
    errExit("json_tokener_parse_ex(...)");
  json_tokener_free(tok);

  /* free'd after returning from prepare_flowquery(...) */
  struct json* json = calloc(1, sizeof(struct json));
  if (json == NULL)
    errExit("calloc");

  struct json_object* branchset = json_object_object_get(query, "branchset");
  json->num_branches = (size_t)json_object_array_length(branchset);

  /* free'd after returning from prepare_flowquery(...) */
  json->branchset = calloc(json->num_branches,
                           sizeof(struct json_branch*));
  if (json->branchset == NULL)
    errExit("calloc");





  /* -----------------------------------------------------------------------*/
  /*                        parse branch rules                              */
  /* -----------------------------------------------------------------------*/

  for (int i = 0; i < json->num_branches; i++) {

    struct json_object* branch_json = json_object_array_get_idx(branchset, i);

    /* free'd after returning from prepare_flowquery(...) */
    struct json_branch* branch = calloc(1, sizeof(struct json_branch));
    if (branch == NULL)
      errExit("calloc");
    json->branchset[i] = branch;


#ifdef FILTER

  /* -----------------------------------------------------------------------*/
  /*                         parse filter rules                             */
  /* -----------------------------------------------------------------------*/

    struct json_object* filter = json_object_object_get(branch_json, "filter");
    struct json_object* fdnf = json_object_object_get(filter, "dnf-expr");
    branch->num_filter_clauses = json_object_array_length(fdnf);

    /* free'd after returning from prepare_flowquery(...) */
    branch->filter_clauseset = calloc(branch->num_filter_clauses,
                                      sizeof(struct json_filter_clause*));
    if (branch->filter_clauseset == NULL)
      errExit("calloc");

    for (int j = 0; j < branch->num_filter_clauses; j++) {

      /* free'd after returning from prepare_flowquery(...) */
      struct json_filter_clause*
      fclause = calloc(1, sizeof(struct json_filter_clause));
      if (fclause == NULL)
        errExit("calloc");
      branch->filter_clauseset[j] = fclause;

      struct json_object* clause = json_object_array_get_idx(fdnf, j);
      struct json_object* termset = json_object_object_get(clause,"clause");
      fclause->num_terms = json_object_array_length(termset);

      /* free'd after returning from prepare_flowquery(...) */
      fclause->termset = calloc(fclause->num_terms,
                                sizeof(struct json_filter_term*));
      if (fclause->termset == NULL)
        errExit("calloc");

      for (int i = 0; i < fclause->num_terms; i++) {

        /* free'd after returning from prepare_flowquery(...) */
        struct json_filter_term*
        fterm = calloc(1, sizeof(struct json_filter_term));
        if (fterm == NULL)
          errExit("calloc");
        fclause->termset[i] = fterm;

        /* free'd after returning from prepare_flowquery(...) */
        fterm->off = calloc(1, sizeof(struct json_filter_term_offset));
        if (fterm->off == NULL)
          errExit("calloc");

        struct json_object* term = json_object_array_get_idx(termset, i);
        struct json_object* term_items = json_object_object_get(term,"term");

        /* free'd before returning from this function */
        struct json_object*
        delta = json_object_object_get(term_items, "delta");
        struct json_object*
        op = json_object_object_get(term_items, "op");
        struct json_object*
        foffset = json_object_object_get(term_items, "offset");
        struct json_object*
        fo_name = json_object_object_get(foffset, "name");
        struct json_object*
        fo_val = json_object_object_get(foffset, "value");
        struct json_object*
        fo_type = json_object_object_get(foffset, "datatype");

        fterm->delta = json_object_get_int(delta);
        fterm->op = strdup(json_object_get_string(op));
        if (fterm->op == NULL) errExit("strdup");

        fterm->off->name = strdup(json_object_get_string(fo_name));
        if (fterm->off->name == NULL) errExit("strdup");

        fterm->off->value = json_object_get_int(fo_val);
        fterm->off->datatype = strdup(json_object_get_string(fo_type));
        if (fterm->off->datatype == NULL) errExit("strdup");
      }
    }


  /* -----------------------------------------------------------------------*/

#endif





#ifdef GROUPER

  /* -----------------------------------------------------------------------*/
  /*                        parse grouper rules                             */
  /* -----------------------------------------------------------------------*/

    struct json_object* grouper =
    json_object_object_get(branch_json, "grouper");
    struct json_object* gdnf = json_object_object_get(grouper, "dnf-expr");
    branch->num_grouper_clauses = json_object_array_length(gdnf);

    /* free'd after returning from prepare_flowquery(...) */
    branch->grouper_clauseset = calloc(branch->num_grouper_clauses,
                                       sizeof(struct json_grouper_clause*));
    if (branch->grouper_clauseset == NULL)
      errExit("calloc");

    for (int j = 0; j < branch->num_grouper_clauses; j++) {

      /* free'd after returning from prepare_flowquery(...) */
      struct json_grouper_clause*
      gclause = calloc(1, sizeof(struct json_grouper_clause));
      if (gclause == NULL)
        errExit("calloc");
      branch->grouper_clauseset[j] = gclause;

      struct json_object* clause = json_object_array_get_idx(gdnf, j);
      struct json_object* termset = json_object_object_get(clause,"clause");
      gclause->num_terms = json_object_array_length(termset);

      /* free'd after returning from prepare_flowquery(...) */
      gclause->termset = calloc(gclause->num_terms,
                                sizeof(struct json_grouper_term*));
      if (gclause->termset == NULL)
        errExit("calloc");

      for (int i = 0; i < gclause->num_terms; i++) {

        /* free'd after returning from prepare_flowquery(...) */
        struct json_grouper_term*
        gterm = calloc(1, sizeof(struct json_grouper_term));
        if (gterm == NULL)
          errExit("calloc");
        gclause->termset[i] = gterm;

        /* free'd after returning from prepare_flowquery(...) */
        gterm->off = calloc(1, sizeof(struct json_grouper_term_offset));
        if (gterm->off == NULL)
          errExit("calloc");

        /* free'd after returning from prepare_flowquery(...) */
        gterm->op = calloc(1, sizeof(struct json_grouper_term_op));
        if (gterm->op == NULL)
          errExit("calloc");

        struct json_object* term = json_object_array_get_idx(termset, i);
        struct json_object* term_items = json_object_object_get(term,"term");

        /* free'd before returning from this function */
        struct json_object*
        delta = json_object_object_get(term_items, "delta");
        struct json_object*
        op = json_object_object_get(term_items, "op");
        struct json_object*
        offset = json_object_object_get(term_items, "offset");

        struct json_object*
        f1_name = json_object_object_get(offset, "f1_name");
        struct json_object*
        f2_name = json_object_object_get(offset, "f2_name");
        struct json_object*
        f1_datatype = json_object_object_get(offset, "f1_datatype");
        struct json_object*
        f2_datatype = json_object_object_get(offset, "f2_datatype");

        struct json_object*
        op_type = json_object_object_get(op, "type");
        struct json_object*
        op_name = json_object_object_get(op, "name");

        gterm->delta = json_object_get_int(delta);

        gterm->op->name = strdup(json_object_get_string(op_name));
        if (gterm->op->name == NULL) errExit("strdup");

        gterm->op->type = strdup(json_object_get_string(op_type));
        if (gterm->op->type == NULL) errExit("strdup");

        gterm->off->f1_name = strdup(json_object_get_string(f1_name));
        if (gterm->off->f1_name == NULL) errExit("strdup");

        gterm->off->f2_name = strdup(json_object_get_string(f2_name));
        if (gterm->off->f2_name == NULL) errExit("strdup");

        gterm->off->f1_datatype = strdup(json_object_get_string(f1_datatype));
        if (gterm->off->f1_datatype == NULL) errExit("strdup");

        gterm->off->f2_datatype = strdup(json_object_get_string(f2_datatype));
        if (gterm->off->f2_datatype == NULL) errExit("strdup");
      }
    }

  /* -----------------------------------------------------------------------*/

#endif














#ifdef GROUPERAGGREGATIONS

  /* -----------------------------------------------------------------------*/
  /*                        parse aggr rules                                */
  /* -----------------------------------------------------------------------*/

    struct json_object* aggr = json_object_object_get(grouper, "aggregation");
    struct json_object* aggr_clause = json_object_object_get(aggr, "clause");
    branch->num_aggr_clause_terms = json_object_array_length(aggr_clause);

    /* free'd after returning from prepare_flowquery(...) */
    branch->aggr_clause_termset = calloc(branch->num_aggr_clause_terms,
                                         sizeof(struct json_aggr_term*));
    if (branch->aggr_clause_termset == NULL)
      errExit("calloc");

    for (int i = 0; i < branch->num_aggr_clause_terms; i++) {

      /* free'd after returning from prepare_flowquery(...) */
      struct json_aggr_term* term = calloc(1, sizeof(struct json_aggr_term));
      if (term == NULL)
        errExit("calloc");
      branch->aggr_clause_termset[i] = term;

      /* free'd after returning from prepare_flowquery(...) */
      term->off = calloc(1, sizeof(struct json_aggr_term_offset));
      if (term->off == NULL)
        errExit("calloc");

      struct json_object* term_json = json_object_array_get_idx(aggr_clause, i);
      struct json_object* term_items = json_object_object_get(term_json,"term");

      /* free'd before returning from this function */
      struct json_object*
      op = json_object_object_get(term_items, "op");
      struct json_object*
      offset = json_object_object_get(term_items, "offset");

      struct json_object*
      name = json_object_object_get(offset, "name");
      struct json_object*
      datatype = json_object_object_get(offset, "datatype");

      term->op = strdup(json_object_get_string(op));
      if (term->op == NULL) errExit("strdup");

      term->off->name = strdup(json_object_get_string(name));
      if (term->off->name == NULL) errExit("strdup");

      term->off->datatype = strdup(json_object_get_string(datatype));
      if (term->off->datatype == NULL) errExit("strdup");
    }


  /* -----------------------------------------------------------------------*/

#endif















#ifdef GROUPFILTER

  /* -----------------------------------------------------------------------*/
  /*                        parse gfilter rules                             */
  /* -----------------------------------------------------------------------*/

    struct json_object*
    gfilter = json_object_object_get(branch_json, "groupfilter");
    struct json_object* gfdnf =
    json_object_object_get(gfilter, "dnf-expr");
    branch->num_groupfilter_clauses = json_object_array_length(gfdnf);

    /* free'd after returning from prepare_flowquery(...) */
    branch->groupfilter_clauseset = calloc(branch->num_groupfilter_clauses,
                                           sizeof(struct groupfilter_clause*));
    if (branch->groupfilter_clauseset == NULL)
      errExit("calloc");


    for (int j = 0; j < branch->num_groupfilter_clauses; j++) {

      /* free'd after returning from prepare_flowquery(...) */
      struct json_groupfilter_clause*
      gfclause = calloc(1, sizeof(struct json_groupfilter_clause));
      if (gfclause == NULL)
        errExit("calloc");
      branch->groupfilter_clauseset[j] = gfclause;

      struct json_object* clause = json_object_array_get_idx(gfdnf, j);
      struct json_object* termset = json_object_object_get(clause, "clause");
      gfclause->num_terms = json_object_array_length(termset);

      /* free'd after returning from prepare_flowquery(...) */
      gfclause->termset = calloc(gfclause->num_terms,
                                sizeof(struct json_groupfilter_term*));
      if (gfclause->termset == NULL)
        errExit("calloc");

      for (int i = 0; i < gfclause->num_terms; i++) {

        /* free'd after returning from prepare_flowquery(...) */
        struct json_groupfilter_term*
        gfterm = calloc(1, sizeof(struct json_groupfilter_term));
        if (gfterm == NULL)
          errExit("calloc");
        gfclause->termset[i] = gfterm;

        /* free'd after returning from prepare_flowquery(...) */
        gfterm->off = calloc(1, sizeof(struct json_groupfilter_term_offset));
        if (gfterm->off == NULL)
          errExit("calloc");

        struct json_object* term = json_object_array_get_idx(termset, i);
        struct json_object* term_items = json_object_object_get(term,"term");

        /* free'd before returning from this function */
        struct json_object*
        delta = json_object_object_get(term_items, "delta");
        struct json_object*
        op = json_object_object_get(term_items, "op");
        struct json_object*
        offset = json_object_object_get(term_items, "offset");
        struct json_object*
        fo_name = json_object_object_get(offset, "name");
        struct json_object*
        fo_val = json_object_object_get(offset, "value");
        struct json_object*
        fo_type = json_object_object_get(offset, "datatype");

        gfterm->delta = json_object_get_int(delta);
        gfterm->op = strdup(json_object_get_string(op));
        if (gfterm->op == NULL) errExit("strdup");
        gfterm->off->name = strdup(json_object_get_string(fo_name));
        if (gfterm->off->name == NULL) errExit("strdup");
        gfterm->off->value = json_object_get_int(fo_val);
        gfterm->off->datatype = strdup(json_object_get_string(fo_type));
        if (gfterm->off->datatype == NULL) errExit("strdup");
      }
    }

  /* -----------------------------------------------------------------------*/

#endif

  }

  /* -----------------------------------------------------------------------*/















#ifdef MERGER

  /* -----------------------------------------------------------------------*/
  /*                        parse merger rules                              */
  /* -----------------------------------------------------------------------*/


  struct json_object*
  merger = json_object_object_get(query, "merger");
  struct json_object* mdnf =
  json_object_object_get(merger, "dnf-expr");
  json->num_merger_clauses = json_object_array_length(mdnf);

  /* free'd after returning from prepare_flowquery(...) */
  json->merger_clauseset = calloc(json->num_merger_clauses,
                                  sizeof(struct merger_clause*));
  if (json->merger_clauseset == NULL)
    errExit("calloc");

  for (int i = 0; i < json->num_merger_clauses; i++) {

    /* free'd after returning from prepare_flowquery(...) */
    struct json_merger_clause*
    mclause = calloc(1, sizeof(struct json_merger_clause));
    if (mclause == NULL)
      errExit("calloc");
    json->merger_clauseset[i] = mclause;

    struct json_object* clause = json_object_array_get_idx(mdnf, i);
    struct json_object* termset = json_object_object_get(clause,"clause");
    mclause->num_terms = json_object_array_length(termset);

    /* free'd after returning from prepare_flowquery(...) */
    mclause->termset = calloc(mclause->num_terms,
                              sizeof(struct json_merger_term*));
    if (mclause->termset == NULL)
      errExit("calloc");

    for (int j = 0; j < mclause->num_terms; j++) {

      /* free'd after returning from prepare_flowquery(...) */
      struct json_merger_term*
      mterm = calloc(1, sizeof(struct json_merger_term));
      if (mterm == NULL)
        errExit("calloc");
      mclause->termset[j] = mterm;

      /* free'd after returning from prepare_flowquery(...) */
      mterm->off = calloc(1, sizeof(struct json_merger_term_offset));
      if (mterm->off == NULL)
        errExit("calloc");

      /* free'd after returning from prepare_flowquery(...) */
      mterm->op = calloc(1, sizeof(struct json_merger_term_op));
      if (mterm->op == NULL)
        errExit("calloc");

      struct json_object* term = json_object_array_get_idx(termset, j);
      struct json_object* term_items = json_object_object_get(term, "term");

      /* free'd before returning from this function */
      struct json_object*
      delta = json_object_object_get(term_items, "delta");
      struct json_object*
      op = json_object_object_get(term_items, "op");
      struct json_object*
      offset = json_object_object_get(term_items, "offset");
      struct json_object*
      b1_id = json_object_object_get(term_items, "branch1_id");
      struct json_object*
      b2_id = json_object_object_get(term_items, "branch2_id");

      struct json_object*
      f1_name = json_object_object_get(offset, "f1_name");
      struct json_object*
      f2_name = json_object_object_get(offset, "f2_name");
      struct json_object*
      f1_datatype = json_object_object_get(offset, "f1_datatype");
      struct json_object*
      f2_datatype = json_object_object_get(offset, "f2_datatype");

      struct json_object*
      op_type = json_object_object_get(op, "type");
      struct json_object*
      op_name = json_object_object_get(op, "name");

      mterm->delta = json_object_get_int(delta);
      mterm->b1_id = json_object_get_int(b1_id);
      mterm->b2_id = json_object_get_int(b2_id);

      mterm->op->name = strdup(json_object_get_string(op_name));
      if (mterm->op->name == NULL) errExit("strdup");
      mterm->op->type = strdup(json_object_get_string(op_type));
      if (mterm->op->type == NULL) errExit("strdup");

      mterm->off->f1_name = strdup(json_object_get_string(f1_name));
      if (mterm->off->f1_name == NULL) errExit("strdup");
      mterm->off->f2_name = strdup(json_object_get_string(f2_name));
      if (mterm->off->f2_name == NULL) errExit("strdup");
      mterm->off->f1_datatype = strdup(json_object_get_string(f1_datatype));
      if (mterm->off->f1_datatype == NULL) errExit("strdup");
      mterm->off->f2_datatype = strdup(json_object_get_string(f2_datatype));
      if (mterm->off->f2_datatype == NULL) errExit("strdup");

    }
  }

  /* -----------------------------------------------------------------------*/

#endif










  /* call put(...) only on the root to decremenet the reference count */
  json_object_put(query); query = NULL;

  return json;
}

struct flowquery*
prepare_flowquery(struct ft_data* const trace,
                  const struct json* const json_query) {


  /* -----------------------------------------------------------------------*/
  /*                          allocating fquery                             */
  /* -----------------------------------------------------------------------*/

  /* free'd just before exiting from main(...) */
  struct flowquery* fquery = (struct flowquery *)
                              calloc(1, sizeof(struct flowquery));
  if (fquery == NULL)
    errExit("calloc");

  fquery->num_branches = json_query->num_branches;

  /* free'd just before exiting from main(...) */
  fquery->branchset =  (struct branch**)
                       calloc(fquery->num_branches,
                       sizeof(struct branch*));
  if (fquery->branchset == NULL)
    errExit("calloc");

  /* ----------------------------------------------------------------------- */







  /* -----------------------------------------------------------------------*/
  /*                       assigning branch rules                           */
  /* -----------------------------------------------------------------------*/

  for (int i = 0; i < fquery->num_branches; i++) {

    /* free'd just before exiting from main(...) */
    struct branch* branch = (struct branch*)
                             calloc(1, sizeof(struct branch));
    if (branch == NULL)
      errExit("calloc");
    fquery->branchset[i] = branch;

    struct json_branch* json_branch = json_query->branchset[i];

    branch->branch_id = i;
    branch->data = trace;

#ifdef FILTER

    branch->num_filter_clauses = json_branch->num_filter_clauses;

    /* filter rules are used in grouper aggregations */
    /* therefore, free'd after returning from grouper(...) in branch.c */
    struct filter_clause** fclauseset = (struct filter_clause**)
                                         calloc(branch->num_filter_clauses,
                                                sizeof(struct filter_clause*));
    if (fclauseset == NULL)
      errExit("calloc");

    for (int k = 0; k < branch->num_filter_clauses; k++) {

      /* free'd after returning from grouper(...) in branch.c */
      struct filter_clause* fclause = calloc(1, sizeof(struct filter_clause));
      if (fclause == NULL)
        errExit("calloc");

      struct json_filter_clause* fclause_json =
      json_branch->filter_clauseset[k];
      fclause->num_terms = fclause_json->num_terms;

      /* free'd after returning from grouper(...) in branch.c */
      struct filter_term** termset = (struct filter_term**)
                                      calloc(fclause->num_terms,
                                             sizeof(struct filter_term*));
      if(termset == NULL)
        errExit("calloc");

      for (int j = 0; j < fclause->num_terms; j++) {

        /* free'd after returning from grouper(...) in branch.c */
        struct filter_term* term = calloc(1, sizeof(struct filter_term));
        if (term == NULL)
          errExit("calloc");

        /* free'd after returning from grouper(...) in branch.c */
        struct filter_op* op = calloc(1, sizeof(struct filter_op));
        if (op == NULL)
          errExit("calloc");
        else
          term->op = op; op = NULL;

        struct json_filter_term* term_json = fclause_json->termset[j];

        size_t offset = get_offset(term_json->off->name, &trace->offsets);
        if (offset == -1)
          errExit("get_offset(term_json->off->name) returned -1");

        uint64_t op_enum = get_enum(term_json->op);
        if (op_enum == -1)
          errExit("get_enum(term_json->op) returned -1");

        uint64_t type_enum = get_enum(term_json->off->datatype);
        if (type_enum == -1)
          errExit("get_enum(term_json->off->datatype) returned -1");

        term->field_offset          =         offset;
        term->value                 =         term_json->off->value;
        term->delta                 =         term_json->delta;
        term->op->op                =         op_enum;
        term->op->field_type        =         type_enum;
        term->func                  =         NULL;

        termset[j] = term; term = NULL; term_json = NULL;
      }

      fclause->termset = termset; termset = NULL;
      fclauseset[k] = fclause; fclause = NULL;
    }
    branch->filter_clauseset = fclauseset; fclauseset = NULL;

#endif

#ifdef GROUPER

    branch->num_grouper_clauses = json_branch->num_grouper_clauses;

    /* filter rules are used in grouper aggregations */
    /* therefore, free'd after returning from grouper(...) in branch.c */
    struct grouper_clause** gclauseset = (struct grouper_clause**)
                                         calloc(branch->num_grouper_clauses,
                                                sizeof(struct grouper_clause*));
    if (gclauseset == NULL)
      errExit("calloc");

    for (int k = 0; k < branch->num_grouper_clauses; k++) {

      /* free'd after returning from grouper(...) in branch.c */
      struct grouper_clause* gclause = calloc(1, sizeof(struct grouper_clause));
      if (gclause == NULL)
        errExit("calloc");

      struct json_grouper_clause* gclause_json =
      json_branch->grouper_clauseset[k];
      gclause->num_terms = gclause_json->num_terms;

      /* free'd after returning from grouper(...) in branch.c */
      struct grouper_term** termset = (struct grouper_term**)
                                       calloc(gclause->num_terms,
                                              sizeof(struct grouper_term*));
      if(termset == NULL)
        errExit("calloc");

      for (int j = 0; j < gclause->num_terms; j++) {

        /* free'd after returning from grouper(...) in branch.c */
        struct grouper_term* term = calloc(1, sizeof(struct grouper_term));
        if (term == NULL)
          errExit("calloc");

        /* free'd after returning from grouper(...) in branch.c */
        struct grouper_op* op = calloc(1, sizeof(struct grouper_op));
        if (op == NULL)
          errExit("calloc");
        else
          term->op = op; op = NULL;

        struct json_grouper_term* term_json = gclause_json->termset[j];

        size_t f1_offset = get_offset(term_json->off->f1_name,
                                      &trace->offsets);
        if (f1_offset == -1)
          errExit("get_offset(term_json->off->f1_name) returned -1");

        size_t f2_offset = get_offset(term_json->off->f2_name,
                                      &trace->offsets);
        if (f2_offset == -1)
          errExit("get_offset(term_json->off->f2_name) returned -1");

        uint64_t op_name_enum = get_enum(term_json->op->name);
        if (op_name_enum == -1)
          errExit("get_enum(term_json->op->name) returned -1");

        uint64_t f1_type_enum = get_enum(term_json->off->f1_datatype);
        if (f1_type_enum == -1)
          errExit("get_enum(term_json->off->f1_datatype) returned -1");

        uint64_t f2_type_enum = get_enum(term_json->off->f2_datatype);
        if (f1_type_enum == -1)
          errExit("get_enum(term_json->off->f2_datatype) returned -1");

        uint64_t op_type_enum = get_enum(term_json->op->type);
        if (op_type_enum == -1)
          errExit("get_enum(term_json->op->type) returned -1");

        term->field_offset1         =        f1_offset;
        term->field_offset2         =        f2_offset;
        term->delta                 =        term_json->delta;
        term->op->op                =        op_name_enum;
        term->op->field1_type       =        f1_type_enum;
        term->op->field2_type       =        f2_type_enum;
        term->op->optype            =        op_type_enum;
        term->func                  =        NULL;

        termset[j] = term; term = NULL; term_json = NULL;
      }

      gclause->termset = termset; termset = NULL;
      gclauseset[k] = gclause; gclause = NULL;
    }
    branch->grouper_clauseset = gclauseset; gclauseset = NULL;

#endif

#ifdef GROUPERAGGREGATIONS

    branch->num_aggr_clause_terms = json_branch->num_aggr_clause_terms;

    /* free'd after returning from grouper(...) */
    struct aggr_term** termset = (struct aggr_term**)
                                   calloc(branch->num_aggr_clause_terms,
                                          sizeof(struct aggr_term*));
    if (termset == NULL)
      errExit("calloc");
    for (int j = 0; j < branch->num_aggr_clause_terms; j++) {

      /* free'd after returning from grouper(...) */
      struct aggr_term* term = calloc(1, sizeof(struct aggr_term));
      if (term == NULL)
        errExit("calloc");

      /* free'd after returning from grouper(...) */
      struct aggr_op* op = calloc(1, sizeof(struct aggr_op));
      if (op == NULL)
        errExit("calloc");
      else
        term->op = op; op = NULL;

      struct json_aggr_term* term_json = json_branch->aggr_clause_termset[j];
      size_t offset = get_offset(term_json->off->name,
                                 &trace->offsets);
      if (offset == -1)
        errExit("get_offset(term_json->off->name) returned -1");

      uint64_t op_enum = get_enum(term_json->op);
      if (op_enum == -1)
        errExit("get_enum(term_json->op) returned -1");

      uint64_t type_enum = get_enum(term_json->off->datatype);
      if (type_enum == -1)
        errExit("get_enum(term_json->off->datatype) returned -1");

      term->field_offset        =         offset;
      term->op->op              =         op_enum;
      term->op->field_type      =         type_enum;
      term->func                =         NULL;

      termset[j] = term; term = NULL;
    }
    branch->aggr_clause_termset = termset; termset = NULL;

#endif

#ifdef GROUPFILTER

    branch->num_groupfilter_clauses = json_branch->num_groupfilter_clauses;

    /* filter rules are used in groupfilter aggregations */
    /* therefore, free'd after returning from groupfilter(...) in branch.c */
    struct groupfilter_clause** gfclauseset =
      (struct groupfilter_clause**) calloc(branch->num_groupfilter_clauses,
                                           sizeof(struct groupfilter_clause*));
    if (gfclauseset == NULL)
      errExit("calloc");

    for (int k = 0; k < branch->num_groupfilter_clauses; k++) {

      /* free'd after returning from groupfilter(...) in branch.c */
      struct groupfilter_clause* gfclause =
      calloc(1, sizeof(struct groupfilter_clause));
      if (gfclause == NULL)
        errExit("calloc");

      struct json_groupfilter_clause* gfclause_json =
      json_branch->groupfilter_clauseset[k];
      gfclause->num_terms = gfclause_json->num_terms;

      /* free'd after returning from groupfilter(...) in branch.c */
      struct groupfilter_term** termset =
        (struct groupfilter_term**) calloc(gfclause->num_terms,
                                           sizeof(struct groupfilter_term*));
      if(termset == NULL)
        errExit("calloc");

      for (int j = 0; j < gfclause->num_terms; j++) {

        /* free'd after returning from groupfilter(...) in branch.c */
        struct groupfilter_term* term =
        calloc(1, sizeof(struct groupfilter_term));
        if (term == NULL)
          errExit("calloc");

        /* free'd after returning from groupfilter(...) in branch.c */
        struct groupfilter_op* op = calloc(1, sizeof(struct groupfilter_op));
        if (op == NULL)
          errExit("calloc");
        else
          term->op = op; op = NULL;

        struct json_groupfilter_term* term_json = gfclause_json->termset[j];

        size_t offset = get_offset(term_json->off->name, &trace->offsets);
        if (offset == -1)
          errExit("get_offset(term_json->off->name) returned -1");

        uint64_t op_enum = get_enum(term_json->op);
        if (op_enum == -1)
          errExit("get_enum(term_json->op) returned -1");

        uint64_t type_enum = get_enum(term_json->off->datatype);
        if (type_enum == -1)
          errExit("get_enum(term_json->off->datatype) returned -1");

        term->field                =         offset;
        term->value                =         term_json->off->value;
        term->delta                =         term_json->delta;
        term->op->op               =         op_enum;
        term->op->field_type       =         type_enum;
        term->func                 =         NULL;

        termset[j] = term; term = NULL; term_json = NULL;
      }

      gfclause->termset = termset; termset = NULL;
      gfclauseset[k] = gfclause; gfclause = NULL;
    }
    branch->groupfilter_clauseset = gfclauseset; gfclauseset = NULL;

#endif

  }


  /* ----------------------------------------------------------------------- */





#ifdef MERGER

  /* -----------------------------------------------------------------------*/
  /*                       assigning merger rules                           */
  /* -----------------------------------------------------------------------*/

  fquery->num_merger_clauses  = json_query->num_merger_clauses;

  /* free'd after returning from merger(...) */
  struct merger_clause** mclauseset = (struct merger_clause**)
                                      calloc(fquery->num_merger_clauses,
                                             sizeof(struct merger_clause*));
  if (mclauseset == NULL)
    errExit("calloc");

  for (int k = 0; k < fquery->num_merger_clauses; k++) {

    /* free'd after returning from merger(...) in branch.c */
    struct merger_clause* mclause = calloc(1, sizeof(struct merger_clause));
    if (mclause == NULL)
      errExit("calloc");

    struct json_merger_clause* mclause_json = json_query->merger_clauseset[k];
    mclause->num_terms = mclause_json->num_terms;

    /* free'd after returning from merger(...) in branch.c */
    struct merger_term** termset = (struct merger_term**)
                                   calloc(mclause->num_terms,
                                          sizeof(struct merger_term*));
    if(termset == NULL)
      errExit("calloc");

    for (int j = 0; j < mclause->num_terms; j++) {

      /* free'd after returning from merger(...) in branch.c */
      struct merger_term* term = calloc(1, sizeof(struct merger_term));
      if (term == NULL)
        errExit("calloc");

      /* free'd after returning from merger(...) in branch.c */
      struct merger_op* op = calloc(1, sizeof(struct merger_op));
      if (op == NULL)
        errExit("calloc");
      else
        term->op = op; op = NULL;

      struct json_merger_term* term_json = mclause_json->termset[j];

      size_t f1_offset = get_offset(term_json->off->f1_name, &trace->offsets);
      if (f1_offset == -1)
        errExit("get_offset(grule_json->off->f1_name) returned -1");

      size_t f2_offset = get_offset(term_json->off->f2_name, &trace->offsets);
      if (f2_offset == -1)
        errExit("get_offset(grule_json->off->f2_name) returned -1");

      uint64_t op_name = get_enum(term_json->op->name);
      if (op_name == -1)
        errExit("get_enum(term_json->op->name) returned -1");

      uint64_t op_type = get_enum(term_json->op->type);
      if (op_type == -1)
        errExit("get_enum(term_json->op->type) returned -1");

      uint64_t f1_type = get_enum(term_json->off->f1_datatype);
      if (f1_type == -1)
        errExit("get_enum(term_json->off->f1_datatype) returned -1");

      uint64_t f2_type = get_enum(term_json->off->f2_datatype);
      if (f2_type == -1)
        errExit("get_enum(term_json->off->f2_datatype) returned -1");

      term->field1               =         f1_offset;
      term->field2               =         f2_offset;
      term->op->op               =         op_name;
      term->op->field1_type      =         f1_type;
      term->op->field2_type      =         f2_type;
      term->op->optype           =         op_type;
      term->branch1              =         fquery->branchset[term_json->b1_id];
      term->branch2              =         fquery->branchset[term_json->b2_id];
      term->delta                =         0;
      term->func                 =         NULL;

      termset[j] = term; term = NULL;
    }

    mclause->termset = termset; termset = NULL;
    mclauseset[k] = mclause; mclause = NULL;
  }
  fquery->merger_clauseset = mclauseset; mclauseset = NULL;

  /* ----------------------------------------------------------------------- */

#endif







  return fquery;
}

struct ft_data*
read_trace(
           const struct parameters_data* const param_data,
           struct flowquery* fquery
          ) {

  struct ft_data* trace = ft_read(param_data->trace, fquery);
  if (trace == NULL)
    errExit("ft_read(...) returned NULL");

  if (close(param_data->trace_fsock) == -1)
    errExit("close");

  return trace;
}

pthread_t*
run_branch_async(const struct flowquery* const fquery){

  /* allocate space for a dedicated thread for each branch */
  /* free'd after returning from this function */
  pthread_t* threadset = (pthread_t *)calloc(fquery->num_branches,
                                             sizeof(pthread_t));
  if (threadset == NULL)
    errExit("calloc");

  /* free'd before returning from this function */
  pthread_attr_t* thread_attrset = (pthread_attr_t *)
                                    calloc(fquery->num_branches,
                                           sizeof(pthread_attr_t));
  if (thread_attrset == NULL)
    errExit("calloc");

  for (int i = 0, ret = 0; i < fquery->num_branches; i++) {

    pthread_t* thread = &threadset[i];
    pthread_attr_t* thread_attr = &thread_attrset[i];
    struct branch* branch = fquery->branchset[i];

    /* initialize each thread attributes */
    ret = pthread_attr_init(thread_attr);
    if (ret != 0)
      errExit("pthread_attr_init");

    /* start each thread for a dedicated branch */
    ret = pthread_create(thread,
                         thread_attr,
                         &branch_start,
                         (void *)(branch));
    if (ret != 0)
      errExit("pthread_create");


    /* destroy the thread attributes once the thread is created */
    ret = pthread_attr_destroy(thread_attr);
    if (ret != 0)
      errExit("pthread_attr_destroy");
  }

  free(thread_attrset);
  return threadset;
}

int
main(int argc, char **argv) {



  /* -----------------------------------------------------------------------*/
  /*                  parsing the command line arguments                    */
  /* -----------------------------------------------------------------------*/

  struct parameters* param = parse_cmdline_args(argc, argv);
  if (param == NULL)
    errExit("parse_cmdline_args(...) returned NULL");

  /* ----------------------------------------------------------------------- */







  /* -----------------------------------------------------------------------*/
  /*                         read the input query                           */
  /* -----------------------------------------------------------------------*/

  struct parameters_data* param_data = open_trace_read_query(param);
  if (param_data == NULL)
    errExit("read_param_data(...) returned NULL");
  else{
    free(param); param = NULL;
  }

  /* ----------------------------------------------------------------------- */







  /* -----------------------------------------------------------------------*/
  /*                  parse the query json into struct                      */
  /* -----------------------------------------------------------------------*/

  struct json* json_query = parse_json_query(param_data->query_mmap);
  if (json_query == NULL)
    errExit("parse_json_query(...) returned NULL");
  else {
    /* free param_data->query_mmap */
    if (munmap(param_data->query_mmap,
               param_data->query_mmap_stat->st_size) == -1)
      errExit("munmap");
    param_data->query_mmap = NULL;
    free(param_data->query_mmap_stat); param_data->query_mmap_stat = NULL;
  }

 /* ----------------------------------------------------------------------- */







  /* -----------------------------------------------------------------------*/
  /*                           prepare flowquery                            */
  /* -----------------------------------------------------------------------*/

  struct flowquery* fquery = prepare_flowquery(param_data->trace, json_query);
  if (fquery == NULL)
    errExit("prepare_flowquery(...) returned NULL");
  else {

    for (int i = 0; i < json_query->num_branches; i++) {

      struct json_branch* json_branch = json_query->branchset[i];

      /* deallocate the filter json query buffers */
      for (int k = 0; k < json_branch->num_filter_clauses; k++) {
        struct json_filter_clause* fclause = json_branch->filter_clauseset[k];
        for (int j = 0; j < fclause->num_terms; j++) {
          struct json_filter_term* term = fclause->termset[j];
          free(term->off->datatype); term->off->datatype = NULL;
          free(term->off->name); term->off->name = NULL;
          free(term->off); term->off = NULL;
          free(term->op); term->op = NULL;
          free(term); term = NULL;
        }
        free(fclause->termset); fclause->termset = NULL;
        free(fclause); fclause = NULL; json_branch->filter_clauseset[k] = NULL;
      }
      free(json_branch->filter_clauseset);
      json_branch->filter_clauseset = NULL;

      /* deallocate the grouper json query buffers */
      for (int k = 0; k < json_branch->num_grouper_clauses; k++) {
        struct json_grouper_clause* gclause = json_branch->grouper_clauseset[k];
        for (int j = 0; j < gclause->num_terms; j++) {
          struct json_grouper_term* term = gclause->termset[j];
          free(term->off->f1_datatype); term->off->f1_datatype = NULL;
          free(term->off->f2_datatype); term->off->f2_datatype = NULL;
          free(term->off->f1_name); term->off->f1_name = NULL;
          free(term->off->f2_name); term->off->f2_name = NULL;
          free(term->off); term->off = NULL;
          free(term->op->name); term->op->name = NULL;
          free(term->op->type); term->op->type = NULL;
          free(term->op); term->op = NULL;
          free(term); term = NULL;
        }
        free(gclause->termset); gclause->termset = NULL;
        free(gclause); gclause = NULL; json_branch->grouper_clauseset[k] = NULL;
      }
      free(json_branch->grouper_clauseset);
      json_branch->grouper_clauseset = NULL;

      /* deallocate the aggr json query buffers */
      for (int j = 0; j < json_branch->num_aggr_clause_terms; j++) {
        struct json_aggr_term* term = json_branch->aggr_clause_termset[j];
        free(term->off->datatype); term->off->datatype = NULL;
        free(term->off->name); term->off->name = NULL;
        free(term->off); term->off = NULL;
        free(term->op); term->op = NULL;
        free(term); term = NULL;
      }
      free(json_branch->aggr_clause_termset);
      json_branch->aggr_clause_termset = NULL;

      /* deallocate the group filter json query buffers */
      for (int k = 0; k < json_branch->num_groupfilter_clauses; k++) {
        struct json_groupfilter_clause* gfclause =
         json_branch->groupfilter_clauseset[k];
        for (int j = 0; j < gfclause->num_terms; j++) {
          struct json_groupfilter_term* term = gfclause->termset[j];
          free(term->off->datatype); term->off->datatype = NULL;
          free(term->off->name); term->off->name = NULL;
          free(term->off); term->off = NULL;
          free(term->op); term->op = NULL;
          free(term); term = NULL;
        }
        free(gfclause->termset); gfclause->termset = NULL;
        free(gfclause); gfclause = NULL;
        json_branch->groupfilter_clauseset[k] = NULL;
      }
      free(json_branch->groupfilter_clauseset);
      json_branch->groupfilter_clauseset = NULL;
      free(json_branch); json_branch = NULL; json_query->branchset[i] = NULL;
    }

    /* deallocate the merger json query buffers */
    for (int k = 0; k < json_query->num_merger_clauses; k++) {
      struct json_merger_clause* mclause = json_query->merger_clauseset[k];
      for (int j = 0; j < mclause->num_terms; j++) {
        struct json_merger_term* term = mclause->termset[j];
        free(term->off->f1_datatype); term->off->f1_datatype = NULL;
        free(term->off->f2_datatype); term->off->f2_datatype = NULL;
        free(term->off->f1_name); term->off->f1_name = NULL;
        free(term->off->f2_name); term->off->f2_name = NULL;
        free(term->off); term->off = NULL;
        free(term->op->name); term->op->name = NULL;
        free(term->op->type); term->op->type = NULL;
        free(term->op); term->op = NULL;
        free(term); term = NULL;
      }
      free(mclause->termset); mclause->termset = NULL;
      free(mclause); mclause = NULL; json_query->merger_clauseset[k] = NULL;
    }
    free(json_query->merger_clauseset); json_query->merger_clauseset = NULL;
    free(json_query->branchset); json_query->branchset = NULL;
    free(json_query); json_query = NULL;
  }

 /* -----------------------------------------------------------------------*/








  /* -----------------------------------------------------------------------*/
  /*          read the input trace and prepare filtered recordsets          */
  /* -----------------------------------------------------------------------*/

  param_data->trace = read_trace(param_data, fquery);
  if(param_data->trace == NULL)
    errExit("read_trace(...) returned NULL");

  /* ----------------------------------------------------------------------- */










  /* -----------------------------------------------------------------------*/
  /*                                branch                                  */
  /*               splitter → filter → grouper → group-filter               */
  /* -----------------------------------------------------------------------*/

  pthread_t* threadset = run_branch_async(fquery);
  if (threadset == NULL)
    errExit("run_branch_async(...) returned NULL");
  else {

    /* - wait for each thread to complete its branch
     * - save and print the number of filtered groups on completion
     * - save the filtered groups on completion
     */
    for (int i = 0, ret = 0; i < fquery->num_branches; i++) {
      void* res = NULL;
      pthread_t* thread = &threadset[i];
      ret = pthread_join(*thread, &res);
      if (ret != 0)
        errExit("pthread_join");
      else if (res != EXIT_SUCCESS)
        errExit("branch (%d) prematurely exited", i);
    }
    free(threadset);

    /* print the filtered records if verbose mode is set */
    if (verbose_v) {

      echo_branch(
                  fquery->num_branches,
                  fquery->branchset,
                  param_data->trace
                  );
    }
  }

  /* -----------------------------------------------------------------------*/








#ifdef GROUPER

  /* -----------------------------------------------------------------------*/
  /*                              free memory                               */
  /* -----------------------------------------------------------------------*/

  if (verbose_v) {
    for (int i = 0; i < fquery->num_branches; i++) {
      struct branch* branch = fquery->branchset[i];

      /* free grouper_result */
      if (verbose_vv) {

        /* free sorted records */
        if (branch->grouper_result->sorted_recordset != NULL) {
          for (int j = 0; j < branch->filter_result->num_filtered_records; j++)
            branch->grouper_result->sorted_recordset[j] = NULL;
          free(branch->grouper_result->sorted_recordset);
          branch->grouper_result->sorted_recordset = NULL;
        }

        /* free unique records */
        if (branch->grouper_result->unique_recordset != NULL) {
          for (int j = 0; j < branch->grouper_result->num_unique_records; j++)
            branch->grouper_result->unique_recordset[j] = NULL;
          free(branch->grouper_result->unique_recordset);
          branch->grouper_result->unique_recordset = NULL;
        }
      }
    }
  }

  /* -----------------------------------------------------------------------*/

#endif








#ifdef MERGER

  /* -----------------------------------------------------------------------*/
  /*                                 merger                                 */
  /* -----------------------------------------------------------------------*/

  fquery->merger_result = merger(
                                 fquery->num_merger_clauses,
                                 fquery->merger_clauseset,

                                 fquery->num_branches,
                                 fquery->branchset
                                );

  if (fquery->merger_result == NULL)
    errExit("merger(...) returned NULL");
  else {

    /* free merger rules */
    for (int j = 0; j < fquery->num_merger_clauses; j++) {

      struct merger_clause* mclause = fquery->merger_clauseset[j];

      for (int i = 0; i < mclause->num_terms; i++) {
        struct merger_term* mterm = mclause->termset[i];
        free(mterm->op); mterm->op = NULL;
        free(mterm); mterm = NULL; mclause->termset[i] = NULL;
      }
      free(mclause->termset); mclause->termset = NULL;
      free(mclause); mclause = NULL; fquery->merger_clauseset[j] = NULL;
    }
    free(fquery->merger_clauseset); fquery->merger_clauseset = NULL;

    /* echo merger results, if verbose mode is SET */
    if (verbose_v)
      echo_merger(
                  fquery->num_branches,
                  fquery->branchset,

                  fquery->merger_result,
                  param_data->trace
                 );

    /* free grouper and groupfilter parts */
    for (int i = 0; i < fquery->num_branches; i++) {
      struct branch* branch = fquery->branchset[i];

      /* free grouper aggregations */
      for (int j = 0; j < branch->grouper_result->num_groups; j++) {
        struct group* group = branch->grouper_result->groupset[j];

#ifdef GROUPERAGGREGATIONS

        /* free group aggregations */
        for (int x = 0; x < branch->num_aggr_clause_terms; x++) {
          struct aggr* aggr = group->aggr_result->aggrset[x];
          free(aggr->values); aggr->values = NULL;
          free(aggr); aggr = NULL; group->aggr_result->aggrset[x] = NULL;
        }
        free(group->aggr_result->aggrset);
        group->aggr_result->aggrset = NULL;

        free(group->aggr_result->aggr_record);
        group->aggr_result->aggr_record = NULL;

        free(group->aggr_result);
        group->aggr_result = NULL;
#endif
      }

      /* free groupfilter result */
      for (int j = 0; j < branch->gfilter_result->num_filtered_groups; j++) {
        /* all the groups were already free'd by grouper just above */
        /* unlink the pointers */
        branch->gfilter_result->filtered_groupset[j] = NULL;
      }
      free(branch->gfilter_result->filtered_groupset);
      branch->gfilter_result->filtered_groupset = NULL;
      free(branch->gfilter_result);
      branch->gfilter_result = NULL;
    }
  }

  /* -----------------------------------------------------------------------*/

#endif










#ifdef UNGROUPER

  /* -----------------------------------------------------------------------*/
  /*                                ungrouper                               */
  /* -----------------------------------------------------------------------*/

  fquery->ungrouper_result = ungrouper(
                                       fquery->num_branches,
                                       fquery->merger_result
                                      );
  if (fquery->ungrouper_result == NULL)
    errExit("ungrouper(...) returned NULL");
  else {

    /* echo ungrouper results */
    echo_results(
                 fquery->ungrouper_result,
                 param_data->trace
                );
  }

  /* -----------------------------------------------------------------------*/

#endif










  /* -----------------------------------------------------------------------*/
  /*                              free memory                               */
  /* -----------------------------------------------------------------------*/

#ifdef GROUPER
  /* free grouper leftovers */
  for (int i = 0; i < fquery->num_branches; i++) {
    struct branch* branch = fquery->branchset[i];
    for (int j = 0; j < branch->grouper_result->num_groups; j++) {
      struct group* group = branch->grouper_result->groupset[j];
      for (int k = 0; k < group->num_members; k++)
        group->members[k] = NULL;
      free(group->members); group->members = NULL;
      free(group); group = NULL; branch->grouper_result->groupset[j] = NULL;
    }
    free(branch->grouper_result->groupset);
    branch->grouper_result->groupset = NULL;
    free(branch->grouper_result); branch->grouper_result = NULL;
  }
#endif

#ifdef MERGER
  /* free merger results */
  for (int i = 0; i < fquery->merger_result->num_group_tuples; i++) {
    struct group** matched_tuple = fquery->merger_result->group_tuples[i];
    for (int j = 0; j < fquery->num_branches; j++) {
      /* unlink the groups */
      matched_tuple[j] = NULL;
    }
    free(matched_tuple);
    matched_tuple = NULL; fquery->merger_result->group_tuples[i] = NULL;
  }
  free(fquery->merger_result->group_tuples);
  fquery->merger_result->group_tuples = NULL;
  free(fquery->merger_result); fquery->merger_result = NULL;
#endif

#ifdef UNGROUPER
  /* free ungrouper results */
  for (int j = 0; j < fquery->ungrouper_result->num_streams; j++) {
    struct stream* stream = fquery->ungrouper_result->streamset[j];
    for (int i = 0; i < stream->num_records; i++){
      /* unlink the record, */
      /* all filtered records are free'd next at once */
      stream->recordset[i] = NULL;
    }
    free(stream->recordset); stream->recordset = NULL;
    free(stream); stream = NULL;
  }
  free(fquery->ungrouper_result->streamset);
  fquery->ungrouper_result->streamset = NULL;
  free(fquery->ungrouper_result); fquery->ungrouper_result = NULL;
#endif

#ifdef FILTER
  /* free filter_result */
  for (int i = 0; i < fquery->num_branches; i++) {
    struct branch* branch = fquery->branchset[i];
    for (int j = 0; j < branch->filter_result->num_filtered_records; j++) {
      /* unlink the records */
      branch->filter_result->filtered_recordset[j] = NULL;
    }

    free(branch->filter_result->filtered_recordset);
    branch->filter_result->filtered_recordset = NULL;

    free(branch->filter_result);
    branch->filter_result = NULL;
  }
#endif

  /* free flowquery */
  for (int i = 0; i < fquery->num_branches; i++) {
    struct branch* branch = fquery->branchset[i];
    free(branch); branch = NULL; fquery->branchset[i] = NULL;
  }
  free(fquery->branchset); fquery->branchset = NULL;
  free(fquery); fquery = NULL;

  /* free param_data */
  ft_close(param_data->trace); param_data->trace = NULL;
  free(param_data); param_data = NULL;

  /* -----------------------------------------------------------------------*/


  exit(EXIT_SUCCESS);
}