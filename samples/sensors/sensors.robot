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

    Wait For Line on UART   0_0,0_1,0_2,1_0,1_1,1_2
    Wait For Line on UART   0.498903,0.498903,0.498903,2.499303,2.499303,2.499303
    Wait For Line on UART   0.197407,0.498903,0.996610,2.196611,2.499303,2.997010
    Wait For Line on UART   -0.502492,0.096909,0.498903,0.000000,4.997410,0.000000
    Wait For Line on UART   -4.999803,0.000000,4.997410,0.000000,0.000000,0.000000
