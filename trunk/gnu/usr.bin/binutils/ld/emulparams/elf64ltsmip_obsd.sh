. ${srcdir}/emulparams/elf64ltsmip.sh
MAXPAGESIZE=0x10000
TEXT_START_ADDR="0x10000000"
. ${srcdir}/emulparams/elf_obsd.sh
# XXX causes GOT oflows
NO_PAD_CDTOR=y
DATA_START_SYMBOLS='_fdata = . ; __data_start = . ;'
