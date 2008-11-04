bindir =	/bin
sbindir =	/sbin
filesdir =	/etc/files.d

INSTALL =	install
INSTALL_PROG =	$(INSTALL) -D -p -m755
INSTALL_DATA =	$(INSTALL) -D -p -m644

diet_progs =			\
	elito-genfiles		\
	elito-wait-for-file	\
	init.wrapper		\
	redir-outerr		\
	sysctl.minit

bin_targets = 	\
	elito-genfiles		\
	elito-wait-for-file	\
	redir-outerr		\

sbin_targets =	\
	init.wrapper		\
	sysctl.minit		\
	elito-load-modules

files_targets = \
	00-varfs.txt

_bin_targets =		$(addprefix $(DESTDIR)$(bindir)/,$(bin_targets))
_sbin_targets =		$(addprefix $(DESTDIR)$(sbindir)/,$(sbin_targets))
_files_targets =	$(addprefix $(DESTDIR)$(filesdir)/,$(files_targets))
_targets =		$(_bin_targets) $(_sbin_targets) $(_files_targets)

all:	$(diet_progs)

install:	${_targets}
	echo $<

$(_bin_targets): $(DESTDIR)$(bindir)/% : %
	$(INSTALL_PROG) '$<' '$@'

$(_sbin_targets): $(DESTDIR)$(sbindir)/% : %
	$(INSTALL_PROG) '$<' '$@'

$(_files_targets): $(DESTDIR)$(filesdir)/% : %
	$(INSTALL_DATA) '$<' '$@'

$(diet_progs): % : %.c
	$(DIET) $(CC) -std=c99 $(CFLAGS) $(LDFLAGS) $< -o $@


