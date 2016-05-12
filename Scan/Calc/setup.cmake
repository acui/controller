###| CMake Kiibohd Controller Macro Module |###
#
# Written by Cui Yuting in 2016 for the Kiibohd Controller
#
# Released into the Public Domain
#
###

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

