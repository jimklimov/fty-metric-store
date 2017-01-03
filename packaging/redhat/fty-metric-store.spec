#
#    fty-metric-store - Persistance for metrics
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

# To build with draft APIs, use "--with drafts" in rpmbuild for local builds or add
#   Macros:
#   %_with_drafts 1
# at the BOTTOM of the OBS prjconf
%bcond_with drafts
%if %{with drafts}
%define DRAFTS yes
%else
%define DRAFTS no
%endif
Name:           fty-metric-store
Version:        1.0.0
Release:        1
Summary:        persistance for metrics
License:        GPL-2.0+
URL:            http://example.com/
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
# Note: ghostscript is required by graphviz which is required by
#       asciidoc. On Fedora 24 the ghostscript dependencies cannot
#       be resolved automatically. Thus add working dependency here!
BuildRequires:  ghostscript
BuildRequires:  asciidoc
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkgconfig
BuildRequires:  systemd-devel
BuildRequires:  systemd
%{?systemd_requires}
BuildRequires:  xmlto
BuildRequires:  gcc-c++
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  tntdb-devel
BuildRequires:  cxxtools-devel
BuildRequires:  fty-proto-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
fty-metric-store persistance for metrics.

%package -n libfty_metric_store1
Group:          System/Libraries
Summary:        persistance for metrics shared library

%description -n libfty_metric_store1
This package contains shared library for fty-metric-store: persistance for metrics

%post -n libfty_metric_store1 -p /sbin/ldconfig
%postun -n libfty_metric_store1 -p /sbin/ldconfig

%files -n libfty_metric_store1
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libfty_metric_store.so.*

%package devel
Summary:        persistance for metrics
Group:          System/Libraries
Requires:       libfty_metric_store1 = %{version}
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       tntdb-devel
Requires:       cxxtools-devel
Requires:       fty-proto-devel

%description devel
persistance for metrics development tools
This package contains development files for fty-metric-store: persistance for metrics

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libfty_metric_store.so
%{_libdir}/pkgconfig/libfty_metric_store.pc
%{_mandir}/man3/*

%prep
%setup -q

%build
sh autogen.sh
%{configure} --enable-drafts=%{DRAFTS} --with-systemd-units
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%doc COPYING
%{_bindir}/fty-metric-store
%{_mandir}/man1/fty-metric-store*
%{_bindir}/fty-metric-store-cleaner
%config(noreplace) %{_sysconfdir}/fty-metric-store/fty-metric-store.cfg
/usr/lib/systemd/system/fty-metric-store.service
%dir %{_sysconfdir}/fty-metric-store
%if 0%{?suse_version} > 1315
%post
%systemd_post fty-metric-store.service
%preun
%systemd_preun fty-metric-store.service
%postun
%systemd_postun_with_restart fty-metric-store.service
%endif

%changelog
