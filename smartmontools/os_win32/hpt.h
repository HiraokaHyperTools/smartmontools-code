/*
 * os_win32/hpt.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2021 kenjiuno
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#define HPT_CTL_CODE(x) CTL_CODE(0x370, 0x900+(x), 0, 0)
#define HPT_IOCTL_CODE_GET_VERSION (0)
#define HPT_IOCTL_CODE_GET_CHANNEL_INFO (3)
#define HPT_IOCTL_CODE_GET_LOGICAL_DEVICES (4)
#define HPT_IOCTL_CODE_GET_DEVICE_INFO (5)
#define HPT_IOCTL_CODE_IDE_PASS_THROUGH (24)
#define HPT_IOCTL_CODE_GET_CONTROLLER_INFO_V2 (47)
#define HPT_IOCTL_CODE_GET_PHYSICAL_DEVICES (60)
#define HPT_IOCTL_CODE_SCSI_PASS_THROUGH (59)

#define HPT_INTERFACE_VERSION 0x02010000

#define MAX_ARRAY_MEMBERS_V1 8
#define MAX_ARRAYNAME_LEN 16
#define MAX_NAME_LENGTH 36

#define LDT_ARRAY  1
#define LDT_DEVICE 2

#define PDT_UNKNOWN  0
#define PDT_HARDDISK 1
#define PDT_CDROM    2
#define PDT_TAPE     3

#define AT_UNKNOWN 0
#define AT_RAID0   1
#define AT_RAID1   2
#define AT_RAID5   3
#define AT_RAID6   4
#define AT_RAID3   5
#define AT_RAID4   6
#define AT_JBOD    7
#define AT_RAID1E  8

#define IO_COMMAND_READ  1
#define IO_COMMAND_WRITE 2

#define DEVICE_FLAG_SATA 0x00000010 /* SATA or SAS device */
#define DEVICE_FLAG_SAS  0x00000040 /* SAS device */

#pragma pack(push,1)

// taken some from vendor header files
namespace os_win32::hpt
{
  typedef struct {
    uint32_t seconds:6;      /* 0 - 59 */
    uint32_t minutes:6;      /* 0 - 59 */
    uint32_t month:4;        /* 1 - 12 */
    uint32_t hours:6;        /* 0 - 59 */
    uint32_t day:5;          /* 1 - 31 */
    uint32_t year:5;         /* 0=2000, 31=2031 */
  } TIME_RECORD;

  typedef struct {
    char name[MAX_ARRAYNAME_LEN];
    char description[64];
    char create_manager[16];
    TIME_RECORD create_time;

    uint8_t array_type;
    uint8_t block_size_shift;
    uint8_t member_count;
    uint8_t sub_array_type;

    uint32_t flags;
    uint32_t members[MAX_ARRAY_MEMBERS_V1];

    uint32_t rebuilding_progress;
    uint32_t rebuilt_sectors;

  } HPT_ARRAY_INFO;

  typedef struct {
    uint16_t general_configuration;
    uint16_t number_of_cylinders;
    uint16_t reserved1;
    uint16_t number_of_heads;
    uint16_t unformatted_bytes_per_track;
    uint16_t unformatted_bytes_per_sector;
    uint8_t sas_address[8];
    uint16_t serial_number[10];
    uint16_t buffer_type;
    uint16_t buffer_sector_size;
    uint16_t number_of_ecc_bytes;
    uint16_t firmware_revision[4];
    uint16_t model_number[20];
    uint8_t maximum_block_transfer;
    uint8_t vendor_unique2;
    uint16_t double_word_io;
    uint16_t capabilities;
    uint16_t reserved2;
    uint8_t vendor_unique3;
    uint8_t pio_cycle_timing_mode;
    uint8_t vendor_unique4;
    uint8_t dma_cycle_timing_mode;
    uint16_t translation_fields_valid;
    uint16_t number_of_current_cylinders;
    uint16_t number_of_current_heads;
    uint16_t current_sectors_per_track;
    uint32_t current_sector_capacity;
    uint16_t current_multi_sector_setting;
    uint32_t user_addressable_sectors;
    uint8_t single_word_dmasupport;
    uint8_t single_word_dmaactive;
    uint8_t multi_word_dmasupport;
    uint8_t multi_word_dmaactive;
    uint8_t advanced_piomodes;
    uint8_t reserved4;
    uint16_t minimum_mwxfer_cycle_time;
    uint16_t recommended_mwxfer_cycle_time;
    uint16_t minimum_piocycle_time;
    uint16_t minimum_piocycle_time_iordy;
    uint16_t reserved5[2];
    uint16_t release_time_overlapped;
    uint16_t release_time_service_command;
    uint16_t major_revision;
    uint16_t minor_revision;
    
  } IDENTIFY_DATA2;

  typedef struct {
    uint8_t controller_id;
    uint8_t bus_id;
    uint8_t target_id;
    uint8_t device_mode_setting;
    uint8_t device_type;
    uint8_t usable_mode;

    uint8_t read_ahead_supported: 1;
    uint8_t read_ahead_enabled: 1;
    uint8_t write_cache_supported: 1;
    uint8_t write_cache_enabled: 1;
    uint8_t tcq_supported: 1;
    uint8_t tcq_enabled: 1;
    uint8_t ncq_supported: 1;
    uint8_t ncq_enabled: 1;
    uint8_t spin_up_mode: 2;
    uint8_t reserved6: 6;

    uint32_t flags;

    IDENTIFY_DATA2 identify_data;

  } DEVICE_INFO;

  typedef struct {
    uint8_t type; // LDT_ARRAY or LDT_DEVICE
    uint8_t reserved[3];

    uint32_t capacity;
    uint32_t parent_array;

    union {
      HPT_ARRAY_INFO array;
      DEVICE_INFO device;
    } un;

  } LOGICAL_DEVICE_INFO;

  typedef struct {
    uint8_t chip_type;
    uint8_t interrupt_level;
    uint8_t num_buses;
    uint8_t chip_flags;

    uint8_t sz_product_id[MAX_NAME_LENGTH];
    uint8_t sz_vendor_id[MAX_NAME_LENGTH];

    uint32_t group_id;
    uint8_t pci_tree;
    uint8_t pci_bus;
    uint8_t pci_device;
    uint8_t pci_function;

    uint32_t ex_flags;
  } CONTROLLER_INFO_V2;

  typedef struct {
    uint32_t id_disk;
    uint8_t features_reg;
    uint8_t sector_count_reg;
    uint8_t lba_low_reg;
    uint8_t lba_mid_reg;
    uint8_t lba_high_reg;
    uint8_t drive_head_reg;
    uint8_t command_reg;
    uint8_t sectors;
    uint8_t protocol;
    uint8_t reserve[3];
    //uint8_t data_buffer[0];
  } IDE_PASS_THROUGH_HEADER;

  typedef struct {
    uint32_t id_disk;
    uint8_t protocol;
    uint8_t reserve1;
    uint8_t reserve2;
    uint8_t cdb_length;
    uint8_t cdb[16];
    uint32_t data_length;
  } SCSI_PASS_THROUGH_IN;

  typedef struct {
    uint8_t scsi_status;
    uint8_t reserve1;
    uint8_t reserve2;
    uint8_t reserve3;
    uint32_t data_length;
  } SCSI_PASS_THROUGH_OUT;

  typedef struct {
    uint32_t io_port;
    uint32_t control_port;

    uint32_t devices[2];

  } CHANNEL_INFO;
}

// smartmontools specific
namespace os_win32::hpt
{
  typedef struct {
    uint32_t in_len;
    uint32_t out_len;
  } IO_LEN;

  /**
   * Represents a buffer layout suitable for HPT HBA `IOCTL_SCSI_MINIPORT` ioctl request.
   * 
   * Layout:
   * 
   * 1. `IOCTL_HEADER`
   * 2. `IO_LEN`
   * 3. input data (written by user)
   * 4. output data (written by driver)
   * 
   * The actual data format is decided by `IOCTL_HEADER.ControlCode`.
   */
  class buffer_layouter {
  public:
    buffer_layouter(unsigned in_len, unsigned out_len)
    : m_blk(sizeof(IOCTL_HEADER) + sizeof(IO_LEN) + in_len + out_len)
    , m_io_len(nullptr)
    , m_in_data(nullptr)
    , m_out_data(nullptr)
    {
      unsigned char * buf = m_blk.data();
      if (buf) {
        m_io_len = reinterpret_cast<IO_LEN *>(buf + sizeof(IOCTL_HEADER));
        m_in_data = buf + sizeof(IOCTL_HEADER) + sizeof(IO_LEN);
        m_out_data = buf + sizeof(IOCTL_HEADER) + sizeof(IO_LEN) + in_len;

        m_io_len->in_len = in_len;
        m_io_len->out_len = out_len;
      }
    }

    bool allocated() const { return m_blk.data() != nullptr; }
    IOCTL_HEADER * ioctl_header() { return reinterpret_cast<IOCTL_HEADER *>(m_blk.data()); }
    IO_LEN * io_len() { return m_io_len; }
    void * in_data() { return m_in_data; }
    void * out_data() { return m_out_data; }
    unsigned total_size() const { return m_blk.size(); }

  private:
    raw_buffer m_blk;
    IO_LEN * m_io_len;
    void * m_in_data;
    void * m_out_data;

    buffer_layouter(const buffer_layouter &);
    void operator =(const buffer_layouter &);
  };

}

#pragma pack(pop)
