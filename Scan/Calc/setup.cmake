###| CMake Kiibohd Controller Macro Module |###
#
# Written by Cui Yuting in 2016 for the Kiibohd Controller
# Originally writtern by Jacob Alexander in 2014-2015 for the Kiibohd Controller
#
# Released into the Public Domain
#
###

###
# Sub-module flag, cannot be included stand-alone
#
set ( SubModule 1 )

###
# Required Sub-modules
#
AddModule ( Scan STLcd )

###
# Module C files
#

set ( Module_SRCS
	calc.c
)


###
# Compiler Family Compatibility
#
set ( ModuleCompatibility
	arm
)
