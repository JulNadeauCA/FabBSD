/*	$FabBSD$	*/

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from;	FabBSD
 */

char *syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"wait4",			/* 7 = wait4 */
	"#8 (obsolete creat)",		/* 8 = obsolete creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"#11 (obsolete execv)",		/* 11 = obsolete execv */
	"chdir",			/* 12 = chdir */
	"fchdir",			/* 13 = fchdir */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"chown",			/* 16 = chown */
	"break",			/* 17 = break */
	"#18 (obsolete getfsstat)",		/* 18 = obsolete getfsstat */
	"#19 (obsolete lseek)",		/* 19 = obsolete lseek */
	"getpid",			/* 20 = getpid */
	"mount",			/* 21 = mount */
	"unmount",			/* 22 = unmount */
	"setuid",			/* 23 = setuid */
	"getuid",			/* 24 = getuid */
	"geteuid",			/* 25 = geteuid */
#ifdef PTRACE
	"ptrace",			/* 26 = ptrace */
#else
	"#26 (unimplemented ptrace)",		/* 26 = unimplemented ptrace */
#endif
	"recvmsg",			/* 27 = recvmsg */
	"sendmsg",			/* 28 = sendmsg */
	"recvfrom",			/* 29 = recvfrom */
	"accept",			/* 30 = accept */
	"getpeername",			/* 31 = getpeername */
	"getsockname",			/* 32 = getsockname */
	"access",			/* 33 = access */
	"chflags",			/* 34 = chflags */
	"fchflags",			/* 35 = fchflags */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"#38 (obsolete stat)",		/* 38 = obsolete stat */
	"getppid",			/* 39 = getppid */
	"#40 (obsolete lstat)",		/* 40 = obsolete lstat */
	"dup",			/* 41 = dup */
	"opipe",			/* 42 = opipe */
	"getegid",			/* 43 = getegid */
	"profil",			/* 44 = profil */
#ifdef KTRACE
	"ktrace",			/* 45 = ktrace */
#else
	"#45 (unimplemented ktrace)",		/* 45 = unimplemented ktrace */
#endif
	"sigaction",			/* 46 = sigaction */
	"getgid",			/* 47 = getgid */
	"sigprocmask",			/* 48 = sigprocmask */
	"getlogin",			/* 49 = getlogin */
	"setlogin",			/* 50 = setlogin */
	"#51 (unimplemented acct)",		/* 51 = unimplemented acct */
	"sigpending",			/* 52 = sigpending */
	"osigaltstack",			/* 53 = osigaltstack */
	"ioctl",			/* 54 = ioctl */
	"reboot",			/* 55 = reboot */
	"revoke",			/* 56 = revoke */
	"symlink",			/* 57 = symlink */
	"readlink",			/* 58 = readlink */
	"execve",			/* 59 = execve */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"#62 (obsolete fstat)",		/* 62 = obsolete fstat */
	"#63 (obsolete getkerninfo)",		/* 63 = obsolete getkerninfo */
	"#64 (obsolete getpagesize)",		/* 64 = obsolete getpagesize */
	"#65 (obsolete msync)",		/* 65 = obsolete msync */
	"vfork",			/* 66 = vfork */
	"#67 (obsolete vread)",		/* 67 = obsolete vread */
	"#68 (obsolete vwrite)",		/* 68 = obsolete vwrite */
	"sbrk",			/* 69 = sbrk */
	"sstk",			/* 70 = sstk */
	"#71 (obsolete mmap)",		/* 71 = obsolete mmap */
	"#72 (obsolete vadvise)",		/* 72 = obsolete vadvise */
	"munmap",			/* 73 = munmap */
	"mprotect",			/* 74 = mprotect */
	"madvise",			/* 75 = madvise */
	"#76 (obsolete vhangup)",		/* 76 = obsolete vhangup */
	"#77 (obsolete vlimit)",		/* 77 = obsolete vlimit */
	"mincore",			/* 78 = mincore */
	"getgroups",			/* 79 = getgroups */
	"setgroups",			/* 80 = setgroups */
	"getpgrp",			/* 81 = getpgrp */
	"setpgid",			/* 82 = setpgid */
	"setitimer",			/* 83 = setitimer */
	"#84 (obsolete wait)",		/* 84 = obsolete wait */
	"#85 (obsolete swapon)",		/* 85 = obsolete swapon */
	"getitimer",			/* 86 = getitimer */
	"#87 (obsolete gethostname)",		/* 87 = obsolete gethostname */
	"#88 (obsolete sethostname)",		/* 88 = obsolete sethostname */
	"#89 (obsolete getdtablesize)",		/* 89 = obsolete getdtablesize */
	"dup2",			/* 90 = dup2 */
	"#91 (unimplemented getdopt)",		/* 91 = unimplemented getdopt */
	"fcntl",			/* 92 = fcntl */
	"select",			/* 93 = select */
	"#94 (unimplemented setdopt)",		/* 94 = unimplemented setdopt */
	"fsync",			/* 95 = fsync */
	"setpriority",			/* 96 = setpriority */
	"socket",			/* 97 = socket */
	"connect",			/* 98 = connect */
	"#99 (obsolete accept)",		/* 99 = obsolete accept */
	"getpriority",			/* 100 = getpriority */
	"#101 (obsolete send)",		/* 101 = obsolete send */
	"#102 (obsolete recv)",		/* 102 = obsolete recv */
	"sigreturn",			/* 103 = sigreturn */
	"bind",			/* 104 = bind */
	"setsockopt",			/* 105 = setsockopt */
	"listen",			/* 106 = listen */
	"#107 (obsolete vtimes)",		/* 107 = obsolete vtimes */
	"#108 (obsolete sigvec)",		/* 108 = obsolete sigvec */
	"#109 (obsolete sigblock)",		/* 109 = obsolete sigblock */
	"#110 (obsolete sigsetmask)",		/* 110 = obsolete sigsetmask */
	"sigsuspend",			/* 111 = sigsuspend */
	"#112 (obsolete sigstack)",		/* 112 = obsolete sigstack */
	"#113 (obsolete recvmsg)",		/* 113 = obsolete recvmsg */
	"#114 (obsolete sendmsg)",		/* 114 = obsolete sendmsg */
	"#115 (obsolete vtrace)",		/* 115 = obsolete vtrace */
	"gettimeofday",			/* 116 = gettimeofday */
	"getrusage",			/* 117 = getrusage */
	"getsockopt",			/* 118 = getsockopt */
	"#119 (obsolete resuba)",		/* 119 = obsolete resuba */
	"readv",			/* 120 = readv */
	"writev",			/* 121 = writev */
	"settimeofday",			/* 122 = settimeofday */
	"fchown",			/* 123 = fchown */
	"fchmod",			/* 124 = fchmod */
	"#125 (obsolete recvfrom)",		/* 125 = obsolete recvfrom */
	"setreuid",			/* 126 = setreuid */
	"setregid",			/* 127 = setregid */
	"rename",			/* 128 = rename */
	"#129 (obsolete truncate)",		/* 129 = obsolete truncate */
	"#130 (obsolete ftruncate)",		/* 130 = obsolete ftruncate */
	"flock",			/* 131 = flock */
	"mkfifo",			/* 132 = mkfifo */
	"sendto",			/* 133 = sendto */
	"shutdown",			/* 134 = shutdown */
	"socketpair",			/* 135 = socketpair */
	"mkdir",			/* 136 = mkdir */
	"rmdir",			/* 137 = rmdir */
	"utimes",			/* 138 = utimes */
	"#139 (obsolete 4.2 sigreturn)",		/* 139 = obsolete 4.2 sigreturn */
	"adjtime",			/* 140 = adjtime */
	"#141 (obsolete getpeername)",		/* 141 = obsolete getpeername */
	"#142 (obsolete gethostid)",		/* 142 = obsolete gethostid */
	"#143 (obsolete sethostid)",		/* 143 = obsolete sethostid */
	"#144 (obsolete getrlimit)",		/* 144 = obsolete getrlimit */
	"#145 (obsolete setrlimit)",		/* 145 = obsolete setrlimit */
	"#146 (obsolete killpg)",		/* 146 = obsolete killpg */
	"setsid",			/* 147 = setsid */
	"quotactl",			/* 148 = quotactl */
	"#149 (obsolete quota)",		/* 149 = obsolete quota */
	"#150 (obsolete getsockname)",		/* 150 = obsolete getsockname */
	"#151 (unimplemented)",		/* 151 = unimplemented */
	"#152 (unimplemented)",		/* 152 = unimplemented */
	"#153 (unimplemented)",		/* 153 = unimplemented */
	"#154 (unimplemented)",		/* 154 = unimplemented */
#if defined(NFSCLIENT) || defined(NFSSERVER)
	"nfssvc",			/* 155 = nfssvc */
#else
	"#155 (unimplemented)",		/* 155 = unimplemented */
#endif
	"#156 (obsolete getdirentries)",		/* 156 = obsolete getdirentries */
	"#157 (obsolete statfs)",		/* 157 = obsolete statfs */
	"#158 (obsolete fstatfs)",		/* 158 = obsolete fstatfs */
	"#159 (unimplemented)",		/* 159 = unimplemented */
	"#160 (unimplemented)",		/* 160 = unimplemented */
	"getfh",			/* 161 = getfh */
	"#162 (obsolete getdomainname)",		/* 162 = obsolete getdomainname */
	"#163 (obsolete setdomainname)",		/* 163 = obsolete setdomainname */
	"#164 (unimplemented ouname)",		/* 164 = unimplemented ouname */
	"sysarch",			/* 165 = sysarch */
	"#166 (unimplemented)",		/* 166 = unimplemented */
	"#167 (unimplemented)",		/* 167 = unimplemented */
	"#168 (unimplemented)",		/* 168 = unimplemented */
	"#169 (obsolete semsys)",		/* 169 = obsolete semsys */
	"#170 (obsolete msgsys)",		/* 170 = obsolete msgsys */
	"#171 (obsolete shmsys)",		/* 171 = obsolete shmsys */
	"#172 (unimplemented)",		/* 172 = unimplemented */
	"pread",			/* 173 = pread */
	"pwrite",			/* 174 = pwrite */
	"#175 (unimplemented ntp_gettime)",		/* 175 = unimplemented ntp_gettime */
	"#176 (unimplemented ntp_adjtime)",		/* 176 = unimplemented ntp_adjtime */
	"#177 (unimplemented)",		/* 177 = unimplemented */
	"#178 (unimplemented)",		/* 178 = unimplemented */
	"#179 (unimplemented)",		/* 179 = unimplemented */
	"#180 (unimplemented)",		/* 180 = unimplemented */
	"setgid",			/* 181 = setgid */
	"setegid",			/* 182 = setegid */
	"seteuid",			/* 183 = seteuid */
	"#184 (unimplemented)",		/* 184 = unimplemented */
	"#185 (unimplemented)",		/* 185 = unimplemented */
	"#186 (unimplemented)",		/* 186 = unimplemented */
	"#187 (unimplemented)",		/* 187 = unimplemented */
	"#188 (obsolete stat)",		/* 188 = obsolete stat */
	"#189 (obsolete fstat)",		/* 189 = obsolete fstat */
	"#190 (obsolete lstat)",		/* 190 = obsolete lstat */
	"pathconf",			/* 191 = pathconf */
	"fpathconf",			/* 192 = fpathconf */
	"swapctl",			/* 193 = swapctl */
	"getrlimit",			/* 194 = getrlimit */
	"setrlimit",			/* 195 = setrlimit */
	"getdirentries",			/* 196 = getdirentries */
	"mmap",			/* 197 = mmap */
	"__syscall",			/* 198 = __syscall */
	"lseek",			/* 199 = lseek */
	"truncate",			/* 200 = truncate */
	"ftruncate",			/* 201 = ftruncate */
	"__sysctl",			/* 202 = __sysctl */
	"mlock",			/* 203 = mlock */
	"munlock",			/* 204 = munlock */
	"#205 (unimplemented sys_undelete)",		/* 205 = unimplemented sys_undelete */
	"futimes",			/* 206 = futimes */
	"getpgid",			/* 207 = getpgid */
	"#208 (unimplemented)",		/* 208 = unimplemented */
	"#209 (unimplemented)",		/* 209 = unimplemented */
#ifdef LKM
	"lkmnosys",			/* 210 = lkmnosys */
	"lkmnosys",			/* 211 = lkmnosys */
	"lkmnosys",			/* 212 = lkmnosys */
	"lkmnosys",			/* 213 = lkmnosys */
	"lkmnosys",			/* 214 = lkmnosys */
	"lkmnosys",			/* 215 = lkmnosys */
	"lkmnosys",			/* 216 = lkmnosys */
	"lkmnosys",			/* 217 = lkmnosys */
	"lkmnosys",			/* 218 = lkmnosys */
	"lkmnosys",			/* 219 = lkmnosys */
#else	/* !LKM */
	"#210 (unimplemented)",		/* 210 = unimplemented */
	"#211 (unimplemented)",		/* 211 = unimplemented */
	"#212 (unimplemented)",		/* 212 = unimplemented */
	"#213 (unimplemented)",		/* 213 = unimplemented */
	"#214 (unimplemented)",		/* 214 = unimplemented */
	"#215 (unimplemented)",		/* 215 = unimplemented */
	"#216 (unimplemented)",		/* 216 = unimplemented */
	"#217 (unimplemented)",		/* 217 = unimplemented */
	"#218 (unimplemented)",		/* 218 = unimplemented */
	"#219 (unimplemented)",		/* 219 = unimplemented */
#endif	/* !LKM */
#ifdef SYSVSEM
	"#220 (obsolete semctl)",		/* 220 = obsolete semctl */
	"semget",			/* 221 = semget */
	"#222 (obsolete semop)",		/* 222 = obsolete semop */
	"#223 (obsolete sys_semconfig)",		/* 223 = obsolete sys_semconfig */
#else
	"#220 (unimplemented semctl)",		/* 220 = unimplemented semctl */
	"#221 (unimplemented semget)",		/* 221 = unimplemented semget */
	"#222 (unimplemented semop)",		/* 222 = unimplemented semop */
	"#223 (unimplemented semconfig)",		/* 223 = unimplemented semconfig */
#endif
#ifdef SYSVMSG
	"#224 (obsolete msgctl)",		/* 224 = obsolete msgctl */
	"msgget",			/* 225 = msgget */
	"msgsnd",			/* 226 = msgsnd */
	"msgrcv",			/* 227 = msgrcv */
#else
	"#224 (unimplemented msgctl)",		/* 224 = unimplemented msgctl */
	"#225 (unimplemented msgget)",		/* 225 = unimplemented msgget */
	"#226 (unimplemented msgsnd)",		/* 226 = unimplemented msgsnd */
	"#227 (unimplemented msgrcv)",		/* 227 = unimplemented msgrcv */
#endif
#ifdef SYSVSHM
	"shmat",			/* 228 = shmat */
	"#229 (obsolete shmctl)",		/* 229 = obsolete shmctl */
	"shmdt",			/* 230 = shmdt */
	"#231 (obsolete shmget)",		/* 231 = obsolete shmget */
#else
	"#228 (unimplemented shmat)",		/* 228 = unimplemented shmat */
	"#229 (unimplemented shmctl)",		/* 229 = unimplemented shmctl */
	"#230 (unimplemented shmdt)",		/* 230 = unimplemented shmdt */
	"#231 (unimplemented shmget)",		/* 231 = unimplemented shmget */
#endif
	"clock_gettime",			/* 232 = clock_gettime */
	"clock_settime",			/* 233 = clock_settime */
	"clock_getres",			/* 234 = clock_getres */
	"#235 (unimplemented timer_create)",		/* 235 = unimplemented timer_create */
	"#236 (unimplemented timer_delete)",		/* 236 = unimplemented timer_delete */
	"#237 (unimplemented timer_settime)",		/* 237 = unimplemented timer_settime */
	"#238 (unimplemented timer_gettime)",		/* 238 = unimplemented timer_gettime */
	"#239 (unimplemented timer_getoverrun)",		/* 239 = unimplemented timer_getoverrun */
	"nanosleep",			/* 240 = nanosleep */
	"#241 (unimplemented)",		/* 241 = unimplemented */
	"#242 (unimplemented)",		/* 242 = unimplemented */
	"#243 (unimplemented)",		/* 243 = unimplemented */
	"#244 (unimplemented)",		/* 244 = unimplemented */
	"#245 (unimplemented)",		/* 245 = unimplemented */
	"#246 (unimplemented)",		/* 246 = unimplemented */
	"#247 (unimplemented)",		/* 247 = unimplemented */
	"#248 (unimplemented)",		/* 248 = unimplemented */
	"#249 (unimplemented)",		/* 249 = unimplemented */
	"minherit",			/* 250 = minherit */
	"rfork",			/* 251 = rfork */
	"poll",			/* 252 = poll */
	"issetugid",			/* 253 = issetugid */
	"lchown",			/* 254 = lchown */
	"getsid",			/* 255 = getsid */
	"msync",			/* 256 = msync */
#ifdef SYSVSEM
	"#257 (obsolete semctl)",		/* 257 = obsolete semctl */
#else
	"#257 (unimplemented)",		/* 257 = unimplemented */
#endif
#ifdef SYSVSHM
	"#258 (obsolete shmctl)",		/* 258 = obsolete shmctl */
#else
	"#258 (unimplemented)",		/* 258 = unimplemented */
#endif
#ifdef SYSVMSG
	"#259 (obsolete msgctl)",		/* 259 = obsolete msgctl */
#else
	"#259 (unimplemented)",		/* 259 = unimplemented */
#endif
	"#260 (obsolete getfsstat)",		/* 260 = obsolete getfsstat */
	"#261 (obsolete statfs)",		/* 261 = obsolete statfs */
	"#262 (obsolete fstatfs)",		/* 262 = obsolete fstatfs */
	"pipe",			/* 263 = pipe */
	"fhopen",			/* 264 = fhopen */
	"#265 (obsolete fhstat)",		/* 265 = obsolete fhstat */
	"#266 (obsolete fhstatfs)",		/* 266 = obsolete fhstatfs */
	"preadv",			/* 267 = preadv */
	"pwritev",			/* 268 = pwritev */
	"kqueue",			/* 269 = kqueue */
	"kevent",			/* 270 = kevent */
	"mlockall",			/* 271 = mlockall */
	"munlockall",			/* 272 = munlockall */
	"getpeereid",			/* 273 = getpeereid */
	"#274 (unimplemented sys_extattrctl)",		/* 274 = unimplemented sys_extattrctl */
	"#275 (unimplemented sys_extattr_set_file)",		/* 275 = unimplemented sys_extattr_set_file */
	"#276 (unimplemented sys_extattr_get_file)",		/* 276 = unimplemented sys_extattr_get_file */
	"#277 (unimplemented sys_extattr_delete_file)",		/* 277 = unimplemented sys_extattr_delete_file */
	"#278 (unimplemented sys_extattr_set_fd)",		/* 278 = unimplemented sys_extattr_set_fd */
	"#279 (unimplemented sys_extattr_get_fd)",		/* 279 = unimplemented sys_extattr_get_fd */
	"#280 (unimplemented sys_extattr_delete_fd)",		/* 280 = unimplemented sys_extattr_delete_fd */
	"getresuid",			/* 281 = getresuid */
	"setresuid",			/* 282 = setresuid */
	"getresgid",			/* 283 = getresgid */
	"setresgid",			/* 284 = setresgid */
	"#285 (obsolete sys_omquery)",		/* 285 = obsolete sys_omquery */
	"mquery",			/* 286 = mquery */
	"closefrom",			/* 287 = closefrom */
	"sigaltstack",			/* 288 = sigaltstack */
#ifdef SYSVSHM
	"shmget",			/* 289 = shmget */
#else
	"#289 (unimplemented shmget)",		/* 289 = unimplemented shmget */
#endif
#ifdef SYSVSEM
	"semop",			/* 290 = semop */
#else
	"#290 (unimplemented semop)",		/* 290 = unimplemented semop */
#endif
	"stat",			/* 291 = stat */
	"fstat",			/* 292 = fstat */
	"lstat",			/* 293 = lstat */
	"fhstat",			/* 294 = fhstat */
#ifdef SYSVSEM
	"__semctl",			/* 295 = __semctl */
#else
	"#295 (unimplemented)",		/* 295 = unimplemented */
#endif
#ifdef SYSVSHM
	"shmctl",			/* 296 = shmctl */
#else
	"#296 (unimplemented)",		/* 296 = unimplemented */
#endif
#ifdef SYSVMSG
	"msgctl",			/* 297 = msgctl */
#else
	"#297 (unimplemented)",		/* 297 = unimplemented */
#endif
	"sched_yield",			/* 298 = sched_yield */
#ifdef RTHREADS
	"getthrid",			/* 299 = getthrid */
	"thrsleep",			/* 300 = thrsleep */
	"thrwakeup",			/* 301 = thrwakeup */
	"threxit",			/* 302 = threxit */
	"thrsigdivert",			/* 303 = thrsigdivert */
#else
	"#299 (unimplemented)",		/* 299 = unimplemented */
	"#300 (unimplemented)",		/* 300 = unimplemented */
	"#301 (unimplemented)",		/* 301 = unimplemented */
	"#302 (unimplemented)",		/* 302 = unimplemented */
	"#303 (unimplemented)",		/* 303 = unimplemented */
#endif
	"__getcwd",			/* 304 = __getcwd */
	"adjfreq",			/* 305 = adjfreq */
	"getfsstat",			/* 306 = getfsstat */
	"statfs",			/* 307 = statfs */
	"fstatfs",			/* 308 = fstatfs */
	"fhstatfs",			/* 309 = fhstatfs */
	"cncmove",			/* 310 = cncmove */
	"cncjog",			/* 311 = cncjog */
	"cncjogstep",			/* 312 = cncjogstep */
	"spinctl",			/* 313 = spinctl */
	"atcctl",			/* 314 = atcctl */
	"laserctl",			/* 315 = laserctl */
	"pickplacectl",			/* 316 = pickplacectl */
	"coolantctl",			/* 317 = coolantctl */
	"estop",			/* 318 = estop */
};