from osv.modules import api

daemon = api.run_on_init('/sysinfo.so --interval 100000 &!')
default = daemon
