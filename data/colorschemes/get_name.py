#!/usr/bin/env python3

import argparse
import configparser
import sys

def main():
    parser = argparse.ArgumentParser(description='Get color scheme name from an INI colorschemes description file.')
    parser.add_argument('input_file',
                        type=argparse.FileType('r'),
                        help='INI File to convert')
    args = parser.parse_args()

    cfg = configparser.ConfigParser()
    cfg.read_file(args.input_file)

    print(cfg.get('Metadata', 'name'))


if __name__ == "__main__":
    main()
