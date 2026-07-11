#!/usr/bin/env python3
from pathlib import Path
import csv,json,sys

root=Path(sys.argv[1]);snapshots=sorted(root.glob("*/snapshot.json"));profiles={}
with Path(sys.argv[2]).open() as stream:
    for row in csv.DictReader(stream,delimiter="\t"):profiles[(row["type"],row["provider_id"])]=row
assert len(snapshots)==77,(len(snapshots),"expected 1950-2026")
assert snapshots[0].parent.name=="1950" and snapshots[-1].parent.name=="2026"
for path in snapshots:
    document=json.loads(path.read_text());year=path.parent.name
    assert document["schedule"]["MRData"]["RaceTable"]["season"]==year
    for section,key in (("results","Results"),("sprint","SprintResults")):
        for race in document[section]["MRData"]["RaceTable"]["Races"]:
            assert len(race[key])<=64
    standings=document["drivers"]["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"]
    for row in standings:
        assert ("D",row["Driver"]["driverId"]) in profiles,(year,row["Driver"]["driverId"],"missing career")
        series=document["progression"]["drivers"].get(row["Driver"]["driverId"],[])
        assert series,(year,row["Driver"]["driverId"],"missing progression")
        assert abs(float(row["points"])-float(series[-1]["points"]))<0.01,(year,row["Driver"]["driverId"],row["points"],series[-1]["points"])
        assert int(row["position"])==int(series[-1]["position"]),(year,row["Driver"]["driverId"],row["position"],series[-1]["position"])
    constructors=document["constructors"]["MRData"]["StandingsTable"]["StandingsLists"][0]["ConstructorStandings"]
    for row in constructors:
        identity=row["Constructor"]["constructorId"];assert ("C",identity) in profiles,(year,identity,"missing career")
        series=document["progression"]["constructors"].get(identity,[]);assert series,(year,identity,"missing constructor progression")
        assert abs(float(row["points"])-float(series[-1]["points"]))<0.01,(year,identity,row["points"],series[-1]["points"])
        assert int(row["position"])==int(series[-1]["position"]),(year,identity,row["position"],series[-1]["position"])
    if year!="2026":assert all(race.get("timeEstimated") is True for race in document["schedule"]["MRData"]["RaceTable"]["Races"])
    else:assert any("FirstPractice" in race for race in document["schedule"]["MRData"]["RaceTable"]["Races"])
print(f"ok: {len(snapshots)} offline seasons and chart totals")
