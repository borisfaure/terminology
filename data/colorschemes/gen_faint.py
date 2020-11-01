#!/usr/bin/env python3

import argparse
import configparser
import sys
from ini2desc import parse_color

def blend_color(cfg, blend_factor, color_name):
    (r1, g1, b1, a1) = parse_color(cfg.get('Colors', 'bg'))
    (r2, g2, b2, a2) = parse_color(cfg.get('Normal', color_name))
    def blend(c1, c2, f):
        d = c2 - c1
        return int(c1 + d * f)
    r = blend(r1, r2, blend_factor)
    g = blend(g1, g2, blend_factor)
    b = blend(b1, b2, blend_factor)
    a = blend(a1, a2, blend_factor)
    if a != 255:
        cfg.set('Faint', color_name,
                '#{:02x}{:02x}{:02x}{:02x}'.format(r, g, b, a))
    else:
        cfg.set('Faint', color_name,
                '#{:02x}{:02x}{:02x}'.format(r, g, b))

def main():
    parser = argparse.ArgumentParser(description='Generate Faint colors in INI colorschemes description files.')
    parser.add_argument('input_file',
                        type=argparse.FileType('r'),
                        help='INI File to convert')
    parser.add_argument('blend_factor',
                        type=int, nargs='?', default=70,
                        help='blend factor between normal color and background')
    args = parser.parse_args()

    cfg = configparser.ConfigParser()
    cfg.read_file(args.input_file)

    f = args.blend_factor

    assert( 0 < f and f < 100)
    f = f / 100

    if not cfg.has_section('Faint'):
        cfg.add_section('Faint')

    blend_color(cfg, f, 'def')
    blend_color(cfg, f, 'black')
    blend_color(cfg, f, 'red')
    blend_color(cfg, f, 'green')
    blend_color(cfg, f, 'yellow')
    blend_color(cfg, f, 'blue')
    blend_color(cfg, f, 'magenta')
    blend_color(cfg, f, 'cyan')
    blend_color(cfg, f, 'white')
    blend_color(cfg, f, 'inverse_fg')
    blend_color(cfg, f, 'inverse_bg')

    cfg.write(sys.stdout)


if __name__ == "__main__":
    main()
