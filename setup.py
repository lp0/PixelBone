#!/usr/bin/env python

"""
setup.py file for SWIG example
"""

from distutils.core import setup, Extension


matrix_module = Extension('_matrix',
                           sources=['matrix_wrap.cxx', 'matrix.cpp',
                                    'gfx.cpp', 'pixel.cpp', 'util.c',
                                    'glcdfont.c'],
			   library_dirs=['/usr/local/lib']
                           )

setup (name = 'matrix',
       version = '0.1',
       author      = "SWIG Docs",
       description = """Simple swig example from docs""",
       ext_modules = [matrix_module],
       py_modules = ["matrix"],
       )
