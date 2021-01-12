#!/usr/bin/python3

import os
import subprocess
import sys

orig_arg = sys.argv[1]
target_arg =sys.argv[2]
exec_name = sys.argv[3]

prefix = os.environ.get('MESON_INSTALL_DESTDIR_PREFIX')

orig_path = os.path.join(prefix, orig_arg, exec_name)
dest_path = os.path.join(prefix, target_arg, exec_name)

if os.path.lexists(dest_path):
    subprocess.call(['unlink', dest_path])

print('Adding symlink %s -> %s' % (orig_path, dest_path))
subprocess.call(['mkdir', '-p', os.path.dirname(dest_path)])

if not os.environ.get('DESTDIR'):
    subprocess.call(['ln', '-s', orig_path, dest_path])
else:
    subprocess.call(['ln', '-rs', orig_path, dest_path])
