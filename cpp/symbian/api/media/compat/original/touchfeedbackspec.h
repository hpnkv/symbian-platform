/*
* Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Container for composit feedback types.
* Part of:      Tactile Feedback.
*
*/

#ifndef C_FEEDBACKSPEC_H
#define C_FEEDBACKSPEC_H

#include <e32base.h>
#include <touchlogicalfeedback.h>


//  Container for one event/feedback type pair
struct TTactileFbItem
    {
    TTouchEventType       iEventType;
    TTouchLogicalFeedback iFeedback;
    TTouchFeedbackType    iFeedbackType;
    };

/**
 *  Contains feedback types used for setting feedback area
 *
 *  This class acts as container for event/feedback type pairs. These pairs
 *  define what kind of feedback is producent when area is touched.
 *
 *  @lib touchfeedback.lib
 *  @since S60 5.2
 */
class CFeedbackSpec: public CBase
    {
public:
    /**
     * Instantiation method.
     *
     * @since S60 5.2
     * @return New CFeedbackSpec instance.
     */
    IMPORT_C static CFeedbackSpec* New();

    /**
     * Destructor.
     *
     * @since S60 5.2
     */
    virtual ~CFeedbackSpec();

    /**
     * Adds event/feedback type pair to feedback spec
     *
     * @since S60 5.2
     * @param aEventType - Touch event type.
     * @param aFeedback  - Logical feedback type.
     * @return KErrNone, or one of standard Symbian OS error codes.
     */
    IMPORT_C TInt AddFeedback( TTouchEventType aEventType,
                               TTouchLogicalFeedback aFeedback );

    /**
     * Gets defined event/feedback type pairs
     *
     * @since S60 5.2
     * @param aArray - Event/feedback type pairs is returned here.
     */
    IMPORT_C void GetFeedbackSpec( RArray<TTactileFbItem>& aArray );

    /**
     * Adds event/feedback type pair to feedback spec.
     * Using this overload it is possible to define only one feedback type
     * (vibra/audio) for event.
     *
     * @since S60 5.2
     * @param aEventType - Touch event type.
     * @param aFeedback  - Logical feedback type.
     * @param aFeedbackType - Bitmask to define feedback type (vibra/audio).
     * @return KErrNone, or one of standard Symbian OS error codes.
     */
    IMPORT_C TInt AddFeedback( TTouchEventType aEventType,
                               TTouchLogicalFeedback aFeedback,
                               TTouchFeedbackType aFeedbackType );


private:
    /**
     * Constructor.
     */
    CFeedbackSpec();

private: // data

    /**
     * Array for event type/feedback type pairs
     * Own.
     */
    RArray<TTactileFbItem> iFbArray;
    };

#endif //  C_FEEDBACKSPEC_H
