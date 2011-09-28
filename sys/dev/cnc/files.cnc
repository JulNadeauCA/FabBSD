# $FabBSD$

# CNC controller interface.
pseudo-device	cnc

file	dev/cnc/cnc.c			cnc needs-flag
file	dev/cnc/cnc_stubs.c
file	dev/cnc/cnc_capture.c		cnc needs-flag
file	dev/cnc/cnc_device.c		cnc needs-flag
file	dev/cnc/cnc_math.c		cnc needs-flag
file	dev/cnc/cnc_quintic.c		cnc needs-flag
file	dev/cnc/cnc_tools.c		cnc needs-flag

# Servo or stepping motor with clock/direction interface.
device	servo
attach	servo at gpio
file	dev/cnc/cnc_servo.c		servo		needs-flag

# Machine tool spindle controller.
device	spindle
attach	spindle at gpio
file	dev/cnc/cnc_spindle.c		spindle		needs-flag

# Control panel for CNC machining equipment.
device	cncpanel
attach	cncpanel at gpio
file	dev/cnc/cnc_panel.c		cncpanel	needs-flag

# Emergency stop button and/or limit switches.
device	estop
attach	estop at gpio
file	dev/cnc/cnc_estop.c		estop		needs-flag

# General-purpose position-sensing encoder.
device	encoder
attach	encoder at gpio
file	dev/cnc/cnc_encoder.c		encoder		needs-flag

# Manual pulse generator with optional axis selection.
device	mpg
attach	mpg at gpio
file	dev/cnc/cnc_mpg.c		mpg		needs-flag

# General-purpose status LED.
device	cncstatled
attach	cncstatled at gpio
file	dev/cnc/cnc_statled.c		cncstatled	needs-flag

# Serial LCD interface (std. 8250/16[45]50 in polled mode).
device	cnclcd
file	dev/cnc/cnc_lcd.c		cnclcd		needs-flag
attach	cnclcd at isa with cnclcd_isa
file	dev/cnc/cnc_lcd_isa.c		cnclcd_isa

# Spot welder controller.
device	spotwelder
attach	spotwelder at gpio
file	dev/cnc/cnc_spotwelder.c	spotwelder	needs-flag
