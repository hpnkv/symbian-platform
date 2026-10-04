/**
* Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:
* Interface Manager Standard Variable Names
* 
*
*/



/**
 @file nifvar_internal.H
 @internalComponent
 @released
*/


#if !defined(__NIFVAR_INTERNAL_H__)
#define __NIFVAR_INTERNAL_H__

#include <e32def.h>

#ifdef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <nifvar.h>
#endif

/**
Generic progress notifications from the null configuration daemon.
@internalComponent
@released
**/
const TInt KNullConfigDaemonConfigureNetwork = 8401;

const TInt KDataTransferUnblocked = 4001;           // eg. resume (GPRS); from an agent



/**
The layer to which the call refers
@note Used by data sent and received to indicate to which layer the byte count refers
@todo Write about this, add appropriate arguments to interfaces
@internalTechnology
*/
enum TConnectionLayer
	{
	EPhysicalLayer,
	EDataLinkLayer = 100,
	ENetworkLayer = 200,
	ETransportLayer = 300,
	ESessionLayer = 400,
	EPresentationLayer = 500,
	EApplicationLayer = 600
	};


#endif

