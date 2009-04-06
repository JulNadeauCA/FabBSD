# $FabBSD$
# $OpenBSD: dot.cshrc,v 1.6 2007/02/16 20:05:49 deraadt Exp $
#
# csh initialization

alias df	df -k
alias du	du -k
alias h		'history -r | more'
alias j		jobs -l
alias la	ls -a
alias lf	ls -FA
alias ll	ls -lsA
alias tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'
alias z		suspend

set path = ( ~/bin /bin /sbin /usr/{bin,sbin,local/bin,local/sbin,games} .)

if ($?prompt) then
	# An interactive shell -- set some stuff up
	set filec
	set history = 1000
	set ignoreeof
	set mch = `hostname -s`
	alias prompt 'set prompt = "$mch:q"":$cwd:t {\!} "'
	alias cd 'cd \!*; prompt'
	alias chdir 'cd \!*; prompt'
	alias popd 'popd \!*; prompt'
	alias pushd 'pushd \!*; prompt'
	cd .
	umask 22
endif
