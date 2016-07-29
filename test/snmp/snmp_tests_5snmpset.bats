load snmp_test_config
load snmp_test_helpers

@test "set wrpcPtpConfigDeltaTx" {
  helper_snmpset wrpcPtpConfigDeltaTx.0 141415
  helper_snmpget wrpcPtpConfigDeltaTx.0 141415
}

@test "set read only wrpcSpllHlock" {
  run snmpset $SNMP_OPTIONS $TARGET_IP wrpcSpllHlock.0 = 141415
  [ "$status" -eq 2 ]
}

@test "set wrpcPtpConfigDeltaTx with two different values" {
  helper_snmpset wrpcPtpConfigDeltaTx.0 141415
  helper_snmpget wrpcPtpConfigDeltaTx.0 141415
  helper_snmpset wrpcPtpConfigDeltaTx.0 151516
  helper_snmpget wrpcPtpConfigDeltaTx.0 151516
}

@test "set wrpcPtpConfigDeltaRx with two different values" {
  helper_snmpset wrpcPtpConfigDeltaRx.0 141416
  helper_snmpget wrpcPtpConfigDeltaRx.0 141416
  helper_snmpset wrpcPtpConfigDeltaRx.0 151517
  helper_snmpget wrpcPtpConfigDeltaRx.0 151517
}

@test "set wrpcPtpConfigAlpha with two different values" {
  helper_snmpset wrpcPtpConfigAlpha.0 141417
  helper_snmpget wrpcPtpConfigAlpha.0 141417
  helper_snmpset wrpcPtpConfigAlpha.0 151518
  helper_snmpget wrpcPtpConfigAlpha.0 151518
}

@test "set wrpcPtpConfigSfpPn with two different values" {
  helper_snmpset wrpcPtpConfigSfpPn.0 "TEST sfp"
  helper_snmpget wrpcPtpConfigSfpPn.0 "TEST sfp"
  helper_snmpset wrpcPtpConfigSfpPn.0 "sfp other"
  helper_snmpget wrpcPtpConfigSfpPn.0 "sfp other"
}

@test "set wrpcPtpConfigSfpPn with empty value" {
  helper_snmpset wrpcPtpConfigSfpPn.0 ""
  helper_snmpget wrpcPtpConfigSfpPn.0 ""
}

@test "set wrpcPtpConfigSfpPn with too long value (error from host)" {
  # set known value first
  helper_snmpset wrpcPtpConfigSfpPn.0 "TEST sfp1"
  # set too long, bad length (error from snmpget program)
  run snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigSfpPn.0 = "0123456789012345678"
  [ "$status" -eq 1 ]
  # expect value to be not changed
  helper_snmpget wrpcPtpConfigSfpPn.0 "TEST sfp1"
}

@test "set wrpcPtpConfigSfpPn with too long value (erorr from target)" {
  # set known value first
  helper_snmpset wrpcPtpConfigSfpPn.0 "TEST sfp2"
  # set too long, bad length (erorr from target)
  run snmpset $SNMP_OPTIONS_NO_M $TARGET_IP .1.3.6.1.4.1.96.101.1.6.3.0 s "0123456789012345678"
  echo $status
  [ "$status" -eq 2 ]
  # expect value to be not changed
  helper_snmpget wrpcPtpConfigSfpPn.0 "TEST sfp2"
}

@test "set wrong type of wrpcPtpConfigSfpPn" {
  # set known value first
  helper_snmpset wrpcPtpConfigSfpPn.0 "TEST sfp3"
  # set too long, bad length (erorr from target)
  run snmpset $SNMP_OPTIONS_NO_M $TARGET_IP .1.3.6.1.4.1.96.101.1.6.3.0 i "012345678"
  echo $status
  [ "$status" -eq 2 ]
  # expect value to be not changed
  helper_snmpget wrpcPtpConfigSfpPn.0 "TEST sfp3"
}

@test "erase sfp database" {
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = eraseFlash | grep applySuccessful | wc -l)"
  [ "$result" -eq 1 ]
}

@test "erase sfp database test helper" {
  helper_erase_sfp_database
}

@test "add sfp with invalid PN to the database" {
  # Empty PN is invalid

  # erase database first
  helper_erase_sfp_database
  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "11112"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "11113"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "11114"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 ""
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep "applyFailedInvalidPN" | wc -l)"
  [ "$result" -eq 1 ]
}

@test "add sfp to the database" {
  # erase database first
  helper_erase_sfp_database
  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 1234
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 4343
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 1258
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashCurrentSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]
}


@test "add 4 sfps to the database" {
  # following entries should be written to the database
  # unfortunately it is not possible to verify these entries now via snmp
  # sfp show
  # 1: PN:test PN1         dTx:    11112 dRx:    11113 alpha:    11114
  # 2: PN:test PN2         dTx:    22223 dRx:    22224 alpha:    22225
  # 3: PN:test PN3         dTx:    33334 dRx:    33335 alpha:    33336
  # 4: PN:test PN4         dTx:    44445 dRx:    44446 alpha:    44447

  # erase database first
  helper_erase_sfp_database
  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "11112"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "11113"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "11114"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN1"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  echo $result
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "22223"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "22224"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "22225"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN2"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "33334"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "33335"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "33336"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN3"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "44445"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "44446"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "44447"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN4"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]
}


@test "add 5 sfps to the database" {
  # following entries should be written to the database, 5th should generate error
  # unfortunately it is not possible to verify these entries now via snmp
  # sfp show
  # 1: PN:test PN1         dTx:    11112 dRx:    11113 alpha:    11114
  # 2: PN:test PN2         dTx:    22223 dRx:    22224 alpha:    22225
  # 3: PN:test PN3         dTx:    33334 dRx:    33335 alpha:    33336
  # 4: PN:test PN4         dTx:    44445 dRx:    44446 alpha:    44447

  # erase database first
  helper_erase_sfp_database
  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "11112"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "11113"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "11114"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN1"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "22223"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "22224"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "22225"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN2"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "33334"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "33335"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "33336"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN3"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "44445"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "44446"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "44447"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN4"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "55556"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "55557"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "55558"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN5"
  # add sfp to the database, it is full now
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep "applyFailedDBFull" | wc -l)"
  [ "$result" -eq 1 ]
}


@test "add 4 sfps to the database, test replacement" {
  # following entries should be written to the database, 5th should replace second entry
  # unfortunately it is not possible to verify these entries now via snmp
  # sfp show
  # 1: PN:test PN1         dTx:    11112 dRx:    11113 alpha:    11114
  # 2: PN:test PN2         dTx:    22223 dRx:    22224 alpha:    22225
  # 3: PN:test PN3         dTx:    33334 dRx:    33335 alpha:    33336
  # 4: PN:test PN4         dTx:    44445 dRx:    44446 alpha:    44447

  # erase database first
  helper_erase_sfp_database
  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "11112"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "11113"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "11114"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN1"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "22223"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "22224"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "22225"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN2"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "33334"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "33335"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "33336"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN3"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "44445"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "44446"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "44447"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN4"
  # add sfp to the database, we don't care if match was successful
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]

  #set delta TX for SFP
  helper_snmpset wrpcPtpConfigDeltaTx.0 "99999"
  #set delta RX for SFP
  helper_snmpset wrpcPtpConfigDeltaRx.0 "99999"
  #set delta Alpha for SFP
  helper_snmpset wrpcPtpConfigAlpha.0 "99999"
  #set PN of SFP
  helper_snmpset wrpcPtpConfigSfpPn.0 "test PN2"
  # add sfp to the database, it is full now
  result="$(snmpset $SNMP_OPTIONS $TARGET_IP wrpcPtpConfigApply.0 = writeToFlashGivenSfp | grep -e "applySuccessful" -e "applySuccessfulMatchFailed" | wc -l)"
  [ "$result" -eq 1 ]
}

