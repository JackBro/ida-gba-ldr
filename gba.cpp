
#include "../idaldr.h"
#include "gba.hpp"

//----------------------------------------------------------------------------
static void map_ewram()
{
  segment_t s;
  s.startEA = 0x2000000;
  s.endEA = 0x2040000;
  s.type = SEG_IMEM;
  s.bitness = 1;
  s.sel = allocate_selector(0);

  char seg_name[0x10];
  qsnprintf(seg_name, sizeof(seg_name), "EWRAM");
  if (!add_segm_ex(&s, seg_name, NULL, ADDSEG_NOSREG))
    loader_failure("Failed adding %s segment\n", seg_name);
}

//----------------------------------------------------------------------------
static void map_iwram()
{
  segment_t s;
  s.startEA = 0x3000000;
  s.endEA = 0x3008000;
  s.type = SEG_IMEM;
  s.bitness = 1;
  s.sel = allocate_selector(0);

  char seg_name[0x10];
  qsnprintf(seg_name, sizeof(seg_name), "IWRAM");
  if (!add_segm_ex(&s, seg_name, NULL, ADDSEG_NOSREG))
    loader_failure("Failed adding %s segment\n", seg_name);
}

//----------------------------------------------------------------------------
static sel_t map_rom(linput_t *li, uint32 rom_size)
{
  // NOTE: multiboot rom is not supported at the moment
  bool succeeded = false;

  segment_t s;
  s.startEA = 0x8000000;
  s.endEA   = 0xA000000;
  s.sel     = allocate_selector(0);

  int32 rom_offset = 0;
  if ( !file2base(li, rom_offset, s.startEA, s.startEA + rom_size, FILEREG_PATCHABLE) )
    loader_failure("Failed mapping 0x%x -> [0x%a, 0x%a)\n", rom_offset, s.startEA, s.endEA);

  succeeded = add_segm(s.sel, s.startEA, s.endEA, "ROM", NULL);
  if ( succeeded )
    succeeded = true;
  else
    loader_failure("Failed adding ROM segment\n");

  return s.sel;
}

//----------------------------------------------------------------------------
static uint32 get_arm_branch_destination(uint32 current_address, uint32 instruction)
{
  if ((instruction & 0x0A000000) != 0x0A000000) {
    return 0;
  }

  int32 offset = (instruction & 0xFFFFFF);
  if ((offset & 0x800000) != 0) {
    offset |= ~0xFFFFFF;
  }

  return current_address + 8 + (offset * 4);
}

//----------------------------------------------------------------------------
static bool is_uppercase_alnum(uint8 * data, size_t size, bool variable_length)
{
  for ( size_t offset = 0; offset < size; offset++ ) {
    if ( ( data[offset] < '0' || data[offset] > '9' ) &&
       ( data[offset] < 'A' || data[offset] > 'Z' ) &&
       ( data[offset] != ' ' ) &&
       ( variable_length && data[offset] != 0 ) ) {
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
static bool is_gba_rom(linput_t *li)
{
  int32 rom_size = qlsize(li);
  if ( rom_size < 192 ) {
    return false;
  }

  int32 original_pos = qltell(li);
  uint8 header[192];
  qlseek(li, 0);
  qlread(li, header, 192);
  qlseek(li, original_pos, SEEK_SET);

  int score = 0;

  // first 32bit ARM branch opcode
  uint32 startop = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
  if ( ( startop & 0xFFC00000 ) == 0xEA000000 ) { // `B rom_start`
    score += 2;

    uint32 dest = get_arm_branch_destination(0, startop);
    if ( dest == 0xC0 ) {  // opcode EA00002E
      score += 2;
    }
    else if ( dest >= (uint32)rom_size ) {
      score -= 2;
    }
  }
  else {
    score -= 4;
  }

  // compressed logo bitmap
  const uint8 NINTENDO_LOGO[156] = {
      0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21, 0x3D, 0x84, 0x82, 0x0A, 0x84, 0xE4, 0x09, 0xAD,
      0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21, 0xA3, 0x52, 0xBE, 0x19, 0x93, 0x09, 0xCE, 0x20,
      0x10, 0x46, 0x4A, 0x4A, 0xF8, 0x27, 0x31, 0xEC, 0x58, 0xC7, 0xE8, 0x33, 0x82, 0xE3, 0xCE, 0xBF,
      0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0xC1, 0x94, 0x56, 0x8A, 0xC0, 0x13, 0x72, 0xA7, 0xFC,
      0x9F, 0x84, 0x4D, 0x73, 0xA3, 0xCA, 0x9A, 0x61, 0x58, 0x97, 0xA3, 0x27, 0xFC, 0x03, 0x98, 0x76,
      0x23, 0x1D, 0xC7, 0x61, 0x03, 0x04, 0xAE, 0x56, 0xBF, 0x38, 0x84, 0x00, 0x40, 0xA7, 0x0E, 0xFD,
      0xFF, 0x52, 0xFE, 0x03, 0x6F, 0x95, 0x30, 0xF1, 0x97, 0xFB, 0xC0, 0x85, 0x60, 0xD6, 0x80, 0x25,
      0xA9, 0x63, 0xBE, 0x03, 0x01, 0x4E, 0x38, 0xE2, 0xF9, 0xA2, 0x34, 0xFF, 0xBB, 0x3E, 0x03, 0x44,
      0x78, 0x00, 0x90, 0xCB, 0x88, 0x11, 0x3A, 0x94, 0x65, 0xC0, 0x7C, 0x63, 0x87, 0xF0, 0x3C, 0xAF,
      0xD6, 0x25, 0xE4, 0x8B, 0x38, 0x0A, 0xAC, 0x72, 0x21, 0xD4, 0xF8, 0x07,
  };
  if ( memcmp(&header[4], NINTENDO_LOGO, 156) == 0 ) {
    score += 2;
  }

  // checksum
  uint8 check = 0;
  for (int32 offset = 0xA0; offset <= 0xBC; offset++) {
    check -= header[offset];
  }
  check -= 0x19;
  // a valid checksum is the biggest indicator of a valid header
  if ( check == header[0xBD] ) {
    score += 4;
  }

  // game title
  if ( is_uppercase_alnum(&header[0xA0], 12, true) ) {
    score++;
  }

  // game code
  if ( is_uppercase_alnum(&header[0xAC], 4, false) ) {
    score++;
  }

  // maker code
  if ( is_uppercase_alnum(&header[0xB0], 2, false) ) {
    score++;
  }

  // fixed 96h
  if ( header[0xB2] == 0x96 ) {
    score++;
  }

  score = qmax(score, 0);
  return score >= 8;
}

//----------------------------------------------------------------------------
int idaapi accept_file(
        linput_t *li,
        char fileformatname[MAX_FILE_FORMAT_NAME],
        int n)
{
  if ( n > 0 )
    return 0;

  if ( !is_gba_rom(li) )
    return 0;

  qstrncpy(fileformatname, "Gameboy Advance ROM", MAX_FILE_FORMAT_NAME);
  return 1;
}

//----------------------------------------------------------------------------
void idaapi load_file(linput_t *li, ushort /*neflags*/, const char * /*ffn*/)
{
  // One should always set the processor type
  // as early as possible: IDA will draw some
  // informations from it; e.g., the size of segments.
  set_processor_type("arm", SETPROC_ALL|SETPROC_FATAL);

  int32 rom_size = qmin(qlsize(li), 0x2000000);

  uint8 header[192];
  qlseek(li, 0);
  qlread(li, header, 192);

  // ROM
  inf.start_cs = map_rom(li, rom_size);

  // EWRAM
  map_ewram();

  // IWRAM
  map_iwram();

  // start address
  inf.startIP = 0x8000000;
  auto_make_code(inf.startIP);

  // get entrypoint from first 32bit ARM branch opcode
  uint32 startop = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
  if ( ( startop & 0xFFC00000 ) == 0xEA000000 ) { // `B rom_start`
    uint32 dest = get_arm_branch_destination(0, startop);
    if ( dest < (uint32)rom_size ) {
      inf.startIP = 0x8000000 + dest;
    }
  }

  // cartridge header
  doByte(0x8000004, 156);
  doASCI(0x80000A0, 12);
  doASCI(0x80000AC, 4);
  doASCI(0x80000B0, 2);
  doByte(0x80000B2, 1);
  doByte(0x80000B3, 1);
  doByte(0x80000B4, 1);
  doByte(0x80000BC, 1);
  doByte(0x80000BD, 1);
}

//----------------------------------------------------------------------------
loader_t LDSC =
{
  IDP_INTERFACE_VERSION,
  LDRF_RELOAD,
  accept_file,
  load_file,
  NULL,
  NULL
};

