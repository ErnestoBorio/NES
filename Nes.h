#ifndef _Nes_h_
   #define _Nes_h_

#include <stdio.h>
#include "Cpu6502.h"

#define bit_value( _byte, bit_order ) ( ( _byte & ( 1 << bit_order ) ) >> bit_order )

#define PRG_ROM_bank_size 0x4000 // PRG-ROM bank is 16kB
#define CHR_ROM_bank_size 0x2000 // CHR-ROM bank is  8kB
#define CHR_UNPACKED_size 0x100 * 8 * 8 // 0x100 tiles * 8 px tall * 8 px wide = 0x4000 bytes at 1 byte per pixel = 16Kb
#define RAM_size 0x800 // Built-in actual unmirrored RAM is 2kB

const byte nes_palette[64][3];

enum {
   mirroring_vertical   = 0,
   mirroring_horizontal = 1,
   mirroring_4screens   = 2
};

enum Nes_Buttons {
   Nes_A      = 0,
   Nes_B      = 1,
   Nes_Select = 2,
   Nes_Start  = 3,
   Nes_Up     = 4,
   Nes_Down   = 5,
   Nes_Left   = 6,
   Nes_Right  = 7
};

enum Nes_Strobe {
   Nes_Strobe_clear = 0,
   Nes_Strobe_reset = 1,
   Nes_Strobe_reading = 2,
   Nes_Strobe_init = 3
};

typedef struct // Nes
{
   Cpu6502 *cpu;
   
   byte *chr_rom; // Chunk with all CHR-ROM banks
   int chr_rom_count; // How many 8kB CHR-ROM banks are present
   byte *chr_unpacked; // 1 byte per pixel translation of CHR-ROM
   byte *chr_unpacked_ptr[2];
   
   byte *prg_rom; // Chunk with all PRG-ROM banks
   int prg_rom_count; // How many 16kB PRG-ROM banks are present
   
   byte ram[RAM_size]; // Built-in 2kB of RAM

   // int scanline;   
   int frames;
   long cpu_cycles;
   long ppu_cycles;
   
   struct
   {
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
      byte *name_attr;   // Chunk of memory for 2 name tables and their attributes
      byte *name_ptr[4]; // pointers to the 4 virtual name tables (2 real)
      byte *attr_ptr[4]; // pointers to the 4 virtual attribute tables (2 real)
      byte palettes[0x20]; // WIP should memory be malloc'ed? the Nes itself is malloc'ed anyway.
      byte sprites[0x100];
   } ppu;
   
   struct {
      byte gamepad[2][8]; // Holds the pressed state [0|1] of the buttons of both gamepads
      byte strobe_state;
      byte read_count[2];
   } input;
   
} Nes;

Nes *Nes_Create();
void Nes_Reset( Nes *this );
void Nes_Free( Nes *this );
int  Nes_LoadRom( Nes *this, FILE *rom_file );
void Nes_DoFrame( Nes *this );
const byte *Nes_GetPaletteColor( Nes *this, byte area, byte palette, byte index );

void Nes_SetInputState( Nes *this, byte gampead, byte button, byte state );

const byte Nes_rgb[64][3];

#endif // #ifndef _Nes_h_