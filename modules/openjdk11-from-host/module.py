from osv.modules.filemap import FileMap
from osv.modules import api
import os, os.path
import subprocess

#api.require('java-cmd')
api.require('libz')
provides = ['java','java11']

#Verify that the jdk exists by trying to locate javac (java compiler)
if subprocess.call(['which', 'javac']) != 0:
     print('Could not find any jdk on the host. Please install openjdk11!')
     os.exit(-1)

java_version = subprocess.check_output(['java', '-version'], stderr=subprocess.STDOUT).decode('utf-8')
if not 'openjdk version "11.0' in java_version:
    print('Could not find openjdk version 11 on the host. Please install openjdk11!')
    os.exit(-1)

#javac_path = subprocess.check_output(['which', 'javac']).decode('utf-8').split('\n')[0]
#javac_real_path = os.path.realpath(javac_path)
javac_real_path = os.path.realpath('/usr/lib/jvm/java-11-openjdk-arm64/bin/javac')
jdk_path = os.path.dirname(os.path.dirname(javac_real_path))
print(jdk_path)

usr_files = FileMap()

jdk_dir = os.path.basename(jdk_path)

usr_files.add(jdk_path).to('/usr/lib/jvm/java') \
    .include('lib/**') \
    .exclude('lib/security/cacerts') \
    .exclude('man/**') \
    .exclude('bin/**') \
    .include('bin/java')

usr_files.link('/usr/lib/jvm/' + jdk_dir).to('java')
#usr_files.link('/usr/lib/jvm/jre').to('java/jre')
usr_files.link('/usr/lib/jvm/java/lib/security/cacerts').to('/etc/pki/java/cacerts')
usr_files.link('/usr/bin/java').to('/usr/lib/jvm/bin/java')
