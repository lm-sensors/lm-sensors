##This source RPM build the foloowing binary packages:
## lm_sensors -- user space stuff
## lm_sensors-devel -- user space stuff for the application development
## lm_sensors-drivers -- kernel space drivers.

##Dependencies: lm_sensors requires lm_sensors_drivers
##              lm_sensors-devel requires lm_sensors
##              lm_sensors-drivers requires new i2c code

## lm_sensors and lm_sensors-devel can be distributed easily as binary
## packages. They will be compatible with different configurations.

## WARNING!!! lm_sensors-drivers must be compiled for the the same kernel
## that will run on the target machine. This implies the same kernel
## version and the  same kernel configuration. Thus, this binary package
## can be provided by distribution vendors only for their stock distribution
## kernels. If you use a custom configured kernel, you must rebuild this
## package. To protect the innocent, we define kversion and make the
## resulting package dependable on the specific version of the kernel.

#This spec file is good for stock kernels of RedHat based distributions.
#If you run a stock smp kernel, the package suitable for smp kernel will be
#built. If you run a stock up kernel, the package suitable for up kernel will be
#built.

## The only customizable variable is prefix.
## If i2c has been installed into a location different from prefix or /usr;
## you must define I2C_HEADERS below.
## This package IS relocatable (change prefix to relocate).

%define prefix /usr

#Define your kernel version here.
%define smptag %(uname -r| cut -f2 -d - |sed 's/[0-9]//g')
%define versiontag %(uname -r|sed 's/smp//')
%define kversion %(uname -r)
%define kname kernel%(echo %{smptag}|sed 's/smp/-smp/')

%define I2C_HEADERS %(rpm -ql kernel-i2c|grep include/linux/i2c.h|head -1|sed 's!/linux/i2c.h!!')

%define name lm_sensors
%define ver 2.5.0
Summary: Hardware Health Monitoring Tools
Name: %{name}
Version: %{ver}
Release: 1rh
Group: Applications/System
Copyright: GPL
Source0: http://www.lm-sensors.nu/lm-sensors/archive/%{name}-%{ver}.tar.gz
Buildroot: /var/tmp/%{name}
Docdir: %{prefix}/doc
Requires: %{name}-drivers >= %{ver}
Url: http://www.netroedge.com/~lm78/
##For officially distributed packages, please sign below
Packager: Constantine Gavrilov <const-g@xpert.com>
Distribution: RedHat 6.1

%package drivers
Summary: Chip and bus drivers for general SMBus access and hardware monitoring.
Group: System Environment/Kernel
Copyright: GPL
Version: %{ver}%{smptag}
Requires: kernel-i2c >= %{ver}, %{kname} = %{versiontag}

%package devel
Summary: Development environment for hardware health monitoring applications
Group: Development/Libraries
Copyright: GPL 
Requires: %{name} = %{ver}

%description
This package contains a collection of user space tools for general SMBus
access and hardware monitoring. SMBus, also known as System Management Bus,
is a protocol for communicating through a I2C ('I squared C') bus. Many modern
mainboards have a System Management Bus. There are a lot of devices which can
be connected to a SMBus; the most notable are modern memory chips with EEPROM
memories and chips for hardware monitoring.

Most modern mainboards incorporate some form of hardware monitoring chips.
These chips read things like chip temperatures, fan rotation speeds and
voltage levels. There are quite a few different chips which can be used
by mainboard builders for approximately the same results.

%description drivers
This package contains a collection of kernel modules for general SMBus
access and hardware monitoring. SMBus, also known as System Management Bus,
is a protocol for communicating through a I2C ('I squared C') bus. Many modern
mainboards have a System Management Bus. There are a lot of devices which can
be connected to a SMBus; the most notable are modern memory chips with EEPROM
memories and chips for hardware monitoring.

Most modern mainboards incorporate some form of hardware monitoring chips.
These chips read things like chip temperatures, fan rotation speeds and
voltage levels. There are quite a few different chips which can be used
by mainboard builders for approximately the same results.

Hardware monitoring chips are often connected to the SMBus, but often they
can also be connected to the ISA bus. The modules in this package usually
support both ways of accessing them.

%description devel
This package contains environment for development of user space applications
for general SMBus access and hardware monitoring. SMBus, also known as
System Management Bus, is a protocol for communicating through a I2C
('I squared C') bus. Many modern mainboards have a System Management Bus.
There are a lot of devices which can be connected to a SMBus; the most
notable are modern memory chips with EEPROM memories and chips for hardware
monitoring.

%prep
%setup

%build
#even for non-SMP systems parallel make will build faster
if [ %{smptag} = smp ]; then
 make -j4 MODVER=1 SMP=1 I2C_HEADERS=%{I2C_HEADERS}
else
 make -j4 MODVER=1 SMP=0 I2C_HEADERS=%{I2C_HEADERS}
fi

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT PREFIX=%{prefix} \
	MODDIR=/lib/modules/%{kversion}/misc
#back up stock config
cp -a $RPM_BUILD_ROOT/etc/sensors.conf $RPM_BUILD_ROOT/etc/sensors.ex

%post
ldconfig || /bin/true
echo "please run \`%{prefix}/sbin/sensors-detect' to configure the sensors."

%postun
ldconfig || /bin/true

%post drivers
depmod -a || /bin/true

%postun drivers
depmod -a || /bin/true

%files
%config /etc/sensors.conf
%config /etc/sensors.ex
%{prefix}/bin/*
%{prefix}/lib/lib*.so.*
%dir %{prefix}/man/man1
%dir %{prefix}/man/man5
%{prefix}/man/man1/*
%{prefix}/man/man5/*
%{prefix}/sbin/*
%doc BACKGROUND BUGS CHANGES CONTRIBUTORS INSTALL README TODO
%doc doc/{FAQ,cvs,fan-divisors,modules,progs,temperature-sensors}
%doc doc/{useful_addresses.html,version-2}

%files drivers
%dir /lib/modules/%{kversion}
%dir /lib/modules/%{kversion}/misc
/lib/modules/%{kversion}/misc/*
%{prefix}/include/linux/*
%doc doc/busses doc/chips doc/developers doc/kernel

%files devel
%dir %{prefix}/include/sensors
%{prefix}/include/sensors/*.h
%{prefix}/lib/*.a
%{prefix}/lib/*.so
%dir %{prefix}/man/man3
%{prefix}/man/man3/*

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf $RPM_BUILD_DIR/%{name}-%{ver}
