SNMP_OPTIONS_NO_M="-On -c public -v 2c "
# be sure you have run download-mibs to download MIBs
SNMP_OPTIONS="$SNMP_OPTIONS_NO_M -m WR-WRPC-MIB -M +/var/lib/mibs/ietf:../../lib"
TOTAL_NUM_OIDS_EXPECT_TEXT="4 temperature sensors, 4 entries in the SFPs database"
# number of OIDs expected
TOTAL_NUM_OIDS=69
