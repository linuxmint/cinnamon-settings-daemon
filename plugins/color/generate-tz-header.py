#!/usr/bin/python3

import re

d = {}


with open("/usr/share/zoneinfo/zone.tab", "r") as f:
    for line in f:
        if line.startswith("#"):
            continue

        res = re.search(r"([A-Z]{2})\s([0-9-+]+)\s([\w/_\-]+)\s", line)
        code, coords, tz = res.groups()

        res = re.search(r"([+-]{1})([0-9]+)([+-]{1})([0-9]+)", coords)
        lat_sign, lat_val, long_sign, long_val = res.groups()

        lat_str = lat_sign + lat_val[0:2] + "." + lat_val[2:]
        long_str = long_sign + long_val[0:3] + "." + long_val[3:]

        lat = float(lat_str)
        long = float(long_str)

        d[tz] = [lat, long]

header = """
// Generated from /usr/share/zoneinfo/zone.tab, used by csd-nightlight.c to calculate sunrise and sunset based on the system timezone

typedef struct
{
    const gchar *timezone;
    double latitude;
    double longitude;
} TZCoords;

static TZCoords tz_coord_list[] = {
"""

for zone in sorted(d.keys()):
    latitude, longitude = d[zone]

    header += "    { \"%s\", %f, %f },\n" % (zone, latitude, longitude)

header += "};"

with open("tz-coords.h", "w") as f:
    f.write(header)

quit()