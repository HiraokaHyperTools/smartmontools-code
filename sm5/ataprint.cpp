/*
 * ataprint.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-9 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif // #ifdef HAVE_LOCALE_H

#include "int64.h"
#include "atacmdnames.h"
#include "atacmds.h"
#include "dev_interface.h"
#include "ataprint.h"
#include "smartctl.h"
#include "extern.h"
#include "utility.h"
#include "knowndrives.h"

const char *ataprint_c_cvsid="$Id: ataprint.cpp,v 1.208 2009/03/09 20:42:33 chrfranke Exp $"
ATACMDNAMES_H_CVSID ATACMDS_H_CVSID ATAPRINT_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID KNOWNDRIVES_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// for passing global control variables
extern smartmonctrl *con;

static const char * infofound(const char *output) {
  return (*output ? output : "[No Information Found]");
}


/* For the given Command Register (CR) and Features Register (FR), attempts
 * to construct a string that describes the contents of the Status
 * Register (ST) and Error Register (ER).  The caller passes the string
 * buffer and the return value is a pointer to this string.  If the
 * meanings of the flags of the error register are not known for the given
 * command then it returns NULL.
 *
 * The meanings of the flags of the error register for all commands are
 * described in the ATA spec and could all be supported here in theory.
 * Currently, only a few commands are supported (those that have been seen
 * to produce errors).  If many more are to be added then this function
 * should probably be redesigned.
 */

static const char * construct_st_er_desc(
  char * s,
  unsigned char CR, unsigned char FR,
  unsigned char ST, unsigned char ER,
  unsigned short SC,
  const ata_smart_errorlog_error_struct * lba28_regs,
  const ata_smart_exterrlog_error * lba48_regs
)
{
  const char *error_flag[8];
  int i, print_lba=0, print_sector=0;

  // Set of character strings corresponding to different error codes.
  // Please keep in alphabetic order if you add more.
  const char  *abrt  = "ABRT";  // ABORTED
 const char   *amnf  = "AMNF";  // ADDRESS MARK NOT FOUND
 const char   *ccto  = "CCTO";  // COMMAND COMPLETION TIMED OUT
 const char   *eom   = "EOM";   // END OF MEDIA
 const char   *icrc  = "ICRC";  // INTERFACE CRC ERROR
 const char   *idnf  = "IDNF";  // ID NOT FOUND
 const char   *ili   = "ILI";   // MEANING OF THIS BIT IS COMMAND-SET SPECIFIC
 const char   *mc    = "MC";    // MEDIA CHANGED 
 const char   *mcr   = "MCR";   // MEDIA CHANGE REQUEST
 const char   *nm    = "NM";    // NO MEDIA
 const char   *obs   = "obs";   // OBSOLETE
 const char   *tk0nf = "TK0NF"; // TRACK 0 NOT FOUND
 const char   *unc   = "UNC";   // UNCORRECTABLE
 const char   *wp    = "WP";    // WRITE PROTECTED

  /* If for any command the Device Fault flag of the status register is
   * not used then used_device_fault should be set to 0 (in the CR switch
   * below)
   */
  int uses_device_fault = 1;

  /* A value of NULL means that the error flag isn't used */
  for (i = 0; i < 8; i++)
    error_flag[i] = NULL;

  switch (CR) {
  case 0x10:  // RECALIBRATE
    error_flag[2] = abrt;
    error_flag[1] = tk0nf;
    break;
  case 0x20:  /* READ SECTOR(S) */
  case 0x21:  // READ SECTOR(S)
  case 0x24:  // READ SECTOR(S) EXT
  case 0xC4:  /* READ MULTIPLE */
  case 0x29:  // READ MULTIPLE EXT
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0x22:  // READ LONG (with retries)
  case 0x23:  // READ LONG (without retries)
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0x2a:  // READ STREAM DMA
  case 0x2b:  // READ STREAM PIO
    if (CR==0x2a)
      error_flag[7] = icrc;
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = ccto;
    print_lba=1;
    print_sector=SC;
    break;
  case 0x3A:  // WRITE STREAM DMA
  case 0x3B:  // WRITE STREAM PIO
    if (CR==0x3A)
      error_flag[7] = icrc;
    error_flag[6] = wp;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = ccto;
    print_lba=1;
    print_sector=SC;
    break;
  case 0x25:  /* READ DMA EXT */
  case 0x26:  // READ DMA QUEUED EXT
  case 0xC7:  // READ DMA QUEUED
  case 0xC8:  /* READ DMA */
  case 0xC9:
    error_flag[7] = icrc;
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    if (CR==0x25 || CR==0xC8)
      print_sector=SC;
    break;
  case 0x30:  /* WRITE SECTOR(S) */
  case 0x31:  // WRITE SECTOR(S)
  case 0x34:  // WRITE SECTOR(S) EXT
  case 0xC5:  /* WRITE MULTIPLE */
  case 0x39:  // WRITE MULTIPLE EXT
  case 0xCE:  // WRITE MULTIPLE FUA EXT
    error_flag[6] = wp;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    print_lba=1;
    break;
  case 0x32:  // WRITE LONG (with retries)
  case 0x33:  // WRITE LONG (without retries)
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    print_lba=1;
    break;
  case 0x3C:  // WRITE VERIFY
    error_flag[6] = unc;
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0x40: // READ VERIFY SECTOR(S) with retries
  case 0x41: // READ VERIFY SECTOR(S) without retries
  case 0x42: // READ VERIFY SECTOR(S) EXT
    error_flag[6] = unc;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    break;
  case 0xA0:  /* PACKET */
    /* Bits 4-7 are all used for sense key (a 'command packet set specific error
     * indication' according to the ATA/ATAPI-7 standard), so "Sense key" will
     * be repeated in the error description string if more than one of those
     * bits is set.
     */
    error_flag[7] = "Sense key (bit 3)",
    error_flag[6] = "Sense key (bit 2)",
    error_flag[5] = "Sense key (bit 1)",
    error_flag[4] = "Sense key (bit 0)",
    error_flag[2] = abrt;
    error_flag[1] = eom;
    error_flag[0] = ili;
    break;
  case 0xA1:  /* IDENTIFY PACKET DEVICE */
  case 0xEF:  /* SET FEATURES */
  case 0x00:  /* NOP */
  case 0xC6:  /* SET MULTIPLE MODE */
    error_flag[2] = abrt;
    break;
  case 0x2F:  // READ LOG EXT
    error_flag[6] = unc;
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = obs;
    break;
  case 0x3F:  // WRITE LOG EXT
    error_flag[4] = idnf;
    error_flag[2] = abrt;
    error_flag[0] = obs;
    break;
  case 0xB0:  /* SMART */
    switch(FR) {
    case 0xD0:  // SMART READ DATA
    case 0xD1:  // SMART READ ATTRIBUTE THRESHOLDS
    case 0xD5:  /* SMART READ LOG */
      error_flag[6] = unc;
      error_flag[4] = idnf;
      error_flag[2] = abrt;
      error_flag[0] = obs;
      break;
    case 0xD6:  /* SMART WRITE LOG */
      error_flag[4] = idnf;
      error_flag[2] = abrt;
      error_flag[0] = obs;
      break;
    case 0xD2:  // Enable/Disable Attribute Autosave
    case 0xD3:  // SMART SAVE ATTRIBUTE VALUES (ATA-3)
    case 0xD8:  // SMART ENABLE OPERATIONS
    case 0xD9:  /* SMART DISABLE OPERATIONS */
    case 0xDA:  /* SMART RETURN STATUS */
    case 0xDB:  // Enable/Disable Auto Offline (SFF)
      error_flag[2] = abrt;
      break;
    case 0xD4:  // SMART EXECUTE IMMEDIATE OFFLINE
      error_flag[4] = idnf;
      error_flag[2] = abrt;
      break;
    default:
      return NULL;
      break;
    }
    break;
  case 0xB1:  /* DEVICE CONFIGURATION */
    switch (FR) {
    case 0xC0:  /* DEVICE CONFIGURATION RESTORE */
      error_flag[2] = abrt;
      break;
    default:
      return NULL;
      break;
    }
    break;
  case 0xCA:  /* WRITE DMA */
  case 0xCB:
  case 0x35:  // WRITE DMA EXT
  case 0x3D:  // WRITE DMA FUA EXT
  case 0xCC:  // WRITE DMA QUEUED
  case 0x36:  // WRITE DMA QUEUED EXT
  case 0x3E:  // WRITE DMA QUEUED FUA EXT
    error_flag[7] = icrc;
    error_flag[6] = wp;
    error_flag[5] = mc;
    error_flag[4] = idnf;
    error_flag[3] = mcr;
    error_flag[2] = abrt;
    error_flag[1] = nm;
    error_flag[0] = amnf;
    print_lba=1;
    if (CR==0x35)
      print_sector=SC;
    break;
  case 0xE4: // READ BUFFER
  case 0xE8: // WRITE BUFFER
    error_flag[2] = abrt;
    break;
  default:
    return NULL;
  }

  s[0] = '\0';

  /* We ignore any status flags other than Device Fault and Error */

  if (uses_device_fault && (ST & (1 << 5))) {
    strcat(s, "Device Fault");
    if (ST & 1)  // Error flag
      strcat(s, "; ");
  }
  if (ST & 1) {  // Error flag
    int count = 0;

    strcat(s, "Error: ");
    for (i = 7; i >= 0; i--)
      if ((ER & (1 << i)) && (error_flag[i])) {
        if (count++ > 0)
           strcat(s, ", ");
        strcat(s, error_flag[i]);
      }
  }

  // If the error was a READ or WRITE error, print the Logical Block
  // Address (LBA) at which the read or write failed.
  if (print_lba) {
    char tmp[128];
    // print number of sectors, if known, and append to print string
    if (print_sector) {
      snprintf(tmp, 128, " %d sectors", print_sector);
      strcat(s, tmp);
    }

    if (lba28_regs) {
      unsigned lba;
      // bits 24-27: bits 0-3 of DH
      lba   = 0xf & lba28_regs->drive_head;
      lba <<= 8;
      // bits 16-23: CH
      lba  |= lba28_regs->cylinder_high;
      lba <<= 8;
      // bits 8-15:  CL
      lba  |= lba28_regs->cylinder_low;
      lba <<= 8;
      // bits 0-7:   SN
      lba  |= lba28_regs->sector_number;
      snprintf(tmp, 128, " at LBA = 0x%08x = %u", lba, lba);
      strcat(s, tmp);
    }
    else if (lba48_regs) {
      // This assumes that upper LBA registers are 0 for 28-bit commands
      // (TODO: detect 48-bit commands above)
      uint64_t lba48;
      lba48   = lba48_regs->lba_high_register_hi;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_mid_register_hi;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_low_register_hi;
      lba48  |= lba48_regs->device_register & 0xf;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_high_register;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_mid_register;
      lba48 <<= 8;
      lba48  |= lba48_regs->lba_low_register;
      snprintf(tmp, 128, " at LBA = 0x%08"PRIx64" = %"PRIu64, lba48, lba48);
      strcat(s, tmp);
    }
  }

  return s;
}

static inline const char * construct_st_er_desc(char * s,
  const ata_smart_errorlog_struct * data)
{
  return construct_st_er_desc(s,
    data->commands[4].commandreg,
    data->commands[4].featuresreg,
    data->error_struct.status,
    data->error_struct.error_register,
    data->error_struct.sector_count,
    &data->error_struct, (const ata_smart_exterrlog_error *)0);
}

static inline const char * construct_st_er_desc(char * s,
  const ata_smart_exterrlog_error_log * data)
{
  return construct_st_er_desc(s,
    data->commands[4].command_register,
    data->commands[4].features_register,
    data->error.status_register,
    data->error.error_register,
    data->error.count_register_hi << 8 | data->error.count_register,
    (const ata_smart_errorlog_error_struct *)0, &data->error);
}


// This returns the capacity of a disk drive and also prints this into
// a string, using comma separators to make it easier to read.  If the
// drive doesn't support LBA addressing or has no user writable
// sectors (eg, CDROM or DVD) then routine returns zero.
static uint64_t determine_capacity(const ata_identify_device * drive, char * pstring)
{
  // get correct character to use as thousands separator
  const char *separator = ",";
#ifdef HAVE_LOCALE_H
  struct lconv *currentlocale=NULL;
  setlocale (LC_ALL, "");
  currentlocale=localeconv();
  if (*(currentlocale->thousands_sep))
    separator=(char *)currentlocale->thousands_sep;
#endif // #ifdef HAVE_LOCALE_H

  // get #sectors and turn into bytes
  uint64_t capacity = get_num_sectors(drive) * 512;
  uint64_t retval = capacity;

  // print with locale-specific separators (default is comma)
  int started=0, k=1000000000;
  uint64_t power_of_ten = k;
  power_of_ten *= k;
  
  for (k=0; k<7; k++) {
    uint64_t threedigits = capacity/power_of_ten;
    capacity -= threedigits*power_of_ten;
    if (started)
      // we have already printed some digits
      pstring += sprintf(pstring, "%s%03"PRIu64, separator, threedigits);
    else if (threedigits || k==6) {
      // these are the first digits that we are printing
      pstring += sprintf(pstring, "%"PRIu64, threedigits);
      started = 1;
    }
    if (k!=6)
      power_of_ten /= 1000;
  }
  
  return retval;
}

static bool ataPrintDriveInfo(const ata_identify_device * drive)
{
  // format drive information (with byte swapping as needed)
  char model[64], serial[64], firm[64];
  format_ata_string(model, drive->model, 40);
  format_ata_string(serial, drive->serial_no, 20);
  format_ata_string(firm, drive->fw_rev, 8);

  // print out model, serial # and firmware versions  (byte-swap ASCI strings)
  const drive_settings * dbentry = lookup_drive(model, firm);

  // Print model family if known
  if (dbentry && *dbentry->modelfamily)
    pout("Model Family:     %s\n", dbentry->modelfamily);

  pout("Device Model:     %s\n", infofound(model));
  if (!con->dont_print_serial)
    pout("Serial Number:    %s\n", infofound(serial));
  pout("Firmware Version: %s\n", infofound(firm));

  char capacity[64];
  if (determine_capacity(drive, capacity))
    pout("User Capacity:    %s bytes\n", capacity);
  
  // See if drive is recognized
  pout("Device is:        %s\n", !dbentry ?
       "Not in smartctl database [for details use: -P showall]":
       "In smartctl database [for details use: -P show]");

  // now get ATA version info
  const char *description; unsigned short minorrev;
  int version = ataVersionInfo(&description, drive, &minorrev);

  // unrecognized minor revision code
  char unknown[64];
  if (!description){
    if (!minorrev)
      sprintf(unknown, "Exact ATA specification draft version not indicated");
    else
      sprintf(unknown,"Not recognized. Minor revision code: 0x%02hx", minorrev);
    description=unknown;
  }
  
  
  // SMART Support was first added into the ATA/ATAPI-3 Standard with
  // Revision 3 of the document, July 25, 1995.  Look at the "Document
  // Status" revision commands at the beginning of
  // http://www.t13.org/project/d2008r6.pdf to see this.  So it's not
  // enough to check if we are ATA-3.  Version=-3 indicates ATA-3
  // BEFORE Revision 3.
  pout("ATA Version is:   %d\n",(int)abs(version));
  pout("ATA Standard is:  %s\n",description);
  
  // print current time and date and timezone
  char timedatetz[DATEANDEPOCHLEN]; dateandtimezone(timedatetz);
  pout("Local Time is:    %s\n", timedatetz);

  // Print warning message, if there is one
  if (dbentry && *dbentry->warningmsg)
    pout("\n==> WARNING: %s\n\n", dbentry->warningmsg);

  if (version>=3)
    return !!dbentry;
  
  pout("SMART is only available in ATA Version 3 Revision 3 or greater.\n");
  pout("We will try to proceed in spite of this.\n");
  return !!dbentry;
}

static const char *OfflineDataCollectionStatus(unsigned char status_byte)
{
  unsigned char stat=status_byte & 0x7f;
  
  switch(stat){
  case 0x00:
    return "was never started";
  case 0x02:
    return "was completed without error";
  case 0x03:
    if (status_byte == 0x03)
      return "is in progress";
    else
      return "is in a Reserved state";
  case 0x04:
    return "was suspended by an interrupting command from host";
  case 0x05:
    return "was aborted by an interrupting command from host";
  case 0x06:
    return "was aborted by the device with a fatal error";
  default:
    if (stat >= 0x40)
      return "is in a Vendor Specific state";
    else
      return "is in a Reserved state";
  }
}
  
  
//  prints verbose value Off-line data collection status byte
static void PrintSmartOfflineStatus(const ata_smart_values * data)
{
  pout("Offline data collection status:  (0x%02x)\t",
       (int)data->offline_data_collection_status);
    
  // Off-line data collection status byte is not a reserved
  // or vendor specific value
  pout("Offline data collection activity\n"
       "\t\t\t\t\t%s.\n", OfflineDataCollectionStatus(data->offline_data_collection_status));
  
  // Report on Automatic Data Collection Status.  Only IBM documents
  // this bit.  See SFF 8035i Revision 2 for details.
  if (data->offline_data_collection_status & 0x80)
    pout("\t\t\t\t\tAuto Offline Data Collection: Enabled.\n");
  else
    pout("\t\t\t\t\tAuto Offline Data Collection: Disabled.\n");
  
  return;
}

static void PrintSmartSelfExecStatus(const ata_smart_values * data)
{
   pout("Self-test execution status:      ");
   
   switch (data->self_test_exec_status >> 4)
   {
      case 0:
        pout("(%4d)\tThe previous self-test routine completed\n\t\t\t\t\t",
                (int)data->self_test_exec_status);
        pout("without error or no self-test has ever \n\t\t\t\t\tbeen run.\n");
        break;
       case 1:
         pout("(%4d)\tThe self-test routine was aborted by\n\t\t\t\t\t",
                 (int)data->self_test_exec_status);
         pout("the host.\n");
         break;
       case 2:
         pout("(%4d)\tThe self-test routine was interrupted\n\t\t\t\t\t",
                 (int)data->self_test_exec_status);
         pout("by the host with a hard or soft reset.\n");
         break;
       case 3:
          pout("(%4d)\tA fatal error or unknown test error\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("occurred while the device was executing\n\t\t\t\t\t");
          pout("its self-test routine and the device \n\t\t\t\t\t");
          pout("was unable to complete the self-test \n\t\t\t\t\t");
          pout("routine.\n");
          break;
       case 4:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("a test element that failed and the test\n\t\t\t\t\t");
          pout("element that failed is not known.\n");
          break;
       case 5:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("the electrical element of the test\n\t\t\t\t\t");
          pout("failed.\n");
          break;
       case 6:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("the servo (and/or seek) element of the \n\t\t\t\t\t");
          pout("test failed.\n");
          break;
       case 7:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("the read element of the test failed.\n");
          break;
       case 8:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("a test element that failed and the\n\t\t\t\t\t");
          pout("device is suspected of having handling\n\t\t\t\t\t");
          pout("damage.\n");
          break;
       case 15:
          if (con->fixfirmwarebug == FIX_SAMSUNG3 && data->self_test_exec_status == 0xf0) {
            pout("(%4d)\tThe previous self-test routine completed\n\t\t\t\t\t",
                    (int)data->self_test_exec_status);
            pout("with unknown result or self-test in\n\t\t\t\t\t");
            pout("progress with less than 10%% remaining.\n");
          }
          else {
            pout("(%4d)\tSelf-test routine in progress...\n\t\t\t\t\t",
                    (int)data->self_test_exec_status);
            pout("%1d0%% of test remaining.\n", 
                  (int)(data->self_test_exec_status & 0x0f));
          }
          break;
       default:
          pout("(%4d)\tReserved.\n",
                  (int)data->self_test_exec_status);
          break;
   }
        
}

static void PrintSmartTotalTimeCompleteOffline (const ata_smart_values * data)
{
  pout("Total time to complete Offline \n");
  pout("data collection: \t\t (%4d) seconds.\n", 
       (int)data->total_time_to_complete_off_line);
}

static void PrintSmartOfflineCollectCap(const ata_smart_values *data)
{
  pout("Offline data collection\n");
  pout("capabilities: \t\t\t (0x%02x) ",
       (int)data->offline_data_collection_capability);
  
  if (data->offline_data_collection_capability == 0x00){
    pout("\tOffline data collection not supported.\n");
  } 
  else {
    pout( "%s\n", isSupportExecuteOfflineImmediate(data)?
          "SMART execute Offline immediate." :
          "No SMART execute Offline immediate.");
    
    pout( "\t\t\t\t\t%s\n", isSupportAutomaticTimer(data)? 
          "Auto Offline data collection on/off support.":
          "No Auto Offline data collection support.");
    
    pout( "\t\t\t\t\t%s\n", isSupportOfflineAbort(data)? 
          "Abort Offline collection upon new\n\t\t\t\t\tcommand.":
          "Suspend Offline collection upon new\n\t\t\t\t\tcommand.");
    
    pout( "\t\t\t\t\t%s\n", isSupportOfflineSurfaceScan(data)? 
          "Offline surface scan supported.":
          "No Offline surface scan supported.");
    
    pout( "\t\t\t\t\t%s\n", isSupportSelfTest(data)? 
          "Self-test supported.":
          "No Self-test supported.");

    pout( "\t\t\t\t\t%s\n", isSupportConveyanceSelfTest(data)? 
          "Conveyance Self-test supported.":
          "No Conveyance Self-test supported.");

    pout( "\t\t\t\t\t%s\n", isSupportSelectiveSelfTest(data)? 
          "Selective Self-test supported.":
          "No Selective Self-test supported.");
  }
}

static void PrintSmartCapability(const ata_smart_values *data)
{
   pout("SMART capabilities:            ");
   pout("(0x%04x)\t", (int)data->smart_capability);
   
   if (data->smart_capability == 0x00)
   {
       pout("Automatic saving of SMART data\t\t\t\t\tis not implemented.\n");
   } 
   else 
   {
        
      pout( "%s\n", (data->smart_capability & 0x01)? 
              "Saves SMART data before entering\n\t\t\t\t\tpower-saving mode.":
              "Does not save SMART data before\n\t\t\t\t\tentering power-saving mode.");
                
      if ( data->smart_capability & 0x02 )
      {
          pout("\t\t\t\t\tSupports SMART auto save timer.\n");
      }
   }
}

static void PrintSmartErrorLogCapability(const ata_smart_values * data, const ata_identify_device * identity)
{
   pout("Error logging capability:       ");
    
   if ( isSmartErrorLogCapable(data, identity) )
   {
      pout(" (0x%02x)\tError logging supported.\n",
               (int)data->errorlog_capability);
   }
   else {
       pout(" (0x%02x)\tError logging NOT supported.\n",
                (int)data->errorlog_capability);
   }
}

static void PrintSmartShortSelfTestPollingTime(const ata_smart_values * data)
{
  pout("Short self-test routine \n");
  if (isSupportSelfTest(data))
    pout("recommended polling time: \t (%4d) minutes.\n", 
         (int)data->short_test_completion_time);
  else
    pout("recommended polling time: \t        Not Supported.\n");
}

static void PrintSmartExtendedSelfTestPollingTime(const ata_smart_values * data)
{
  pout("Extended self-test routine\n");
  if (isSupportSelfTest(data))
    pout("recommended polling time: \t (%4d) minutes.\n", 
         (int)data->extend_test_completion_time);
  else
    pout("recommended polling time: \t        Not Supported.\n");
}

static void PrintSmartConveyanceSelfTestPollingTime(const ata_smart_values * data)
{
  pout("Conveyance self-test routine\n");
  if (isSupportConveyanceSelfTest(data))
    pout("recommended polling time: \t (%4d) minutes.\n", 
         (int)data->conveyance_test_completion_time);
  else
    pout("recommended polling time: \t        Not Supported.\n");
}

// onlyfailed=0 : print all attribute values
// onlyfailed=1:  just ones that are currently failed and have prefailure bit set
// onlyfailed=2:  ones that are failed, or have failed with or without prefailure bit set
static void PrintSmartAttribWithThres(const ata_smart_values * data,
                                      const ata_smart_thresholds_pvt * thresholds,
                                      int onlyfailed)
{
  int needheader=1;
  char rawstring[64];
    
  // step through all vendor attributes
  for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++) {
    const char *status;
    const ata_smart_attribute * disk = data->vendor_attributes+i;
    const ata_smart_threshold_entry * thre = thresholds->thres_entries+i;
    
    // consider only valid attributes (allowing some screw-ups in the
    // thresholds page data to slip by)
    if (disk->id){
      const char *type, *update;
      int failednow,failedever;
      char attributename[64];

      failednow = (disk->current <= thre->threshold);
      failedever= (disk->worst   <= thre->threshold);
      
      // These break out of the loop if we are only printing certain entries...
      if (onlyfailed==1 && (!ATTRIBUTE_FLAGS_PREFAILURE(disk->flags) || !failednow))
        continue;
      
      if (onlyfailed==2 && !failedever)
        continue;
      
      // print header only if needed
      if (needheader){
        if (!onlyfailed){
          pout("SMART Attributes Data Structure revision number: %d\n",(int)data->revnumber);
          pout("Vendor Specific SMART Attributes with Thresholds:\n");
        }
        pout("ID# ATTRIBUTE_NAME          FLAG     VALUE WORST THRESH TYPE      UPDATED  WHEN_FAILED RAW_VALUE\n");
        needheader=0;
      }
      
      // is this Attribute currently failed, or has it ever failed?
      if (failednow)
        status="FAILING_NOW";
      else if (failedever)
        status="In_the_past";
      else
        status="    -";

      // Print name of attribute
      ataPrintSmartAttribName(attributename,disk->id, con->attributedefs);
      pout("%-28s",attributename);

      // printing line for each valid attribute
      type=ATTRIBUTE_FLAGS_PREFAILURE(disk->flags)?"Pre-fail":"Old_age";
      update=ATTRIBUTE_FLAGS_ONLINE(disk->flags)?"Always":"Offline";

      pout("0x%04x   %.3d   %.3d   %.3d    %-10s%-9s%-12s", 
             (int)disk->flags, (int)disk->current, (int)disk->worst,
             (int)thre->threshold, type, update, status);

      // print raw value of attribute
      ataPrintSmartAttribRawValue(rawstring, disk, con->attributedefs);
      pout("%s\n", rawstring);
      
      // print a warning if there is inconsistency here!
      if (disk->id != thre->id){
        char atdat[64],atthr[64];
        ataPrintSmartAttribName(atdat, disk->id, con->attributedefs);
        ataPrintSmartAttribName(atthr, thre->id, con->attributedefs);
        pout("%-28s<== Data Page      |  WARNING: PREVIOUS ATTRIBUTE HAS TWO\n",atdat);
        pout("%-28s<== Threshold Page |  INCONSISTENT IDENTITIES IN THE DATA\n",atthr);
      }
    }
  }
  if (!needheader) pout("\n");
}

// Print SMART related SCT capabilities
static void ataPrintSCTCapability(const ata_identify_device *drive)
{
  unsigned short sctcaps = drive->words088_255[206-88];
  if (!(sctcaps & 0x01))
    return;
  pout("SCT capabilities: \t       (0x%04x)\tSCT Status supported.\n", sctcaps);
  if (sctcaps & 0x10)
    pout("\t\t\t\t\tSCT Feature Control supported.\n");
  if (sctcaps & 0x20)
    pout("\t\t\t\t\tSCT Data Table supported.\n");
}


static void ataPrintGeneralSmartValues(const ata_smart_values *data, const ata_identify_device *drive)
{
  pout("General SMART Values:\n");
  
  PrintSmartOfflineStatus(data); 
  
  if (isSupportSelfTest(data)){
    PrintSmartSelfExecStatus (data);
  }
  
  PrintSmartTotalTimeCompleteOffline(data);
  PrintSmartOfflineCollectCap(data);
  PrintSmartCapability(data);
  
  PrintSmartErrorLogCapability(data, drive);

  pout( "\t\t\t\t\t%s\n", isGeneralPurposeLoggingCapable(drive)?
        "General Purpose Logging supported.":
        "No General Purpose Logging support.");

  if (isSupportSelfTest(data)){
    PrintSmartShortSelfTestPollingTime (data);
    PrintSmartExtendedSelfTestPollingTime (data);
  }
  if (isSupportConveyanceSelfTest(data))
    PrintSmartConveyanceSelfTestPollingTime (data);

  ataPrintSCTCapability(drive);

  pout("\n");
}

// Get # sectors of a log addr, 0 if log does not exist.
static unsigned GetNumLogSectors(const ata_smart_log_directory * logdir, unsigned logaddr, bool gpl)
{
  if (!logdir)
    return 0;
  if (logaddr > 0xff)
    return 0;
  if (logaddr == 0)
    return 1;
  unsigned n = logdir->entry[logaddr-1].numsectors;
  if (gpl)
    // GP logs may have >255 sectors
    n |= logdir->entry[logaddr-1].reserved << 8;
  return n;
}

// Get name of log.
// Table A.2 of T13/1699-D Revision 6
static const char * GetLogName(unsigned logaddr)
{
    switch (logaddr) {
      case 0x00: return "Log Directory";
      case 0x01: return "Summary SMART error log";
      case 0x02: return "Comprehensive SMART error log";
      case 0x03: return "Ext. Comprehensive SMART error log";
      case 0x04: return "Device Statistics";
      case 0x06: return "SMART self-test log";
      case 0x07: return "Extended self-test log";
      case 0x09: return "Selective self-test log";
      case 0x10: return "NCQ Command Error";
      case 0x11: return "SATA Phy Event Counters";
      case 0x20: return "Streaming performance log"; // Obsolete
      case 0x21: return "Write stream error log";
      case 0x22: return "Read stream error log";
      case 0x23: return "Delayed sector log"; // Obsolete
      case 0xe0: return "SCT Command/Status";
      case 0xe1: return "SCT Data Transfer";
      default:
        if (0xa0 <= logaddr && logaddr <= 0xdf)
          return "Device vendor specific log";
        if (0x80 <= logaddr && logaddr <= 0x9f)
          return "Host vendor specific log";
        if (0x12 <= logaddr && logaddr <= 0x17)
          return "Reserved for Serial ATA";
        return "Reserved";
    }
    /*NOTREACHED*/
}

// Print SMART and/or GP Log Directory
static void PrintLogDirectories(const ata_smart_log_directory * gplogdir,
                                const ata_smart_log_directory * smartlogdir)
{
  if (gplogdir)
    pout("General Purpose Log Directory Version %u\n", gplogdir->logversion);
  if (smartlogdir)
    pout("SMART %sLog Directory Version %u%s\n",
         (gplogdir ? "          " : ""), smartlogdir->logversion,
         (smartlogdir->logversion==1 ? " [multi-sector log support]" : ""));

  for (unsigned i = 0; i <= 0xff; i++) {
    // Get number of sectors
    unsigned smart_numsect = GetNumLogSectors(smartlogdir, i, false);
    unsigned gp_numsect    = GetNumLogSectors(gplogdir   , i, true );

    if (!(smart_numsect || gp_numsect))
      continue; // Log does not exist

    const char * name = GetLogName(i);

    // Print name and length of log.
    // If both SMART and GP exist, print separate entries if length differ.
    if (smart_numsect == gp_numsect)
      pout(  "GP/S  Log at address 0x%02x has %4d sectors [%s]\n", i, smart_numsect, name);
    else {
      if (gp_numsect)
        pout("GP %sLog at address 0x%02x has %4d sectors [%s]\n", (smartlogdir?"   ":""),
             i, gp_numsect, name);
      if (smart_numsect)
        pout("SMART Log at address 0x%02x has %4d sectors [%s]\n", i, smart_numsect, name);
    }
  }
  pout("\n");
}

// Print hexdump of log pages.
// Format is compatible with 'xxd -r'.
static void PrintLogPages(const char * type, const unsigned char * data,
                          unsigned char logaddr, unsigned page,
                          unsigned num_pages, unsigned max_pages)
{
  pout("%s Log 0x%02x [%s], Page %u-%u (of %u)\n",
    type, logaddr, GetLogName(logaddr), page, page+num_pages-1, max_pages);
  for (unsigned i = 0; i < num_pages * 512; i += 16) {
    const unsigned char * p = data+i;
    pout("%07x: %02x %02x %02x %02x %02x %02x %02x %02x "
               "%02x %02x %02x %02x %02x %02x %02x %02x\n",
         (page * 512) + i,
         p[ 0], p[ 1], p[ 2], p[ 3], p[ 4], p[ 5], p[ 6], p[ 7],
         p[ 8], p[ 9], p[10], p[11], p[12], p[13], p[14], p[15]);
    if ((i & 0x1ff) == 0x1f0)
      pout("\n");
  }
}

// Print log 0x11
static void PrintSataPhyEventCounters(const unsigned char * data, bool reset)
{
  if (checksum(data))
    checksumwarning("SATA Phy Event Counters");
  pout("SATA Phy Event Counters (GP Log 0x11)\n");
  if (data[0] || data[1] || data[2] || data[3])
    pout("[Reserved: 0x%02x 0x%02x 0x%02x 0x%02x]\n",
    data[0], data[1], data[2], data[3]);
  pout("ID      Size     Value  Description\n");

  for (unsigned i = 4; ; ) {
    // Get counter id and size (bits 14:12)
    unsigned id = data[i] | (data[i+1] << 8);
    unsigned size = ((id >> 12) & 0x7) << 1;
    id &= 0x8fff;

    // End of counter table ?
    if (!id)
      break;
    i += 2;

    if (!(2 <= size && size <= 8 && i + size < 512)) {
      pout("0x%04x  %u: Invalid entry\n", id, size);
      break;
    }

    // Get value
    uint64_t val = 0, max_val = 0;
    for (unsigned j = 0; j < size; j+=2) {
        val |= (uint64_t)(data[i+j] | (data[i+j+1] << 8)) << (j*8);
        max_val |= (uint64_t)0xffffU << (j*8);
    }
    i += size;

    // Get name
    const char * name;
    switch (id) {
      case 0x001: name = "Command failed due to ICRC error"; break; // Mandatory
      case 0x002: name = "R_ERR response for data FIS"; break;
      case 0x003: name = "R_ERR response for device-to-host data FIS"; break;
      case 0x004: name = "R_ERR response for host-to-device data FIS"; break;
      case 0x005: name = "R_ERR response for non-data FIS"; break;
      case 0x006: name = "R_ERR response for device-to-host non-data FIS"; break;
      case 0x007: name = "R_ERR response for host-to-device non-data FIS"; break;
      case 0x008: name = "Device-to-host non-data FIS retries"; break;
      case 0x009: name = "Transition from drive PhyRdy to drive PhyNRdy"; break;
      case 0x00A: name = "Device-to-host register FISes sent due to a COMRESET"; break; // Mandatory
      case 0x00B: name = "CRC errors within host-to-device FIS"; break;
      case 0x00D: name = "Non-CRC errors within host-to-device FIS"; break;
      case 0x00F: name = "R_ERR response for host-to-device data FIS, CRC"; break;
      case 0x010: name = "R_ERR response for host-to-device data FIS, non-CRC"; break;
      case 0x012: name = "R_ERR response for host-to-device non-data FIS, CRC"; break;
      case 0x013: name = "R_ERR response for host-to-device non-data FIS, non-CRC"; break;
      default:    name = (id & 0x8000 ? "Vendor specific" : "Unknown"); break;
    }

    // Counters stop at max value, add '+' in this case
    pout("0x%04x  %u %12"PRIu64"%c %s\n", id, size, val,
      (val == max_val ? '+' : ' '), name);
  }
  if (reset)
    pout("All counters reset\n");
  pout("\n");
}

// Get description for 'state' value from SMART Error Logs
static const char * get_error_log_state_desc(unsigned state)
{
  state &= 0x0f;
  switch (state){
    case 0x0: return "in an unknown state";
    case 0x1: return "sleeping";
    case 0x2: return "in standby mode";
    case 0x3: return "active or idle";
    case 0x4: return "doing SMART Offline or Self-test";
  default:
    return (state < 0xb ? "in a reserved state"
                        : "in a vendor specific state");
  }
}

// returns number of errors
static int ataPrintSmartErrorlog(const ata_smart_errorlog *data)
{
  pout("SMART Error Log Version: %d\n", (int)data->revnumber);
  
  // if no errors logged, return
  if (!data->error_log_pointer){
    pout("No Errors Logged\n\n");
    return 0;
  }
  PRINT_ON(con);
  // If log pointer out of range, return
  if (data->error_log_pointer>5){
    pout("Invalid Error Log index = 0x%02x (T13/1321D rev 1c "
         "Section 8.41.6.8.2.2 gives valid range from 1 to 5)\n\n",
         (int)data->error_log_pointer);
    return 0;
  }

  // Some internal consistency checking of the data structures
  if ((data->ata_error_count-data->error_log_pointer)%5 && con->fixfirmwarebug != FIX_SAMSUNG2) {
    pout("Warning: ATA error count %d inconsistent with error log pointer %d\n\n",
         data->ata_error_count,data->error_log_pointer);
  }
  
  // starting printing error log info
  if (data->ata_error_count<=5)
    pout( "ATA Error Count: %d\n", (int)data->ata_error_count);
  else
    pout( "ATA Error Count: %d (device log contains only the most recent five errors)\n",
           (int)data->ata_error_count);
  PRINT_OFF(con);
  pout("\tCR = Command Register [HEX]\n"
       "\tFR = Features Register [HEX]\n"
       "\tSC = Sector Count Register [HEX]\n"
       "\tSN = Sector Number Register [HEX]\n"
       "\tCL = Cylinder Low Register [HEX]\n"
       "\tCH = Cylinder High Register [HEX]\n"
       "\tDH = Device/Head Register [HEX]\n"
       "\tDC = Device Command Register [HEX]\n"
       "\tER = Error register [HEX]\n"
       "\tST = Status register [HEX]\n"
       "Powered_Up_Time is measured from power on, and printed as\n"
       "DDd+hh:mm:SS.sss where DD=days, hh=hours, mm=minutes,\n"
       "SS=sec, and sss=millisec. It \"wraps\" after 49.710 days.\n\n");
  
  // now step through the five error log data structures (table 39 of spec)
  for (int k = 4; k >= 0; k-- ) {

    // The error log data structure entries are a circular buffer
    int j, i=(data->error_log_pointer+k)%5;
    const ata_smart_errorlog_struct * elog = data->errorlog_struct+i;
    const ata_smart_errorlog_error_struct * summary = &(elog->error_struct);

    // Spec says: unused error log structures shall be zero filled
    if (nonempty((unsigned char*)elog,sizeof(*elog))){
      // Table 57 of T13/1532D Volume 1 Revision 3
      const char *msgstate = get_error_log_state_desc(summary->state);
      int days = (int)summary->timestamp/24;

      // See table 42 of ATA5 spec
      PRINT_ON(con);
      pout("Error %d occurred at disk power-on lifetime: %d hours (%d days + %d hours)\n",
             (int)(data->ata_error_count+k-4), (int)summary->timestamp, days, (int)(summary->timestamp-24*days));
      PRINT_OFF(con);
      pout("  When the command that caused the error occurred, the device was %s.\n\n",msgstate);
      pout("  After command completion occurred, registers were:\n"
           "  ER ST SC SN CL CH DH\n"
           "  -- -- -- -- -- -- --\n"
           "  %02x %02x %02x %02x %02x %02x %02x",
           (int)summary->error_register,
           (int)summary->status,
           (int)summary->sector_count,
           (int)summary->sector_number,
           (int)summary->cylinder_low,
           (int)summary->cylinder_high,
           (int)summary->drive_head);
      // Add a description of the contents of the status and error registers
      // if possible
      char descbuf[256];
      const char * st_er_desc = construct_st_er_desc(descbuf, elog);
      if (st_er_desc)
        pout("  %s", st_er_desc);
      pout("\n\n");
      pout("  Commands leading to the command that caused the error were:\n"
           "  CR FR SC SN CL CH DH DC   Powered_Up_Time  Command/Feature_Name\n"
           "  -- -- -- -- -- -- -- --  ----------------  --------------------\n");
      for ( j = 4; j >= 0; j--){
        const ata_smart_errorlog_command_struct * thiscommand = elog->commands+j;

        // Spec says: unused data command structures shall be zero filled
        if (nonempty((unsigned char*)thiscommand,sizeof(*thiscommand))) {
	  char timestring[32];
	  
	  // Convert integer milliseconds to a text-format string
	  MsecToText(thiscommand->timestamp, timestring);
	  
          pout("  %02x %02x %02x %02x %02x %02x %02x %02x  %16s  %s\n",
               (int)thiscommand->commandreg,
               (int)thiscommand->featuresreg,
               (int)thiscommand->sector_count,
               (int)thiscommand->sector_number,
               (int)thiscommand->cylinder_low,
               (int)thiscommand->cylinder_high,
               (int)thiscommand->drive_head,
               (int)thiscommand->devicecontrolreg,
	       timestring,
               look_up_ata_command(thiscommand->commandreg, thiscommand->featuresreg));
	}
      }
      pout("\n");
    }
  }
  PRINT_ON(con);
  if (con->printing_switchable)
    pout("\n");
  PRINT_OFF(con);
  return data->ata_error_count;  
}

// Print SMART Extended Comprehensive Error Log (GP Log 0x03)
static int PrintSmartExtErrorLog(const ata_smart_exterrlog * log,
                                 unsigned nsectors, unsigned max_errors)
{
  pout("SMART Extended Comprehensive Error Log Version: %u (%u sectors)\n",
       log->version, nsectors);

  if (!log->device_error_count) {
    pout("No Errors Logged\n\n");
    return 0;
  }
  PRINT_ON(con);

  // Check index
  unsigned nentries = nsectors * 4;
  unsigned erridx = log->error_log_index;
  if (!(1 <= erridx && erridx <= nentries)){
    // Some Samsung disks (at least SP1614C/SW100-25, HD300LJ/ZT100-12) use the
    // former index from Summary Error Log (byte 1, now reserved) and set byte 2-3
    // to 0.
    if (!(erridx == 0 && 1 <= log->reserved1 && log->reserved1 <= nentries)) {
      pout("Invalid Error Log index = 0x%04x (reserved = 0x%02x)\n", erridx, log->reserved1);
      return 0;
    }
    pout("Invalid Error Log index = 0x%04x, trying reserved byte (0x%02x) instead\n", erridx, log->reserved1);
    erridx = log->reserved1;
  }

  // Index base is not clearly specified by ATA8-ACS (T13/1699-D Revision 6a),
  // it is 1-based in practice.
  erridx--;

  // Calculate #errors to print
  unsigned errcnt = log->device_error_count;

  if (errcnt <= nentries)
    pout("Device Error Count: %u\n", log->device_error_count);
  else {
    errcnt = nentries;
    pout("Device Error Count: %u (device log contains only the most recent %u errors)\n",
         log->device_error_count, errcnt);
  }

  if (max_errors < errcnt)
    errcnt = max_errors;

  PRINT_OFF(con);
  pout("\tCR     = Command Register\n"
       "\tFEATR  = Features Register\n"
       "\tCOUNT  = Count (was: Sector Count) Register\n"
       "\tLBA_48 = Upper bytes of LBA High/Mid/Low Registers ]  ATA-8\n"
       "\tLH     = LBA High (was: Cylinder High) Register    ]   LBA\n"
       "\tLM     = LBA Mid (was: Cylinder Low) Register      ] Register\n"
       "\tLL     = LBA Low (was: Sector Number) Register     ]\n"
       "\tDV     = Device (was: Device/Head) Register\n"
       "\tDC     = Device Control Register\n"
       "\tER     = Error register\n"
       "\tST     = Status register\n"
       "Powered_Up_Time is measured from power on, and printed as\n"
       "DDd+hh:mm:SS.sss where DD=days, hh=hours, mm=minutes,\n"
       "SS=sec, and sss=millisec. It \"wraps\" after 49.710 days.\n\n");

  // Iterate through circular buffer in reverse direction
  for (unsigned i = 0, errnum = log->device_error_count;
       i < errcnt; i++, errnum--, erridx = (erridx > 0 ? erridx - 1 : nentries - 1)) {

    const ata_smart_exterrlog_error_log & entry = log[erridx / 4].error_logs[erridx % 4];

    // Skip unused entries
    if (!nonempty(&entry, sizeof(entry))) {
      pout("Error %u [%u] log entry is empty\n", errnum, erridx);
      continue;
    }

    // Print error information
    PRINT_ON(con);
    const ata_smart_exterrlog_error & err = entry.error;
    pout("Error %u [%u] occurred at disk power-on lifetime: %u hours (%u days + %u hours)\n",
         errnum, erridx, err.timestamp, err.timestamp / 24, err.timestamp % 24);
    PRINT_OFF(con);

    pout("  When the command that caused the error occurred, the device was %s.\n\n",
      get_error_log_state_desc(err.state));

    // Print registers
    pout("  After command completion occurred, registers were:\n"
         "  ER -- ST COUNT  LBA_48  LH LM LL DV DC\n"
         "  -- -- -- == -- == == == -- -- -- -- --\n"
         "  %02x -- %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
         err.error_register,
         err.status_register,
         err.count_register_hi,
         err.count_register,
         err.lba_high_register_hi,
         err.lba_mid_register_hi,
         err.lba_low_register_hi,
         err.lba_high_register,
         err.lba_mid_register,
         err.lba_low_register,
         err.device_register,
         err.device_control_register);

    // Add a description of the contents of the status and error registers
    // if possible
    char descbuf[256];
    const char * st_er_desc = construct_st_er_desc(descbuf, &entry);
    if (st_er_desc)
      pout("  %s", st_er_desc);
    pout("\n\n");

    // Print command history
    pout("  Commands leading to the command that caused the error were:\n"
         "  CR FEATR COUNT  LBA_48  LH LM LL DV DC  Powered_Up_Time  Command/Feature_Name\n"
         "  -- == -- == -- == == == -- -- -- -- --  ---------------  --------------------\n");
    for (int ci = 4; ci >= 0; ci--) {
      const ata_smart_exterrlog_command & cmd = entry.commands[ci];

      // Skip unused entries
      if (!nonempty(&cmd, sizeof(cmd)))
        continue;

      // Print registers, timestamp and ATA command name
      char timestring[32];
      MsecToText(cmd.timestamp, timestring);

      pout("  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %16s  %s\n",
           cmd.command_register,
           cmd.features_register_hi,
           cmd.features_register,
           cmd.count_register_hi,
           cmd.count_register,
           cmd.lba_high_register_hi,
           cmd.lba_mid_register_hi,
           cmd.lba_low_register_hi,
           cmd.lba_high_register,
           cmd.lba_mid_register,
           cmd.lba_low_register,
           cmd.device_register,
           cmd.device_control_register,
           timestring,
           look_up_ata_command(cmd.command_register, cmd.features_register));
    }
    pout("\n");
  }

  PRINT_ON(con);
  if (con->printing_switchable)
    pout("\n");
  PRINT_OFF(con);
  return log->device_error_count;
}

static void ataPrintSelectiveSelfTestLog(const ata_selective_self_test_log * log, const ata_smart_values * sv)
{
  int i,field1,field2;
  const char *msg;
  char tmp[64];
  uint64_t maxl=0,maxr=0;
  uint64_t current=log->currentlba;
  uint64_t currentend=current+65535;

  // print data structure revision number
  pout("SMART Selective self-test log data structure revision number %d\n",(int)log->logversion);
  if (1 != log->logversion)
    pout("Note: revision number not 1 implies that no selective self-test has ever been run\n");
  
  switch((sv->self_test_exec_status)>>4){
  case  0:msg="Completed";
    break;
  case  1:msg="Aborted_by_host";
    break;
  case  2:msg="Interrupted";
    break;
  case  3:msg="Fatal_error";
    break;
  case  4:msg="Completed_unknown_failure";
    break;
  case  5:msg="Completed_electrical_failure";
    break;
  case  6:msg="Completed_servo/seek_failure";
    break;
  case  7:msg="Completed_read_failure";
    break;
  case  8:msg="Completed_handling_damage??";
    break;
  case 15:msg="Self_test_in_progress";
    break;
  default:msg="Unknown_status ";
    break;
  }

  // find the number of columns needed for printing. If in use, the
  // start/end of span being read-scanned...
  if (log->currentspan>5) {
    maxl=current;
    maxr=currentend;
  }
  for (i=0; i<5; i++) {
    uint64_t start=log->span[i].start;
    uint64_t end  =log->span[i].end; 
    // ... plus max start/end of each of the five test spans.
    if (start>maxl)
      maxl=start;
    if (end > maxr)
      maxr=end;
  }
  
  // we need at least 7 characters wide fields to accomodate the
  // labels
  if ((field1=snprintf(tmp,64, "%"PRIu64, maxl))<7)
    field1=7;
  if ((field2=snprintf(tmp,64, "%"PRIu64, maxr))<7)
    field2=7;

  // now print the five test spans
  pout(" SPAN  %*s  %*s  CURRENT_TEST_STATUS\n", field1, "MIN_LBA", field2, "MAX_LBA");

  for (i=0; i<5; i++) {
    uint64_t start=log->span[i].start;
    uint64_t end=log->span[i].end;
    
    if ((i+1)==(int)log->currentspan)
      // this span is currently under test
      pout("    %d  %*"PRIu64"  %*"PRIu64"  %s [%01d0%% left] (%"PRIu64"-%"PRIu64")\n",
	   i+1, field1, start, field2, end, msg,
	   (int)(sv->self_test_exec_status & 0xf), current, currentend);
    else
      // this span is not currently under test
      pout("    %d  %*"PRIu64"  %*"PRIu64"  Not_testing\n",
	   i+1, field1, start, field2, end);
  }  
  
  // if we are currently read-scanning, print LBAs and the status of
  // the read scan
  if (log->currentspan>5)
    pout("%5d  %*"PRIu64"  %*"PRIu64"  Read_scanning %s\n",
	 (int)log->currentspan, field1, current, field2, currentend,
	 OfflineDataCollectionStatus(sv->offline_data_collection_status));
  
  /* Print selective self-test flags.  Possible flag combinations are
     (numbering bits from 0-15):
     Bit-1 Bit-3   Bit-4
     Scan  Pending Active
     0     *       *       Don't scan
     1     0       0       Will carry out scan after selective test
     1     1       0       Waiting to carry out scan after powerup
     1     0       1       Currently scanning       
     1     1       1       Currently scanning
  */
  
  pout("Selective self-test flags (0x%x):\n", (unsigned int)log->flags);
  if (log->flags & SELECTIVE_FLAG_DOSCAN) {
    if (log->flags & SELECTIVE_FLAG_ACTIVE)
      pout("  Currently read-scanning the remainder of the disk.\n");
    else if (log->flags & SELECTIVE_FLAG_PENDING)
      pout("  Read-scan of remainder of disk interrupted; will resume %d min after power-up.\n",
	   (int)log->pendingtime);
    else
      pout("  After scanning selected spans, read-scan remainder of disk.\n");
  }
  else
    pout("  After scanning selected spans, do NOT read-scan remainder of disk.\n");
  
  // print pending time
  pout("If Selective self-test is pending on power-up, resume after %d minute delay.\n",
       (int)log->pendingtime);

  return; 
}

// Format SCT Temperature value
static const char * sct_ptemp(signed char x, char * buf)
{
  if (x == -128 /*0x80 = unknown*/)
    strcpy(buf, " ?");
  else
    sprintf(buf, "%2d", x);
  return buf;
}

static const char * sct_pbar(int x, char * buf)
{
  if (x <= 19)
    x = 0;
  else
    x -= 19;
  bool ov = false;
  if (x > 40) {
    x = 40; ov = true;
  }
  if (x > 0) {
    memset(buf, '*', x);
    if (ov)
      buf[x-1] = '+';
    buf[x] = 0;
  }
  else {
    buf[0] = '-'; buf[1] = 0;
  }
  return buf;
}

static const char * sct_device_state_msg(unsigned char state)
{
  switch (state) {
    case 0: return "Active";
    case 1: return "Stand-by";
    case 2: return "Sleep";
    case 3: return "DST executing in background";
    case 4: return "SMART Off-line Data Collection executing in background";
    case 5: return "SCT command executing in background";
    default:return "Unknown";
  }
}

// Print SCT Status
static int ataPrintSCTStatus(const ata_sct_status_response * sts)
{
  pout("SCT Status Version:                  %u\n", sts->format_version);
  pout("SCT Version (vendor specific):       %u (0x%04x)\n", sts->sct_version, sts->sct_version);
  pout("SCT Support Level:                   %u\n", sts->sct_spec);
  pout("Device State:                        %s (%u)\n",
    sct_device_state_msg(sts->device_state), sts->device_state);
  char buf1[20], buf2[20];
  if (   !sts->min_temp && !sts->life_min_temp && !sts->byte205
      && !sts->under_limit_count && !sts->over_limit_count     ) {
    // "Reserved" fields not set, assume "old" format version 2
    // Table 11 of T13/1701DT Revision 5
    // Table 54 of T13/1699-D Revision 3e
    pout("Current Temperature:                 %s Celsius\n",
      sct_ptemp(sts->hda_temp, buf1));
    pout("Power Cycle Max Temperature:         %s Celsius\n",
      sct_ptemp(sts->max_temp, buf2));
    pout("Lifetime    Max Temperature:         %s Celsius\n",
      sct_ptemp(sts->life_max_temp, buf2));
  }
  else {
    // Assume "new" format version 2 or version 3
    // T13/e06152r0-3 (Additional SCT Temperature Statistics)
    // Table 60 of T13/1699-D Revision 3f
    pout("Current Temperature:                    %s Celsius\n",
      sct_ptemp(sts->hda_temp, buf1));
    pout("Power Cycle Min/Max Temperature:     %s/%s Celsius\n",
      sct_ptemp(sts->min_temp, buf1), sct_ptemp(sts->max_temp, buf2));
    pout("Lifetime    Min/Max Temperature:     %s/%s Celsius\n",
      sct_ptemp(sts->life_min_temp, buf1), sct_ptemp(sts->life_max_temp, buf2));
    if (sts->byte205) // e06152r0-2, removed in e06152r3
      pout("Lifetime    Average Temperature:        %s Celsius\n",
        sct_ptemp((signed char)sts->byte205, buf1));
    pout("Under/Over Temperature Limit Count:  %2u/%u\n",
      sts->under_limit_count, sts->over_limit_count);
  }
  return 0;
}

// Print SCT Temperature History Table
static int ataPrintSCTTempHist(const ata_sct_temperature_history_table * tmh)
{
  char buf1[20], buf2[80];
  pout("SCT Temperature History Version:     %u\n", tmh->format_version);
  pout("Temperature Sampling Period:         %u minute%s\n",
    tmh->sampling_period, (tmh->sampling_period==1?"":"s"));
  pout("Temperature Logging Interval:        %u minute%s\n",
    tmh->interval,        (tmh->interval==1?"":"s"));
  pout("Min/Max recommended Temperature:     %s/%s Celsius\n",
    sct_ptemp(tmh->min_op_limit, buf1), sct_ptemp(tmh->max_op_limit, buf2));
  pout("Min/Max Temperature Limit:           %s/%s Celsius\n",
    sct_ptemp(tmh->under_limit, buf1), sct_ptemp(tmh->over_limit, buf2));
  pout("Temperature History Size (Index):    %u (%u)\n", tmh->cb_size, tmh->cb_index);
  if (!(0 < tmh->cb_size && tmh->cb_size <= sizeof(tmh->cb) && tmh->cb_index < tmh->cb_size)) {
    pout("Error invalid Temperature History Size or Index\n");
    return 0;
  }

  // Print table
  pout("\nIndex    Estimated Time   Temperature Celsius\n");
  unsigned n = 0, i = (tmh->cb_index+1) % tmh->cb_size;
  unsigned interval = (tmh->interval > 0 ? tmh->interval : 1);
  time_t t = time(0) - (tmh->cb_size-1) * interval * 60;
  t -= t % (interval * 60);
  while (n < tmh->cb_size) {
    // Find range of identical temperatures
    unsigned n1 = n, n2 = n+1, i2 = (i+1) % tmh->cb_size;
    while (n2 < tmh->cb_size && tmh->cb[i2] == tmh->cb[i]) {
      n2++; i2 = (i2+1) % tmh->cb_size;
    }
    // Print range
    while (n < n2) {
      if (n == n1 || n == n2-1 || n2 <= n1+3) {
        char date[30];
        // TODO: Don't print times < boot time
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&t));
        pout(" %3u    %s    %s  %s\n", i, date,
          sct_ptemp(tmh->cb[i], buf1), sct_pbar(tmh->cb[i], buf2));
      }
      else if (n == n1+1) {
        pout(" ...    ..(%3u skipped).    ..  %s\n",
          n2-n1-2, sct_pbar(tmh->cb[i], buf2));
      }
      t += interval * 60; i = (i+1) % tmh->cb_size; n++;
    }
  }
  //assert(n == tmh->cb_size && i == (tmh->cb_index+1) % tmh->cb_size);

  return 0;
}


// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
void failuretest(int type, int returnvalue){

  // If this is an error in an "optional" SMART command
  if (type==OPTIONAL_CMD){
    if (con->conservative){
      pout("An optional SMART command failed: exiting.  Remove '-T conservative' option to continue.\n");
      EXIT(returnvalue);
    }
    return;
  }

  // If this is an error in a "mandatory" SMART command
  if (type==MANDATORY_CMD){
    if (con->permissive--)
      return;
    pout("A mandatory SMART command failed: exiting. To continue, add one or more '-T permissive' options.\n");
    EXIT(returnvalue);
  }

  pout("Smartctl internal error in failuretest(type=%d). Please contact developers at " PACKAGE_HOMEPAGE "\n",type);
  EXIT(returnvalue|FAILCMD);
}

// Initialize to zero just in case some SMART routines don't work
static ata_identify_device drive;
static ata_smart_values smartval;
static ata_smart_thresholds_pvt smartthres;
static ata_smart_errorlog smarterror;
static ata_smart_selftestlog smartselftest;

int ataPrintMain (ata_device * device, const ata_print_options & options)
{
  int timewait,code;
  int returnval=0, retid=0, supported=0, needupdate=0;
  const char * powername = 0; char powerchg = 0;

  // If requested, check power mode first
  if (con->powermode) {
    unsigned char powerlimit = 0xff;
    int powermode = ataCheckPowerMode(device);
    switch (powermode) {
      case -1:
        if (errno == ENOSYS) {
          pout("CHECK POWER STATUS not implemented, ignoring -n Option\n"); break;
        }
        powername = "SLEEP";   powerlimit = 2;
        break;
      case 0:
        powername = "STANDBY"; powerlimit = 3; break;
      case 0x80:
        powername = "IDLE";    powerlimit = 4; break;
      case 0xff:
        powername = "ACTIVE or IDLE"; break;
      default:
        pout("CHECK POWER STATUS returned %d, not ATA compliant, ignoring -n Option\n", powermode);
        break;
    }
    if (powername) {
      if (con->powermode >= powerlimit) {
        pout("Device is in %s mode, exit(%d)\n", powername, FAILPOWER);
        return FAILPOWER;
      }
      powerchg = (powermode != 0xff); // SMART tests will spin up drives
    }
  }

  // Start by getting Drive ID information.  We need this, to know if SMART is supported.
  if ((retid=ataReadHDIdentity(device,&drive))<0){
    pout("Smartctl: Device Read Identity Failed (not an ATA/ATAPI device)\n\n");
    failuretest(MANDATORY_CMD, returnval|=FAILID);
  }

  // If requested, show which presets would be used for this drive and exit.
  if (con->showpresets) {
    showpresets(&drive);
    EXIT(0);
  }

  // Use preset vendor attribute options unless user has requested otherwise.
  if (!con->ignorepresets){
    apply_presets(&drive, con->attributedefs, con->fixfirmwarebug);
  }

  // Print most drive identity information if requested
  bool known = false;
  if (con->driveinfo){
    pout("=== START OF INFORMATION SECTION ===\n");
    known = ataPrintDriveInfo(&drive);
  }

  // Was this a packet device?
  if (retid>0){
    pout("SMART support is: Unavailable - Packet Interface Devices [this device: %s] don't support ATA SMART\n", packetdevicetype(retid-1));
    failuretest(MANDATORY_CMD, returnval|=FAILSMART);
  }
  
  // if drive does not supports SMART it's time to exit
  supported=ataSmartSupport(&drive);
  if (supported != 1){
    if (supported==0) {
      pout("SMART support is: Unavailable - device lacks SMART capability.\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
      pout("                  Checking to be sure by trying SMART ENABLE command.\n");
    }
    else {
      pout("SMART support is: Ambiguous - ATA IDENTIFY DEVICE words 82-83 don't show if SMART supported.\n");
      if (!known) failuretest(MANDATORY_CMD, returnval|=FAILSMART);
      pout("                  Checking for SMART support by trying SMART ENABLE command.\n");
    }

    if (ataEnableSmart(device)){
      pout("                  SMART ENABLE failed - this establishes that this device lacks SMART functionality.\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
      supported=0;
    }
    else {
      pout("                  SMART ENABLE appeared to work!  Continuing.\n");
      supported=1;
    }
    if (!con->driveinfo) pout("\n");
  }
  
  // Now print remaining drive info: is SMART enabled?    
  if (con->driveinfo){
    int ison=ataIsSmartEnabled(&drive),isenabled=ison;
    
    if (ison==-1) {
      pout("SMART support is: Ambiguous - ATA IDENTIFY DEVICE words 85-87 don't show if SMART is enabled.\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
      // check SMART support by trying a command
      pout("                  Checking to be sure by trying SMART RETURN STATUS command.\n");
      isenabled=ataDoesSmartWork(device);
    }
    else {
      pout("SMART support is: Available - device has SMART capability.\n");
      if (device->ata_identify_is_cached()) {
        pout("                  %sabled status cached by OS, trying SMART RETURN STATUS cmd.\n",
                    (isenabled?"En":"Dis"));
        isenabled=ataDoesSmartWork(device);
      }
    }

    if (isenabled)
      pout("SMART support is: Enabled\n");
    else {
      if (ison==-1)
        pout("SMART support is: Unavailable\n");
      else
        pout("SMART support is: Disabled\n");
    }
    // Print the (now possibly changed) power mode if available
    if (powername)
      pout("Power mode %s   %s\n", (powerchg?"was:":"is: "), powername);
    pout("\n");
  }
  
  // START OF THE ENABLE/DISABLE SECTION OF THE CODE
  if (con->smartenable || con->smartdisable || 
      con->smartautosaveenable || con->smartautosavedisable || 
      con->smartautoofflineenable || con->smartautoofflinedisable)
    pout("=== START OF ENABLE/DISABLE COMMANDS SECTION ===\n");
  
  // Enable/Disable SMART commands
  if (con->smartenable){
    if (ataEnableSmart(device)) {
      pout("Smartctl: SMART Enable Failed.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Enabled.\n");
  }
  
  // From here on, every command requires that SMART be enabled...
  if (!ataDoesSmartWork(device)) {
    pout("SMART Disabled. Use option -s with argument 'on' to enable it.\n");
    return returnval;
  }
  
  // Turn off SMART on device
  if (con->smartdisable){    
    if (ataDisableSmart(device)) {
      pout( "Smartctl: SMART Disable Failed.\n\n");
      failuretest(MANDATORY_CMD,returnval|=FAILSMART);
    }
    pout("SMART Disabled. Use option -s with argument 'on' to enable it.\n");
    return returnval;           
  }
  
  // Let's ALWAYS issue this command to get the SMART status
  code=ataSmartStatus2(device);
  if (code==-1)
    failuretest(MANDATORY_CMD, returnval|=FAILSMART);

  // Enable/Disable Auto-save attributes
  if (con->smartautosaveenable){
    if (ataEnableAutoSave(device)){
      pout( "Smartctl: SMART Enable Attribute Autosave Failed.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Attribute Autosave Enabled.\n");
  }
  if (con->smartautosavedisable){
    if (ataDisableAutoSave(device)){
      pout( "Smartctl: SMART Disable Attribute Autosave Failed.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Attribute Autosave Disabled.\n");
  }
  
  // for everything else read values and thresholds are needed
  if (ataReadSmartValues(device, &smartval)){
    pout("Smartctl: SMART Read Values failed.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }
  if (ataReadSmartThresholds(device, &smartthres)){
    pout("Smartctl: SMART Read Thresholds failed.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }

  // Enable/Disable Off-line testing
  if (con->smartautoofflineenable){
    if (!isSupportAutomaticTimer(&smartval)){
      pout("Warning: device does not support SMART Automatic Timers.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    needupdate=1;
    if (ataEnableAutoOffline(device)){
      pout( "Smartctl: SMART Enable Automatic Offline Failed.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Automatic Offline Testing Enabled every four hours.\n");
  }
  if (con->smartautoofflinedisable){
    if (!isSupportAutomaticTimer(&smartval)){
      pout("Warning: device does not support SMART Automatic Timers.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    needupdate=1;
    if (ataDisableAutoOffline(device)){
      pout("Smartctl: SMART Disable Automatic Offline Failed.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Automatic Offline Testing Disabled.\n");
  }

  if (needupdate && ataReadSmartValues(device, &smartval)){
    pout("Smartctl: SMART Read Values failed.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }

  // all this for a newline!
  if (con->smartenable || con->smartdisable || 
      con->smartautosaveenable || con->smartautosavedisable || 
      con->smartautoofflineenable || con->smartautoofflinedisable)
    pout("\n");

  // START OF READ-ONLY OPTIONS APART FROM -V and -i
  if (   con->checksmart || con->generalsmartvalues || con->smartvendorattrib || con->smarterrorlog
      || con->smartselftestlog || con->selectivetestlog || con->scttempsts || con->scttemphist
      || options.smart_ext_error_log                                                               )
    pout("=== START OF READ SMART DATA SECTION ===\n");
  
  // Check SMART status (use previously returned value)
  if (con->checksmart){
    switch (code) {

    case 0:
      // The case where the disk health is OK
      pout("SMART overall-health self-assessment test result: PASSED\n");
      if (ataCheckSmart(&smartval, &smartthres,0)){
        if (con->smartvendorattrib)
          pout("See vendor-specific Attribute list for marginal Attributes.\n\n");
        else {
          PRINT_ON(con);
          pout("Please note the following marginal Attributes:\n");
          PrintSmartAttribWithThres(&smartval, &smartthres,2);
        } 
        returnval|=FAILAGE;
      }
      else
        pout("\n");
      break;
      
    case 1:
      // The case where the disk health is NOT OK
      PRINT_ON(con);
      pout("SMART overall-health self-assessment test result: FAILED!\n"
           "Drive failure expected in less than 24 hours. SAVE ALL DATA.\n");
      PRINT_OFF(con);
      if (ataCheckSmart(&smartval, &smartthres,1)){
        returnval|=FAILATTR;
        if (con->smartvendorattrib)
          pout("See vendor-specific Attribute list for failed Attributes.\n\n");
        else {
          PRINT_ON(con);
          pout("Failed Attributes:\n");
          PrintSmartAttribWithThres(&smartval, &smartthres,1);
        }
      }
      else
        pout("No failed Attributes found.\n\n");   
      returnval|=FAILSTATUS;
      PRINT_OFF(con);
      break;

    case -1:
    default:
      // The case where something went wrong with HDIO_DRIVE_TASK ioctl()
      if (ataCheckSmart(&smartval, &smartthres,1)){
        PRINT_ON(con);
        pout("SMART overall-health self-assessment test result: FAILED!\n"
             "Drive failure expected in less than 24 hours. SAVE ALL DATA.\n");
        PRINT_OFF(con);
        returnval|=FAILATTR;
        returnval|=FAILSTATUS;
        if (con->smartvendorattrib)
          pout("See vendor-specific Attribute list for failed Attributes.\n\n");
        else {
          PRINT_ON(con);
          pout("Failed Attributes:\n");
          PrintSmartAttribWithThres(&smartval, &smartthres,1);
        }
      }
      else {
        pout("SMART overall-health self-assessment test result: PASSED\n");
        if (ataCheckSmart(&smartval, &smartthres,0)){
          if (con->smartvendorattrib)
            pout("See vendor-specific Attribute list for marginal Attributes.\n\n");
          else {
            PRINT_ON(con);
            pout("Please note the following marginal Attributes:\n");
            PrintSmartAttribWithThres(&smartval, &smartthres,2);
          } 
          returnval|=FAILAGE;
        }
        else
          pout("\n");
      } 
      PRINT_OFF(con);
      break;
    } // end of switch statement
    
    PRINT_OFF(con);
  } // end of checking SMART Status
  
  // Print general SMART values
  if (con->generalsmartvalues)
    ataPrintGeneralSmartValues(&smartval, &drive); 

  // Print vendor-specific attributes
  if (con->smartvendorattrib){
    PRINT_ON(con);
    PrintSmartAttribWithThres(&smartval, &smartthres,con->printing_switchable?2:0);
    PRINT_OFF(con);
  }

  // Print SMART and/or GP log Directory and/or logs
  if (   options.gp_logdir || options.smart_logdir
      || options.sataphy || options.smart_ext_error_log
      || !options.log_requests.empty()                 ) {

    bool gpl_cap = !!isGeneralPurposeLoggingCapable(&drive);
    bool gpl_try = gpl_cap;
    if (!gpl_try && con->permissive) {
        con->permissive--;
        gpl_try = true;
    }

    if (!gpl_try) {
      pout("Device does not support General Purpose Loggin (GPL) feature set\n"
           "(override with '-T permissive' option)\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      PRINT_ON(con);
      if (gpl_cap)
        pout("General Purpose Logging (GPL) feature set supported\n");

      // Detect directories needed
      bool need_smart_logdir = options.smart_logdir;
      bool need_gp_logdir    = options.gp_logdir || options.smart_ext_error_log;
      unsigned i;
      for (i = 0; i < options.log_requests.size(); i++) {
        if (options.log_requests[i].gpl)
          need_gp_logdir = true;
        else
          need_smart_logdir = true;
      }

      ata_smart_log_directory smartlogdir_buf, gplogdir_buf;
      const ata_smart_log_directory * smartlogdir = 0, * gplogdir = 0;

      // Read SMART Log directory
      if (need_smart_logdir) {
        if (ataReadLogDirectory(device, &smartlogdir_buf, false)){
          PRINT_OFF(con);
          pout("Read SMART Log Directory failed.\n\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        else
          smartlogdir = &smartlogdir_buf;
      }
      PRINT_ON(con);

      // Read GP Log directory
      if (need_gp_logdir) {
        if (ataReadLogDirectory(device, &gplogdir_buf, true)){
          PRINT_OFF(con);
          pout("Read GP Log Directory failed.\n\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        else
          gplogdir = &gplogdir_buf;
      }
      PRINT_ON(con);

      // Print log directories
      if ((options.gp_logdir && gplogdir) || (options.smart_logdir && smartlogdir))
        PrintLogDirectories(gplogdir, smartlogdir);
      PRINT_OFF(con);

      // Print log pages
      for (i = 0; i < options.log_requests.size(); i++) {
        const ata_log_request & req = options.log_requests[i];

        const char * type;
        unsigned max_nsectors;
        if (req.gpl) {
          type = "General Purpose";
          max_nsectors = GetNumLogSectors(gplogdir, req.logaddr, true);
        }
        else {
          type = "SMART";
          max_nsectors = GetNumLogSectors(smartlogdir, req.logaddr, false);
        }

        if (!max_nsectors) {
          if (!con->permissive) {
            pout("%s Log 0x%02x does not exist (override with '-T permissive' option)\n", type, req.logaddr);
            continue;
          }
          con->permissive--;
          max_nsectors = req.page+1;
        }
        if (max_nsectors <= req.page) {
          pout("%s Log 0x%02x has only %u sectors, output skipped\n", type, req.logaddr, max_nsectors);
          continue;
        }

        unsigned ns = req.nsectors;
        if (ns > max_nsectors - req.page) {
          if (req.nsectors != ~0U) // "FIRST-max"
            pout("%s Log 0x%02x has only %u sectors, output truncated\n", type, req.logaddr, max_nsectors);
          ns = max_nsectors - req.page;
        }

        // SMART log don't support sector offset, start with first sector
        unsigned offs = (req.gpl ? 0 : req.page);

        raw_buffer log_buf((offs + ns) * 512);
        bool ok;
        if (req.gpl)
          ok = ataReadLogExt(device, req.logaddr, 0x00, req.page, log_buf.data(), ns);
        else
          ok = ataReadSmartLog(device, req.logaddr, log_buf.data(), offs + ns);
        if (!ok)
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        else
          PrintLogPages(type, log_buf.data() + offs*512, req.logaddr, req.page, ns, max_nsectors);
      }

      // Print SMART Extendend Comprehensive Error Log
      if (options.smart_ext_error_log) {
        unsigned nsectors = GetNumLogSectors(gplogdir, 0x03, true);
        if (!nsectors)
          pout("SMART Extended Comprehensive Error Log (GP Log 0x03) does not exist\n");
        else if (nsectors >= 256)
          pout("SMART Extended Comprehensive Error Log size %u not supported\n", nsectors);
        else {
          raw_buffer log_03_buf(nsectors * 512);
          ata_smart_exterrlog * log_03 = (ata_smart_exterrlog *)log_03_buf.data();
          if (!ataReadExtErrorLog(device, log_03, nsectors))
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          else
            PrintSmartExtErrorLog(log_03, nsectors, options.smart_ext_error_log);
        }
      }

      // Print SATA Phy Event Counters
      if (options.sataphy) {
        unsigned char log_11[512] = {0, };
        unsigned char features = (options.sataphy_reset ? 0x01 : 0x00);
        if (!ataReadLogExt(device, 0x11, features, 0, log_11, 1))
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        else
          PrintSataPhyEventCounters(log_11, options.sataphy_reset);
      }
    }
  }

  // Print SMART error log
  if (con->smarterrorlog){
    if (!isSmartErrorLogCapable(&smartval, &drive)){
      pout("Warning: device does not support Error Logging\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    if (ataReadErrorLog(device, &smarterror)){
      pout("Smartctl: SMART Error Log Read Failed\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      // quiet mode is turned on inside ataPrintSmartErrorLog()
      if (ataPrintSmartErrorlog(&smarterror))
	returnval|=FAILERR;
      PRINT_OFF(con);
    }
  }
  
  // Print SMART self-test log
  if (con->smartselftestlog){
    if (!isSmartTestLogCapable(&smartval, &drive)){
      pout("Warning: device does not support Self Test Logging\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }    
    if(ataReadSelfTestLog(device, &smartselftest)){
      pout("Smartctl: SMART Self Test Log Read Failed\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      PRINT_ON(con);
      if (ataPrintSmartSelfTestlog(&smartselftest,!con->printing_switchable))
	returnval|=FAILLOG;
      PRINT_OFF(con);
      pout("\n");
    }
  }

  // Print SMART selective self-test log
  if (con->selectivetestlog){
    ata_selective_self_test_log log;

    if (!isSupportSelectiveSelfTest(&smartval))
      pout("Device does not support Selective Self Tests/Logging\n");
    else if(ataReadSelectiveSelfTestLog(device, &log)) {
      pout("Smartctl: SMART Selective Self Test Log Read Failed\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else {
      PRINT_ON(con);
      // If any errors were found, they are logged in the SMART Self-test log.
      // So there is no need to print the Selective Self Test log in silent
      // mode.
      if (!con->printing_switchable) ataPrintSelectiveSelfTestLog(&log, &smartval);
      PRINT_OFF(con);
      pout("\n");
    }
  }

  // Print SMART SCT status and temperature history table
  if (con->scttempsts || con->scttemphist || con->scttempint) {
    for (;;) {
      if (!isSCTCapable(&drive)) {
        pout("Warning: device does not support SCT Commands\n");
        failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        break;
      }
      if (con->scttempsts || con->scttemphist) {
        ata_sct_status_response sts;
        ata_sct_temperature_history_table tmh;
        if (!con->scttemphist) {
          // Read SCT status only
          if (ataReadSCTStatus(device, &sts)) {
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
            break;
          }
        }
        else {
          if (!isSCTDataTableCapable(&drive)) {
            pout("Warning: device does not support SCT Data Table command\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
            break;
          }
          // Read SCT status and temperature history
          if (ataReadSCTTempHist(device, &tmh, &sts)) {
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
            break;
          }
        }
        if (con->scttempsts)
          ataPrintSCTStatus(&sts);
        if (con->scttemphist)
          ataPrintSCTTempHist(&tmh);
        pout("\n");
      }
      if (con->scttempint) {
        // Set new temperature logging interval
        if (!isSCTFeatureControlCapable(&drive)) {
          pout("Warning: device does not support SCT Feature Control command\n");
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          break;
        }
        if (ataSetSCTTempInterval(device, con->scttempint, !!con->scttempintp)) {
          failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
          break;
        }
        pout("Temperature Logging Interval set to %u minute%s (%s)\n",
          con->scttempint, (con->scttempint==1?"":"s"), (con->scttempintp?"persistent":"volatile"));
      }
      break;
    }
  }

  // START OF THE TESTING SECTION OF THE CODE.  IF NO TESTING, RETURN
  if (con->testcase==-1)
    return returnval;
  
  pout("=== START OF OFFLINE IMMEDIATE AND SELF-TEST SECTION ===\n");
  // if doing a self-test, be sure it's supported by the hardware
  switch (con->testcase){
  case OFFLINE_FULL_SCAN:
    if (!isSupportExecuteOfflineImmediate(&smartval)){
      pout("Warning: device does not support Execute Offline Immediate function.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    break;
  case ABORT_SELF_TEST:
  case SHORT_SELF_TEST:
  case EXTEND_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    if (!isSupportSelfTest(&smartval)){
      pout("Warning: device does not support Self-Test functions.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    break;
  case CONVEYANCE_SELF_TEST:
  case CONVEYANCE_CAPTIVE_SELF_TEST:
    if (!isSupportConveyanceSelfTest(&smartval)){
      pout("Warning: device does not support Conveyance Self-Test functions.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    break;
  case SELECTIVE_SELF_TEST:
  case SELECTIVE_CAPTIVE_SELF_TEST:
    if (!isSupportSelectiveSelfTest(&smartval)){
      pout("Warning: device does not support Selective Self-Test functions.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    break;
  default:
    pout("Internal error in smartctl: con->testcase==%d not recognized\n", (int)con->testcase);
    pout("Please contact smartmontools developers at %s.\n", PACKAGE_BUGREPORT);
    EXIT(returnval|=FAILCMD);
  }

  // Now do the test.  Note ataSmartTest prints its own error/success
  // messages
  if (ataSmartTest(device, con->testcase, &smartval, get_num_sectors(&drive)))
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  else {  
    // Tell user how long test will take to complete.  This is tricky
    // because in the case of an Offline Full Scan, the completion
    // timer is volatile, and needs to be read AFTER the command is
    // given. If this will interrupt the Offline Full Scan, we don't
    // do it, just warn user.
    if (con->testcase==OFFLINE_FULL_SCAN){
      if (isSupportOfflineAbort(&smartval))
	pout("Note: giving further SMART commands will abort Offline testing\n");
      else if (ataReadSmartValues(device, &smartval)){
	pout("Smartctl: SMART Read Values failed.\n");
	failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
    }
    
    // Now say how long the test will take to complete
    if ((timewait=TestTime(&smartval,con->testcase))){ 
      time_t t=time(NULL);
      if (con->testcase==OFFLINE_FULL_SCAN) {
	t+=timewait;
	pout("Please wait %d seconds for test to complete.\n", (int)timewait);
      } else {
	t+=timewait*60;
	pout("Please wait %d minutes for test to complete.\n", (int)timewait);
      }
      pout("Test will complete after %s\n", ctime(&t));
      
      if (con->testcase!=SHORT_CAPTIVE_SELF_TEST && 
	  con->testcase!=EXTEND_CAPTIVE_SELF_TEST && 
	  con->testcase!=CONVEYANCE_CAPTIVE_SELF_TEST && 
	  con->testcase!=SELECTIVE_CAPTIVE_SELF_TEST)
	pout("Use smartctl -X to abort test.\n"); 
    }
  }

  return returnval;
}
