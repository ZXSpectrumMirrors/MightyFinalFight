TRKCACHE edited_track;

unsigned sector_offset, sector;

void findsector(unsigned addr)
{
   for (sector_offset = sector = 0; sector < edited_track.s; sector++, sector_offset += edited_track.hdr[sector].datlen)
      if (addr >= sector_offset && addr < sector_offset + edited_track.hdr[sector].datlen)
         return;
   errexit("internal diskeditor error");
}

unsigned char *editam(unsigned addr)
{
   if (editor == ED_MEM) return am_r(addr);
   if (!edited_track.trkd) return 0;
   if (editor == ED_PHYS) return edited_track.trkd + addr;
   // editor == ED_LOG
   findsector(addr); return edited_track.hdr[sector].data + addr - sector_offset;
}

__inline unsigned char editrm(unsigned addr)
{
   unsigned char *ptr = editam(addr);
   return ptr? *ptr : 0;
}

void editwm(unsigned addr, unsigned char byte)
{
   if (editrm(addr) == byte) return;
   unsigned char *ptr = editam(addr);
   if (!ptr) return; *ptr = byte;
   if (editor == ED_MEM) return;
   if (editor == ED_PHYS) { comp.wd.fdd[mem_disk].optype |= 2; return; }
   comp.wd.fdd[mem_disk].optype |= 1;
   // recalc sector checksum
   findsector(addr);
   *(unsigned short*)(edited_track.hdr[sector].data + edited_track.hdr[sector].datlen) =
      wd93_crc(edited_track.hdr[sector].data - 1, edited_track.hdr[sector].datlen + 1);
}

unsigned memadr(unsigned addr)
{
   if (editor == ED_MEM) return (addr & 0xFFFF);
   // else if (editor == ED_PHYS || editor == ED_LOG)
   if (!mem_max) return 0;
   while ((int)addr < 0) addr += mem_max;
   while (addr >= mem_max) addr -= mem_max;
   return addr;
}

void showmem()
{
   char line[80]; unsigned ii;
   unsigned cursor_found = 0;
   if (mem_dump) mem_ascii = 1, mem_sz = 32; else mem_sz = 8;

   if (editor != ED_MEM) {
      edited_track.clear();
      edited_track.seek(comp.wd.fdd+mem_disk, mem_track/2, mem_track & 1, (editor == ED_LOG)? LOAD_SECTORS : JUST_SEEK);
      if (!edited_track.trkd) { // no track
         for (ii = 0; ii < mem_size; ii++) {
            sprintf(line, (ii == mem_size/2)?
               "          track not found            ":
               "                                     ");
            tprint(mem_x, mem_y+ii, line, (activedbg == WNDMEM) ? W_SEL : W_NORM);
         }
         mem_max = 0;
         goto title;
      }
      mem_max = edited_track.trklen;
      if (editor == ED_LOG)
         for (mem_max = ii = 0; ii < edited_track.s; ii++)
            mem_max += edited_track.hdr[ii].datlen;
   } else mem_max = 0x10000;
redraw:
   mem_curs = memadr(mem_curs); mem_top = memadr(mem_top);
   for (ii = 0; ii < mem_size; ii++) {
      unsigned ptr = memadr(mem_top + ii*mem_sz);
      sprintf(line, "%04X ", ptr);
      unsigned cx = 0;
      if (mem_dump) {      // 0000 0123456789ABCDEF0123456789ABCDEF
         for (unsigned dx = 0; dx < 32; dx++) {
            if (ptr == mem_curs) cx = dx+5;
            unsigned char c = editrm(ptr); ptr = memadr(ptr+1);
            line[dx+5] = c ? c : '.';
         }
      } else {             // 0000 11 22 33 44 55 66 77 88 abcdefgh
         for (unsigned dx = 0; dx < 8; dx++) {
            if (ptr == mem_curs) cx = (mem_ascii) ? dx+29 : dx*3 + 5 + mem_second;
            unsigned char c = editrm(ptr); ptr = memadr(ptr+1);
            sprintf(line+5+3*dx, "%02X", c); line[7+3*dx] = ' ';
            line[29+dx] = c ? c : '.';
         }
      }
      line[37] = 0;
      tprint(mem_x, mem_y+ii, line, (activedbg == WNDMEM) ? W_SEL : W_NORM);
      cursor_found |= cx;
      if (cx && (activedbg == WNDMEM))
         txtscr[(mem_y+ii)*80+mem_x+cx+80*30] = W_CURS;
   }
   if (!cursor_found) { cursor_found=1; mem_top=mem_curs & ~(mem_sz-1); goto redraw; }
title:
   if (editor == ED_MEM) sprintf(line, "memory: %04X", mem_curs & 0xFFFF);
   else if (editor == ED_PHYS) sprintf(line, "disk %c, trk %02X, offs %04X", mem_disk+'A', mem_track, mem_curs);
   else { // ED_LOG
      if (mem_max) findsector(mem_curs);
      sprintf(line, "disk %c, trk %02X, sec %02X[%02X], offs %04X",
        mem_disk+'A', mem_track,
        sector, edited_track.hdr[sector].n,
        mem_curs-sector_offset);
   }
   tprint(mem_x, mem_y-1, line, W_TITLE);
   frame(mem_x,mem_y,37,mem_size,FRAME);
}

      /* ------------------------------------------------------------- */
void mstl() { if (mem_max) mem_curs &= ~(mem_sz-1), mem_second = 0; }
void mendl() { if (mem_max) mem_curs |= (mem_sz-1), mem_second = 1; }
void mup() { if (mem_max) mem_curs -= mem_sz; }
void mpgdn() { if (mem_max) mem_curs += mem_size*mem_sz, mem_top += mem_size*mem_sz; }
void mpgup() { if (mem_max) mem_curs -= mem_size*mem_sz, mem_top -= mem_size*mem_sz; }

void mdown()
{
   if (!mem_max) return;
   mem_curs += mem_sz;
   if (((mem_curs - mem_top + mem_max) % mem_max) / mem_sz >= mem_size) mem_top += mem_sz;
}

void mleft()
{
   if (!mem_max) return;
   if (mem_ascii || !mem_second) mem_curs--;
   if (!mem_ascii) mem_second ^= 1;
}

void mright()
{
   if (!mem_max) return;
   if (mem_ascii || mem_second) mem_curs++;
   if (!mem_ascii) mem_second ^= 1;
   if (((mem_curs - mem_top + mem_max) % mem_max) / mem_sz >= mem_size) mem_top += mem_sz;
}

char dispatch_mem()
{
   if (!mem_max) return 0;
   if (mem_ascii) {
      unsigned short k;
      if (ToAscii(input.lastkey,0,kbdpc,&k,0) != 1) return 0;
      k &= 0xFF; if (k < 0x20 || k >= 0x80) return 0;
      editwm(mem_curs, (unsigned char)k);
      mright();
      return 1;
   } else {
      if ((input.lastkey >= '0' && input.lastkey <= '9') || (input.lastkey >= 'A' && input.lastkey <= 'F')) {
         unsigned char k = (input.lastkey >= 'A') ? input.lastkey-'A'+10 : input.lastkey-'0';
         unsigned char c = editrm(mem_curs);
         if (mem_second) editwm(mem_curs, (c & 0xF0) | k);
         else editwm(mem_curs, (c & 0x0F) | (k << 4));
         mright();
         return 1;
      }
   }
   return 0;
}

void mtext()
{
   unsigned rs = find1dlg(mem_curs);
   if (rs != -1) mem_curs = rs;
}

void mcode()
{
   unsigned rs = find2dlg(mem_curs);
   if (rs != -1) mem_curs = rs;
}

void mgoto()
{
   unsigned v = input4(mem_x, mem_y, mem_top);
   if (v != -1) mem_top = (v & ~(mem_sz-1)), mem_curs = v;
}

void mswitch() { mem_ascii ^= 1; }
void msp() { mem_curs = cpu.sp; }
void mpc() { mem_curs = cpu.pc; }
void mbc() { mem_curs = cpu.bc; }
void mde() { mem_curs = cpu.de; }
void mhl() { mem_curs = cpu.hl; }
void mix() { mem_curs = cpu.ix; }
void miy() { mem_curs = cpu.iy; }

void mmodemem() { editor = ED_MEM; }
void mmodephys() { editor = ED_PHYS; }
void mmodelog() { editor = ED_LOG; }

void mdiskgo()
{
   if (editor == ED_MEM) return;
   for (;;) {
      *(unsigned*)str = mem_disk + 'A';
      if (!inputhex(mem_x+5, mem_y-1, 1, 1)) return;
      if (*str >= 'A' && *str <= 'D') break;
   }
   mem_disk = *str-'A'; showmem();
   unsigned x = input2(mem_x+12, mem_y-1, mem_track);
   if (x == -1) return;
   mem_track = x;
   if (editor == ED_PHYS) return;
   showmem();
   // enter sector
   for (;;) {
      findsector(mem_curs); x = input2(mem_x+20, mem_y-1, sector);
      if (x == -1) return; if (x < edited_track.s) break;
   }
   for (mem_curs = 0; x; x--) mem_curs += edited_track.hdr[x-1].datlen;
}
