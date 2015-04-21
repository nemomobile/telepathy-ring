Name:       telepathy-ring

%define keepstatic 1

Summary:    GSM connection manager for the Telepathy framework
Version:    2.2.1
Release:    2
Group:      System/Libraries
License:    LGPLv2.1
URL:        https://github.com/nemomobile/telepathy-ring/
Source0:    %{name}-%{version}.tar.bz2
Requires:   ofono
Requires:   telepathy-mission-control
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(check)
BuildRequires:  pkgconfig(libxslt)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(uuid)
BuildRequires:  pkgconfig(telepathy-glib) >= 0.11.7
BuildRequires:  pkgconfig(mission-control-plugins)
BuildRequires:  pkgconfig(libngf0) >= 0.24
BuildRequires:  python >= 2.5

%description
%{summary}.

%package tests
Summary:    Tests for %{name}
Group:      System/Libraries
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
%{summary}.

%prep
%setup -q -n %{name}-%{version}


%build
mkdir m4 || true

%reconfigure 
make %{?jobs:-j%jobs}


%install
rm -rf %{buildroot}
%make_install


%files
%defattr(-,root,root,-)
%{_datadir}/dbus-1/services/*
%{_datadir}/telepathy/managers/*
%{_libexecdir}/*
%{_libdir}/mission-control-plugins.0/mcp-account-manager-ring.so
%doc %{_mandir}/man8/telepathy-ring.8.gz

%files tests
%defattr(-,root,root,-)
/opt/tests/%{name}/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/*.a
%{_includedir}/*
