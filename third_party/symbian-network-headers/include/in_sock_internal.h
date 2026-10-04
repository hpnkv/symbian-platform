
// Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
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
/** @file in_sock_internal.h
* IPv6/IPv4 socket library internal header 
*
*/

#ifndef __IN_SOCK_INTERNAL_H__
#define __IN_SOCK_INTERNAL_H__

#include <e32std.h>


/** Load scope vector from iZone (Set) @internalAll */
const TUint KSoInetIfQuerySetScope	= 0x10;
/** Set interface to Host mode @internalAll */
const TUint KSoInetIfQuerySetHost	= 0x11;
/** Set interface to Router mode @internalAll */
const TUint KSoInetIfQuerySetRouter	= 0x12;



/**
Set the UDP receive buffer size for a socket in bytes. Overrides global ini parameter
<tt>udp_recv_buf</tt>. At least one datagram always fits to the buffer, no matter how
small it is.

Default receive buffer size is 8192 bytes, or the value given in <tt>udp_recv_buf</tt>.

@internalAll
@released
*/
const TUint KSoUdpRecvBuf = 0x501;
 
#endif
