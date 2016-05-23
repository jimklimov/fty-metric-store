#
#    agent-metric-store - Persistance layer for metrics
#
#    Copyright (C) 2014 - 2015 Eaton                                        
#                                                                           
#    This program is free software; you can redistribute it and/or modify   
#    it under the terms of the GNU General Public License as published by   
#    the Free Software Foundation; either version 2 of the License, or      
#    (at your option) any later version.                                    
#                                                                           
#    This program is distributed in the hope that it will be useful,        
#    but WITHOUT ANY WARRANTY; without even the implied warranty of         
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
#    GNU General Public License for more details.                           
#                                                                           
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            
#

Name:           agent-metric-store
Version:        0.1.0
Release:        1
Summary:        persistance layer for metrics
License:        GPL-2.0+
URL:            https://eaton.com/
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkg-config
BuildRequires:  systemd-devel
BuildRequires:  gcc-c++
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  cxxtools-devel
BuildRequires:  tntdb-devel
BuildRequires:  biosproto-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
agent-metric-store persistance layer for metrics.

%package -n libagent_metric_store0
Group:          System/Libraries
Summary:        persistance layer for metrics

%description -n libagent_metric_store0
agent-metric-store persistance layer for metrics.
This package contains shared library.

%post -n libagent_metric_store0 -p /sbin/ldconfig
%postun -n libagent_metric_store0 -p /sbin/ldconfig

%files -n libagent_metric_store0
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libagent_metric_store.so.*

%package devel
Summary:        persistance layer for metrics
Group:          System/Libraries
Requires:       libagent_metric_store0 = %{version}
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       cxxtools-devel
Requires:       tntdb-devel
Requires:       biosproto-devel

%description devel
agent-metric-store persistance layer for metrics.
This package contains development files.

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libagent_metric_store.so
%{_libdir}/pkgconfig/libagent_metric_store.pc

%prep
%setup -q

%build
sh autogen.sh
%{configure} --with-systemd-units
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%{_bindir}/bios-agent-ms
%{_prefix}/lib/systemd/system/bios-agent-ms*.service
%{_prefix}/lib/systemd/system/bios-agent-ms*.timer
%{_datadir}/bios/etc/default/bios__bios-agent-ms.service.conf


%changelog
