#!/usr/bin/env python3

import re
from argparse import ArgumentParser
from pathlib import Path

COORDS_RE = re.compile(r"([+-]{1}[0-9]{2})([0-9]{2})([0-9]*)([+-]{1}[0-9]{3})([0-9]{2})([0-9]*)")

d = {}

parser = ArgumentParser(prog='generate-tz-header',
                        description='Generate tz-coords.h header from timezone-data')
parser.add_argument('-i', '--zone_tab', nargs='?', default='/usr/share/zoneinfo/zone.tab', type=Path)
parser.add_argument('-o', '--out_file', nargs='?', default='tz-coords.h', type=Path)
args = parser.parse_args()

with open(args.zone_tab, "r") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        coords, tz = line.split('\t')[1:3]
        lat_deg, lat_min, lat_sec, long_deg, long_min, long_sec = COORDS_RE.search(coords).groups()

        lat = float(lat_deg + str((int(lat_min) / 60.0) + ((int(lat_sec) if lat_sec else 0) / 3600.0))[1:])
        long = float(long_deg + str((int(long_min) / 60.0) + ((int(long_sec) if long_sec else 0) / 3600.0))[1:])

        d[tz] = [lat, long]

header = """
// Generated from %s, used by csd-nightlight.c to calculate sunrise and sunset based on the system timezone

typedef struct
{
    const gchar *timezone;
    double latitude;
    double longitude;
} TZCoords;

static TZCoords tz_coord_list[] = {
""" % (args.zone_tab)

for zone in sorted(d.keys()):
    latitude, longitude = d[zone]

    header += "    { \"%s\", %f, %f },\n" % (zone, latitude, longitude)

header += "};"

with open(args.out_file, "w") as f:
    f.write(header)

quit()
