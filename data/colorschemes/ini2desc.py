#!/usr/bin/env python3

import argparse
import configparser

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

def parse_color(color_string):
    h = color_string.lstrip('#')
    if len(h) == 6:
        return tuple(int(h[i:i+2], 16) for i in (0, 2, 4)) + (255,)
    elif len(h) == 3:
        return tuple(int(h[i]+h[i], 16) for i in (0, 1, 2)) + (255,)
    elif len(h) == 8:
        return tuple(int(h[i:i+2], 16) for i in (0, 2, 4, 6))
    elif len(h) == 4:
        return tuple(int(h[i]+h[i], 16) for i in (0, 1, 2, 3))

def write_color(color_string):
    (r, g, b, a) = parse_color(color_string)
    out.write('        group "Color" struct {\n')
    out.write('            value "r" uchar: {};\n'.format(r))
    out.write('            value "g" uchar: {};\n'.format(g))
    out.write('            value "b" uchar: {};\n'.format(b))
    out.write('            value "a" uchar: {};\n'.format(a))
    out.write('        }\n')

def write_name_color(color_name, default):
    out.write('    group "{}" struct {{\n'.format(color_name))
    write_color(cfg.get('Colors', color_name, fallback=default))
    out.write('    }\n')

write_name_color('def', '#aaaaaa')
write_name_color('bg', '#202020')
write_name_color('fg', '#aaaaaa')
write_name_color('main', '#3599ff')
write_name_color('hl', '#ffffff')
write_name_color('end_sel', '#ff3300')
write_name_color('tab_missed_1', '#ff9933')
write_name_color('tab_missed_2', '#ff3300')
write_name_color('tab_missed_3', '#ff0000')
write_name_color('tab_missed_over_1', '#ffff40')
write_name_color('tab_missed_over_2', '#ff9933')
write_name_color('tab_missed_over_3', '#ff0000')
write_name_color('tab_title_2', '#000000')

def write_ansi():
    out.write('    group "ansi" array {\n')
    out.write('        count 16;\n')
    default = ['#000000',
               '#cc3333',
               '#33cc33',
               '#cc8833',
               '#3333cc',
               '#cc33cc',
               '#33cccc',
               '#cccccc',
               '#666666',
               '#ff6666',
               '#66ff66',
               '#ffff66',
               '#6666ff',
               '#ff66ff',
               '#66ffff',
               '#ffffff']

    for c in range(15):
        write_color(cfg.get('Ansi', 'ansi{0:02d}'.format(c),
                            fallback=default[c]))
    out.write('    }\n')

write_ansi()
out.write('}\n')
