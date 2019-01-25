#!/usr/bin/env python

import os
import sys

config_map = {}

def map_config(source_dir, site_config):
    global config_map
    source_dir = os.path.realpath(source_dir)
    source_dir = os.path.normcase(source_dir)
    site_config = os.path.normpath(site_config)
    config_map[source_dir] = site_config

# Variables configured at build time.
llvm_source_root = "C:/emslave/buildslave/win-tagbuilder/emsdk/clang/tag-e1.38.20/src"
llvm_obj_root = "C:/emslave/buildslave/win-tagbuilder/emsdk/clang/tag-e1.38.20/build_tag-e1.38.20_vs2017_64"

# Make sure we can find the lit package.
sys.path.insert(0, os.path.join(llvm_source_root, 'utils', 'lit'))

# Set up some builtin parameters, so that by default the LLVM test suite
# configuration file knows how to find the object tree.
builtin_parameters = { 'build_mode' : "Release" }



builtin_parameters['config_map'] = config_map

if __name__=='__main__':
    from lit.main import main
    main(builtin_parameters)
