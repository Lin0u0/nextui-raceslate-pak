#define _POSIX_C_SOURCE 200809L
#include "rs_reference.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool rs_reference_load(const char *path, RsReferenceCatalog *out){
    FILE *f; char line[8192]; if(!path||!out)return false; memset(out,0,sizeof(*out)); f=fopen(path,"r"); if(!f)return false;
    if(!fgets(line,sizeof(line),f)){fclose(f);return false;}
    while(out->count<RS_MAX_REFERENCES&&fgets(line,sizeof(line),f)){
        RsCircuitReference *r=&out->circuits[out->count]; char *fields[15],*p,*save=NULL; int i=0;
        p=strtok_r(line,"\t\r\n",&save);
        while(i<15&&p){fields[i++]=p;p=strtok_r(NULL,"\t\r\n",&save);}
        if(i<15)continue;
        snprintf(r->provider_id,sizeof(r->provider_id),"%s",fields[0]); r->length_km=strtod(fields[1],NULL); r->turns=atoi(fields[2]);
        snprintf(r->direction,sizeof(r->direction),"%s",fields[3]); r->first_year=atoi(fields[4]); r->races=atoi(fields[5]);
        snprintf(r->lap_record,sizeof(r->lap_record),"%s",fields[6]); snprintf(r->record_driver,sizeof(r->record_driver),"%s",fields[7]); r->record_year=atoi(fields[8]);
        snprintf(r->recent_winners,sizeof(r->recent_winners),"%s",fields[9]);snprintf(r->recent_poles,sizeof(r->recent_poles),"%s",fields[10]);snprintf(r->most_wins,sizeof(r->most_wins),"%s",fields[11]);snprintf(r->most_poles,sizeof(r->most_poles),"%s",fields[12]);snprintf(r->all_winners,sizeof(r->all_winners),"%s",fields[13]);snprintf(r->all_poles,sizeof(r->all_poles),"%s",fields[14]); out->count++;
    }
    fclose(f); return out->count>0;
}
const RsCircuitReference *rs_reference_circuit(const RsReferenceCatalog *catalog,const char *id){size_t i;if(!catalog||!id)return NULL;for(i=0;i<catalog->count;i++)if(!strcmp(catalog->circuits[i].provider_id,id))return &catalog->circuits[i];return NULL;}
