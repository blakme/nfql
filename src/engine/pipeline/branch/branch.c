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

#include "branch.h"

void *
branch_start(void *arg) {
  
  struct branch_info*         binfo = (struct branch_info *)arg;  
  
  /* filter stage variables */
  char**                      filtered_records;
  size_t                      num_filtered_records = 0;
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*                                filter                                  */
  /* -----------------------------------------------------------------------*/  
  
  filtered_records = filter(binfo->data, 
                            binfo->filter_rules, 
                            binfo->num_filter_rules, 
                            &num_filtered_records);
  
  if(verbose_v){
    
    binfo->filtered_records = (char**)
    malloc(num_filtered_records * sizeof(char*));
    for (int i = 0; i < num_filtered_records; i++)
      binfo->filtered_records[i] = filtered_records[i];
    binfo->num_filtered_records = num_filtered_records;
  }
  
  /* -----------------------------------------------------------------------*/
  
  
  
  
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*                               grouper                                  */
  /* -----------------------------------------------------------------------*/  
  
  
  binfo->groupset = grouper(filtered_records, 
                            num_filtered_records, 
                            binfo,
                            binfo->group_modules, 
                            binfo->num_group_modules, 
                            binfo->aggr, 
                            binfo->num_aggr, 
                            &binfo->num_groups);
  
  
  /* -----------------------------------------------------------------------*/
  
  
  
  
  
  
  
  
  
  
  /* -----------------------------------------------------------------------*/  
  /*                         grouper aggregations                           */
  /* -----------------------------------------------------------------------*/
  
#ifdef GROUPERAGGREGATIONS  
  
  for (int i = 0; i < binfo->num_groups; i++)
    binfo->groupset[i]->group_aggr_record = 
    grouper_aggregations(binfo->groupset[i], binfo);
  
#endif
  
  /* -----------------------------------------------------------------------*/
  
  
  
  
  
  
  
#ifdef GROUPFILTER
  
  /* -----------------------------------------------------------------------*/  
  /*                            grouper-filter                              */
  /* -----------------------------------------------------------------------*/  
  
  binfo->filtered_groupset = groupfilter(binfo->groupset, 
                                          binfo->num_groups, 
                                          binfo->gfilter_rules, 
                                          binfo->num_gfilter_rules, 
                                          &binfo->num_filtered_groups);
  
  /* -----------------------------------------------------------------------*/
  
#endif  
  
  pthread_exit(NULL);
}