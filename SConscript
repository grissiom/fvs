from building import *
from os.path import join as pj

cwd     = GetCurrentDir()
src     = ['fvs.c']
CPPPATH = [cwd]

import rtconfig

rtcfg_attrs = dir(rtconfig)

if 'EFM32_TYPE' in rtcfg_attrs:
    src.append('hal/efm32gg/hw.c')
    CPPPATH.append(pj(cwd, 'hal/efm32gg/'))
elif 'STM32_TYPE' in rtcfg_attrs:
    src.append('hal/stm32f10x/hw.c')
    CPPPATH.append(pj(cwd, 'hal/stm32f10x/'))
else:
    import sys
    print "MCU type not supported by FVS"
    sys.exit(2)

group   = DefineGroup('fvs', src,
        depend = ['RT_USING_FVS'],
        CPPPATH = CPPPATH)

Return('group')
