%define name	busybox
%define epoch   0
%define version	0.61.pre
%define release	%(date -I | sed -e 's/-/_/g')
%define serial  1

Name:	 %{name}
#Epoch:   %{epoch}
Version: %{version}
Release: %{release}
Serial:	 %{serial}
Copyright: GPL
Group: System/Utilities
Summary: BusyBox is a tiny suite of Unix utilities in a multi-call binary.
URL:	 http://busybox.net/
Source:	 ftp://busybox.net/busybox/%{name}-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}
Packager : Erik Andersen <andersen@codepoet.org>

%Description
BusyBox combines tiny versions of many common UNIX utilities into a single
small executable. It provides minimalist replacements for most of the utilities
you usually find in fileutils, shellutils, findutils, textutils, grep, gzip,
tar, etc.  BusyBox provides a fairly complete POSIX environment for any small
or emdedded system.  The utilities in BusyBox generally have fewer options then
their full featured GNU cousins; however, the options that are provided behave
very much like their GNU counterparts.

%Prep
%setup -q -n %{name}-%{version}

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
