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
#include "flowy.h"

struct parameters*
parse_cmdline_args(int argc, char**argv) {

  int                                 opt;
  char*                               shortopts = "v:d";
  static struct option                longopts[] = {
    { "debug",      no_argument,        NULL,           'd' },
    { "verbose",    required_argument,  NULL,           'v' },    
    {  NULL,        0,                  NULL,            0  }
  };
  enum verbosity_levels               verbose_level;
  
  /* free'd after calling read_param_data(...) */
  struct parameters* param = calloc(1, sizeof(struct parameters));
  if (param == NULL)
    errExit("calloc");

  while ((opt = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
    switch (opt) {
      case 'd': debug = TRUE; verbose_level = HIGH;
      case 'v': if (optarg)
        verbose_level = atoi(optarg); 
        switch (verbose_level) {
          case HIGH: verbose_vvv = TRUE;                       
          case MEDIUM: verbose_vv = TRUE;                       
          case LOW: verbose_v = TRUE; break;
          default: errExit("valid verbosity levels: (1-3)");                      
        }
        break;        
      case ':': usageError(argv[0], "Missing argument", optopt);
      default: exit(EXIT_FAILURE);
    }
  }
  
  if (argc != optind + 2)
    usageErr("%s $TRACE $QUERY\n", argv[0], argv[0]);
  else {
    param->trace_filename = argv[optind];
    param->query_filename = argv[optind+1];
  }  
  return param;
}

struct parameters_data*
read_param_data(struct parameters* param) {
  
  int                                 fsock;
  
  /* param_data->query_mmap is free'd after calling parse_json_query(...)
   * param_data->query_mmap_stat is free'd after freeing param_data->query_mmap
   * TODO: param_data->trace: when free'd?
   * TODO: param_data: when free'd?
   */ 
  struct parameters_data* param_data = calloc(1, 
                                              sizeof(struct parameters_data));
  if (param_data == NULL)
    errExit("calloc");
  
  fsock = open(param->trace_filename, O_RDONLY);
  if (fsock == -1)
    errExit("open"); 
  param_data->trace = ft_open(fsock);
  if (close(fsock) == -1)
    errExit("close");

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

  /* param_data->query_mmap_stat is free'd after freeing 
   * param_data->query_mmap  
   */
  param_data->query_mmap = mmap(NULL, 
                                param_data->query_mmap_stat->st_size, 
                                PROT_READ, 
                                MAP_PRIVATE, 
                                fsock, 0); 
  if (param_data->query_mmap == MAP_FAILED)
    errExit("mmap");
  if (close(fsock) == -1)
    errExit("close"); 
  
  return param_data;
}

struct json*
parse_json_query(char* query_mmap) {
  
  /* the json_objects are free'd before returning from this function */
  struct json_object*                 query;
  struct json_object*                 filter_offset;
  
  
  /* TODO: when free'd? */
  struct json* json = calloc(1, sizeof(struct json));
  if (json == NULL)
    errExit("calloc");
  json->num_frules = 1;
  
  /* TODO: when free'd? */
  json->fruleset = calloc(json->num_frules, 
                          sizeof(struct json_filter_rule*));
  if (json->fruleset == NULL)
    errExit("calloc");

  /* TODO: when free'd? */
  for (int i = 0; i < json->num_frules; i++) {
    json->fruleset[i] = calloc(1, sizeof(struct json_filter_rule));
    if (json->fruleset[i] == NULL)
      errExit("calloc");
    
    /* TODO: when free'd? */
    json->fruleset[i]->off = 
    calloc(1, sizeof(struct json_filter_rule_offset));
    if (json->fruleset[i]->off == NULL)
      errExit("calloc");
    
    query = json_tokener_parse(query_mmap);
    json->fruleset[i]->delta = 
    json_object_get_int(json_object_object_get(query, "delta"));  
    json->fruleset[i]->op = 
    json_object_get_string(json_object_object_get(query, "op"));  
    filter_offset = 
    json_object_object_get(query, "offset");
    json->fruleset[i]->off->name = 
    json_object_get_string(json_object_object_get(filter_offset, "name"));  
    json->fruleset[i]->off->value = 
    json_object_get_int(json_object_object_get(filter_offset, "value"));
    json->fruleset[i]->off->datatype = 
    json_object_get_string(json_object_object_get(filter_offset, "datatype"));
    
    /* free the json objects */
    json_object_object_del(filter_offset, "offset"); 
    json_object_object_del(query, "");    
  } 
  
  return json;
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
  /*                  reading the input trace and query                     */
  /* -----------------------------------------------------------------------*/
  
  struct parameters_data* param_data = read_param_data(param);
  if (param_data == NULL)
    errExit("read_param_data(...) returned NULL");
  else
    free(param);
  
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
    free(param_data->query_mmap_stat);
  }
  
 /* ----------------------------------------------------------------------- */  
  
  
   
  
  
 
  
  
  /* -----------------------------------------------------------------------*/  
  /*                      allocating fquery and binfos                      */
  /* -----------------------------------------------------------------------*/
  
  /* TODO: when free'd? */
  struct flowquery* fquery = (struct flowquery *)
                             calloc(1, sizeof(struct flowquery));
  if (fquery == NULL) 
    errExit("calloc");
  fquery->num_branches = NUM_BRANCHES;
  
  /* TODO: when free'd? */
  fquery->branches = (struct branch_info *)
                      calloc(fquery->num_branches, 
                      sizeof(struct branch_info));
  if (fquery->branches == NULL)
    errExit("calloc");
  
 /* ----------------------------------------------------------------------- */


  

  
  
  
    
  /* -----------------------------------------------------------------------*/  
  /*                           pipeline rules                               */
  /* -----------------------------------------------------------------------*/  
 
  fquery->branches[0].num_filter_rules = NUM_FILTER_RULES_BRANCH1;
  fquery->branches[1].num_filter_rules = NUM_FILTER_RULES_BRANCH2;
  
  fquery->branches[0].num_group_modules = NUM_GROUPER_RULES_BRANCH1;
  fquery->branches[1].num_group_modules = NUM_GROUPER_RULES_BRANCH2;  
  
  fquery->branches[0].num_aggr = NUM_GROUPER_AGGREGATION_RULES_BRANCH1;
  fquery->branches[1].num_aggr = NUM_GROUPER_AGGREGATION_RULES_BRANCH2;  
  
  fquery->branches[0].num_gfilter_rules = NUM_GROUP_FILTER_RULES_BRANCH1;
  fquery->branches[1].num_gfilter_rules = NUM_GROUP_FILTER_RULES_BRANCH2;
  
  fquery->num_merger_rules = NUM_MERGER_RULES;

  
  /* rules for each branch */
  for (int i = 0; i < fquery->num_branches; i++) {
    
    fquery->branches[i].branch_id = i;
    fquery->branches[i].data = param_data->trace;
    
    
    struct filter_rule* frule = calloc(fquery->branches[i].num_filter_rules, 
                                       sizeof(struct filter_rule));    
    for (int j = 0; j < fquery->branches[i].num_filter_rules; j++) {
      /* assumes that there are 2 branches */
      switch (i) {
        case 0:
          frule[j].field_offset     =         param_data->trace->offsets.dstport;
          break;
        case 1:          
          frule[j].field_offset     =         param_data->trace->offsets.srcport;
          break;
      }
      frule[j].value                =         json_query->fruleset[0]->off->value;
      frule[j].delta                =         json_query->fruleset[0]->delta;
      frule[j].op                   =         RULE_EQ | RULE_S1_16;
      frule[j].func                 =         NULL;      
    }    
    fquery->branches[i].filter_rules = frule;
    
    
    
    struct grouper_rule* grule = calloc(fquery->branches[i].num_group_modules, 
                                        sizeof(struct grouper_rule));
    for (int j = 0; j < fquery->branches[i].num_group_modules; j++) {
      switch (j) {
        case 0:
          grule[j].field_offset1     =        param_data->trace->offsets.srcaddr;      
          grule[j].field_offset2     =        param_data->trace->offsets.srcaddr;          
          break;
        case 1:
          grule[j].field_offset1     =        param_data->trace->offsets.dstaddr;      
          grule[j].field_offset2     =        param_data->trace->offsets.dstaddr;         
          break;
      }
      
      grule[j].delta                 =         0;
      grule[j].op                    =         RULE_EQ | RULE_S1_32 | 
                                               RULE_S2_32 | RULE_NO;
      grule[j].func                  =         NULL;
    }
    fquery->branches[i].group_modules = grule;    
    


    struct grouper_aggr* aggrule = calloc(fquery->branches[i].num_aggr, 
                                          sizeof(struct grouper_aggr));
    for (int j = 0; j < fquery->branches[i].num_aggr; j++) {
      aggrule[j].module           =         0;      
      switch (j) {
        case 0:
          aggrule[j].field_offset =         param_data->trace->offsets.srcaddr;
          break;
        case 1:
          aggrule[j].field_offset =         param_data->trace->offsets.dstaddr;
          break;
        case 2:
          aggrule[j].field_offset =         param_data->trace->offsets.dPkts;
          break;
        case 3:
          aggrule[j].field_offset =         param_data->trace->offsets.dOctets;
          break;
      }
      switch (j) {
        case 0:
        case 1:
          aggrule[j].op           =         RULE_STATIC | RULE_S1_32;
          break;
        case 2:
        case 3:
          aggrule[j].op           =         RULE_SUM | RULE_S1_32;
          break;
      }      
      aggrule[j].func             =         NULL;
    }
    fquery->branches[i].aggr = aggrule;    


    
    struct gfilter_rule* gfrule = calloc(fquery->branches[i].num_gfilter_rules, 
                                         sizeof(struct gfilter_rule));
    for (int j = 0; j < fquery->branches[i].num_gfilter_rules; j++) {
      gfrule[j].field             =         param_data->trace->offsets.dPkts;
      gfrule[j].value             =         200;
      gfrule[j].delta             =         0;
      gfrule[j].op                =         RULE_GT | RULE_S1_32;
      gfrule[j].func              =         NULL;
    }
    fquery->branches[i].gfilter_rules = gfrule;
    
  }
  
  
  struct merger_rule* mrule = calloc(fquery->num_merger_rules, 
                                     sizeof(struct merger_rule));
  for (int j = 0; j < fquery->num_merger_rules; j++) {    
    mrule[j].branch1               =         &fquery->branches[0];
    mrule[j].branch2               =         &fquery->branches[1];    
    switch (j) {        
      case 0:
        mrule[j].field1            =         param_data->trace->offsets.srcaddr;
        mrule[j].field2            =         param_data->trace->offsets.dstaddr;        
        break;
      case 1:
        mrule[j].field1            =         param_data->trace->offsets.dstaddr;
        mrule[j].field2            =         param_data->trace->offsets.srcaddr;        
        break;
    }
    mrule[j].delta                 =         0;
    mrule[j].op                    =         RULE_EQ | RULE_S1_32 | RULE_S2_32;
    mrule[j].func                  =         NULL;
  }
  fquery->mrules = mrule;
  
  /* deallocate the json query buffers */
  for (int i = 0; i < json_query->num_frules; i++) {
    struct json_filter_rule* frule = json_query->fruleset[i];
    free(frule->off);
    free(frule);
  }
  free(json_query->fruleset);
  free(json_query);
  
 /* -----------------------------------------------------------------------*/

  
  
  
  
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*            fills filter_rule.func and grouper_rule.func                */
  /*                        from the grouper_rule.op                        */
  /*             by falling through a huge switch statement                 */
  /* -----------------------------------------------------------------------*/    
  
  assign_fptr(fquery, 
              fquery->branches, 
              fquery->num_branches);
  
  /* -----------------------------------------------------------------------*/

  
  
  
  
  
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*               splitter → filter → grouper → group-filter               */
  /* -----------------------------------------------------------------------*/   
  

  /* allocate space for a dedicated thread for each branch */
  pthread_t* thread_ids = (pthread_t *)calloc(fquery->num_branches, 
                                              sizeof(pthread_t));
  if (thread_ids == NULL)
    errExit("calloc");
  
  pthread_attr_t* thread_attrs = (pthread_attr_t *)
                                  calloc(fquery->num_branches, 
                                         sizeof(pthread_attr_t));
  if (thread_attrs == NULL)
    errExit("calloc");
  
  for (int i = 0, ret = 0; i < fquery->num_branches; i++) {

    /* initialize each thread attributes */
    ret = pthread_attr_init(&thread_attrs[i]);
    if (ret != 0)
      errExit("pthread_attr_init");
    
    /* start each thread for a dedicated branch */      
    ret = pthread_create(&thread_ids[i], 
                         &thread_attrs[i], 
                         &branch_start, 
                         (void *)(&fquery->branches[i]));    
    if (ret != 0)
      errExit("pthread_create");
    
    
    /* destroy the thread attributes once the thread is created */  
    ret = pthread_attr_destroy(&thread_attrs[i]);
    if (ret != 0) 
      errExit("pthread_attr_destroy");
  }
  
  /* - wait for each thread to complete its branch
   * - save and print the number of filtered groups on completion
   * - save the filtered groups on completion */
  for (int i = 0, ret = 0; i < fquery->num_branches; i++) {
    ret = pthread_join(thread_ids[i], NULL);
    if (ret != 0)
      errExit("pthread_join");
  }
  
  free(thread_ids);
  free(thread_attrs);
  
  /* -----------------------------------------------------------------------*/    
  
  
  
  
  
  
  
  
  
  
#ifdef MERGER
  
  /* -----------------------------------------------------------------------*/  
  /*                                 merger                                 */
  /* -----------------------------------------------------------------------*/
  
  fquery->group_tuples = merger(fquery->branches,
                                fquery->num_branches, 
                                mrule, 
                                fquery->num_merger_rules,
                                &fquery->total_num_group_tuples,
                                &fquery->num_group_tuples);
  
  /* -----------------------------------------------------------------------*/    
  
#endif  
  
  

  
  
  
  
  
  
  
#ifdef UNGROUPER  
  
  /* -----------------------------------------------------------------------*/  
  /*                                ungrouper                               */
  /* -----------------------------------------------------------------------*/  
  // TODO: free group_collections at some point
  /* -----------------------------------------------------------------------*/
  
  fquery->streamset = ungrouper(fquery,
                                fquery->group_tuples,
                                fquery->num_group_tuples);
  
  /* -----------------------------------------------------------------------*/
  
#endif  
  
  
  
  
  
  
  
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*                                debugging                               */
  /* -----------------------------------------------------------------------*/  

  if(verbose_v){
    
    /* process each branch */
    for (int i = 0; i < fquery->num_branches; i++) {
      struct branch_info* branch = &fquery->branches[i];
      
      printf("\nNo. of Filtered Records: %zd\n", branch->num_filtered_records);      
      puts(FLOWHEADER);      
      for (int j = 0; j < branch->num_filtered_records; j++) {
        flow_print_record(branch->data, branch->filtered_records[j]);
        
        /* not free'd since they point to original records */
        branch->filtered_records[j] = NULL;
      }      
      free(branch->filtered_records);
      
      if (verbose_vv){
        if(if_group_modules_exist){
          
          printf("\nNo. of Sorted Records: %zd\n", branch->num_filtered_records);      
          puts(FLOWHEADER);      
          for (int j = 0; j < branch->num_filtered_records; j++) {
            flow_print_record(branch->data, branch->sorted_records[j]);
            
            /* not free'd since they point to original records */
            branch->sorted_records[j] = NULL;
          }      
          free(branch->sorted_records);
          
          
          printf("\nNo. of Unique Records: %zd\n", branch->num_unique_records);      
          puts(FLOWHEADER);      
          for (int j = 0; j < branch->num_unique_records; j++) {
            flow_print_record(branch->data, branch->unique_records[j]);
            
            /* not free'd since they point to original records */
            branch->unique_records[j] = NULL;
          }      
          free(branch->unique_records);                
        }      
        
        printf("\nNo. of Groups: %zu (Verbose Output)\n", branch->num_groups);
        puts(FLOWHEADER); 
        for (int j = 0; j < branch->num_groups; j++) {
          
          printf("\n");
          struct group* group = branch->groupset[j];
          
          /* print group members */ 
          for (int k = 0; k < group->num_members; k++) {
            flow_print_record(branch->data, group->members[k]);
            group->members[k] = NULL;
          }     
          free(group->members);
        }
      }
      
      
#ifdef GROUPERAGGREGATIONS
      printf("\nNo. of Groups: %zu (Aggregations)\n", branch->num_groups);
      puts(FLOWHEADER); 
      for (int j = 0; j < branch->num_groups; j++) {        
        struct group* group = branch->groupset[j];
        flow_print_record(branch->data, group->group_aggr_record);  
        
        for (int x = 0; x < branch->num_aggr; x++)
          free(group->aggr[x].values);
        free(group->aggr); 
      }
#endif
      
      
#ifdef GROUPFILTER
      printf("\nNo. of Filtered Groups: %zu (Aggregations)\n", 
             branch->num_filtered_groups);      
      puts(FLOWHEADER); 
      
      for (int j = 0; j < branch->num_filtered_groups; j++) {
        
        struct group* filtered_group = branch->filtered_groupset[j];
        flow_print_record(branch->data, filtered_group->group_aggr_record);
      }
#endif
      
      
      /* free memory */
      for (int j = 0; j < branch->num_groups; j++) {        
        struct group* group = branch->groupset[j];
        free(group->group_aggr_record);
        free(group);
      }
      free(branch->groupset);
    }
    
    /* merger */

#ifdef MERGER    
    if (verbose_vv) {
      
      struct permut_iter *iter = iter_init(fquery->branches, 
                                           fquery->num_branches);
      printf("\nNo. of (to be) Matched Groups: %zu \n", 
             fquery->total_num_group_tuples);
      puts(FLOWHEADER);      
      while(iter_next(iter)) {
        for (int j = 0; j < fquery->num_branches; j++) {          
          flow_print_record(param_data->trace, 
                            fquery->branches[j].
                            filtered_groupset[iter->filtered_group_tuple[j] - 1]
                            ->group_aggr_record);
        }
        printf("\n");
      }
      iter_destroy(iter);
    }
    
    printf("\nNo. of Merged Groups: %zu (Tuples)\n", 
           fquery->num_group_tuples);      
    puts(FLOWHEADER);
    
    for (int j = 0; j < fquery->num_group_tuples; j++) {
      struct group** group_tuple = fquery->group_tuples[j];
      for (int i = 0; i < fquery->num_branches; i++) {
        struct group* group = group_tuple[i];
        flow_print_record(param_data->trace, group->group_aggr_record);
      }
      printf("\n");
    }    
#endif

  }
  
  /* -----------------------------------------------------------------------*/      
  
  
  
  
  

  
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*                                output                                  */
  /* -----------------------------------------------------------------------*/    
  
  printf("\nNo. of Streams: %zu \n", fquery->num_group_tuples);
  printf("----------------- \n");  
  for (int j = 0; j < fquery->num_group_tuples; j++) {
    struct stream* stream = fquery->streamset[j];
    printf("\nNo. of Records in Stream (%d): %zu \n",j+1, 
                                                      stream->num_records);
    puts(FLOWHEADER);
    for (int i = 0; i < stream->num_records; i++) {
      char* record = stream->recordset[i];
      flow_print_record(param_data->trace, record);
    }
    printf("\n");
  }
  /* -----------------------------------------------------------------------*/
  
  

/* free branch_info */
  
  
  exit(EXIT_SUCCESS);
}