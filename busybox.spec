Name: busybox
Version: 0.44
Release: 1
Group: System/Utilities
Summary: BusyBox is a tiny suite of Unix utilities in a multi-call binary.
Copyright: GPL
Packager : Erik Andersen <andersen@lineo.com>
Conflicts: fileutils grep shellutils
Buildroot: /tmp/%{Name}-%{Version}
Source: %{Name}-%{Version}.tar.gz

%Description
BusyBox combines tiny versions of many common UNIX utilities into a single
small executable. It provides minimalist replacements for most of the utilities
you usually find in fileutils, shellutils, findutils, textutils, grep, gzip,
tar, etc.  BusyBox provides a fairly complete POSIX environment for any small
or emdedded system.  The utilities in BusyBox generally have fewer options then
their full featured GNU cousins; however, the options that are provided behave
very much like their GNU counterparts.

%Prep
%setup -q -n %{Name}-%{Version}

%Build
make

%Install
rm -rf $RPM_BUILD_ROOT
make PREFIX=$RPM_BUILD_ROOT install

%Clean
rm -rf $RPM_BUILD_ROOT

%Files 
%defattr(-,root,root)
/
