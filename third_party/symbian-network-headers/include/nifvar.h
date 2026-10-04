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
 @file NIFVAR.H
 @publishedAll
 @released
*/


#if !defined(__NIFVAR_H__)
#define __NIFVAR_H__

#include <e32std.h>
#ifndef SYMBIAN_ENABLE_SPLIT_HEADERS
#include <nifvar_internal.h>
#endif

/**
Generic Progress Constant
From NIFMAN
@note These ranges must be obeyed by all network adapters
@note These are augmented by the PSD and CSD constants below

@publishedAll
@released
*/
const TInt KConnectionUninitialised  = 0;              // From NIFMAN
const TInt KStartingSelection        = 1000;           // From NIFMAN
const TInt KFinishedSelection        = 2000;           // From NIFMAN
const TInt KConnectionFailure        = 2001;           // From NIFMAN

const TInt KMinAgtProgress           = 2500;
const TInt KConnectionOpen           = 3500;           // From an Agent
const TInt KConnectionClosed         = 4500;           // From an Agent
const TInt KMaxAgtProgress           = 5500;

const TInt KMinNifProgress           = 6000;
const TInt KMaxNifProgress           = 9000;

/**
Do no use in new code. Use KConnectionUp or KConnectionDown
from es_sock.h instead.

@publishedAll
@deprecated
*/
const TInt KLinkLayerOpen            = 7000;           // From a NIF
const TInt KLinkLayerClosed          = 8000;           // From a NIF

/**
Additional generic progress constantfrom an agent
@note These values may be implemented by network adapters (nifs/agents) 
@see KPsdStartingConfiguration
@see KCsdStartingDialling

@publishedAll
@released
*/
const TInt KStartingConnection = 3000;				// from an agent
const TInt KIncomingConnectionInProgress = 3350;	// from an agent
const TInt KDataTransferTemporarilyBlocked = 4000;	// eg. suspended (GPRS); from an agent
//const TInt KDataTransferUnblocked = 4001;           // eg. resume (GPRS); from an agent - declared in nifvar_internal.h
const TInt KConnectionStartingClose	= 4250;			// from an agent

/**
Additional generic progress constant
from an agent
@note These values may be implemented by network adapters (nifs/agents) 
@see KPsdStartingConfiguration
@see KCsdStartingDialling

@publishedAll
@released
*/
const TInt KPsdStartingConfiguration = KMinAgtProgress;	// required
const TInt KPsdFinishedConfiguration = 2750;			// desirable

const TInt KPsdAnsweringIncoming	 = KIncomingConnectionInProgress; // required if supporting incoming connections

const TInt KPsdStartingActivation	 = KStartingConnection; // required
const TInt KPsdFinishedActivation	 = KConnectionOpen;		// required

const TInt KPsdSuspended			 = KDataTransferTemporarilyBlocked;	// required if this case can occur

const TInt KPsdStartingDeactivation	 = KConnectionStartingClose;	// desirable
const TInt KPsdFinishedDeactivation  = KConnectionClosed;			// required

/**
Additional generic progress constant
eg. suspended (GPRS); from an agent
@note These values may be implemented by network adapters (nifs/agents) 
@see KPsdStartingConfiguration
@see KCsdStartingDialling

@publishedAll
@released
*/
const TInt KCsdStartingDialling	= KMinAgtProgress;		// optional
const TInt KCsdFinishedDialling = 2600;					// desirable
const TInt KCsdScanningScript	= 2650;					// optional
const TInt KCsdScannedScript	= 2750;					// optional
const TInt KCsdGettingLoginInfo	= 2800;					// optional
const TInt KCsdGotLoginInfo		= 2900;					// optional
const TInt KCsdStartingConnect	= KStartingConnection;	// optional
const TInt KCsdFinishedConnect	= 3100;					// desirable
const TInt KCsdStartingLogIn	= 3200;					// optional
const TInt KCsdFinishedLogIn	= 3250;					// optional

const TInt KCsdStartingAnswer	= KIncomingConnectionInProgress; // required if supporting incoming connections/callback
const TInt KCsdAnswered			= 3400;					// required if supporting incoming connections/callback

const TInt KCsdConnectionOpen	= KConnectionOpen;		// required
const TInt KCsdStartingHangUp	= KConnectionStartingClose;	// desirable
const TInt KCsdFinishedHangUp	= KConnectionClosed;	// required

/**
 Generic progress notifications from the configuration daemon.
 @publishedAll
 @released
**/
const TInt KMinConfigDaemonProgress = 8100;
const TInt KConfigDaemonLoading = 8100;
const TInt KConfigDaemonLoaded = 8200;
const TInt KConfigDaemonStartingRegistration = 8300;
const TInt KConfigDaemonFinishedRegistration = 8400;
const TInt KConfigDaemonStartingDeregistration = 8600;
const TInt KConfigDaemonFinishedDeregistrationStop = 8700;
const TInt KConfigDaemonFinishedDeregistrationPreserve = 8701;
const TInt KConfigDaemonFinishedDormantMode = 8750;
const TInt KConfigDaemonUnloading = 8800;
const TInt KConfigDaemonUnloaded = 8900;
const TInt KMaxConfigDaemonProgress = 8900;
 
/**
New software should use the progress ranges defined above

@publishedAll
@deprecated 7.0s - maintained for compatibility with 6.1
*/
const TInt KAgentUninitialised = KConnectionUninitialised;
const TInt KAgentUnconnected = 1;
const TInt KMaxAgentProgress = 999;
const TInt KMinInterfaceProgress = 1000;
const TInt KMaxInterfaceProgress = 1999;

/**
The type of connection provided by the network interface

@publishedAll
@released
@note It is acceptable for clients to check that a TConnectionType falls within a 
specified range, and assume that the basic parameters can be obtained.  For example,
if the returned TConnectionType is between 2000 and 2499, it would be acceptable for
a client to assume that it was a GPRS connection, and that TGPRSSubConnectionInfo 
is an acceptable class to use for GetSubConnectionInfo() calls
*/
enum TConnectionType
	{
	/** Connection values <1000 for generic interface values
	These are used to indicate an interface does not support extended 
	management functionality, and a limited subset of information is made 
	available through a compatibility layer
	*/
	EConnectionGeneric,

	/** Connection values 1000-1999 for CSD connections
	 */
	EConnectionCSD = 1000,

	/** Connection values > 2000 for PSD connections
	Connection values 2000-2499 for subsets of GPRS/UMTS
	*/
	EConnectionGPRS = 2000,

	EConnectionGPRSR97,
	EConnectionGPRSR99,
	EConnectionGPRSRel4,
	EConnectionGPRSRel5,

	/** Connection values 3000 for IEEE802.3 (Ethernet)
	*/
	EConnectionEthernet = 3000,

	/** Connection values 3100 for IEEE802.11 (WLAN)
	*/
	EConnectionWLAN = 3100,

	/** Connection values 4000 - 4099 for Bluetooth PAN profile interfaces
	*/ 
	EConnectionBTPAN = 4000

	/** Connection values 4100+ for other PS technologies
	*/
	};

/**
Callback actions
@publishedAll
@released
*/
enum TCallbackAction
	{
	ECallbackActionIETFType0	=0,
	ECallbackActionIETFType1	=1,
	ECallbackActionIETFType2	=2,
	ECallbackActionIETFType3	=3,
	ECallbackActionIETFType4	=4,
	ECallbackActionIETFType5	=5,
	//
	ECallbackActionMSCBCPRequireClientSpecifiedNumber	= 1001,
	ECallbackActionMSCBCPAcceptServerSpecifiedNumber	= 1002,
	ECallbackActionMSCBCPOverrideServerSpecifiedNumber	= 1003
	};

#endif
