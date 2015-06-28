#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "Nes.h"

// How many PPU cycles until starting VBlank. 262 scanlines * 341 ppu cycles (one per pixel)
#define VBlank_ppu_cycles 262 * 341
#define VBlank_end_ppu_cycles 20 * 341

#ifdef _Cpu6502_Disassembler
   static byte read_memory_disasm( void *parent_system, word address );
#endif
static void init_builtin_memory_handlers( Nes *this );

// -------------------------------------------------------------------------------
static void initialize( Nes *this )
{
   Cpu6502_Initialize( this->cpu );
   
   this->ppu.nmi_enabled      = 0;
   this->ppu.sprite_height    = 8;
   this->ppu.back_pattern     = 0;
   this->ppu.sprite_pattern   = 0;
   this->ppu.increment_vram   = 1;
   this->ppu.scroll_high_bits = 0;

   this->ppu.color_emphasis     = 0;
   this->ppu.sprites_visible    = 0;
   this->ppu.background_visible = 0;
   this->ppu.sprite_clip        = 0;
   this->ppu.background_clip    = 0;
   this->ppu.monochrome         = 0;

   this->ppu.vblank_flag  = 0;
   this->ppu.sprite0_hit  = 0;
   this->ppu.sprites_lost = 0;
   
   this->ppu.write_count  = 0;
   this->ppu.horz_scroll  = 0;
   this->ppu.vert_scroll  = 0;
   this->ppu.vram_address = 0;
   this->ppu.mirroring    = 0;
   
   this->cpu_cycles      = 0;
   this->ppu_cycles      = 0;
   this->frames          = 0;
   this->scanline        = -1;
   this->scanpixel       = 0;
   this->vblank          = 0;
   this->last_scanpixel  = 0;
   
   memset( this->input.gamepad,    0, sizeof this->input.gamepad );
   memset( this->input.read_count, 0, sizeof this->input.read_count );
   this->input.strobe_state = Nes_Strobe_init;
}
// -------------------------------------------------------------------------------
Nes *Nes_Create()
{  
   Nes *this = (Nes*) malloc( sizeof( Nes ) );
   if( this == NULL ){
      return NULL;
   }
      
   this->cpu = Cpu6502_Create( this );
   if( this->cpu == NULL ) {
      return NULL;
   }
   Cpu6502_Initialize( this->cpu );
   
   #ifdef _Cpu6502_Disassembler
      this->cpu->read_memory_disasm = read_memory_disasm;
   #endif
   
   this->prg_rom_count = 0;
   this->prg_rom = NULL;
   this->chr_rom_count = 0;
   this->chr_rom = NULL;

   memset( this->ram, 0, 0x800 );
   memset( this->save_ram, 0, 0x2000 );
   
   this->ppu.name_attr = (byte *) malloc( 0x800 );
   memset( this->ppu.name_attr, 0xFF, 0x800 );
   memset( this->ppu.name_ptr, 0, 4 );
   memset( this->ppu.attr_ptr, 0, 4 );
   memset( this->ppu.palettes, 0, 0x20 );
   memset( this->ppu.sprites, 0, 0x100 );
   this->chr_unpacked = NULL;

   initialize( this );
   init_builtin_memory_handlers( this );
   
   return this;
}

// -------------------------------------------------------------------------------
void Nes_Free( Nes *this )
{
   if( this->prg_rom != NULL ) {
      free( this->prg_rom );
   }
   if( this->chr_rom != NULL ) {
      free( this->chr_rom );
   }
   if( this->chr_unpacked != NULL ) {
      free( this->chr_unpacked );
   }
      free( this->ppu.name_attr );
   }
   free( this );
}

// -------------------------------------------------------------------------------
void Nes_UnpackChrRom( Nes *this )
{
   if( this->chr_unpacked != NULL ) {
      free( this->chr_unpacked );
   }
   
   // 1 CHR-ROM bank = 2 CHR-ROM tables
   assert( this->chr_rom_count == 1 ); // for now
   this->chr_unpacked = (byte *) malloc( 2 * CHR_UNPACKED_size );
   
   byte *chr_rom_ptr[] = {
      &this->chr_rom[0],
      &this->chr_rom[0x1000]
   };
   
   // Each pointer points to each of the 2 CHR-ROM tables, 0: sprites, 1: background
   this->chr_unpacked_ptr[0] = &this->chr_unpacked[0];
   this->chr_unpacked_ptr[1] = &this->chr_unpacked[CHR_UNPACKED_size];
   
   for( int chrom = 0; chrom <= 1; ++chrom )
   {
      byte *lsb = chr_rom_ptr[chrom];
      byte *msb = lsb + 8;
      byte *unpacked = this->chr_unpacked_ptr[chrom];
      for( int tilen = 0; tilen < 0x100; ++tilen )
      {
         for( int line = 0; line <= 7; ++line )
         {
            for( int bit = 7; bit >= 0; --bit )
            {
               byte color_index = ( *lsb & (1<<bit) ) >>bit;
               color_index |= ( ( *msb & (1<<bit) ) >>bit ) <<1 ;
               
               *unpacked = color_index;
               assert( unpacked < &this->chr_unpacked[ CHR_UNPACKED_size*2 ] );
               unpacked++;
            }
            ++lsb;
            ++msb;
         }
         lsb += 8;
         msb += 8;
      }
   }
}

// -------------------------------------------------------------------------------
void Nes_Reset( Nes *this )
{
   Cpu6502_Reset( this->cpu );
   initialize( this );
}

// -------------------------------------------------------------------------------
void check_sprite0hit( Nes *this );

void Nes_DoFrame( Nes *this )
{
   int cpu_cycles = 0;
   int vblank_started = 0;
   while( 1 )
   {
      if(( this->scanline == 241 ) && ( this->vblank == 0 ))
      {
         this->frames++;
         this->ppu.vblank_flag = 1;
         this->vblank = 1;
         vblank_started = 1;
         if( this->ppu.nmi_enabled )
         {
            cpu_cycles = Cpu6502_NMI( this->cpu );
         }
      }
      else
      {
         cpu_cycles = Cpu6502_CpuStep( this->cpu );
      }
      
      this->cpu_cycles += cpu_cycles;
      this->ppu_cycles += 3 * cpu_cycles;

      this->last_scanpixel = this->scanpixel;
      this->scanpixel += 3 * cpu_cycles;
      
      if( this->scanpixel >= 341 )
      {
         this->scanpixel -= 341;
         this->scanline++;
         
         if( this->scanline >= 261 )
         {
            this->scanline = -1;
            this->ppu.sprite0_hit = 0; // WIP this actually happens on scanpixel 1, but does it matter?
            this->ppu.vblank_flag = 0; // WIP this may actually happen on next scanline (0)
            this->vblank = 0;
         }
      }
      
      if( vblank_started ) { // Reaching scanline 241
         break;
      }
      
      // if last rendered pixels lie inside the visible screen and collide with the sprite 0 area
      if(( this->scanline > -1 ) && ( this->scanline < 240 ) && ( this->last_scanpixel < 256 ) // WIP this will change when scroll is implemented
         && ( this->ppu.sprite0_hit == 0 )
         && ( this->ppu.sprites[0] >= this->scanline - 8 ) && ( this->ppu.sprites[0] <= this->scanline )
         && ( this->ppu.sprites[3] >= this->scanpixel - 8 ) && ( this->ppu.sprites[3] <= this->scanpixel ))
      {
         check_sprite0hit( this );
      }
   }
}

// http://wiki.nesdev.com/w/index.php/PPU_OAM
void check_sprite0hit( Nes *this )
{
   this->ppu.sprite0_hit = 1;
   printf( "S0h %03d,%03d frame %03d\n", this->scanpixel, this->scanline, this->frames );
}

// -------------------------------------------------------------------------------
int Nes_LoadRom( Nes *this, FILE *rom_file )
{
   if( this->prg_rom != NULL ) {
      free( this->prg_rom );
      this->prg_rom = NULL;
   }
   if( this->chr_rom != NULL ) {
      free( this->chr_rom );
      this->chr_rom = NULL;
   }
   
   rewind( rom_file );
   byte header[10];
   fread( header, 10, 1, rom_file );
         
   int trainer = ( header[6] & (1<<2) ) > 0; 
   int offset = 16 + ( trainer ? 512 : 0 ); // skip 16 bytes header + 512B trainer
   
   // Read PRG-ROM banks
   this->prg_rom_count = (int) header[4];
   this->prg_rom = (byte*) malloc( this->prg_rom_count * PRG_ROM_bank_size );
   if( this->prg_rom == NULL ) {
      goto Exception;
   }
   fseek( rom_file, offset, SEEK_SET );
   size_t read_count = fread( this->prg_rom, PRG_ROM_bank_size, this->prg_rom_count, rom_file );
   if( read_count != this->prg_rom_count ) {
      goto Exception;
   }
   
   // The CHR-ROM banks immediately follow the PRG-ROM banks, no fseek() needed
   this->chr_rom_count = (int) header[5];
   if( this->chr_rom_count == 0 ) {
      this->chr_rom_count = 1; // WIP: CHR-ROM count of 0 means 1 as most docs say or does it mean it has only CHR-RAM?
   }
   this->chr_rom = (byte*) malloc( this->chr_rom_count * CHR_ROM_bank_size );
   if( this->chr_rom == NULL ) {
      goto Exception;
   }
   read_count = fread( this->chr_rom, CHR_ROM_bank_size, this->chr_rom_count, rom_file );
   if( read_count != this->chr_rom_count ) {
      goto Exception;
   }
   
   Nes_UnpackChrRom( this );

   if( header[5] & (1<<3) ) {
      this->ppu.mirroring = mirroring_4screens;
      assert( 0 && "4 screen [non]mirroring not implemented yet" );
   }
   else if( header[5] & 1 ) {
      this->ppu.mirroring = mirroring_vertical;
      this->ppu.name_ptr[0] = &this->ppu.name_attr[0];
      this->ppu.name_ptr[1] = &this->ppu.name_attr[0x400];
      this->ppu.name_ptr[2] = &this->ppu.name_attr[0];
      this->ppu.name_ptr[3] = &this->ppu.name_attr[0x400];
      
      this->ppu.attr_ptr[0] = &this->ppu.name_attr[0x3C0];
      this->ppu.attr_ptr[1] = &this->ppu.name_attr[0x7C0];
      this->ppu.attr_ptr[2] = &this->ppu.name_attr[0x3C0];
      this->ppu.attr_ptr[3] = &this->ppu.name_attr[0x7C0];
   }
   else {
      this->ppu.mirroring = mirroring_horizontal;
      this->ppu.name_ptr[0] = &this->ppu.name_attr[0];
      this->ppu.name_ptr[1] = &this->ppu.name_attr[0];
      this->ppu.name_ptr[2] = &this->ppu.name_attr[0x400];
      this->ppu.name_ptr[3] = &this->ppu.name_attr[0x400];
      
      this->ppu.attr_ptr[0] = &this->ppu.name_attr[0x3C0];
      this->ppu.attr_ptr[1] = &this->ppu.name_attr[0x3C0];
      this->ppu.attr_ptr[2] = &this->ppu.name_attr[0x7C0];
      this->ppu.attr_ptr[3] = &this->ppu.name_attr[0x7C0];
   }
   
   // Extra check to trap any unseen error in reading the rom file
   byte dummy;
   size_t remaining = fread( &dummy, 1, 1, rom_file );   
   if( ! feof( rom_file ) || ( remaining != 0 ) ) {
      fprintf( stderr, "The rom file didn't end after CHR-ROM banks as expected.\n" );
   }
      
   return true;
   
Exception:
   
   if( this->prg_rom != NULL ) {
      free( this->prg_rom );
      this->prg_rom = NULL;
   }
   if( this->chr_rom != NULL ) {
      free( this->chr_rom );
      this->chr_rom = NULL;
   }  
   return false;
}

// -------------------------------------------------------------------------------
#ifdef _Cpu6502_Disassembler
   static byte read_memory_disasm( void *sys, word address )
   {
      // For side effect reads, this should be avoided and direct access should be done instead.
      // return ((Nes*)sys)->cpu->read_memory[address]( sys, address );
      if( address < 0x2000 || address >= 0x8000 ) {
         return ((Nes*)sys)->cpu->read_memory[address]( sys, address );
      }
      else {
         return 0; // disasm.value should be overwritten by the register functions themselves
      }
   }
#endif

// -------------------------------------------------------------------------------
#include "MemoryAccess.h"

byte read_ignore( void *sys, word address ) {
   return 0;
}
void write_ignore( void *sys, word address, byte value ) {
}

static void init_builtin_memory_handlers( Nes *this )
{
   int i;
// RAM
   for( i=0; i<=0x7FF; ++i ) {
      this->cpu->read_memory[i]  = read_ram;
      this->cpu->write_memory[i] = write_ram;      
   }
   for( i=0x800; i<=0x1FFF; ++i ) {
      this->cpu->read_memory[i]  = read_ram_mirror;
      this->cpu->write_memory[i] = write_ram_mirror;
   }
   // Default all registers as unimplemented and then overwrite each one as they are implemented
   for( i=0x2000; i<=0x7FFF; ++i ) {
      this->cpu->read_memory[i]  = read_unimplemented;
      this->cpu->write_memory[i] = write_unimplemented;
   }
// PPU
   for( i=0x2000; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_ignore; // WIP write only port, should return open bus
      this->cpu->write_memory[i] = write_ppu_control1;
   }
   for( i=0x2001; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_ignore; // WIP write only port, should return open bus
      this->cpu->write_memory[i] = write_ppu_control2;
   }
   for( i=0x2002; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_ppu_status;
      this->cpu->write_memory[i] = write_unimplemented;
   }
   for( i=0x2003; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_ignore; // WIP write only port, should return open bus
      this->cpu->write_memory[i] = write_spr_ram_address;
   }
   for( i=0x2004; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_spr_ram_io;
      this->cpu->write_memory[i] = write_spr_ram_io;
   }
   for( i=0x2005; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_ignore; // WIP write only port, should return open bus
      this->cpu->write_memory[i] = write_scroll;
   }
   for( i=0x2006; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_ignore; // WIP write only port, should return open bus
      this->cpu->write_memory[i] = write_vram_address;
   }
   for( i=0x2007; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i]  = read_vram_io;
      this->cpu->write_memory[i] = write_vram_io;
   }
// APU
   for( i = 0x4000; i <= 0x4020; ++i ) {
      this->cpu->read_memory[i]  = read_ignore;
      this->cpu->write_memory[i] = write_ignore;
   }
// Sprite DMA
   this->cpu->read_memory[0x4014]  = read_unimplemented;
   this->cpu->write_memory[0x4014] = write_sprite_dma;
// Input
   for( i=0x4016; i<=0x4017; ++i ) {
      this->cpu->read_memory[i]  = read_gamepad;
      this->cpu->write_memory[i] = write_gamepad;
   }
// Save RAM
   for( i=0x6000; i<=0x7FFF; ++i ) {
      this->cpu->read_memory[i]  = read_save_ram;
      this->cpu->write_memory[i] = write_save_ram;
   }
// PRG ROM
   for( i=0x8000; i<=0xFFFF; ++i ) {
      this->cpu->read_memory[i]  = read_prg_rom;
      this->cpu->write_memory[i] = write_ignore;
   }
}

// -------------------------------------------------------------------------------
// Returns palette 0, color 0 for any color index 0, even for sprite palettes
// area 0 for background palettes, area 1 for sprite palettes
const byte *Nes_GetPaletteColor( Nes *this, byte area, byte palette, byte index )
{
   byte rgb_index;
   if( index == 0 ) {
      rgb_index = this->ppu.palettes[0];
   }
   else {
      rgb_index = this->ppu.palettes[ area * 0x10 + palette * 4 + index ];
   }
   return (const byte*) &Nes_rgb[rgb_index];
}

// -------------------------------------------------------------------------------
void Nes_SetInputState( Nes *this, byte gamepad, byte button, byte state )
{
   this->input.gamepad[gamepad][button] = state;
}

// -------------------------------------------------------------------------------
// From: "Matthew Conte" <itsbroke@classicgaming.com>
// To: "nesdev" <nesdev@onelist.com>
// Date: Wed, 17 Mar 1999 17:22:56 -0500
// Subject: [nesdev] NES Palette (modified)
const byte Nes_rgb[64][3] =
{ //      0                 1                 2                 3
   {0x80,0x80,0x80}, {0x00,0x00,0xBB}, {0x37,0x00,0xBF}, {0x84,0x00,0xA6}, // 00
   {0xBB,0x00,0x6A}, {0xB7,0x00,0x1E}, {0xB3,0x00,0x00}, {0x91,0x26,0x00}, // 04
   {0x7B,0x2B,0x00}, {0x00,0x3E,0x00}, {0x00,0x48,0x0D}, {0x00,0x3C,0x22}, // 08
   {0x00,0x2F,0x66}, {0x00,0x00,0x00}, {0x05,0x05,0x05}, {0x05,0x05,0x05}, // 0C

   {0xC8,0xC8,0xC8}, {0x00,0x59,0xFF}, {0x44,0x3C,0xFF}, {0xB7,0x33,0xCC}, // 10
   {0xFF,0x33,0xAA}, {0xFF,0x37,0x5E}, {0xFF,0x37,0x1A}, {0xD5,0x4B,0x00}, // 14
   {0xC4,0x62,0x00}, {0x3C,0x7B,0x00}, {0x1E,0x84,0x15}, {0x00,0x95,0x66}, // 18
   {0x00,0x84,0xC4}, {0x11,0x11,0x11}, {0x09,0x09,0x09}, {0x09,0x09,0x09}, // 1C

   {0xFF,0xFF,0xFF}, {0x00,0x95,0xFF}, {0x6F,0x84,0xFF}, {0xD5,0x6F,0xFF}, // 20
   {0xFF,0x77,0xCC}, {0xFF,0x6F,0x99}, {0xFF,0x7B,0x59}, {0xFF,0xB6,0x00}, // 24
   {0xFF,0xA2,0x33}, {0xA6,0xBF,0x00}, {0x51,0xD9,0x6A}, {0x4D,0xD5,0xAE}, // 28
   {0x00,0xD9,0xFF}, {0x66,0x66,0x66}, {0x0D,0x0D,0x0D}, {0x0D,0x0D,0x0D}, // 2C

   {0xFF,0xFF,0xFF}, {0x84,0xBF,0xFF}, {0xBB,0xBB,0xFF}, {0xD0,0xBB,0xFF}, // 30
   {0xFF,0xBF,0xEA}, {0xFF,0xBF,0xCC}, {0xFF,0xC4,0xB7}, {0xFF,0xCC,0xAE}, // 34
   {0xFF,0xD9,0xA2}, {0xCC,0xE1,0x99}, {0xAE,0xEE,0xB7}, {0xAA,0xF7,0xEE}, // 38
   {0xB3,0xEE,0xFF}, {0xDD,0xDD,0xDD}, {0x11,0x11,0x11}, {0x11,0x11,0x11}  // 3C
};

// http://www.thealmightyguru.com/Games/Hacking/Wiki/index.php?title=NES_Palette
// 7C,7C,7C
// 00,00,FC
// 00,00,BC
// 44,28,BC
// 94,00,84
// A8,00,20
// A8,10,00
// 88,14,00
// 50,30,00
// 00,78,00
// 00,68,00
// 00,58,00
// 00,40,58
// 00,00,00
// 00,00,00
// 00,00,00
// BC,BC,BC
// 00,78,F8
// 00,58,F8
// 68,44,FC
// D8,00,CC
// E4,00,58
// F8,38,00
// E4,5C,10
// AC,7C,00
// 00,B8,00
// 00,A8,00
// 00,A8,44
// 00,88,88
// 00,00,00
// 00,00,00
// 00,00,00
// F8,F8,F8
// 3C,BC,FC
// 68,88,FC
// 98,78,F8
// F8,78,F8
// F8,58,98
// F8,78,58
// FC,A0,44
// F8,B8,00
// B8,F8,18
// 58,D8,54
// 58,F8,98
// 00,E8,D8
// 78,78,78
// 00,00,00
// 00,00,00
// FC,FC,FC
// A4,E4,FC
// B8,B8,F8
// D8,B8,F8
// F8,B8,F8
// F8,A4,C0
// F0,D0,B0
// FC,E0,A8
// F8,D8,78
// D8,F8,78
// B8,F8,B8
// B8,F8,D8
// 00,FC,FC
// F8,D8,F8
// 00,00,00
// 00,00,00