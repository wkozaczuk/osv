#!/usr/bin/python3

import subprocess, os, string, sys
from linux_distro import linux_distribution

def aarch64_download(version):
    boost_packages = ['libboost%s-dev' % version,
                      'libboost-system%s' % version,
                      'libboost-system%s-dev' % version,
                      'libboost-filesystem%s' % version,
                      'libboost-filesystem%s-dev' % version,
                      'libboost-test%s' % version,
                      'libboost-test%s-dev' % version,
                      'libboost-timer%s' % version,
                      'libboost-timer%s-dev' % version,
                      'libboost-chrono%s' % version,
                      'libboost-chrono%s-dev' % version]

    osv_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
    script_path = '%s/scripts/download_ubuntu_aarch64_deb_package.sh' % osv_root
    destination = '%s/build/downloaded_packages/aarch64' % osv_root

    install_commands = ['%s boost%s %s %s/boost' % (script_path, version, package, destination) for package in boost_packages]
    install_commands = ['rm -rf %s/boost/install' % destination] + install_commands
    return ' && '.join(install_commands)

(name, version) = linux_distribution()
if name.lower() != 'ubuntu':
    print("The distribution %s is not supported for cross-compiling aarch64 version of OSv" % name)
    sys.exit(1)

print('Downloading aarch64 packages to cross-compile ARM version ...')
subprocess.check_call(aarch64_download('1.71'), shell=True)
print('Downloaded all aarch64 packages!')
