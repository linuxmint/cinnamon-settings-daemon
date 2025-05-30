
// Generated from /usr/share/zoneinfo/zone.tab, used by csd-nightlight.c to calculate sunrise and sunset based on the system timezone

typedef struct
{
    const gchar *timezone;
    double latitude;
    double longitude;
} TZCoords;

static TZCoords tz_coord_list[] = {
    { "Africa/Abidjan", 5.190000, -4.020000 },
    { "Africa/Accra", 5.330000, -0.130000 },
    { "Africa/Addis_Ababa", 9.020000, 38.420000 },
    { "Africa/Algiers", 36.470000, 3.030000 },
    { "Africa/Asmara", 15.200000, 38.530000 },
    { "Africa/Bamako", 12.390000, -8.000000 },
    { "Africa/Bangui", 4.220000, 18.350000 },
    { "Africa/Banjul", 13.280000, -16.390000 },
    { "Africa/Bissau", 11.510000, -15.350000 },
    { "Africa/Blantyre", -15.470000, 35.000000 },
    { "Africa/Brazzaville", -4.160000, 15.170000 },
    { "Africa/Bujumbura", -3.230000, 29.220000 },
    { "Africa/Cairo", 30.030000, 31.150000 },
    { "Africa/Casablanca", 33.390000, -7.350000 },
    { "Africa/Ceuta", 35.530000, -5.190000 },
    { "Africa/Conakry", 9.310000, -13.430000 },
    { "Africa/Dakar", 14.400000, -17.260000 },
    { "Africa/Dar_es_Salaam", -6.480000, 39.170000 },
    { "Africa/Djibouti", 11.360000, 43.090000 },
    { "Africa/Douala", 4.030000, 9.420000 },
    { "Africa/El_Aaiun", 27.090000, -13.120000 },
    { "Africa/Freetown", 8.300000, -13.150000 },
    { "Africa/Gaborone", -24.390000, 25.550000 },
    { "Africa/Harare", -17.500000, 31.030000 },
    { "Africa/Johannesburg", -26.150000, 28.000000 },
    { "Africa/Juba", 4.510000, 31.370000 },
    { "Africa/Kampala", 0.190000, 32.250000 },
    { "Africa/Khartoum", 15.360000, 32.320000 },
    { "Africa/Kigali", -1.570000, 30.040000 },
    { "Africa/Kinshasa", -4.180000, 15.180000 },
    { "Africa/Lagos", 6.270000, 3.240000 },
    { "Africa/Libreville", 0.230000, 9.270000 },
    { "Africa/Lome", 6.080000, 1.130000 },
    { "Africa/Luanda", -8.480000, 13.140000 },
    { "Africa/Lubumbashi", -11.400000, 27.280000 },
    { "Africa/Lusaka", -15.250000, 28.170000 },
    { "Africa/Malabo", 3.450000, 8.470000 },
    { "Africa/Maputo", -25.580000, 32.350000 },
    { "Africa/Maseru", -29.280000, 27.300000 },
    { "Africa/Mbabane", -26.180000, 31.060000 },
    { "Africa/Mogadishu", 2.040000, 45.220000 },
    { "Africa/Monrovia", 6.180000, -10.470000 },
    { "Africa/Nairobi", -1.170000, 36.490000 },
    { "Africa/Ndjamena", 12.070000, 15.030000 },
    { "Africa/Niamey", 13.310000, 2.070000 },
    { "Africa/Nouakchott", 18.060000, -15.570000 },
    { "Africa/Ouagadougou", 12.220000, -1.310000 },
    { "Africa/Porto-Novo", 6.290000, 2.370000 },
    { "Africa/Sao_Tome", 0.200000, 6.440000 },
    { "Africa/Tripoli", 32.540000, 13.110000 },
    { "Africa/Tunis", 36.480000, 10.110000 },
    { "Africa/Windhoek", -22.340000, 17.060000 },
    { "America/Adak", 51.524800, -176.392900 },
    { "America/Anchorage", 61.130500, -149.540100 },
    { "America/Anguilla", 18.120000, -63.040000 },
    { "America/Antigua", 17.030000, -61.480000 },
    { "America/Araguaina", -7.120000, -48.120000 },
    { "America/Argentina/Buenos_Aires", -34.360000, -58.270000 },
    { "America/Argentina/Catamarca", -28.280000, -65.470000 },
    { "America/Argentina/Cordoba", -31.240000, -64.110000 },
    { "America/Argentina/Jujuy", -24.110000, -65.180000 },
    { "America/Argentina/La_Rioja", -29.260000, -66.510000 },
    { "America/Argentina/Mendoza", -32.530000, -68.490000 },
    { "America/Argentina/Rio_Gallegos", -51.380000, -69.130000 },
    { "America/Argentina/Salta", -24.470000, -65.250000 },
    { "America/Argentina/San_Juan", -31.320000, -68.310000 },
    { "America/Argentina/San_Luis", -33.190000, -66.210000 },
    { "America/Argentina/Tucuman", -26.490000, -65.130000 },
    { "America/Argentina/Ushuaia", -54.480000, -68.180000 },
    { "America/Aruba", 12.300000, -69.580000 },
    { "America/Asuncion", -25.160000, -57.400000 },
    { "America/Atikokan", 48.453100, -91.371800 },
    { "America/Bahia", -12.590000, -38.310000 },
    { "America/Bahia_Banderas", 20.480000, -105.150000 },
    { "America/Barbados", 13.060000, -59.370000 },
    { "America/Belem", -1.270000, -48.290000 },
    { "America/Belize", 17.300000, -88.120000 },
    { "America/Blanc-Sablon", 51.250000, -57.070000 },
    { "America/Boa_Vista", 2.490000, -60.400000 },
    { "America/Bogota", 4.360000, -74.050000 },
    { "America/Boise", 43.364900, -116.120900 },
    { "America/Cambridge_Bay", 69.065000, -105.031000 },
    { "America/Campo_Grande", -20.270000, -54.370000 },
    { "America/Cancun", 21.050000, -86.460000 },
    { "America/Caracas", 10.300000, -66.560000 },
    { "America/Cayenne", 4.560000, -52.200000 },
    { "America/Cayman", 19.180000, -81.230000 },
    { "America/Chicago", 41.510000, -87.390000 },
    { "America/Chihuahua", 28.380000, -106.050000 },
    { "America/Ciudad_Juarez", 31.440000, -106.290000 },
    { "America/Costa_Rica", 9.560000, -84.050000 },
    { "America/Coyhaique", -45.340000, -72.040000 },
    { "America/Creston", 49.060000, -116.310000 },
    { "America/Cuiaba", -15.350000, -56.050000 },
    { "America/Curacao", 12.110000, -69.000000 },
    { "America/Danmarkshavn", 76.460000, -18.400000 },
    { "America/Dawson", 64.040000, -139.250000 },
    { "America/Dawson_Creek", 55.460000, -120.140000 },
    { "America/Denver", 39.442100, -104.590300 },
    { "America/Detroit", 42.195300, -83.024500 },
    { "America/Dominica", 15.180000, -61.240000 },
    { "America/Edmonton", 53.330000, -113.280000 },
    { "America/Eirunepe", -6.400000, -69.520000 },
    { "America/El_Salvador", 13.420000, -89.120000 },
    { "America/Fort_Nelson", 58.480000, -122.420000 },
    { "America/Fortaleza", -3.430000, -38.300000 },
    { "America/Glace_Bay", 46.120000, -59.570000 },
    { "America/Goose_Bay", 53.200000, -60.250000 },
    { "America/Grand_Turk", 21.280000, -71.080000 },
    { "America/Grenada", 12.030000, -61.450000 },
    { "America/Guadeloupe", 16.140000, -61.320000 },
    { "America/Guatemala", 14.380000, -90.310000 },
    { "America/Guayaquil", -2.100000, -79.500000 },
    { "America/Guyana", 6.480000, -58.100000 },
    { "America/Halifax", 44.390000, -63.360000 },
    { "America/Havana", 23.080000, -82.220000 },
    { "America/Hermosillo", 29.040000, -110.580000 },
    { "America/Indiana/Indianapolis", 39.460600, -86.092900 },
    { "America/Indiana/Knox", 41.174500, -86.373000 },
    { "America/Indiana/Marengo", 38.223200, -86.204100 },
    { "America/Indiana/Petersburg", 38.293100, -87.164300 },
    { "America/Indiana/Tell_City", 37.571100, -86.454100 },
    { "America/Indiana/Vevay", 38.445200, -85.040200 },
    { "America/Indiana/Vincennes", 38.403800, -87.314300 },
    { "America/Indiana/Winamac", 41.030500, -86.361100 },
    { "America/Inuvik", 68.205900, -133.430000 },
    { "America/Iqaluit", 63.440000, -68.280000 },
    { "America/Jamaica", 17.580500, -76.473600 },
    { "America/Juneau", 58.180700, -134.251100 },
    { "America/Kentucky/Louisville", 38.151500, -85.453400 },
    { "America/Kentucky/Monticello", 36.494700, -84.505700 },
    { "America/Kralendijk", 12.090300, -68.163600 },
    { "America/La_Paz", -16.300000, -68.090000 },
    { "America/Lima", -12.030000, -77.030000 },
    { "America/Los_Angeles", 34.030800, -118.143400 },
    { "America/Lower_Princes", 18.030500, -63.025000 },
    { "America/Maceio", -9.400000, -35.430000 },
    { "America/Managua", 12.090000, -86.170000 },
    { "America/Manaus", -3.080000, -60.010000 },
    { "America/Marigot", 18.040000, -63.050000 },
    { "America/Martinique", 14.360000, -61.050000 },
    { "America/Matamoros", 25.500000, -97.300000 },
    { "America/Mazatlan", 23.130000, -106.250000 },
    { "America/Menominee", 45.062800, -87.365100 },
    { "America/Merida", 20.580000, -89.370000 },
    { "America/Metlakatla", 55.073700, -131.343500 },
    { "America/Mexico_City", 19.240000, -99.090000 },
    { "America/Miquelon", 47.030000, -56.200000 },
    { "America/Moncton", 46.060000, -64.470000 },
    { "America/Monterrey", 25.400000, -100.190000 },
    { "America/Montevideo", -34.543300, -56.124500 },
    { "America/Montserrat", 16.430000, -62.130000 },
    { "America/Nassau", 25.050000, -77.210000 },
    { "America/New_York", 40.425100, -74.002300 },
    { "America/Nome", 64.300400, -165.242300 },
    { "America/Noronha", -3.510000, -32.250000 },
    { "America/North_Dakota/Beulah", 47.155100, -101.464000 },
    { "America/North_Dakota/Center", 47.065900, -101.175700 },
    { "America/North_Dakota/New_Salem", 46.504200, -101.243900 },
    { "America/Nuuk", 64.110000, -51.440000 },
    { "America/Ojinaga", 29.340000, -104.250000 },
    { "America/Panama", 8.580000, -79.320000 },
    { "America/Paramaribo", 5.500000, -55.100000 },
    { "America/Phoenix", 33.265400, -112.042400 },
    { "America/Port-au-Prince", 18.320000, -72.200000 },
    { "America/Port_of_Spain", 10.390000, -61.310000 },
    { "America/Porto_Velho", -8.460000, -63.540000 },
    { "America/Puerto_Rico", 18.280600, -66.062200 },
    { "America/Punta_Arenas", -53.090000, -70.550000 },
    { "America/Rankin_Inlet", 62.490000, -92.045900 },
    { "America/Recife", -8.030000, -34.540000 },
    { "America/Regina", 50.240000, -104.390000 },
    { "America/Resolute", 74.414400, -94.494500 },
    { "America/Rio_Branco", -9.580000, -67.480000 },
    { "America/Santarem", -2.260000, -54.520000 },
    { "America/Santiago", -33.270000, -70.400000 },
    { "America/Santo_Domingo", 18.280000, -69.540000 },
    { "America/Sao_Paulo", -23.320000, -46.370000 },
    { "America/Scoresbysund", 70.290000, -21.580000 },
    { "America/Sitka", 57.103500, -135.180700 },
    { "America/St_Barthelemy", 17.530000, -62.510000 },
    { "America/St_Johns", 47.340000, -52.430000 },
    { "America/St_Kitts", 17.180000, -62.430000 },
    { "America/St_Lucia", 14.010000, -61.000000 },
    { "America/St_Thomas", 18.210000, -64.560000 },
    { "America/St_Vincent", 13.090000, -61.140000 },
    { "America/Swift_Current", 50.170000, -107.500000 },
    { "America/Tegucigalpa", 14.060000, -87.130000 },
    { "America/Thule", 76.340000, -68.470000 },
    { "America/Tijuana", 32.320000, -117.010000 },
    { "America/Toronto", 43.390000, -79.230000 },
    { "America/Tortola", 18.270000, -64.370000 },
    { "America/Vancouver", 49.160000, -123.070000 },
    { "America/Whitehorse", 60.430000, -135.030000 },
    { "America/Winnipeg", 49.530000, -97.090000 },
    { "America/Yakutat", 59.324900, -139.433800 },
    { "Antarctica/Casey", -66.170000, 110.310000 },
    { "Antarctica/Davis", -68.350000, 77.580000 },
    { "Antarctica/DumontDUrville", -66.400000, 140.010000 },
    { "Antarctica/Macquarie", -54.300000, 158.570000 },
    { "Antarctica/Mawson", -67.360000, 62.530000 },
    { "Antarctica/McMurdo", -77.500000, 166.360000 },
    { "Antarctica/Palmer", -64.480000, -64.060000 },
    { "Antarctica/Rothera", -67.340000, -68.080000 },
    { "Antarctica/Syowa", -69.002200, 39.352400 },
    { "Antarctica/Troll", -72.004100, 2.320600 },
    { "Antarctica/Vostok", -78.240000, 106.540000 },
    { "Arctic/Longyearbyen", 78.000000, 16.000000 },
    { "Asia/Aden", 12.450000, 45.120000 },
    { "Asia/Almaty", 43.150000, 76.570000 },
    { "Asia/Amman", 31.570000, 35.560000 },
    { "Asia/Anadyr", 64.450000, 177.290000 },
    { "Asia/Aqtau", 44.310000, 50.160000 },
    { "Asia/Aqtobe", 50.170000, 57.100000 },
    { "Asia/Ashgabat", 37.570000, 58.230000 },
    { "Asia/Atyrau", 47.070000, 51.560000 },
    { "Asia/Baghdad", 33.210000, 44.250000 },
    { "Asia/Bahrain", 26.230000, 50.350000 },
    { "Asia/Baku", 40.230000, 49.510000 },
    { "Asia/Bangkok", 13.450000, 100.310000 },
    { "Asia/Barnaul", 53.220000, 83.450000 },
    { "Asia/Beirut", 33.530000, 35.300000 },
    { "Asia/Bishkek", 42.540000, 74.360000 },
    { "Asia/Brunei", 4.560000, 114.550000 },
    { "Asia/Chita", 52.030000, 113.280000 },
    { "Asia/Colombo", 6.560000, 79.510000 },
    { "Asia/Damascus", 33.300000, 36.180000 },
    { "Asia/Dhaka", 23.430000, 90.250000 },
    { "Asia/Dili", -8.330000, 125.350000 },
    { "Asia/Dubai", 25.180000, 55.180000 },
    { "Asia/Dushanbe", 38.350000, 68.480000 },
    { "Asia/Famagusta", 35.070000, 33.570000 },
    { "Asia/Gaza", 31.300000, 34.280000 },
    { "Asia/Hebron", 31.320000, 35.054200 },
    { "Asia/Ho_Chi_Minh", 10.450000, 106.400000 },
    { "Asia/Hong_Kong", 22.170000, 114.090000 },
    { "Asia/Hovd", 48.010000, 91.390000 },
    { "Asia/Irkutsk", 52.160000, 104.200000 },
    { "Asia/Jakarta", -6.100000, 106.480000 },
    { "Asia/Jayapura", -2.320000, 140.420000 },
    { "Asia/Jerusalem", 31.465000, 35.132600 },
    { "Asia/Kabul", 34.310000, 69.120000 },
    { "Asia/Kamchatka", 53.010000, 158.390000 },
    { "Asia/Karachi", 24.520000, 67.030000 },
    { "Asia/Kathmandu", 27.430000, 85.190000 },
    { "Asia/Khandyga", 62.392300, 135.331400 },
    { "Asia/Kolkata", 22.320000, 88.220000 },
    { "Asia/Krasnoyarsk", 56.010000, 92.500000 },
    { "Asia/Kuala_Lumpur", 3.100000, 101.420000 },
    { "Asia/Kuching", 1.330000, 110.200000 },
    { "Asia/Kuwait", 29.200000, 47.590000 },
    { "Asia/Macau", 22.115000, 113.323000 },
    { "Asia/Magadan", 59.340000, 150.480000 },
    { "Asia/Makassar", -5.070000, 119.240000 },
    { "Asia/Manila", 14.351200, 120.580400 },
    { "Asia/Muscat", 23.360000, 58.350000 },
    { "Asia/Nicosia", 35.100000, 33.220000 },
    { "Asia/Novokuznetsk", 53.450000, 87.070000 },
    { "Asia/Novosibirsk", 55.020000, 82.550000 },
    { "Asia/Omsk", 55.000000, 73.240000 },
    { "Asia/Oral", 51.130000, 51.210000 },
    { "Asia/Phnom_Penh", 11.330000, 104.550000 },
    { "Asia/Pontianak", -0.020000, 109.200000 },
    { "Asia/Pyongyang", 39.010000, 125.450000 },
    { "Asia/Qatar", 25.170000, 51.320000 },
    { "Asia/Qostanay", 53.120000, 63.370000 },
    { "Asia/Qyzylorda", 44.480000, 65.280000 },
    { "Asia/Riyadh", 24.380000, 46.430000 },
    { "Asia/Sakhalin", 46.580000, 142.420000 },
    { "Asia/Samarkand", 39.400000, 66.480000 },
    { "Asia/Seoul", 37.330000, 126.580000 },
    { "Asia/Shanghai", 31.140000, 121.280000 },
    { "Asia/Singapore", 1.170000, 103.510000 },
    { "Asia/Srednekolymsk", 67.280000, 153.430000 },
    { "Asia/Taipei", 25.030000, 121.300000 },
    { "Asia/Tashkent", 41.200000, 69.180000 },
    { "Asia/Tbilisi", 41.430000, 44.490000 },
    { "Asia/Tehran", 35.400000, 51.260000 },
    { "Asia/Thimphu", 27.280000, 89.390000 },
    { "Asia/Tokyo", 35.391600, 139.444100 },
    { "Asia/Tomsk", 56.300000, 84.580000 },
    { "Asia/Ulaanbaatar", 47.550000, 106.530000 },
    { "Asia/Urumqi", 43.480000, 87.350000 },
    { "Asia/Ust-Nera", 64.333700, 143.133600 },
    { "Asia/Vientiane", 17.580000, 102.360000 },
    { "Asia/Vladivostok", 43.100000, 131.560000 },
    { "Asia/Yakutsk", 62.000000, 129.400000 },
    { "Asia/Yangon", 16.470000, 96.100000 },
    { "Asia/Yekaterinburg", 56.510000, 60.360000 },
    { "Asia/Yerevan", 40.110000, 44.300000 },
    { "Atlantic/Azores", 37.440000, -25.400000 },
    { "Atlantic/Bermuda", 32.170000, -64.460000 },
    { "Atlantic/Canary", 28.060000, -15.240000 },
    { "Atlantic/Cape_Verde", 14.550000, -23.310000 },
    { "Atlantic/Faroe", 62.010000, -6.460000 },
    { "Atlantic/Madeira", 32.380000, -16.540000 },
    { "Atlantic/Reykjavik", 64.090000, -21.510000 },
    { "Atlantic/South_Georgia", -54.160000, -36.320000 },
    { "Atlantic/St_Helena", -15.550000, -5.420000 },
    { "Atlantic/Stanley", -51.420000, -57.510000 },
    { "Australia/Adelaide", -34.550000, 138.350000 },
    { "Australia/Brisbane", -27.280000, 153.020000 },
    { "Australia/Broken_Hill", -31.570000, 141.270000 },
    { "Australia/Darwin", -12.280000, 130.500000 },
    { "Australia/Eucla", -31.430000, 128.520000 },
    { "Australia/Hobart", -42.530000, 147.190000 },
    { "Australia/Lindeman", -20.160000, 149.000000 },
    { "Australia/Lord_Howe", -31.330000, 159.050000 },
    { "Australia/Melbourne", -37.490000, 144.580000 },
    { "Australia/Perth", -31.570000, 115.510000 },
    { "Australia/Sydney", -33.520000, 151.130000 },
    { "Europe/Amsterdam", 52.220000, 4.540000 },
    { "Europe/Andorra", 42.300000, 1.310000 },
    { "Europe/Astrakhan", 46.210000, 48.030000 },
    { "Europe/Athens", 37.580000, 23.430000 },
    { "Europe/Belgrade", 44.500000, 20.300000 },
    { "Europe/Berlin", 52.300000, 13.220000 },
    { "Europe/Bratislava", 48.090000, 17.070000 },
    { "Europe/Brussels", 50.500000, 4.200000 },
    { "Europe/Bucharest", 44.260000, 26.060000 },
    { "Europe/Budapest", 47.300000, 19.050000 },
    { "Europe/Busingen", 47.420000, 8.410000 },
    { "Europe/Chisinau", 47.000000, 28.500000 },
    { "Europe/Copenhagen", 55.400000, 12.350000 },
    { "Europe/Dublin", 53.200000, -6.150000 },
    { "Europe/Gibraltar", 36.080000, -5.210000 },
    { "Europe/Guernsey", 49.271700, -2.321000 },
    { "Europe/Helsinki", 60.100000, 24.580000 },
    { "Europe/Isle_of_Man", 54.090000, -4.280000 },
    { "Europe/Istanbul", 41.010000, 28.580000 },
    { "Europe/Jersey", 49.110100, -2.062400 },
    { "Europe/Kaliningrad", 54.430000, 20.300000 },
    { "Europe/Kirov", 58.360000, 49.390000 },
    { "Europe/Kyiv", 50.260000, 30.310000 },
    { "Europe/Lisbon", 38.430000, -9.080000 },
    { "Europe/Ljubljana", 46.030000, 14.310000 },
    { "Europe/London", 51.303000, -0.073100 },
    { "Europe/Luxembourg", 49.360000, 6.090000 },
    { "Europe/Madrid", 40.240000, -3.410000 },
    { "Europe/Malta", 35.540000, 14.310000 },
    { "Europe/Mariehamn", 60.060000, 19.570000 },
    { "Europe/Minsk", 53.540000, 27.340000 },
    { "Europe/Monaco", 43.420000, 7.230000 },
    { "Europe/Moscow", 55.452100, 37.370400 },
    { "Europe/Oslo", 59.550000, 10.450000 },
    { "Europe/Paris", 48.520000, 2.200000 },
    { "Europe/Podgorica", 42.260000, 19.160000 },
    { "Europe/Prague", 50.050000, 14.260000 },
    { "Europe/Riga", 56.570000, 24.060000 },
    { "Europe/Rome", 41.540000, 12.290000 },
    { "Europe/Samara", 53.120000, 50.090000 },
    { "Europe/San_Marino", 43.550000, 12.280000 },
    { "Europe/Sarajevo", 43.520000, 18.250000 },
    { "Europe/Saratov", 51.340000, 46.020000 },
    { "Europe/Simferopol", 44.570000, 34.060000 },
    { "Europe/Skopje", 41.590000, 21.260000 },
    { "Europe/Sofia", 42.410000, 23.190000 },
    { "Europe/Stockholm", 59.200000, 18.030000 },
    { "Europe/Tallinn", 59.250000, 24.450000 },
    { "Europe/Tirane", 41.200000, 19.500000 },
    { "Europe/Ulyanovsk", 54.200000, 48.240000 },
    { "Europe/Vaduz", 47.090000, 9.310000 },
    { "Europe/Vatican", 41.540800, 12.271100 },
    { "Europe/Vienna", 48.130000, 16.200000 },
    { "Europe/Vilnius", 54.410000, 25.190000 },
    { "Europe/Volgograd", 48.440000, 44.250000 },
    { "Europe/Warsaw", 52.150000, 21.000000 },
    { "Europe/Zagreb", 45.480000, 15.580000 },
    { "Europe/Zurich", 47.230000, 8.320000 },
    { "Indian/Antananarivo", -18.550000, 47.310000 },
    { "Indian/Chagos", -7.200000, 72.250000 },
    { "Indian/Christmas", -10.250000, 105.430000 },
    { "Indian/Cocos", -12.100000, 96.550000 },
    { "Indian/Comoro", -11.410000, 43.160000 },
    { "Indian/Kerguelen", -49.211000, 70.130300 },
    { "Indian/Mahe", -4.400000, 55.280000 },
    { "Indian/Maldives", 4.100000, 73.300000 },
    { "Indian/Mauritius", -20.100000, 57.300000 },
    { "Indian/Mayotte", -12.470000, 45.140000 },
    { "Indian/Reunion", -20.520000, 55.280000 },
    { "Pacific/Apia", -13.500000, -171.440000 },
    { "Pacific/Auckland", -36.520000, 174.460000 },
    { "Pacific/Bougainville", -6.130000, 155.340000 },
    { "Pacific/Chatham", -43.570000, -176.330000 },
    { "Pacific/Chuuk", 7.250000, 151.470000 },
    { "Pacific/Easter", -27.090000, -109.260000 },
    { "Pacific/Efate", -17.400000, 168.250000 },
    { "Pacific/Fakaofo", -9.220000, -171.140000 },
    { "Pacific/Fiji", -18.080000, 178.250000 },
    { "Pacific/Funafuti", -8.310000, 179.130000 },
    { "Pacific/Galapagos", -0.540000, -89.360000 },
    { "Pacific/Gambier", -23.080000, -134.570000 },
    { "Pacific/Guadalcanal", -9.320000, 160.120000 },
    { "Pacific/Guam", 13.280000, 144.450000 },
    { "Pacific/Honolulu", 21.182500, -157.513000 },
    { "Pacific/Kanton", -2.470000, -171.430000 },
    { "Pacific/Kiritimati", 1.520000, -157.200000 },
    { "Pacific/Kosrae", 5.190000, 162.590000 },
    { "Pacific/Kwajalein", 9.050000, 167.200000 },
    { "Pacific/Majuro", 7.090000, 171.120000 },
    { "Pacific/Marquesas", -9.000000, -139.300000 },
    { "Pacific/Midway", 28.130000, -177.220000 },
    { "Pacific/Nauru", -0.310000, 166.550000 },
    { "Pacific/Niue", -19.010000, -169.550000 },
    { "Pacific/Norfolk", -29.030000, 167.580000 },
    { "Pacific/Noumea", -22.160000, 166.270000 },
    { "Pacific/Pago_Pago", -14.160000, -170.420000 },
    { "Pacific/Palau", 7.200000, 134.290000 },
    { "Pacific/Pitcairn", -25.040000, -130.050000 },
    { "Pacific/Pohnpei", 6.580000, 158.130000 },
    { "Pacific/Port_Moresby", -9.300000, 147.100000 },
    { "Pacific/Rarotonga", -21.140000, -159.460000 },
    { "Pacific/Saipan", 15.120000, 145.450000 },
    { "Pacific/Tahiti", -17.320000, -149.340000 },
    { "Pacific/Tarawa", 1.250000, 173.000000 },
    { "Pacific/Tongatapu", -21.080000, -175.120000 },
    { "Pacific/Wake", 19.170000, 166.370000 },
    { "Pacific/Wallis", -13.180000, -176.100000 },
};