#!/usr/bin/env python3

import argparse
import json

parser = argparse.ArgumentParser(description='Convert JSON colorschemes to EET description files.')
parser.add_argument('input_file',
                    type=argparse.FileType('r'),
                    help='JSON File to convert')
parser.add_argument('output_file',
                    type=argparse.FileType('w'),
                    help='EET description to write')
args = parser.parse_args()

d = json.load(args.input_file)
out = args.output_file

out.write('group "Color_Scheme" struct {\n')
out.write('    value "version" int: {};\n'
          .format(d['version']))
out.write('    value "md.version" int: {};\n'
          .format(d['md']['version']))
out.write('    value "md.name" string: "{}";\n'
          .format(d['md']['name']))
out.write('    value "md.author" string: "{}";\n'
          .format(d['md']['author']))
out.write('    value "md.website" string: "{}";\n'
          .format(d['md']['website']))
out.write('    value "md.license" string: "{}";\n'
          .format(d['md']['license']))

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


def write_name_color(color_name):
    out.write('    group "{}" struct {{\n'.format(color_name))
    write_color(d[color_name])
    out.write('    }\n')


write_name_color('def')
write_name_color('bg')
write_name_color('fg')
write_name_color('main')
write_name_color('hl')
write_name_color('end_sel')
write_name_color('tab_missed_1')
write_name_color('tab_missed_2')
write_name_color('tab_missed_3')
write_name_color('tab_missed_over_1')
write_name_color('tab_missed_over_2')
write_name_color('tab_missed_over_3')
write_name_color('tab_title_2')

def write_ansi():
    assert(len(d['ansi']) == 16)
    out.write('    group "ansi" array {\n')
    out.write('        count 16;\n')
    for c in d['ansi']:
        write_color(c)
    out.write('    }\n')

write_ansi()
out.write('}\n')
