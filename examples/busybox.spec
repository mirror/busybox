Name: busybox
Version: 0.40
Release: 1
Group: System/Utilities
Summary: BusyBox is a tiny suite of Unix utilities in a multi-call binary.
Copyright: GPL
Packager : Erik Andersen <andersen@lineo.com>
Conflicts: fileutils grep shellutils
Buildroot: /tmp/%{Name}-%{Version}
Source: %{Name}-%{Version}.tar.gz

%Description
BusyBox is a suite of "tiny" Unix utilities in a multi-call binary. It
provides a pretty complete POSIX environment in a very small package.
Just add a kernel, "ash" (Keith Almquists tiny Bourne shell clone), and
an editor such as "elvis-tiny" or "ae", and you have a full system. This
is makes an excellent environment for a "rescue" disk or any small or
embedded system.

%Prep
%setup -q -n %{Name}-%{Version}

%Build
BB_INIT_RC_EXIT_CMD=\"/bin/sh\" make

%Install
rm -rf $RPM_BUILD_ROOT
make PREFIX=$RPM_BUILD_ROOT install

%Clean
rm -rf $RPM_BUILD_ROOT

%Files 
%defattr(-,root,root)
/
