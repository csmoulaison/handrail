#ifndef handrail_coff_h_INCLUDED
#define handrail_coff_h_INCLUDED

// MSVC's COFF format, along with the PE format, is documented here:
// https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
//
// Constants are defined here rather than taken from windows.h so the object
// can be written from any host platform.
#define COFF_MACHINE_AMD64             0x8664
#define COFF_SECTION_INITIALIZED_DATA  0x00000040
#define COFF_SECTION_ALIGN_16_BYTES    0x00500000
#define COFF_SECTION_MEMORY_READ       0x40000000
#define COFF_SYMBOL_CLASS_EXTERNAL     2

#pragma pack(push, 1)
typedef struct {
    u16 machine;
    u16 section_count;
    u32 time_date_stamp;
    u32 symbol_table_offset;
    u32 symbol_count;
    u16 optional_header_size;
    u16 characteristics;
} CoffHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char name[8];
    u32  virtual_size;
    u32  virtual_address;
    u32  raw_data_size;
    u32  raw_data_offset;
    u32  relocations_offset;
    u32  line_numbers_offset;
    u16  relocation_count;
    u16  line_number_count;
    u32  characteristics;
} CoffSectionHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    char name[8];
    u32  value;
    u16  section;
    u16  type;
    u8   storage_class;
    u8   aux_count;
} CoffSymbol;
#pragma pack(pop)

// Write data as a linkable COFF object exporting it under symbol_name (8 chars max).
void coff_output_binary_object(void* data, u64 size, String coff_path, String symbol_name);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_coff_h_IMPLEMENTED)
#define handrail_coff_h_IMPLEMENTED

#include <time.h>

void coff_output_binary_object(void* data, u64 size, String coff_path, String symbol_name) {
    assert(symbol_name.len <= 8);
    assert(size % 4 == 0);
    assert(sizeof(CoffHeader) == 20);
    assert(sizeof(CoffSectionHeader) == 40);
    assert(sizeof(CoffSymbol) == 18);

    // Write COFF file, embedding data and symbol name
    File coff_file = file_open(coff_path, FILE_OPEN_WRITE);
    u32 raw_data_offset = sizeof(CoffHeader) + sizeof(CoffSectionHeader);

    CoffHeader header = {};
    header.machine             = COFF_MACHINE_AMD64;
    header.section_count       = 1;
    header.time_date_stamp     = (u32)time(NULL);
    header.symbol_table_offset = raw_data_offset + (u32)size;
    header.symbol_count        = 1;
    file_write(&coff_file, &header, sizeof(CoffHeader));

    CoffSectionHeader section_header = {};
    memcpy(section_header.name, ".rdata", 7);
    section_header.raw_data_size   = (u32)size;
    section_header.raw_data_offset = raw_data_offset;
    section_header.characteristics = COFF_SECTION_INITIALIZED_DATA | COFF_SECTION_ALIGN_16_BYTES | COFF_SECTION_MEMORY_READ;
    file_write(&coff_file, &section_header, sizeof(CoffSectionHeader));
    file_write(&coff_file, data, size);

    CoffSymbol symbol = {};
    memcpy(symbol.name, symbol_name.text, symbol_name.len);
    symbol.section       = 1;
    symbol.storage_class = COFF_SYMBOL_CLASS_EXTERNAL;
    file_write(&coff_file, &symbol, sizeof(CoffSymbol));

    u32 string_table_size = 4;
    file_write(&coff_file, &string_table_size, 4);

    file_close(&coff_file);
}

#endif
