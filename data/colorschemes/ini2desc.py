#!/usr/bin/env python3

import argparse
import configparser

def ensure_premultiplied(t):
    (r, g, b, a) = t
    if a != 255 and (a > r or a > g or a > b):
        r = (r * a) // 255
        g = (g * a) // 255
        b = (b * a) // 255
    return (r, g, b, a)

def parse_color(color_string):
    h = color_string.lstrip('#')
    if len(h) == 6:
        return tuple(int(h[i:i+2], 16) for i in (0, 2, 4)) + (255,)
    elif len(h) == 3:
        return tuple(int(h[i]+h[i], 16) for i in (0, 1, 2)) + (255,)
    elif len(h) == 8:
        t = tuple(int(h[i:i+2], 16) for i in (0, 2, 4, 6))
        t = ensure_premultiplied(t)
        return t
    elif len(h) == 4:
        t = tuple(int(h[i]+h[i], 16) for i in (0, 1, 2, 3))
        t = ensure_premultiplied(t)
        return t

def write_color(out, color_string):
    (r, g, b, a) = parse_color(color_string)
    out.write('        group "Color" struct {\n')
    out.write('            value "r" uchar: {};\n'.format(r))
    out.write('            value "g" uchar: {};\n'.format(g))
    out.write('            value "b" uchar: {};\n'.format(b))
    out.write('            value "a" uchar: {};\n'.format(a))
    out.write('        }\n')

def write_md(out, cfg):
    out.write('    value "version" int: {};\n'
              .format(cfg.get('Main', 'version', fallback='1')))

    out.write('    value "md.version" int: {};\n'
              .format(cfg.get('Metadata', 'version', fallback='1')))
    out.write('    value "md.name" string: "{}";\n'
              .format(cfg['Metadata']['name']))
    out.write('    value "md.author" string: "{}";\n'
              .format(cfg['Metadata']['author']))
    out.write('    value "md.website" string: "{}";\n'
              .format(cfg.get('Metadata', 'website', fallback='')))
    out.write('    value "md.license" string: "{}";\n'
              .format(cfg['Metadata']['license']))

def write_named_color(out, cfg, section, color_name, default=None):
    out.write('    group "{}" struct {{\n'.format(color_name))
    write_color(out, cfg.get(section, color_name, fallback=default))
    out.write('    }\n')

def write_ui_colors(out, cfg):
    write_named_color(out, cfg, 'Colors', 'def', '#aaaaaa')
    write_named_color(out, cfg, 'Colors', 'bg', '#202020')
    write_named_color(out, cfg, 'Colors', 'fg', '#aaaaaa')
    write_named_color(out, cfg, 'Colors', 'main', '#3599ff')
    write_named_color(out, cfg, 'Colors', 'hl', '#ffffff')
    write_named_color(out, cfg, 'Colors', 'end_sel', '#ff3300')
    write_named_color(out, cfg, 'Colors', 'tab_missed_1', '#ff9933')
    write_named_color(out, cfg, 'Colors', 'tab_missed_2', '#ff3300')
    write_named_color(out, cfg, 'Colors', 'tab_missed_3', '#ff0000')
    write_named_color(out, cfg, 'Colors', 'tab_missed_over_1', '#ffff40')
    write_named_color(out, cfg, 'Colors', 'tab_missed_over_2', '#ff9933')
    write_named_color(out, cfg, 'Colors', 'tab_missed_over_3', '#ff0000')
    write_named_color(out, cfg, 'Colors', 'tab_title_2', '#000000')

def write_color_block(out, cfg, block):
    out.write('    group "{}" struct {{\n'.format(block))
    out.write('    group "Color_Block" struct {\n')
    write_named_color(out, cfg, block, 'def')
    write_named_color(out, cfg, block, 'black')
    write_named_color(out, cfg, block, 'red')
    write_named_color(out, cfg, block, 'green')
    write_named_color(out, cfg, block, 'yellow')
    write_named_color(out, cfg, block, 'blue')
    write_named_color(out, cfg, block, 'magenta')
    write_named_color(out, cfg, block, 'cyan')
    write_named_color(out, cfg, block, 'white')
    write_named_color(out, cfg, block, 'inverse_fg')
    write_named_color(out, cfg, block, 'inverse_bg')
    out.write('    }\n')
    out.write('    }\n')

def main():
    parser = argparse.ArgumentParser(description='Convert INI colorschemes to EET description files.')
    parser.add_argument('input_file',
                        type=argparse.FileType('r'),
                        help='INI File to convert')
    parser.add_argument('output_file',
                        type=argparse.FileType('w'),
                        help='EET description to write')
    args = parser.parse_args()

    cfg = configparser.ConfigParser()
    cfg.read_file(args.input_file)

    out = args.output_file

    assert(int(cfg['Main']['version']) == 1)
    out.write('group "Color_Scheme" struct {\n')
    write_md(out, cfg)
    write_ui_colors(out, cfg)
    write_color_block(out, cfg, 'Normal')
    write_color_block(out, cfg, 'Bright')
    write_color_block(out, cfg, 'Faint')
    out.write('}\n')

if __name__ == "__main__":
    main()
