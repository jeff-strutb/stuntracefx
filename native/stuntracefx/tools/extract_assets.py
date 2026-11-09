#!/usr/bin/env python3
"""extract_assets.py ROM OUTDIR: the game's data from the player's own copy of
the cartridge. Checks the ROM, then writes OUTDIR/cart.dat: an image of the
cartridge with the data the game reads at its place and nothing where the
original's code was. The list of what is data is assets.csv beside this."""
import csv, hashlib, os, sys

SHA1 = '781e376ff453a83c7bcf72339abe6b842efd9c86'


def main():
    rom = open(sys.argv[1], 'rb').read()
    if len(rom) & 0x200:
        rom = rom[0x200:]
    if hashlib.sha1(rom).hexdigest() != SHA1:
        sys.exit('extract_assets: this is not the expected Stunt Race FX (USA) (Rev 1) cartridge')
    image = bytearray(len(rom))
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, 'assets.csv')) as f:
        for row in csv.DictReader(f):
            start, end = int(row['start'], 16), int(row['end'], 16)
            image[start:end] = rom[start:end]
    os.makedirs(sys.argv[2], exist_ok=True)
    with open(os.path.join(sys.argv[2], 'cart.dat'), 'wb') as f:
        f.write(image)


main()
