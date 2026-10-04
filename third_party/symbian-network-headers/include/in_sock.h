// Copyright (c) 2003-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// IPv6/IPv4 socket library public header 
// 
//

/**
 @file in_sock.h
 @publishedAll
 @released
*/

#ifndef __IN_SOCK_H__
#define __IN_SOCK_H__

#ifndef __ES_SOCK_H__
#include <es_sock.h>
#endif

#ifndef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <in_sock_internal.h>
#endif

/**
* @name TCP/IP Protocol and address family
*
* The TCP/IP stack supports two different address formats
* in the TInetAddr: KAfInet for plain IPv4 addresses (for
* backward compatibility), and KAfInet6 for both IPv6 and
* IPv4 addresses.
*
* Only the KAfInet is used as a <em>protocol family</em> constant
* for the TCP/IP protocol family, when sockets are opened
* (RSocket::Open() and RHostResolver::Open() ).
*
* KAfInet6 is only <em>address family</em>, and can only appear
* as a family constant in TSockAddr class.
*
* @since 7.0
*/
//@{
/** Identifies the TCP/IP protocol family and v4 address family.
*
* @see TInetAddr
*/
const TUint KAfInet				= 0x0800;
/** Identifies the TCP/IP v6 address family.
*
* @see TInetAddr
* @since 7.0
*/
const TUint KAfInet6			= 0x0806;
//@}

/**
* @name IP protocol IDs 
* @ingroup ip_packet_formats
*/
//@{
/** 
* Identifies the ICMP protocol.
*
* @capability NetworkServices	Required for opening 'icmp' sockets.
* @ref RSocket::Open()
*/
const TUint KProtocolInetIcmp	= 1;

/**
* Identifies the TCP protocol.
*
* @capability NetworkServices	Required for opening 'tcp' sockets.
* @ref RSocket::Open()
*/
const TUint KProtocolInetTcp	= 6;

/**
* Identifies the UDP protocol.
*
* @capability NetworkServices	Required for opening 'udp' sockets.
* @ref RSocket::Open()
*/
const TUint KProtocolInetUdp	= 17;

/** IPv6 inside IP (v4 or v6). @since 7.0 */
const TUint KProtocolInet6Ipip	= 41;

/** Identifies the ICMPv6 protocol.
*
* @since 7.0
* @capability NetworkServices	Required for opening 'icmp6' sockets.
* @ref RSocket::Open()
*/
const TUint KProtocolInet6Icmp	= 58;
//@}

/**
* @name Internal Protocol IDs
* Internal protocol id's do not appear in real
* packets. An internal id only identifies a protocol
* instance.
* @{
*/
/**
* Identifies the IP (v4) protocol module.
*
* @capability NetworkControl		Required for opening 'ip' sockets.
* @ref RSocket::Open()
*/
const TUint KProtocolInetIp		= 0x100;

/**
* Identifies the IP (v6) protocol module.
*
* @since 7.0
* @capability NetworkControl		Required for opening 'ip6' sockets.
* @ref RSocket::Open()
*/
const TUint KProtocolInet6Ip	= 0xF00;

/** Unknown Protocol ID. @deprecated (never use in anything that binds to IP) */
const TUint KProtocolUnknown = 0xdead;

//@}

/**  @name Socket option/ioctl levels */
//@{
/** ICMP socket option level (no options). */
const TUint KSolInetIcmp		= 0x101;
/** TCP socket options level. */
const TUint KSolInetTcp			= 0x106;
/** UDP socket options  level. */
const TUint KSolInetUdp			= 0x111;
/** IP socket options  level. */
const TUint KSolInetIp			= 0x100;
/** Interface control socket options level. */
const TUint KSolInetIfCtrl		= 0x201;
/** Route control socket options level.  */
const TUint KSolInetRtCtrl		= 0x202;
/** DNS control socket options level. @removed */
const TUint KSolInetDnsCtrl		= 0x204;
/** Interface query socket options level. @since 7.0 */
const TUint KSolInetIfQuery		= 0x206;
#ifdef SYMBIAN_DNS_PUNYCODE
/** DNS set options level. */
/**using a new constant instead of KSolInetDnsCtrl
 */
const TUint KSolInetDns			= 0x208;
#endif //SYMBIAN_DNS_PUNYCODE
//@}

/** Maximum IPv4 address length (bits). */
const TInt KInetAddrMaxBits         = 32;
/** Maximum IPv6 address length (bits). */
const TInt KInet6AddrMaxBits        = 128;

/** @name Port constants */
//@{
/** Any port flag (0). */
const TUint KInetPortAny			= 0x0000; 
/** No port flag (0). */
const TUint KInetPortNone			= 0x0000;
/** Minimum value of an automatically allocated port. */
const TUint KInetMinAutoPort		= 32768;
/** Maximum value of an automatically allocated port. */
const TUint KInetMaxAutoPort		= 60999;
//@}

/**  @name IPv4 address constants and definitions */
//@{

/** 
* Forms a 32-bit integer IPv4 address from the normal dotted-decimal representation. 
* 
* The four arguments are the four parts of the IPv4 address.
* 
* Example:
* @code
* TInetAddr addr;
* const TUint32 KInetAddr = INET_ADDR(194,129,2,54);
* addr.SetAddress(KInetAddr);
* @endcode 
*/
#define INET_ADDR(a,b,c,d) (TUint32)((((TUint32)(a))<<24)|((b)<<16)|((c)<<8)|(d))

/** Any address flag (0.0.0.0). */
const TUint32 KInetAddrAny				= INET_ADDR(0,0,0,0);
/** No address flag (0.0.0.0). */
const TUint32 KInetAddrNone				= INET_ADDR(0,0,0,0);
/** All addresses mask (255.255.255.255). */
const TUint32 KInetAddrAll				= INET_ADDR(255,255,255,255);
/** Broadcast address (255.255.255.255). */
const TUint32 KInetAddrBroadcast		= INET_ADDR(255,255,255,255);
/** Loopback address (127.0.0.1). */
const TUint32 KInetAddrLoop				= INET_ADDR(127,0,0,1);

/** Group address range start. */
const TUint32 KInetAddrGroupUnspec		= INET_ADDR(224,0,0,0);
/** All hosts address (224.0.0.1). */
const TUint32 KInetAddrGroupAllHosts	= INET_ADDR(224,0,0,1);
/** Link-local net number. @since 7.0s */
const TUint32 KInetAddrLinkLocalNet		= INET_ADDR(169,254,0,0);
/** Link-local net mask.  @since 7.0s */
const TUint32 KInetAddrLinkLocalNetMask	= INET_ADDR(255,255,0,0);

/** All addresses mask (0.0.0.0). */
const TUint32 KInetAddrMaskAll			= INET_ADDR(0,0,0,0);
/** All bits mask (255.255.255.255). */
const TUint32 KInetAddrMaskHost			= INET_ADDR(255,255,255,255);

/** Class A net mask (255.0.0.0). */
const TUint32 KInetAddrNetMaskA			= INET_ADDR(255,0,0,0);
/** Class A host mask (0.255.255.255). */
const TUint32 KInetAddrHostMaskA		= ~KInetAddrNetMaskA;
/** Number of bits to right-shift a Class A address to obtain the network number. */
const TInt KInetAddrShiftA				= 24;
/** Class B net mask (255.255.0.0). */
const TUint32 KInetAddrNetMaskB 		= INET_ADDR(255,255,0,0);
/** Class B host mask (0.0.255.255). */
const TUint32 KInetAddrHostMaskB		= ~KInetAddrNetMaskB;
/** Number of bits to right-shift a Class B address to obtain the network number. */
const TInt KInetAddrShiftB				= 16;
/** Class C net mask (255.255.255.0). */
const TUint32 KInetAddrNetMaskC 		= INET_ADDR(255,255,255,0);
/** Class C host mask (0.0.0.255). */
const TUint32 KInetAddrHostMaskC		= ~KInetAddrNetMaskC;
/** Number of bits to right-shift a Class C address to obtain the network number. */
const TInt KInetAddrShiftC				= 8;

/** . */
const TUint32 KInetAddrIdMaskA			= 0x80000000;
/** . */
const TUint32 KInetAddrIdValA			= 0x00000000;
/** . */
const TUint32 KInetAddrIdMaskB			= 0xc0000000;
/** . */
const TUint32 KInetAddrIdValB			= 0x80000000;
/** . */
const TUint32 KInetAddrIdMaskC			= 0xe0000000;
/** . */
const TUint32 KInetAddrIdValC			= 0xc0000000;
/** . */
const TUint32 KInetAddrIdMaskD			= 0xf0000000;
/** . */
const TUint32 KInetAddrIdValD			= 0xe0000000;
/** . */
const TUint32 KInetAddrIdMaskE			= 0xf8000000;
/** . */
const TUint32 KInetAddrIdValE			= 0xf0000000;

enum TInetAddrClass
/**
* @publishedAll
* @released
*/
	{
	EInetClassUnknown = 0,
	EInetClassA,
	EInetClassB,
	EInetClassC,
	EInetClassD,
	EInetClassE,
	EInetMulticast = EInetClassD,
	EInetExperimental = EInetClassE
	};
//@}

struct SInetAddr
/**
* IPv4 socket address.
*
* This exists for backward compatibility. SInet6Addr is
* the preferred format for both IPv4 and IPv6 addresses
* in TInetAddr.
*
* @publishedAll
* @released
*/
	{
	/** Plain IPv4 address */
	TUint32 iAddr;
	};


class TIp6Addr
/**
* The 128 bits of IPv6 or IPv4 address stored in network byte order.
*
* IPv4 addresses are stored in IPv4 mapped format.
*
* @publishedAll
* @released
* @since 7.0
*/
	{
public:
	IMPORT_C TBool IsUnicast() const;
	IMPORT_C TBool IsMulticast() const;
	IMPORT_C TBool IsLoopback() const;
	IMPORT_C TBool IsUnspecified() const;
	IMPORT_C TBool IsLinkLocal() const;
	IMPORT_C TBool IsSiteLocal() const;
	IMPORT_C TBool IsV4Compat() const;
	IMPORT_C TBool IsV4Mapped() const;
	IMPORT_C TBool IsEqual(const TIp6Addr &aAddr) const;
	IMPORT_C TInt Match(const TIp6Addr &aAddr) const;
	IMPORT_C TInt Scope() const;
	union
		{
		TUint8  iAddr8[16];
		TUint16 iAddr16[8];
		TUint32 iAddr32[4];
		} u;
	};

struct SInet6Addr
/**
* IPv4 and IPv6 socket address.
*
* Defines the address information inside the TInetAddr.
*
* @publishedAll
* @released
*
* @sa SInetAddr
* @since 7.0
*/
	{
	/** 16 bytes of IP6/IP4 address (128 bits) */
	TIp6Addr iAddr;
	/** 4 bytes of Flow Id */
	TUint32 iFlow;
	/**  4 bytes of Scope Id. */
	TUint32 iScope;
	};

/**
* @name IPv6 address constants..
* @since 7.0
*/
//@{
/** Node-local scope level (RFC-2373 2.7). */
const TInt KIp6AddrScopeNodeLocal = 0x01;
/** Link-local scope level (RFC-2373 2.7). */
const TInt KIp6AddrScopeLinkLocal = 0x02;
/** Site-local scope level (RFC-2373 2.7). */
const TInt KIp6AddrScopeSiteLocal = 0x05;
/** Organisation-local scope level (RFC-2373 2.7). */
const TInt KIp6AddrScopeOrganization = 0x08;
/** Global scope level (RFC-2373 2.7). */
const TInt KIp6AddrScopeGlobal = 0x0E;
/** Network scope level (non-standard value, used internally) */
const TInt KIp6AddrScopeNetwork = 0x10;

/** No address (all 0s). */
const TIp6Addr KInet6AddrNone = {{{0}}};
/** Loopback address (::1). */
const TIp6Addr KInet6AddrLoop = {{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}}};
/** Link-local address (prefix fe80::). */
const TIp6Addr KInet6AddrLinkLocal = {{{0xfe, 0x80, }}};
//@}

class TInetAddr : public TSockAddr
/**
* This class specialises the generic socket server address class
* TSockAddr for the TCP/IP protocol family. It represents an IP
* address and stores either an IPv4 or an IPv6 address in its buffer
* after the generic data defined by TSockAddr. The protocol family
* field provided by the TSockAddr base class can be set to KAfInet,
* KAfInet6 or KAFUnspec.
*
* The address family defines the format of the stored address:
*
* @li #KAfInet
*	is for plain 32 bits IPv4 address presented as SInetAddr
*	structure.
* @li #KAfInet6
*	is for both IPv4 and IPv6 addresses (IPv4 addresses
*	are in IPv4 mapped format). The content is presented as
*	SInet6Addr structure, which includes the scope id
*	and flow label, in addition to the 128 address bits.
* @li KAFUnspec
*	does not contain any addresses and works in most
*	contexts as unspecified address, This is better than
*	placing explicit IPv4 "0.0.0.0" or IPv6 "::", which
*	in certain situations may limit the connections to either
*	IPv4 or IPv6, but not both.
*
* The flow label and scope id fields exist only in KAfInet6
* format. However, the access functions TInetAddr::FlowLabel
* and TInetAddr::Scope will also work for other address formats
* by always returning 0.
*
* Any function which sets or changes the address bits, will
* always reset the scope id and flow label to 0 (the
* TInetAddr::Init is always called internally). These are
* reasonable defaults, and normal application user does not
* normally need to worry about flow label or scope id.
*
* When address is returned from the stack, it will often
* be in KAfInet6 format, and may contain non-zero values
* for flow label and scope id fields. When copying addresses,
* the full TInetAddr (or at least SInet6Addr) should be
* copied to preserve these fields.
*
* @publishedAll
* @released
* @since 7.0
*	The IPv4 only portion is backward compatible with
*	older versions. In 7.0s some functions have additional
*	features.
*/
	{
public:
	IMPORT_C TInetAddr();
	IMPORT_C TInetAddr(const TSockAddr& aAddr);
	IMPORT_C TInetAddr(TUint aPort);
	IMPORT_C TInetAddr(const TIp6Addr &aAddr, TUint aPort);
	IMPORT_C TInetAddr(TUint32 aAddr, TUint aPort);
	IMPORT_C void SetAddress(TUint32 aAddr);
	IMPORT_C void SetAddress(const TIp6Addr &aAddr);
	IMPORT_C void SetV4CompatAddress(TUint32 aAddr);
	IMPORT_C void SetV4MappedAddress(TUint32 aAddr);
	IMPORT_C void SetFlowLabel(TInt aLabel);
	IMPORT_C TInt FlowLabel() const;
	IMPORT_C const TIp6Addr &Ip6Address() const;
	IMPORT_C void ConvertToV4Compat();
	IMPORT_C void ConvertToV4Mapped();
	IMPORT_C void ConvertToV4();
	IMPORT_C TBool CmpAddr(const TInetAddr& aAddr) const;
	IMPORT_C TBool Match(const TInetAddr& aHost) const;
	IMPORT_C TBool Match(const TInetAddr& aNet, const TInetAddr& aMask) const;
	IMPORT_C TBool Match(const TInetAddr& aNet, TInt aPrefixLen) const;
	IMPORT_C void PrefixMask(TInt aPrefixLen);
	IMPORT_C void Prefix(const TInetAddr& aAddr, TInt aPrefixLen);
	IMPORT_C void Output(TDes &aBuf) const;
	IMPORT_C TInt Input(const TDesC &aBuf);
	inline static TInetAddr& Cast(const TSockAddr& aAddr);
	inline static TInetAddr& Cast(const TSockAddr* aAddr);
	
	IMPORT_C TBool IsUnicast() const;
	IMPORT_C TBool IsMulticast() const;
	IMPORT_C TBool IsLoopback() const;
	IMPORT_C TBool IsUnspecified() const;
	IMPORT_C TBool IsLinkLocal() const;
	IMPORT_C TBool IsSiteLocal() const;
	IMPORT_C TBool IsV4Compat() const;
	IMPORT_C TBool IsV4Mapped() const;

	IMPORT_C TUint32 Address() const;
	IMPORT_C void NetMask(const TInetAddr& aAddr);
	IMPORT_C void Net(const TInetAddr& aAddr);
	IMPORT_C void NetBroadcast(const TInetAddr& aAddr);
	IMPORT_C void SubNet(const TInetAddr& aAddr, const TInetAddr& aMask);
	IMPORT_C void SubNetBroadcast(const TInetAddr& aAddr, const TInetAddr& aMask);

	inline TBool IsClassA() const;
	inline TBool IsClassB() const;
	inline TBool IsClassC() const;
	inline TBool IsBroadcast() const;
	inline TBool IsWildAddr() const;
	inline TBool IsWildPort() const;

	IMPORT_C void SetScope(TUint32 aScope);
	IMPORT_C TUint32 Scope() const;

	IMPORT_C void Init(TUint aFamily);
	IMPORT_C void OutputWithScope(TDes &aBuf) const;
protected:
	inline SInetAddr *Addr4Ptr() const;
	inline SInet6Addr *AddrPtr() const;
	inline static TInt AddrLen();
private:
	TInt Ipv4Input(const TDesC& aDes);
	TInt Ipv6Input(const TDesC& aDes);
	};

inline SInet6Addr* TInetAddr::AddrPtr() const
	/** Returns a pointer to #KAfInet6 content format. */
	{ return (SInet6Addr*)UserPtr(); }

inline TInt TInetAddr::AddrLen()
	/** Returns the size of the #KAfInet6 content format. */
	{ return sizeof(SInet6Addr); }

inline TInetAddr& TInetAddr::Cast(const TSockAddr& aAddr)
	/**
	* Casts a TSockAddr to a TInetAddr reference.
	*
	* The cast is only safe if the object being referenced is actually aTInetAddr. 
	*
	* @param aAddr  TSockAddr to cast
	* @return Casted reference to a TInetAddr. 
	*/
	{ return *((TInetAddr*)&aAddr); }

inline TInetAddr& TInetAddr::Cast(const TSockAddr* aAddr)
	/**
	* Casts a TSockAddr to a TInetAddr reference.
	*
	* The cast is only safe if the object being referenced is actually aTInetAddr. 
	*
	* @param aAddr  TSockAddr to cast
	* @return Casted pointer to a TInetAddr. 
	*/
	{ return *((TInetAddr*)aAddr); }

inline SInetAddr* TInetAddr::Addr4Ptr() const
	/** Returns a pointer to #KAfInet content format  */
	{ return (SInetAddr*)UserPtr(); }

inline TBool TInetAddr::IsBroadcast() const
	/**
	* Tests if the IP address is a limited broadcast address (255.255.255.255).
	*
	* @return ETrue if the IPv4 address value is a limited broadcast address; otherwise, EFalse
	*/
	{ return Address() == KInetAddrBroadcast; }

inline TBool TInetAddr::IsWildPort() const
	/**
	* Tests if the port is zero.
	*
	* @return ETrue if the port is zero; otherwise, EFalse.
	*/
	{ return Port() == KInetPortNone; }

inline TBool TInetAddr::IsWildAddr() const
	/**
	* Tests if the IP address is unspecified.
	*
	* This is same as IsUnspecified()
	*
	* @return ETrue if the IP address value is unspecified; otherwise, EFalse. 
	*/
	{ return IsUnspecified(); }

inline TBool TInetAddr::IsClassA() const
	/**
	* Tests if the IP address is Class A.
	*
	* @return ETrue if the IPv4 address value is Class A; otherwise, EFalse
	*
	* @deprecated Works only for IPv4. It assumes the old IPv4 address classes
	* (A, B, C). Applications using this function may not work properly in the
	* current internet environment.
	*/
	{
		return (Family() == KAfInet || IsV4Mapped() || IsV4Compat()) && (Address() & KInetAddrIdMaskA) == KInetAddrIdValA;
	}

inline TBool TInetAddr::IsClassB() const
	/**
	* Tests if the IP address is Class B.
	*
	* @return ETrue if the IPv4 address value is Class B; otherwise. EFalse
	*
	* @deprecated Works only for IPv4. It assumes the old IPv4 address classes
	* (A, B, C). Applications using this function may not work properly in the
	* current internet environment.
	*/
	{ return (Address() & KInetAddrIdMaskB) == KInetAddrIdValB; }

inline TBool TInetAddr::IsClassC() const
	/**
	* Tests if the IP address is Class C.
	*
	* @return ETrue if the IPv4 address value is Class C; otherwise, EFalse
	* @deprecated Works only for IPv4. It assumes the old IPv4 address classes
	* (A, B, C). Applications using this function may not work properly in the
	* current internet environment.
	*/
	{ return (Address() & KInetAddrIdMaskC) == KInetAddrIdValC; }

/**
* @name Send/Recv flags (datagram sockets only)
*/
//@{
/** Don't fragment the packet.
*
* If the packet would require fragmentation due to a small
* maximum transmission unit size (MTU), the packet is dropped
* and an ICMP error message is generated: for ICMPv4 type=3 and code=4
* (#KInet4ICMP_Unreachable); and for ICMPv6 type=2 and code=0
* (#KInet6ICMP_PacketTooBig).
*
* Application must enable #KSoInetLastError to detect this situation.
*/
const TUint KIpDontFragment		= 0x010000;
/**
* Packet includes IP or IPv6 header.
*
* When reading, the returned buffer starts with the received
* IP header, which can be either IPv6 (TInet6HeaderIP4) or
* IPv4 (TInet6HeaderIP).
*
* When writing buffers, the buffer must start with the IP
* header and it must be complete. The stack does not
* provide any defaults.
*
* @sa	KSoRawMode, KSoHeaderIncluded
*/
const TUint KIpHeaderIncluded	= 0x020000;
/**
* Don't route the packet.
*
* Not supported in Symbian 7.0 and 7.0s.
*/
const TUint KIpDontRoute		= 0x040000;
//@}


/**
* @name Interface control socket options
*
* Level: #KSolInetIfCtrl
*
* Enumerating & Configuring Interfaces using TSoInetInterfaceInfo and TSoInet6InterfaceInfo.
*/ 
//@{
/**
* Begin enumeration of network interfaces. 
* 
* This option should be set before enumerating interfaces with #KSoInetNextInterface. 
* 
* This option is for use with RSocket::SetOpt() only.
*/
const TInt KSoInetEnumInterfaces = 0x211;
/**
* Return details of the next interface in an enumeration started by setting the 
* option #KSoInetEnumInterfaces.
*
* This option is for use with RSocket::GetOpt() only.
* 
* Option data type is TSoInetInterfaceInfo.
*
* @note
*	If the interface has multiple addresses, then each address
*	is returned as a separate instance of TSoInetInterfaceInfo
*	(only address information is different each time).
* @note
*	If the interface has no addresses, then one entry
*	with unspecified address is returned.
*/
const TInt KSoInetNextInterface = 0x212;

/**
* Configures the interface.
* 
* This option is for use with RSocket::SetOpt() only.
*
* Option data type is TSoInet6InterfaceInfo.
* 
* The interface is specified by setting the TSoInetInterfaceInfo::iName,
* @since 7.0
*
* @capability NetworkControl Modifying interfaces is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TInt KSoInetConfigInterface = 0x213;
/**
* Deletes the interface.
* 
* This option is for use with RSocket::SetOpt() only.
*
* Option data type is TSoInet6InterfaceInfo.
* 
* The interface is specified by setting the TSoInetInterfaceInfo::iName,
* @since 7.0
*
* @capability NetworkControl Modifying interfaces is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TUint KSoInetDeleteInterface = 0x214;
/**
* Configure the interface details, if it exists.
* 
* Option data type is TSoInet6InterfaceInfo.
* 
* The interface is specified by setting the TSoInetInterfaceInfo::iName,
*
* @note
*	Unlike KSoInetConfigInterface, never creates a new interface
*	entry, if one does not already exist. KSoInetConfigInterface
*	never fails with interface not found, as it always finds or
*	creates one.
* @since 7.0
*
* @capability NetworkControl Modifying interfaces is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TUint KSoInetChangeInterface = 0x215;
/**
* Resets interface to initial state.
*
* Delete all configuration (routes and addresses) from the
* interface. Any sockets (flows) currently using this interface,
* are set to holding state (#KSoNoInterfaceError is not required).
*
* The interface reconfigures, if the NIF driver
* calls CProtocolBase::StartSending(), or if
* #KSoInetStartInterface socket option is used.
*
* Option data type is TSoInet6InterfaceInfo.
* 
* The interface is specified by setting the TSoInetInterfaceInfo::iName,
* No other fields are used.
* @since 7.0s
*
* @capability NetworkControl Modifying interfaces is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TUint KSoInetResetInterface = 0x216;
/**
* Restart interface, auto-reconfigure.
*
* Option data type is TSoInet6InterfaceInfo.
* 
* The interface is specified by setting the TSoInetInterfaceInfo::iName,
* No other fields are used. The selected interface is autoconfigured using the
* information supplied by the attached network driver.
*
* Should normally only be called after #KSoInetResetInterface.
* @since 7.0
*
* @capability NetworkControl Modifying interfaces is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TUint KSoInetStartInterface = 0x217;

/**
* Trigger link local creation.
*
* Option data type is TSoInet6InterfaceInfo.
* 
* The interface is specified by setting the TSoInetInterfaceInfo::iName and any state
* change required. Called by a configuration deamon to trigger IPv4 zeroconf and link
* local creation if no address server is found.  Does nothing if
* EV4LLConfigDaemonControlled option is not specified for interface.
*
* @since 9.2
*
* @capability NetworkControl Modifying interfaces is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TUint KSoInetCreateIPv4LLOnInterface = 0x226;

/**
* Describes the state of an interface. 
* 
* It is used as a data member of TSoInetInterfaceInfo.
*
* @note
*	This enumeration is supported only because of backward
*	compatibility. The real interface state uses the system
*	error codes directly. The interface is either up
*	(KErrNone) or down because of some error condition (state
*	is one of the system wide error codes indicating the reason
*	for the down state).
*
* @publishedAll
* @released
*/
enum TIfStatus
	{
	/** The interface has been initiated, but is not yet available. */
	EIfPending,
	/** The interface is up and available. */
	EIfUp,
	/** The interface is up, but flowed off. */
	EIfBusy,
	/** The interface is down. */
	EIfDown
#ifdef SYMBIAN_TCPIPDHCP_UPDATE
	,
    /** The interface is not-configured */
    EIfNotConfigured
#endif //SYMBIAN_TCPIPDHCP_UPDATE
	};

class TSoInetInterfaceInfo
/**
* Used when listing interfaces with socket option.
*
* Used with interface level #KSolInetIfCtrl option #KSoInetNextInterface.
*
* This is also a base class for the TSoInet6InterfaceInfo,
* which is used in modifying the interface configuration.
*
* @publishedAll
* @released
*/
	{
public:
	/** Ignored since 7.0. @removed The field exists, but it is ignored. */
	TName iTag;
	/** Interface name */
	TName iName;
	/** Interface state. */
	TIfStatus iState;
	/** Maximum transmission unit (bytes) */
	TInt iMtu;
	/** An approximation of the interface speed in Kbps. */
	TInt iSpeedMetric;
	/**
	* Feature flags. 
	* 
	* Possible values are defined in in_iface.h.
	*/
	TUint iFeatures;
	/** Hardware address. */
	TSockAddr iHwAddr;
	/** Interface IP address. */
	TInetAddr iAddress;
	/** IP netmask. */
	TInetAddr iNetMask;
	/** IP broadcast address. */
	TInetAddr iBrdAddr;
	/** IP default gateway or peer address (if known). */
	TInetAddr iDefGate;
	/** IP primary name server (if any). */
	TInetAddr iNameSer1;
	/** IP secondary name server (if any). */
	TInetAddr iNameSer2;
	};

class TSoInet6InterfaceInfo : public TSoInetInterfaceInfo
/**
* Extension for TSoInetInterfaceInfo. Available in Symbian OS v7.0 and later.
*
* Used with the following interface level #KSolInetIfCtrl options:
* @li	#KSoInetConfigInterface
* @li	#KSoInetDeleteInterface
* @li	#KSoInetChangeInterface
* @li	#KSoInetResetInterface
* @li	#KSoInetStartInterface
* @li	#KSoInetCreateIPv4LLOnInterface
*
* The following configuration changes are only activated with
* #KSoInetConfigInterface and #KSoInetChangeInterface options.
* For these two, the extension specifies the details of actions
* to be performed. The extension is a collection of control bits,
* which can be grouped as
*
* @li modifiers (#iDelete and #iAlias)
* @li actions: #iDoState, #iDoId (with subactions #iDoAnycast or #iDoProxy) and #iDoPrefix.
*
* The effect of the modifiers depend on the chosen action (in some
* actions modifiers are ignored).
* The iDoState can be combined with any other actions, but for the
* remaining only the following combinations are valid:
*
* @li
*	no address configuration: iDoId=0, iDoPrefix=0 (iDoProxy,
*	iDoAnycast, iDelete and iAlias are ignored).
* @li
*	configure single IPv6 or IPv4 address:
*	iDoid, iAddress has the address, iNetMask unspecified.
* @li
*	configure IPv6 id part: iDoId, iAddress is IPv6 address
*	(<tt>fe80::id</tt>) and iNetMask defined (for 64 bits, use
*	<tt>ffff:ffff:ffff:ffff::</tt>).
* @li
*	configure IPv4 address and netmask:	iDoId, iNetMask defined.
* @li
*	configure IPv6 or IPv4 anycast address: iDoId, iDoAnycast,
*	iAddress has the address (iNetMask ignored).
* @li
*	configure IPv6 or IPv4 proxy address: iDoId, iDoProxy,
*	iAddress has the address (iNetMask ignored).
* @li
*	configure IPv6 prefix and id part: iDoId, iDoPrefix,
*	iAddress is the IPv6 address (prefix::id) and iNetMask defined
*	(for 64 bits, use <tt>ffff:ffff:ffff:ffff::</tt>).
* @li
*	configure IPv6 prefix: iDoPrefix, iAddress is the prefix (prefix::)
*	and iNetMask defined (for 64 bits, use <tt>ffff:ffff:ffff:ffff::</tt>).
*
* The default route is processed if #iDefGate is specified.
* If the gateway address is an IPv4 address, then it defines IPv4
* default route. Additionally, if the iDefGate is same as iAddress, then
* this is interpreted as a request to treat the default route as
* "onlink route" instead of the normal <em>gateway route</em>. #iDelete modifier
* controls whether default route is added or deleted.
*
* The MTU is updated, if #iMtu has non-zero value.
*
* Available in Symbian OS v9.2 and later.
*
* Used with the following interface level #KSolInetIfCtrl option:
* @li	#KSoInetCreateIPv4LLOnInterface
*
* @li actions: #iDoState.
*
* This configuration acts as a notification from a config daemon to the IP stack
* which controls link local behaviour if the llv4linklocal=ELLV4ConfigDeamonControlled
* TCPIP.ini option is specified for the interface:
*
* @li
*	notification from config daemon (e.g., DHCP) that address assignment terminated
*   so a link local should be created if appropriate TCPIP.ini setting is used:
*	iDoState.
*
* @publishedAll
* @released
* @since 7.0 (some functionality only in 7.0s and >=9.3 for config daemon controlled link local creation)
*/
	{
public:
	/**
	* Add or delete modifier.
	* 
	* 0 = add, 1 = delete
	*
	* Modifies the actions for address configuration (#iDoId, #iDoPrefix)
	* and iDefGate processing (see detail descripton above).
	*/
	TUint iDelete:1;
	/**
	* Primary or alias modifier.
	* 
	* 0 = primary, 1 = alias.
	*
	* @note
	*	Always use 1 here (this is a relic, that most likely
	*	should be deprecated, and defaulted to 1 always).
	*/
	TUint iAlias:1;
	/**
	* Prefix action (only for IPv6 addresses).
	* 
	* 0 = don't do prefix, 1 = do the prefix.
	*
	* #iAddress must be specified.
	*
	* If set and iNetMask is defined, then #iNetMask and #iAddress
	* define a prefix for the interface (link). If iNetMask is
	* unspecified, then the iDoPrefix is ignored.
	*
	* @li iDelete=0: 
	*	Acts as if an IPv6 Router Advertisement with prefix option A=1
	*	and L=1 has arrived (e.g. this prefix can be used in address
	*	generation and all addresses with this prefix are onlink on
	*	this interface).
	*
	* @li iDelete=1:
	*	The specified prefix is deleted from the interface (if it
	*	existed before).
	*
	* @note
	*	Current IPv6 specification allows only 64 for the number
	*	of prefix bits.
	*/
	TUint iDoPrefix:1;
	/**
	* Address action.
	* 
	* 0 = don't do address, 1= do the address.
	*
	* #iAddress must be specified.
	*
	* @note
	*	If also either #iDoAnycast or #iDoProxy is set, then
	*	the action is special for them (and the following
	*	does not apply).
	*
	* If #iNetMask is unspecified, then #iAddress defines a single
	* address (either IPv4 or IPv6) which is to be added or removed,
	* depending on the state of the #iDelete modifier. #iAlias
	* is ignored.
	*
	* If #iNetMask is specified, then the following applies:
	*
	* @li iDelete=0 and iAddress is IPv4 address:
	*	iAddress and iNetMask are used to configure additional
	*	IPv4 address and netmask for the interface.
	* @li iDelete=0 and iAddress is IPv6 address
	*	The iNetmask and iAddress define a ID part, which can be
	*	combined with any defined prefix to form a full IPv6 address.
	*	If iAlias is set, then a new ID is added; otherwise whatever
	*	happens to be stored in the primary id slot is overwritten
	*	(always use iAlias=1 to avoid confusion).
	*
	* @li iDelete=1:
	*	The previously configured address or ID is deleted.
	*
	* @note
	*	The IPv4 netmask alone cannot be added or deleted. Use #KSolInetRtCtrl
	*	options.
	*/
	TUint iDoId:1;
	/**
	* Interface state action.
	* 
	* 0 = ignore TSoInetInterfaceInfo::iState,
	*
	* 1 = set interface state based on
	* TSoInetInterfaceInfo::iState as follows:
	* @li EIfDown:
	*	The interface state is set to KErrNotReady.
	* @li EIfUp:
	*	The interface state is set to 0 (KErrNone).
	* @li
	*	Attempt to set any other state results failed operation
	*	with KErrArgument result.
	*/
	TUint iDoState:1;
	/**
	* Configure address as Anycast.
	*
	* The anycast address is defined by #iAddress.
	*
	* Anycast address is recognized as own address for incoming
	* packets, but it cannot be used as a source address for
	* outgoing packets. IPv6 DAD (or IPv4 ARP duplicate address)
	* test is not done for anycast addresses. Anycast address is
	* advertised on the link as an address of this host.
	*
	* 1 = configure anycast (#iDoId must also be set, #iDoPrefix is ignored)
	*
	* @li iDelete=0:
	*	Add anycast address.
	* @li iDelete=1:
	*	Remove the previously configured anycast address.
	*
	* @since 7.0s
	*/
	TUint iDoAnycast:1;
	/**
	* Confiture address as Proxy.
	*
	* The proxy address is defined by #iAddress.
	*
	* Proxy address is not recognized as own address for incoming
	* packets (nor can it be used as own address for outgoing packets).
	* IPv6 DAD (or IPv4 ARP duplicate address) test is performed for
	* proxy address. Proxy address is advertised on the link as an
	* address of this host.
	*
	* 1 = configure proxy (#iDoId must also be set, #iDoPrefix is ignored)
	*
	* @li iDelete=0:
	*	Add proxy address.
	* @li iDelete=1:
	*	Remove the previously configured proxy address.
	*
	* @since 7.0s
	*/
	TUint iDoProxy:1;
	};
//@}

/**
* @name Interface query socket options
*
* Level: #KSolInetIfQuery
*
* Querying information about interfaces using TSoInetIfQuery.
*
* @since 7.0 (some additions in 7.0s)
*/
//@{
/** Scope Id vector (member of TSoInetIfQuery). @since 7.0s */
typedef TUint32 TInetScopeIds[16];

class TSoInetIfQuery
/**
* Interface query.
*
* Used with interface query options:
* @li	#KSoInetIfQueryByDstAddr
* @li	#KSoInetIfQueryBySrcAddr
* @li	#KSoInetIfQueryByIndex
* @li	#KSoInetIfQueryByName
*
* Only GetOption for KSolInetIfQuery is supported. It returns
* information about the selected interface. The option name
* determines the input field in the TSoInetIfQuery, which is
* used as a key for locating the interface.
*
* Returns, KErrNotFound, if interface is not found
*
* Returns, KErrNone, if interface is located, and fills
* fields from the interface with following logic
*
* @li	iDstAddr: not touched, left as is
* @li	iSrcAddr is result of the Select Source Address algorithm
*	    for the interface using the iDstAddr as input. If there
*		is no valid source address, the value will be KAFUnspec.
* @li	iIndex is loaded with the interface index of the interface
* @li	iName is loaded from the name of the interface
* @li	iIsUp is set 1, if interface has CNifIfBase pointer attached,
*		and 0 otherwise.
*
* For example, if QueryByDstAddr for specified destination address
* results iIsUp == 1, then there is an interface and route for that
* destination. iIsUp == 0, means that trying to connect to the address
* will most likely activate a netdial/interface link startup phase.
*
* @publishedAll
* @released
*/
	{
public:
	/**
	* Destination address.
	* @li input:
	*	If the option is #KSoInetIfQueryByDstAddr, select
	*	interface by finding a route for this address;
	*	otherwise, ignored.
	* @li output: not changed.
	*
	* @note
	*	On returning interface information, regardless of
	*	the option used, the content of this is used to select
	*	a matching source address (#iSrcAddr).
	*/
	TInetAddr iDstAddr;
	/**
	* Source address.
	* @li input:
	*	If the option is #KSoInetIfQueryBySrcAddr, select
	*	interface by source address; otherwise, ignored.
	* @li output:
	*	The result of the source address
	*	selection algorithm based on the content of the
	*	#iDstAddr.
	*/
	TInetAddr iSrcAddr;
	/**
	* Interface Index. 
	*
	* @li input:
	*	If the option is #KSoInetIfQueryByIndex, select
	*	interface by this interface index; otherwise,
	*	ignored.
	* @li output:
	*	The interface index the located interface.
	*	(always same as iZone[0] in 7.0s).
	*/
	TUint32 iIndex;
	/** Interface name.
	* @li input:
	*	If the option is #KSoInetIfQueryByName, select
	*	interface by this name; otherwise, ignored.
	* @li output:
	*	The name of the located interface.
	*/
	TName iName;
	/**
	* Flag that is set to 1 if the network interface is attached.
	* @li input: ignored
	* @li output: set as indicated.
	*/
	TUint iIsUp:1;
	/**
	* Scope Id Vector (iZone[0] = Interface Index, iZone[1] = IAP ID, iZone[15] = Network ID).
	* @li input: ignored
	* @li output: The scope id vector
	* @since 7.0s
	*/
	TInetScopeIds iZone;
	};
/**
* Get information for the interface specified by the destination address (iDstAddr) 
* field of the passed packaged TSoInetIfQuery.
* 
* This allows the caller to find out what interface would be used (without invoking 
* a dial-up process) for the specified destination. A path for this destination 
* is open, if GetOpt() returns KErrNone, iSrcAddr is not KAFUnspec, and iIsUp 
* == 1.
*/
const TUint KSoInetIfQueryByDstAddr	= 0x1;
/**
* Get information for the interface specified by the source address (iSrcAddr) 
* field of the passed packaged TSoInetIfQuery.
*
* If there are multiple interfaces with the same source address, then the first 
* matching interface is returned. 
* 
* @note
*	The information return phase will overwrite the iSrcAddr based on
*	whatever happens to be in iDstAddr. It is not necessary to initialize
*	iDstAddr, if application is not interested in resulting iSrcAddr.
*/
const TUint KSoInetIfQueryBySrcAddr	= 0x2;
/**
* Get information for the interface specified by the Interface Index (iIndex) 
* field of the passed packaged TSoInetIfQuery.
*/
const TUint KSoInetIfQueryByIndex	= 0x3;
/**
* Get information for the interface specified by the Interface Name (iName) field 
* of the passed packaged TSoInetIfQuery.
*/
const TUint KSoInetIfQueryByName	= 0x4;

//@}

/**
* @name	Route control socket options
*
* Level: #KSolInetRtCtrl
*
* Enumerating & Configuring Routes using TSoInetRouteInfo.
*/
//@{
/**
* Begin enumeration of routes. 
*
* This option can only be used with RSocket::SetOpt().
* 
* This option should be set before enumerating routes with #KSoInetNextRoute.
*
* @capability NetworkServices
*/
const TInt KSoInetEnumRoutes  = 0x221;
/**
* Return the next route in an enumeration started by setting the option #KSoInetEnumRoutes.
* 
* Option data type is TSoInetRouteInfo.
* 
* This option can only be used with RSocket::GetOpt().
*
* @capability NetworkServices
*/
const TInt KSoInetNextRoute	  = 0x222;
/**
* Adds the specified route to the routing table.
* 
* Option data type is TSoInetRouteInfo.
* The interface is defined by the TSoInetRouteInfo::iIfAddr and must exist.
* 
* This option can only be used with RSocket::SetOpt().
*
* @capability NetworkControl Modifying routes is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TInt KSoInetAddRoute	  = 0x223;
/**
* Deletes the specified route from the routing table.
* 
* The route is identified by destination, netmask and gateway,
* These must exactly match the old route to be deleted.
*
* Option data type is TSoInetRouteInfo.
* The interface is defined by the TSoInetRouteInfo::iIfAddr and must exist.
* 
* This option can only be used with RSocket::SetOpt().
*
* @capability NetworkControl Modifying routes is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TInt KSoInetDeleteRoute = 0x224;
/**
* Modifies the specified route in the routing table. 
* 
* The destination, netmask and gateway settings must be the same
* in the new route as in the old.
* 
* Option data type is TSoInetRouteInfo.
* The interface is defined by the TSoInetRouteInfo::iIfAddr and must exist.
* 
* This option can only be used with RSocket::SetOpt().
*
* @capability NetworkControl Modifying routes is allowed for authorized apps only.
* @ref RSocket::SetOpt()
*/
const TInt KSoInetChangeRoute = 0x225;

/**
* Identifies the state of a route held in an entry in the IP routing table. 
* 
* It is used as a data member of TSoInetRouteInfo.
*
* @note
*	This enumeration is present only because of backward
*	compatibility. Only two values are used.
*
* @since 7.0 (in this form)
*
* @publishedAll
* @released
*/
enum TRouteState
	{
	/** Unused. */
	ERtNone,
	/** Route is neighbour cache entry, ARP or Neighbor discovery is in progress. */
	ERtPending,
	/** Unused */
	ERtBusy,
	/** The interface for the route is up and ready. */
	ERtReady,
	/** Unused */
	ERtDown
	};

/**
* Identifies the type of creator of an entry in the IP routing table. 
* 
* It is used as a data member of TSoInetRouteInfo.
*
* @note
*	This enumeration is present only because of backward
*	compatibility. Only two values are used.
*
* @since 7.0 (in this form)
*
* @publishedAll
* @released
*/
enum TRouteType
	{
	/** Normal route entry */
	ERtNormal,
	/** Unused */
	ERtUser,
	/** Route is ARP or neighbor cache entry */
	ERtIcmpAdd,
	/** Unused */
	ERtIcmpDel
	};

class TLinkAddr : public TSockAddr
	/**
	* TLinkAddr
	*
	* Link layer address utility.
	*
	* Lightweight helper class for handling link layer addresses.
	*
	* This class is mainly used to obtain direct access to the raw address
	* bytes which are inacessible from the TSockAddr interface.
	*
	* A link layer address is a binary string of octets.
	*
	* The address family of the TLinkAddr is determined by the interface. If
	* the interface uses link layer addresses, it must support the
	* #KSoIfHardwareAddr control option, and the returned address family of the
	* harware address is supposed to represent the family of all link layer addresses
	* of peers on the interface.
	*
	* Link layer addresses can be obtained from e.g. TSoInetRouteInfo structures
	* using the TSoInetRouteInfo::GetLinkAddr function.  Note: this may fail if the
	* link layer address of the peer corresponding to the route is not known.
	*
	* @publishedAll
	* @released
	*/
	{
public:
	IMPORT_C TLinkAddr();
	IMPORT_C void SetAddress(const TDesC8 &aAddr);

	IMPORT_C TPtrC8 Address() const;

	IMPORT_C const static TLinkAddr& Cast(const TSockAddr& aAddr);
	IMPORT_C static TLinkAddr& Cast(TSockAddr& aAddr);
	IMPORT_C const static TLinkAddr* Cast(const TSockAddr* aAddr);
	IMPORT_C static TLinkAddr* Cast(TSockAddr* aAddr);

	};

class TSoInetRouteInfo
/**
* Route information structure.
*
* Used with route options:
*
* @li	#KSoInetNextRoute
* @li	#KSoInetAddRoute
* @li	#KSoInetDeleteRoute
*
* IPv4 addresses are returned as IPv4-mapped IPv6 addresses
* qualified with appropriate scope id (Symbian OS 7.0 and later).
*
* @publishedAll
* @released
*/
	{
public:
	IMPORT_C TInt GetLinkAddr( TLinkAddr &aInfo ) const;

	/** Route type. */
	TRouteType iType;
	/** Route state. */
	TRouteState iState;
	/** Route preference, with a smaller value indicating a preferred route. */
	TInt iMetric;
	/** IP address of the interface used for this route. */
	TInetAddr iIfAddr;
	/** IP address of the gateway, or link-layer address for neighbour cache entries */
	TInetAddr iGateway;
	/** IP address of the destination network or host. */
	TInetAddr iDstAddr;
	/** Destination mask of network. */
	TInetAddr iNetMask;
	};

class TSoInetCachedRouteInfo : public TSoInetRouteInfo
/**
* Access to route cache TPckgBuf<TSoInetCachedRouteInfo>, set iDstAddr for required address
* With a level of KSolInetRtCtrl. This API is no longer suported.
*
* @removed
* @since 7.0
*/
	{
public:
	/** Unused */
	TInt iPathMtu;
	/** Unused */
	TUint iPathRtt;
	};

/** No longer supported. @removed. @since 7.0 */
const TInt KSoInetCachedRouteByDest = 0x225;

//@}


/**
* @name	DNS definitions
*/
//@{
/**
* Flags returned from DNS records.
*
* Provides flag bitmasks that are used to describe properties of results of DNS 
* queries via RHostResolver.
* @publishedAll
* @released
*/
enum TNameRecordFlags
	{
	/** Name is an Alias. */
	EDnsAlias=0x00000001,
	/** Answer is authoritive. */
	EDnsAuthoritive=0x00000002,
	/** Answer is from hosts file. */
	EDnsHostsFile=0x00000004,
	/** Answer is from a DNS server. */
	EDnsServer=0x00000008,
	/** Answer is host name for this host. */
	EDnsHostName=0x00000010,
	/** Answer is from the resolver cache. */
	EDnsCache=0x00000020,
	/** Answer does not have a route set */
	EDnsNoRoute=0x00000040
	};

/** No longer supported.
* @removed
* @since 7.0 */
const TUint KSoDnsCacheEnable = 0x600;
/** No longer supported.
* @removed
* @since 7.0 */
const TUint KSoDnsCacheFlush = 0x601;
#ifdef SYMBIAN_DNS_PUNYCODE
/** Enable International Domain Name support 
 * @publishedAll
 * @released
 */
const TUint KSoDnsEnableIdn = 0x602;
#endif //SYMBIAN_DNS_PUNYCODE
//@}

/**
* @name TCP socket options
*
* Level: #KSolInetTcp
*/
//@{
/**
* Complete the ioctl request when the data has been sent.
*/
const TUint KIoctlTcpNotifyDataSent = 0x300;

/**
* The maximum number of bytes that can be queued for sending. 
* 
* If this option is set when the connection state is not closed,
* then KErrLocked is returned.
* 
* Option data type is TInt.
* 
* The default value is 8192.
*/
const TUint KSoTcpSendWinSize = 0x301;
/**
* The maximum number of bytes that can be buffered for receiving. 
* 
* If this option is set when the connection state is not closed,
* then KErrLocked is returned.
* 
* Option data type is TInt.
* 
* The default value is 8192.
*/
const TUint KSoTcpRecvWinSize = 0x302;
/**
* The maximum TCP segment size (bytes). 
* 
* If this option is set when the connection state is not closed,
* then KErrLocked is returned.
*
* Option data type is TInt.
*
* The default value is 1460.
*/
const TUint KSoTcpMaxSegSize = 0x303;
/**
* Send data at once if there is an established connection, without
* waiting for the maximum segment size to be reached. 
* 
* The default is disabled.
* 
* Option data type is TInt.
* 
* Values are: 0 = Disable, 1 = Enable.
*/
const TUint KSoTcpNoDelay = 0x304;
/**
* On the time-out expiring without an acknowledgement being received,
* send a packet designed to force a response if the peer is up and reachable.
*
* The default is disabled.
* 
* Option data type is TInt.
* 
* Values are: 0 = Disable, 1 = Enable.
*/
const TUint KSoTcpKeepAlive = 0x305;
/**
* If address reuse is allowed, and a connection already exists between
* the requested local and remote addresses, wait for the address to
* become available.
* 
* The default is disabled.
* 
* Option data type is TInt.
* 
* Values are: 0 = Disable, 1 = Enable.
* @removed
*/
const TUint KSoTcpAsync2MslWait = 0x306;
/**
* The number of bytes currently queued for sending.
* 
* Option data type is TInt.
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpSendBytesPending = 0x307;
/**
* The number of bytes currently available for reading (the same value as
* is obtained using KSOReadBytesPending).
* 
* Option data type is TInt.
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpReadBytesPending = 0x308;
/**
* The socket has been set to listen (through RSocket::Listen()).
* 
* Option data type is TInt.
* 
* Values are: 0. Not listening, 1. Listening
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpListening = 0x309;
/**
* The number of current TCP sockets.
* 
* Option data type is TInt.
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpNumSockets = 0x310;
/**
* Read out-of-band urgent data.
* 
* KErrNotFound is returned if there is no data waiting and no
* urgent data pointer has been received.
* 
* KErrWouldBlock is returned if and urgent is available but data needs to be 
* read from the current stream to match the urgent data mark.
* 
* Option data type is TInt.
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpReadUrgentData = 0x311;
/**
* Peeks for urgent data. The behaviour is the same as KSoTcpReadUrgentData,
* but the urgent data is not removed.
* 
* Option data type is TInt.
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpPeekUrgentData = 0x312;
/**
* True if the data stream has been read up to the point where urgent
* data is available, otherwise false.
* 
* Option data type is TInt.
* 
* This option can only be used with RSocket::GetOpt(), not RSocket::SetOpt().
*/
const TUint KSoTcpRcvAtMark = 0x313;
/**
* The next send operation will mark the last byte sent as urgent data.
* 
* The default is disabled.
* 
* Option data type is TInt.
* 
* Values are: 0 = Disable, 1 = Enable.
*/
const TUint KSoTcpNextSendUrgentData = 0x314;
/**
* Receive out-of-band data in the normal data stream.
* 
* The default is disabled.
* 
* Option data type is TInt.
* 
* Values are: 0 = Disable, 1 = Enable.
*/
const TUint KSoTcpOobInline = 0x315;

#ifdef SYMBIAN_ADAPTIVE_TCP_RECEIVE_WINDOW
/**
* TCP max receive value
* 
* Used with SetOpt to set TCP Max recv window size
*/
const TUint KSoTcpMaxRecvWin = 0x316;
/**
* TCP Receive window size for auto tuning
* 
* Used with SetOpt to set TCP Max recv window size
*/
const TUint KSoTcpRecvWinAuto = 0x317;

#endif //SYMBIAN_ADAPTIVE_TCP_RECEIVE_WINDOW


/** Not supported. @removed */
const TUint KSOTcpDebugMode = 0x11110000;
//@}

/**
* @name IPv6 and IPv4 socket options
*
* Level: #KSolInetIp
*
*/
//@{
/**
* Data to place in IP Options field of sent datagrams. 
* 
* Not supported. @removed
* @since 7.0
*/
const TUint KSoIpOptions = 0x401;
/**
* Include IP header in data returned to client.
* 
* IPv4 packets are returned as is with all headers in network byte order (until 
* v7.0, this returned IPv4 headers in host order). See TInet6HeaderIP4 and
* TInet6HeaderIP for the header layout.
* 
* The default is disabled.
* 
* Option data type is TInt.
* Values are: 0 = Disable, 1 = Enable.
*
* @sa	KSoHeaderIncluded, KIpHeaderIncluded
*/
const TUint KSoRawMode = 0x402;
/**
* Assume that the IP header is included in all data written by the client.
*
* KSoRawMode must be set before this is allowed.
*
* 
* Option data type is TInt.
* Values are: 0. Disable; 1. Enable
* 
* The default is disabled.
*
* @sa	KSoRawMode, KIpHeaderIncluded
*/
const TUint KSoHeaderIncluded = 0x403;
/**
* Type of Service field of outgoing datagrams.
* 
* For IPv6, there is no Type of Service field, so this option sets the Traffic Class.
* 
* Option data type is TInt.
* Values are 0-255. Because Explicit Congestion Notification [RFC3168] uses bits 6 & 7
* in the IP field, modifying the two least significant bits is not allowed with TCP.
* SetOpt processing silently ignores any modifications on these bits when using TCP socket.
* 
* The default value is 0.
*/
const TUint KSoIpTOS = 0x404;
/**
* Time to Live field of outgoing datagrams.
* 
* This is same as #KSoIp6UnicastHops.
* 
* Option data type is TInt.
* Values are [-1,,255]. The -1 resets to the configured default value.
* 
* There are separate configured default values for the link local and other
* destinations. Both defaults can be configured by the TCPIP.INI parameters
* <tt>maxttl</tt>  and <tt>linklocalttl</tt>, The compiled defaults are
* #KTcpipIni_Maxttl and #KTcpipIni_LinkLocalttl.
*
* @note
*	For the TTL of multicast destinations, @see KSoIp6MulticastHops.
*/
const TUint KSoIpTTL = 0x405;
/**
* Allow a socket to be bound to an local address that is already in use.
* 
* Option data type is TInt.
* Values are: 0 = Disable, 1 = Enable.
* 
* The default is disabled.
* @capability NetworkControl		Required for 'udp' sockets.
*/
const TUint KSoReuseAddr = 0x406;
/**
* Do not set socket into error state if currently connected
* interface reports an error.
*
* For example, this could be enabled for a unconnected datagram
* socket. Unconnected datagram sockets are connected to the
* interface of the last sent packet. If multiple interfaces
* are present, erroring the socket might not be the desired
* action if just one interface gives an error.
*
* Another use case would be a connected socket (TCP), which
* does not get error even if interface goes down, and comes
* up with the same source address after a while.
* 
* Option data type is TInt.
* Values are: 0 = Disable, 1 = Enable.
* 
* The default can be changed by the TCPIP.INI parameter <tt>noiferror</tt>,
* and the compiled default is #KTcpipIni_Noiferror..
* 
* @since 7.0
*/
const TUint KSoNoInterfaceError = 0x407;
/**
* Modify socket visibility.
* 
* Background internet services that have sockets open count as active 
* user and prevents the TCPIP from shutting down. 
* By this socket option, such a process can make selected sockets to be
* excluded from the count.
* 
* Option data type is TInt.
* By setting the value to 0, the socket is not counted as active user. The value 
* 1 makes it visible again.
* 
* The option has no effect if the visibility state already matches the parameter.
*
* By default, all sockets are initially visible.
*
* @note
*	This option should only be used by background daemons which are
*	started by the TCPIP stack.
* @since 7.0
*/
const TUint KSoUserSocket = 0x408;
/**
* Set or get interface index of the socket.
*
* Gets the current interface index of the socket. Returns the
* value used in the last set.
*
* If interface index has not been set by this option, then value is
* determined as follows:
*
* @li
*	the interface index of the interface which got the last packet
*	from this socket.
* @li
*	zero, if no packets have been sent or interface cannot be
*	determined.
*
* Option data type is TUint32.
*
* @since 7.0s
*/
const TUint KSoInterfaceIndex = 0x409;
/**
* Controls whether the interface flow counter is affected by this socket. 
* 
* This counter is used in determining when the interface can be brought down.
* 
* Option data type is TInt.
* Values are: 0=Don't count, 1= count flow against 
* interface flow count.
*
* The default can be changed by the TCPIP.INI parameter <tt>keepinterfaceup</tt>,
* and the compiled default is #KTcpipIni_KeepInterfaceUp.
*
* @since 7.0s
*/
const TUint KSoKeepInterfaceUp = 0x40a;
/**
* Enable use of 0 as a source address.
*
* When socket is bound to unspecified address (0), the stack will automaticly
* select the source address for the outgoing packets. When this option is
* set <b>after bind</b>, the stack will not select a new address.
*
* @since 7.0s
*/
const TUint KSoNoSourceAddressSelect = 0x40b;
/**
* Retrieve last error information.
* 
* This option is for use with GetOpt() only.
* 
* Option data type is TSoInetLastErr.
*/
const TUint KSoInetLastError = 0x200;
/**
* An Ioctl corresponding to the socket option KSoInetLastError.
*/
const TUint KIoctlInetLastError = 0x200;
/**
* Hop limit for outgoing datagrams: same as #KSoIpTTL.
* 
* Option data type is TInt.
* Values are [-1,,255]. The -1 resets to the configured default value.
* 
* @see #KSoIpTTL for details.
*
* @note
*	KSoIp6UnicastHops can be used to detect dual IPv4/IPv6 stack from
*	from the old TCPIP only-IPv4 stack. This option is only implemented
*	in the dual stack.
* @since 7.0
*/
const TUint KSoIp6UnicastHops = 0x465;
/**
* Interface for outgoing multicast packets
* 
* Unused.
*/
const TUint KSoIp6MulticastIf = 0x46a;
/**
* Hop limit for multicast packets.
* 
* Option data type is TInt.
* Values are [-1..255]. The -1 resets to the configured default value.
* 
* The default is 1.
* @since 7.0
*/
const TUint KSoIp6MulticastHops	= 0x46b;
/**
* Enable/disable loopback of the multicast packets.
*
* When enabled, multicast packets sent to this socket are internally
* looped back (in addition to sending them onto the interface). Another
* or same application listening the group and port, receives copies of
* the transmitted packets.
*
* When disabled, an application on this host listening the same group
* and port, does not receive multicast packets originating from this
* socket (unless the interface or link echoes them back to the TCP/IP
* stack).
* 
* Option data type is TInt.
* Values are 1=enable; 0=disable. The default is 1.
* @since 7.0
*/
const TUint KSoIp6MulticastLoop	= 0x46c;
/**
* Join multicast group. 
*
* Option data type is TIp6Mreq.
* @since 7.0
*/
const TUint KSoIp6JoinGroup	= 0x46d;
/**
* Leave multicast group. 
*
* Option data type is TIp6Mreq.
* @since 7.0
*/
const TUint KSoIp6LeaveGroup = 0x46e;
/**
* Hop limit for outgoing datagrams: similar to KSoIp6UnicastHops except
* any socket option to override the current setting is ignored.  The value
* returned is either the value of the associated interface or the TCP/IP 6
* stack default if no interface has been selected yet.
* 
* Option data type is TInt.
* Values are [0..255]. Value cannot be modified, only queried.
* 
* @see KSoIp6UnicastHops for details.
*
* @since 9.2
*/
const TUint KSoIp6InterfaceUnicastHops = 0x46f;


class TSoInetLastErr
/**
* Error information for TCP/IP protocols. 
* 
* An object of this class is returned packaged as a TPckgBuf<TSoInetLastErr> 
* in the option argument of RSocket::GetOpt(), when this function is called 
* with (KSolInetIp, KSoInetLastError). The data members of this object 
* are updated whenever a packet carrying an ICMP message is received.
*
* @note
*	This class is originally defined only for the IPv4 environment, and
*	there is no definite way of knowing whether the fields iErrType and
*	iErrCode contain ICMPv4 or ICMPv6 codes. A solution that will give
*	the correct answer in most normal cases, is
@code
	TSoInetLastErr p;
	...
	if (p.iErrAddr.Family() == KAfInet || p.iErrAddr.IsV4Mapped())
		// assume ICMPv4 type and code
	else
		// assume ICMPv6 type and code
@endcode

* Alternatively, the error can be interpreted based on the member variable
* iStatus, if it contains one of the extended error codes. These are are
* common for both IPv4 and IPv6.
*
* @publishedAll
* @released
*/
	{
public:
	/** The error code returned by the last ESOCK API function called. */
	TInt iStatus;
	/** The value of the Type field of the last ICMP message. */
	TInt iErrType;
	/** The value of the Code field of the last ICMP message. */
	TInt iErrCode;
	/** A TInetAddr with the IP address and port set to the source address and port 
	* of the failed datagram. */
	TInetAddr iSrcAddr;
	/** A TInetAddr with the IP address and port set to the destination address and 
	* port of the failed datagram. */
	TInetAddr iDstAddr;
	/** A TInetAddr with the IP address set to the address of the host that generated 
	* the error. */
	TInetAddr iErrAddr;
	};

class TIp6Mreq
/**
* TIp6Mreq.
*
* Used by IPv6 or IPv4 multicast join/leave group socket options
* #KSoIp6JoinGroup and #KSoIp6LeaveGroup.
*
* Joining to a multicast group address adds this address to the
* list of addresses for which incoming packets are accepted.
* Optionally, if the required support has been installed, some
* MLD (Multicast Listener Discovery) protocol messages may be
* generated.
*
* The multicast join/leave are always interface specific,
* and the interface index should be specified in the set option call.
* If the index value is set to 0, the stack attempts to select
* some interface.
*
* @publishedAll
* @released
* @since 7.0
*/
	{
public:
	/** IPv6 or IPv4 multicast address. */
	TIp6Addr iAddr;
	/** Interface Index. */
	TUint iInterface;
	};
//@}

/**
* @name UDP options
*
* Level: #KSolInetUdp
*/
//@{
/**
* Inform client of error if ICMP error packets received.
* 
* The default is disabled.
* 
* Option data type is TInt.
* Values are: 0 = Disable, 1 = Enable.
*/
const TUint KSoUdpReceiveICMPError = 0x500;

/**
Modifies address flag of UDP. Flag is used to control whether the socket is bound to
IP address or not. Binding to specific address and then clearing this flag makes possible
to receive packets sent to broadcast address but still to have a specific bound address 
for outgoing packets.
*/
const TUint KSoUdpAddressSet = 0x502;

/**
* Setting this option causes the UDP send operation to block when dynamic interface 
* setup is in progress, or when local flow control within the stack would otherwise 
* cause the packet to be dropped.
* @since 7.0
*/
const TUint KSoUdpSynchronousSend = 0x560;
//@}

/**
* @name TCP/IP specific Socket provider options
*
* Level: #KSOLProvider
*/
//@{
/**
* Internal flow option called when the flow is about to be closed.
*
* This option is only for the outbound flow hooks, which are
* attached to the TCIP/IP stack. The stack generates a call, just
* before the flow context associated with service access point
* (SAP) provider is about to be closed. If a hook has stored
* some socket specific state information into the flow context,
* then this option event may be of some use to it.
* @since 7.0
*/
const TUint KSoFlowClosing = 0x600 | KSocketInternalOptionBit;
//@}

/**  @name Extended error codes */
//@{
/** Network could not be reached. */
const TInt KErrNetUnreach = -190;
/** Host could not be reached. */
const TInt KErrHostUnreach = -191;
/** Protocol could not be reached. */
const TInt KErrNoProtocolOpt = -192;
/** Urgent data error. */
const TInt KErrUrgentData = -193;
//@}

#endif
