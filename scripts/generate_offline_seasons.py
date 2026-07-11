#!/usr/bin/env python3
"""Generate RaceSlate offline season snapshots from a pinned F1DB checkout."""
from pathlib import Path
import csv,json,sys,yaml

def load(path):
    if not path.exists():return []
    with path.open() as stream:return yaml.safe_load(stream) or []

def integer(value,default=99):
    try:return int(value)
    except (TypeError,ValueError):return default

def main():
    if len(sys.argv)!=4:raise SystemExit("usage: generate_offline_seasons.py F1DB_ROOT PROFILES_TSV OUTPUT_DIR")
    root,profiles_path,output=Path(sys.argv[1]),Path(sys.argv[2]),Path(sys.argv[3]);output.mkdir(parents=True,exist_ok=True)
    aliases={};profile_ids=set()
    with profiles_path.open() as stream:
        for row in csv.DictReader(stream,delimiter="\t"):aliases.setdefault((row["type"],row["source_id"]),row["provider_id"]);profile_ids.add((row["type"],row["provider_id"]))
    drivers={path.stem:load(path) for path in (root/"src/data/drivers").glob("*.yml")};constructors={path.stem:load(path) for path in (root/"src/data/constructors").glob("*.yml")};circuits={path.stem:load(path) for path in (root/"src/data/circuits").glob("*.yml")}
    current_schedule=json.loads((output.parent/"schedule.json").read_text()) if (output.parent/"schedule.json").exists() else None
    def driver(source):
        data=drivers.get(source,{})
        return {"driverId":aliases.get(("D",source),source),"code":data.get("abbreviation") or "---","givenName":data.get("firstName") or data.get("name",source),"familyName":data.get("lastName") or ""}
    def constructor(source,identity=None):
        data=constructors.get(source,{})
        return {"constructorId":identity or aliases.get(("C",source),source),"name":data.get("name",source)}
    def constructor_identity(item,year):
        source=item.get("constructorId");engine=item.get("engineManufacturerId");candidate=f"{source}-{engine}" if source and engine and source!=engine else source
        if item.get("position") and not str(item.get("position")).isdigit():candidate=f"{candidate}-excluded"
        if year<2026 and ("C",candidate) in profile_ids:return candidate
        return source if year<2026 else aliases.get(("C",source),source)
    for season_dir in sorted((root/"src/data/seasons").glob("[12][0-9][0-9][0-9]")):
        year=int(season_dir.name);race_paths=sorted(season_dir.glob("races/*/race.yml"),key=lambda path:int(load(path).get("round",0)))
        schedule=[];race_results=[];qualifying_results=[];sprint_results=[];driver_team={};driver_wins={};constructor_wins={};progression={"drivers":{},"constructors":{}}
        for race_path in race_paths:
            race=load(race_path);circuit=circuits.get(race.get("circuitId"),{});round_number=int(race.get("round",0));date=str(race.get("date"));grand_prix=(race.get("grandPrixId") or "").replace("-"," ").title();name=grand_prix if grand_prix.lower().endswith("grand prix") else grand_prix+" Grand Prix"
            base={"season":str(year),"round":str(round_number),"raceName":name,"Circuit":{"circuitId":race.get("circuitId","unknown"),"circuitName":circuit.get("name",race.get("circuitId","Unknown")),"Location":{"lat":str(circuit.get("latitude",0)),"long":str(circuit.get("longitude",0)),"locality":circuit.get("placeName","Unknown"),"country":circuit.get("countryId","Unknown").replace("-"," ").title()}},"date":date,"time":"12:00:00Z","timeEstimated":True}
            schedule.append(base)
            for item in load(race_path.parent/"driver-standings.yml"):
                source=item.get("driverId");provider=aliases.get(("D",source),source);progression["drivers"].setdefault(provider,[]).append({"round":round_number,"position":integer(item.get("position")),"points":float(item.get("points") or 0)})
            constructor_totals={}
            for item in load(race_path.parent/"constructor-standings.yml"):
                provider=constructor_identity(item,year);constructor_totals[provider]=constructor_totals.get(provider,0.0)+float(item.get("points") or 0)
            for position,(provider,points) in enumerate(sorted(constructor_totals.items(),key=lambda value:(-value[1],value[0])),1):progression["constructors"].setdefault(provider,[]).append({"round":round_number,"position":position,"points":points})
            def rows(filename,kind):
                converted=[]
                for item in load(race_path.parent/filename):
                    source_driver=item.get("driverId");source_constructor=item.get("constructorId");identity=constructor_identity(item,year);driver_team[source_driver]=(source_constructor,identity)
                    row={"position":str(item.get("position",0)),"grid":str(item.get("gridPosition",0)),"laps":str(item.get("laps",0)),"points":str(item.get("points") or 0),"Driver":driver(source_driver),"Constructor":constructor(source_constructor,identity),"status":item.get("reasonRetired") or "Finished"}
                    if kind=="qualifying":row.update({"Q1":item.get("q1") or "","Q2":item.get("q2") or "","Q3":item.get("q3") or ""})
                    else:
                        value=item.get("time") or item.get("gap") or ""
                        if value:row["Time"]={"time":str(value)}
                    converted.append(row)
                return converted
            converted=rows("race-results.yml","race")
            if converted:
                race_results.append({**base,"Results":converted});winner=load(race_path.parent/"race-results.yml")[0];driver_wins[winner.get("driverId")]=driver_wins.get(winner.get("driverId"),0)+1;winner_identity=constructor_identity(winner,year);constructor_wins[winner_identity]=constructor_wins.get(winner_identity,0)+1
            converted=rows("qualifying-results.yml","qualifying")
            if converted:qualifying_results.append({**base,"QualifyingResults":converted})
            converted=rows("sprint-race-results.yml","sprint")
            if converted:sprint_results.append({**base,"SprintResults":converted})
        driver_rows=[]
        for item in load(season_dir/"driver-standings.yml"):
            source=item.get("driverId");provider=aliases.get(("D",source),source);team=driver_team.get(source);position=integer(item.get("position"));points=float(item.get("points") or 0);driver_rows.append({"position":str(position),"points":str(item.get("points") or 0),"wins":str(driver_wins.get(source,0)),"Driver":driver(source),"Constructors":[constructor(team[0],team[1])] if team else [{"constructorId":"unknown","name":"Unknown"}]});series=progression["drivers"].get(provider)
            if series:series[-1].update({"position":position,"points":points})
            else:progression["drivers"][provider]=[{"round":len(race_paths),"position":position,"points":points}]
        constructor_rows=[]
        for item in load(season_dir/"constructor-standings.yml"):
            source=item.get("constructorId");provider=constructor_identity(item,year);position=integer(item.get("position"));points=float(item.get("points") or 0);constructor_rows.append({"position":str(position),"points":str(item.get("points") or 0),"wins":str(constructor_wins.get(provider,0)),"Constructor":constructor(source,provider)});series=progression["constructors"].get(provider); 
            if series:series[-1].update({"position":position,"points":points})
            else:progression["constructors"][provider]=[{"round":len(race_paths),"position":position,"points":points}]
        def race_table(races):return {"MRData":{"RaceTable":{"season":str(year),"Races":races}}}
        def standings_table(key,rows):return {"MRData":{"StandingsTable":{"season":str(year),"StandingsLists":[{"season":str(year),"round":str(len(schedule)),key:rows}]}}}
        if current_schedule and current_schedule.get("MRData",{}).get("RaceTable",{}).get("season")==str(year):schedule=current_schedule["MRData"]["RaceTable"]["Races"]
        snapshot={"schedule":race_table(schedule),"drivers":standings_table("DriverStandings",driver_rows),"constructors":standings_table("ConstructorStandings",constructor_rows),"results":race_table(race_results),"qualifying":race_table(qualifying_results),"sprint":race_table(sprint_results),"progression":progression,"weather":None}
        target=output/str(year)/"snapshot.json";target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(snapshot,separators=(",",":")))

if __name__=="__main__":main()
