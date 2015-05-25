#ifndef _Nes_h_
   #define _Nes_h_

#include <stdio.h>
#include "Cpu6502.h"

#define bit_value( _byte, bit_order ) ( ( _byte & ( 1 << bit_order ) ) >> bit_order )

#define PRG_ROM_bank_size 0x4000 // PRG-ROM bank is 16kB
#define CHR_ROM_bank_size 0x2000 // CHR-ROM bank is  8kB
#define RAM_size 0x800 // Built-in actual unmirrored RAM is 2kB

enum {
   mirroring_vertical   = 0,
   mirroring_horizontal = 1,
   mirroring_4screens   = 2
};

typedef struct // Nes
{
   Cpu6502 *cpu;
   
   byte *chr_rom; // Chunk with all CHR-ROM banks
   int chr_rom_count; // How many 8kB CHR-ROM banks are present
   
   byte *prg_rom; // Chunk with all PRG-ROM banks
   int prg_rom_count; // How many 16kB PRG-ROM banks are present
   
   byte ram[RAM_size]; // Built-in 2kB of RAM
   
   struct
   {
      int cycles; // PPU cycles countdown to starting Vblank

      // $2000
      byte nmi_enabled;
      byte sprite_height;  // 8|16
      word back_pattern;   // 0|$1000
      word sprite_pattern; // 0|$1000
      byte increment_vram; // 1|32
      word scroll_high_bits; // 0..3 WIP writes to $2006 supposedly modify this flag
      
      // $2001
      byte color_emphasis; // 0..7
      byte sprites_visible;
      byte background_visible;
      byte sprite_clip;
      byte background_clip;
      byte monochrome;
            
      // $2002
      byte vblank_flag;
      byte sprite0_hit;
      byte sprites_lost;
      
      byte write_count; // writes counter for $2005 & $2006. 0 = no write yet. 1 = one write done, waiting for 2nd.
      byte horz_scroll;
      byte vert_scroll;
      word vram_address; // VRAM address to read from or write to
      
      byte mirroring;
      byte *name_attr;    // Chunk of memory for 2 name tables and their attributes
      // byte *name_ptr[4]; // pointers to the 4 virtual name tables
   } ppu;
   
} Nes;

Nes *Nes_Create();
void Nes_Reset( Nes *this );
void Nes_Free( Nes *this );
int  Nes_LoadRom( Nes *this, FILE *rom_file );
void Nes_DoFrame( Nes *this );

#endif // #ifndef _Nes_h_