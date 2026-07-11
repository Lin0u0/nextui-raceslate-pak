#include "rs_results.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *txt(const cJSON *o,const char *k){const cJSON *i=cJSON_GetObjectItemCaseSensitive(o,k);return cJSON_IsString(i)?i->valuestring:NULL;}
static int integer(const cJSON *o,const char *k){const char *v=txt(o,k);return v?atoi(v):0;}
static void cp(char *d,size_t n,const char *s){if(!s)s="";snprintf(d,n,"%s",s);}
const RsClassification *rs_results_find(const RsResultsCatalog *c,int round,RsResultKind kind){size_t i;if(!c)return NULL;for(i=0;i<c->count;i++)if(c->classifications[i].round==round&&c->classifications[i].kind==kind)return &c->classifications[i];return NULL;}
static RsClassification *slot(RsResultsCatalog *c,int round,RsResultKind kind){size_t i;for(i=0;i<c->count;i++)if(c->classifications[i].round==round&&c->classifications[i].kind==kind)return &c->classifications[i];if(c->count>=RS_MAX_CLASSIFICATIONS)return NULL;c->classifications[c->count]=(RsClassification){.round=round,.kind=kind};return &c->classifications[c->count++];}
bool rs_results_decode(const char *json,RsResultKind kind,RsResultsCatalog *catalog){
 cJSON *root;const cJSON *mr,*table,*races,*race;const char *key=kind==RS_RESULT_RACE?"Results":kind==RS_RESULT_SPRINT?"SprintResults":"QualifyingResults";bool any=false;
 if(!json||!catalog)return false;root=cJSON_Parse(json);if(!root)return false;mr=cJSON_GetObjectItemCaseSensitive(root,"MRData");table=cJSON_GetObjectItemCaseSensitive(mr,"RaceTable");races=cJSON_GetObjectItemCaseSensitive(table,"Races");if(!cJSON_IsArray(races)){cJSON_Delete(root);return false;}
 cJSON_ArrayForEach(race,races){const cJSON *rows=cJSON_GetObjectItemCaseSensitive(race,key),*row;RsClassification *out;if(!cJSON_IsArray(rows))continue;out=slot(catalog,integer(race,"round"),kind);if(!out){cJSON_Delete(root);return false;}out->entry_count=0;
  cJSON_ArrayForEach(row,rows){const cJSON *driver,*constructor,*fl,*time;RsClassificationEntry *e;char name[96];if(out->entry_count>=RS_MAX_CLASSIFICATION_ENTRIES){cJSON_Delete(root);return false;}e=&out->entries[out->entry_count++];memset(e,0,sizeof(*e));driver=cJSON_GetObjectItemCaseSensitive(row,"Driver");constructor=cJSON_GetObjectItemCaseSensitive(row,"Constructor");
   e->position=integer(row,"position");e->grid=integer(row,"grid");e->laps=integer(row,"laps");e->points=strtod(txt(row,"points")?txt(row,"points"):"0",NULL);cp(e->driver_id,sizeof(e->driver_id),txt(driver,"driverId"));cp(e->driver_code,sizeof(e->driver_code),txt(driver,"code"));snprintf(name,sizeof(name),"%s %s",txt(driver,"givenName")?txt(driver,"givenName"):"",txt(driver,"familyName")?txt(driver,"familyName"):"");cp(e->driver_name,sizeof(e->driver_name),name);cp(e->constructor_name,sizeof(e->constructor_name),txt(constructor,"name"));cp(e->status,sizeof(e->status),txt(row,"status")?txt(row,"status"):"CLASSIFIED");cp(e->q1,sizeof(e->q1),txt(row,"Q1"));cp(e->q2,sizeof(e->q2),txt(row,"Q2"));cp(e->q3,sizeof(e->q3),txt(row,"Q3"));time=cJSON_GetObjectItemCaseSensitive(row,"Time");cp(e->time,sizeof(e->time),txt(time,"time"));fl=cJSON_GetObjectItemCaseSensitive(row,"FastestLap");e->fastest_lap=cJSON_IsObject(fl)&&integer(fl,"rank")==1;
  }any=any||out->entry_count>0;
 }cJSON_Delete(root);return any;
}
