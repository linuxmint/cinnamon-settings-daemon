#!/usr/bin/python3

import os
import subprocess
import sys

libexecdir = sys.argv[1]
bindir =sys.argv[2]
exec_name = sys.argv[3]

prefix = os.environ.get('MESON_INSTALL_DESTDIR_PREFIX')

orig_path = os.path.join(prefix, libexecdir, exec_name)
dest_path = os.path.join(prefix, bindir, exec_name)

if os.path.lexists(dest_path):
    print('%s already exists, skipping symlink creation' % dest_path)
else:
    print('Adding bin symlink %s -> %s' % (orig_path, dest_path))
    subprocess.call(['mkdir', '-p', os.path.dirname(dest_path)])
    subprocess.call(['ln', '-rs', orig_path, dest_path])
