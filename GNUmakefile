bindir =	/bin
sbindir =	/sbin
filesdir =	/etc/tmpfiles.d

INSTALL =	install
INSTALL_PROG =	$(INSTALL) -D -p -m 0755
INSTALL_DATA =	$(INSTALL) -D -p -m 0644

CFLAGS = -Wall -W  -Wno-unused-parameter -std=gnu99 \
	-Wp,-D_FORTIFY_SOURCE=2 -DENABLE_SYSTEMD -O2

diet_progs =			\
	elito-genfiles		\
	elito-wait-for-file	\
	init.wrapper		\
	redir-outerr		\
	sysctl.minit		\
	elito-mmc-boot

bin_targets = \
	elito-genfiles		\
	elito-wait-for-file	\
	redir-outerr		\
	elito-mmc-boot

sbin_targets =	\
	init.wrapper		\
	sysctl.minit

files_targets =

_bin_targets =		$(addprefix $(DESTDIR)$(bindir)/,$(bin_targets))
_sbin_targets =		$(addprefix $(DESTDIR)$(sbindir)/,$(sbin_targets))
_files_targets =	$(addprefix $(DESTDIR)$(filesdir)/,$(files_targets))
_targets =		$(_bin_targets) $(_sbin_targets) $(_files_targets)

DIST_FILES =		$(addsuffix .c,$(diet_progs))	\
			GNUmakefile
export DIST_FILES

all:	$(diet_progs)

install:	${_targets}
	echo $<

clean:
	rm -f ${diet_progs}

$(_bin_targets): $(DESTDIR)$(bindir)/% : %
	$(INSTALL_PROG) '$<' '$@'

$(_sbin_targets): $(DESTDIR)$(sbindir)/% : %
	$(INSTALL_PROG) '$<' '$@'

$(_files_targets): $(DESTDIR)$(filesdir)/% : %
	$(INSTALL_DATA) '$<' '$@'

elito-mmc-boot:	LDFLAGS += -static

$(diet_progs): % : %.c
	$(DIET) $(CC) -std=c99 $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< -o $@
