%{!?release_func:%global release_func() %1%{?dist}}

%if 0%{?elitoeabi}
%undefine	with_dietlibc
%else
%{!?_without_dietlibc:%global	with_dietlibc	with-dietlibc}
%endif

Name:		%ELITO_RPMNAME setup
Version:	0.7
Release:	%release_func 11.1
Summary:	Setup for elito-environment

Group:		%ELITO_GROUP Development
License:	proprietary
Source100:	configure.cache.arm-xscale-linux-gnu
Source101:	configure.cache.arm-xscale-linux-uclibc
Source102:	configure.cache.arm-iwmmxt-linux-gnueabi
Source103:	configure.cache.arm-iwmmxt-linux-uclibceabi
Source110:	configure.cache.x86_64-linux-gnu
Source111:	configure.cache.x86_64-linux-uclibc
Source200:	cflags.arm-xscale-linux-gnu
Source201:	cflags.arm-xscale-linux-uclibc
Source202:	cflags.arm-iwmmxt-linux-gnueabi
Source203:	cflags.arm-iwmmxt-linux-uclibceabi
Source210:	cflags.x86_64-linux-gnu
Source211:	cflags.x86_64-linux-uclibc
Source300:	configure.arm-xscale-linux-uclibc
BuildRoot:	%_tmppath/%name-%version-%release-root

BuildRequires:	elito-buildroot
Requires:	elito-buildroot

%if 0%{!?_with_bootstrap:1}
%package tools
Summary:	Base tools for ELiTo environments
Group:		%ELITO_GROUP System Environment/Base
Source0:	init-wrapper.c
Source1:	redir-outerr.c
Source2:	sysctl.minit.c
%{?with_dietlibc:BuildRequires:	%{ELITO_RPMNAME dietlibc}}
%ELITOSYS_HEADERS


%ELITO_COMMON
%ELITO_FSSETUP


%description tools
%endif


%description


%if 0%{!?_with_bootstrap:1}
%prep
%setup -T -c


%build
D='%{?with_dietlibc:%elitoarch-diet -Os}'
$D %__elito_cc %elito_cflags -Wall -W -std=c99 %SOURCE0 -o init.wrapper
$D %__elito_cc %elito_cflags -Wall -W -std=c99 %SOURCE1 -o redir-outerr
$D %__elito_cc %elito_cflags -Wall -W -std=c99 %SOURCE2 -o sysctl.minit
%endif

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT{%_elito_sysconfdir,%_elitosys_sbindir,%_elitosys_bindir}

f=%_sourcedir/configure.cache.%elitoarch
sed -e 's!@ELITO_BINDIR@!%_elito_bindir!g' \
	$f >$RPM_BUILD_ROOT%_elito_sysconfdir/configure.cache
chmod 0644 $RPM_BUILD_ROOT%_elito_sysconfdir/configure.cache

for i in cflags cxxflags ldflags configure; do
	f=%_sourcedir/$i.%elitoarch
	test -e "$f" || continue
	install -p -m0644 "$f" $RPM_BUILD_ROOT%_elito_sysconfdir/build.$i
done


%if 0%{!?_with_bootstrap:1}
install -p -m0755 init.wrapper sysctl.minit $RPM_BUILD_ROOT%_elitosys_sbindir/
install -p -m0755 redir-outerr              $RPM_BUILD_ROOT%_elitosys_bindir/

%elito_installfixup fssetup
%endif


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%_elito_sysconfdir/*


%if 0%{!?_with_bootstrap:1}
%files tools
  %defattr(-,root,root,-)
  %_elitosys_sbindir/*
  %_elitosys_bindir/*
%endif

%changelog
* Sun Jul  8 2007 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-11
- fixed dietlibc macros

* Sun Jul  8 2007 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-10
- rebuilt

* Tue May 15 2007 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-9
- rebuilt

* Wed May  9 2007 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-8
- rebuilt

* Mon Jan 15 2007 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-7
- rebuilt

* Thu Oct 12 2006 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-6
- added support for arm-iwmmxt platform
- allowed to build without dietlibc (required for iwmmxt)

* Fri Jul 21 2006 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-5
- rebuilt

* Mon Jun 26 2006 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-3
- rebuilt with new dietlibc

* Fri Jun 23 2006 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-2
- enlarged initial /dev to 256k

* Fri Jun 23 2006 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.7-1
- enlarged initial /dev to 128k

* Thu Apr 20 2006 Enrico Scholz <enrico.scholz@sigma-chemnitz.de> - 0.6-1
- added configuration for softfloat targets
