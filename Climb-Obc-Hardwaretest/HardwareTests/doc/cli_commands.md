Available CLI Commands
----------------------

### Time/RTC Commands

| Command | Parameters        | Description                               |
| ------- | ----------------- | ----------------------------------------- |
| getTime | none              | shows current RTC date,time in format: 'yyyymmddHHMMSS' and Status:<br>xEE .. clock error<br>0x11 .. synchronized<br>0x22 .. running<br>0x33 .. reset  to default|
| setTime | [hours] [min] [seconds] [sync]  | Sets the time portion of RTC.<br>if par omitted default = 0, sync any value -> true |
| setDate | [year] [month] [day]            | Sets the date portion of RTC. |
| getSeconds| none                           | Gets seconds ticked (timer with 12Mhz clock) since last reset |


### Memory Commands

| Command  | Parameters        | Description                               |
| -------- | ----------------- | ----------------------------------------- |
| crcFlash | [start] [len] | Calculates FlashChecksum from start to start+len if between 0x000000 and 0x0007FFFF<br>(hex input 0x... possible) |
| eeWriteName | [name] [hwrev]| Writes device name and hardware revision to EEPROM1 status page.<br>default: '<nset>' and '-.-'|
| eeStatus | none| Reads the devicename, hardwarerevision and some counters from EEPROM1 status page|
| readPage | mem pagenr | Reads one page of memory device 'mem'.<br>'EE1','EE2','EE3','FRA' (fram chip) |
| writePage | mem pagenr [fillbyte]| Fills and writes a page with fillbyte (default=0xAA)|
| fRead | flashNr adr len | Reads len bytes from adr in flash '1' or '2'. |
| fWrite | flashNr adr fillbyte len | Writes the fillbyte len times from adr in flash '1' or '2' |
| fErase | flashNr adr | Erases the Flash sector at adr in flash '1' or '2' |
| mRead | adr len | Reads len bytes at adr from MRAM chip. |
| mWrite | adr fillByte len | Writes the fillbyte len times at adr from MRAM chip. |

### Other Commands
| Command  | Parameters        | Description                               |
| -------- | ----------------- | ----------------------------------------- |
| cliStat | none | Prints some status info of CLI module  |
| simErr | tbd. | Provokes errors in checksums for radiation test checks. Do not use without reading current code !!!! |
| flogRead | [repeatCount] | reads out all registers of the FGD-02F dosimeter and prints 2 lines of measurment values.<br>repeatCount defaults to 1 <br>if >1 number of repeatCount measurments are taken every 3 seconds |
| adcRead | none | prints out some analouge measurments (temp, current) |
| readTemp | none | prints the TMP100 temperature |
 

### Not available during radiation test (special compiled inmage)

 Command  | Parameters        | Description                               |
| -------- | ----------------- | ----------------------------------------- |
| clkOut | [source] [divider] | Puts the internal clock on the CLKOUT (P1.27) pin<br>source (default='CPU'):<br>'OFF','CPU','OSC','IRC','USB','RTC'<br>devider 1..16 (default=10) |



                                                                                                                
