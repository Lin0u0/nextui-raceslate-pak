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

def write_venue(writer,provider,venue,anchor,drivers):
    archives=[]; records=[]
    for race in venue:
        results=race["dir"]/"race-results.yml"; fast=race["dir"]/"fastest-laps.yml"; qualifying=race["dir"]/"qualifying-results.yml"
        winner=None; pole=None
        if results.exists():
            rows=load(results) or []
            if rows:winner=drivers.get(rows[0].get("driverId"),rows[0].get("driverId","UNKNOWN"))
        if qualifying.exists():
            rows=load(qualifying) or []
            if rows:pole=drivers.get(rows[0].get("driverId"),rows[0].get("driverId","UNKNOWN"))
        if winner or pole:archives.append((race["year"],winner or "—",pole or "—"))
        if fast.exists() and race.get("circuitLayoutId")==anchor.get("circuitLayoutId"):
            rows=load(fast) or []
            if rows and rows[0].get("time"):records.append((rows[0]["time"],drivers.get(rows[0].get("driverId"),"UNKNOWN"),race["year"]))
    record=min(records,key=lambda value:tuple(map(float,value[0].split(":")))) if records else ("UNKNOWN","UNKNOWN",0)
    winners=[(year,winner) for year,winner,_ in archives if winner!="—"];poles=[(year,pole) for year,_,pole in archives if pole!="—"]
    win_leader=Counter(name for _,name in winners).most_common(1);pole_leader=Counter(name for _,name in poles).most_common(1)
    writer.writerow([provider,anchor.get("courseLength",0),anchor.get("turns",0),anchor.get("direction","UNKNOWN"),venue[0]["year"],len(venue),record[0],record[1],record[2],", ".join(f"{year} {name}" for year,name in winners[-5:][::-1]) or "NO RESULTS",", ".join(f"{year} {name}" for year,name in poles[-5:][::-1]) or "NO RESULTS",f"{win_leader[0][0]} ({win_leader[0][1]})" if win_leader else "UNKNOWN",f"{pole_leader[0][0]} ({pole_leader[0][1]})" if pole_leader else "UNKNOWN","|".join(f"{year} {winner}" for year,winner,_ in archives[::-1]) or "NO RESULTS","|".join(f"{year} {pole}" for year,_,pole in archives[::-1]) or "NO RESULTS"])

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
            write_venue(w,provider,venue,current[circuit],drivers)
        for circuit in sorted({race.get("circuitId") for race in races if race.get("circuitId")}):
            venue=sorted((race for race in races if race.get("circuitId")==circuit),key=lambda race:(race["year"],race.get("round",0)))
            for layout in sorted({race.get("circuitLayoutId") for race in venue if race.get("circuitLayoutId")}):
                layout_races=[race for race in venue if race.get("circuitLayoutId")==layout]
                write_venue(w,f"layout-{layout}",venue,layout_races[-1],drivers)
if __name__=="__main__": main()
