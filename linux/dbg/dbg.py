#!/usr/bin/python
# -*- coding: utf-8 -*-


import sys
import argparse
try:
    import readline
except:
    pass
import shlex


def print8(*args):
    print " ".join(unicode(x).encode(u"utf-8") for x in args)

def printerr8(*args):
    print >>sys.stderr, " ".join(unicode(x).encode(u"utf-8") for x in args)


MODES = [
    u"MONO",
    u"ATTR",
    u"2BPP",
    u"4BPP",
]

REG_NAMES = [
    u"MODE_CTRL",
    u"SCROLL",
    u"SCREEN_BASE_L",
    u"SCREEN_BASE_H",
    u"MODULO_L",
    u"MODULO_H",
    u"TILE_CTRL",
    u"TILE_BASE_H",
    u"REG_08",
    u"REG_09",
    u"REG_0a",
    u"REG_0b",
    u"REG_0c",
    u"REG_0d",
    u"REG_0e",
    u"REG_0f",
    
    u"SPRITE0_ADDR_L",
    u"SPRITE0_ADDR_H",
    u"SPRITE1_ADDR_L",
    u"SPRITE1_ADDR_H",
    u"SPRITE2_ADDR_L",
    u"SPRITE2_ADDR_H",
    u"SPRITE3_ADDR_L",
    u"SPRITE3_ADDR_H",
    u"SPRITE4_ADDR_L",
    u"SPRITE4_ADDR_H",
    u"SPRITE5_ADDR_L",
    u"SPRITE5_ADDR_H",
    u"SPRITE6_ADDR_L",
    u"SPRITE6_ADDR_H",
    u"SPRITE7_ADDR_L",
    u"SPRITE7_ADDR_H",
    
    u"SPRITE0_CTRL",
    u"SPRITE0_COLOR",
    u"SPRITE0_XPOS",
    u"SPRITE0_YPOS",
    
    u"SPRITE1_CTRL",
    u"SPRITE1_COLOR",
    u"SPRITE1_XPOS",
    u"SPRITE1_YPOS",
    
    u"SPRITE2_CTRL",
    u"SPRITE2_COLOR",
    u"SPRITE2_XPOS",
    u"SPRITE2_YPOS",
    
    u"SPRITE3_CTRL",
    u"SPRITE3_COLOR",
    u"SPRITE3_XPOS",
    u"SPRITE3_YPOS",
    
    u"SPRITE4_CTRL",
    u"SPRITE4_COLOR",
    u"SPRITE4_XPOS",
    u"SPRITE4_YPOS",
    
    u"SPRITE5_CTRL",
    u"SPRITE5_COLOR",
    u"SPRITE5_XPOS",
    u"SPRITE5_YPOS",
    
    u"SPRITE6_CTRL",
    u"SPRITE6_COLOR",
    u"SPRITE6_XPOS",
    u"SPRITE6_YPOS",
    
    u"SPRITE7_CTRL",
    u"SPRITE7_COLOR",
    u"SPRITE7_XPOS",
    u"SPRITE7_YPOS",

    u"COLOR0_L",
    u"COLOR0_H",
    u"COLOR1_L",
    u"COLOR1_H",
    u"COLOR2_L",
    u"COLOR2_H",
    u"COLOR3_L",
    u"COLOR3_H",
    u"COLOR4_L",
    u"COLOR4_H",
    u"COLOR5_L",
    u"COLOR5_H",
    u"COLOR6_L",
    u"COLOR6_H",
    u"COLOR7_L",
    u"COLOR7_H",
    u"COLOR8_L",
    u"COLOR8_H",
    u"COLOR9_L",
    u"COLOR9_H",
    u"COLOR10_L",
    u"COLOR10_H",
    u"COLOR11_L",
    u"COLOR11_H",
    u"COLOR12_L",
    u"COLOR12_H",
    u"COLOR13_L",
    u"COLOR13_H",
    u"COLOR14_L",
    u"COLOR14_H",
    u"COLOR15_L",
    u"COLOR15_H",

    u"SPRCOLOR0_L",
    u"SPRCOLOR0_H",
    u"SPRCOLOR1_L",
    u"SPRCOLOR1_H",
    u"SPRCOLOR2_L",
    u"SPRCOLOR2_H",
    u"SPRCOLOR3_L",
    u"SPRCOLOR3_H",
    u"SPRCOLOR4_L",
    u"SPRCOLOR4_H",
    u"SPRCOLOR5_L",
    u"SPRCOLOR5_H",
    u"SPRCOLOR6_L",
    u"SPRCOLOR6_H",
    u"SPRCOLOR7_L",
    u"SPRCOLOR7_H",
    u"SPRCOLOR8_L",
    u"SPRCOLOR8_H",
    u"SPRCOLOR9_L",
    u"SPRCOLOR9_H",
    u"SPRCOLOR10_L",
    u"SPRCOLOR10_H",
    u"SPRCOLOR11_L",
    u"SPRCOLOR11_H",
    u"SPRCOLOR12_L",
    u"SPRCOLOR12_H",
    u"SPRCOLOR13_L",
    u"SPRCOLOR13_H",
    u"SPRCOLOR14_L",
    u"SPRCOLOR14_H",
    u"SPRCOLOR15_L",
    u"SPRCOLOR15_H",
]


class HostMem(object):
    
    def __init__(self, devpath):
        super(HostMem, self).__init__()
        self.devfile = open(devpath, u"rb")
        self.update()
    
    def update(self):
        data = self.devfile.read(0x10000)
        self.mem = data[:0x10000]
        self.io = data[0x10000:0x11000]
        #self.mem = "\x02c\x03b" * 0x4000
        #self.io = "\x00" * 0x1000
    
    def peek(self, addr):
        return ord(self.mem[addr])
    
    def peekw(self, addr):
        return ord(self.mem[addr]) + ord(self.mem[(addr + 1) & 0xffff]) * 256


def ascii(b):
    if b >= 0x20 and b < 0x7f:
        return unichr(b)
    else:
        return u"."


class Monitor(object):
    
    def __init__(self, devpath):
        super(Monitor, self).__init__()
        self.mem = HostMem(devpath)
        self.quit = False
    
    def parse_cmd(self, argv):
        if not argv:
            return
        if hasattr(self, u"cmd_" + argv[0]):
            getattr(self, u"cmd_" + argv[0])(argv[1:])
        else:
            print8(u"Unknown command: '%s'" % argv[0])
    
    def prompt(self):
        while not self.quit:
            try:
                argv8 = shlex.split(raw_input(u"(0000) ").decode(u"utf-8"))
            except ValueError as e:
                print8(u"Error: %s" % unicode(e))
                continue
            except EOFError:
                print8(u"quit")
                return
            self.parse_cmd(argv8)
    
    def cmd_dl(self, argv):
        self.xhi = {}
        self.yhi = {}
        if (self.mem.peek(0x03fc) != 0xf1) or (self.mem.peek(0x03fd) != 0xf0):
            print8(u"Error: No F1F0 marker at $03fc")
            return
        dladdr = self.mem.peekw(0x03fe)
        print8(u"Display list at $%04x:" % dladdr)
        addr = dladdr
        dl = []
        while len(dl) < 256:
            cmd = self.mem.peek(addr) * 256 + self.mem.peek((addr + 1) & 0xffff)
            dl.append(cmd)
            addr = (addr + 2) & 0xffff
            if cmd == 0x8000:
                break
        i = 0
        while i < len(dl):
            a = dladdr + i * 2
            if dl[i] == 0x8000:
                print8(u"\n%04x: %04x\tEND\n" % (a, dl[i]))
            elif dl[i] & 0x8000:
                hpos = (dl[i] >> 9) & 0x3f;
                vpos = dl[i] & 0x1ff
                print8(u"\n%04x: %04x\tWAIT\t$%02x, $%03x" % (a, dl[i], hpos, vpos))
            else:
                reg = dl[i] >> 8
                value = dl[i] & 0xff
                name = REG_NAMES[reg]
                word = False
                try:
                    if name.endswith(u"_H"):
                        next = REG_NAMES[dl[i + 1] >> 8]
                        if next.endswith(u"_L") and (next[:-2] == name[:-2]):
                            word = True
                            value = value * 256 + (dl[i + 1] & 0xff)
                except IndexError:
                    pass
                if word:
                    op = u"MOVEW"
                    arg = u"%s, $%04x" % (name[:-2], value)
                    comment = self.decode_dlword(name[:-2], value)
                    self.printdl(a, u"%04x%04x" % (dl[i], dl[i + 1]), op, arg, comment)
                    #print8(u"%04x: %04x\tMOVEW\t%s, $%04x%s" % (a, dl[i], name[:-2], value, comment))
                    i += 1
                else:
                    op = u"MOVE"
                    arg = u"%s, $%02x" % (name, value)
                    comment = self.decode_dlbyte(name, value)
                    self.printdl(a, u"%04x" % dl[i], op, arg, comment)
            i += 1
    
    def printdl(self, addr, cmd, op, arg, comment):
        if len(arg) < 8:
            pad = u"\t\t"
        elif len(arg) < 16:
            pad = u"\t"
        else:
            pad = u""
        if comment:
            pad += u"\t; "
        print8(u"%04x: %s\t%s\t%s%s%s" % (addr, cmd, op, arg, pad, comment))
    
    def decode_dlword(self, name, value):
        if name.startswith(u"COLOR") or name.startswith(u"SPRCOLOR"):
            r5 = value >> 11
            g6 = (value >> 5) & 0x3f
            b5 = value & 0x1f
            r = (r5 << 3) | (r5 >> 2)
            g = (g6 << 2) | (g6 >> 4)
            b = (b5 << 3) | (b5 >> 2)
            return u"#%02x%02x%02x" % (r, g, b)
        
        return u""
    
    def decode_dlbyte(self, name, value):
        if name == u"MODE_CTRL":
            w = 640 if value & 0x40 else 320
            h = 480 if value & 0x10 else 240
            m = MODES[(value >> 2) & 3]
            b = u"BITMAP" if value & 1 else u"TILES"
            return u"%dx%d, %s, %s" % (w, h, m, b)
        
        if name == u"TILE_CTRL":
            w = 16 if value & 4 else 8
            h = 16 if value & 1 else 8
            return u"%dx%d" % (w, h)
        
        if name.startswith(u"SPRITE"):
            try:
                spr = name[6:8]
            except ValueError:
                spr = name[6:7]
            if name.endswith(u"_CTRL"):
                e = u"ON" if value & 0x80 else u"OFF"
                self.xhi[spr] = value & 3
                self.yhi[spr] = (value >> 2) & 3
                return u"%s, $%xxx, $%xyy" % (e, self.xhi[spr], self.yhi[spr])
            if name.endswith(u"_COLOR"):
                m = MODES[value >> 6]
                y = u", YEXPAND" if value & 0x20 else u""
                x = u", XEXPAND" if value & 0x10 else u""
                c = value & 0x0f
                return u"%s%s%s, COLOR%d" % (m, y, x, c)
            if name.endswith(u"_XPOS"):
                try:
                    return u"$%x%02x" % (self.xhi[spr], value)
                except KeyError:
                    return u""
            if name.endswith(u"_YPOS"):
                try:
                    return u"$%x%02x" % (self.yhi[spr], value)
                except KeyError:
                    return u""
        
        return u""
    
    def cmd_m(self, argv):
        addr = int(argv[0], 16)
        for lines in xrange(16):
            a = u"%04x" % addr
            h = []
            c = []
            for col in xrange(16):
                b = self.mem.peek(addr)
                addr += 1
                h.append(u"%02x" % b)
                c.append(ascii(b))
            print8(u"%s\t%s\t; %s" % (a, u" ".join(h), u"".join(c)))
    
    def cmd_ms2(self, argv):
        addr = int(argv[0], 16)
        for lines in xrange(32):
            a = u"%04x" % addr
            h = []
            for col in xrange(8):
                b = self.mem.peek(addr)
                addr += 1
                h.append(u"%x" % (b >> 6))
                h.append(u"%x" % ((b >> 4) & 3))
                h.append(u"%x" % ((b >> 2) & 3))
                h.append(u"%x" % (b & 3))
            print8(u"%s\t%s" % (a, u"".join(h)))


def main(argv):
    #p = argparse.ArgumentParser()
    #p.add_argument(u"-v", u"--verbose", action=u"store_true",
    #               help=u"Verbose output.")
    #args = p.parse_args()
    argv8 = list(x.decode(u"utf-8") for x in argv[1:])
    
    m = Monitor(u"/dev/fifogfx0")
    
    if argv8:
        m.parse_cmd(argv8)
    else:
        m.prompt()
    
    return 0
    

if __name__ == '__main__':
    sys.exit(main(sys.argv))
    
