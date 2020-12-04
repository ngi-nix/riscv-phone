EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A3 16535 11693
encoding utf-8
Sheet 3 4
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L myPower:+VSYS #PWR0301
U 1 1 5C8F0FDD
P 3100 700
F 0 "#PWR0301" H 3100 550 50  0001 C CNN
F 1 "+VSYS" H 3100 840 50  0000 C CNN
F 2 "" H 3100 700 50  0001 C CNN
F 3 "" H 3100 700 50  0001 C CNN
	1    3100 700 
	1    0    0    -1  
$EndComp
Wire Wire Line
	3850 850  4150 850 
Wire Wire Line
	3100 700  3100 1200
Wire Wire Line
	4450 850  4750 850 
Connection ~ 3100 1200
Wire Wire Line
	5400 850  5700 850 
Connection ~ 3100 2100
Wire Wire Line
	6000 850  6300 850 
Connection ~ 3100 3000
Wire Wire Line
	6600 850  6900 850 
Connection ~ 3100 3900
Wire Wire Line
	7200 850  7500 850 
Connection ~ 3100 4600
Wire Wire Line
	8150 850  8450 850 
Connection ~ 3100 5500
Wire Wire Line
	9100 850  9400 850 
Connection ~ 3100 6400
Wire Wire Line
	10050 850  10350 850 
Connection ~ 3100 7300
Wire Wire Line
	10650 850  10950 850 
Connection ~ 3100 8200
Wire Wire Line
	11600 850  11900 850 
Connection ~ 3100 9100
Wire Wire Line
	3000 10900 3100 10900
Connection ~ 3100 10000
$Comp
L power:GND #PWR0333
U 1 1 5C8F1120
P 1400 11000
F 0 "#PWR0333" H 1400 10750 50  0001 C CNN
F 1 "GND" H 1400 10850 50  0000 C CNN
F 2 "" H 1400 11000 50  0001 C CNN
F 3 "" H 1400 11000 50  0001 C CNN
	1    1400 11000
	1    0    0    -1  
$EndComp
Wire Wire Line
	1500 800  1400 800 
Wire Wire Line
	1400 800  1400 1700
Wire Wire Line
	1500 1700 1400 1700
Connection ~ 1400 1700
Wire Wire Line
	1500 2600 1400 2600
Connection ~ 1400 2600
Wire Wire Line
	1500 3500 1400 3500
Connection ~ 1400 3500
Wire Wire Line
	1500 4300 1400 4300
Connection ~ 1400 4300
Wire Wire Line
	1500 5100 1400 5100
Connection ~ 1400 5100
Wire Wire Line
	1500 6000 1400 6000
Connection ~ 1400 6000
Wire Wire Line
	1500 6900 1400 6900
Connection ~ 1400 6900
Wire Wire Line
	1500 7800 1400 7800
Connection ~ 1400 7800
Wire Wire Line
	1500 8700 1400 8700
Connection ~ 1400 8700
Wire Wire Line
	1500 9600 1400 9600
Connection ~ 1400 9600
Wire Wire Line
	1500 9900 1100 9900
Wire Wire Line
	1100 9900 1100 10000
$Comp
L Device:C C301
U 1 1 5C8F1372
P 1100 10150
F 0 "C301" H 1125 10250 50  0000 L CNN
F 1 "4.7uF" H 1125 10050 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 1138 10000 50  0001 C CNN
F 3 "" H 1100 10150 50  0001 C CNN
	1    1100 10150
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0331
U 1 1 5C8F1395
P 1100 10400
F 0 "#PWR0331" H 1100 10150 50  0001 C CNN
F 1 "GND" H 1100 10250 50  0000 C CNN
F 2 "" H 1100 10400 50  0001 C CNN
F 3 "" H 1100 10400 50  0001 C CNN
	1    1100 10400
	1    0    0    -1  
$EndComp
Wire Wire Line
	1100 10300 1100 10400
$Comp
L Device:C C302
U 1 1 5C8F15E6
P 4150 1050
F 0 "C302" H 4175 1150 50  0000 L CNN
F 1 "0.1uF" H 4175 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4188 900 50  0001 C CNN
F 3 "" H 4150 1050 50  0001 C CNN
	1    4150 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C303
U 1 1 5C8F160D
P 4750 1050
F 0 "C303" H 4775 1150 50  0000 L CNN
F 1 "0.1uF" H 4775 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 4788 900 50  0001 C CNN
F 3 "" H 4750 1050 50  0001 C CNN
	1    4750 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C304
U 1 1 5C8F1635
P 5700 1050
F 0 "C304" H 5725 1150 50  0000 L CNN
F 1 "0.1uF" H 5725 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 5738 900 50  0001 C CNN
F 3 "" H 5700 1050 50  0001 C CNN
	1    5700 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C305
U 1 1 5C8F168B
P 6300 1050
F 0 "C305" H 6325 1150 50  0000 L CNN
F 1 "0.1uF" H 6325 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 6338 900 50  0001 C CNN
F 3 "" H 6300 1050 50  0001 C CNN
	1    6300 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C306
U 1 1 5C8F16BB
P 6900 1050
F 0 "C306" H 6925 1150 50  0000 L CNN
F 1 "0.1uF" H 6925 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 6938 900 50  0001 C CNN
F 3 "" H 6900 1050 50  0001 C CNN
	1    6900 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C307
U 1 1 5C8F16F2
P 7500 1050
F 0 "C307" H 7525 1150 50  0000 L CNN
F 1 "0.1uF" H 7525 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 7538 900 50  0001 C CNN
F 3 "" H 7500 1050 50  0001 C CNN
	1    7500 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C308
U 1 1 5C8F175A
P 8450 1050
F 0 "C308" H 8475 1150 50  0000 L CNN
F 1 "0.1uF" H 8475 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 8488 900 50  0001 C CNN
F 3 "" H 8450 1050 50  0001 C CNN
	1    8450 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C309
U 1 1 5C8F17BA
P 9400 1050
F 0 "C309" H 9425 1150 50  0000 L CNN
F 1 "0.1uF" H 9425 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 9438 900 50  0001 C CNN
F 3 "" H 9400 1050 50  0001 C CNN
	1    9400 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C310
U 1 1 5C8F180E
P 10350 1050
F 0 "C310" H 10375 1150 50  0000 L CNN
F 1 "0.1uF" H 10375 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 10388 900 50  0001 C CNN
F 3 "" H 10350 1050 50  0001 C CNN
	1    10350 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C311
U 1 1 5C8F1852
P 10950 1050
F 0 "C311" H 10975 1150 50  0000 L CNN
F 1 "0.1uF" H 10975 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 10988 900 50  0001 C CNN
F 3 "" H 10950 1050 50  0001 C CNN
	1    10950 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C312
U 1 1 5C8F189B
P 11900 1050
F 0 "C312" H 11925 1150 50  0000 L CNN
F 1 "0.1uF" H 11925 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 11938 900 50  0001 C CNN
F 3 "" H 11900 1050 50  0001 C CNN
	1    11900 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C313
U 1 1 5C8F18E5
P 12600 1050
F 0 "C313" H 12625 1150 50  0000 L CNN
F 1 "0.1uF" H 12625 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 12638 900 50  0001 C CNN
F 3 "" H 12600 1050 50  0001 C CNN
	1    12600 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C314
U 1 1 5C8F1C4F
P 5100 1050
F 0 "C314" H 5125 1150 50  0000 L CNN
F 1 "22uF" H 5125 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 5138 900 50  0001 C CNN
F 3 "" H 5100 1050 50  0001 C CNN
	1    5100 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C315
U 1 1 5C8F1D0B
P 7850 1050
F 0 "C315" H 7875 1150 50  0000 L CNN
F 1 "22uF" H 7875 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 7888 900 50  0001 C CNN
F 3 "" H 7850 1050 50  0001 C CNN
	1    7850 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C316
U 1 1 5C8F1D61
P 8800 1050
F 0 "C316" H 8825 1150 50  0000 L CNN
F 1 "22uF" H 8825 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 8838 900 50  0001 C CNN
F 3 "" H 8800 1050 50  0001 C CNN
	1    8800 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C317
U 1 1 5C8F1DBA
P 9750 1050
F 0 "C317" H 9775 1150 50  0000 L CNN
F 1 "22uF" H 9775 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 9788 900 50  0001 C CNN
F 3 "" H 9750 1050 50  0001 C CNN
	1    9750 1050
	1    0    0    -1  
$EndComp
$Comp
L Device:C C318
U 1 1 5C8F1E4E
P 11300 1050
F 0 "C318" H 11325 1150 50  0000 L CNN
F 1 "22uF" H 11325 950 50  0000 L CNN
F 2 "Capacitor_SMD:C_0603_1608Metric" H 11338 900 50  0001 C CNN
F 3 "" H 11300 1050 50  0001 C CNN
	1    11300 1050
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0308
U 1 1 5C8F2209
P 12600 1250
F 0 "#PWR0308" H 12600 1000 50  0001 C CNN
F 1 "GND" H 12600 1100 50  0000 C CNN
F 2 "" H 12600 1250 50  0001 C CNN
F 3 "" H 12600 1250 50  0001 C CNN
	1    12600 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0307
U 1 1 5C8F2381
P 11900 1250
F 0 "#PWR0307" H 11900 1000 50  0001 C CNN
F 1 "GND" H 11900 1100 50  0000 C CNN
F 2 "" H 11900 1250 50  0001 C CNN
F 3 "" H 11900 1250 50  0001 C CNN
	1    11900 1250
	1    0    0    -1  
$EndComp
Wire Wire Line
	12600 900  12600 850 
Wire Wire Line
	11900 1250 11900 1200
Wire Wire Line
	11900 850  11900 900 
Wire Wire Line
	11300 850  11300 900 
Wire Wire Line
	10950 900  10950 850 
Connection ~ 10950 850 
$Comp
L power:GND #PWR0313
U 1 1 5C8F26B6
P 10950 1300
F 0 "#PWR0313" H 10950 1050 50  0001 C CNN
F 1 "GND" H 10950 1150 50  0000 C CNN
F 2 "" H 10950 1300 50  0001 C CNN
F 3 "" H 10950 1300 50  0001 C CNN
	1    10950 1300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0306
U 1 1 5C8F2788
P 10350 1250
F 0 "#PWR0306" H 10350 1000 50  0001 C CNN
F 1 "GND" H 10350 1100 50  0000 C CNN
F 2 "" H 10350 1250 50  0001 C CNN
F 3 "" H 10350 1250 50  0001 C CNN
	1    10350 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0312
U 1 1 5C8F27E8
P 9400 1300
F 0 "#PWR0312" H 9400 1050 50  0001 C CNN
F 1 "GND" H 9400 1150 50  0000 C CNN
F 2 "" H 9400 1300 50  0001 C CNN
F 3 "" H 9400 1300 50  0001 C CNN
	1    9400 1300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0310
U 1 1 5C8F28E0
P 7500 1300
F 0 "#PWR0310" H 7500 1050 50  0001 C CNN
F 1 "GND" H 7500 1150 50  0000 C CNN
F 2 "" H 7500 1300 50  0001 C CNN
F 3 "" H 7500 1300 50  0001 C CNN
	1    7500 1300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0305
U 1 1 5C8F2940
P 6900 1250
F 0 "#PWR0305" H 6900 1000 50  0001 C CNN
F 1 "GND" H 6900 1100 50  0000 C CNN
F 2 "" H 6900 1250 50  0001 C CNN
F 3 "" H 6900 1250 50  0001 C CNN
	1    6900 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0304
U 1 1 5C8F29FF
P 6300 1250
F 0 "#PWR0304" H 6300 1000 50  0001 C CNN
F 1 "GND" H 6300 1100 50  0000 C CNN
F 2 "" H 6300 1250 50  0001 C CNN
F 3 "" H 6300 1250 50  0001 C CNN
	1    6300 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0303
U 1 1 5C8F2A5F
P 5700 1250
F 0 "#PWR0303" H 5700 1000 50  0001 C CNN
F 1 "GND" H 5700 1100 50  0000 C CNN
F 2 "" H 5700 1250 50  0001 C CNN
F 3 "" H 5700 1250 50  0001 C CNN
	1    5700 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0309
U 1 1 5C8F2ABF
P 4750 1300
F 0 "#PWR0309" H 4750 1050 50  0001 C CNN
F 1 "GND" H 4750 1150 50  0000 C CNN
F 2 "" H 4750 1300 50  0001 C CNN
F 3 "" H 4750 1300 50  0001 C CNN
	1    4750 1300
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0302
U 1 1 5C8F2B1F
P 4150 1250
F 0 "#PWR0302" H 4150 1000 50  0001 C CNN
F 1 "GND" H 4150 1100 50  0000 C CNN
F 2 "" H 4150 1250 50  0001 C CNN
F 3 "" H 4150 1250 50  0001 C CNN
	1    4150 1250
	1    0    0    -1  
$EndComp
$Comp
L power:GND #PWR0311
U 1 1 5C8F2E11
P 8450 1300
F 0 "#PWR0311" H 8450 1050 50  0001 C CNN
F 1 "GND" H 8450 1150 50  0000 C CNN
F 2 "" H 8450 1300 50  0001 C CNN
F 3 "" H 8450 1300 50  0001 C CNN
	1    8450 1300
	1    0    0    -1  
$EndComp
Wire Wire Line
	8450 1200 8450 1250
Wire Wire Line
	9400 1200 9400 1250
Wire Wire Line
	10350 1200 10350 1250
Wire Wire Line
	10950 1200 10950 1250
Wire Wire Line
	11300 1200 11300 1250
Wire Wire Line
	11300 1250 10950 1250
Connection ~ 10950 1250
Wire Wire Line
	12600 1200 12600 1250
Wire Wire Line
	10350 850  10350 900 
Wire Wire Line
	9400 850  9400 900 
Wire Wire Line
	9750 850  9750 900 
Connection ~ 9400 850 
Wire Wire Line
	9750 1200 9750 1250
Wire Wire Line
	9750 1250 9400 1250
Connection ~ 9400 1250
Wire Wire Line
	8450 850  8450 900 
Wire Wire Line
	8800 850  8800 900 
Connection ~ 8450 850 
Wire Wire Line
	8800 1200 8800 1250
Wire Wire Line
	8800 1250 8450 1250
Connection ~ 8450 1250
Wire Wire Line
	7500 1200 7500 1250
Wire Wire Line
	7850 1200 7850 1250
Wire Wire Line
	7850 1250 7500 1250
Connection ~ 7500 1250
Wire Wire Line
	7500 850  7500 900 
Wire Wire Line
	7850 850  7850 900 
Connection ~ 7500 850 
Wire Wire Line
	6900 850  6900 900 
Wire Wire Line
	6900 1200 6900 1250
Wire Wire Line
	6300 1200 6300 1250
Wire Wire Line
	6300 850  6300 900 
Wire Wire Line
	5700 850  5700 900 
Wire Wire Line
	5700 1250 5700 1200
Wire Wire Line
	4750 1300 4750 1250
Wire Wire Line
	5100 1200 5100 1250
Wire Wire Line
	5100 1250 4750 1250
Connection ~ 4750 1250
Wire Wire Line
	4750 850  4750 900 
Wire Wire Line
	5100 850  5100 900 
Connection ~ 4750 850 
Wire Wire Line
	4150 850  4150 900 
Wire Wire Line
	4150 1200 4150 1250
NoConn ~ 1500 900 
NoConn ~ 1500 1800
NoConn ~ 1500 1900
NoConn ~ 1500 2000
NoConn ~ 1500 2100
NoConn ~ 1500 2200
NoConn ~ 1500 2300
NoConn ~ 1500 2900
NoConn ~ 1500 3200
NoConn ~ 3000 800 
NoConn ~ 3000 900 
NoConn ~ 3000 1000
NoConn ~ 3000 1100
NoConn ~ 3000 1300
NoConn ~ 3000 1400
NoConn ~ 3000 1500
NoConn ~ 3000 1600
NoConn ~ 3000 1700
NoConn ~ 3000 1800
NoConn ~ 3000 1900
NoConn ~ 3000 2000
NoConn ~ 3000 2200
NoConn ~ 3000 2300
NoConn ~ 3000 2400
NoConn ~ 3000 2500
NoConn ~ 3000 2600
NoConn ~ 3000 2700
NoConn ~ 3000 2800
NoConn ~ 3000 2900
NoConn ~ 3000 3100
NoConn ~ 3000 3200
NoConn ~ 3000 3300
NoConn ~ 3000 3400
NoConn ~ 3000 3500
NoConn ~ 1500 3800
NoConn ~ 1500 3900
NoConn ~ 1500 4000
NoConn ~ 1500 4100
NoConn ~ 1500 4200
NoConn ~ 1500 4400
NoConn ~ 3000 4500
NoConn ~ 3000 4400
NoConn ~ 3000 4300
NoConn ~ 3000 4200
NoConn ~ 3000 4100
NoConn ~ 3000 4000
NoConn ~ 3000 3800
NoConn ~ 3000 4700
NoConn ~ 3000 4800
NoConn ~ 3000 4900
NoConn ~ 3000 5300
NoConn ~ 3000 5400
NoConn ~ 1500 4600
NoConn ~ 1500 4700
NoConn ~ 1500 4800
NoConn ~ 1500 4900
NoConn ~ 1500 5000
NoConn ~ 1500 5200
NoConn ~ 1500 5300
NoConn ~ 1500 5400
NoConn ~ 1500 5500
NoConn ~ 1500 5600
NoConn ~ 1500 5700
NoConn ~ 1500 5800
NoConn ~ 1500 5900
NoConn ~ 1500 6100
NoConn ~ 1500 6200
NoConn ~ 1500 6300
NoConn ~ 1500 6400
NoConn ~ 3000 5600
NoConn ~ 3000 5700
NoConn ~ 3000 5800
NoConn ~ 3000 5900
NoConn ~ 3000 6000
NoConn ~ 3000 6100
NoConn ~ 3000 6200
NoConn ~ 3000 6300
NoConn ~ 3000 6500
NoConn ~ 3000 6600
NoConn ~ 3000 6700
NoConn ~ 3000 6800
NoConn ~ 3000 6900
NoConn ~ 3000 7000
NoConn ~ 3000 7100
NoConn ~ 3000 7200
NoConn ~ 3000 7400
NoConn ~ 3000 7500
NoConn ~ 3000 7600
NoConn ~ 3000 7700
NoConn ~ 3000 7800
NoConn ~ 3000 7900
NoConn ~ 3000 8000
NoConn ~ 3000 8100
NoConn ~ 3000 8300
NoConn ~ 3000 8500
NoConn ~ 3000 8600
NoConn ~ 3000 8700
NoConn ~ 3000 8800
NoConn ~ 3000 8900
NoConn ~ 3000 9000
NoConn ~ 3000 9200
NoConn ~ 3000 9300
NoConn ~ 3000 9400
NoConn ~ 3000 9700
NoConn ~ 3000 9800
NoConn ~ 3000 9900
NoConn ~ 3000 10100
NoConn ~ 3000 10200
NoConn ~ 3000 10300
NoConn ~ 3000 10400
NoConn ~ 3000 10500
NoConn ~ 3000 10600
NoConn ~ 3000 10800
NoConn ~ 1500 10900
NoConn ~ 1500 10800
NoConn ~ 1500 10700
NoConn ~ 1500 10600
NoConn ~ 1500 10500
NoConn ~ 1500 10400
NoConn ~ 1500 10300
NoConn ~ 1500 10200
NoConn ~ 1500 10100
NoConn ~ 1500 10000
NoConn ~ 1500 9800
NoConn ~ 1500 9700
NoConn ~ 1500 9500
NoConn ~ 1500 9400
NoConn ~ 1500 9300
NoConn ~ 1500 9200
NoConn ~ 1500 9100
NoConn ~ 1500 9000
NoConn ~ 1500 8900
NoConn ~ 1500 8800
NoConn ~ 1500 8600
NoConn ~ 1500 8100
NoConn ~ 1500 8000
NoConn ~ 1500 7900
NoConn ~ 1500 7700
NoConn ~ 1500 7600
NoConn ~ 1500 7400
NoConn ~ 1500 7300
NoConn ~ 1500 7100
NoConn ~ 1500 7000
NoConn ~ 1500 6800
NoConn ~ 1500 6700
NoConn ~ 1500 6600
NoConn ~ 1500 6500
Text GLabel 1300 1300 0    55   BiDi ~ 0
ESP32.HSPI.MOSI
Wire Wire Line
	1300 1300 1500 1300
Text GLabel 1300 1400 0    55   BiDi ~ 0
ESP32.HSPI.SCK
Text GLabel 1300 1500 0    55   BiDi ~ 0
ESP32.HSPI.MISO
Text GLabel 1300 1600 0    55   BiDi ~ 0
ESP32.HSPI.SS0
Wire Wire Line
	1300 1600 1500 1600
Wire Wire Line
	1300 1500 1500 1500
Wire Wire Line
	1300 1400 1500 1400
Wire Wire Line
	3000 10000 3100 10000
Wire Wire Line
	3000 9100 3100 9100
Wire Wire Line
	3000 8200 3100 8200
Wire Wire Line
	3000 7300 3100 7300
Wire Wire Line
	3000 6400 3100 6400
Wire Wire Line
	3000 4600 3100 4600
Wire Wire Line
	3000 3900 3100 3900
Wire Wire Line
	3000 3000 3100 3000
Wire Wire Line
	3000 2100 3100 2100
Wire Wire Line
	3000 1200 3100 1200
Text Label 3800 1200 2    55   ~ 0
VSYS1
Text Label 3800 2100 2    55   ~ 0
VSYS2
Text Label 3800 3000 2    55   ~ 0
VSYS3
Text Label 3800 3900 2    55   ~ 0
VSYS4
Text Label 3800 4600 2    55   ~ 0
VSYS5
Text Label 3800 5500 2    55   ~ 0
VSYS6
Text Label 3800 6400 2    55   ~ 0
VSYS7
Text Label 3800 7300 2    55   ~ 0
VSYS8
Text Label 3800 8200 2    55   ~ 0
VSYS9
Text Label 3800 9100 2    55   ~ 0
VSYS10
Text Label 3800 10000 2    55   ~ 0
VSYS11
Text Label 3800 10900 2    55   ~ 0
VSYS12
Connection ~ 3100 10900
Wire Wire Line
	3000 5500 3100 5500
Text Label 3850 850  0    55   ~ 0
VSYS1
Text Label 4450 850  0    55   ~ 0
VSYS2
Text Label 5400 850  0    55   ~ 0
VSYS3
Text Label 6000 850  0    55   ~ 0
VSYS4
Text Label 6600 850  0    55   ~ 0
VSYS5
Text Label 7200 850  0    55   ~ 0
VSYS6
Text Label 8150 850  0    55   ~ 0
VSYS7
Text Label 9100 850  0    55   ~ 0
VSYS8
Text Label 10050 850  0    55   ~ 0
VSYS9
Text Label 10650 850  0    55   ~ 0
VSYS10
Text Label 11600 850  0    55   ~ 0
VSYS11
Text Label 12300 850  0    55   ~ 0
VSYS12
Wire Wire Line
	12600 850  12300 850 
Text GLabel 1300 1100 0    55   Output ~ 0
ESP32.IO36
Text GLabel 1300 1200 0    55   BiDi ~ 0
ESP32.IO27
Wire Wire Line
	1300 1200 1500 1200
Wire Wire Line
	1300 1100 1500 1100
Wire Wire Line
	3100 1200 3100 2100
Wire Wire Line
	3100 1200 3800 1200
Wire Wire Line
	3100 2100 3800 2100
Wire Wire Line
	3100 2100 3100 3000
Wire Wire Line
	3100 3000 3800 3000
Wire Wire Line
	3100 3000 3100 3900
Wire Wire Line
	3100 3900 3800 3900
Wire Wire Line
	3100 3900 3100 4600
Wire Wire Line
	3100 4600 3800 4600
Wire Wire Line
	3100 4600 3100 5500
Wire Wire Line
	3100 5500 3800 5500
Wire Wire Line
	3100 5500 3100 6400
Wire Wire Line
	3100 6400 3800 6400
Wire Wire Line
	3100 6400 3100 7300
Wire Wire Line
	3100 7300 3800 7300
Wire Wire Line
	3100 7300 3100 8200
Wire Wire Line
	3100 8200 3800 8200
Wire Wire Line
	3100 8200 3100 9100
Wire Wire Line
	3100 9100 3800 9100
Wire Wire Line
	3100 9100 3100 10000
Wire Wire Line
	3100 10000 3800 10000
Wire Wire Line
	3100 10000 3100 10900
Wire Wire Line
	1400 1700 1400 2600
Wire Wire Line
	1400 2600 1400 3500
Wire Wire Line
	1400 3500 1400 4300
Wire Wire Line
	1400 4300 1400 5100
Wire Wire Line
	1400 5100 1400 6000
Wire Wire Line
	1400 6000 1400 6900
Wire Wire Line
	1400 6900 1400 7800
Wire Wire Line
	1400 7800 1400 8700
Wire Wire Line
	1400 8700 1400 9600
Wire Wire Line
	1400 9600 1400 11000
Wire Wire Line
	10950 850  11300 850 
Wire Wire Line
	10950 1250 10950 1300
Wire Wire Line
	9400 850  9750 850 
Wire Wire Line
	9400 1250 9400 1300
Wire Wire Line
	8450 850  8800 850 
Wire Wire Line
	8450 1250 8450 1300
Wire Wire Line
	7500 1250 7500 1300
Wire Wire Line
	7500 850  7850 850 
Wire Wire Line
	4750 1250 4750 1200
Wire Wire Line
	4750 850  5100 850 
Wire Wire Line
	3100 10900 3800 10900
Text GLabel 1300 2500 0    55   Output ~ 0
iMX8.LVDS0.CLK_N
Text GLabel 1300 2400 0    55   Output ~ 0
iMX8.LVDS0.CLK_P
Text GLabel 1300 2700 0    55   Output ~ 0
iMX8.LVDS0.A0_P
Text GLabel 1300 2800 0    55   Output ~ 0
iMX8.LVDS0.A0_N
Text GLabel 1300 3000 0    55   Output ~ 0
iMX8.LVDS0.A1_P
Text GLabel 1300 3100 0    55   Output ~ 0
iMX8.LVDS0.A1_N
Text GLabel 1300 3300 0    55   Output ~ 0
iMX8.LVDS0.A2_P
Text GLabel 1300 3400 0    55   Output ~ 0
iMX8.LVDS0.A2_N
Text GLabel 1300 3600 0    55   Output ~ 0
iMX8.LVDS0.A3_P
Text GLabel 1300 3700 0    55   Output ~ 0
iMX8.LVDS0.A3_N
Wire Wire Line
	1300 3400 1500 3400
Wire Wire Line
	1300 3300 1500 3300
Wire Wire Line
	1300 3100 1500 3100
Wire Wire Line
	1300 3000 1500 3000
Wire Wire Line
	1300 2800 1500 2800
Wire Wire Line
	1300 2700 1500 2700
Wire Wire Line
	1300 2500 1500 2500
Wire Wire Line
	1300 2400 1500 2400
Text GLabel 1300 7200 0    55   Output ~ 0
iMX8.I2C1.SCL
Text GLabel 1300 7500 0    55   BiDi ~ 0
iMX8.I2C1.SDA
Wire Wire Line
	1300 7500 1500 7500
Wire Wire Line
	1300 7200 1500 7200
Text GLabel 1300 4500 0    55   Output ~ 0
iMX8.PWM1
Text GLabel 1300 8400 0    55   BiDi ~ 0
iMX8.GPIO3.IO11
Wire Wire Line
	1300 8400 1500 8400
Wire Wire Line
	1300 3600 1500 3600
Wire Wire Line
	1300 3700 1500 3700
Text GLabel 6900 3700 0    55   Output ~ 0
RESET
Text GLabel 6900 3400 0    55   Input ~ 0
JTAG_TDO
Text GLabel 6900 3300 0    55   Output ~ 0
JTAG_TCK
Text GLabel 6900 3500 0    55   Output ~ 0
JTAG_TMS
Text GLabel 6900 3600 0    55   Output ~ 0
JTAG_TDI
Text GLabel 6900 3100 0    55   Input ~ 0
ESP32.TXD0
Text GLabel 6900 3000 0    55   Output ~ 0
ESP32.RXD0
Text GLabel 6900 4000 0    55   Output ~ 0
UART0.RX
Text GLabel 6900 3900 0    55   Input ~ 0
UART0.TX
$Comp
L Connector_Generic:Conn_01x12 J302
U 1 1 5DAD3DBD
P 7200 3500
F 0 "J302" H 7150 4100 50  0000 L CNN
F 1 "Prog" H 7100 2800 50  0000 L CNN
F 2 "Connector_FFC-FPC:Hirose_FH12-12S-0.5SH_1x12-1MP_P0.50mm_Horizontal" H 7200 3500 50  0001 C CNN
F 3 "~" H 7200 3500 50  0001 C CNN
	1    7200 3500
	1    0    0    -1  
$EndComp
Wire Wire Line
	6900 3000 7000 3000
Wire Wire Line
	6900 3100 7000 3100
Wire Wire Line
	6900 3300 7000 3300
Wire Wire Line
	6900 3400 7000 3400
Wire Wire Line
	6900 3500 7000 3500
Wire Wire Line
	6900 3600 7000 3600
Wire Wire Line
	6900 3700 7000 3700
Wire Wire Line
	6900 3900 7000 3900
Wire Wire Line
	6900 4000 7000 4000
Wire Wire Line
	7000 3800 6950 3800
Wire Wire Line
	6950 3800 6950 4200
$Comp
L power:GND #PWR0318
U 1 1 5DF22AF7
P 6950 4200
F 0 "#PWR0318" H 6950 3950 50  0001 C CNN
F 1 "GND" H 6955 4027 50  0000 C CNN
F 2 "" H 6950 4200 50  0001 C CNN
F 3 "" H 6950 4200 50  0001 C CNN
	1    6950 4200
	1    0    0    -1  
$EndComp
Wire Wire Line
	7000 3200 6950 3200
Wire Wire Line
	6950 3200 6950 3800
Connection ~ 6950 3800
NoConn ~ 3000 8400
Wire Wire Line
	1300 8300 1500 8300
Text GLabel 1300 8300 0    55   BiDi ~ 0
iMX8.GPIO3.IO12
NoConn ~ 1500 8500
NoConn ~ 3000 5000
Wire Wire Line
	1300 4500 1500 4500
NoConn ~ 3000 10700
Text GLabel 3200 9500 2    55   Input ~ 0
iMX8.USB1.D_P
Text GLabel 3200 9600 2    55   Input ~ 0
iMX8.USB1.D_N
Wire Wire Line
	3000 9500 3200 9500
Wire Wire Line
	3000 9600 3200 9600
NoConn ~ 3000 3700
NoConn ~ 1500 1000
NoConn ~ 17750 14950
NoConn ~ 3000 3600
NoConn ~ 3000 5100
NoConn ~ 3000 5200
$Comp
L myConn:iMX8 J301
U 1 1 61A6861D
P 2250 5850
F 0 "J301" H 2250 11150 60  0000 C CNN
F 1 "iMX8" H 2250 550 60  0000 C CNN
F 2 "footprints:Socket_SODIMM_DDR3_TE_2013289" H 2250 5850 60  0001 C CNN
F 3 "" H 2250 5850 60  0001 C CNN
	1    2250 5850
	1    0    0    -1  
$EndComp
Text GLabel 1300 8200 0    55   BiDi ~ 0
iMX8.GPIO3.IO13
Wire Wire Line
	1300 8200 1500 8200
NoConn ~ 7000 4100
$EndSCHEMATC
