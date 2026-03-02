*** Settings ***
Resource			${KEYWORDS}

*** Test Cases ***
Should Display Sensors Test
    Prepare Machine

    Execute Command    machine LoadPlatformDescriptionFromString "lis2ds12_1: Sensors.LIS2DS12 @ i2c1 0x3d"
    Execute Command    machine LoadPlatformDescriptionFromString "lis2ds12_2: Sensors.LIS2DS12 @ i2c1 0x2d"

    Execute Command    sysbus.i2c1.lis2ds12_1 FeedSample @${CURDIR}${/}input1.data -1
    Execute Command    sysbus.i2c1.lis2ds12_2 FeedSample @${CURDIR}${/}input2.data -1

    Start Emulation

    Wait For Line on UART    Anomaly detected with probability 0.99963
    Wait For Line on UART    Anomaly detected with probability 1.00000
    Wait For Line on UART    Anomaly detected with probability 0.99963
    Wait For Line on UART    Anomaly detected with probability 1.00000
    Wait For Line on UART    Successfully shut down classifier
