# CAEN QDC Support for SIS3153

This directory contains EPICS database templates for CAEN QDC (Charge to Digital Converter) modules accessed via the SIS3153 VME interface.

## Supported Modules

### CAEN V965 - Dual Range QDC (16 channels)
- 16 input channels
- Dual threshold per channel (low and high range)
- 12-bit ADC resolution
- Common stop or gate operation

### CAEN V792 - QDC (32 channels)
- 32 input channels
- Single threshold per channel
- 12-bit ADC resolution
- Common stop operation

## Files

### Templates
- `CAEN965.template` - Database template for V965 board registers and channels
- `CAEN792.template` - Database template for V792 board registers and channels

### Substitution Files
- `CAEN965_channels.substitutions` - Pre-configured 16 channel instantiation for V965
- `CAEN792_channels.substitutions` - Pre-configured 32 channel instantiation for V792

### Startup Scripts
- `st-btf.cmd` - Example startup script for BTF configuration using addresses from `btf-conf.txt`

## Register Map

Both modules share a common register layout:

| Register | Offset | Access | Description |
|----------|--------|--------|-------------|
| Manufacturer ID | 0x802A | R | Should be 0x40 for CAEN |
| Board ID | 0x803A | R | 0x03 for both V792 and V965 |
| Board Version | 0x8032 | R | 0x00 (V965), 0x11 (V792) |
| Firmware Rev | 0x1000 | R | Firmware revision |
| Serial LSB | 0x803E | R | Serial number low byte |
| Serial MSB | 0x8036 | R | Serial number high byte |
| Geo Address | 0x1002 | R/W | Geographic address |
| Bit Set | 0x1006 | W | Set control bits |
| Bit Clear | 0x1008 | W | Clear control bits |
| Status | 0x100E | R | Status register |
| Control | 0x1010 | R/W | Control register |
| Software Reset | 0x1016 | W | Software reset |
| Event Trigger | 0x1020 | W | Software trigger |
| Status 2 | 0x1022 | R | Buffer status |
| Event Count Low | 0x1024 | R | Event counter (low 16 bits) |
| Event Count High | 0x1026 | R | Event counter (high 16 bits) |
| Bit Set 2 | 0x1032 | W | Set control bits 2 |
| Bit Clear 2 | 0x1034 | W | Clear control bits 2 |
| Crate Select | 0x103C | R/W | Crate number |
| Event Counter Reset | 0x1040 | W | Reset event counter |
| IPED | 0x1060 | R/W | Pedestal current |
| Slide Constant | 0x106A | R/W | Slide subtraction constant |
| AAD | 0x1070 | R/W | Almost full level |
| BAD | 0x1072 | R/W | Buffer almost done |
| Thresholds | 0x1080+ | R/W | Channel thresholds (see below) |

### Threshold Addresses

**V965 (16 channels, dual threshold):**
- Channel N low threshold: `0x1080 + (N * 4)`
- Channel N high threshold: `0x1082 + (N * 4)`

**V792 (32 channels, single threshold):**
- Channel N threshold: `0x1080 + (N * 4)`

### Data FIFO
- Address: `0x0000`
- Access: A32D32 read
- Returns 32-bit data words with header, channel data, and end-of-block markers

## Control Bits (BitSet2/BitClear2)

| Bit | Name | Description |
|-----|------|-------------|
| 0 | TEST_MEM | Test memory mode |
| 1 | OFFLINE | ADC offline |
| 2 | CLEARDATA | Clear data buffer |
| 3 | OVERRANGE_EN | Accept overrange data |
| 4 | LOWTHR_EN | Disable zero suppression |
| 5 | VALID_CONTROL | Disable valid control (V775) |
| 6 | TESTAQ | Test acquisition mode |
| 7 | SLIDE_EN | Enable slide subtraction |
| 8 | STEP_TH | Zero suppression resolution |
| 10 | COMMON_STOP | Common stop mode (V775) |
| 11 | AUTOINCR | Auto-increment read pointer |
| 12 | EMPTY_EN | Write header/EOB when empty |
| 13 | SLIDESUB_EN | Allow operation mode change |
| 14 | ALLTRG_EN | Count all triggers |

## Status Bits

| Bit | Name | Description |
|-----|------|-------------|
| 0 | DREADY | Data ready |
| 1 | GDREADY | Global data ready |
| 2 | BUSY | Board busy |
| 3 | GBUSY | Global busy |
| 5 | PURGE | Board purged |
| 8 | EVRDY | Event ready |

## Status2 (Buffer Status) Bits

| Bit | Name | Description |
|-----|------|-------------|
| 1 | BUFFER_EMPTY | Data buffer empty |
| 2 | BUFFER_FULL | Data buffer full |

## Usage Examples

### Loading V965 Database

```bash
# In your IOC startup script
dbLoadTemplate("db/CAEN965_channels.substitutions", "P=MYIOC:QDC965:,PORT=VME1,BASE=0xb7640000")
```

### Loading V792 Database

```bash
# In your IOC startup script
dbLoadTemplate("db/CAEN792_channels.substitutions", "P=MYIOC:QDC792:,PORT=VME1,BASE=0xEE540000")
```

### Initialization Sequence

```bash
# 1. Software reset
dbpf MYIOC:QDC965:SoftwareReset 1
epicsThreadSleep 1

# 2. Configure control bits
# CLEARDATA | ALLTRG_EN | EMPTY_EN | AUTOINCR | OVERRANGE_EN | LOWTHR_EN
# = 0x5C14
dbpf MYIOC:QDC965:BitSet2 0x5C14

# 3. Clear test modes
# TEST_MEM | OFFLINE | TESTAQ | CLEARDATA = 0x0047
dbpf MYIOC:QDC965:BitClear2 0x0047

# 4. Set crate number
dbpf MYIOC:QDC965:CrateSelect 0

# 5. Set pedestal
dbpf MYIOC:QDC965:IPedestal 255

# 6. Set thresholds (per channel)
dbpf MYIOC:QDC965:Ch0ThresholdLow 10
dbpf MYIOC:QDC965:Ch0ThresholdHigh 100
```

### Reading Channel Data

The data FIFO returns structured data:

**Header Format (signature=2):**
```
Bits 31-27: Not used
Bits 26-24: Signature (0b010 = 2)
Bits 23-18: Geographic address
Bits 17-16: Pad
Bits 15-8:  Crate number
Bits 7-2:   Pad
Bits 1-0:   Channel count
```

**Data Format (signature=0):**
```
Bits 31-27: Not used
Bits 26-24: Signature (0b000 = 0)
Bits 23-19: Geographic address (V792) or Bits 23-17 (V965)
Bits 18-16: Channel number high bits (V965 only)
Bits 16:    Range (V965 only)
Bits 15-14: Flags (VD=valid, pad)
Bits 13:    Underflow
Bits 12:    Overflow
Bits 11-0:  ADC value (12 bits)
```

**End of Block Format (signature=4):**
```
Bits 31-27: Not used
Bits 26-24: Signature (0b100 = 4)
Bits 23-0:  Event counter
```

## Troubleshooting

### Board Not Responding
1. Check VME base address matches hardware switches
2. Verify SIS3153 connection (USB or Ethernet)
3. Check that board has power
4. Read ManufacturerID register - should be 0x40

### No Data Available
1. Check Status register - DREADY bit should be set after trigger
2. Verify trigger input is connected
3. Check that board is not in OFFLINE mode
4. Ensure CLEARDATA bit is cleared after initialization

### Incorrect Data
1. Verify threshold settings are appropriate
2. Check pedestal (IPED) setting
3. Ensure zero suppression is configured correctly (LOWTHR_EN)
4. Check for overflow/underflow flags in data

## BTF Configuration

The `btf-conf.txt` file specifies the VME base addresses for BTF facility:

```
pio:0x44200000
scaler:0x38380000
qdc965:0xb7640000
qdc792:0xEE540000
```

Use the provided `st-btf.cmd` startup script for this configuration.

## References

- CAEN V965 Manual: Dual Range QDC 16 Channels
- CAEN V792 Manual: QDC 32 Channels
- VMEbus Specification: ANSI/VITA 1-1994
- SIS3153 User Manual: USB/Ethernet VME Interface

## Author

Database templates and documentation created as part of SIS3153 Ethernet support addition.
Date: October 2025
