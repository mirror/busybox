Name: busybox
Version: 0.29alpha
Release: 1
Group: System/Utilities
Summary: BusyBox is a tiny suite of Unix utilities in a multi-call binary.
Copyright: GPL
Packager : Erik Andersen <andersen@lineo.com>
Conflicts: fileutils grep shellutils
Buildroot: /tmp/%{Name}-%{Version}
Source: busybox-0.29a1.tar.gz

%Description
BusyBox is a suite of "tiny" Unix utilities in a multi-call binary. It
provides a pretty complete environment that fits on a floppy or in a
ROM. Just add "ash" (Keith Almquists tiny Bourne shell clone) and "ae",
and a kernel and you have a full system. This is used on the Debian
install disk and in an internet router, and it makes a good environment
for a "rescue" disk or any small or embedded system.

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
	(cd $RPM_BUILD_ROOT/bin ; ln -s ln `echo $i | sed -e 's/\(^.*\/\)\(.*\)/\2/g' ` ); 
done 
rm -f $RPM_BUILD_ROOT/bin/ln
install -m 755 busybox $RPM_BUILD_ROOT/bin/ln

%Clean
rm -rf $RPM_BUILD_ROOT

%Files 
%defattr(-,root,root)
/
