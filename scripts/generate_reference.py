#!/usr/bin/env python3
"""Generate compact, attributed RaceSlate history from a pinned F1DB checkout."""
from pathlib import Path
import csv, sys, yaml
from collections import Counter

ALIASES = {
 "albert_park":"melbourne", "shanghai":"shanghai", "suzuka":"suzuka", "miami":"miami",
 "villeneuve":"montreal", "monaco":"monaco", "catalunya":"catalunya", "red_bull_ring":"spielberg",
 "silverstone":"silverstone", "spa":"spa-francorchamps", "hungaroring":"hungaroring", "zandvoort":"zandvoort",
 "monza":"monza", "madring":"madring", "baku":"baku", "marina_bay":"marina-bay", "americas":"austin",
 "rodriguez":"mexico-city", "interlagos":"interlagos", "vegas":"las-vegas", "losail":"lusail", "yas_marina":"yas-marina"}

def load(path):
    with path.open() as f: return yaml.safe_load(f)

def main():
    root, output = Path(sys.argv[1]), Path(sys.argv[2]); output.parent.mkdir(parents=True, exist_ok=True)
    drivers={p.stem:load(p).get("name",p.stem) for p in (root/"src/data/drivers").glob("*.yml")}
    races=[]
    for path in (root/"src/data/seasons").glob("*/races/*/race.yml"):
        data=load(path); data["year"]=int(path.parts[-4]); data["dir"]=path.parent; races.append(data)
    current={r["circuitId"]:r for r in races if r["year"]==2026}
    with output.open("w",newline="") as f:
        w=csv.writer(f,delimiter="\t",lineterminator="\n"); w.writerow(["provider_id","length","turns","direction","first_year","races","lap_record","record_driver","record_year","recent_winners","recent_poles","most_wins","most_poles","all_winners","all_poles"])
        for provider,circuit in ALIASES.items():
            venue=sorted((r for r in races if r.get("circuitId")==circuit),key=lambda r:(r["year"],r.get("round",0)))
            winners=[]; poles=[]; records=[]
            for r in venue:
                results=r["dir"]/"race-results.yml"; fast=r["dir"]/"fastest-laps.yml"
                if results.exists():
                    rows=load(results) or []
                    if rows: winners.append((r["year"],drivers.get(rows[0].get("driverId"),rows[0].get("driverId","UNKNOWN"))))
                qualifying=r["dir"]/"qualifying-results.yml"
                if qualifying.exists():
                    rows=load(qualifying) or []
                    if rows:poles.append((r["year"],drivers.get(rows[0].get("driverId"),rows[0].get("driverId","UNKNOWN"))))
                if fast.exists() and r.get("circuitLayoutId")==current[circuit].get("circuitLayoutId"):
                    rows=load(fast) or []
                    if rows and rows[0].get("time"): records.append((rows[0]["time"],drivers.get(rows[0].get("driverId"),"UNKNOWN"),r["year"]))
            record=min(records,key=lambda x:tuple(map(float,x[0].split(":")))) if records else ("UNKNOWN","UNKNOWN",0)
            recent=", ".join(f"{year} {name}" for year,name in winners[-5:][::-1]) or "NO RESULTS"
            recent_poles=", ".join(f"{year} {name}" for year,name in poles[-5:][::-1]) or "NO RESULTS"
            win_leader=Counter(name for _,name in winners).most_common(1);pole_leader=Counter(name for _,name in poles).most_common(1)
            most_wins=f"{win_leader[0][0]} ({win_leader[0][1]})" if win_leader else "UNKNOWN"
            most_poles=f"{pole_leader[0][0]} ({pole_leader[0][1]})" if pole_leader else "UNKNOWN"
            r=current[circuit]
            all_winners="|".join(f"{year} {name}" for year,name in winners[::-1]) or "NO RESULTS"
            all_poles="|".join(f"{year} {name}" for year,name in poles[::-1]) or "NO RESULTS"
            w.writerow([provider,r.get("courseLength",0),r.get("turns",0),r.get("direction","UNKNOWN"),venue[0]["year"] if venue else 0,len(venue),record[0],record[1],record[2],recent,recent_poles,most_wins,most_poles,all_winners,all_poles])
if __name__=="__main__": main()
