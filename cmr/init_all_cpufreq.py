#!/usr/bin/python3

import subprocess

freq = 2100000
for cpu_id in range(15):
    subprocess.run(args=('sudo', 'tee', f'/sys/devices/system/cpu/cpu{cpu_id}/cpufreq/scaling_setspeed'), check=True,
                   input=f'{freq}\n', encoding='ASCII', stdout=subprocess.DEVNULL)

