Name: busybox
Version: 0.33
Release: 1
Group: System/Utilities
Summary: BusyBox is a tiny suite of Unix utilities in a multi-call binary.
Copyright: GPL
Packager : Erik Andersen <andersen@lineo.com>
Conflicts: fileutils grep shellutils
Buildroot: /tmp/%{Name}-%{Version}
Source: busybox-%{Version}.tar.gz

%Description
BusyBox is a suite of "tiny" Unix utilities in a multi-call binary. It
provides a pretty complete POSIX environment in a very small package.
Just add a kernel, "ash" (Keith Almquists tiny Bourne shell clone), and
an editor such as "elvis-tiny" or "ae", and you have a full system. This
is makes an excellent environment for a "rescue" disk or any small or
embedded system.

%Prep
%setup -q -n busybox

%Build
make

%Install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/bin
h=`cat busybox.links`

for i in $h ; do
	mkdir -p $RPM_BUILD_ROOT/`echo $i | sed -e 's/\(^.*\/\)\(.*\)/\1/g' `
	(cd $RPM_BUILD_ROOT/bin ; ln -s busybox `echo $i | sed -e 's/\(^.*\/\)\(.*\)/\2/g' ` ); 
done 
rm -f $RPM_BUILD_ROOT/bin/busybox
install -m 755 busybox $RPM_BUILD_ROOT/bin/busybox

%Clean
rm -rf $RPM_BUILD_ROOT

%Files 
%defattr(-,root,root)
/
