#include "rs_circuit_atlas.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool rs_circuit_atlas_load(const char *path,RsCircuitAtlas *atlas){FILE *file;char line[256];if(!path||!atlas)return false;memset(atlas,0,sizeof(*atlas));file=fopen(path,"r");if(!file)return false;if(!fgets(line,sizeof(line),file)){fclose(file);return false;}while(atlas->count<RS_MAX_ATLAS_CIRCUITS&&fgets(line,sizeof(line),file)){RsCircuitAtlasEntry *entry=&atlas->entries[atlas->count];char id[64];double latitude,longitude;int year,round;if(sscanf(line,"%d\t%d\t%63[^\t]\t%lf\t%lf",&year,&round,id,&latitude,&longitude)!=5)continue;entry->year=year;entry->round=round;snprintf(entry->asset_id,sizeof(entry->asset_id),"%s",id);entry->latitude=latitude;entry->longitude=longitude;atlas->count++;}fclose(file);return atlas->count>0;}

const char *rs_circuit_atlas_nearest(const RsCircuitAtlas *atlas,int year,int round,double latitude,double longitude){size_t index;double best=DBL_MAX;const char *asset=NULL;if(!atlas)return NULL;for(index=0;index<atlas->count;index++){double dy,dx,distance;if(atlas->entries[index].year!=year||atlas->entries[index].round!=round)continue;dy=atlas->entries[index].latitude-latitude;dx=(atlas->entries[index].longitude-longitude)*0.65;distance=dx*dx+dy*dy;if(distance<best){best=distance;asset=atlas->entries[index].asset_id;}}return best<=0.25?asset:NULL;}
